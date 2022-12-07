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

#include <iostream>

#include "swarm_manager.h"
#include "connectivity/multiplexed_socket.h"

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
    std::lock_guard<std::mutex> lock(mutex);
    removeNodeInternal(nodeId);
}

void
SwarmManager::removeNodeInternal(const NodeId& nodeId)
{
    auto bucket = routing_table.findBucket(nodeId);
    bucket->removeNode(nodeId);
}

void
SwarmManager::setKnownNodes(const std::vector<NodeId>& known_nodes)
{
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto& NodeId : known_nodes) {
        addKnownNodes(std::move(NodeId));
    }
    JAMI_ERROR("MB FROM SKN RCV {}", id_.toString());

    maintainBuckets();
}

void
SwarmManager::setMobileNodes(const std::vector<NodeId>& mobile_nodes)
{
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto& nodeId : mobile_nodes) {
        addMobileNodes(nodeId);
    }
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
    msg.is_mobile = isPersistent;
    msg.request = std::move(toRequest);

    pk.pack(msg);

    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);

    if (ec) {
        JAMI_ERR("%s", ec.message().c_str());
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
        msg.is_mobile = isPersistent;
        msg.response = std::move(toResponse);

        msgpack::sbuffer buffer((size_t) 1024);
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(msg);

        std::error_code ec;
        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec) {
            JAMI_ERR("%s", ec.message().c_str());
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
            if (!shared || !socket) {
                JAMI_ERROR("Swarm Manager was destroyed :/");
                return (size_t) 0;
            }

            try {
                Message msg;
                oh.get().convert(msg);
                if (!msg.is_mobile) {
                    shared->changePersistency(socket->deviceId(), msg.is_mobile);
                }

                if (msg.request) {
                    shared->sendAnswer(socket, msg);
                }
                if (msg.response) {
                    shared->setKnownNodes(msg.response->nodes);
                    shared->setMobileNodes(msg.response->mobile_nodes);
                }

            } catch (const std::exception& e) {
                JAMI_WARN("Error DRT recv: %s", e.what());
                return len;
            }
        }

        return len;
    });

    socket->onShutdown([w = weak(), wsocket = std::weak_ptr<ChannelSocketInterface>(socket)] {
        auto shared = w.lock();
        auto socket = wsocket.lock();
        if (shared and socket) {
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->removeNodeInternal(socket->deviceId());
            shared->maintainBuckets();
            JAMI_ERROR("MB FROM SD RCV {} FROM {}",
                       shared->id_.toString(),
                       socket->deviceId().toString());
        }
    });
}

void
SwarmManager::maintainBuckets()
{
    auto& buckets = routing_table.getBuckets();
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        auto& bucket = *it;
        bool myBucket = routing_table.contains(it, id_);
        auto connecting_nodes = myBucket ? bucket.getConnectingNodesSize()
                                         : bucket.getConnectingNodesSize() + bucket.getNodesSize();
        if (connecting_nodes < Bucket::BUCKET_MAX_SIZE) {
            auto nodesToTry = bucket.getKnownNodesRandom(Bucket::BUCKET_MAX_SIZE - connecting_nodes,
                                                         rd);

            for (auto& node : nodesToTry) {
                tryConnect(node);
            }
        }
    }
}

void
SwarmManager::tryConnect(const NodeId& nodeId)
{
    routing_table.addConnectingNode(nodeId);
    JAMI_ERROR(" {} TRY CONNECT TO {}", id_.toString(), nodeId.toString());
    if (needSocketCb_)
        needSocketCb_(
            nodeId.toString(),
            [this, nodeId](const std::shared_ptr<ChannelSocketInterface>& socket) {
                auto bucket = routing_table.findBucket(getId());
                std::unique_lock<std::mutex> lock(mutex);
                if (socket) {
                    JAMI_ERROR(" {} CONNECTED TO {}",
                               this->getId().toString(),
                               socket->deviceId().toString());

                    // TODO homogeneize with addChannel?
                    auto emit = bucket->getNodeIds().size() == 0;
                    if (routing_table.addNode(socket)) {
                        // JAMI_ERROR(" {} CONNECTED TO {}", this->getId().toString(),
                        //  socket->deviceId().toString());
                        std::error_code ec;
                        resetNodeExpiry(ec, socket, id_);
                        lock.unlock();
                        receiveMessage(socket);
                        if (emit && onConnectionChanged_) {
                            JAMI_DEBUG("[SwarmManager {}] Bootstrap: Connected!", fmt::ptr(this));
                            onConnectionChanged_(true);
                        }
                    }
                } else {
                    routing_table.addKnownNode(nodeId); // TODO move?

                    if (bucket->getConnectingNodesSize() == 0 && bucket->getNodeIds().size() == 0
                        && onConnectionChanged_) {
                        JAMI_WARNING("[SwarmManager {}] Bootstrap: all connections failed",
                                     fmt::ptr(this));
                        onConnectionChanged_(false);
                    }
                    // TODO => Infite loop here: maintainBuckets();
                    //  maintainBuckets();->tryConnect
                    //  (failure)->maintainBuckets()->tryConnect ->etc
                }
                return true;
            });
}

void
SwarmManager::addChannel(std::shared_ptr<ChannelSocketInterface> channel)
{
    {
        // JAMI_DEBUG("{} ADDED CHANNEL {}", this->id_.toString(), channel->deviceId().toString());

        std::lock_guard<std::mutex> lock(mutex);
        auto bucket = routing_table.findBucket(channel->deviceId());
        routing_table.addNode(channel, bucket);
    }
    receiveMessage(channel);
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
                                       this,
                                       std::placeholders::_1,
                                       socket,
                                       NodeId {}));
    }
}

void
SwarmManager::changePersistency(const NodeId& nodeId, bool isPersistent)
{
    auto bucket = routing_table.findBucket(nodeId);
    bucket->changePersistency(nodeId, isPersistent);
}

} // namespace jami
