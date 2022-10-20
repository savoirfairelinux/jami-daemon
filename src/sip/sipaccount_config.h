/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
#include "sipaccountbase_config.h"

namespace jami {

struct SipAccountConfig : public SipAccountBaseConfig {
    void serialize(YAML::Emitter& out) const override;
    void unserialize(const YAML::Node& node) override;

    std::map<std::string, std::string> toMap() const override;
    void fromMap(const std::map<std::string, std::string>&) override;

    std::string username;

    /**
     * Local port to whih this account is bound
     */
    uint16_t localPort {0};

    /**
     * Potential ip addresss on which this account is bound
     */
    std::string bindAddress {};

    /**
     * Published port, used only if defined by the user
     */
    uint16_t publishedPort {5060};

    /**
     * interface name on which this account is bound
     */
    std::string interface;

    /**
     * Determine if STUN public address resolution is required to register this account. In this
     * case a STUN server hostname must be specified.
     */
    bool stunEnabled {false};

    /**
     * The STUN server hostname (optional), used to provide the public IP address in case the
     * softphone stay behind a NAT.
     */
    std::string stunServer {};

    /**
     * Network settings
     */
    unsigned registrationExpire {3600};
    bool registrationRefreshEnabled {true};

    // If true, the contact addreass and header will be rewritten
    // using the information received from the registrar.
    bool allowIPAutoRewrite {true};

    /**
     * Input Outbound Proxy Server Address
     */
    std::string serviceRoute;

    /**
     * The TLS listener port
     */
    uint16_t tlsListenerPort {sip_utils::DEFAULT_AUTO_SELECT_PORT};
    bool tlsEnable {false};
    std::string tlsMethod;
    std::string tlsCiphers;
    std::string tlsServerName;
    bool tlsVerifyServer {true};
    bool tlsVerifyClient {true};
    bool tlsRequireClientCertificate {true};
    int tlsNegotiationTimeout {2};

    /**
     * Determine if the softphone should fallback on non secured media channel if SRTP negotiation
     * fails. Make sure other SIP endpoints share the same behavior since it could result in
     * encrypted data to be played through the audio device.
     */
    bool srtpFallback {false};

    bool presenceEnabled {false};
    bool publishSupported {false};
    bool subscribeSupported {false};
};

}
