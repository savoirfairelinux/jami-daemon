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

#include "sip/sip_utils.h"
#include "ip_utils.h"
#include "array_size.h"
#include "map_utils.h"
#include "call_factory.h"

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

void
Call::setConnectionState(ConnectionState state)
{
    std::lock_guard<std::mutex> lock(callMutex_);
    connectionState_ = state;
}

Call::ConnectionState
Call::getConnectionState()
{
    std::lock_guard<std::mutex> lock(callMutex_);
    return connectionState_;
}

bool
Call::validTransition(CallState newState)
{
    switch (callState_) {
        case INACTIVE:
            switch (newState) {
                case INACTIVE:
                    return false;
                default:
                    return true;
            }

        case ACTIVE:
            switch (newState) {
                case HOLD:
                    return true;
                default:
                    return false;
            }

        case HOLD:
            switch (newState) {
                case ACTIVE:
                    return true;
                default:
                    return false;
            }

        default:
            return false;
    }
}

bool
Call::setState(CallState state)
{
    std::lock_guard<std::mutex> lock(callMutex_);
    if (not validTransition(state)) {
        static const char *states[] = {"INACTIVE", "ACTIVE", "HOLD", "BUSY", "ERROR"};
        assert(callState_ < RING_ARRAYSIZE(states) and state < RING_ARRAYSIZE(states));

        RING_ERR("Invalid call state transition from %s to %s",
              states[callState_], states[state]);
        return false;
    }

    callState_ = state;
    return true;
}

Call::CallState
Call::getState()
{
    std::lock_guard<std::mutex> lock(callMutex_);
    return callState_;
}

std::string
Call::getStateStr()
{
    switch (getState()) {
        case ACTIVE:
            switch (getConnectionState()) {
                case RINGING:
                    return isIncoming() ? "INCOMING" : "RINGING";
                case CONNECTED:
                default:
                    return "CURRENT";
            }

        case HOLD:
            return "HOLD";
        case BUSY:
            return "BUSY";
        case INACTIVE:

            switch (getConnectionState()) {
                case RINGING:
                    return isIncoming() ? "INCOMING" : "RINGING";
                case CONNECTED:
                    return "CURRENT";
                default:
                    return "INACTIVE";
            }

        case MERROR:
        default:
            return "FAILURE";
    }
}

IpAddr
Call::getLocalIp() const
{
    std::lock_guard<std::mutex> lock(callMutex_);
    return localAddr_;
}

unsigned int
Call::getLocalAudioPort() const
{
    std::lock_guard<std::mutex> lock(callMutex_);
    return localAudioPort_;
}

unsigned int
Call::getLocalVideoPort() const
{
    std::lock_guard<std::mutex> lock(callMutex_);
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
        case INCOMING:
            return "incoming";
        case OUTGOING:
            return "outgoing";
        case MISSED:
            return "missed";
        default:
            return "";
    }
}

static std::string
timestamp_to_string(const time_t &timestamp)
{
    std::stringstream time_str;
    time_str << timestamp;
    return time_str.str();
}

std::map<std::string, std::string>
Call::getDetails()
{
    std::map<std::string, std::string> details;
    std::ostringstream type;
    type << type_;
    details["CALL_TYPE"] = type.str();
    details["PEER_NUMBER"] = peerNumber_;
    details["DISPLAY_NAME"] = displayName_;
    details["CALL_STATE"] = getStateStr();
    details["CONF_ID"] = confID_;
    details["TIMESTAMP_START"] = timestamp_to_string(timestamp_start_);
    details["ACCOUNTID"] = getAccountId();
    return details;
}

std::map<std::string, std::string>
Call::getNullDetails()
{
    std::map<std::string, std::string> details;
    details["CALL_TYPE"] = "0";
    details["PEER_NUMBER"] = "Unknown";
    details["DISPLAY_NAME"] = "Unknown";
    details["CALL_STATE"] = "UNKNOWN";
    details["CONF_ID"] = "";
    details["TIMESTAMP_START"] = "";
    details["ACCOUNTID"] = "";
    return details;
}

void
Call::initIceTransport(bool master, unsigned channel_num)
{
    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    iceTransport_ = iceTransportFactory.createTransport(getCallId().c_str(), channel_num,
                                                        master, account_.getUPnPActive());
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

IceSocket*
Call::newIceSocket(unsigned compId) const
{
    return new IceSocket(iceTransport_, compId);
}

} // namespace ring
