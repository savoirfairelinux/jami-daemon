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
#include "account_config.h"

namespace jami {
constexpr static const char* const OVERRTP_STR = "overrtp";
constexpr static const char* const SIPINFO_STR = "sipinfo";
constexpr static unsigned MAX_PORT {65536};
constexpr static unsigned HALF_MAX_PORT {MAX_PORT / 2};

struct SipAccountBaseConfig: public AccountConfig {
    SipAccountBaseConfig(const std::string& type, const std::string& id, const std::string& path): AccountConfig(type, id, path) {}

    void serializeDiff(YAML::Emitter& out, const SipAccountBaseConfig& def) const;
    void unserialize(const YAML::Node& node) override;

    std::map<std::string, std::string> toMap() const override;
    void fromMap(const std::map<std::string, std::string>&) override;

    /**
     * interface name on which this account is bound
     */
    std::string interface {"default"};

    /**
     * Flag which determine if localIpAddress_ or publishedIpAddress_ is used in
     * sip headers
     */
    bool publishedSameasLocal {true};

    std::string publishedIp;

    /**
     * Determine if TURN public address resolution is required to register this account. In this
     * case a TURN server hostname must be specified.
     */
    bool turnEnabled {false};

    /**
     * The TURN server hostname (optional), used to provide the public IP address in case the
     * softphone stay behind a NAT.
     */
    std::string turnServer;
    std::string turnServerUserName;
    std::string turnServerPwd;
    std::string turnServerRealm;

    std::string tlsCaListFile;
    std::string tlsCertificateFile;
    std::string tlsPrivateKeyFile;
    std::string tlsPassword;

    std::string dtmfType {OVERRTP_STR};
    /*
     * Port range for audio RTP ports
     */
    std::pair<uint16_t, uint16_t> audioPortRange {16384, 32766};

    /**
     * Port range for video RTP ports
     */
    std::pair<uint16_t, uint16_t> videoPortRange {49152, (65536) -2};
};

inline void
updateRange(uint16_t min, uint16_t max, std::pair<uint16_t, uint16_t>& range)
{
    if (min > 0 and (max > min) and max <= MAX_PORT - 2) {
        range.first = min;
        range.second = max;
    }
}

}
