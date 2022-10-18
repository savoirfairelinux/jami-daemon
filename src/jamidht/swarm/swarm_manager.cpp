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

using namespace swarm_protocol;

SwarmManager::SwarmManager(const NodeId& id)
    : myId(id)
{
    JAMI_ERROR("SwarmManager::SwarmManager {:s}", id.to_c_str());
}

void
SwarmManager::setKnownNodes(const std::vector<NodeId>& known_nodes)
{
    JAMI_ERROR("############## SETTINGS KNOWNODES ##############");
    for (const auto& nodeId : known_nodes)
        addKnownNodes(nodeId);

    maintainBuckets();
}

void
SwarmManager::addKnownNodes(const NodeId& nodeId)
{
    JAMI_ERROR("SwarmManager::addKnownNodes {:s}", nodeId.to_c_str());
    routing_table.addKnownNode(nodeId);
}

void
SwarmManager::sendRequest(const std::shared_ptr<ChannelSocket>& socket,
                          NodeId& nodeId,
                          Query q,
                          int numberNodes)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;

    Request toRequest {q, numberNodes, nodeId};
    Message msg;
    msg.request = toRequest;
    pk.pack(msg);
    JAMI_ERROR("############## SEND REQUEST ##############");
    JAMI_ERROR("DEVICE ID {:s}", socket->deviceId().to_c_str());

    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    if (ec) {
        JAMI_ERR("%s", ec.message().c_str());
        return;
    }
}

void
SwarmManager::sendAnswer(const std::shared_ptr<ChannelSocket>& socket, const Message& msg_)
{
    Response toResponse;
    Message msg;
    std::vector<NodeId> nodesToSend;

    /*     if (msg_.request->q == Query::FIND_KNODES) {
            auto bucket = routing_table.findBucket(msg_.request->nodeId);
            nodesToSend = bucket->getKnownNodesVector();
            toResponse.q = Query::FOUND_KNODES;

        } */

    if (msg_.request->q == Query::FIND) {
        nodesToSend = routing_table.closestNodes(msg_.request->nodeId, msg_.request->num);
        toResponse.q = Query::FOUND;
    }

    toResponse.nodes = nodesToSend;
    msg.response = toResponse;

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;

    pk.pack(msg);

    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    if (ec) {
        JAMI_ERR("%s", ec.message().c_str());
        return;
    }
}

void
SwarmManager::receiveMessage(const std::shared_ptr<ChannelSocket>& socket)
{
    socket->setOnRecv([w = weak(), socket](const uint8_t* buf, size_t len) {
        auto shared = w.lock();

        Message msg;
        try {
            msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(buf), len);
            oh.get().convert(msg);

        } catch (const std::exception& e) {
            JAMI_WARN("Error DRT recv: %s", e.what());
            return len;
        }

        shared->sendAnswer(socket, msg);

        return len;
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
    JAMI_ERROR("############## TRY CONNECT DEVICE ID {:s}", nodeId.to_c_str());

    needSocketCb_(nodeId.toString(), [&](const std::shared_ptr<ChannelSocket>& socket) {
        if (socket) {
            JAMI_ERROR("############## CONNECTED TO ID {:s}", socket->deviceId().to_c_str());

            routing_table.addNode(socket, bucket);
            bucket->removeConnectingNode(socket->deviceId());
            receiveMessage(socket);
            sendRequest(socket, myId, Query::FIND);
            std::error_code ec;
            resetNodeExpiry(ec, socket);

        } else {
            routing_table.addKnownNode(nodeId);
        }
        return true;
    });
}

void
SwarmManager::addChannel(const std::shared_ptr<ChannelSocket>& channel)
{
    JAMI_ERROR("############## ADD CHANNEL SWARM ##############");
    JAMI_ERROR("DEVICE ID {:s}", channel->deviceId().to_c_str());
    auto bucket = routing_table.findBucket(channel->deviceId());
    routing_table.addNode(channel, bucket);
    receiveMessage(channel);
};

void
SwarmManager::resetNodeExpiry(const asio::error_code& ec,
                              const std::shared_ptr<ChannelSocket>& socket)
{
    if (ec == asio::error::operation_aborted)
        return;
    auto bucket = routing_table.findBucket(socket->deviceId());

    auto idToFind = bucket->randomId();

    sendRequest(socket, idToFind, Query::FIND);

    auto& nodeTimer = bucket->getNodeTimer(socket);
    nodeTimer.expires_after(FIND_PERIOD);
    nodeTimer.async_wait(
        std::bind(&jami::SwarmManager::resetNodeExpiry, this, std::placeholders::_1, socket));
}

} // namespace jami
