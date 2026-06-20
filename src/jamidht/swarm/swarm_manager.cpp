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

#include "swarm_manager.h"
#include <dhtnet/multiplexed_socket.h>
#include <dhtnet/channel_utils.h>
#include <opendht/thread_pool.h>

#include <algorithm>

namespace jami {

using namespace swarm_protocol;

// Reconnection backoff for swarm nodes whose link keeps dropping (typically
// peers behind a restrictive NAT relying on relayed paths that tear down every
// few seconds). Instead of reconnecting immediately and indefinitely, attempts
// are spaced out exponentially. Past RECONNECT_ONDEMAND_THRESHOLD consecutive
// failures the node stops being eagerly maintained in the mesh and is only
// retried slowly (it is still reconnected immediately on demand, e.g. when a
// message must be sent, through the regular channel handlers).
static constexpr std::chrono::seconds RECONNECT_BASE_DELAY {5};
static constexpr std::chrono::seconds RECONNECT_MAX_DELAY {300};
static constexpr unsigned RECONNECT_ONDEMAND_THRESHOLD {6};
// A connection must stay up at least this long to be considered stable; a
// shorter-lived link counts as a failure for backoff escalation purposes.
static constexpr std::chrono::seconds RECONNECT_STABLE_MIN {60};

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

    dht::ThreadPool::io().run([w = weak(), newNodes = std::move(newNodes)] {
        auto shared = w.lock();
        if (!shared)
            return;
        // If we detect a new node which already got a TCP link
        // we can use it to speed-up the bootstrap (because opening
        // a new channel will be easy)
        std::set<NodeId> toConnect;
        for (const auto& nodeId : newNodes) {
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
            // Record that we are connected and cancel any pending reconnection
            // backoff. The failure counter is only reset once the connection
            // proves stable (see removeNode), so a peer that connects then drops
            // within seconds keeps escalating toward on-demand mode.
            markConnected(channel->deviceId());
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
        // If the connection stayed up long enough, treat this as a clean drop
        // and allow a quick reconnection; otherwise keep escalating the backoff
        // so an unstable peer that drops every few seconds is not hammered.
        auto it = reconnectInfos_.find(nodeId);
        bool wasStable = it != reconnectInfos_.end()
                         && it->second.connectedAt != std::chrono::steady_clock::time_point {}
                         && std::chrono::steady_clock::now() - it->second.connectedAt >= RECONNECT_STABLE_MIN;
        lk.unlock();
        scheduleReconnect(nodeId, not wasStable);
        // Other buckets are still maintained.
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
    for (auto& [id, info] : reconnectInfos_) {
        if (info.timer)
            info.timer->cancel();
    }
    reconnectInfos_.clear();
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
    if (isShutdown_)
        return;
    auto& buckets = routing_table.getBuckets();
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        auto& bucket = *it;
        bool myBucket = routing_table.contains(it, id_);
        auto connecting_nodes = myBucket ? bucket.getConnectingNodesSize()
                                         : bucket.getConnectingNodesSize() + bucket.getNodesSize();
        if (connecting_nodes < Bucket::BUCKET_MAX_SIZE) {
            auto nodesToTry = bucket.getKnownNodesRandom(Bucket::BUCKET_MAX_SIZE - connecting_nodes, rd);
            for (auto& node : nodesToTry) {
                // Skip nodes that are in reconnection backoff or on-demand mode:
                // they must not be eagerly reconnected here.
                if (shouldDeferReconnect(node))
                    continue;
                routing_table.addConnectingNode(node);
                nodes.insert(node);
            }
        }
    }
    lock.unlock();
    for (const auto& node : nodes)
        tryConnect(node);
}

void
SwarmManager::sendRequest(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                          const NodeId& nodeId,
                          Query q,
                          int numberNodes)
{
    dht::ThreadPool::io().run([socket, isMobile = isMobile_, nodeId, q, numberNodes] {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        Message msg;
        msg.is_mobile = isMobile;
        msg.request = Request {q, numberNodes, nodeId};
        pk.pack(msg);

        std::error_code ec;
        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec) {
            JAMI_ERROR("{}", ec.message());
        }
    });
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
}

void
SwarmManager::receiveMessage(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    socket->setOnRecv(dhtnet::buildMsgpackReader<Message>(
        [w = weak(), wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket)](Message&& msg) {
            auto shared = w.lock();
            auto socket = wsocket.lock();
            if (!shared || !socket)
                return std::make_error_code(std::errc::operation_canceled);

            if (msg.is_mobile)
                shared->changeMobility(socket->deviceId(), msg.is_mobile);

            if (msg.request) {
                shared->sendAnswer(socket, msg);

            } else if (msg.response) {
                shared->setKnownNodes(msg.response->nodes);
                shared->setMobileNodes(msg.response->mobile_nodes);
            }
            return std::error_code();
        }));

    socket->onShutdown([w = weak(), deviceId = socket->deviceId()](const std::error_code&) {
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
SwarmManager::tryConnect(const NodeId& nodeId, bool noNewSocket)
{
    if (needSocketCb_)
        needSocketCb_(
            nodeId.toString(),
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
                auto myBucket = shared->routing_table.findBucket(shared->getId());
                bool allFailed = myBucket->getConnectingNodesSize() == 0 && myBucket->isEmpty();
                lk.unlock();
                // The attempt failed: back off before trying this node again.
                shared->scheduleReconnect(nodeId, true);
                if (allFailed && shared->onConnectionChanged_) {
                    JAMI_LOG("[SwarmManager {:p}] Bootstrap: all connections failed", fmt::ptr(shared.get()));
                    shared->onConnectionChanged_(false);
                }
                return true;
            },
            noNewSocket);
}

void
SwarmManager::removeNodeInternal(const NodeId& nodeId)
{
    routing_table.removeNode(nodeId);
}

void
SwarmManager::scheduleReconnect(const NodeId& nodeId, bool countFailure)
{
    std::lock_guard lock(mutex);
    if (isShutdown_)
        return;
    auto& info = reconnectInfos_[nodeId];
    std::chrono::steady_clock::duration delay;
    if (countFailure) {
        info.failures++;
        // Exponential backoff capped at RECONNECT_MAX_DELAY (cap the exponent to
        // avoid shifting past the width of the operand).
        unsigned shift = std::min(info.failures - 1, 16u);
        delay = std::min(RECONNECT_BASE_DELAY * (1u << shift), RECONNECT_MAX_DELAY);
        if (info.failures > RECONNECT_ONDEMAND_THRESHOLD) {
            // Stop eagerly maintaining this peer in the mesh: it is excluded from
            // proactive bucket filling and only retried at the (slow) max interval.
            // Real traffic still reconnects it immediately via the channel handlers.
            info.onDemand = true;
            delay = RECONNECT_MAX_DELAY;
        }
    } else {
        // Clean drop after a stable connection: reset the backoff and reconnect
        // without delay.
        info.failures = 0;
        info.onDemand = false;
        delay = std::chrono::seconds {0};
    }
    info.nextRetry = std::chrono::steady_clock::now() + delay;
    if (!info.timer)
        info.timer = std::make_shared<asio::steady_timer>(*Manager::instance().ioContext());
    info.timer->expires_after(delay);
    info.timer->async_wait([w = weak(), nodeId](const asio::error_code& ec) {
        if (ec)
            return;
        if (auto shared = w.lock())
            shared->maintainBuckets({nodeId});
    });
}

bool
SwarmManager::shouldDeferReconnect(const NodeId& nodeId) const
{
    auto it = reconnectInfos_.find(nodeId);
    if (it == reconnectInfos_.end())
        return false;
    return it->second.onDemand || it->second.nextRetry > std::chrono::steady_clock::now();
}

void
SwarmManager::markConnected(const NodeId& nodeId)
{
    auto& info = reconnectInfos_[nodeId];
    info.connectedAt = std::chrono::steady_clock::now();
    info.nextRetry = {};
    if (info.timer)
        info.timer->cancel();
}

void
SwarmManager::connectNode(const NodeId& nodeId)
{
    {
        std::lock_guard lock(mutex);
        if (isShutdown_)
            return;
        if (isConnectedWith(nodeId))
            return;
        addKnownNode(nodeId);
        if (!routing_table.addConnectingNode(nodeId))
            return;
    }
    tryConnect(nodeId, true);
}

std::vector<NodeId>
SwarmManager::getAllNodes() const
{
    std::lock_guard lock(mutex);
    return routing_table.getAllNodes();
}

std::vector<NodeId>
SwarmManager::getConnectedNodes() const
{
    std::lock_guard lock(mutex);
    return routing_table.getConnectedNodes();
}

std::vector<std::map<std::string, std::string>>
SwarmManager::getRoutingTableInfo() const
{
    std::lock_guard lock(mutex);
    auto stats = routing_table.getRoutingTableStats();
    std::vector<std::map<std::string, std::string>> result;
    result.reserve(stats.size());
    for (const auto& stat : stats) {
        result.push_back({{"id", stat.id},
                          {"device", stat.id},
                          {"status", stat.status},
                          {"remoteAddress", stat.remoteAddress},
                          {"mobile", stat.isMobile ? "true" : "false"}});
        if (stat.connectionTime != std::chrono::system_clock::time_point::min()) {
            auto tt = std::chrono::system_clock::to_time_t(stat.connectionTime);
            result.back().emplace("connectionTime", std::to_string(tt));
        }
    }
    return result;
}

bool
SwarmManager::isConnected() const
{
    std::lock_guard lock(mutex);
    return !routing_table.isEmpty();
}

void
SwarmManager::deleteNode(const std::vector<NodeId>& nodes)
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
