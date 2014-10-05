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

#ifndef SIPACCOUNTBASE_H
#define SIPACCOUNTBASE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "siptransport.h"
#include "account.h"
#include "noncopyable.h"
#include "ip_utils.h"
//#include "sfl_types.h" // enable_if_base_of

#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_types.h>

#include <vector>
#include <map>
#include <sstream>

typedef std::vector<pj_ssl_cipher> CipherArray;

namespace Conf {
    // SIP specific configuration keys
    const char *const INTERFACE_KEY = "interface";
    const char *const PORT_KEY = "port";
    const char *const PUBLISH_ADDR_KEY = "publishAddr";
    const char *const PUBLISH_PORT_KEY = "publishPort";
    const char *const SAME_AS_LOCAL_KEY = "sameasLocal";
    const char *const DTMF_TYPE_KEY = "dtmfType";
    const char *const SERVICE_ROUTE_KEY = "serviceRoute";
    const char *const PRESENCE_ENABLED_KEY = "presenceEnabled";
    const char *const PRESENCE_PUBLISH_SUPPORTED_KEY = "presencePublishSupported";
    const char *const PRESENCE_SUBSCRIBE_SUPPORTED_KEY = "presenceSubscribeSupported";
    const char *const PRESENCE_STATUS_KEY = "presenceStatus";
    const char *const PRESENCE_NOTE_KEY = "presenceNote";

    // TODO: write an object to store credential which implement serializable
    const char *const SRTP_KEY = "srtp";
    const char *const SRTP_ENABLE_KEY = "enable";
    const char *const KEY_EXCHANGE_KEY = "keyExchange";
    const char *const RTP_FALLBACK_KEY = "rtpFallback";

    // TODO: wirte an object to store zrtp params wich implement serializable
    const char *const ZRTP_KEY = "zrtp";
    const char *const DISPLAY_SAS_KEY = "displaySas";
    const char *const DISPLAY_SAS_ONCE_KEY = "displaySasOnce";
    const char *const HELLO_HASH_ENABLED_KEY = "helloHashEnabled";
    const char *const NOT_SUPP_WARNING_KEY = "notSuppWarning";

    // TODO: write an object to store tls params which implement serializable
    const char *const TLS_KEY = "tls";
    const char *const TLS_PORT_KEY = "tlsPort";
    const char *const CERTIFICATE_KEY = "certificate";
    const char *const CALIST_KEY = "calist";
    const char *const CIPHERS_KEY = "ciphers";
    const char *const TLS_ENABLE_KEY = "enable";
    const char *const METHOD_KEY = "method";
    const char *const TIMEOUT_KEY = "timeout";
    const char *const TLS_PASSWORD_KEY = "password";
    const char *const PRIVATE_KEY_KEY = "privateKey";
    const char *const REQUIRE_CERTIF_KEY = "requireCertif";
    const char *const SERVER_KEY = "server";
    const char *const VERIFY_CLIENT_KEY = "verifyClient";
    const char *const VERIFY_SERVER_KEY = "verifyServer";

    const char *const STUN_ENABLED_KEY = "stunEnabled";
    const char *const STUN_SERVER_KEY = "stunServer";
    const char *const CRED_KEY = "credential";
    const char *const AUDIO_PORT_MIN_KEY = "audioPortMin";
    const char *const AUDIO_PORT_MAX_KEY = "audioPortMax";
    const char *const VIDEO_PORT_MIN_KEY = "videoPortMin";
    const char *const VIDEO_PORT_MAX_KEY = "videoPortMax";
}

class SIPVoIPLink;
class SIPCall;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */
enum {MAX_PORT = 65536};
enum {HALF_MAX_PORT = MAX_PORT / 2};
enum class MatchRank {NONE, PARTIAL, FULL};

class SIPAccountBase : public Account {
public:
    constexpr static const char * const OVERRTP_STR = "overrtp";
    constexpr static const char * const SIPINFO_STR = "sipinfo";

    /**
     * Constructor
     * @param accountID The account identifier
     */
    SIPAccountBase(const std::string& accountID);

    virtual ~SIPAccountBase() {
        setTransport();
    }

    /**
     * Create incoming SIPCall.
     * @param[in] id The ID of the call
     * @return std::shared_ptr<T> A shared pointer on the created call.
     *      The type of this instance is given in template argument.
     *      This type can be any base class of SIPCall class (included).
     */
    virtual std::shared_ptr<SIPCall>
    newIncomingCall(const std::string& id) = 0;

    virtual bool isStunEnabled() const {
        return false;
    }

    virtual pj_str_t getStunServerName() const { return pj_str_t {nullptr, 0}; };

    virtual pj_uint16_t getStunPort() const { return 0; };

    virtual std::string getDtmfType() const {
        return dtmfType_;
    }

    virtual bool isTlsEnabled() const {
        return tlsEnable_;
    }

    virtual pjsip_tls_setting * getTlsSetting() {
        return nullptr;
    }

    /**
     * Get the local port for TLS listener.
     * @return pj_uint16 The port used for that account
     */
    pj_uint16_t getTlsListenerPort() const {
        return tlsListenerPort_;
    }

    /**
     * Get the public IP address set by the user for this account.
     * If this setting is not provided, the local bound adddress
     * will be used.
     * @return std::string The public IPv4 or IPv6 address formatted in standard notation.
     */
    std::string getPublishedAddress() const {
        return publishedIpAddress_;
    }

    IpAddr getPublishedIpAddress() const {
        return publishedIp_;
    }

    void setPublishedAddress(const IpAddr& ip_addr) {
        publishedIp_ = ip_addr;
        publishedIpAddress_ = ip_addr.toString();
    }

    /**
     * Get the local interface name on which this account is bound.
     */
    const std::string& getLocalInterface() const {
        return interface_;
    }

    /**
     * Get a flag which determine the usage in sip headers of either the local
     * IP address and port (_localAddress and localPort_) or to an address set
     * manually (_publishedAddress and publishedPort_).
     */
    bool getPublishedSameasLocal() const {
        return publishedSameasLocal_;
    }

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
     * Get the published port, which is the port to be advertised as the port
     * for the chosen SIP transport.
     * @return pj_uint16 The port used for that account
     */
    pj_uint16_t getPublishedPort() const {
        return (pj_uint16_t) publishedPort_;
    }

    /**
     * Set the published port, which is the port to be advertised as the port
     * for the chosen SIP transport.
     * @pram port The port used by this account.
     */
    void setPublishedPort(pj_uint16_t port) {
        publishedPort_ = port;
    }

    virtual bool getSrtpEnabled() const {
        return srtpEnabled_;
    }

    virtual std::string getSrtpKeyExchange() const {
        return srtpKeyExchange_;
    }

    bool getSrtpFallback() const {
        return srtpFallback_;
    }

    /**
     * Get the contact header for
     * @return pj_str_t The contact header based on account information
     */
    virtual pj_str_t getContactHeader() = 0;

    virtual std::string getToUri(const std::string& username) const = 0;

    virtual std::string getServerUri() const = 0;

    uint16_t generateAudioPort() const;
#ifdef SFL_VIDEO
    uint16_t generateVideoPort() const;
#endif
    static void releasePort(uint16_t port);

    virtual void setTransport(const std::shared_ptr<SipTransport>& = nullptr);

    inline const std::shared_ptr<SipTransport>& getTransport() {
        return transport_;
    }

    inline pjsip_transport_type_e getTransportType() const {
        return transportType_;
    }

    /**
     * Shortcut for SipTransport::getTransportSelector(account.getTransport()).
     */
    inline pjsip_tpselector getTransportSelector() {
        if (!transport_)
            return SipTransportBroker::getTransportSelector(nullptr);
        return SipTransportBroker::getTransportSelector(transport_->get());
    }

protected:
    virtual void serialize(YAML::Emitter &out);
    virtual void unserialize(const YAML::Node &node);

    virtual void setAccountDetails(const std::map<std::string, std::string> &details);

    virtual std::map<std::string, std::string> getAccountDetails() const;

    /**
     * Retrieve volatile details such as recent registration errors
     * @return std::map< std::string, std::string > The account volatile details
     */
    virtual std::map<std::string, std::string> getVolatileAccountDetails() const;

    /**
     * Callback called by the transport layer when the registration
     * transport state changes.
     */
    virtual void onTransportStateChanged(pjsip_transport_state state, const pjsip_transport_state_info *info);

    /**
     * Voice over IP Link contains a listener thread and calls
     */
    std::shared_ptr<SIPVoIPLink> link_;

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
     * interface name on which this account is bound
     */
    std::string interface_ {"default"};

    /**
     * Flag which determine if localIpAddress_ or publishedIpAddress_ is used in
     * sip headers
     */
    bool publishedSameasLocal_ {true};

    /**
     * Published IP address, used only if defined by the user in account
     * configuration
     */
    IpAddr publishedIp_ {};
    std::string publishedIpAddress_ {};

    /**
     * Local port to whih this account is bound
     */
    pj_uint16_t localPort_ {DEFAULT_SIP_PORT};

    /**
     * Published port, used only if defined by the user
     */
    pj_uint16_t publishedPort_ {DEFAULT_SIP_PORT};

    /**
     * The global TLS listener port which can be configured through the IP2IP_PROFILE
     */
    pj_uint16_t tlsListenerPort_ {DEFAULT_SIP_TLS_PORT};

    /**
     * DTMF type used for this account SIPINFO or RTP
     */
    std::string dtmfType_ {OVERRTP_STR};

    /**
     * Determine if TLS is enabled for this account. TLS provides a secured channel for
     * SIP signalization. It is independant than the media encription provided by SRTP or ZRTP.
     */
    bool tlsEnable_ {false};

    /**
     * Determine if SRTP is enabled for this account, SRTP and ZRTP are mutually exclusive
     * This only determine if the media channel is secured. One could only enable TLS
     * with no secured media channel.
     */
    bool srtpEnabled_ {false};

    /**
     * Specifies the type of key exchange usd for SRTP (sdes/zrtp)
     */
    std::string srtpKeyExchange_ {""};

    /**
     * Determine if the softphone should fallback on non secured media channel if SRTP negotiation fails.
     * Make sure other SIP endpoints share the same behavior since it could result in encrypted data to be
     * played through the audio device.
     */
    bool srtpFallback_ {};

    /*
     * Port range for audio RTP ports
     */
    std::pair<uint16_t, uint16_t> audioPortRange_ {16384, 32766};

    /**
     * Port range for video RTP ports
     */
    std::pair<uint16_t, uint16_t> videoPortRange_ {49152, (MAX_PORT) - 2};

    static bool portsInUse_[HALF_MAX_PORT];
    static uint16_t getRandomEvenNumber(const std::pair<uint16_t, uint16_t> &range);

    static void
    addRangeToDetails(std::map<std::string, std::string> &a, const char *minKey, const char *maxKey, const std::pair<uint16_t, uint16_t> &range)
    {
        std::ostringstream os;
        os << range.first;
        a[minKey] = os.str();
        os.str("");
        os << range.second;
        a[maxKey] = os.str();
    }

private:
    NON_COPYABLE(SIPAccountBase);

};

#endif
