/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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
#include "history/historyitem.h"

#include "sip/sip_utils.h"
#include "ip_utils.h"
#include "array_size.h"
#include "map_utils.h"
#include "call_factory.h"

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
        assert(callState_ < ARRAYSIZE(states) and state < ARRAYSIZE(states));

        ERROR("Invalid call state transition from %s to %s",
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

        case ERROR:
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

std::map<std::string, std::string> Call::createHistoryEntry() const
{
    using sfl::HistoryItem;
    std::map<std::string, std::string> result;

    result[HistoryItem::ACCOUNT_ID_KEY] = getAccountId();
    result[HistoryItem::CONFID_KEY] = confID_;
    result[HistoryItem::CALLID_KEY] = id_;
    result[HistoryItem::DISPLAY_NAME_KEY] = displayName_;
    result[HistoryItem::PEER_NUMBER_KEY] = peerNumber_;
    result[HistoryItem::RECORDING_PATH_KEY] = recAudio_.fileExists() ? getFilename() : "";
    result[HistoryItem::TIMESTAMP_START_KEY] = timestamp_to_string(timestamp_start_);
    result[HistoryItem::TIMESTAMP_STOP_KEY] = timestamp_to_string(timestamp_stop_);

    // FIXME: state will no longer exist, it will be split into
    // a boolean field called "missed" and a direction field "incoming" or "outgoing"
    if (connectionState_ == RINGING) {
        result[HistoryItem::STATE_KEY] = HistoryItem::MISSED_STRING;
        result[HistoryItem::MISSED_KEY] = "true";
    } else {
        result[HistoryItem::STATE_KEY] = getTypeStr();
        result[HistoryItem::MISSED_KEY] = "false";
    }

    // now "missed" and direction are independent
    result[HistoryItem::DIRECTION_KEY] = getTypeStr();

    return result;
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
