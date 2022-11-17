/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/sipaccountbase.h"
#include "sip/siptransport.h"
#include "noncopyable.h"
#include "ring_types.h" // enable_if_base_of
#include "sipaccount_config.h"

#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_types.h>
#include <pjsip-ua/sip_regc.h>

#include <vector>
#include <map>

namespace jami {

typedef std::vector<pj_ssl_cipher> CipherArray;

class SIPPresence;
class SIPCall;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */
class SIPAccount : public SIPAccountBase
{
public:
    constexpr static auto ACCOUNT_TYPE = ACCOUNT_TYPE_SIP;

    std::shared_ptr<SIPAccount> shared()
    {
        return std::static_pointer_cast<SIPAccount>(shared_from_this());
    }
    std::shared_ptr<SIPAccount const> shared() const
    {
        return std::static_pointer_cast<SIPAccount const>(shared_from_this());
    }
    std::weak_ptr<SIPAccount> weak()
    {
        return std::static_pointer_cast<SIPAccount>(shared_from_this());
    }
    std::weak_ptr<SIPAccount const> weak() const
    {
        return std::static_pointer_cast<SIPAccount const>(shared_from_this());
    }

    /**
     * Constructor
     * @param accountID The account identifier
     */
    SIPAccount(const std::string& accountID, bool presenceEnabled);

    ~SIPAccount() noexcept;

    const SipAccountConfig& config() const
    {
        return *static_cast<const SipAccountConfig*>(&Account::config());
    }

    std::unique_ptr<AccountConfig> buildConfig() const override
    {
        return std::make_unique<SipAccountConfig>(getAccountID());
    }
    inline void editConfig(std::function<void(SipAccountConfig& conf)>&& edit)
    {
        Account::editConfig(
            [&](AccountConfig& conf) { edit(*static_cast<SipAccountConfig*>(&conf)); });
    }

    std::string_view getAccountType() const override { return ACCOUNT_TYPE; }

    pjsip_host_port getHostPortFromSTUN(pj_pool_t* pool);

    void updateDialogViaSentBy(pjsip_dialog* dlg);

    void resetAutoRegistration();

    /**
     * Update NAT address, Via and Contact header from the REGISTER response
     * @param param pjsip reg cbparam
     * @param pool
     * @return update status
     */
    bool checkNATAddress(pjsip_regc_cbparam* param, pj_pool_t* pool);

    /**
     * Returns true if this is the IP2IP account
     */
    bool isIP2IP() const override;

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
     * Build and send SIP registration request
     */
    void sendRegister();

    /**
     * Build and send SIP unregistration request
     * @param destroy_transport If true, attempt to destroy the transport.
     */
    void sendUnregister();

    const pjsip_cred_info* getCredInfo() const { return cred_.data(); }

    /**
     * Get the number of credentials defined for
     * this account.
     * @param none
     * @return int The number of credentials set for this account.
     */
    unsigned getCredentialCount() const { return config().credentials.size(); }

    bool hasCredentials() const { return not config().credentials.empty(); }

    std::vector<std::map<std::string, std::string>> getCredentials() const
    {
        return config().getCredentials();
    }

    virtual void setRegistrationState(RegistrationState state,
                                      int code = 0,
                                      const std::string& detail_str = {}) override;

    /**
     * A client sendings a REGISTER request MAY suggest an expiration
     * interval that indicates how long the client would like the
     * registration to be valid.
     *
     * @return the expiration value.
     */
    unsigned getRegistrationExpire() const
    {
        unsigned re = config().registrationExpire;
        return re ? re : PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;
    }

    /**
     * Registration flag
     */
    bool isRegistered() const { return bRegister_; }

    /**
     * Get the registration structure that is used
     * for PJSIP in the registration process.
     * Settings are loaded from configuration file.
     * @return pjsip_regc* A pointer to the registration structure
     */
    pjsip_regc* getRegistrationInfo() { return regc_; }

    /**
     * Set the registration structure that is used
     * for PJSIP in the registration process;
     * @pram A pointer to the new registration structure
     * @return void
     */
    void setRegistrationInfo(pjsip_regc* regc)
    {
        if (regc_)
            destroyRegistrationInfo();
        regc_ = regc;
    }

    void destroyRegistrationInfo();

    /**
     * Get the port on which the transport/listener should use, or is
     * actually using.
     * @return pj_uint16 The port used for that account
     */
    uint16_t getLocalPort() const { return config().localPort; }

    void setLocalPort(uint16_t port)
    {
        editConfig([&](SipAccountConfig& config) { config.localPort = port; });
    }

    /**
     * @return pjsip_tls_setting structure, filled from the configuration
     * file, that can be used directly by PJSIP to initialize
     * TLS transport.
     */
    pjsip_tls_setting* getTlsSetting() { return &tlsSetting_; }

    /**
     * Get the local port for TLS listener.
     * @return pj_uint16 The port used for that account
     */
    pj_uint16_t getTlsListenerPort() const { return config().tlsListenerPort; }

    pj_str_t getStunServerName() const { return stunServerName_; }

    static const std::vector<std::string>& getSupportedTlsCiphers();
    static const std::vector<std::string>& getSupportedTlsProtocols();

    /**
     * @return pj_uint8_t structure, filled from the configuration
     * file, that can be used directly by PJSIP to initialize
     * an alternate UDP transport.
     */
    pj_uint16_t getStunPort() const override { return stunPort_; }

    /**
     * @return bool Tells if current transport for that
     * account is set to OTHER.
     */
    bool isStunEnabled() const override { return config().stunEnabled; }

    /**
     * @return pj_str_t , filled from the configuration
     * file, that can be used directly by PJSIP to initialize
     * an alternate UDP transport.
     */
    std::string getStunServer() const { return config().stunServer; }

    /**
     * @return pj_str_t "From" uri based on account information.
     * From RFC3261: "The To header field first and foremost specifies the desired
     * logical" recipient of the request, or the address-of-record of the
     * user or resource that is the target of this request. [...]  As such, it is
     * very important that the From URI not contain IP addresses or the FQDN
     * of the host on which the UA is running, since these are not logical
     * names."
     */
    std::string getFromUri() const override;

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
     * Get the contact address
     * @return The current contact address
     */
    IpAddr getContactAddress() const;
    /**
     * Get the contact header
     * @return The current contact header
     */
    std::string getContactHeader() const;

    std::string getServiceRoute() const { return config().serviceRoute; }

    bool hasServiceRoute() const { return not config().serviceRoute.empty(); }

    virtual bool isTlsEnabled() const override { return config().tlsEnable; }

    virtual bool getSrtpFallback() const override { return config().srtpFallback; }

    void setReceivedParameter(const std::string& received)
    {
        receivedParameter_ = received;
        via_addr_.host = sip_utils::CONST_PJ_STR(receivedParameter_);
    }

    const std::string& getReceivedParameter() const { return receivedParameter_; }

    pjsip_host_port* getViaAddr() { return &via_addr_; }

    int getRPort() const
    {
        if (rPort_ == -1)
            return config().localPort;
        else
            return rPort_;
    }

    void setRPort(int rPort)
    {
        rPort_ = rPort;
        via_addr_.port = rPort;
    }

    bool isRegistrationRefreshEnabled() const { return config().registrationRefreshEnabled; }

    void setTransport(const std::shared_ptr<SipTransport>& = nullptr);

    virtual inline std::shared_ptr<SipTransport> getTransport() { return transport_; }

    inline pjsip_transport_type_e getTransportType() const { return transportType_; }

    /**
     * Shortcut for SipTransport::getTransportSelector(account.getTransport()).
     */
    pjsip_tpselector getTransportSelector();

    /* Returns true if the username and/or hostname match this account */
    MatchRank matches(std::string_view username, std::string_view hostname) const override;

    /**
     * Presence management
     */
    SIPPresence* getPresence() const;

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
     * Create outgoing SIPCall.
     * @param[in] toUrl the address to call
     * @param[in] mediaList list of medias
     * @return a shared pointer on the created call.
     */
    std::shared_ptr<Call> newOutgoingCall(std::string_view toUrl,
                                          const std::vector<libjami::MediaMap>& mediaList) override;

    /**
     * Create incoming SIPCall.
     * @param[in] from The origin of the call
     * @param mediaList A list of media
     * @param sipTr: SIP Transport
     * @return A shared pointer on the created call.
     */
    std::shared_ptr<SIPCall> newIncomingCall(
        const std::string& from,
        const std::vector<libjami::MediaMap>& mediaList,
        const std::shared_ptr<SipTransport>& sipTr = {}) override;

    void onRegister(pjsip_regc_cbparam* param);

    virtual void sendMessage(const std::string& to,
                             const std::map<std::string, std::string>& payloads,
                             uint64_t id,
                             bool retryOnTimeout = true,
                             bool onlyConnected = false,
                             const std::string& deviceId = {}) override;

    void connectivityChanged() override;

    std::string getUserUri() const override;

    /**
     * Create the Ip address that the transport uses
     * @return IpAddr created
     */
    IpAddr createBindingAddress();

    void setActiveCodecs(const std::vector<unsigned>& list) override;
    bool isSrtpEnabled() const override
    {
        return config().srtpKeyExchange != KeyExchangeProtocol::NONE;
    }

    bool setPushNotificationToken(const std::string& pushDeviceToken = "") override;

    /**
     * To be called by clients with relevant data when a push notification is received.
     */
    void pushNotificationReceived(const std::string& from,
                                  const std::map<std::string, std::string>& data);

private:
    void doRegister1_();
    void doRegister2_();

    // Initialize the address to be used in contact header. Might
    // be updated (as the contact header)after the registration.
    bool initContactAddress();
    void updateContactHeader();

    void setCredentials(const std::vector<SipAccountConfig::Credentials>& creds);

    void setUpTransmissionData(pjsip_tx_data* tdata, long transportKeyType);

    NON_COPYABLE(SIPAccount);

    std::shared_ptr<Call> newRegisteredAccountCall(const std::string& id, const std::string& toUrl);

    /**
     * Start a SIP Call
     * @param call  The current call
     * @return true if all is correct
     */
    bool SIPStartCall(std::shared_ptr<SIPCall>& call);

    void usePublishedAddressPortInVIA();
    void useUPnPAddressPortInVIA();
    bool fullMatch(std::string_view username, std::string_view hostname) const;
    bool userMatch(std::string_view username) const;
    bool hostnameMatch(std::string_view hostname) const;
    bool proxyMatch(std::string_view hostname) const;

    /**
     * Callback called by the transport layer when the registration
     * transport state changes.
     */
    virtual void onTransportStateChanged(pjsip_transport_state state,
                                         const pjsip_transport_state_info* info);

    struct
    {
        pj_bool_t active {false}; /**< Flag of reregister status. */
        pj_timer_entry timer {};  /**< Timer for reregistration.  */
        unsigned attempt_cnt {0}; /**< Attempt counter.     */
    } auto_rereg_ {};             /**< Reregister/reconnect data. */

    std::uniform_int_distribution<int> delay10ZeroDist_ {-10000, 10000};
    std::uniform_int_distribution<unsigned int> delay10PosDist_ {0, 10000};

    void scheduleReregistration();
    void autoReregTimerCb();

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
     * Print contact header in certain format
     */
    static std::string printContactHeader(const std::string& username,
                                          const std::string& displayName,
                                          const std::string& address,
                                          pj_uint16_t port,
                                          bool secure,
                                          const std::string& deviceKey = {});

    /**
     * Resolved IP of hostname_ (for registration)
     */
    IpAddr hostIp_;

    /**
     * The pjsip client registration information
     */
    pjsip_regc* regc_ {nullptr};

    /**
     * To check if the account is registered
     */
    bool bRegister_;

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
     * Send Request Callback
     */
    static void onComplete(void* token, pjsip_event* event);

    /**
     * Details about the registration state.
     * This is a protocol Code:Description pair.
     */
    std::pair<int, std::string> registrationStateDetailed_;

    /**
     * Optional: "received" parameter from VIA header
     */
    std::string receivedParameter_;

    /**
     * Optional: "rport" parameter from VIA header
     */
    int rPort_ {-1};

    /**
     * Optional: via_addr construct from received parameters
     */
    pjsip_host_port via_addr_;

    // This is used at runtime . Mainly by SIPAccount::usePublishedAddressPortInVIA()
    std::string publishedIpStr_ {};

    /**
     * Temporary storage for getUPnPIpAddress().toString()
     * Used only by useUPnPAddressPortInVIA().
     */
    std::string upnpIpAddr_;

    mutable std::mutex contactMutex_;
    // Contact header
    std::string contactHeader_;
    // Contact address (the address part of a SIP URI)
    IpAddr contactAddress_ {};
    pjsip_transport* via_tp_ {nullptr};

    /**
     * Presence data structure
     */
    SIPPresence* presence_;

    /**
     * SIP port actually used,
     * this holds the actual port used for SIP, which may not be the port
     * selected in the configuration in the case that UPnP is used and the
     * configured port is already used by another client
     */
    pj_uint16_t publishedPortUsed_ {sip_utils::DEFAULT_SIP_PORT};
};

} // namespace jami
