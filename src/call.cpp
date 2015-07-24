/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "call.h"
#include "account.h"
#include "manager.h"
#include "audio/ringbufferpool.h"
#include "dring/call_const.h"
#include "client/ring_signal.h"

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
    Manager::instance().callFactory.removeCall(*this);
    iceTransport_.reset();
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
    // - list only permit transition (return true)
    // - let non permit ones as default case (return false)
    switch (callState_) {
        case CallState::INACTIVE:
            switch (newState) {
                case CallState::ACTIVE:
                case CallState::BUSY:
                case CallState::MERROR:
                    return true;
                default: // INACTIVE, HOLD
                    return true;
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
             callState_, call_state, connectionState_, cnx_state, code);

    if (callState_ != call_state) {
        if (not validStateTransition(call_state)) {
            RING_ERR("[call:%s] invalid call state transition from %u to %u",
                     id_.c_str(), callState_, call_state);
            return false;
        }
    } else if (connectionState_ == cnx_state)
        return true; // no changes as no-op

    // Emit client state only if changed
    auto old_client_state = getStateStr();
    callState_ = call_state;
    connectionState_ = cnx_state;
    auto new_client_state = getStateStr();

    if (old_client_state != new_client_state) {
        RING_DBG("[call:%s] emit client call state change %s, code %d",
                 id_.c_str(), new_client_state.c_str(), code);
        emitSignal<DRing::CallSignal::StateChange>(id_, new_client_state, code);
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

        case CallState::MERROR:
        default:
            return StateEvent::FAILURE;
    }
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
    RingBufferPool &rbPool = Manager::instance().getRingBufferPool();
    std::string process_id = Recordable::recorder_.getRecorderID();

    if (startRecording) {
        rbPool.bindHalfDuplexOut(process_id, id_);
        rbPool.bindHalfDuplexOut(process_id, RingBufferPool::DEFAULT_ID);

        Recordable::recorder_.start();
    } else {
        rbPool.unBindHalfDuplexOut(process_id, id_);
        rbPool.unBindHalfDuplexOut(process_id, RingBufferPool::DEFAULT_ID);
    }

    return startRecording;
}

void Call::time_stop()
{
    time(&timestamp_stop_);
}

std::string Call::getTypeStr() const
{
    switch (type_) {
        case CallType::INCOMING:
            return "incoming";
        case CallType::OUTGOING:
            return "outgoing";
        case CallType::MISSED:
            return "missed";
        default:
            return "";
    }
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
Call::peerHungup()
{
    const auto state = getState();
    const auto aborted = state == CallState::ACTIVE or state == CallState::HOLD;
    setState(ConnectionState::DISCONNECTED,
             aborted ? ECONNABORTED : ECONNREFUSED);
}

} // namespace ring
