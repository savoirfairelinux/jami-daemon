/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *  Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>
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

#include <string>
#include <string_view>
#include <msgpack.hpp>

#include <opendht/infohash.h>

using namespace std::literals;
using NodeId = dht::h256;

namespace jami {

namespace swarm_protocol {

static constexpr int version = 1;

enum class Query : uint8_t { FIND = 1, FOUND = 2 };

using NodeId = dht::PkId;

struct Request
{
    Query q; // Type of query
    int num; // Number of nodes
    NodeId nodeId;
    MSGPACK_DEFINE_MAP(q, num, nodeId);
};

struct Response
{
    Query q;
    std::vector<NodeId> nodes;
    std::vector<NodeId> mobile_nodes;

    MSGPACK_DEFINE_MAP(q, nodes, mobile_nodes);
};

struct Message
{
    int v = version;
    bool is_mobile {false};
    std::optional<Request> request;
    std::optional<Response> response;
    MSGPACK_DEFINE_MAP(v, is_mobile, request, response);
};

}; // namespace swarm_protocol

} // namespace jami
MSGPACK_ADD_ENUM(jami::swarm_protocol::Query);
