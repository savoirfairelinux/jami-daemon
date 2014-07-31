/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef IAXACCOUNT_H
#define IAXACCOUNT_H

#include "account.h"
#include "iaxvoiplink.h"
#include "noncopyable.h"

/**
 * @file: iaxaccount.h
 * @brief An IAX Account specify IAX specific functions and objects (IAXCall/IAXVoIPLink)
 */
class IAXAccount : public Account {
    public:
        constexpr static const char * const ACCOUNT_TYPE = "IAX";

        IAXAccount(const std::string& accountID);

        const char* getAccountType() const {
            return ACCOUNT_TYPE;
        }

        /**
         * Create a new outgoing call
         * @param id  The ID of the call
         * @param toUrl The address to call
         * @return Call*  A pointer on the call
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& id,
                                              const std::string& toUrl);

        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

        std::map<std::string, std::string> getAccountDetails() const;

        void setNextRefreshStamp(int value) {
            nextRefreshStamp_ = value;
        }

        // Actually useless, since config loading is done in init()
        void loadConfig();

        // Register an account
        void registerVoIPLink();

        // Unregister an account
        void unregisterVoIPLink(std::function<void(bool)> cb = std::function<void(bool)>());

        /**
         * Send out registration
         */
        void sendRegister();

        /**
         * Destroy registration session
         * @todo Send an IAX_COMMAND_REGREL to force unregistration upstream.
         *       Urgency: low
         */
        void sendUnregister(std::function<void(bool)> cb = std::function<void(bool)>());

        std::string getPassword() const {
            return password_;
        }

        iax_session* getRegSession() const {
            return regSession_;
        }

        void destroyRegSession();

        void checkRegister();

        VoIPLink* getVoIPLink() {
            return &link_;
        }

    private:
        NON_COPYABLE(IAXAccount);

        void setAccountDetails(const std::map<std::string, std::string> &details);

        /**
         * Send an outgoing call invite to iax
         * @param call An IAXCall pointer
         */
        void iaxOutgoingInvite(IAXCall* call);

        /** registration session : nullptr if not register */
        iax_session* regSession_ = nullptr;

         // Account login information: password
        std::string password_;
        IAXVoIPLink link_;

        /** Timestamp of when we should refresh the registration up with
         * the registrar.  Values can be: EPOCH timestamp, 0 if we want no registration, 1
         * to force a registration. */
        int nextRefreshStamp_ = 0;
};

#endif
