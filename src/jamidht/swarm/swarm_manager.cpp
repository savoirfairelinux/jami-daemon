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
#include "jamidht/multiplexed_socket.h"

#include <iostream>

namespace jami {

SwarmManager::SwarmManager(const NodeId& id)
    : myId(id)
{}

void
SwarmManager::setKnownNodes(const std::set<NodeId>& known_nodes)
{
    for (const auto& nodeId : known_nodes)

        addKnownNodes(nodeId);

    maintainBuckets();
}

void
SwarmManager::addKnownNodes(const NodeId& nodeId)
{
    routing_table.addKnownNode(nodeId);
}

void
SwarmManager::sendNodesRequest(const std::shared_ptr<ChannelSocket>& socket,
                               NodeId& nodeId,
                               swarm_protocol::Query q,
                               int numberNodes)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;

    Request toRequest {q, numberNodes, nodeId};
    pk.pack(toRequest);

    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    if (ec) {
        JAMI_ERR("%s", ec.message().c_str());
        return;
    }
}

void
SwarmManager::sendNodesAnswer(const std::shared_ptr<ChannelSocket>& socket, Message msg)
{
    auto bucket = routing_table.findBucket(msg.nodeId);
    Response toResponse;
    std::set<NodeId> nodesToSend;

    if (msg.request->q == swarm_protocol::Query::FIND_KNODES) {
        nodesToSend = bucket->getKnownNodes();

    }

    else if (msg.request->q == swarm_protocol::Query::FIND_NODES) {
        nodesToSend = bucket->getNodeIds();
    }

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;
}

void
SwarmManager::receiveMessage(const std::shared_ptr<ChannelSocket>& socket)
{
    socket->setOnRecv([w = weak()](const uint8_t* buf, size_t len) {
        auto shared = w.lock();

        Message msg;
        try {
            msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(buf), len);
            oh.get().convert(msg);

        } catch (const std::exception& e) {
            JAMI_WARN("Error DRT recv: %s", e.what());
            return len;
        }

        if (msg.request) {
            swarm_protocol::Query q = msg.request->q;

            if (q == swarm_protocol::Query::FIND) {
                int numberNodes = msg.request->num;
                NodeId id = msg.request->nodeId;
                auto bucket = shared->routing_table.closestNodes(id, numberNodes);
            }
        }

        if (msg.response) {
            swarm_protocol::Query q = msg.request->q;
            if (q == swarm_protocol::Query::FOUND) {
                std::set<NodeId> nodes = msg.response->nodes;

                shared->setKnownNodes(nodes);
            }
        }
    });
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

void
SwarmManager::tryConnect(const NodeId& nodeId)
{
    auto bucket = routing_table.findBucket(nodeId);
    bucket->removeKnownNode(nodeId);
    bucket->addConnectingNode(nodeId);

    needSocketCb_(nodeId.toString(), [&](const std::shared_ptr<ChannelSocket>& socket) {
        if (socket) {
            routing_table.addNode(socket, bucket);
            sendNodesRequest(socket);
            // bucket->resetNodeExpiry(socket);

        } else {
            routing_table.addKnownNode(nodeId);
        }
        return true;
    });
}

void
SwarmManager::addChannel(const std::shared_ptr<ChannelSocket>& channel)
{
    JAMI_ERROR("|||||||||||||||ADD CHANNEL SWARM ##############");
};

} // namespace jami
