/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#ifndef __VOIP_LINK_H__
#define __VOIP_LINK_H__

#include <stdexcept>
#include <functional>
#include <string>
#include <vector>
#include <memory>

class Call;
class Account;

class VoipLinkException : public std::runtime_error {
    public:
        VoipLinkException(const std::string &str = "") :
            std::runtime_error("UserAgent: VoipLinkException occured: " + str) {}
};

/**
 * @file voiplink.h
 * @brief Listener and manager interface for each VoIP protocol
 */
class VoIPLink {
    public:
        VoIPLink();
        virtual ~VoIPLink();

        /**
         * Virtual method
         * Event listener. Each event send by the call manager is received and handled from here
         */
        virtual bool handleEvents() = 0;

        /**
         * Virtual method
         * Build and send account registration request
         */
        virtual void sendRegister(Account& a) = 0;

        /**
         * Virtual method
         * Build and send account unregistration request
         * Underlying ressources are released after unregistration
         */
        virtual void sendUnregister(Account& a, std::function<void(bool)> cb = std::function<void(bool)>()) = 0;

        /**
         * Place a new call
         * @param id  The call identifier
         * @param toUrl  The address of the recipient of the call
         * @return Call* The current call
         */
        virtual std::shared_ptr<Call> newOutgoingCall(const std::string &id,
                                                      const std::string &toUrl,
                                                      const std::string &account_id) = 0;

        /**
         * Virtual method
         * Returns calls involving this account.
         */
        virtual std::vector<std::shared_ptr<Call> > getCalls(const std::string &account_id) const = 0;

        /**
         * Answer the call
         * @param c The call
         */
        virtual void answer(Call *c) = 0;

        /**
         * Hang up a call
         * @param id The call identifier
         */
        virtual void hangup(const std::string &id, int reason) = 0;

        /**
        * Peer Hung up a call
        * @param id The call identifier
        */
        virtual void peerHungup(const std::string &id) = 0;

        /**
         * Put a call on hold
         * @param id The call identifier
         * @return bool True on success
         */
        virtual void onhold(const std::string &id) = 0;

        /**
         * Resume a call from hold state
         * @param id The call identifier
         * @return bool True on success
         */
        virtual void offhold(const std::string &id) = 0;

        /**
         * Transfer a call to specified URI
         * @param id The call identifier
         * @param to The recipient of the call
         */
        virtual void transfer(const std::string &id, const std::string &to) = 0;

        /**
         * Attended transfer
         * @param The transfered call id
         * @param The target call id
         * @return True on success
         */
        virtual bool attendedTransfer(const std::string&, const std::string&) = 0;

        /**
         * Refuse incoming call
         * @param id The call identifier
         */
        virtual void refuse(const std::string &id) = 0;

        /**
         * Send DTMF
         * @param id The call identifier
         * @param code  The char code
         */
        virtual void carryingDTMFdigits(const std::string &id, char code) = 0;

        /**
         * Send a message to a call identified by its callid
         *
         * @param The Id of the call to send the message to
         * @param The actual message to be transmitted
         * @param The sender of this message (could be another participant of a conference)
         */
#if HAVE_INSTANT_MESSAGING
        virtual void sendTextMessage(const std::string &callID,
                                     const std::string &message,
                                     const std::string &from) = 0;
#endif

    protected:
        static void unloadAccount(std::pair<const std::string, Account*> &item);
        bool handlingEvents_;
};

#endif // __VOIP_LINK_H__
