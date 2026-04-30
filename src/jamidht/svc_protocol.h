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

#include <cstdint>
#include <string>
#include <vector>

#include <msgpack.hpp>

namespace jami {
namespace svc_protocol {

/// Maximum protocol version implemented.
constexpr uint8_t kMaxVersion = 1;

/// Discovery message type discriminators.
namespace MsgType {
constexpr const char* kQuery            = "query";
constexpr const char* kServiceList      = "service_list";
constexpr const char* kError            = "error";
constexpr const char* kVersionMismatch  = "version_mismatch";
} // namespace MsgType

/// Channel name prefix used for tunnels: "svc://<service-uuid>".
constexpr const char* kTunnelChannelPrefix = "svc://";
/// Channel name used for discovery: "svcdisc://query".
constexpr const char* kDiscoveryChannelName = "svcdisc://query";

/// Single service descriptor exposed in a service_list response.
struct SvcInfo
{
    std::string id;          ///< RFC 4122 v4 UUID
    std::string name;
    std::string description;
    std::string proto;       ///< "tcp" in v1
    std::string scheme;      ///< Optional URI scheme hint (e.g. "http", "https"); empty means raw TCP
    MSGPACK_DEFINE_MAP(id, name, description, proto, scheme)
};

/// Request sent by the client over `svcdisc://query`.
struct SvcDiscQuery
{
    uint8_t v {kMaxVersion};
    std::string type {MsgType::kQuery};
    MSGPACK_DEFINE_MAP(v, type)
};

/// Successful response listing the services visible to the requesting peer.
struct SvcDiscResponse
{
    uint8_t v {kMaxVersion};
    std::string type {MsgType::kServiceList};
    /// Long device id of the responder, so the requester can target the
    /// exact device when opening a tunnel without a separate lookup.
    std::string device;
    std::vector<SvcInfo> services;
    MSGPACK_DEFINE_MAP(v, type, device, services)
};

/// Application-level error response.
struct SvcDiscError
{
    uint8_t v {kMaxVersion};
    std::string type {MsgType::kError};
    uint16_t code {0};
    std::string message;
    MSGPACK_DEFINE_MAP(v, type, code, message)
};

/// Sent when the client requested a higher protocol version than supported.
struct SvcDiscVersionMismatch
{
    uint8_t v {kMaxVersion};
    std::string type {MsgType::kVersionMismatch};
    uint8_t max_supported {kMaxVersion};
    MSGPACK_DEFINE_MAP(v, type, max_supported)
};

/**
 * Try to read the `type` discriminator field from an opaque msgpack object
 * without committing to a specific message struct yet.
 * Returns the type string or an empty string if the field is missing.
 */
inline std::string
peekType(const msgpack::object& obj)
{
    if (obj.type != msgpack::type::MAP)
        return {};
    for (uint32_t i = 0; i < obj.via.map.size; ++i) {
        const auto& kv = obj.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR) {
            std::string_view k(kv.key.via.str.ptr, kv.key.via.str.size);
            if (k == "type" && kv.val.type == msgpack::type::STR)
                return std::string(kv.val.via.str.ptr, kv.val.via.str.size);
        }
    }
    return {};
}

/**
 * Try to read the `v` (version) field from an opaque msgpack object.
 * Returns 0 if the field is missing or not a positive integer.
 */
inline uint8_t
peekVersion(const msgpack::object& obj)
{
    if (obj.type != msgpack::type::MAP)
        return 0;
    for (uint32_t i = 0; i < obj.via.map.size; ++i) {
        const auto& kv = obj.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR) {
            std::string_view k(kv.key.via.str.ptr, kv.key.via.str.size);
            if (k == "v" && kv.val.type == msgpack::type::POSITIVE_INTEGER) {
                auto n = kv.val.via.u64;
                return n > 255 ? 255 : static_cast<uint8_t>(n);
            }
        }
    }
    return 0;
}

} // namespace svc_protocol
} // namespace jami
