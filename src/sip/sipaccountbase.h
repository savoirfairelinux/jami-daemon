/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
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
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "account.h"

#include "sip_utils.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "im/message_engine.h"

#include <pjsip/sip_types.h>

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#ifdef _WIN32
typedef uint16_t in_port_t;
#else
#include <netinet/in.h> // For in_port_t support
#endif

struct pjsip_dialog;
struct pjsip_inv_session;
struct pjmedia_sdp_session;

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
    const char *const TURN_ENABLED_KEY = "turnEnabled";
    const char *const TURN_SERVER_KEY = "turnServer";
    const char *const TURN_SERVER_UNAME_KEY = "turnServerUserName";
    const char *const TURN_SERVER_PWD_KEY = "turnServerPassword";
    const char *const TURN_SERVER_REALM_KEY = "turnServerRealm";
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
     * @param details use to set some specific details
     * @return std::shared_ptr<T> A shared pointer on the created call.
     *      The type of this instance is given in template argument.
     *      This type can be any base class of SIPCall class (included).
     */
    virtual std::shared_ptr<SIPCall>
    newIncomingCall(const std::string& from, const std::map<std::string, std::string>& details = {}) = 0;

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
     * SIP signalization. It is independent of the media encryption (as provided by SRTP).
     */
    virtual bool isTlsEnabled() const {
        return false;
    }

    /**
     * Create UAC attached dialog and invite session
     * @return true if success. false if failure and dlg and inv pointers
     *         should not be considered as valid.
     */
    bool CreateClientDialogAndInvite(const pj_str_t* from,
                                     const pj_str_t* contact,
                                     const pj_str_t* to,
                                     const pj_str_t* target,
                                     const pjmedia_sdp_session* local_sdp,
                                     pjsip_dialog** dlg,
                                     pjsip_inv_session** inv);

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

    void setPublishedAddress(const IpAddr& ip_addr);

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

    /**
     * @return pj_str_t , filled from the configuration
     * file, that can be used directly by PJSIP to initialize
     * an alternate UDP transport.
     */
    std::string getStunServer() const {
        return stunServer_;
    }

    void setStunServer(const std::string &srv) {
        stunServer_ = srv;
    }

    const IceTransportOptions getIceOptions() const noexcept override;

    virtual void sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads, uint64_t id) = 0;

    virtual uint64_t sendTextMessage(const std::string& to,
                                     const std::map<std::string, std::string>& payloads) override {
        return messageEngine_.sendMessage(to, payloads);
    }

    virtual im::MessageStatus getMessageStatus(uint64_t id) const override {
        return messageEngine_.getStatus(id);
    }

    virtual bool cancelMessage(uint64_t id) override {
        return messageEngine_.cancel(id);
    }

    void onTextMessage(const std::string& from, const std::map<std::string, std::string>& payloads);

    /* Returns true if the username and/or hostname match this account */
    virtual MatchRank matches(const std::string &username, const std::string &hostname) const = 0;

    void connectivityChanged() override {};

    virtual std::string getUserUri() const = 0;

    std::vector<DRing::Message> getLastMessages(const uint64_t& base_timestamp) override {
        std::lock_guard<std::mutex> lck(mutexLastMessages_);
        auto it = lastMessages_.begin();
        size_t num = lastMessages_.size();
        while (it != lastMessages_.end() and it->received <= base_timestamp) {
            num--;
            ++it;
        }
        if (num == 0)
            return {};
        return {it, lastMessages_.end()};
    }

public: // overloaded methods
    virtual void flush() override;

protected:
    virtual void serialize(YAML::Emitter &out) override;
    virtual void serializeTls(YAML::Emitter &out);
    virtual void unserialize(const YAML::Node &node) override;

    virtual void setAccountDetails(const std::map<std::string, std::string> &details) override;

    virtual std::map<std::string, std::string> getAccountDetails() const override;

    /**
     * Retrieve volatile details such as recent registration errors
     * @return std::map< std::string, std::string > The account volatile details
     */
    virtual std::map<std::string, std::string> getVolatileAccountDetails() const override;

    virtual void setRegistrationState(RegistrationState state, unsigned code=0, const std::string& detail_str={}) override;

    im::MessageEngine messageEngine_;

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

    /**
     * Determine if STUN public address resolution is required to register this account. In this case a
     * STUN server hostname must be specified.
     */
    bool stunEnabled_ {false};

    /**
     * The STUN server hostname (optional), used to provide the public IP address in case the softphone
     * stay behind a NAT.
     */
    std::string stunServer_ {};

    /**
     * Determine if TURN public address resolution is required to register this account. In this case a
     * TURN server hostname must be specified.
     */
    bool turnEnabled_ {false};

    /**
     * The TURN server hostname (optional), used to provide the public IP address in case the softphone
     * stay behind a NAT.
     */
    std::string turnServer_;
    std::string turnServerUserName_;
    std::string turnServerPwd_;
    std::string turnServerRealm_;

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
        UsedPort(UsedPort&& o) noexcept : port_(o.port_) {
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
        UsedPort& operator=(UsedPort&& o) noexcept {
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

    /**
     * The deamon can be launched without any client (or with a non ready client)
     * Like call and file transfer, a client should be able to retrieve current messages.
     * To avoid to explode the size in memory, this container should be limited.
     * We don't want to see monsters in memory.
     */
    std::mutex mutexLastMessages_;
    static constexpr size_t MAX_WAITING_MESSAGES_SIZE = 1000;
    std::deque<DRing::Message> lastMessages_;

private:
    NON_COPYABLE(SIPAccountBase);

};

} // namespace ring
