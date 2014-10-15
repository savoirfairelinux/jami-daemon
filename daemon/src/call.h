/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef __CALL_H__
#define __CALL_H__

#include "logger.h"

#include "audio/recordable.h"
#include "ip_utils.h"

#include <mutex>
#include <map>
#include <sstream>
#include <memory>
#include <vector>

class VoIPLink;
class Account;

template <class T> using CallMap = std::map<std::string, std::shared_ptr<T> >;

/*
 * @file call.h
 * @brief A call is the base class for protocol-based calls
 */

class Call : public sfl::Recordable {
    public:
        static const char * const DEFAULT_ID;

        /**
         * This determines if the call originated from the local user (OUTGOING)
         * or from some remote peer (INCOMING, MISSED).
         */
        enum CallType {INCOMING, OUTGOING, MISSED};

        /**
         * Tell where we're at with the call. The call gets Connected when we know
         * from the other end what happened with out call. A call can be 'Connected'
         * even if the call state is Busy, or Error.
         *
         * Audio should be transmitted when ConnectionState = Connected AND
         * CallState = Active.
         */
        enum ConnectionState {DISCONNECTED, TRYING, PROGRESSING, RINGING, CONNECTED};

        /**
         * The Call State.
         */
        enum CallState {INACTIVE, ACTIVE, HOLD, BUSY, ERROR};

        virtual ~Call();

        /**
         * Return a reference on the call id
         * @return call id
         */
        const std::string& getCallId() const {
            return id_;
        }

        /**
         * Return a reference on the conference id
         * @return call id
         */
        const std::string& getConfId() const {
            return confID_;
        }

        void setConfId(const std::string &id) {
            confID_ = id;
        }

        Account& getAccount() const { return account_; }
        const std::string& getAccountId() const;

        CallType getCallType() const {
            return type_;
        }

        /**
         * Set the peer number (destination on outgoing)
         * not protected by mutex (when created)
         * @param number peer number
         */
        void setPeerNumber(const std::string& number) {
            peerNumber_ = number;
        }

        /**
         * Get the peer number (destination on outgoing)
         * not protected by mutex (when created)
         * @return std::string The peer number
         */
        std::string getPeerNumber() const {
            return peerNumber_;
        }

        /**
         * Set the display name (caller in ingoing)
         * not protected by mutex (when created)
         * @return std::string The peer display name
         */
        void setDisplayName(const std::string& name) {
            displayName_ = name;
        }

        /**
         * Get the peer display name (caller in ingoing)
         * not protected by mutex (when created)
         * @return std::string The peer name
         */
        const std::string& getDisplayName() const {
            return displayName_;
        }

        /**
         * Tell if the call is incoming
         * @return true if yes false otherwise
         */
        bool isIncoming() const {
            return type_ == INCOMING;
        }

        /**
         * Set the connection state of the call (protected by mutex)
         * @param state The connection state
         */
        void setConnectionState(ConnectionState state);

        /**
         * Get the connection state of the call (protected by mutex)
         * @return ConnectionState The connection state
         */
        ConnectionState getConnectionState();

        /**
         * Set the state of the call (protected by mutex)
         * @param state The call state
         * @return true if the requested state change was valid, false otherwise
         */
        bool setState(CallState state);

        /**
         * Get the call state of the call (protected by mutex)
         * @return CallState  The call state
         */
        CallState getState();

        std::string getStateStr();

        void setIPToIP(bool IPToIP) {
            isIPToIP_ = IPToIP;
        }

        /**
         * Set my IP [not protected]
         * @param ip  The local IP address
         */
        void setLocalIp(const IpAddr& ip) {
            localAddr_ = ip;
        }

        /**
         * Set local audio port, as seen by me [not protected]
         * @param port  The local audio port
         */
        void setLocalAudioPort(unsigned int port) {
            localAudioPort_ = port;
        }

        /**
         * Set local video port, as seen by me [not protected]
         * @param port  The local video port
         */
        void setLocalVideoPort(unsigned int port)  {
            localVideoPort_ = port;
        }

        /**
         * Return my IP [mutex protected]
         * @return std::string The local IP
         */
        IpAddr getLocalIp() const;

        /**
         * Return port used locally (for my machine) [mutex protected]
         * @return unsigned int  The local audio port
         */
        unsigned int getLocalAudioPort() const;

        /**
         * Return port used locally (for my machine) [mutex protected]
         * @return unsigned int  The local video port
         */
        unsigned int getLocalVideoPort() const;

        void time_stop();
        virtual std::map<std::string, std::string> getDetails();
        static std::map<std::string, std::string> getNullDetails();

        virtual std::map<std::string, std::string>
        createHistoryEntry() const;

        virtual bool toggleRecording();

        /**
         * Answer the call
         */
        virtual void answer() = 0;

        /**
         * Hang up the call
         * @param reason
         */
        virtual void hangup(int reason) = 0;

        /**
         * Refuse incoming call
         */
        virtual void refuse() = 0;

        /**
         * Transfer a call to specified URI
         * @param to The recipient of the call
         */
        virtual void transfer(const std::string &to) = 0;

        /**
         * Attended transfer
         * @param The target call id
         * @return True on success
         */
        virtual bool attendedTransfer(const std::string& to) = 0;

        /**
         * Put a call on hold
         * @return bool True on success
         */
        virtual void onhold() = 0;

        /**
         * Resume a call from hold state
         * @return bool True on success
         */
        virtual void offhold() = 0;

        /**
         * Peer Hung up a call
         */
        virtual void peerHungup() = 0;

        /**
         * Send DTMF
         * @param code  The char code
         */
        virtual void carryingDTMFdigits(char code) = 0;

#if HAVE_INSTANT_MESSAGING
        /**
         * Send a message to a call identified by its callid
         *
         * @param The actual message to be transmitted
         * @param The sender of this message (could be another participant of a conference)
         */
        virtual void sendTextMessage(const std::string &message,
                                     const std::string &from) = 0;
#endif

        void removeCall();

    protected:
        /**
         * Constructor of a call
         * @param id Unique identifier of the call
         * @param type set definitely this call as incoming/outgoing
         */
        Call(Account& account, const std::string& id, Call::CallType type);

    private:
        bool validTransition(CallState newState);

        std::string getTypeStr() const;
        /** Protect every attribute that can be changed by two threads */
        mutable std::mutex callMutex_ {};

        // Informations about call socket / audio

        /** My IP address */
        IpAddr localAddr_ {};

        /** Local audio port, as seen by me. */
        unsigned int localAudioPort_ {0};

        /** Local video port, as seen by me. */
        unsigned int localVideoPort_ {0};

        /** Unique ID of the call */
        std::string id_;

        /** Unique conference ID, used exclusively in case of a conferece */
        std::string confID_ {};

        /** Type of the call */
        CallType type_;

        /** Associate account ID */
        Account& account_;

        /** Disconnected/Progressing/Trying/Ringing/Connected */
        ConnectionState connectionState_ {Call::DISCONNECTED};

        /** Inactive/Active/Hold/Busy/Error */
        CallState callState_ {Call::INACTIVE};

        /** Direct IP-to-IP or classic call */
        bool isIPToIP_ {false};

        /** Number of the peer */
        std::string peerNumber_ {};

        /** Display Name */
        std::string displayName_ {};

        time_t timestamp_start_ {0};
        time_t timestamp_stop_ {0};
};

#endif // __CALL_H__
