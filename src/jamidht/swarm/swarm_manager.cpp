/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
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

#include <iostream>

#include "swarm_manager.h"
#include "connectivity/multiplexed_socket.h"
#include <opendht/thread_pool.h>

constexpr const std::chrono::minutes FIND_PERIOD {10};

namespace jami {

using namespace swarm_protocol;

SwarmManager::SwarmManager(const NodeId& id)
    : id_(id)
    , rd(dht::crypto::getSeededRandomEngine<std::mt19937_64>())
{
    routing_table.setId(id);
}

SwarmManager::~SwarmManager()
{
    shutdown();
}

void
SwarmManager::removeNode(const NodeId& nodeId)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        removeNodeInternal(nodeId);
    }
    maintainBuckets();
}

void
SwarmManager::removeNodeInternal(const NodeId& nodeId)
{
    routing_table.removeNode(nodeId);
}

void
SwarmManager::setKnownNodes(const std::vector<NodeId>& known_nodes)
{
    isShutdown_ = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& NodeId : known_nodes)
            addKnownNodes(std::move(NodeId));
    }
    maintainBuckets();
}

void
SwarmManager::setMobileNodes(const std::vector<NodeId>& mobile_nodes)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& nodeId : mobile_nodes)
            addMobileNodes(nodeId);
    }
    maintainBuckets();
}

void
SwarmManager::addKnownNodes(const NodeId& nodeId)
{
    if (id_ != nodeId) {
        routing_table.addKnownNode(nodeId);
    }
}

void
SwarmManager::addMobileNodes(const NodeId& nodeId)
{
    if (id_ != nodeId) {
        routing_table.addMobileNode(nodeId);
    }
}

void
SwarmManager::sendRequest(const std::shared_ptr<ChannelSocketInterface>& socket,
                          NodeId& nodeId,
                          Query q,
                          int numberNodes)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;

    Request toRequest {q, numberNodes, nodeId};
    Message msg;
    msg.is_mobile = isMobile_;
    msg.request = std::move(toRequest);

    pk.pack(msg);

    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);

    if (ec) {
        JAMI_ERROR("{}", ec.message());
        return;
    }
}

void
SwarmManager::sendAnswer(const std::shared_ptr<ChannelSocketInterface>& socket, const Message& msg_)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (msg_.request->q == Query::FIND) {
        auto nodes = routing_table.closestNodes(msg_.request->nodeId, msg_.request->num);
        auto bucket = routing_table.findBucket(msg_.request->nodeId);
        const auto& m_nodes = bucket->getMobileNodes();
        Response toResponse {Query::FOUND, nodes, {m_nodes.begin(), m_nodes.end()}};

        Message msg;
        msg.is_mobile = isMobile_;
        msg.response = std::move(toResponse);

        msgpack::sbuffer buffer((size_t) 1024);
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(msg);

        std::error_code ec;
        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec) {
            JAMI_ERROR("{}", ec.message());
            return;
        }
    }
}

void
SwarmManager::receiveMessage(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    struct DecodingContext
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               512};
    };

    socket->setOnRecv([w = weak(),
                       wsocket = std::weak_ptr<ChannelSocketInterface>(socket),
                       ctx = std::make_shared<DecodingContext>()](const uint8_t* buf, size_t len) {
        ctx->pac.reserve_buffer(len);
        std::copy_n(buf, len, ctx->pac.buffer());
        ctx->pac.buffer_consumed(len);

        msgpack::object_handle oh;
        while (ctx->pac.next(oh)) {
            auto shared = w.lock();
            auto socket = wsocket.lock();
            if (!shared || !socket)
                return 0lu;

            try {
                Message msg;
                oh.get().convert(msg);

                if (msg.is_mobile)
                    shared->changeMobility(socket->deviceId(), msg.is_mobile);

                if (msg.request)
                    shared->sendAnswer(socket, msg);
                else if (msg.response) {
                    shared->setKnownNodes(msg.response->nodes);
                    shared->setMobileNodes(msg.response->mobile_nodes);
                }

            } catch (const std::exception& e) {
                JAMI_WARNING("Error DRT recv: {}", e.what());
                return len;
            }
        }

        return len;
    });

    socket->onShutdown([w = weak(), deviceId = socket->deviceId()] {
        auto shared = w.lock();
        if (shared && !shared->isShutdown_) {
            shared->removeNode(deviceId);
        }
    });
}

void
SwarmManager::maintainBuckets()
{
    std::set<NodeId> nodes;
    std::unique_lock<std::mutex> lock(mutex);
    auto& buckets = routing_table.getBuckets();
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        auto& bucket = *it;
        bool myBucket = routing_table.contains(it, id_);
        auto connecting_nodes = myBucket ? bucket.getConnectingNodesSize()
                                         : bucket.getConnectingNodesSize() + bucket.getNodesSize();
        if (connecting_nodes < Bucket::BUCKET_MAX_SIZE) {
            auto nodesToTry = bucket.getKnownNodesRandom(Bucket::BUCKET_MAX_SIZE - connecting_nodes,
                                                         rd);
            for (auto& node : nodesToTry)
                routing_table.addConnectingNode(node);

            nodes.insert(nodesToTry.begin(), nodesToTry.end());
        }
    }
    lock.unlock();
    for (auto& node : nodes)
        tryConnect(node);
}
bool
SwarmManager::isConnectedWith(const NodeId& deviceId)
{
    return routing_table.hasNode(deviceId);
}

void
SwarmManager::tryConnect(const NodeId& nodeId)
{
    if (needSocketCb_)
        needSocketCb_(nodeId.toString(),
                      [w = weak(), nodeId](const std::shared_ptr<ChannelSocketInterface>& socket) {
                          auto shared = w.lock();
                          if (!shared)
                              return true;
                          if (socket) {
                              shared->addChannel(socket);
                              return true;
                          }
                          std::unique_lock<std::mutex> lk(shared->mutex);
                          auto bucket = shared->routing_table.findBucket(nodeId);
                          bucket->removeConnectingNode(nodeId);
                          bucket->addKnownNode(nodeId);
                          bucket = shared->routing_table.findBucket(shared->getId());
                          if (bucket->getConnectingNodesSize() == 0
                              && bucket->getNodeIds().size() == 0 && shared->onConnectionChanged_) {
                              lk.unlock();
                              JAMI_WARNING("[SwarmManager {:p}] Bootstrap: all connections failed",
                                           fmt::ptr(shared.get()));
                              shared->onConnectionChanged_(false);
                          }
                          return true;
                      });
}

void
SwarmManager::addChannel(std::shared_ptr<ChannelSocketInterface> channel)
{
    auto emit = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        emit = routing_table.findBucket(getId())->getNodeIds().size() == 0;
        auto bucket = routing_table.findBucket(channel->deviceId());
        if (routing_table.addNode(channel, bucket)) {
            std::error_code ec;
            resetNodeExpiry(ec, channel, id_);
        }
    }
    receiveMessage(channel);
    if (emit && onConnectionChanged_) {
        // If it's the first channel we add, we're now connected!
        JAMI_DEBUG("[SwarmManager {}] Bootstrap: Connected!", fmt::ptr(this));
        onConnectionChanged_(true);
    }
}

void
SwarmManager::resetNodeExpiry(const asio::error_code& ec,
                              const std::shared_ptr<ChannelSocketInterface>& socket,
                              NodeId node)
{
    NodeId idToFind;
    std::list<Bucket>::iterator bucket;

    if (ec == asio::error::operation_aborted)
        return;

    if (!node) {
        bucket = routing_table.findBucket(socket->deviceId());
        idToFind = bucket->randomId(rd);
    } else {
        bucket = routing_table.findBucket(node);
        idToFind = node;
    }

    sendRequest(socket, idToFind, Query::FIND, Bucket::BUCKET_MAX_SIZE);

    if (!node) {
        auto& nodeTimer = bucket->getNodeTimer(socket);
        nodeTimer.expires_after(FIND_PERIOD);
        nodeTimer.async_wait(std::bind(&jami::SwarmManager::resetNodeExpiry,
                                       shared_from_this(),
                                       std::placeholders::_1,
                                       socket,
                                       NodeId {}));
    }
}

void
SwarmManager::changeMobility(const NodeId& nodeId, bool isMobile)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto bucket = routing_table.findBucket(nodeId);
    bucket->changeMobility(nodeId, isMobile);
}

void
SwarmManager::shutdown()
{
    isShutdown_ = true;
    std::lock_guard<std::mutex> lock(mutex);
    routing_table.shutdownAllNodes();
}
} // namespace jami
