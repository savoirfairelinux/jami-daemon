/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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
#include "manager.h"
#include "audio/ringbufferpool.h"
#include "dring/call_const.h"
#include "client/ring_signal.h"
#include "audio/audiorecord.h"
#include "sip/sip_utils.h"
#include "ip_utils.h"
#include "array_size.h"
#include "map_utils.h"
#include "call_factory.h"
#include "string_utils.h"
#include "enumclass_utils.h"

#include "errno.h"

#include <stdexcept>
#include <system_error>
#include <algorithm>
#include <functional>
#include <utility>

namespace ring {

/// Hangup many calls with same error code, filtered by a predicate
///
/// For each call pointer given by iterating on given \a callptr_list
/// calls the unary predicate \a pred with this call pointer and hangup the call with given error
/// code \a errcode when the predicate return true.
/// The predicate should have <code>bool(Call*) signature</code>.
inline void
hangupCallsIf(Call::SubcallSet callptr_list, int errcode, const std::function<bool(Call*)>& pred)
{
    std::for_each(std::begin(callptr_list), std::end(callptr_list),
                  [&](const std::shared_ptr<Call>& call_ptr) {
                      if (pred(call_ptr.get())) {
                          try {
                              call_ptr->hangup(errcode);
                          } catch (const std::exception& e) {
                              RING_ERR("[call:%s] hangup failed: %s",
                                       call_ptr->getCallId().c_str(), e.what());
                          }
                      }
                  });
}

/// Hangup many calls with same error code.
///
/// Works as hangupCallsIf() with a predicate that always return true.
inline void
hangupCalls(Call::SubcallSet callptr_list, int errcode)
{
    hangupCallsIf(callptr_list, errcode, [](Call*){ return true; });
}

//==============================================================================

Call::Call(Account& account, const std::string& id, Call::CallType type)
    : id_(id)
    , type_(type)
    , account_(account)
{
    addStateListener([this](UNUSED Call::CallState call_state,
                            UNUSED Call::ConnectionState cnx_state,
                            UNUSED int code) {
        checkPendingIM();
        checkAudio();

        // kill pending subcalls at disconnect
        if (call_state == CallState::OVER)
            hangupCalls(safePopSubcalls(), 0);
    });

    time(&timestamp_start_);
    account_.attachCall(id_);
}

Call::~Call()
{
    account_.detachCall(id_);
}

void
Call::removeCall()
{
    auto this_ = shared_from_this();
    Manager::instance().callFactory.removeCall(*this);
    setState(CallState::OVER);
    recAudio_->closeFile();
}

const std::string&
Call::getAccountId() const
{
    return account_.getAccountID();
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
                case CallState::MERROR:
                    return true;
                default: // INACTIVE, HOLD
                    return false;
            }

        case CallState::ACTIVE:
            switch (newState) {
                case CallState::HOLD:
                case CallState::MERROR:
                    return true;
                default: // INACTIVE, ACTIVE, BUSY
                    return false;
            }

        case CallState::HOLD:
            switch (newState) {
                case CallState::ACTIVE:
                case CallState::MERROR:
                    return true;
                default: // INACTIVE, HOLD, BUSY, MERROR
                    return false;
            }

        case CallState::BUSY:
            switch (newState) {
                case CallState::MERROR:
                    return true;
                default: // INACTIVE, ACTIVE, HOLD, BUSY
                    return false;
            }

        default: // MERROR
            return false;
    }
}

bool
Call::setState(CallState call_state, ConnectionState cnx_state, signed code)
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    RING_DBG("[call:%s] state change %u/%u, cnx %u/%u, code %d", id_.c_str(),
             (unsigned)callState_, (unsigned)call_state, (unsigned)connectionState_,
             (unsigned)cnx_state, code);

    if (callState_ != call_state) {
        if (not validStateTransition(call_state)) {
            RING_ERR("[call:%s] invalid call state transition from %u to %u",
                     id_.c_str(), (unsigned)callState_, (unsigned)call_state);
            return false;
        }
    } else if (connectionState_ == cnx_state)
        return true; // no changes as no-op

    // Emit client state only if changed
    auto old_client_state = getStateStr();
    callState_ = call_state;
    connectionState_ = cnx_state;
    auto new_client_state = getStateStr();

    for (auto& l : stateChangedListeners_)
        l(callState_, connectionState_, code);

    if (old_client_state != new_client_state) {
        if (not parent_) {
            RING_DBG("[call:%s] emit client call state change %s, code %d",
                     id_.c_str(), new_client_state.c_str(), code);
            emitSignal<DRing::CallSignal::StateChange>(id_, new_client_state, code);
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
    using namespace DRing::Call;

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
            if(getConnectionState() == ConnectionState::DISCONNECTED)
                return StateEvent::HUNGUP;
            return StateEvent::HOLD;

        case CallState::BUSY:
            return StateEvent::BUSY;

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
    std::string process_id = Recordable::recAudio_->getRecorderID();
    RingBufferPool &rbPool = Manager::instance().getRingBufferPool();

    if (startRecording) {
        rbPool.bindHalfDuplexOut(process_id, id_);
        rbPool.bindHalfDuplexOut(process_id, RingBufferPool::DEFAULT_ID);
    } else {
        rbPool.unBindHalfDuplexOut(process_id, id_);
        rbPool.unBindHalfDuplexOut(process_id, RingBufferPool::DEFAULT_ID);
    }

    return startRecording;
}

std::map<std::string, std::string>
Call::getDetails() const
{
    return {
        {DRing::Call::Details::CALL_TYPE,        ring::to_string((unsigned)type_)},
        {DRing::Call::Details::PEER_NUMBER,      peerNumber_},
        {DRing::Call::Details::DISPLAY_NAME,     peerDisplayName_},
        {DRing::Call::Details::CALL_STATE,       getStateStr()},
        {DRing::Call::Details::CONF_ID,          confID_},
        {DRing::Call::Details::TIMESTAMP_START,  ring::to_string(timestamp_start_)},
        {DRing::Call::Details::ACCOUNTID,        getAccountId()},
        {DRing::Call::Details::AUDIO_MUTED,      std::string(bool_to_str(isAudioMuted_))},
        {DRing::Call::Details::VIDEO_MUTED,      std::string(bool_to_str(isVideoMuted_))},
    };
}

std::map<std::string, std::string>
Call::getNullDetails()
{
    return {
        {DRing::Call::Details::CALL_TYPE,        "0"},
        {DRing::Call::Details::PEER_NUMBER,      ""},
        {DRing::Call::Details::DISPLAY_NAME,     ""},
        {DRing::Call::Details::CALL_STATE,       "UNKNOWN"},
        {DRing::Call::Details::CONF_ID,          ""},
        {DRing::Call::Details::TIMESTAMP_START,  ""},
        {DRing::Call::Details::ACCOUNTID,        ""},
        {DRing::Call::Details::VIDEO_SOURCE,     "UNKNOWN"},
    };
}

void
Call::onTextMessage(std::map<std::string, std::string>&& messages)
{
    {
        std::lock_guard<std::recursive_mutex> lk {callMutex_};
        if (parent_) {
            pendingInMessages_.emplace_back(std::move(messages), "");
            return;
        }
    }
    Manager::instance().incomingMessage(getCallId(), getPeerNumber(), messages);
}

void
Call::peerHungup()
{
    const auto state = getState();
    const auto aborted = state == CallState::ACTIVE or state == CallState::HOLD;
    setState(ConnectionState::DISCONNECTED,
             aborted ? ECONNABORTED : ECONNREFUSED);
}

void
Call::addSubCall(Call& subcall)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    // XXX: following check seems wobbly - need comment
    if (connectionState_ == ConnectionState::CONNECTED
        || callState_ == CallState::ACTIVE
        || callState_ == CallState::OVER) {
        subcall.removeCall();
        return;
    }

    if (not subcalls_.emplace(getPtr(subcall)).second) {
        RING_ERR("[call:%s] add twice subcall %s", getCallId().c_str(), subcall.getCallId().c_str());
        return;
    }

    RING_DBG("[call:%s] add subcall %s", getCallId().c_str(), subcall.getCallId().c_str());
    subcall.parent_ = getPtr(*this);

    for (const auto& msg : pendingOutMessages_)
        subcall.sendTextMessage(msg.first, msg.second);

    subcall.addStateListener(
        [&subcall](Call::CallState new_state, Call::ConnectionState new_cstate, UNUSED int code) {
            auto parent = subcall.parent_;
            assert(parent != nullptr); // subcall cannot be "un-parented"
            parent->subcallStateChanged(subcall, new_state, new_cstate);
        });
}

/// Called by a subcall when its states change (multidevice)
///
/// Its purpose is to manage per device call and try to found the first responding.
/// Parent call states are managed by these subcalls.
/// \note this method may decrease the given \a subcall ref count.
void
Call::subcallStateChanged(Call& subcall,
                          Call::CallState new_state,
                          Call::ConnectionState new_cstate)
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
        RING_DBG("[call:%s] subcall %s answered by peer", getCallId().c_str(),
                 subcall.getCallId().c_str());

        hangupCallsIf(safePopSubcalls(), 0, [&](const Call* call){ return call != &subcall; });
        merge(subcall);
        Manager::instance().peerAnsweredCall(*this);
        return;
    }

    // Hangup the call if any device hangup
    // XXX: not sure it's what we really want
    if (new_state == CallState::ACTIVE and new_cstate == ConnectionState::DISCONNECTED) {
        RING_WARN("[call:%s] subcall %s hangup by peer", getCallId().c_str(),
                  subcall.getCallId().c_str());

        hangupCalls(safePopSubcalls(), 0);
        Manager::instance().peerHungupCall(*this);
        removeCall();
        return;
    }

    // Subcall is busy or failed
    if (new_state >= CallState::BUSY) {
        RING_WARN("[call:%s] subcall %s failed", getCallId().c_str(), subcall.getCallId().c_str());
        std::lock_guard<std::recursive_mutex> lk {callMutex_};
        subcalls_.erase(getPtr(subcall));

        // Parent call fails if all subcalls have failed
        if (subcalls_.empty())
            // XXX: first idea was to use std::errc::host_unreachable, but it's not available on some platforms
            // like mingw.
            setState(CallState::MERROR, ConnectionState::DISCONNECTED, static_cast<int>(std::errc::io_error));
        else
            RING_DBG("[call:%s] remains %zu subcall(s)", getCallId().c_str(), subcalls_.size());
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
///
void
Call::merge(Call& subcall)
{
    RING_DBG("[call:%s] merge subcall %s", getCallId().c_str(), subcall.getCallId().c_str());

    // Merge data
    {
        std::lock(callMutex_, subcall.callMutex_);
        std::lock_guard<std::recursive_mutex> lk1 {callMutex_, std::adopt_lock};
        std::lock_guard<std::recursive_mutex> lk2 {subcall.callMutex_, std::adopt_lock};
        pendingInMessages_ = std::move(subcall.pendingInMessages_);
        if (peerNumber_.empty())
            peerNumber_ = std::move(subcall.peerNumber_);
        peerDisplayName_ = std::move(subcall.peerDisplayName_);
        setState(subcall.getState(), subcall.getConnectionState());
    }

    subcall.removeCall();
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
        if (state == DRing::Call::StateEvent::CURRENT) {
            for (const auto& msg : pendingInMessages_)
                Manager::instance().incomingMessage(getCallId(), getPeerNumber(), msg.first);
            pendingInMessages_.clear();

            for (const auto& msg : pendingOutMessages_)
                sendTextMessage(msg.first, msg.second);
            pendingOutMessages_.clear();
        }
    }
}

/// Handle tones for RINGING and BUSY calls
///
void
Call::checkAudio()
{
    using namespace DRing::Call;

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

} // namespace ring
