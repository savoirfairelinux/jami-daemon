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

#include "routing_table.h"

#include <dhtnet/multiplexed_socket.h>
#include <opendht/infohash.h>

#include <math.h>
#include <stdio.h>
#include <iostream>
#include <iterator>
#include <stdlib.h>
#include <time.h>

constexpr const std::chrono::minutes FIND_PERIOD {10};
using namespace std::placeholders;

namespace jami {

using namespace dht;

Bucket::Bucket(const NodeId& id)
    : lowerLimit_(id)
{}

bool
Bucket::addNode(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    return addNode(NodeInfo(socket));
}

bool
Bucket::addNode(NodeInfo&& info)
{
    auto nodeId = info.socket->deviceId();
    if (nodes.try_emplace(nodeId, std::move(info)).second) {
        connecting_nodes.erase(nodeId);
        known_nodes.erase(nodeId);
        mobile_nodes.erase(nodeId);
        return true;
    }
    return false;
}

bool
Bucket::removeNode(const NodeId& nodeId)
{
    auto node = nodes.find(nodeId);
    auto isMobile = node->second.isMobile_;
    if (node == nodes.end())
        return false;
    nodes.erase(nodeId);
    if (isMobile) {
        addMobileNode(nodeId);
    } else {
        addKnownNode(nodeId);
    }

    return true;
}

std::set<NodeId>
Bucket::getNodeIds() const
{
    std::set<NodeId> nodesId;
    for (auto const& key : nodes)
        nodesId.insert(key.first);
    return nodesId;
}

bool
Bucket::hasNode(const NodeId& nodeId) const
{
    return nodes.find(nodeId) != nodes.end();
}

bool
Bucket::addKnownNode(const NodeId& nodeId)
{
    if (!hasNode(nodeId)) {
        if (known_nodes.emplace(nodeId).second) {
            return true;
        }
    }
    return false;
}

NodeId
Bucket::getKnownNode(unsigned index) const
{
    if (index > known_nodes.size()) {
        throw std::out_of_range("End of table for get known Node Id " + std::to_string(index));
    }
    auto it = known_nodes.begin();
    std::advance(it, index);

    return *it;
}

bool
Bucket::addMobileNode(const NodeId& nodeId)
{
    if (!hasNode(nodeId)) {
        if (mobile_nodes.emplace(nodeId).second) {
            known_nodes.erase(nodeId);
            return true;
        }
    }
    return false;
}

bool
Bucket::addConnectingNode(const NodeId& nodeId)
{
    if (!hasNode(nodeId)) {
        if (connecting_nodes.emplace(nodeId).second) {
            known_nodes.erase(nodeId);
            mobile_nodes.erase(nodeId);
            return true;
        }
    }
    return false;
}

std::set<NodeId>
Bucket::getKnownNodesRandom(unsigned numberNodes, std::mt19937_64& rd) const
{
    std::set<NodeId> nodesToReturn;

    if (getKnownNodesSize() <= numberNodes)
        return getKnownNodes();

    std::uniform_int_distribution<unsigned> distrib(0, getKnownNodesSize() - 1);

    while (nodesToReturn.size() < numberNodes) {
        nodesToReturn.emplace(getKnownNode(distrib(rd)));
    }

    return nodesToReturn;
}

asio::steady_timer&
Bucket::getNodeTimer(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    auto node = nodes.find(socket->deviceId());
    if (node == nodes.end()) {
        throw std::range_error("Unable to find timer " + socket->deviceId().toString());
    }
    return node->second.refresh_timer;
}

bool
Bucket::shutdownNode(const NodeId& nodeId)
{
    auto node = nodes.find(nodeId);

    if (node != nodes.end()) {
        auto socket = node->second.socket;
        auto node = socket->deviceId();
        socket->shutdown();
        removeNode(node);
        return true;
    }
    return false;
}

void
Bucket::shutdownAllNodes()
{
    while (not nodes.empty()) {
        auto it = nodes.begin();
        auto socket = it->second.socket;
        auto nodeId = socket->deviceId();
        socket->shutdown();
        removeNode(nodeId);
    }
}

void
Bucket::printBucket(unsigned number) const
{
    JAMI_ERROR("BUCKET Number: {:d}", number);

    unsigned nodeNum = 1;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        JAMI_DEBUG("Node {:s}   Id: {:s}  isMobile: {:s}", std::to_string(nodeNum), it->first.toString(), std::to_string(it->second.isMobile_));
        nodeNum++;
    }
    JAMI_ERROR("Mobile Nodes");
    nodeNum = 0;
    for (auto it = mobile_nodes.begin(); it != mobile_nodes.end(); ++it) {
        JAMI_DEBUG("Node {:s}   Id: {:s}", std::to_string(nodeNum), (*it).toString());
        nodeNum++;
    }

    JAMI_ERROR("Known Nodes");
    nodeNum = 0;
    for (auto it = known_nodes.begin(); it != known_nodes.end(); ++it) {
        JAMI_DEBUG("Node {:s}   Id: {:s}", std::to_string(nodeNum), (*it).toString());
        nodeNum++;
    }
    JAMI_ERROR("Connecting_nodes");
    nodeNum = 0;
    for (auto it = connecting_nodes.begin(); it != connecting_nodes.end(); ++it) {
        JAMI_DEBUG("Node {:s}   Id: {:s}", std::to_string(nodeNum), (*it).toString());
        nodeNum++;
    }
};

void
Bucket::changeMobility(const NodeId& nodeId, bool isMobile)
{
    auto itn = nodes.find(nodeId);
    if (itn != nodes.end()) {
        itn->second.isMobile_ = isMobile;
    }
}

// For tests

std::set<std::shared_ptr<dhtnet::ChannelSocketInterface>>
Bucket::getNodeSockets() const
{
    std::set<std::shared_ptr<dhtnet::ChannelSocketInterface>> sockets;
    for (auto const& info : nodes)
        sockets.insert(info.second.socket);
    return sockets;
}

// ####################################################################################################

RoutingTable::RoutingTable()
{
    buckets.emplace_back(NodeId::zero());
}

bool
RoutingTable::addNode(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    auto bucket = findBucket(socket->deviceId());
    return addNode(socket, bucket);
}

bool
RoutingTable::addNode(const std::shared_ptr<dhtnet::ChannelSocketInterface>& channel,
                      std::list<Bucket>::iterator& bucket)
{
    NodeId nodeId = channel->deviceId();

    if (bucket->hasNode(nodeId) || id_ == nodeId) {
        return false;
    }

    while (bucket->isFull()) {
        if (contains(bucket, id_)) {
            split(bucket);
            bucket = findBucket(nodeId);

        } else {
            return bucket->addNode(std::move(channel));
        }
    }
    return bucket->addNode(std::move(channel));
}

bool
RoutingTable::removeNode(const NodeId& nodeId)
{
    return findBucket(nodeId)->removeNode(nodeId);
}

bool
RoutingTable::hasNode(const NodeId& nodeId)
{
    return findBucket(nodeId)->hasNode(nodeId);
}

bool
RoutingTable::addKnownNode(const NodeId& nodeId)
{
    if (id_ == nodeId)
        return false;

    auto bucket = findBucket(nodeId);
    if (bucket == buckets.end())
        return false;

    return bucket->addKnownNode(nodeId);
}

bool
RoutingTable::addMobileNode(const NodeId& nodeId)
{
    if (id_ == nodeId)
        return false;

    auto bucket = findBucket(nodeId);

    if (bucket == buckets.end())
        return 0;

    bucket->addMobileNode(nodeId);
    return 1;
}

void
RoutingTable::removeMobileNode(const NodeId& nodeId)
{
    return findBucket(nodeId)->removeMobileNode(nodeId);
}

bool
RoutingTable::hasMobileNode(const NodeId& nodeId)
{
    return findBucket(nodeId)->hasMobileNode(nodeId);
};

bool
RoutingTable::addConnectingNode(const NodeId& nodeId)
{
    if (id_ == nodeId)
        return false;

    auto bucket = findBucket(nodeId);

    if (bucket == buckets.end())
        return 0;

    bucket->addConnectingNode(nodeId);
    return 1;
}

void
RoutingTable::removeConnectingNode(const NodeId& nodeId)
{
    findBucket(nodeId)->removeConnectingNode(nodeId);
}

std::list<Bucket>::iterator
RoutingTable::findBucket(const NodeId& nodeId)
{
    if (buckets.empty())
        throw std::runtime_error("No bucket");

    auto b = buckets.begin();

    while (true) {
        auto next = std::next(b);
        if (next == buckets.end())
            return b;
        if (std::memcmp(nodeId.data(), next->getLowerLimit().data(), nodeId.size()) < 0)
            return b;
        b = next;
    }
}

std::vector<NodeId>
RoutingTable::closestNodes(const NodeId& nodeId, unsigned count)
{
    std::vector<NodeId> closestNodes;
    auto bucket = findBucket(nodeId);
    auto sortedBucketInsert = [&](const std::list<Bucket>::iterator& b) {
        auto nodes = b->getNodeIds();
        for (auto n : nodes) {
            if (n != nodeId) {
                auto here = std::find_if(closestNodes.begin(),
                                         closestNodes.end(),
                                         [&nodeId, &n](NodeId& NodeId) {
                                             return nodeId.xorCmp(n, NodeId) < 0;
                                         });

                closestNodes.insert(here, n);
            }
        }
    };

    auto itn = bucket;
    auto itp = (bucket == buckets.begin()) ? buckets.end() : std::prev(bucket);
    while (itn != buckets.end() || itp != buckets.end()) {
        if (itn != buckets.end()) {
            sortedBucketInsert(itn);
            itn = std::next(itn);
        }
        if (itp != buckets.end()) {
            sortedBucketInsert(itp);
            itp = (itp == buckets.begin()) ? buckets.end() : std::prev(itp);
        }
    }

    if (closestNodes.size() > count) {
        closestNodes.resize(count);
    }

    return closestNodes;
}

void
RoutingTable::printRoutingTable() const
{
    int counter = 1;
    JAMI_DEBUG("SWARM: {:s} ", id_.toString());
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        it->printBucket(counter);
        counter++;
    }
    JAMI_DEBUG("_____________________________________________________________________________");
}

void
RoutingTable::shutdownNode(const NodeId& nodeId)
{
    findBucket(nodeId)->shutdownNode(nodeId);
}

std::vector<NodeId>
RoutingTable::getNodes() const
{
    std::lock_guard lock(mutex_);
    std::vector<NodeId> ret;
    for (const auto& b : buckets) {
        const auto& nodes = b.getNodeIds();
        ret.insert(ret.end(), nodes.begin(), nodes.end());
    }
    return ret;
}

std::vector<NodeId>
RoutingTable::getKnownNodes() const
{
    std::vector<NodeId> ret;
    for (const auto& b : buckets) {
        const auto& nodes = b.getKnownNodes();
        ret.insert(ret.end(), nodes.begin(), nodes.end());
    }
    return ret;
}

std::vector<NodeId>
RoutingTable::getMobileNodes() const
{
    std::vector<NodeId> ret;
    for (const auto& b : buckets) {
        const auto& nodes = b.getMobileNodes();
        ret.insert(ret.end(), nodes.begin(), nodes.end());
    }
    return ret;
}

std::vector<NodeId>
RoutingTable::getConnectingNodes() const
{
    std::vector<NodeId> ret;
    for (const auto& b : buckets) {
        const auto& nodes = b.getConnectingNodes();
        ret.insert(ret.end(), nodes.begin(), nodes.end());
    }
    return ret;
}

std::vector<NodeId>
RoutingTable::getBucketMobileNodes() const
{
    std::vector<NodeId> ret;
    auto bucket = findBucket(id_);
    const auto& nodes = bucket->getMobileNodes();
    ret.insert(ret.end(), nodes.begin(), nodes.end());

    return ret;
}

bool
RoutingTable::contains(const std::list<Bucket>::iterator& bucket, const NodeId& nodeId) const
{
    return NodeId::cmp(bucket->getLowerLimit(), nodeId) <= 0
           && (std::next(bucket) == buckets.end()
               || NodeId::cmp(nodeId, std::next(bucket)->getLowerLimit()) < 0);
}

std::vector<NodeId>
RoutingTable::getAllNodes() const
{
    std::vector<NodeId> ret;
    for (const auto& b : buckets) {
        const auto& nodes = b.getNodeIds();
        const auto& knownNodes = b.getKnownNodes();
        const auto& mobileNodes = b.getMobileNodes();
        const auto& connectingNodes = b.getConnectingNodes();
        ret.reserve(nodes.size() + knownNodes.size() + mobileNodes.size() + connectingNodes.size());
        ret.insert(ret.end(), nodes.begin(), nodes.end());
        ret.insert(ret.end(), knownNodes.begin(), knownNodes.end());
        ret.insert(ret.end(), mobileNodes.begin(), mobileNodes.end());
        ret.insert(ret.end(), connectingNodes.begin(), connectingNodes.end());
    }
    return ret;
}

void
RoutingTable::deleteNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);
    bucket->shutdownNode(nodeId);
    bucket->removeConnectingNode(nodeId);
    bucket->removeKnownNode(nodeId);
    bucket->removeMobileNode(nodeId);
}

NodeId
RoutingTable::middle(std::list<Bucket>::iterator& it) const
{
    unsigned bit = depth(it);
    if (bit >= 8 * HASH_LEN)
        throw std::out_of_range("End of table");

    NodeId id = it->getLowerLimit();
    id.setBit(bit, true);
    return id;
}

unsigned
RoutingTable::depth(std::list<Bucket>::iterator& bucket) const
{
    int bit1 = bucket->getLowerLimit().lowbit();
    int bit2 = std::next(bucket) != buckets.end() ? std::next(bucket)->getLowerLimit().lowbit()
                                                  : -1;
    return std::max(bit1, bit2) + 1;
}

bool
RoutingTable::split(std::list<Bucket>::iterator& bucket)
{
    NodeId id = middle(bucket);
    auto newBucketIt = buckets.emplace(std::next(bucket), id);
    // Re-assign nodes
    auto& nodeSwap = bucket->getNodes();

    for (auto it = nodeSwap.begin(); it != nodeSwap.end();) {
        auto& node = *it;

        auto nodeId = it->first;

        if (!contains(bucket, nodeId)) {
            newBucketIt->addNode(std::move(node.second));
            it = nodeSwap.erase(it);
        } else {
            ++it;
        }
    }

    auto connectingSwap = bucket->getConnectingNodes();
    for (auto it = connectingSwap.begin(); it != connectingSwap.end();) {
        auto nodeId = *it;

        if (!contains(bucket, nodeId)) {
            newBucketIt->addConnectingNode(nodeId);
            it = connectingSwap.erase(it);
            bucket->removeConnectingNode(nodeId);
        } else {
            ++it;
        }
    }

    auto knownSwap = bucket->getKnownNodes();
    for (auto it = knownSwap.begin(); it != knownSwap.end();) {
        auto nodeId = *it;

        if (!contains(bucket, nodeId)) {
            newBucketIt->addKnownNode(nodeId);
            it = knownSwap.erase(it);
            bucket->removeKnownNode(nodeId);
        } else {
            ++it;
        }
    }

    auto mobileSwap = bucket->getMobileNodes();
    for (auto it = mobileSwap.begin(); it != mobileSwap.end();) {
        auto nodeId = *it;

        if (!contains(bucket, nodeId)) {
            newBucketIt->addMobileNode(nodeId);
            it = mobileSwap.erase(it);
            bucket->removeMobileNode(nodeId);
        } else {
            ++it;
        }
    }

    return true;
}

} // namespace jami