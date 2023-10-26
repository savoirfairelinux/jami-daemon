/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "connectivity/sip_utils.h"
#include "noncopyable.h"
#include "im/message_engine.h"
#include "sipaccountbase_config.h"

#include <dhtnet/turn_cache.h>
#include <dhtnet/ip_utils.h>
#include <dhtnet/ice_options.h>

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

extern "C" {
#include <pjsip/sip_types.h>
#ifdef _WIN32
typedef uint16_t in_port_t;
#else
#include <netinet/in.h> // For in_port_t support
#endif

struct pjsip_dialog;
struct pjsip_inv_session;
struct pjmedia_sdp_session;
}

static constexpr const char MIME_TYPE_TEXT_PLAIN[] {"text/plain"};

namespace jami {

class SipTransport;
class Task;

typedef std::vector<pj_ssl_cipher> CipherArray;

class SIPVoIPLink;
class SIPCall;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */

enum class MatchRank { NONE, PARTIAL, FULL };

class SIPAccountBase : public Account
{
public:
    constexpr static unsigned MAX_PORT {65536};
    constexpr static unsigned HALF_MAX_PORT {MAX_PORT / 2};

    /**
     * Constructor
     * @param accountID The account identifier
     */
    SIPAccountBase(const std::string& accountID);

    virtual ~SIPAccountBase() noexcept;

    const SipAccountBaseConfig& config() const
    {
        return *static_cast<const SipAccountBaseConfig*>(&Account::config());
    }

    void loadConfig() override;

    /**
     * Create incoming SIPCall.
     * @param[in] from The origin of the call
     * @param mediaList A list of media
     * @param sipTr: SIP Transport
     * @return A shared pointer on the created call.
     */
    virtual std::shared_ptr<SIPCall> newIncomingCall(const std::string& from,
                                                     const std::vector<libjami::MediaMap>& mediaList,
                                                     const std::shared_ptr<SipTransport>& sipTr = {})
        = 0;

    virtual bool isStunEnabled() const { return false; }

    virtual pj_uint16_t getStunPort() const { return 0; };

    virtual std::string getDtmfType() const { return config().dtmfType; }

    /**
     * Determine if TLS is enabled for this account. TLS provides a secured channel for
     * SIP signalization. It is independent of the media encryption (as provided by SRTP).
     */
    virtual bool isTlsEnabled() const { return false; }

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
    const std::string& getLocalInterface() const { return config().interface; }

    /**
     * Get the public IP address set by the user for this account.
     * If this setting is not provided, the local bound adddress
     * will be used.
     * @return std::string The public IPv4 or IPv6 address formatted in standard notation.
     */
    std::string getPublishedAddress() const { return config().publishedIp; }

    virtual dhtnet::IpAddr getPublishedIpAddress(uint16_t family = PF_UNSPEC) const;

    void setPublishedAddress(const dhtnet::IpAddr& ip_addr);

    /**
     * Get a flag which determine the usage in sip headers of either the local
     * IP address and port (_localAddress and localPort_) or to an address set
     * manually (_publishedAddress and publishedPort_).
     */
    bool getPublishedSameasLocal() const { return config().publishedSameasLocal; }

    virtual bool isSrtpEnabled() const = 0;

    virtual bool getSrtpFallback() const = 0;

    virtual std::string getToUri(const std::string& username) const = 0;

    /**
     * Socket port generators for media
     * Note: given ports are application wide, a port cannot be given again
     * by any account instances until it's released by the static method
     * releasePort().
     */
    uint16_t generateAudioPort() const;
#ifdef ENABLE_VIDEO
    uint16_t generateVideoPort() const;
#endif
    static void releasePort(uint16_t port) noexcept;

    virtual dhtnet::IceTransportOptions getIceOptions() const noexcept;

    virtual void sendMessage(const std::string& to,
                             const std::string& deviceId,
                             const std::map<std::string, std::string>& payloads,
                             uint64_t id,
                             bool retryOnTimeout = true,
                             bool onlyConnected = false)
        = 0;

    virtual uint64_t sendTextMessage(const std::string& to,
                                     const std::string& deviceId,
                                     const std::map<std::string, std::string>& payloads,
                                     uint64_t refreshToken = 0,
                                     bool onlyConnected = false) override
    {
        if (onlyConnected) {
            auto token = std::uniform_int_distribution<uint64_t> {1, JAMI_ID_MAX_VAL}(rand);
            sendMessage(to, deviceId, payloads, token, false, true);
            return token;
        }
        return messageEngine_.sendMessage(to, deviceId, payloads, refreshToken);
    }

    im::MessageStatus getMessageStatus(uint64_t id) const override
    {
        return messageEngine_.getStatus(id);
    }

    bool cancelMessage(uint64_t id) override
    {
        return messageEngine_.cancel(id);
    }

    virtual void onTextMessage(const std::string& id,
                               const std::string& from,
                               const std::string& deviceId,
                               const std::map<std::string, std::string>& payloads);

    /* Returns true if the username and/or hostname match this account */
    virtual MatchRank matches(std::string_view username, std::string_view hostname) const = 0;

    void connectivityChanged() override {};

    virtual std::string getUserUri() const = 0;

    std::vector<libjami::Message> getLastMessages(const uint64_t& base_timestamp) override;

    // Build the list of medias to be included in the SDP (offer/answer)
    std::vector<MediaAttribute> createDefaultMediaList(bool addVideo, bool onHold = false);

public: // overloaded methods
    virtual void flush() override;

protected:
    /**
     * Retrieve volatile details such as recent registration errors
     * @return std::map< std::string, std::string > The account volatile details
     */
    virtual std::map<std::string, std::string> getVolatileAccountDetails() const override;

    virtual void setRegistrationState(RegistrationState state,
                                      int code = 0,
                                      const std::string& detail_str = {}) override;

    im::MessageEngine messageEngine_;

    /**
     * Voice over IP Link contains a listener thread and calls
     */
    SIPVoIPLink& link_;

    /**
     * Published IPv4/IPv6 addresses, used only if defined by the user in account
     * configuration
     *
     */
    dhtnet::IpAddr publishedIp_[2] {};

    pj_status_t transportStatus_ {PJSIP_SC_TRYING};
    std::string transportError_ {};

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
    std::deque<libjami::Message> lastMessages_;

    std::string composingUri_;
    std::chrono::steady_clock::time_point composingTime_ {
        std::chrono::steady_clock::time_point::min()};
    std::shared_ptr<Task> composingTimeout_;

    std::shared_ptr<dhtnet::TurnCache> turnCache_;

private:
    NON_COPYABLE(SIPAccountBase);
};

} // namespace jami
