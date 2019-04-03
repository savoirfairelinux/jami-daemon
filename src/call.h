/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logger.h"

#include "recordable.h"
#include "ip_utils.h"

#include <mutex>
#include <map>
#include <sstream>
#include <memory>
#include <vector>
#include <condition_variable>
#include <set>
#include <list>
#include <functional>

namespace jami {

class VoIPLink;
class Account;
struct AccountVideoCodecInfo;

template <class T> using CallMap = std::map<std::string, std::shared_ptr<T> >;

/*
 * @file call.h
 * @brief A call is the base class for protocol-based calls
 */

class Call : public Recordable, public std::enable_shared_from_this<Call> {
    public:
        using SubcallSet = std::set<std::shared_ptr<Call>, std::owner_less<std::shared_ptr<Call>>>;

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
            INACTIVE, ACTIVE, HOLD, BUSY, PEER_BUSY, MERROR, OVER, COUNT__
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

        virtual std::map<std::string, std::string> getDetails() const;
        static std::map<std::string, std::string> getNullDetails();

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
        virtual bool onhold() = 0;

        /**
         * Resume a call from hold state
         * @return bool True on success
         */
        virtual bool offhold() = 0;

        virtual void sendKeyframe() = 0;

        /**
         * Peer has hung up a call
         */
        virtual void peerHungup();

        virtual void removeCall();

        using StateListener = std::function<void(CallState, int)>;

        template<class T>
        void addStateListener(T&& list) {
            stateChangedListeners_.emplace_back(std::forward<T>(list));
        }

        /**
         * Attach subcall to this instance.
         * If this subcall is answered, this subcall and this instance will be merged using merge().
         */
        void addSubCall(Call& call);

        ///
        /// Return true if this call instance is a subcall (internal call for multi-device handling)
        ///
        bool isSubcall() const {
            std::lock_guard<std::recursive_mutex> lk {callMutex_};
            return parent_ != nullptr;
        }

    public: // media management
        virtual bool toggleRecording();

        virtual void switchInput(const std::string&) {};

        /**
         * mute/unmute a media of a call
         * @param mediaType type of media
         * @param isMuted true for muting, false for unmuting
         */
        virtual void muteMedia(const std::string& mediaType, bool isMuted)  = 0;

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


        void onTextMessage(std::map<std::string, std::string>&& messages);

        virtual bool useVideoCodec(const AccountVideoCodecInfo* /*codec*/) const {
            return false;
        }

        virtual void restartMediaSender() = 0;

        /**
         * Update call details after creation.
         * @param details to update
         *
         * \note No warranty to update any details, only some details can be modified.
         *       See the implementation for more ... details :-).
         */
        void updateDetails(const std::map<std::string, std::string>& details);

    protected:
        virtual void merge(Call& scall);

        /**
         * Constructor of a call
         * @param id Unique identifier of the call
         * @param type set definitely this call as incoming/outgoing
         * @param details volatile details to customize the call creation
         */
        Call(Account& account, const std::string& id, Call::CallType type,
             const std::map<std::string, std::string>& details = {});

        // TODO all these members are not protected against multi-thread access

        bool isAudioMuted_{false};
        bool isVideoMuted_{false};

        ///< MultiDevice: parent call, nullptr otherwise. Access protected by callMutex_.
        mutable std::shared_ptr<Call> parent_;

        ///< MultiDevice: list of attached subcall
        SubcallSet subcalls_;

        using MsgList = std::list<std::pair<std::map<std::string, std::string>, std::string>>;

        ///< MultiDevice: message waiting to be sent (need a valid subcall)
        MsgList pendingOutMessages_;

        /** Protect every attribute that can be changed by two threads */
        mutable std::recursive_mutex callMutex_ {};

    private:
        bool validStateTransition(CallState newState);

        void checkPendingIM();

        void checkAudio();

        void subcallStateChanged(Call&, Call::CallState, Call::ConnectionState);

        SubcallSet safePopSubcalls();

        std::vector<std::function<void(CallState, ConnectionState, int)>> stateChangedListeners_ {};

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

        ///< MultiDevice: message received by subcall to merged yet
        MsgList pendingInMessages_;
};

// Helpers

/**
 * Obtain a shared smart pointer of instance
 */
inline std::shared_ptr<Call> getPtr(Call& call)
{
    return call.shared_from_this();
}

} // namespace jami
