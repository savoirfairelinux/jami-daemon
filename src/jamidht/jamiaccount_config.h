/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#pragma once
#include "sip/sipaccountbase_config.h"

namespace jami {
constexpr static std::string_view ACCOUNT_TYPE_JAMI = "RING";
constexpr static const char* const DHT_DEFAULT_BOOTSTRAP = "bootstrap.jami.net";
constexpr static const char* DEFAULT_TURN_SERVER = "turn.jami.net";
constexpr static const char* DEFAULT_TURN_USERNAME = "ring";
constexpr static const char* DEFAULT_TURN_PWD = "ring";
constexpr static const char* DEFAULT_TURN_REALM = "ring";

struct JamiAccountConfig : public SipAccountBaseConfig {
    JamiAccountConfig(const std::string& id = {}, const std::string& path = {})
        : SipAccountBaseConfig(std::string(ACCOUNT_TYPE_JAMI), id, path)
    {
        // Default values specific to Jami accounts
        hostname = DHT_DEFAULT_BOOTSTRAP;
        turnServer = DEFAULT_TURN_SERVER;
        turnServerUserName = DEFAULT_TURN_USERNAME;
        turnServerPwd = DEFAULT_TURN_PWD;
        turnServerRealm = DEFAULT_TURN_REALM;
        turnEnabled = true;
        upnpEnabled = true;
    }
    void serialize(YAML::Emitter& out) const override;
    void unserialize(const YAML::Node& node) override;
    std::map<std::string, std::string> toMap() const override;
    void fromMap(const std::map<std::string, std::string>&) override;

    std::string deviceName {};
    uint16_t dhtPort {0};
    bool dhtPeerDiscovery {false};
    bool accountPeerDiscovery {false};
    bool accountPublish {false};
    std::string bootstrapListUrl {"https://config.jami.net/bootstrapList"};

    bool proxyEnabled {false};
    std::string proxyServer {"dhtproxy.jami.net:[80-95]"};
    std::string proxyListUrl {"https://config.jami.net/proxyList"};

    std::string nameServer {};
    std::string registeredName {};

    bool allowPeersFromHistory {true};
    bool allowPeersFromContact {true};
    bool allowPeersFromTrusted {true};
    bool allowPublicIncoming {true};

    std::string managerUri {};
    std::string managerUsername {};

    std::string archivePath {"archive.gz"};
    bool archiveHasPassword {true};
    // not saved, only used client->daemon
    std::string archive_password;
    std::string archive_pin;
    std::string archive_path;

    std::string receipt {};
    std::vector<uint8_t> receiptSignature {};
};

}
