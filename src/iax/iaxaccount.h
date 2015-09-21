/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef IAXACCOUNT_H
#define IAXACCOUNT_H

#include "account.h"
#include "iaxvoiplink.h"
#include "ring_types.h" // enable_if_base_of

namespace YAML {
    class Emitter;
    class Node;
}

namespace ring {

class IAXCall;

/**
 * @file: iaxaccount.h
 * @brief An IAX Account specify IAX specific functions and objects (IAXCall/IAXVoIPLink)
 */
class IAXAccount : public Account {
    public:
        constexpr static const char * const ACCOUNT_TYPE = "IAX";

        IAXAccount(const std::string& accountID);

        virtual void serialize(YAML::Emitter &out);
        virtual void unserialize(const YAML::Node &node);

        const char* getAccountType() const {
            return ACCOUNT_TYPE;
        }

        std::map<std::string, std::string> getAccountDetails() const;

        virtual std::map<std::string, std::string> getVolatileAccountDetails() const;

        void setNextRefreshStamp(int value) {
            nextRefreshStamp_ = value;
        }

        // Actually useless, since config loading is done in init()
        void loadConfig();

        // Register an account
        void doRegister();

        // Unregister an account
        void doUnregister(std::function<void(bool)> cb = std::function<void(bool)>());

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

        bool matchRegSession(const iax_session* session) const;

        void destroyRegSession();

        void checkRegister();

        /**
         * Implementation of Account::newOutgoingCall()
         * Note: keep declaration before newOutgoingCall template.
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl);

        /**
         * Create outgoing IAXCall.
         * @param[in] toUrl The address to call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of IAXCall class (included).
         */
        template <class T=IAXCall>
        std::shared_ptr<enable_if_base_of<T, IAXCall> >
        newOutgoingCall(const std::string& toUrl);

        /**
         * Create incoming IAXCall.
         * @param[in] from The origin uri of the call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of IAXCall class (included).
         */
        template <class T=IAXCall>
        std::shared_ptr<enable_if_base_of<T, IAXCall> >
        newIncomingCall(const std::string& from);

        /**
         * Set whether or not to use UPnP
         */
        void setUseUPnP(bool) {
            /* do nothing for now as UPnP isn't implemented for IAX */
        }

    private:

        void setAccountDetails(const std::map<std::string, std::string> &details);

        /**
         * Send an outgoing call invite to iax
         * @param call An IAXCall pointer
         */
        void iaxOutgoingInvite(IAXCall* call);

        /** registration session : nullptr if not register */
        struct RegSessionDeleter {
                void operator()(iax_session* session) { iax_destroy(session); }
        };
        std::unique_ptr<iax_session, RegSessionDeleter> regSession_ = nullptr;

         // Account login information: password
        std::string password_{};
        std::unique_ptr<IAXVoIPLink> link_;

        /** Timestamp of when we should refresh the registration up with
         * the registrar.  Values can be: EPOCH timestamp, 0 if we want no registration, 1
         * to force a registration. */
        int nextRefreshStamp_ = 0;
};

} // namespace ring

#endif
