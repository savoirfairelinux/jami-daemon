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
    if (nodes.try_emplace(socket, *Manager::instance().ioContext(), FIND_PERIOD).second) {
        connecting_nodes.erase(socket->deviceId());
        known_nodes.erase(socket->deviceId());
        return true;
    }
    return false;
}

bool
Bucket::deleteNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    addKnownNode(socket->deviceId());
    return nodes.erase(socket);
}

bool
Bucket::deleteNodeId(const NodeId& nodeId)
{
    std::shared_ptr<ChannelSocketInterface> socketToDelete;
    for (const auto& node : nodes) {
        if (node.first->deviceId() == nodeId) {
            return deleteNode(node.first);
        }
    }

    return 0;
}

asio::steady_timer&
Bucket::getNodeTimer(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto node = nodes.find(socket);
    if (node == nodes.end()) {
        throw std::range_error("Can't find timer " + socket->deviceId().toString());
    }
    return node->second;
}

std::set<std::shared_ptr<ChannelSocketInterface>>
Bucket::getNodes() const
{
    std::set<std::shared_ptr<ChannelSocketInterface>> sockets;
    for (auto const& key : nodes) {
        sockets.insert(key.first);
    }
    return sockets;
}

std::set<NodeId>
Bucket::getNodeIds() const
{
    std::set<NodeId> nodesId;

    for (auto const& key : nodes) {
        nodesId.insert(key.first->deviceId());
    }

    return nodesId;
}

NodeId
Bucket::getKnownNodeId(unsigned index) const
{
    if (index > known_nodes.size()) {
        throw std::out_of_range("End of table for get known Node Id " + std::to_string(index));
    }
    auto it = known_nodes.begin();
    std::advance(it, index);

    return *it;
}

std::set<NodeId>
Bucket::getKnownNodesRandom(unsigned numberNodes, std::mt19937_64& rd) const
{
    std::set<NodeId> nodesToReturn;

    if (getKnownNodesSize() <= numberNodes)
        return getKnownNodes();

    std::uniform_int_distribution<unsigned> distrib(0, getKnownNodesSize() - 1);

    while (nodesToReturn.size() < numberNodes) {
        nodesToReturn.insert(getKnownNodeId(distrib(rd)));
    }

    return nodesToReturn;
}

void
Bucket::printBucket(unsigned number) const
{
    JAMI_ERROR("BUCKET Number: {:d}", number);

    unsigned nodeNum = 1;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        JAMI_DEBUG("Node {:s}   Id: {:s}",
                   std::to_string(nodeNum),
                   it->first->deviceId().to_c_str());
        nodeNum++;
    }
};

bool
Bucket::shutdownNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto node = nodes.find(socket);

    if (node != nodes.end()) {
        node->first->shutdown();
        deleteNode(socket);
        return 1;
    }
    return 0;
}

void
Bucket::shutdownAllNodes()
{
    while (not nodes.empty()) {
        auto socket = nodes.begin()->first;
        socket->shutdown();
        deleteNode(socket);
    }
}

bool
Bucket::hasNodeId(const NodeId& nodeId) const
{
    for (const auto& sockets : nodes) {
        if (sockets.first->deviceId() == nodeId) {
            return 1;
        }
    }

    return 0;
}

//####################################################################################################

RoutingTable::RoutingTable()
{
    Bucket bucket(NodeId::zero());
    buckets.emplace_back(std::move(bucket));
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
RoutingTable::contains(std::list<Bucket>::iterator& bucket, const NodeId& nodeId)
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
    Bucket newBucket(id);
    auto newBucketIt = buckets.insert(std::next(bucket), std::move(newBucket));

    // Re-assign nodes
    auto nodeSwap = bucket->getNodes();

    while (!nodeSwap.empty()) {
        auto n = nodeSwap.begin();

        if (!contains(bucket, (*n)->deviceId())) {
            bucket->deleteNode(*n);
            newBucketIt->addNode(*n);
        }

        nodeSwap.erase(n);
    }

    auto knownSwap = bucket->getKnownNodes();

    while (!knownSwap.empty()) {
        auto n = knownSwap.begin();

        if (!contains(bucket, (*n))) {
            bucket->removeKnownNode(*n);
            newBucketIt->addKnownNode(*n);
        }

        knownSwap.erase(n);
    }

    auto connectingNodes = bucket->getConnectingNodes();
    std::set<NodeId> connectingSwap(connectingNodes.begin(), connectingNodes.end());

    while (!connectingSwap.empty()) {
        auto n = connectingSwap.begin();

        if (!contains(bucket, (*n))) {
            bucket->removeConnectingNode(*n);
            newBucketIt->addConnectingNode(*n);
        }

        connectingSwap.erase(n);
    }

    return true;
}

bool
RoutingTable::addNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto nodeId = socket->deviceId();
    auto bucket = findBucket(nodeId);
    auto added = addNode(socket, bucket);
    bucket->removeConnectingNode(nodeId);
    return added;
}

bool
RoutingTable::addNode(const std::shared_ptr<ChannelSocketInterface>& socket,
                      std::list<Bucket>::iterator& bucket)
{
    if (bucket->hasNodeId(socket->deviceId()) || id_ == socket->deviceId()) {
        return false;
    }

    while (bucket->isFull()) {
        if (contains(bucket, id_)) {
            split(bucket);
            bucket = findBucket(socket->deviceId());
        } else
            return false;
    }
    return bucket->addNode(socket);
}

bool
RoutingTable::deleteNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto bucket = findBucket(socket->deviceId());

    if (bucket == buckets.end()) {
        return 0;
    }

    return bucket->deleteNode(socket);
}

bool
RoutingTable::hasNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto bucket = findBucket(socket->deviceId());
    return bucket->hasNode(socket);
};

std::vector<NodeId>
RoutingTable::closestNodes(const NodeId& nodeId, unsigned count)
{
    auto bucket = findBucket(nodeId);
    std::vector<NodeId> closestNodes;
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
RoutingTable::shutdownNode(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    auto bucket = findBucket(socket->deviceId());

    if (bucket != buckets.end()) {
        bucket->shutdownNode(socket);
    }
}

} // namespace jami