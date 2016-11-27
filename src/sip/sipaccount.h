/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#ifndef SIPACCOUNT_H
#define SIPACCOUNT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sipaccountdb.h"
#include "siptransport.h"
#include "noncopyable.h"
#include "ring_types.h" // enable_if_base_of

#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_types.h>
#include <pjsip-ua/sip_regc.h>

#include <vector>
#include <map>

namespace YAML {
    class Node;
    class Emitter;
}

namespace ring {

typedef std::vector<pj_ssl_cipher> CipherArray;

class SIPPresence;
class SIPCall;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */
class SIPAccount : public SIPAccountDB {
    public:
        constexpr static const char * const ACCOUNT_TYPE = "SIP";

        /**
         * Constructor
         * @param accountID The account identifier
         */
        SIPAccount(const std::string& accountID, bool presenceEnabled);

        ~SIPAccount();

        void updateDialogViaSentBy(pjsip_dialog *dlg);

        /**
         * Serialize internal state of this account for configuration
         * @param out Emitter to which state will be saved
         */
        virtual void serialize(YAML::Emitter &out) override;

        /**
         * Populate the internal state for this account based on info stored in the configuration file
         * @param The configuration node for this account
         */
        virtual void unserialize(const YAML::Node &node) override;

        /**
         * Return an map containing the internal state of this account. Client application can use this method to manage
         * account info.
         * @return A map containing the account information.
         */
        virtual std::map<std::string, std::string> getAccountDetails() const override;

        /**
         * Retrieve volatile details such as recent registration errors
         * @return std::map< std::string, std::string > The account volatile details
         */
        virtual std::map<std::string, std::string> getVolatileAccountDetails() const override;

        /**
         * Presence management
         */
        SIPPresence * getPresence() const;

        /**
         * Activate the module.
         * @param function Publish or subscribe to enable
         * @param enable Flag
         */
        void enablePresence(const bool& enable);
        /**
         * Activate the publish/subscribe.
         * @param enable Flag
         */
        void supportPresence(int function, bool enable);

        /**
         * Implementation of Account::newOutgoingCall()
         * Note: keep declaration before newOutgoingCall template.
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl) override;

        /**
         * Create outgoing SIPCall.
         * @param[in] toUrl The address to call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        template <class T=SIPCall>
        std::shared_ptr<enable_if_base_of<T, SIPCall> >
        newOutgoingCall(const std::string& toUrl);

        /**
         * Create incoming SIPCall.
         * @param[in] from The origin uri of the call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        std::shared_ptr<SIPCall>
        newIncomingCall(const std::string& from) override;

        virtual void sendTextMessage(const std::string& to,
                                     const std::map<std::string, std::string>& payloads,
                                     uint64_t id) override;

    protected:
        void doRegisterIPToIP();

        /**
         * Set the internal state for this account, mainly used to manage account details from the client application.
         * @param The map containing the account information.
         */
        void setAccountDetails(const std::map<std::string, std::string> &details) override;

        NON_COPYABLE(SIPAccount);

        std::shared_ptr<Call> newRegisteredAccountCall(const std::string& id,
                                                       const std::string& toUrl);

        /**
         * Start a SIP Call
         * @param call  The current call
         * @return true if all is correct
         */
        bool SIPStartCall(std::shared_ptr<SIPCall>& call);

        /**
         * Presence data structure
         */
        SIPPresence * presence_;
};

} // namespace ring

#endif
