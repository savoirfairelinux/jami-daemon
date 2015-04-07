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

#ifndef SIPACCOUNTBASE_H
#define SIPACCOUNTBASE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "account.h"

#include "sip_utils.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "dring/security_const.h"

#include <pjsip/sip_types.h>
#include <opendht/value.h>

#include <array>
#include <vector>
#include <map>
#include <memory>

namespace ring {

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

typedef std::vector<pj_ssl_cipher> CipherArray;

class SIPVoIPLink;
class SIPCall;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */

enum class MatchRank {NONE, PARTIAL, FULL};

class SIPAccountBase : public Account {
public:
    constexpr static const char * const OVERRTP_STR = "overrtp";
    constexpr static const char * const SIPINFO_STR = "sipinfo";
    constexpr static unsigned MAX_PORT {65536};
    constexpr static unsigned HALF_MAX_PORT {MAX_PORT / 2};

    /**
     * Constructor
     * @param accountID The account identifier
     */
    SIPAccountBase(const std::string& accountID);

    virtual ~SIPAccountBase();

    /**
     * Create incoming SIPCall.
     * @param[in] id The ID of the call
     * @return std::shared_ptr<T> A shared pointer on the created call.
     *      The type of this instance is given in template argument.
     *      This type can be any base class of SIPCall class (included).
     */
    virtual std::shared_ptr<SIPCall>
    newIncomingCall(const std::string& from) = 0;

    virtual bool isStunEnabled() const {
        return false;
    }

    virtual pj_str_t getStunServerName() const { return pj_str_t {nullptr, 0}; };

    virtual pj_uint16_t getStunPort() const { return 0; };

    virtual std::string getDtmfType() const {
        return dtmfType_;
    }

    /**
     * Determine if TLS is enabled for this account. TLS provides a secured channel for
     * SIP signalization. It is independant than the media encription provided by SRTP or ZRTP.
     */
    virtual bool isTlsEnabled() const {
        return false;
    }

    /**
     * Get the local interface name on which this account is bound.
     */
    const std::string& getLocalInterface() const {
        return interface_;
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

    /**
     * Get a flag which determine the usage in sip headers of either the local
     * IP address and port (_localAddress and localPort_) or to an address set
     * manually (_publishedAddress and publishedPort_).
     */
    bool getPublishedSameasLocal() const {
        return publishedSameasLocal_;
    }

    virtual sip_utils::KeyExchangeProtocol getSrtpKeyExchange() const = 0;

    virtual bool getSrtpFallback() const = 0;

    /**
     * Get the contact header for
     * @return pj_str_t The contact header based on account information
     */
    virtual pj_str_t getContactHeader(pjsip_transport* = nullptr) = 0;

    virtual std::string getToUri(const std::string& username) const = 0;

    /**
     * Socket port generators for media
     * Note: given ports are application wide, a port cannot be given again
     * by any account instances until it's released by the static method
     * releasePort().
     */
    uint16_t generateAudioPort() const;
#ifdef RING_VIDEO
    uint16_t generateVideoPort() const;
#endif
    static void releasePort(uint16_t port) noexcept;

    bool setCertificateStatus(const std::string& cert_id, const DRing::Certificate::Status status) {
        certificateStatus_[cert_id] = status;
        return true;
    }
    std::vector<std::string> getCertificatesByStatus(DRing::Certificate::Status status) {
        std::vector<std::string> ret;
        for (const auto& i : certificateStatus_)
            if (i.second == status)
                ret.emplace_back(i.first);
        return ret;
    }

protected:
    virtual void serialize(YAML::Emitter &out);
    virtual void serializeTls(YAML::Emitter &out);
    virtual void unserialize(const YAML::Node &node);

    virtual void setAccountDetails(const std::map<std::string, std::string> &details);

    virtual std::map<std::string, std::string> getAccountDetails() const;

    /**
     * Retrieve volatile details such as recent registration errors
     * @return std::map< std::string, std::string > The account volatile details
     */
    virtual std::map<std::string, std::string> getVolatileAccountDetails() const;

    /**
     * Voice over IP Link contains a listener thread and calls
     */
    std::shared_ptr<SIPVoIPLink> link_;

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
     * Published port, used only if defined by the user
     */
    pj_uint16_t publishedPort_ {sip_utils::DEFAULT_SIP_PORT};

    std::string tlsCaListFile_;
    std::string tlsCertificateFile_;
    std::string tlsPrivateKeyFile_;
    std::string tlsPassword_;

    /**
     * DTMF type used for this account SIPINFO or RTP
     */
    std::string dtmfType_ {OVERRTP_STR};

    pj_status_t transportStatus_ {PJSIP_SC_TRYING};
    std::string transportError_ {};

    /*
     * Port range for audio RTP ports
     */
    std::pair<uint16_t, uint16_t> audioPortRange_ {16384, 32766};

    /**
     * Port range for video RTP ports
     */
    std::pair<uint16_t, uint16_t> videoPortRange_ {49152, (MAX_PORT) - 2};

    struct UsedPort {
        UsedPort() {};
        UsedPort(UsedPort&& o) : port_(o.port_) {
            o.port_ = 0;
        }
        UsedPort(in_port_t p) : port_(p) {
            if (port_)
                acquirePort(port_);
        };
        ~UsedPort() {
            if (port_)
                releasePort(port_);
        };
        UsedPort& operator=(UsedPort&& o) {
            if (port_)
                releasePort(port_);
            port_ = o.port_;
            o.port_ = 0;
            return *this;
        }
        UsedPort& operator=(in_port_t p) {
            if (port_)
                releasePort(port_);
            port_ = p;
            if (port_)
                acquirePort(port_);
            return *this;
        }
        explicit operator in_port_t() const { return port_; }
    private:
        in_port_t port_ {0};
        NON_COPYABLE(UsedPort);
    };

    static std::array<bool, HALF_MAX_PORT>& getPortsReservation() noexcept;
    static uint16_t acquirePort(uint16_t port);
    uint16_t getRandomEvenPort(const std::pair<uint16_t, uint16_t>& range) const;
    uint16_t acquireRandomEvenPort(const std::pair<uint16_t, uint16_t>& range) const;

    std::map<std::string, DRing::Certificate::Status> certificateStatus_;

private:
    NON_COPYABLE(SIPAccountBase);

};

} // namespace ring

#endif
