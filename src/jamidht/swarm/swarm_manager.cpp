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

#include "swarm_manager.h"
#include "swarm_protocol.h"
#include "jamidht/multiplexed_socket.h"

#include <iostream>

namespace jami {

using Request = swarm_protocol::Request;
using Response = swarm_protocol::Response;
using Message = swarm_protocol::Message;

SwarmManager::SwarmManager(const NodeId& id)
    : myId(id)
{}

/**
 * Add list of nodes to the known nodes list
 * @param std::vector<NodeId>& known_nodes
 */
void
SwarmManager::setKnownNodes(const std::vector<NodeId>& known_nodes)
{
    for (const auto& nodeId : known_nodes)

        addKnownNodes(nodeId);

    maintainBuckets();
}

/**
 * Add node to the known nodes list
 * @param NodeId nodeId
 */
void
SwarmManager::addKnownNodes(const NodeId& nodeId)
{
    routing_table.addKnownNode(nodeId);
}

/**
 * Send nodes request to fill known_nodes list
 * @param NodeId nodeId
 */
void
SwarmManager::sendNodesRequest(const std::shared_ptr<ChannelSocket>& socket)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    Request toRequest {jami::swarm_protocol::Query::FIND, 2};
    pk.pack(toRequest);

    // send(socket->channel, buffer.data(), buffer.size());
}

void
SwarmManager::sendNodesAnswer(const std::shared_ptr<ChannelSocket>& socket)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    auto bucket = routing_table.findBucket(socket->deviceId());
    std::set<NodeId> nodesToSend = bucket->getKnownNodes();
    Response toResponse {nodesToSend};
}

void
SwarmManager::maintainBuckets()
{
    for (auto it = routing_table.buckets.begin(); it != routing_table.buckets.end(); ++it) {
        auto& bucket = *it;
        auto connecting_nodes = bucket.getConnectingNodesSize() + bucket.getNodesSize();
        if (connecting_nodes < Bucket::BUCKET_MAX_SIZE) {
            if (routing_table.contains(it, myId)) {
                auto closest_nodes = routing_table.getKnownNodesRandom(myId,
                                                                       Bucket::BUCKET_MAX_SIZE
                                                                           - connecting_nodes);
                for (auto node : closest_nodes) {
                    tryConnect(node);
                }
            } else {
                auto id = bucket.randomId();
                auto closest_nodes = routing_table.getKnownNodesRandom(id,
                                                                       Bucket::BUCKET_MAX_SIZE
                                                                           - connecting_nodes);
                for (auto node : closest_nodes) {
                    tryConnect(node);
                }
            }
        }
    }
}

/**
 * Add connected node to the table's bucket
 * @param NodeId nodeId
 */
void
SwarmManager::tryConnect(const NodeId& nodeId)
{
    auto bucket = routing_table.findBucket(nodeId);
    bucket->removeKnownNode(nodeId);
    bucket->addConnectingNode(nodeId);

    needSocketCb_(nodeId.toString(), [&](const std::shared_ptr<ChannelSocket>& socket) {
        if (socket) {
            routing_table.addNode(socket, bucket);
        } else {
            // routing_table.failedNode(socket);
        }
        return true;
    });
}

void
SwarmManager::receiveMessage()
{
    /*
        auto oh = msgpack::unpack(str.data(), str.size());
        msgpack::object obj = oh.get();
        //

         if find
        recuperer le bucket et envoyer la liste.
        */
}

} // namespace jami
