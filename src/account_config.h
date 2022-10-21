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
#pragma once
#include "sip/sip_utils.h"
#include "serializable.h"

#include <yaml-cpp/yaml.h>
#include <string>
#include <string_view>
#include <utility>

using namespace std::literals;

namespace jami {

struct AccountConfig: public Serializable {
    AccountConfig(const std::string& p = {}): path(p) {}

    virtual void serialize(YAML::Emitter& out) const;
    virtual void unserialize(const YAML::Node& node);

    virtual std::map<std::string, std::string> toMap() const;
    virtual void fromMap(const std::map<std::string, std::string>&);

    /** Path where the configuration file is stored.
     * Part of the context but not stored in the configuration
     * Used to compute relative paths for configuraton fields */
    const std::string path {};

    /** Account type */
    std::string type {};

    /** A user-defined name for this account */
    std::string alias {};

    /** SIP hostname (SIP account) or DHT bootstrap nodes (Jami account) */
    std::string hostname {};

    /** True if the account is enabled. */
    bool enabled {true};

    /** If true, automatically answer calls to this account */
    bool autoAnswerEnabled {false};

    /** If true, send displayed status (and emit to the client) */
    bool sendReadReceipt {true};

    /** If true mix calls into a conference */
    bool isRendezVous {false};

    /**
     * The number of concurrent calls for the account
     * -1: Unlimited
     *  0: Do not disturb
     *  1: Single call
     *  +: Multi line
     */
    int activeCallLimit {-1};

    [[deprecated]]
    std::vector<unsigned> activeCodecs {};

    std::vector<unsigned> disabledCodecs {};

    /**
     * Play ringtone when receiving a call
     */
    bool ringtoneEnabled {true};

    /**
     * Ringtone .au file used for this account
     */
    std::string ringtonePath {};

    /**
     * Allows user to temporarily disable video calling
     */
    bool videoEnabled {true};

    /**
     * Display name when calling
     */
    std::string displayName {};

    /**
     * User-agent used for registration
     */
    std::string customUserAgent {};

    /**
     * Account mail box
     */
    std::string mailbox {};

    /**
     * UPnP IGD controller and the mutex to access it
     */
    bool upnpEnabled {};

    std::set<std::string> defaultModerators {};
    bool localModeratorsEnabled {true};
    bool allModeratorsEnabled {true};

    bool multiStreamEnabled {false};
    bool iceForMediaEnabled {true};
    bool iceCompIdRfc5245Compliant {false};

    /**
     * Device push notification token.
     */
    std::string deviceKey {};

    /**
     * Push notification topic.
     */
    std::string notificationTopic {};
};

inline void parseString(const std::map<std::string, std::string>& details, const char* key, std::string& s)
{
    auto it = details.find(key);
    if (it != details.end())
        s = it->second;
}

inline void parseBool(const std::map<std::string, std::string>& details, const char* key, bool& s)
{
    auto it = details.find(key);
    if (it != details.end())
        s = it->second == TRUE_STR;
}

template<class T>
inline void parseInt(const std::map<std::string, std::string>& details, const char* key, T& s)
{
    auto it = details.find(key);
    if (it != details.end())
        s = to_int<T>(it->second);
}

void
parsePath(const std::map<std::string, std::string>& details,
                   const char* key,
                   std::string& s,
                   const std::string& base);

}
