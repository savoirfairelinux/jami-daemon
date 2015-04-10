/*
 *  Copyright (C) 2014-2015 Savoir-Faire Linux Inc.
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

#ifndef RINGACCOUNT_H
#define RINGACCOUNT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/sipaccountbase.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "ring_types.h" // enable_if_base_of

#include <opendht/dhtrunner.h>

#include <pjsip/sip_types.h>

#include <vector>
#include <map>
#include <chrono>
#include <list>

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */

namespace YAML {
class Node;
class Emitter;
}

namespace ring {

namespace Conf {
    const char *const DHT_PORT_KEY = "dhtPort";
    const char *const DHT_VALUES_PATH_KEY = "dhtValuesPath";
}

namespace tls {
class GnuTlsGlobalInit;
} // namespace tls

class IceTransport;

class RingAccount : public SIPAccountBase {
    public:
        constexpr static const char * const ACCOUNT_TYPE = "RING";
        constexpr static const in_port_t DHT_DEFAULT_PORT = 4222;
        constexpr static const char * const DHT_DEFAULT_BOOTSTRAP = "bootstrap.ring.cx";
        /* constexpr */ static const std::pair<uint16_t, uint16_t> DHT_PORT_RANGE;

        const char* getAccountType() const {
            return ACCOUNT_TYPE;
        }

        /**
         * Constructor
         * @param accountID The account identifier
         */
        RingAccount(const std::string& accountID, bool presenceEnabled);

        ~RingAccount();

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
         * In the current version of Ring, "srv" uri is obtained in the preformated
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
        pj_str_t getContactHeader(pjsip_transport* = nullptr);

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

        /* Returns true if the username and/or hostname match this account */
        MatchRank matches(const std::string &username, const std::string &hostname) const;

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

        /**
         * Implementation of Account::newOutgoingCall()
         * Note: keep declaration before newOutgoingCall template.
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl);

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
         * @param[in] from The origin of the call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        virtual std::shared_ptr<SIPCall>
        newIncomingCall(const std::string& from = {});

        virtual bool isTlsEnabled() const {
            return true;
        }

        virtual sip_utils::KeyExchangeProtocol getSrtpKeyExchange() const {
            return sip_utils::KeyExchangeProtocol::SDES;
        }

        virtual bool getSrtpFallback() const {
            return false;
        }

        void registerCA(const dht::crypto::Certificate&);
        bool unregisterCA(const dht::InfoHash&);
        std::vector<std::string> getRegistredCAs();

    private:

        void doRegister_();

        const dht::ValueType USER_PROFILE_TYPE = {9, "User profile", std::chrono::hours(24 * 7)};
        //const dht::ValueType ICE_ANNOUCEMENT_TYPE = {10, "ICE descriptors", std::chrono::minutes(3)};

        NON_COPYABLE(RingAccount);

        void handleEvents();

        void createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to_id, IpAddr target);

        /**
         * Set the internal state for this account, mainly used to manage account details from the client application.
         * @param The map containing the account information.
         */
        virtual void setAccountDetails(const std::map<std::string, std::string> &details);

        /**
         * Start a SIP Call
         * @param call  The current call
         * @return true if all is correct
         */
        bool SIPStartCall(const std::shared_ptr<SIPCall>& call, IpAddr target);

        void regenerateCAList();

        /**
         * Maps require port via UPnP
         */
        bool mapPortUPnP();

        dht::DhtRunner dht_ {};

        struct PendingCall {
            std::chrono::steady_clock::time_point start;
            std::shared_ptr<IceTransport> ice;
            std::weak_ptr<SIPCall> call;
            std::future<size_t> listen_key;
            dht::InfoHash call_key;
            dht::InfoHash id;
        };

        /**
         * DHT calls waiting for ICE negotiation
         */
        std::list<PendingCall> pendingCalls_ {};
        /**
         * Incoming DHT calls that are not yet actual SIP calls.
         */
        std::list<PendingCall> pendingSipCalls_ {};
        std::set<dht::Value::Id> treatedCalls_ {};
        mutable std::mutex callsMutex_ {};

        std::string idPath_ {};
        std::string cachePath_ {};
        std::string dataPath_ {};
        std::string caPath_ {};
        std::string caListPath_ {};

        /**
         * Validate the values for privkeyPath_ and certPath_.
         * If one of these fields is empty, reset them to the default values.
         */
        void checkIdentityPath();

        void saveIdentity(const dht::crypto::Identity id, const std::string& path) const;
        void saveNodes(const std::vector<dht::Dht::NodeExport>&) const;
        void saveValues(const std::vector<dht::Dht::ValuesExport>&) const;

        void loadTreatedCalls();
        void saveTreatedCalls() const;

        /**
         * If privkeyPath_ is a valid private key file (PEM or DER),
         * and certPath_ a valid certificate file, load and returns them.
         * Otherwise, generate a new identity and returns it.
         */
        std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::crypto::Identity> loadIdentity();
        std::vector<dht::Dht::NodeExport> loadNodes() const;
        std::vector<dht::Dht::ValuesExport> loadValues() const;

        /**
         * Initializes tls settings from configuration file.
         */
        void initTlsConfiguration();

        /**
         * DHT port preference
         */
        in_port_t dhtPort_ {};

        /**
         * DHT port actually used,
         * this holds the actual port used for DHT, which may not be the port
         * selected in the configuration in the case that UPnP is used and the
         * configured port is already used by another client
         */
        UsedPort dhtPortUsed_ {};

        /**
         * The TLS settings, used only if tls is chosen as a sip transport.
         */
        void generateDhParams();

        std::shared_future<std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&>> dhParams_;
        std::mutex dhParamsMtx_;
        std::condition_variable dhParamsCv_;

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
        pjsip_host_port via_addr_;

        char contactBuffer_[PJSIP_MAX_URL_SIZE] {};
        pj_str_t contact_ {contactBuffer_, 0};
        pjsip_transport *via_tp_ {nullptr};

        std::unique_ptr<tls::GnuTlsGlobalInit> gtlsGIG_;
};

} // namespace ring

#endif
