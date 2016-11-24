/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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


namespace ring {



Call::Call(Account& account, const std::string& id, Call::CallType type)
    : id_(id)
    , type_(type)
    , account_(account)
{
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
    iceTransport_.reset();
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
    auto old_client_state = getStateStr().first;
    callState_ = call_state;
    connectionState_ = cnx_state;
    auto new_client_state = getStateStr().first;

    if (call_state == CallState::OVER) {
        RING_DBG("[call:%s] %lu subcalls %lu listeners", id_.c_str(), subcalls.size(), stateChangedListeners_.size());
        if (not subcalls.empty()) {
            auto subs = std::move(subcalls);
            for (auto c : subs)
                c->hangup(0);
            pendingInMessages_.clear();
            pendingOutMessages_.clear();
        }
    } else if (call_state == CallState::ACTIVE and connectionState_ == ConnectionState::CONNECTED and not pendingOutMessages_.empty()) {
        for (const auto& msg : pendingOutMessages_)
            sendTextMessage(msg.first, msg.second);
        pendingOutMessages_.clear();
    }

    for (auto& l : stateChangedListeners_)
        l(callState_, connectionState_, code);

    if (old_client_state != new_client_state) {
        if (not quiet) {
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



void CallState_::configureConference(std::shared_ptr<Conference>& conf, 
        const std::string& callId, Manager* manager){
        RING_WARN("Call state not recognized");
}
const std::string IncomingCall::getState() const {
    return "Incoming";
}
void IncomingCall::configureConference(std::shared_ptr<Conference>& conf, 
    const std::string& callId, Manager* manager){
    conf->bindParticipant(callId);
    manager->offHoldCall(callId);
}
const std::string HoldCall::getState() const {
    return "Hold";
}

void HoldCall::configureConference(std::shared_ptr<Conference>& conf, 
    const std::string& callId, Manager* manager){
    conf->bindParticipant(callId);
    manager->offHoldCall(callId);
}

const std::string CurrentCall::getState() const {
    return "Current";
}
void CurrentCall::configureConference(std::shared_ptr<Conference>& conf, 
    const std::string& callId, Manager* manager){
    conf->bindParticipant(callId);
}

const std::string InactiveCall::getState() const {
    return "Inactive";
}
void InactiveCall::configureConference(std::shared_ptr<Conference>& conf, 
    const std::string& callId, Manager* manager){
    conf->bindParticipant(callId);
    manager->offHoldCall(callId);
}

const std::string ConnectingCall::getState() const {
    return "Connecting";
}

const std::string RingingCall::getState() const {
    return "Ringing";
}

const std::string HungupCall::getState() const {
    return "Hungup";
}

const std::string BusyCall::getState() const {
    return "Busy";
}
const std::string OverCall::getState() const {
    return "Over";
}
const std::string FailureCall::getState() const {
    return "Failure";
}


std::pair<std::string, CallState_*>
Call::onActiveState(Call::ConnectionState connectionState_) const
{
    using namespace DRing::Call;
    if(connectionState_ == ConnectionState::PROGRESSING)
    {        
        return std::make_pair(StateEvent::CONNECTING, new ConnectingCall());
    }
    else if(connectionState_ == ConnectionState::RINGING)
    {        
        if(isIncoming())
            return std::make_pair(StateEvent::INCOMING, new IncomingCall());
        else
            return std::make_pair(StateEvent::RINGING, new RingingCall());
    }
    else if(connectionState_ == ConnectionState::DISCONNECTED)
    {
        return std::make_pair(StateEvent::HUNGUP, new HungupCall());
    }
    else if (connectionState_ == ConnectionState::CONNECTED)
    {
        return std::make_pair(StateEvent::CURRENT, new CurrentCall());
    }
    else
    {
        return std::make_pair(StateEvent::CURRENT, new CurrentCall());
    }
}

std::pair<std::string, CallState_*>
Call::onInactiveState(Call::ConnectionState connectionState_) const
{
    using namespace DRing::Call;
    if(connectionState_ == ConnectionState::PROGRESSING)
    {        
        return std::make_pair(StateEvent::CONNECTING, new ConnectingCall());
    }
    else if(connectionState_ == ConnectionState::RINGING)
    {        
        if(isIncoming())
            return std::make_pair(StateEvent::INCOMING, new IncomingCall());
        else 
            return std::make_pair(StateEvent::RINGING, new RingingCall());
    }
    else if (connectionState_ == ConnectionState::CONNECTED)
    {
        return std::make_pair(StateEvent::CURRENT, new CurrentCall());        
    }
    else
    {
        return std::make_pair(StateEvent::INACTIVE, new InactiveCall());
    }
}

std::pair<std::string, CallState_*>
Call::getStateStr() const
{
   using namespace DRing::Call;
   Call::CallState callState = getState();
   Call::ConnectionState connectionState = getConnectionState();

   if(callState == CallState::ACTIVE)
   {
        return onActiveState(connectionState);
   }
   else if(callState == CallState::HOLD)
   {
        if(connectionState == ConnectionState::DISCONNECTED)
            return std::make_pair(StateEvent::HUNGUP, new HungupCall());
        return std::make_pair(StateEvent::HOLD, new HoldCall());
   }
   else if(callState == CallState::BUSY)
   {
        return std::make_pair(StateEvent::BUSY, new BusyCall());
   }
   else if (callState == CallState::INACTIVE)
   {
        return onInactiveState(connectionState);
   }
   else if (callState == CallState::OVER)
   {
        return std::make_pair(StateEvent::OVER, new OverCall());

   }
   else if(callState == CallState::MERROR)
   {
        return std::make_pair(StateEvent::FAILURE, new FailureCall());
   }
    return std::make_pair(StateEvent::FAILURE, new FailureCall());
}

IpAddr
Call::getLocalIp() const
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return localAddr_;
}

unsigned int
Call::getLocalAudioPort() const
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return localAudioPort_;
}

unsigned int
Call::getLocalVideoPort() const
{
    std::lock_guard<std::recursive_mutex> lock(callMutex_);
    return localVideoPort_;
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

CallState_*
Call::getCallState()
{
    return getStateStr().second;
}

std::map<std::string, std::string>
Call::getDetails() const
{
    return {
        {DRing::Call::Details::CALL_TYPE,        ring::to_string((unsigned)type_)},
        {DRing::Call::Details::PEER_NUMBER,      peerNumber_},
        {DRing::Call::Details::DISPLAY_NAME,     peerDisplayName_},
        {DRing::Call::Details::CALL_STATE,       getStateStr().first},
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

bool
Call::initIceTransport(bool master, unsigned channel_num)
{
    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    iceTransport_ = iceTransportFactory.createTransport(getCallId().c_str(),
                                                        channel_num, master,
                                                        account_.getIceOptions());
    return static_cast<bool>(iceTransport_);
}

int
Call::waitForIceInitialization(unsigned timeout)
{
    return iceTransport_->waitForInitialization(timeout);
}

int
Call::waitForIceNegotiation(unsigned timeout)
{
    return iceTransport_->waitForNegotiation(timeout);
}

bool
Call::isIceUsed() const
{
    return iceTransport_ and iceTransport_->isInitialized();
}

bool
Call::isIceRunning() const
{
    return iceTransport_ and iceTransport_->isRunning();
}

std::unique_ptr<IceSocket>
Call::newIceSocket(unsigned compId)
{
    return std::unique_ptr<IceSocket> {new IceSocket(iceTransport_, compId)};
}

void
Call::onTextMessage(std::map<std::string, std::string>&& messages)
{
    if (quiet)
        pendingInMessages_.emplace_back(std::move(messages), "");
    else
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
Call::addSubCall(const std::shared_ptr<Call>& call)
{
    if (connectionState_ == ConnectionState::CONNECTED
           || callState_ == CallState::ACTIVE
           || callState_ == CallState::OVER) {
        call->removeCall();
    } else {
        std::lock_guard<std::recursive_mutex> lk (callMutex_);
        if (not subcalls.emplace(call).second)
            return;
        call->quiet = true;

        for (auto& pmsg : pendingOutMessages_)
            call->sendTextMessage(pmsg.first, pmsg.second);

        std::weak_ptr<Call> wthis = shared_from_this();
        std::weak_ptr<Call> wcall = call;
        call->addStateListener([wcall,wthis](Call::CallState new_state, Call::ConnectionState new_cstate, int code) {
            if (auto call = wcall.lock()) {
                if (auto sthis = wthis.lock()) {
                    auto& this_ = *sthis;
                    auto sit = this_.subcalls.find(call);
                    if (sit == this_.subcalls.end())
                        return;
                    RING_WARN("[call %s] DeviceCall call %s state changed %d %d", this_.getCallId().c_str(), call->getCallId().c_str(), new_state, new_cstate);
                    if (new_state == CallState::OVER) {
                        std::lock_guard<std::recursive_mutex> lk (this_.callMutex_);
                        this_.subcalls.erase(call);
                    } else if (new_state == CallState::ACTIVE && this_.callState_ == CallState::INACTIVE) {
                        this_.setState(new_state);
                    }
                    if ((unsigned)this_.connectionState_ < (unsigned)new_cstate && (unsigned)new_cstate <= (unsigned)ConnectionState::RINGING) {
                        this_.setState(new_cstate);
                    } else if (new_cstate == ConnectionState::DISCONNECTED && new_state == CallState::ACTIVE) {
                        std::lock_guard<std::recursive_mutex> lk (this_.callMutex_);
                        RING_WARN("[call %s] peer hangup", this_.getCallId().c_str());
                        auto subcalls = std::move(this_.subcalls);
                        for (auto& sub : subcalls) {
                            if (sub != call)
                                try {
                                    sub->hangup(0);
                                } catch(const std::exception& e) {
                                    RING_WARN("[call %s] error hanging up: %s", this_.getCallId().c_str());
                                }
                        }
                        this_.peerHungup();
                    }
                    if (new_state == CallState::ACTIVE && new_cstate == ConnectionState::CONNECTED) {
                        std::lock_guard<std::recursive_mutex> lk (this_.callMutex_);
                        RING_WARN("[call %s] peer answer", this_.getCallId().c_str());
                        auto subcalls = std::move(this_.subcalls);
                        for (auto& sub : subcalls) {
                            if (sub != call)
                                sub->hangup(0);
                        }
                        this_.merge(call);
                        Manager::instance().peerAnsweredCall(this_);
                    }
                    RING_WARN("[call %s] Remaining %d subcalls", this_.getCallId().c_str(), this_.subcalls.size());
                    if (this_.subcalls.empty())
                        this_.pendingOutMessages_.clear();
                } else {
                    RING_WARN("DeviceCall IGNORED call %s state changed %d %d", call->getCallId().c_str(), new_state, new_cstate);
                }
            }
        });
        setState(ConnectionState::TRYING);
    }
}

void
Call::merge(std::shared_ptr<Call> scall)
{
    RING_WARN("[call %s] merge to -> [call %s]", scall->getCallId().c_str(), getCallId().c_str());
    auto& call = *scall;
    std::lock(callMutex_, call.callMutex_);
    std::lock_guard<std::recursive_mutex> lk1 (callMutex_, std::adopt_lock);
    std::lock_guard<std::recursive_mutex> lk2 (call.callMutex_, std::adopt_lock);
    auto pendingInMessages = std::move(call.pendingInMessages_);
    iceTransport_ = std::move(call.iceTransport_);
    peerDisplayName_ = std::move(call.peerDisplayName_);
    localAddr_ = call.localAddr_;
    localAudioPort_ = call.localAudioPort_;
    localVideoPort_ = call.localVideoPort_;
    setState(call.getState());
    setState(call.getConnectionState());
    for (const auto& msg : pendingInMessages)
        Manager::instance().incomingMessage(getCallId(), getPeerNumber(), msg.first);
    scall->removeCall();
}


} // namespace ring
