/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include "sipaccountbase.h"
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

namespace jami {

namespace Conf {
    const char *const KEEP_ALIVE_ENABLED = "keepAlive";

    // TODO: write an object to store credential which implement serializable
    const char *const SRTP_KEY = "srtp";
    const char *const SRTP_ENABLE_KEY = "enable";
    const char *const KEY_EXCHANGE_KEY = "keyExchange";
    const char *const RTP_FALLBACK_KEY = "rtpFallback";
}

typedef std::vector<pj_ssl_cipher> CipherArray;

class SIPPresence;
class SIPCall;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */
class SIPAccount : public SIPAccountBase {
    public:
        constexpr static const char * const ACCOUNT_TYPE = "SIP";

        std::shared_ptr<SIPAccount> shared() {
            return std::static_pointer_cast<SIPAccount>(shared_from_this());
        }
        std::shared_ptr<SIPAccount const> shared() const {
            return std::static_pointer_cast<SIPAccount const>(shared_from_this());
        }
        std::weak_ptr<SIPAccount> weak() {
            return std::static_pointer_cast<SIPAccount>(shared_from_this());
        }
        std::weak_ptr<SIPAccount const> weak() const {
            return std::static_pointer_cast<SIPAccount const>(shared_from_this());
        }

        /**
         * Constructor
         * @param accountID The account identifier
         */
        SIPAccount(const std::string& accountID, bool presenceEnabled);

        ~SIPAccount();

        const char* getAccountType() const override {
            return ACCOUNT_TYPE;
        }

        pjsip_host_port getHostPortFromSTUN(pj_pool_t *pool);

        std::string getUserAgentName() const;
        void setRegistrationStateDetailed(const std::pair<int, std::string> &details) {
            registrationStateDetailed_ = details;
        }

        void updateDialogViaSentBy(pjsip_dialog *dlg);

        void resetAutoRegistration();
        bool checkNATAddress(pjsip_regc_cbparam *param, pj_pool_t *pool);

        /**
         * Returns true if this is the IP2IP account
         */
        bool isIP2IP() const override;

        /**
         * Serialize internal state of this account for configuration
         * @param out Emitter to which state will be saved
         */
        virtual void serialize(YAML::Emitter &out) const override;

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
         * Return the TLS settings, mainly used to return security information to
         * a client application
         */
        std::map<std::string, std::string> getTlsSettings() const;

        /**
         * Actually useless, since config loading is done in init()
         */
        void loadConfig() override;

        /**
         * Initialize the SIP voip link with the account parameters and send registration
         */
        void doRegister() override;

        /**
         * Send unregistration.
         */
        void doUnregister(std::function<void(bool)> cb = std::function<void(bool)>()) override;

        /**
         * Start the keep alive function, once started, the account will be registered periodically
         * a new REGISTER request is sent bey the client application. The account must be initially
         * registered for this call to be effective.
         */
        void startKeepAliveTimer();

        /**
         * Stop the keep alive timer. Once canceled, no further registration will be scheduled
         */
        void stopKeepAliveTimer();

        /**
         * Build and send SIP registration request
         */
        void sendRegister();

        /**
         * Build and send SIP unregistration request
         * @param destroy_transport If true, attempt to destroy the transport.
         */
        void sendUnregister();

        const pjsip_cred_info* getCredInfo() const {
            return cred_.data();
        }

        /**
         * Get the number of credentials defined for
         * this account.
         * @param none
         * @return int The number of credentials set for this account.
         */
        unsigned getCredentialCount() const {
            return credentials_.size();
        }

        bool hasCredentials() const {
            return not credentials_.empty();
        }

        void setCredentials(const std::vector<std::map<std::string, std::string> >& details);

        std::vector<std::map<std::string, std::string>>
        getCredentials() const;

        virtual void setRegistrationState(RegistrationState state, unsigned code=0, const std::string& detail_str={}) override;

        /**
         * A client sendings a REGISTER request MAY suggest an expiration
         * interval that indicates how long the client would like the
         * registration to be valid.
         *
         * @return the expiration value.
         */
        unsigned getRegistrationExpire() const {
            if (registrationExpire_ == 0)
                return PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;

            return registrationExpire_;
        }

        /**
         * Set the expiration for this account as found in
         * the "Expire" sip header or the CONTACT's "expire" param.
         */
        void setRegistrationExpire(int expire) {
            if (expire > 0)
                registrationExpire_ = expire;
        }

        /**
         * Doubles the Expiration Interval sepecified for registration.
         */
        void doubleRegistrationExpire() {
            registrationExpire_ *= 2;

            if (registrationExpire_ < 0)
                registrationExpire_ = 0;
        }

        /**
         * Registration flag
         */
        bool isRegistered() const {
            return bRegister_;
        }

        /**
         * Set registration flag
         */
        void setRegister(bool result) {
            bRegister_ = result;
        }

        /**
         * Get the registration structure that is used
         * for PJSIP in the registration process.
         * Settings are loaded from configuration file.
         * @return pjsip_regc* A pointer to the registration structure
         */
        pjsip_regc* getRegistrationInfo() {
            return regc_;
        }

        /**
         * Set the registration structure that is used
         * for PJSIP in the registration process;
         * @pram A pointer to the new registration structure
         * @return void
         */
        void setRegistrationInfo(pjsip_regc *regc) {
            if (regc_) destroyRegistrationInfo();
            regc_ = regc;
        }

        void destroyRegistrationInfo();

        /**
         * Get the port on which the transport/listener should use, or is
         * actually using.
         * @return pj_uint16 The port used for that account
         */
        pj_uint16_t getLocalPort() const {
            return localPort_;
        }

        /**
         * Set the new port on which this account is running over.
         * @pram port The port used by this account.
         */
        void setLocalPort(pj_uint16_t port) {
            localPort_ = port;
        }

        /**
         * Get the bind ip address on which the account should use, or is
         * actually using.
         * Note: if it is NULL, this address should not be used
         * @return std::string The bind ip address used for that account
         */
        std::string getSIPBindAddress() const {
            return sipBindAddress_;
        }

        /**
         * Set the new bind ip address on which this account is bind on.
         * @pram address The bind ip address used by this account.
         */
        void setSIPBindAddress(const std::string &address) {
            sipBindAddress_ = address;
        }

        /**
         * @return pjsip_tls_setting structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * TLS transport.
         */
        pjsip_tls_setting * getTlsSetting() {
            return &tlsSetting_;
        }

        /**
         * Get the local port for TLS listener.
         * @return pj_uint16 The port used for that account
         */
        pj_uint16_t getTlsListenerPort() const {
            return tlsListenerPort_;
        }

        pj_str_t getStunServerName() const override {
            return stunServerName_;
        }

        static const std::vector<std::string>& getSupportedTlsCiphers();
        static const std::vector<std::string>& getSupportedTlsProtocols();

        /**
         * @return pj_uint8_t structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * an alternate UDP transport.
         */
        pj_uint16_t getStunPort() const override {
            return stunPort_;
        }

        /**
         * @return bool Tells if current transport for that
         * account is set to OTHER.
         */
        bool isStunEnabled() const override {
            return stunEnabled_;
        }

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
        std::string getToUri(const std::string& username) const override;

        /**
         * In the current version, "srv" uri is obtained in the preformatted
         * way: hostname:port. This method adds the correct scheme and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         *
         * @return pj_str_t "server" uri based on @param hostPort
         * @param hostPort A string formatted as : "hostname:port"
         */
        std::string getServerUri() const;

        /**
         * Get the contact header for
         * @return pj_str_t The contact header based on account information
         */
        pj_str_t getContactHeader(pjsip_transport* = nullptr) override;


        std::string getServiceRoute() const {
            return serviceRoute_;
        }

        bool hasServiceRoute() const { return not serviceRoute_.empty(); }

        virtual bool isTlsEnabled() const override {
            return tlsEnable_;
        }

        virtual sip_utils::KeyExchangeProtocol getSrtpKeyExchange() const override {
            if (tlsEnable_ && srtpKeyExchange_ == sip_utils::KeyExchangeProtocol::NONE)
                return sip_utils::KeyExchangeProtocol::SDES;
            return srtpKeyExchange_;
        }

        virtual bool getSrtpFallback() const override {
            return srtpFallback_;
        }

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

        /**
         * Timer used to periodically send re-register request based
         * on the "Expire" sip header (or the "expire" Contact parameter)
         */
        static void keepAliveRegistrationCb(pj_timer_heap_t *th, pj_timer_entry *te);

        bool isKeepAliveEnabled() const {
            return keepAliveEnabled_;
        }

        void setTransport(const std::shared_ptr<SipTransport>& = nullptr);

        virtual inline std::shared_ptr<SipTransport> getTransport() {
            return transport_;
        }

        inline pjsip_transport_type_e getTransportType() const {
            return transportType_;
        }

        /**
         * Shortcut for SipTransport::getTransportSelector(account.getTransport()).
         */
        pjsip_tpselector getTransportSelector();

        /* Returns true if the username and/or hostname match this account */
        MatchRank matches(const std::string &username, const std::string &hostname) const override;

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
        std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl,
                                              const std::map<std::string, std::string>& volatileCallDetails = {}) override;

        /**
         * Create outgoing SIPCall.
         * @param[in] toUrl The address to call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
#ifndef _MSC_VER
        template <class T=SIPCall>
        std::shared_ptr<enable_if_base_of<T, SIPCall> >
        newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails = {});
#else
        template <class T>
        std::shared_ptr<T>
        newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails = {});
#endif

        /**
         * Create incoming SIPCall.
         * @param[in] from The origin uri of the call
         * @param details use to set some specific details
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        std::shared_ptr<SIPCall>
        newIncomingCall(const std::string& from, const std::map<std::string, std::string>& details = {}) override;

        void onRegister(pjsip_regc_cbparam *param);

        virtual void sendTextMessage(const std::string& to,
                                     const std::map<std::string, std::string>& payloads,
                                     uint64_t id) override;

        void connectivityChanged() override;

        std::string getUserUri() const override;

        IpAddr createIpAdress(const pjsip_transport_type_e &type, const pj_uint16_t port, const std::string& localInterface);

    private:
        void doRegister1_();
        void doRegister2_();

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

        void usePublishedAddressPortInVIA();
        void useUPnPAddressPortInVIA();
        bool fullMatch(const std::string &username, const std::string &hostname) const;
        bool userMatch(const std::string &username) const;
        bool hostnameMatch(const std::string &hostname) const;
        bool proxyMatch(const std::string &hostname) const;

        bool isSrtpEnabled() const {
            return srtpKeyExchange_ != sip_utils::KeyExchangeProtocol::NONE;
        }

        /**
         * Callback called by the transport layer when the registration
         * transport state changes.
         */
        virtual void onTransportStateChanged(pjsip_transport_state state, const pjsip_transport_state_info *info);

        struct {
            pj_bool_t    active;    /**< Flag of reregister status. */
            pj_timer_entry   timer;     /**< Timer for reregistration.  */
            void        *reg_tp;    /**< Transport for registration.    */
            unsigned     attempt_cnt; /**< Attempt counter.     */
        } auto_rereg_;           /**< Reregister/reconnect data. */

        std::uniform_int_distribution<int> delay10ZeroDist_ {-10000, 10000};
        std::uniform_int_distribution<unsigned int> delay10PosDist_ {0, 10000};

        void scheduleReregistration();
        void autoReregTimerCb();

        /**
         * Map of credential for this account
         */
        struct Credentials {
            std::string realm {};
            std::string username {};
            std::string password {};
            std::string password_h {};
            Credentials(const std::string& r, const std::string& u, const std::string& p)
             : realm(r), username(u), password(p) {}
            void computePasswordHash();
        };
        std::vector<Credentials> credentials_;

        std::shared_ptr<SipTransport> transport_ {};

        std::shared_ptr<TlsListener> tlsListener_ {};

        /**
         * Transport type used for this sip account. Currently supported types:
         *    PJSIP_TRANSPORT_UNSPECIFIED
         *    PJSIP_TRANSPORT_UDP
         *    PJSIP_TRANSPORT_TLS
         */
        pjsip_transport_type_e transportType_ {PJSIP_TRANSPORT_UNSPECIFIED};

        /**
         * Maps a string description of the SSL method
         * to the corresponding enum value in pjsip_ssl_method.
         * @param method The string representation
         * @return pjsip_ssl_method The corresponding value in the enum
         */
        static pj_uint32_t tlsProtocolFromString(const std::string& method);

        /**
         * Initializes tls settings from configuration file.
         */
        void initTlsConfiguration();

        /**
         * PJSIP aborts if the string length of our cipher list is too
         * great, so this function forces our cipher list to fit this constraint.
         */
        void trimCiphers();

        /**
         * Initializes STUN config from the config file
         */
        void initStunConfiguration();

        /**
         * If username is not provided, as it happens for Direct ip calls,
         * fetch the Real Name field of the user that is currently
         * running this program.
         * @return std::string The login name under which the software is running.
         */
        static std::string getLoginName();

        /**
         * Maps require port via UPnP
         */
        bool mapPortUPnP();

        /**
         * Resolved IP of hostname_ (for registration)
         */
        IpAddr hostIp_;

        /**
         * The pjsip client registration information
         */
        pjsip_regc *regc_;

        /**
         * To check if the account is registered
         */
        bool bRegister_;

        /**
         * Network settings
         */
        int registrationExpire_;

        /**
         * Optional list of SIP service this
         */
        std::string serviceRoute_;

        /**
         * Credential information stored for further registration.
         * Points to credentials_ members.
         */
        std::vector<pjsip_cred_info> cred_;

        /**
         * The TLS settings, used only if tls is chosen as a sip transport.
         */
        pjsip_tls_setting tlsSetting_;

        /**
         * Allocate a vector to be used by pjsip to store the supported ciphers on this system.
         */
        CipherArray ciphers_;

        /**
         * The STUN server name (hostname)
         */
        pj_str_t stunServerName_ {nullptr, 0};

        /**
         * The STUN server port
         */
        pj_uint16_t stunPort_ {PJ_STUN_PORT};

        /**
         * Local port to whih this account is bound
         */
        pj_uint16_t localPort_ {sip_utils::DEFAULT_SIP_PORT};

        /**
         * Potential ip addresss on which this account is bound
         */
        std::string sipBindAddress_ {"192.168.49.230"};

        /**
         * The TLS listener port
         */
        pj_uint16_t tlsListenerPort_ {sip_utils::DEFAULT_SIP_TLS_PORT};

        /**
         * Send Request Callback
         */
        static void onComplete(void *token, pjsip_event *event);

        bool tlsEnable_ {false};
        std::string tlsMethod_;
        std::string tlsCiphers_;
        std::string tlsServerName_;
        bool tlsVerifyServer_;
        bool tlsVerifyClient_;
        bool tlsRequireClientCertificate_;
        std::string tlsNegotiationTimeoutSec_;

        /**
         * Specifies the type of key exchange used for SRTP, if any.
         * This only determine if the media channel is secured.
         */
        sip_utils::KeyExchangeProtocol srtpKeyExchange_ {sip_utils::KeyExchangeProtocol::NONE};

        /**
         * Determine if the softphone should fallback on non secured media channel if SRTP negotiation fails.
         * Make sure other SIP endpoints share the same behavior since it could result in encrypted data to be
         * played through the audio device.
         */
        bool srtpFallback_ {};

        /**
         * Details about the registration state.
         * This is a protocol Code:Description pair.
         */
        std::pair<int, std::string> registrationStateDetailed_;

        /**
         * Determine if the keep alive timer will be activated or not
         */
        bool keepAliveEnabled_;

        /**
         * Timer used to regularrly send re-register request based
         * on the "Expire" sip header (or the "expire" Contact parameter)
         */
        pj_timer_entry keepAliveTimer_;
        std::uniform_int_distribution<decltype(pj_timer_entry::id)> timerIdDist_ {};

        /**
         * Once enabled, this variable tells if the keepalive timer is activated
         * for this accout
         */
        bool keepAliveTimerActive_;

        /**
         * Optional: "received" parameter from VIA header
         */
        std::string receivedParameter_;

        /**
         * Optional: "rport" parameter from VIA header
         */
        int rPort_;

        /**
         * Optional: via_addr construct from received parameters
         */
        pjsip_host_port via_addr_;

        /**
         * Temporary storage for getUPnPIpAddress().toString()
         * Used only by useUPnPAddressPortInVIA().
         */
        std::string upnpIpAddr_;

        char contactBuffer_[PJSIP_MAX_URL_SIZE];
        pj_str_t contact_;
        int contactRewriteMethod_;
        bool allowViaRewrite_;
        /* Undocumented feature in pjsip, this can == 2 */
        int allowContactRewrite_;
        bool contactOverwritten_;
        pjsip_transport *via_tp_;

        /**
         * Presence data structure
         */
        SIPPresence * presence_;

        /**
         * SIP port actually used,
         * this holds the actual port used for SIP, which may not be the port
         * selected in the configuration in the case that UPnP is used and the
         * configured port is already used by another client
         */
        pj_uint16_t publishedPortUsed_ {sip_utils::DEFAULT_SIP_PORT};
};

} // namespace jami

#endif
