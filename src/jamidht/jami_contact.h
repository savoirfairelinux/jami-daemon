/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "string_utils.h"
#include "timestamp.h"

#include <opendht/infohash.h>
#include <opendht/value.h>
#include <opendht/default_types.h>

#include <msgpack.hpp>
#include <json/json.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace jami {

namespace ContactMapKeys {
static constexpr const char* ADDED {"added"};
static constexpr const char* REMOVED {"removed"};
static constexpr const char* CONFIRMED {"confirmed"};
static constexpr const char* BANNED {"banned"};
static constexpr const char* CONVERSATIONID {"conversationId"};
static constexpr const char* DEVICE {"device"};
static constexpr const char* RECEIVED {"received"};
static constexpr const char* PAYLOAD {"payload"};
// Millisecond-resolution variants. Legacy keys above keep carrying seconds so
// that older devices (which ignore unknown keys) remain compatible.
static constexpr const char* ADDED_MS {"addedMs"};
static constexpr const char* REMOVED_MS {"removedMs"};
static constexpr const char* RECEIVED_MS {"receivedMs"};
} // namespace ContactMapKeys

struct Contact
{
    /** Time of contact addition */
    TimePoint added {};

    /** Time of contact removal */
    TimePoint removed {};

    /** True if we got confirmation that this contact also added us */
    bool confirmed {false};

    /** True if the contact is banned (if not active) */
    bool banned {false};

    /** Non empty if a swarm is linked */
    std::string conversationId {};

    /** True if the contact is an active contact (not banned nor removed) */
    bool isActive() const { return added > removed; }
    bool isBanned() const { return not isActive() and banned; }

    Contact() = default;
    Contact(const Json::Value& json)
    {
        // Prefer the millisecond keys, fall back to the legacy seconds keys
        // (written by older devices).
        if (json.isMember(ContactMapKeys::ADDED_MS))
            added = timePointFromMilliseconds(json[ContactMapKeys::ADDED_MS].asLargestInt());
        else
            added = timePointFromSeconds(json[ContactMapKeys::ADDED].asLargestInt());
        if (json.isMember(ContactMapKeys::REMOVED_MS))
            removed = timePointFromMilliseconds(json[ContactMapKeys::REMOVED_MS].asLargestInt());
        else
            removed = timePointFromSeconds(json[ContactMapKeys::REMOVED].asLargestInt());
        confirmed = json[ContactMapKeys::CONFIRMED].asBool();
        banned = json[ContactMapKeys::BANNED].asBool();
        conversationId = json[ContactMapKeys::CONVERSATIONID].asString();
    }

    /**
     * Update this contact using other known contact information,
     * return true if contact state was changed.
     */
    bool update(const Contact& c)
    {
        const auto copy = *this;
        auto isMoreRecent = std::max(c.added, c.removed) > std::max(added, removed);
        if (isMoreRecent) {
            added = c.added;
            removed = c.removed;
            banned = c.banned;
            conversationId = c.conversationId;
            confirmed = c.confirmed;
        } else if (isActive() && added == c.added) {
            confirmed = confirmed or c.confirmed;
        }
        return hasDifferentState(copy);
    }

    bool hasDifferentState(const Contact& other) const
    {
        return other.isActive() != isActive() or other.isBanned() != isBanned() or other.confirmed != confirmed;
    }

    Json::Value toJson() const
    {
        Json::Value json;
        json[ContactMapKeys::ADDED] = Json::Int64(toSecondsSinceEpoch(added));
        json[ContactMapKeys::ADDED_MS] = Json::Int64(toMillisecondsSinceEpoch(added));
        if (removed != TimePoint {}) {
            json[ContactMapKeys::REMOVED] = Json::Int64(toSecondsSinceEpoch(removed));
            json[ContactMapKeys::REMOVED_MS] = Json::Int64(toMillisecondsSinceEpoch(removed));
        }
        if (confirmed)
            json[ContactMapKeys::CONFIRMED] = confirmed;
        if (banned)
            json[ContactMapKeys::BANNED] = banned;
        json[ContactMapKeys::CONVERSATIONID] = conversationId;
        return json;
    }

    std::map<std::string, std::string> toMap() const
    {
        std::map<std::string, std::string> result {{"added", std::to_string(toSecondsSinceEpoch(added))},
                                                   {"removed", std::to_string(toSecondsSinceEpoch(removed))},
                                                   {"conversationId", conversationId}};

        if (isActive())
            result.emplace("confirmed", confirmed ? TRUE_STR : FALSE_STR);
        if (isBanned())
            result.emplace("banned", TRUE_STR);

        return result;
    }

    // Hand-written msgpack serialization (replaces MSGPACK_DEFINE_MAP) to emit
    // dual keys: legacy seconds (added/removed) + milliseconds
    // (addedMs/removedMs). Readers prefer the ms keys and fall back to
    // seconds * 1000.
    template<typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        int64_t addedSec = toSecondsSinceEpoch(added);
        int64_t removedSec = toSecondsSinceEpoch(removed);
        int64_t addedMs = toMillisecondsSinceEpoch(added);
        int64_t removedMs = toMillisecondsSinceEpoch(removed);
        msgpack::type::make_define_map(ContactMapKeys::ADDED,
                                       addedSec,
                                       ContactMapKeys::REMOVED,
                                       removedSec,
                                       ContactMapKeys::CONFIRMED,
                                       confirmed,
                                       ContactMapKeys::BANNED,
                                       banned,
                                       ContactMapKeys::CONVERSATIONID,
                                       conversationId,
                                       ContactMapKeys::ADDED_MS,
                                       addedMs,
                                       ContactMapKeys::REMOVED_MS,
                                       removedMs)
            .msgpack_pack(pk);
    }

    void msgpack_unpack(const msgpack::object& o)
    {
        if (o.type != msgpack::type::MAP)
            throw msgpack::type_error();
        int64_t addedSec = 0, removedSec = 0;
        std::optional<int64_t> addedMs, removedMs;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            const auto& kv = o.via.map.ptr[i];
            if (kv.key.type != msgpack::type::STR)
                continue;
            std::string_view key(kv.key.via.str.ptr, kv.key.via.str.size);
            if (key == ContactMapKeys::ADDED)
                kv.val.convert(addedSec);
            else if (key == ContactMapKeys::REMOVED)
                kv.val.convert(removedSec);
            else if (key == ContactMapKeys::ADDED_MS)
                addedMs = kv.val.as<int64_t>();
            else if (key == ContactMapKeys::REMOVED_MS)
                removedMs = kv.val.as<int64_t>();
            else if (key == ContactMapKeys::CONFIRMED)
                kv.val.convert(confirmed);
            else if (key == ContactMapKeys::BANNED)
                kv.val.convert(banned);
            else if (key == ContactMapKeys::CONVERSATIONID)
                kv.val.convert(conversationId);
        }
        added = addedMs ? timePointFromMilliseconds(*addedMs) : timePointFromSeconds(addedSec);
        removed = removedMs ? timePointFromMilliseconds(*removedMs) : timePointFromSeconds(removedSec);
    }

    template<typename MSGPACK_OBJECT>
    void msgpack_object(MSGPACK_OBJECT* o, msgpack::zone& z) const
    {
        int64_t addedSec = toSecondsSinceEpoch(added);
        int64_t removedSec = toSecondsSinceEpoch(removed);
        int64_t addedMs = toMillisecondsSinceEpoch(added);
        int64_t removedMs = toMillisecondsSinceEpoch(removed);
        msgpack::type::make_define_map(ContactMapKeys::ADDED,
                                       addedSec,
                                       ContactMapKeys::REMOVED,
                                       removedSec,
                                       ContactMapKeys::CONFIRMED,
                                       confirmed,
                                       ContactMapKeys::BANNED,
                                       banned,
                                       ContactMapKeys::CONVERSATIONID,
                                       conversationId,
                                       ContactMapKeys::ADDED_MS,
                                       addedMs,
                                       ContactMapKeys::REMOVED_MS,
                                       removedMs)
            .msgpack_object(o, z);
    }
};

struct TrustRequest
{
    std::shared_ptr<dht::crypto::PublicKey> device;
    std::string conversationId;
    TimePoint received {};
    std::vector<uint8_t> payload;

    // Hand-written msgpack serialization (replaces MSGPACK_DEFINE_MAP) to emit
    // dual keys: legacy seconds (received) + milliseconds (receivedMs). Readers
    // prefer the ms key and fall back to seconds * 1000.
    template<typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        int64_t receivedSec = toSecondsSinceEpoch(received);
        int64_t receivedMs = toMillisecondsSinceEpoch(received);
        msgpack::type::make_define_map(ContactMapKeys::DEVICE,
                                       device,
                                       ContactMapKeys::CONVERSATIONID,
                                       conversationId,
                                       ContactMapKeys::RECEIVED,
                                       receivedSec,
                                       ContactMapKeys::PAYLOAD,
                                       payload,
                                       ContactMapKeys::RECEIVED_MS,
                                       receivedMs)
            .msgpack_pack(pk);
    }

    void msgpack_unpack(const msgpack::object& o)
    {
        if (o.type != msgpack::type::MAP)
            throw msgpack::type_error();
        int64_t receivedSec = 0;
        std::optional<int64_t> receivedMs;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            const auto& kv = o.via.map.ptr[i];
            if (kv.key.type != msgpack::type::STR)
                continue;
            std::string_view key(kv.key.via.str.ptr, kv.key.via.str.size);
            if (key == ContactMapKeys::DEVICE)
                kv.val.convert(device);
            else if (key == ContactMapKeys::CONVERSATIONID)
                kv.val.convert(conversationId);
            else if (key == ContactMapKeys::RECEIVED)
                kv.val.convert(receivedSec);
            else if (key == ContactMapKeys::RECEIVED_MS)
                receivedMs = kv.val.as<int64_t>();
            else if (key == ContactMapKeys::PAYLOAD)
                kv.val.convert(payload);
        }
        received = receivedMs ? timePointFromMilliseconds(*receivedMs) : timePointFromSeconds(receivedSec);
    }

    template<typename MSGPACK_OBJECT>
    void msgpack_object(MSGPACK_OBJECT* o, msgpack::zone& z) const
    {
        int64_t receivedSec = toSecondsSinceEpoch(received);
        int64_t receivedMs = toMillisecondsSinceEpoch(received);
        msgpack::type::make_define_map(ContactMapKeys::DEVICE,
                                       device,
                                       ContactMapKeys::CONVERSATIONID,
                                       conversationId,
                                       ContactMapKeys::RECEIVED,
                                       receivedSec,
                                       ContactMapKeys::PAYLOAD,
                                       payload,
                                       ContactMapKeys::RECEIVED_MS,
                                       receivedMs)
            .msgpack_object(o, z);
    }
};

struct DeviceAnnouncement : public dht::SignedValue<DeviceAnnouncement>
{
private:
    using BaseClass = dht::SignedValue<DeviceAnnouncement>;

public:
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    dht::InfoHash dev;
    std::shared_ptr<dht::crypto::PublicKey> pk;
    MSGPACK_DEFINE_MAP(dev, pk)
};

struct KnownDeviceSync
{
    std::string name;
    MSGPACK_DEFINE_MAP(name)
};

struct DeviceSync : public dht::EncryptedValue<DeviceSync>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    uint64_t date;
    std::string device_name;
    std::map<dht::PkId, KnownDeviceSync> devices;
    std::map<dht::InfoHash, Contact> peers;
    std::map<dht::InfoHash, TrustRequest> trust_requests;
    MSGPACK_DEFINE_MAP(date, device_name, devices, peers, trust_requests)
};

struct KnownDevice
{
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
        : certificate(cert)
        , name(n)
        , last_sync(sync)
    {}
};

} // namespace jami
