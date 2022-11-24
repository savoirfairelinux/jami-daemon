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

#include "routing_table.h"

#include <math.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <stdlib.h>
#include <time.h>

#include "connectivity/multiplexed_socket.h"
#include "opendht/infohash.h"

constexpr const std::chrono::minutes FIND_PERIOD {10};
using namespace std::placeholders;

namespace jami {

using namespace dht;

Bucket::Bucket(const NodeId& id)
    : lowerLimit_(id)
{}

bool
Bucket::addNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    return addNode(NodeInfo(socket));
}
/**
 * Add Node socket to bucket
 * @param NodeInfo& nodeInfo
 */
bool
Bucket::addNode(NodeInfo&& info)
{
    auto nodeId = info.socket->deviceId();
    if (nodes.try_emplace(nodeId, std::move(info)).second) {
        connecting_nodes.erase(nodeId);
        known_nodes.erase(nodeId);
        return true;
    }
    return false;
}

bool
Bucket::deleteNode(const NodeId& nodeId)
{
    return nodes.erase(nodeId);
}

asio::steady_timer&
Bucket::getNodeTimer(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto node = nodes.find(socket->deviceId());
    if (node == nodes.end()) {
        throw std::range_error("Can't find timer " + socket->deviceId().toString());
    }
    return node->second.refresh_timer;
}

std::set<NodeId>
Bucket::getNodeIds() const
{
    std::set<NodeId> nodesId;

    for (auto const& key : nodes) {
        nodesId.insert(key.first);
    }

    return nodesId;
}

bool
Bucket::addKnownNode(const NodeId& nodeId)
{
    if (known_nodes.emplace(nodeId).second) {
        connecting_nodes.erase(nodeId);
        mobile_nodes.erase(nodeId);
        nodes.erase(nodeId);
        return true;
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
    if (mobile_nodes.emplace(nodeId).second) {
        connecting_nodes.erase(nodeId);
        known_nodes.erase(nodeId);
        nodes.erase(nodeId);
        return true;
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

void
Bucket::printBucket(unsigned number) const
{
    JAMI_ERROR("BUCKET Number: {:d}", number);

    unsigned nodeNum = 1;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        JAMI_DEBUG("Node {:s}   Id: {:s}", std::to_string(nodeNum), it->first.toString());
        nodeNum++;
    }
};

bool
Bucket::shutdownNode(const NodeId& nodeId)
{
    auto node = nodes.find(nodeId);

    if (node != nodes.end()) {
        auto socket = node->second.socket;
        auto node = socket->deviceId();
        socket->shutdown();
        deleteNode(node);
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
        deleteNode(nodeId);
    }
}

bool
Bucket::hasNode(const NodeId& nodeId) const
{
    auto found = nodes.find(nodeId);
    if (found != nodes.end()) {
        return true;
    }

    return false;
}

// ####################################################################################################

RoutingTable::RoutingTable()
{
    buckets.emplace_back(NodeId::zero());
}

bool
RoutingTable::addKnownNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);
    if (bucket == buckets.end() || id_ == nodeId) {
        return 0;
    }

    else {
        bucket->addKnownNode(nodeId);
        return 1;
    }
}
bool
RoutingTable::addMobileNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);
    if (bucket == buckets.end() || id_ == nodeId) {
        return 0;
    }

    else {
        bucket->addMobileNode(nodeId);
        return 1;
    }
}

bool
RoutingTable::deleteMobileNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);
    if (bucket == buckets.end() || id_ == nodeId) {
        return 0;
    }

    else {
        bucket->removeMobileNode(nodeId);
        return 1;
    }
}

bool
RoutingTable::contains(const std::list<Bucket>::iterator& bucket, const NodeId& nodeId) const
{
    return NodeId::cmp(bucket->getLowerLimit(), nodeId) <= 0
           && (std::next(bucket) == buckets.end()
               || NodeId::cmp(nodeId, std::next(bucket)->getLowerLimit()) < 0);
}

std::list<Bucket>::iterator
RoutingTable::findBucket(const NodeId& nodeId)
{
    if (buckets.empty())
        return buckets.end();

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

unsigned
RoutingTable::depth(std::list<Bucket>::iterator& bucket) const
{
    int bit1 = bucket->getLowerLimit().lowbit();
    int bit2 = std::next(bucket) != buckets.end() ? std::next(bucket)->getLowerLimit().lowbit()
                                                  : -1;
    return std::max(bit1, bit2) + 1;
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
            bucket->deleteNode(nodeId);
        } else {
            ++it;
        }
    }

    auto& connectingSwap = bucket->getConnectingNodes();
    for (auto it = connectingSwap.begin(); it != connectingSwap.end();) {
        auto& node = *it;
        auto nodeId = node.first;

        if (!contains(bucket, nodeId)) {
            newBucketIt->addConnectingNode(nodeId, std::move(node.second));
            bucket->removeConnectingNode(nodeId);
        } else {
            ++it;
        }
    }

    const auto& knownSwap = bucket->getKnownNodes();
    for (auto it = knownSwap.begin(); it != knownSwap.end();) {
        auto nodeId = *it;

        if (!contains(bucket, nodeId)) {
            bucket->removeKnownNode(nodeId);
            newBucketIt->addKnownNode(nodeId);
        } else {
            ++it;
        }
    }

    const auto& mobileSwap = bucket->getMobileNodes();
    for (auto it = mobileSwap.begin(); it != mobileSwap.end();) {
        auto nodeId = *it;

        if (!contains(bucket, nodeId)) {
            bucket->removeMobileNode(nodeId);
            newBucketIt->addMobileNode(nodeId);
        } else {
            ++it;
        }
    }

    return true;
}

bool
RoutingTable::addNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto bucket = findBucket(socket->deviceId());
    return bucket->addNode(socket);
}

bool
RoutingTable::addNode(const std::shared_ptr<ChannelSocketInterface>& channel,
                      std::list<Bucket>::iterator& bucket)
{
    NodeId nodeId = channel->deviceId();

    if (bucket->hasNode(nodeId) || id_ == nodeId) {
        return false;
    }

    while (bucket->isFull()) {
        if (contains(bucket, nodeId)) {
            split(bucket);
            bucket = findBucket(nodeId);
        } else
            return false;
    }

    return bucket->addNode(channel);
}

bool
RoutingTable::deleteNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);

    if (bucket == buckets.end()) {
        return 0;
    }

    return bucket->deleteNode(nodeId);
}

bool
RoutingTable::hasNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);
    return bucket->hasNode(nodeId);
};

std::vector<NodeId>
RoutingTable::closestNodes(const NodeId& nodeId, unsigned count)
{
    std::vector<NodeId> closestNodes;
    auto bucket = findBucket(nodeId);
    if (bucket == buckets.end()) {
        return closestNodes;
    }

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
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        it->printBucket(counter);
        counter++;
    }
    JAMI_DEBUG("_____________________________________________________________________________");
}

void
RoutingTable::shutdownNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);

    if (bucket != buckets.end()) {
        bucket->shutdownNode(nodeId);
    }
}

} // namespace jami