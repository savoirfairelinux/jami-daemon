/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include "swarm_manager.h"
#include <dhtnet/multiplexed_socket.h>
#include <opendht/thread_pool.h>

constexpr const std::chrono::minutes FIND_PERIOD {10};

namespace jami {

using namespace swarm_protocol;

SwarmManager::SwarmManager(const NodeId& id, const std::mt19937_64& rand, ToConnectCb&& toConnectCb)
    : id_(id)
    , rd(rand)
    , toConnectCb_(toConnectCb)
{
    routing_table.setId(id);
}

SwarmManager::~SwarmManager()
{
    if (!isShutdown_)
        shutdown();
}

bool
SwarmManager::setKnownNodes(const std::vector<NodeId>& known_nodes)
{
    isShutdown_ = false;
    std::vector<NodeId> newNodes;
    {
        std::lock_guard lock(mutex);
        for (const auto& nodeId : known_nodes) {
            if (addKnownNode(nodeId)) {
                newNodes.emplace_back(nodeId);
            }
        }
    }

    if (newNodes.empty())
        return false;

    dht::ThreadPool::io().run([w=weak(), newNodes=std::move(newNodes)] {
        auto shared = w.lock();
        if (!shared)
            return;
        // If we detect a new node which already got a TCP link
        // we can use it to speed-up the bootstrap (because opening
        // a new channel will be easy)
        std::set<NodeId> toConnect;
        for (const auto& nodeId: newNodes) {
            if (shared->toConnectCb_ && shared->toConnectCb_(nodeId))
                toConnect.emplace(nodeId);
        }
        shared->maintainBuckets(toConnect);
    });
    return true;
}

void
SwarmManager::setMobileNodes(const std::vector<NodeId>& mobile_nodes)
{
    {
        std::lock_guard lock(mutex);
        for (const auto& nodeId : mobile_nodes)
            addMobileNodes(nodeId);
    }
}

void
SwarmManager::addChannel(const std::shared_ptr<dhtnet::ChannelSocketInterface>& channel)
{
    // JAMI_WARNING("[SwarmManager {}] addChannel! with {}", fmt::ptr(this), channel->deviceId().to_view());
    if (channel) {
        auto emit = false;
        {
            std::lock_guard lock(mutex);
            emit = routing_table.findBucket(getId())->isEmpty();
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
}

void
SwarmManager::removeNode(const NodeId& nodeId)
{
    std::unique_lock lk(mutex);
    if (isConnectedWith(nodeId)) {
        removeNodeInternal(nodeId);
        lk.unlock();
        maintainBuckets();
    }
}

void
SwarmManager::changeMobility(const NodeId& nodeId, bool isMobile)
{
    std::lock_guard lock(mutex);
    auto bucket = routing_table.findBucket(nodeId);
    bucket->changeMobility(nodeId, isMobile);
}

bool
SwarmManager::isConnectedWith(const NodeId& deviceId)
{
    return routing_table.hasNode(deviceId);
}

void
SwarmManager::shutdown()
{
    if (isShutdown_) {
        return;
    }
    isShutdown_ = true;
    std::lock_guard lock(mutex);
    routing_table.shutdownAllNodes();
}

void
SwarmManager::restart()
{
    isShutdown_ = false;
}

bool
SwarmManager::addKnownNode(const NodeId& nodeId)
{
    return routing_table.addKnownNode(nodeId);
}

void
SwarmManager::addMobileNodes(const NodeId& nodeId)
{
    if (id_ != nodeId) {
        routing_table.addMobileNode(nodeId);
    }
}

void
SwarmManager::maintainBuckets(const std::set<NodeId>& toConnect)
{
    std::set<NodeId> nodes = toConnect;
    std::unique_lock lock(mutex);
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

void
SwarmManager::sendRequest(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
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
SwarmManager::sendAnswer(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, const Message& msg_)
{
    std::lock_guard lock(mutex);

    if (msg_.request->q == Query::FIND) {
        auto nodes = routing_table.closestNodes(msg_.request->nodeId, msg_.request->num);
        auto bucket = routing_table.findBucket(msg_.request->nodeId);
        const auto& m_nodes = bucket->getMobileNodes();
        Response toResponse {Query::FOUND, nodes, {m_nodes.begin(), m_nodes.end()}};

        Message msg;
        msg.is_mobile = isMobile_;
        msg.response = std::move(toResponse);

        msgpack::sbuffer buffer((size_t) 60000);
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(msg);

        std::error_code ec;

        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec) {
            JAMI_ERROR("{}", ec.message());
            return;
        }
    }

    else {
    }
}

void
SwarmManager::receiveMessage(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    struct DecodingContext
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               512};
    };

    socket->setOnRecv([w = weak(),
                       wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket),
                       ctx = std::make_shared<DecodingContext>()](const uint8_t* buf, size_t len) {
        ctx->pac.reserve_buffer(len);
        std::copy_n(buf, len, ctx->pac.buffer());
        ctx->pac.buffer_consumed(len);

        msgpack::object_handle oh;
        while (ctx->pac.next(oh)) {
            auto shared = w.lock();
            auto socket = wsocket.lock();
            if (!shared || !socket)
                return size_t {0};

            try {
                Message msg;
                oh.get().convert(msg);

                if (msg.is_mobile)
                    shared->changeMobility(socket->deviceId(), msg.is_mobile);

                if (msg.request) {
                    shared->sendAnswer(socket, msg);

                } else if (msg.response) {
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
        dht::ThreadPool::io().run([w, deviceId] {
            auto shared = w.lock();
            if (shared && !shared->isShutdown_) {
                shared->removeNode(deviceId);
            }
        });
    });
}

void
SwarmManager::resetNodeExpiry(const asio::error_code& ec,
                              const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
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
SwarmManager::tryConnect(const NodeId& nodeId)
{
    if (needSocketCb_)
        needSocketCb_(nodeId.toString(),
                      [w = weak(), nodeId](const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket) {
                          auto shared = w.lock();
                          if (!shared || shared->isShutdown_)
                              return true;
                          if (socket) {
                              shared->addChannel(socket);
                              return true;
                          }
                          std::unique_lock lk(shared->mutex);
                          auto bucket = shared->routing_table.findBucket(nodeId);
                          bucket->removeConnectingNode(nodeId);
                          bucket->addKnownNode(nodeId);
                          bucket = shared->routing_table.findBucket(shared->getId());
                          if (bucket->getConnectingNodesSize() == 0
                              && bucket->isEmpty() && shared->onConnectionChanged_) {
                              lk.unlock();
                              JAMI_WARNING("[SwarmManager {:p}] Bootstrap: all connections failed",
                                           fmt::ptr(shared.get()));
                              shared->onConnectionChanged_(false);
                          }
                          return true;
                      });
}

void
SwarmManager::removeNodeInternal(const NodeId& nodeId)
{
    routing_table.removeNode(nodeId);
}

std::vector<NodeId>
SwarmManager::getAllNodes() const
{
    std::lock_guard lock(mutex);
    return routing_table.getAllNodes();
}

void
SwarmManager::deleteNode(std::vector<NodeId> nodes)
{
    {
        std::lock_guard lock(mutex);
        for (const auto& node : nodes) {
            routing_table.deleteNode(node);
        }
    }
    maintainBuckets();
}

} // namespace jami
