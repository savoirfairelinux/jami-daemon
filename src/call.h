/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef __CALL_H__
#define __CALL_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logger.h"

#include "recordable.h"
#include "ip_utils.h"
#include "ice_transport.h"

#include <mutex>
#include <map>
#include <sstream>
#include <memory>
#include <vector>
#include <condition_variable>

namespace ring {

class VoIPLink;
class Account;
class AccountVideoCodecInfo;

template <class T> using CallMap = std::map<std::string, std::shared_ptr<T> >;

/*
 * @file call.h
 * @brief A call is the base class for protocol-based calls
 */

class Call : public Recordable, public std::enable_shared_from_this<Call> {
    public:
        static const char * const DEFAULT_ID;

        /**
         * This determines if the call originated from the local user (OUTGOING)
         * or from some remote peer (INCOMING, MISSED).
         */
        enum class CallType : unsigned {INCOMING, OUTGOING, MISSED};

        /**
         * Tell where we're at with the call. The call gets Connected when we know
         * from the other end what happened with out call. A call can be 'Connected'
         * even if the call state is Busy, or Error.
         *
         * Audio should be transmitted when ConnectionState = Connected AND
         * CallState = Active.
         *
         * \note modify validStateTransition/getStateStr if this enum changes
         */
        enum class ConnectionState : unsigned {
            DISCONNECTED, TRYING, PROGRESSING, RINGING, CONNECTED, COUNT__
        };

        /**
         * The Call State.
         *
         * \note modify validStateTransition/getStateStr if this enum changes
         */
        enum class CallState : unsigned {
            INACTIVE, ACTIVE, HOLD, BUSY, MERROR, OVER, COUNT__
        };

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

        virtual const char* getLinkType() const = 0;

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
        void setPeerDisplayName(const std::string& name) {
            peerDisplayName_ = name;
        }

        /**
         * Get the peer display name (caller in ingoing)
         * not protected by mutex (when created)
         * @return std::string The peer name
         */
        const std::string& getPeerDisplayName() const {
            return peerDisplayName_;
        }

        /**
         * Tell if the call is incoming
         * @return true if yes false otherwise
         */
        bool isIncoming() const {
            return type_ == CallType::INCOMING;
        }

        /**
         * Set the state of the call (protected by mutex)
         * @param call_state The call state
         * @param cnx_state The call connection state
         * @param code Optionnal state dependent error code (used to report more information)
         * @return true if the requested state change was valid, false otherwise
         */
        bool setState(CallState call_state, signed code=0);
        bool setState(CallState call_state, ConnectionState cnx_state, signed code=0);
        bool setState(ConnectionState cnx_state, signed code=0);

        /**
         * Get the call state of the call (protected by mutex)
         * @return CallState  The call state
         */
        CallState getState() const;

        /**
         * Get the connection state of the call (protected by mutex)
         * @return ConnectionState The connection state
         */
        ConnectionState getConnectionState() const;

        std::string getStateStr() const;

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

        virtual std::map<std::string, std::string> getDetails() const;
        static std::map<std::string, std::string> getNullDetails();

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

        virtual void switchInput(const std::string&) {};

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
        virtual bool onhold() = 0;

        /**
         * Resume a call from hold state
         * @return bool True on success
         */
        virtual bool offhold() = 0;

        /**
         * mute/unmute a media of a call
         * @param mediaType type of media
         * @param isMuted true for muting, false for unmuting
         */
        virtual void muteMedia(const std::string& mediaType, bool isMuted)  = 0;

        /**
         * Peer has hung up a call
         */
        virtual void peerHungup();

        /**
         * Send DTMF
         * @param code  The char code
         */
        virtual void carryingDTMFdigits(char code) = 0;

        /**
         * Send a message to a call identified by its callid
         *
         * @param A list of mimetype/payload pairs
         * @param The sender of this message (could be another participant of a conference)
         */
        virtual void sendTextMessage(const std::map<std::string, std::string>& messages,
                                     const std::string &from) = 0;

        void removeCall();

        virtual bool initIceTransport(bool master, unsigned channel_num=4);

        int waitForIceInitialization(unsigned timeout);

        int waitForIceNegotiation(unsigned timeout);

        bool isIceUsed() const;
        bool isIceRunning() const;

        std::unique_ptr<IceSocket> newIceSocket(unsigned compId);

        std::shared_ptr<IceTransport> getIceTransport() const {
            return iceTransport_;
        }

        virtual bool useVideoCodec(const AccountVideoCodecInfo* /*codec*/) const {
            return false;
        }

        virtual void restartMediaSender() = 0;

    protected:
        /**
         * Constructor of a call
         * @param id Unique identifier of the call
         * @param type set definitely this call as incoming/outgoing
         */
        Call(Account& account, const std::string& id, Call::CallType type);

        std::shared_ptr<IceTransport> iceTransport_ {};

        bool isAudioMuted_{false};
        bool isVideoMuted_{false};

    private:
        bool validStateTransition(CallState newState);

        /** Protect every attribute that can be changed by two threads */
        mutable std::recursive_mutex callMutex_ {};

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
        ConnectionState connectionState_ {ConnectionState::DISCONNECTED};

        /** Inactive/Active/Hold/Busy/Error */
        CallState callState_ {CallState::INACTIVE};

        /** Direct IP-to-IP or classic call */
        bool isIPToIP_ {false};

        /** Number of the peer */
        std::string peerNumber_ {};

        /** Peer Display Name */
        std::string peerDisplayName_ {};

        time_t timestamp_start_ {0};
};

} // namespace ring

#endif // __CALL_H__
