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

#include <string>
#include <string_view>
#include <msgpack.hpp>

#include <opendht/crypto.h>
#include <opendht/infohash.h>

using namespace std::literals;
using NodeId = dht::h256;

namespace jami {

namespace swarm_protocol {

static constexpr int version = 3;
static constexpr size_t MAX_MOBILE_CERTIFICATE_SIZE = 64 * 1024;
static constexpr size_t MAX_MOBILE_CERTIFICATES_SIZE = 256 * 1024;
static constexpr size_t MAX_MOBILE_LEASE_SIGNATURE_SIZE = 1024;
static constexpr size_t MAX_MOBILE_LEASE_IDENTIFIER_SIZE = 128;
static constexpr size_t MAX_MOBILE_NODE_INFOS = 128;
static constexpr std::chrono::days MAX_MOBILE_LEASE_DURATION {30};
static constexpr std::chrono::days MOBILE_LEASE_RENEWAL_THRESHOLD {14};
// 2026-10-01T00:00:00Z. A fixed cutoff cannot be extended by restart or gossip.
static constexpr uint64_t LEGACY_MOBILE_NODE_SUNSET {1'790'812'800};

enum class Query : uint8_t { FIND = 1, FOUND = 2 };

using NodeId = dht::PkId;

struct Request
{
    Query q; // Type of query
    int num; // Number of nodes
    NodeId nodeId;
    MSGPACK_DEFINE_MAP(q, num, nodeId);
};

struct MobileLease
{
    uint8_t format_version {1};
    std::string conversation_id;
    dht::InfoHash issuer_id;
    NodeId device_id;
    uint64_t issued_at {0};
    uint64_t expires_at {0};
    dht::Blob signature;

    MSGPACK_DEFINE_MAP(format_version, conversation_id, issuer_id, device_id, issued_at, expires_at, signature);
};

struct MobileNodeInfo
{
    NodeId id;
    dht::Blob certificate;
    std::optional<MobileLease> lease;

    MSGPACK_DEFINE_MAP(id, certificate, lease);
};

dht::Blob mobileLeasePayload(const MobileLease& lease);

struct Response
{
    Query q;
    std::vector<NodeId> nodes;
    std::vector<NodeId> mobile_nodes;
    std::vector<MobileNodeInfo> mobile_node_infos;

    MSGPACK_DEFINE_MAP(q, nodes, mobile_nodes, mobile_node_infos);
};

struct Message
{
    int v = version;
    bool is_mobile {false};
    std::optional<MobileNodeInfo> self_mobile_info;
    std::optional<Request> request;
    std::optional<Response> response;
    MSGPACK_DEFINE_MAP(v, is_mobile, self_mobile_info, request, response);
};

}; // namespace swarm_protocol

} // namespace jami
MSGPACK_ADD_ENUM(jami::swarm_protocol::Query);
