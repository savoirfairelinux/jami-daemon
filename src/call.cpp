/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "call.h"
#include "account.h"
#include "callstreamsmanager.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#include "plugin/streamdata.h"
#endif
#include "audio/ringbufferpool.h"
#include "jami/call_const.h"
#include "client/ring_signal.h"
#include "connectivity/sip_utils.h"
#include "connectivity/ip_utils.h"
#include "map_utils.h"
#include "call_factory.h"
#include "string_utils.h"
#include "enumclass_utils.h"

#include "errno.h"

#include <opendht/thread_pool.h>

#include <stdexcept>
#include <system_error>
#include <algorithm>
#include <functional>
#include <utility>

namespace jami {

/// Hangup many calls with same error code, filtered by a predicate
///
/// For each call pointer given by iterating on given \a callptr_list
/// calls the unary predicate \a pred with this call pointer and hangup the call with given error
/// code \a errcode when the predicate return true.
/// The predicate should have <code>bool(Call*) signature</code>.
template<typename T>
inline void
hangupCallsIf(Call::SubcallSet&& calls, int errcode, T pred)
{
    for (auto& call : calls) {
        if (not pred(call.get()))
            continue;
        dht::ThreadPool::io().run([call = std::move(call), errcode] { call->hangup(errcode); });
    }
}

/// Hangup many calls with same error code.
///
/// Works as hangupCallsIf() with a predicate that always return true.
inline void
hangupCalls(Call::SubcallSet&& callptr_list, int errcode)
{
    hangupCallsIf(std::move(callptr_list), errcode, [](Call*) { return true; });
}

//==============================================================================

Call::Call(const std::shared_ptr<Account>& account,
           const std::string& id,
           Call::CallType type,
           const std::map<std::string, std::string>& details)
    : id_(id)
    , type_(type)
    , account_(account)
{
    updateDetails(details);

    addStateListener([this](Call::CallState call_state,
                            Call::ConnectionState cnx_state,
                            UNUSED int code) {
        checkPendingIM();
        runOnMainThread([callWkPtr = weak()] {
            if (auto call = callWkPtr.lock())
                call->checkAudio();
        });

        // if call just started ringing, schedule call timeout
        if (type_ == CallType::INCOMING and cnx_state == ConnectionState::RINGING) {
            auto timeout = Manager::instance().getRingingTimeout();
            JAMI_DBG("Scheduling call timeout in %d seconds", timeout);

            Manager::instance().scheduler().scheduleIn(
                [callWkPtr = weak()] {
                    if (auto callShPtr = callWkPtr.lock()) {
                        if (callShPtr->getConnectionState() == Call::ConnectionState::RINGING) {
                            JAMI_DBG(
                                "Call %s is still ringing after timeout, setting state to BUSY",
                                callShPtr->getCallId().c_str());
                            callShPtr->hangup(PJSIP_SC_BUSY_HERE);
                            Manager::instance().callFailure(*callShPtr);
                        }
                    }
                },
                std::chrono::seconds(timeout));
        }

        if (!isSubcall()) {
            if (cnx_state == ConnectionState::CONNECTED && duration_start_ == time_point::min())
                duration_start_ = clock::now();
            else if (cnx_state == ConnectionState::DISCONNECTED && call_state == CallState::OVER) {
                if (auto jamiAccount = std::dynamic_pointer_cast<JamiAccount>(getAccount().lock())) {
                    // TODO: This will be removed when 1:1 swarm will have a conference.
                    // For now, only commit for 1:1 calls
                    if (toUsername().find('/') == std::string::npos && getCallType() == CallType::OUTGOING) {
                        jamiAccount->convModule()->addCallHistoryMessage(getPeerNumber(),
                                                                         getCallDuration().count());
                    }
                    monitor();
                }
            }
        }

        // kill pending subcalls at disconnect
        if (call_state == CallState::OVER)
            hangupCalls(safePopSubcalls(), 0);

        return true;
    });

    time(&timestamp_start_);
}

Call::~Call() {}

void
Call::removeCall()
{
    auto this_ = shared_from_this();
    Manager::instance().callFactory.removeCall(*this);
    setState(CallState::OVER);
    if (Recordable::isRecording())
        Recordable::stopRecording();
    if (auto account = account_.lock())
        account->detach(this_);
}

std::string
Call::getAccountId() const
{
    if (auto shared = account_.lock())
        return shared->getAccountID();
    JAMI_ERR("No account detected");
    return {};
}

Call::ConnectionState
Call::getConnectionState() const
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return connectionState_;
}

Call::CallState
Call::getState() const
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return callState_;
}

bool
Call::validStateTransition(CallState newState)
{
    // Notice to developper:
    // - list only permitted transition (return true)
    // - let non permitted ones as default case (return false)

    // always permited
    if (newState == CallState::OVER)
        return true;

    switch (callState_) {
    case CallState::INACTIVE:
        switch (newState) {
        case CallState::ACTIVE:
        case CallState::BUSY:
        case CallState::PEER_BUSY:
        case CallState::MERROR:
            return true;
        default: // INACTIVE, HOLD
            return false;
        }

    case CallState::ACTIVE:
        switch (newState) {
        case CallState::BUSY:
        case CallState::PEER_BUSY:
        case CallState::HOLD:
        case CallState::MERROR:
            return true;
        default: // INACTIVE, ACTIVE
            return false;
        }

    case CallState::HOLD:
        switch (newState) {
        case CallState::ACTIVE:
        case CallState::MERROR:
            return true;
        default: // INACTIVE, HOLD, BUSY, PEER_BUSY, MERROR
            return false;
        }

    case CallState::BUSY:
        switch (newState) {
        case CallState::MERROR:
            return true;
        default: // INACTIVE, ACTIVE, HOLD, BUSY, PEER_BUSY
            return false;
        }

    default: // MERROR
        return false;
    }
}

bool
Call::setState(CallState call_state, ConnectionState cnx_state, signed code)
{
    std::unique_lock<std::recursive_mutex> lock(callMutex_);
    JAMI_DBG("[call:%s] state change %u/%u, cnx %u/%u, code %d",
             id_.c_str(),
             (unsigned) callState_,
             (unsigned) call_state,
             (unsigned) connectionState_,
             (unsigned) cnx_state,
             code);

    if (callState_ != call_state) {
        if (not validStateTransition(call_state)) {
            JAMI_ERR("[call:%s] invalid call state transition from %u to %u",
                     id_.c_str(),
                     (unsigned) callState_,
                     (unsigned) call_state);
            return false;
        }
    } else if (connectionState_ == cnx_state)
        return true; // no changes as no-op

    // Emit client state only if changed
    auto old_client_state = getStateStr();
    callState_ = call_state;
    connectionState_ = cnx_state;
    auto new_client_state = getStateStr();

    for (auto it = stateChangedListeners_.begin(); it != stateChangedListeners_.end();) {
        if ((*it)(callState_, connectionState_, code))
            ++it;
        else
            it = stateChangedListeners_.erase(it);
    }

    if (old_client_state != new_client_state) {
        if (not parent_) {
            JAMI_DBG("[call:%s] emit client call state change %s, code %d",
                     id_.c_str(),
                     new_client_state.c_str(),
                     code);
            lock.unlock();
            emitSignal<libjami::CallSignal::StateChange>(getAccountId(),
                                                         id_,
                                                         new_client_state,
                                                         code);
        }
    }

    return true;
}

bool
Call::setState(CallState call_state, signed code)
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return setState(call_state, connectionState_, code);
}

bool
Call::setState(ConnectionState cnx_state, signed code)
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return setState(callState_, cnx_state, code);
}

std::string
Call::getStateStr() const
{
    using namespace libjami::Call;

    switch (getState()) {
    case CallState::ACTIVE:
        switch (getConnectionState()) {
        case ConnectionState::PROGRESSING:
            return StateEvent::CONNECTING;

        case ConnectionState::RINGING:
            return isIncoming() ? StateEvent::INCOMING : StateEvent::RINGING;

        case ConnectionState::DISCONNECTED:
            return StateEvent::HUNGUP;

        case ConnectionState::CONNECTED:
        default:
            return StateEvent::CURRENT;
        }

    case CallState::HOLD:
        if (getConnectionState() == ConnectionState::DISCONNECTED)
            return StateEvent::HUNGUP;
        return StateEvent::HOLD;

    case CallState::BUSY:
        return StateEvent::BUSY;

    case CallState::PEER_BUSY:
        return StateEvent::PEER_BUSY;

    case CallState::INACTIVE:
        switch (getConnectionState()) {
        case ConnectionState::PROGRESSING:
            return StateEvent::CONNECTING;

        case ConnectionState::RINGING:
            return isIncoming() ? StateEvent::INCOMING : StateEvent::RINGING;

        case ConnectionState::CONNECTED:
            return StateEvent::CURRENT;

        default:
            return StateEvent::INACTIVE;
        }

    case CallState::OVER:
        return StateEvent::OVER;

    case CallState::MERROR:
    default:
        return StateEvent::FAILURE;
    }
}

bool
Call::toggleRecording()
{
    const bool startRecording = Recordable::toggleRecording();
    return startRecording;
}

void
Call::updateDetails(const std::map<std::string, std::string>& details)
{
    const auto& iter = details.find(libjami::Call::Details::AUDIO_ONLY);
    if (iter != std::end(details))
        isAudioOnly_ = iter->second == TRUE_STR;
}

std::map<std::string, std::string>
Call::getDetails() const
{
    auto conference = conf_.lock();
    return {
        {libjami::Call::Details::CALL_TYPE, std::to_string((unsigned) type_)},
        {libjami::Call::Details::PEER_NUMBER, peerNumber_},
        {libjami::Call::Details::DISPLAY_NAME, peerDisplayName_},
        {libjami::Call::Details::CALL_STATE, getStateStr()},
        {libjami::Call::Details::CONF_ID, conference ? conference->getConfId() : ""},
        {libjami::Call::Details::TIMESTAMP_START, std::to_string(timestamp_start_)},
        {libjami::Call::Details::ACCOUNTID, getAccountId()},
        {libjami::Call::Details::AUDIO_MUTED,
         std::string(bool_to_str(isCaptureDeviceMuted(MediaType::MEDIA_AUDIO)))},
        {libjami::Call::Details::VIDEO_MUTED,
         std::string(bool_to_str(isCaptureDeviceMuted(MediaType::MEDIA_VIDEO)))},
        {libjami::Call::Details::AUDIO_ONLY, std::string(bool_to_str(not hasVideo()))},
    };
}

void
Call::onTextMessage(std::map<std::string, std::string>&& messages)
{
    auto it = messages.find("application/confInfo+json");
    if (it != messages.end()) {
        setConferenceInfo(it->second);
        return;
    }

    it = messages.find("application/confOrder+json");
    if (it != messages.end()) {
        if (auto conf = conf_.lock())
            conf->onConfOrder(getCallId(), it->second);
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lk {callMutex_};
        if (parent_) {
            pendingInMessages_.emplace_back(std::move(messages), "");
            return;
        }
    }
#ifdef ENABLE_PLUGIN
    auto& pluginChatManager = Manager::instance().getJamiPluginManager().getChatServicesManager();
    if (pluginChatManager.hasHandlers()) {
        pluginChatManager.publishMessage(
            std::make_shared<JamiMessage>(getAccountId(), getPeerNumber(), true, messages, false));
    }
#endif
    Manager::instance().incomingMessage(getAccountId(), getCallId(), getPeerNumber(), messages);
}

void
Call::peerHungup()
{
    const auto state = getState();
    const auto aborted = state == CallState::ACTIVE or state == CallState::HOLD;
    setState(ConnectionState::DISCONNECTED, aborted ? ECONNABORTED : ECONNREFUSED);
}

void
Call::addSubCall(Call& subcall)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    // Add subCall only if call is not connected or terminated
    // Because we only want to addSubCall if the peer didn't answer
    // So till it's <= RINGING
    if (connectionState_ == ConnectionState::CONNECTED
        || connectionState_ == ConnectionState::DISCONNECTED || callState_ == CallState::OVER) {
        subcall.removeCall();
        return;
    }

    if (not subcalls_.emplace(getPtr(subcall)).second) {
        JAMI_ERR("[call:%s] add twice subcall %s", getCallId().c_str(), subcall.getCallId().c_str());
        return;
    }

    JAMI_DBG("[call:%s] add subcall %s", getCallId().c_str(), subcall.getCallId().c_str());
    subcall.parent_ = getPtr(*this);

    for (const auto& msg : pendingOutMessages_)
        subcall.sendTextMessage(msg.first, msg.second);

    subcall.addStateListener(
        [sub = subcall.weak(), parent = weak()](Call::CallState new_state,
                                                Call::ConnectionState new_cstate,
                                                int /* code */) {
            runOnMainThread([sub, parent, new_state, new_cstate]() {
                if (auto p = parent.lock()) {
                    if (auto s = sub.lock()) {
                        p->subcallStateChanged(*s, new_state, new_cstate);
                    }
                }
            });
            return true;
        });
}

/// Called by a subcall when its states change (multidevice)
///
/// Its purpose is to manage per device call and try to found the first responding.
/// Parent call states are managed by these subcalls.
/// \note this method may decrease the given \a subcall ref count.
void
Call::subcallStateChanged(Call& subcall, Call::CallState new_state, Call::ConnectionState new_cstate)
{
    {
        // This condition happens when a subcall hangups/fails after removed from parent's list.
        // This is normal to keep parent_ != nullptr on the subcall, as it's the way to flag it
        // as an subcall and not a master call.
        // XXX: having a way to unsubscribe the state listener could be better than such test
        std::lock_guard<std::recursive_mutex> lk {callMutex_};
        auto sit = subcalls_.find(getPtr(subcall));
        if (sit == subcalls_.end())
            return;
    }

    // We found a responding device: hangup all other subcalls and merge
    if (new_state == CallState::ACTIVE and new_cstate == ConnectionState::CONNECTED) {
        JAMI_DBG("[call:%s] subcall %s answered by peer",
                 getCallId().c_str(),
                 subcall.getCallId().c_str());

        hangupCallsIf(safePopSubcalls(), 0, [&](const Call* call) { return call != &subcall; });
        merge(subcall);
        Manager::instance().peerAnsweredCall(*this);
        return;
    }

    // Hangup the call if any device hangup or send busy
    if ((new_state == CallState::ACTIVE or new_state == CallState::PEER_BUSY)
        and new_cstate == ConnectionState::DISCONNECTED) {
        JAMI_WARN("[call:%s] subcall %s hangup by peer",
                  getCallId().c_str(),
                  subcall.getCallId().c_str());

        hangupCalls(safePopSubcalls(), 0);
        Manager::instance().peerHungupCall(*this);
        removeCall();
        return;
    }

    // Subcall is busy or failed
    if (new_state >= CallState::BUSY) {
        if (new_state == CallState::BUSY || new_state == CallState::PEER_BUSY)
            JAMI_WARN("[call:%s] subcall %s busy", getCallId().c_str(), subcall.getCallId().c_str());
        else
            JAMI_WARN("[call:%s] subcall %s failed",
                      getCallId().c_str(),
                      subcall.getCallId().c_str());
        std::lock_guard<std::recursive_mutex> lk {callMutex_};
        subcalls_.erase(getPtr(subcall));

        // Parent call fails if last subcall is busy or failed
        if (subcalls_.empty()) {
            if (new_state == CallState::BUSY) {
                setState(CallState::BUSY,
                         ConnectionState::DISCONNECTED,
                         static_cast<int>(std::errc::device_or_resource_busy));
            } else if (new_state == CallState::PEER_BUSY) {
                setState(CallState::PEER_BUSY,
                         ConnectionState::DISCONNECTED,
                         static_cast<int>(std::errc::device_or_resource_busy));
            } else {
                // XXX: first idea was to use std::errc::host_unreachable, but it's not available on
                // some platforms like mingw.
                setState(CallState::MERROR,
                         ConnectionState::DISCONNECTED,
                         static_cast<int>(std::errc::io_error));
            }
            removeCall();
        } else {
            JAMI_DBG("[call:%s] remains %zu subcall(s)", getCallId().c_str(), subcalls_.size());
        }

        return;
    }

    // Copy call/cnx states (forward only)
    if (new_state == CallState::ACTIVE && callState_ == CallState::INACTIVE) {
        setState(new_state);
    }
    if (static_cast<unsigned>(connectionState_) < static_cast<unsigned>(new_cstate)
        and static_cast<unsigned>(new_cstate) <= static_cast<unsigned>(ConnectionState::RINGING)) {
        setState(new_cstate);
    }
}

/// Replace current call data with ones from the given \a subcall.
/// Must be called while locked by subclass
void
Call::merge(Call& subcall)
{
    JAMI_DBG("[call:%s] merge subcall %s", getCallId().c_str(), subcall.getCallId().c_str());

    // Merge data
    pendingInMessages_ = std::move(subcall.pendingInMessages_);
    if (peerNumber_.empty())
        peerNumber_ = std::move(subcall.peerNumber_);
    peerDisplayName_ = std::move(subcall.peerDisplayName_);
    setState(subcall.getState(), subcall.getConnectionState());

    std::weak_ptr<Call> subCallWeak = subcall.shared_from_this();
    runOnMainThread([subCallWeak] {
        if (auto subcall = subCallWeak.lock())
            subcall->removeCall();
    });
}

/// Handle pending IM message
///
/// Used in multi-device context to send pending IM when the master call is connected.
void
Call::checkPendingIM()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    auto state = getStateStr();
    // Let parent call handles IM after the merge
    if (not parent_) {
        if (state == libjami::Call::StateEvent::CURRENT) {
            for (const auto& msg : pendingInMessages_)
                Manager::instance().incomingMessage(getAccountId(),
                                                    getCallId(),
                                                    getPeerNumber(),
                                                    msg.first);
            pendingInMessages_.clear();

            std::weak_ptr<Call> callWkPtr = shared_from_this();
            runOnMainThread([callWkPtr, pending = std::move(pendingOutMessages_)] {
                if (auto call = callWkPtr.lock())
                    for (const auto& msg : pending)
                        call->sendTextMessage(msg.first, msg.second);
            });
        }
    }
}

/// Handle tones for RINGING and BUSY calls
///
void
Call::checkAudio()
{
    using namespace libjami::Call;

    auto state = getStateStr();
    if (state == StateEvent::RINGING) {
        Manager::instance().peerRingingCall(*this);
    } else if (state == StateEvent::BUSY) {
        Manager::instance().callBusy(*this);
    }
}

// Helper to safely pop subcalls list
Call::SubcallSet
Call::safePopSubcalls()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    // std::exchange is C++14
    auto old_value = std::move(subcalls_);
    subcalls_.clear();
    return old_value;
}

void
Call::setConferenceInfo(const std::string& msg)
{
    ConfInfo newInfo;
    Json::Value json;
    std::string err;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (reader->parse(msg.data(), msg.data() + msg.size(), &json, &err)) {
        if (json.isObject()) {
            // new confInfo
            if (json.isMember("p"))
                newInfo.mergeJson(json["p"]);
            if (json.isMember("v")) {
                newInfo.v = json["v"].asInt();
                peerConfProtocol_ = newInfo.v;
            }
            if (json.isMember("w"))
                newInfo.w = json["w"].asInt();
            if (json.isMember("h"))
                newInfo.h = json["h"].asInt();
        } else {
            newInfo.mergeJson(json);
        }
    }

    {
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        if (not isConferenceParticipant()) {
            // confID_ empty -> participant set confInfo with the received one
            confInfo_ = std::move(newInfo);

            // Create sink for each participant
#ifdef ENABLE_VIDEO
            createSinks(confInfo_);
#endif
            // Inform client that layout has changed
            jami::emitSignal<libjami::CallSignal::OnConferenceInfosUpdated>(
                id_, confInfo_.toVectorMapStringString());
        } else if (auto conf = conf_.lock()) {
            conf->mergeConfInfo(newInfo, getPeerNumber());
        }
    }
}

void
Call::sendConfOrder(const Json::Value& root)
{
    std::map<std::string, std::string> messages;
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    messages["application/confOrder+json"] = Json::writeString(wbuilder, root);

    auto w = getAccount();
    auto account = w.lock();
    if (account)
        sendTextMessage(messages, account->getFromUri());
}

void
Call::sendConfInfo(const std::string& json)
{
    std::map<std::string, std::string> messages;
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    messages["application/confInfo+json"] = json;

    auto w = getAccount();
    auto account = w.lock();
    if (account)
        sendTextMessage(messages, account->getFromUri());
}

void
Call::resetConfInfo()
{
    sendConfInfo("{}");
}

} // namespace jami
