/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#ifndef DHTACCOUNT_H
#define DHTACCOUNT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/siptransport.h"
#include "sip/sipaccountbase.h"
#include "noncopyable.h"
#include "ip_utils.h"
#include "sfl_types.h" // enable_if_base_of
#include "dhtcpp/dhtrunner.h"

#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_types.h>
#include <pjsip-ua/sip_regc.h>

#include <vector>
#include <map>

namespace Conf {
    const char *const DHT_NODE_PATH_KEY = "dhtNodePath";
    const char *const DHT_PRIVKEY_PATH_KEY = "dhtPrivkeyPath";
    const char *const DHT_CERT_PATH_KEY = "dhtPubkeyPath";
}

namespace YAML {
    class Node;
    class Emitter;
}

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */
class DHTAccount : public SIPAccountBase {
    public:
        constexpr static const char * const ACCOUNT_TYPE = "DHT";

        const char* getAccountType() const {
            return ACCOUNT_TYPE;
        }

        /**
         * Constructor
         * @param accountID The account identifier
         */
        DHTAccount(const std::string& accountID, bool presenceEnabled);

        ~DHTAccount();

        /**
         * Serialize internal state of this account for configuration
         * @param YamlEmitter the configuration engine which generate the configuration file
         */
        virtual void serialize(YAML::Emitter &out);

        /**
         * Populate the internal state for this account based on info stored in the configuration file
         * @param The configuration node for this account
         */
        virtual void unserialize(const YAML::Node &node);

        /**
         * Return an map containing the internal state of this account. Client application can use this method to manage
         * account info.
         * @return A map containing the account information.
         */
        virtual std::map<std::string, std::string> getAccountDetails() const;

        /**
         * Actually useless, since config loading is done in init()
         */
        void loadConfig();

        /**
         * Connect to the DHT.
         */
        void doRegister();

        /**
         * Disconnect from the DHT.
         */
        void doUnregister(std::function<void(bool)> cb = std::function<void(bool)>());

        /**
         * @return pj_str_t "From" uri based on account information.
         * From RFC3261: "The To header field first and foremost specifies the desired
         * logical" recipient of the request, or the address-of-record of the
         * user or resource that is the target of this request. [...]  As such, it is
         * very important that the From URI not contain IP addresses or the FQDN
         * of the host on which the UA is running, since these are not logical
         * names."
         */
        std::string getFromUri() const;

        /**
         * This method adds the correct scheme, hostname and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         * It is expected that "port" is present in the internal hostname_.
         *
         * @return pj_str_t "To" uri based on @param username
         * @param username A string formatted as : "username"
         */
        std::string getToUri(const std::string& username) const;

        /**
         * In the current version of SFLPhone, "srv" uri is obtained in the preformated
         * way: hostname:port. This method adds the correct scheme and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         *
         * @return pj_str_t "server" uri based on @param hostPort
         * @param hostPort A string formatted as : "hostname:port"
         */
        std::string getServerUri() const { return ""; };

        /**
         * Get the contact header for
         * @return pj_str_t The contact header based on account information
         */
        pj_str_t getContactHeader();

        void setReceivedParameter(const std::string &received) {
            receivedParameter_ = received;
            via_addr_.host.ptr = (char *) receivedParameter_.c_str();
            via_addr_.host.slen = receivedParameter_.size();
        }

        std::string getReceivedParameter() const {
            return receivedParameter_;
        }

        pjsip_host_port *
        getViaAddr() {
            return &via_addr_;
        }

        int getRPort() const {
            if (rPort_ == -1)
                return localPort_;
            else
                return rPort_;
        }

        void setRPort(int rPort) {
            rPort_ = rPort;
            via_addr_.port = rPort;
        }

        /* Returns true if the username and/or hostname match this account */
        MatchRank matches(const std::string &username, const std::string &hostname, pjsip_endpoint *endpt, pj_pool_t *pool) const;

#ifdef SFL_PRESENCE
        /**
         * Presence management
         */
        //SIPPresence * getPresence() const;

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
#endif

        /**
         * Implementation of Account::newOutgoingCall()
         * Note: keep declaration before newOutgoingCall template.
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& id,
                                              const std::string& toUrl);

        /**
         * Create outgoing SIPCall.
         * @param[in] id The ID of the call
         * @param[in] toUrl The address to call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        template <class T=SIPCall>
        std::shared_ptr<enable_if_base_of<T, SIPCall> >
        newOutgoingCall(const std::string& id, const std::string& toUrl);

        /**
         * Create incoming SIPCall.
         * @param[in] id The ID of the call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        virtual std::shared_ptr<SIPCall>
        newIncomingCall(const std::string& id);

        virtual bool isTlsEnabled() const {
            return true;
        }

        virtual bool getSrtpEnabled() const {
            return true;
        }

        virtual std::string getSrtpKeyExchange() const {
            return "sdes";
        }

        virtual bool getSrtpFallback() const {
            return false;
        }

    private:
        void createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to, const std::string& toUrl, const IpAddr& peer);

        /**
         * Set the internal state for this account, mainly used to manage account details from the client application.
         * @param The map containing the account information.
         */
        virtual void setAccountDetails(const std::map<std::string, std::string> &details);

        NON_COPYABLE(DHTAccount);

        /**
         * Start a SIP Call
         * @param call  The current call
         * @return true if all is correct
         */
        bool SIPStartCall(const std::shared_ptr<SIPCall>& call);

        bool fullMatch(const std::string &username, const std::string &hostname, pjsip_endpoint *endpt, pj_pool_t *pool) const;
        bool userMatch(const std::string &username) const;

        /**
         * @return pjsip_tls_setting structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * TLS transport.
         */
        pjsip_tls_setting * getTlsSetting() {
            return &tlsSetting_;
        }

        Dht::DhtRunner dht_ {};

        std::string nodePath_ {};
        std::string privkeyPath_ {};
        std::string certPath_ {};

        /**
         * If identityPath_ is a valid private key file (PEM or DER),
         * load and returns it. Otherwise, generate one.
         * Check if the given path contains a valid private key.
         * @return the key if a valid private key exists there, nullptr otherwise.
         */
        Dht::SecureDht::Identity loadIdentity() const;
        void saveIdentity(const Dht::SecureDht::Identity id) const;
        void saveNodes(const std::vector<Dht::Dht::NodeExport>& nodes) const;
        std::vector<Dht::Dht::NodeExport> loadNodes() const;

#if HAVE_TLS
        /**
         * Maps a string description of the SSL method
         * to the corresponding enum value in pjsip_ssl_method.
         * @param method The string representation
         * @return pjsip_ssl_method The corresponding value in the enum
         */
        static pjsip_ssl_method sslMethodStringToPjEnum(const std::string& method);

        /**
         * Initializes tls settings from configuration file.
         */
        void initTlsConfiguration();

        /**
         * PJSIP aborts if the string length of our cipher list is too
         * great, so this function forces our cipher list to fit this constraint.
         */
        void trimCiphers();

#endif

        /**
         * If username is not provided, as it happens for Direct ip calls,
         * fetch the Real Name field of the user that is currently
         * running this program.
         * @return std::string The login name under which SFLPhone is running.
         */
        static std::string getLoginName();

        /**
         * The TLS settings, used only if tls is chosen as a sip transport.
         */
        pjsip_tls_setting tlsSetting_ {};

        /**
         * Allocate a vector to be used by pjsip to store the supported ciphers on this system.
         */
        CipherArray ciphers_ {100};

        /**
         * Certificate autority file
         */
        std::string tlsCaListFile_ {};
        bool tlsVerifyServer_ {false};
        bool tlsVerifyClient_ {false};
        bool tlsRequireClientCertificate_ {false};
        std::string tlsNegotiationTimeoutSec_ {"2"};

        /**
         * Optional: "received" parameter from VIA header
         */
        std::string receivedParameter_ {};

        /**
         * Optional: "rport" parameter from VIA header
         */
        int rPort_ {-1};

        /**
         * Optional: via_addr construct from received parameters
         */
        pjsip_host_port via_addr_ {};

        char contactBuffer_[PJSIP_MAX_URL_SIZE] {};
        pj_str_t contact_ {contactBuffer_, 0};
        pjsip_transport *via_tp_ {nullptr};

};

#endif
