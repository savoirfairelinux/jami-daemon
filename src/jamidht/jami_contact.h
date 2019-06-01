/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "string_utils.h"

#include <opendht/infohash.h>
#include <opendht/value.h>
#include <opendht/default_types.h>

#include <msgpack.hpp>
#include <json/json.h>

#include <map>
#include <string>
#include <ctime>
#include <ciso646>

namespace jami {

struct Contact
{
    /** Time of contact addition */
    time_t added {0};

    /** Time of contact removal */
    time_t removed {0};

    /** True if we got confirmation that this contact also added us */
    bool confirmed {false};

    /** True if the contact is banned (if not active) */
    bool banned {false};

    /** True if the contact is an active contact (not banned nor removed) */
    bool isActive() const { return added > removed; }
    bool isBanned() const { return not isActive() and banned; }

    Contact() = default;
    Contact(const Json::Value& json) {
        added = json["added"].asInt();
        removed = json["removed"].asInt();
        confirmed = json["confirmed"].asBool();
        banned = json["banned"].asBool();
    }

    /**
     * Update this contact using other known contact information,
     * return true if contact state was changed.
     */
    bool update(const Contact& c) {
        const auto copy = *this;
        if (c.added > added) {
            added = c.added;
        }
        if (c.removed > removed) {
            removed = c.removed;
            banned = c.banned;
        }
        if (c.confirmed != confirmed) {
            confirmed = c.confirmed or confirmed;
        }
        return hasDifferentState(copy);
    }
    bool hasDifferentState(const Contact& other) const {
        return other.isActive() != isActive()
            or other.isBanned() != isBanned()
            or other.confirmed  != confirmed;
    }

    Json::Value toJson() const {
        Json::Value json;
        json["added"] = Json::Int64(added);
        json["removed"] = Json::Int64(removed);
        json["confirmed"] = confirmed;
        json["banned"] = banned;
        return json;
    }

    std::map<std::string, std::string> toMap() const {
        if (not (isActive() or isBanned())) {
            return {};
        }

        std::map<std::string, std::string> result {
            {"added", std::to_string(added)}
        };

        if (isActive())
            result.emplace("confirmed", confirmed ? TRUE_STR : FALSE_STR);
        else if (isBanned())
            result.emplace("banned", TRUE_STR);

        return result;
    }

    MSGPACK_DEFINE_MAP(added, removed, confirmed, banned)
};

struct TrustRequest {
    dht::InfoHash device;
    time_t received;
    std::vector<uint8_t> payload;
    MSGPACK_DEFINE_MAP(device, received, payload)
};

struct DeviceAnnouncement : public dht::SignedValue<DeviceAnnouncement>
{
private:
    using BaseClass = dht::SignedValue<DeviceAnnouncement>;
public:
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    dht::InfoHash dev;
    MSGPACK_DEFINE_MAP(dev);
};

struct DeviceSync : public dht::EncryptedValue<DeviceSync>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    uint64_t date;
    std::string device_name;
    std::map<dht::InfoHash, std::string> devices_known;
    std::map<dht::InfoHash, Contact> peers;
    std::map<dht::InfoHash, TrustRequest> trust_requests;
    MSGPACK_DEFINE_MAP(date, device_name, devices_known, peers, trust_requests)
};

struct KnownDevice {
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;

    /** Device certificate */
    std::shared_ptr<dht::crypto::Certificate> certificate;

    /** Device name */
    std::string name {};

    /** Time of last received device sync */
    time_point last_sync {time_point::min()};

    KnownDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                const std::string& n = {},
                time_point sync = time_point::min())
        : certificate(cert), name(n), last_sync(sync) {}
};

}
