/* /*
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

#include "jamidht/multiplexed_socket.h"
#include "opendht/infohash.h"
namespace jami {
using namespace dht;

Bucket::Bucket(const NodeId& id)
    : lowerLimit(id)
{}

bool
Bucket::addNode(const std::shared_ptr<ChannelSocket>& socket)
{
    if (isFull()) {
        return 0;
    }

    nodes.emplace(socket, asio::steady_timer(*Manager::instance().ioContext(), FIND_PERIOD));
    connecting_nodes.erase(socket->deviceId());

    return 1;
}

bool
Bucket::deleteNode(const std::shared_ptr<ChannelSocket>& socket)
{
    auto it = nodes.find(socket);

    if (it != nodes.end()) {
        nodes.erase(it);
        addKnownNode(socket->deviceId());
        return 1;

    } else {
        JAMI_ERROR("Node doesn't exist");
        return 0;
    }
}

std::set<std::shared_ptr<ChannelSocket>>
Bucket::getNodes() const
{
    std::set<std::shared_ptr<ChannelSocket>> sockets;
    for (auto const& key : nodes) {
        sockets.insert(key.first);
    }
    return sockets;
};

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
Bucket::getChannelNodeId(int index) const
{
    if (index > nodes.size()) {
        throw std::out_of_range("End of table for get Channel " + std::to_string(index));
    }
    auto it = nodes.begin();
    std::advance(it, index);

    return it->first->deviceId();
}

NodeId
Bucket::getKnownNodeId(int index) const
{
    if (index > known_nodes.size()) {
        throw std::out_of_range("End of table for get known Node Id " + std::to_string(index));
    }
    auto it = known_nodes.begin();
    std::advance(it, index);

    return *it;
}

std::set<NodeId>
Bucket::getKnownNodesRandom(int numberNodes) const
{
    std::set<NodeId> nodesToReturn;

    if (getKnownNodesSize() <= numberNodes) {
        return getKnownNodes();
    }
    std::mt19937_64 rand_(time(nullptr));
    std::uniform_int_distribution<> distrib(0, getKnownNodesSize());
    int counter = 0;
    int a;

    while (counter < numberNodes) {
        a = distrib(rand_);
        auto found = std::find(nodesToReturn.begin(), nodesToReturn.end(), getKnownNodeId(a));

        if (found == nodesToReturn.end()) {
            nodesToReturn.insert(getKnownNodeId(a));
            counter++;
        }
    }

    return nodesToReturn;
}

//####################################################################################################

RoutingTable::RoutingTable()
{
    Bucket bucket(NodeId::zero());
    buckets.push_back(std::move(bucket));
}

bool
RoutingTable::addKnownNode(const NodeId& nodeId)
{
    auto bucket = findBucket(nodeId);
    if (bucket == buckets.end()) {
        JAMI_ERROR("BUCKET NOT FOUND");
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
    JAMI_ERROR("############## CONTAINES {:s}", bucket->getLowerLimit().to_c_str());
    return Hash<32>::cmp(bucket->getLowerLimit(), nodeId) <= 0
           && (std::next(bucket) == buckets.end()
               || Hash<32>::cmp(nodeId, std::next(bucket)->getLowerLimit()))
                  < 0;
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
    buckets.insert(std::next(bucket), std::move(newBucket));

    // Re-assign nodes
    auto sockets = bucket->getNodes();
    std::set<std::shared_ptr<ChannelSocket>> nodeSwap(sockets.begin(), sockets.end());

    while (!nodeSwap.empty()) {
        auto n = nodeSwap.begin();
        auto b = findBucket((*n)->deviceId());

        b->addNode(*n);
        nodeSwap.erase(n);
    }

    auto knownNodes = bucket->getKnownNodes();
    std::set<NodeId> knownSwap(knownNodes.begin(), knownNodes.end());

    while (!knownSwap.empty()) {
        auto n = knownSwap.begin();
        auto b = findBucket(*n);

        if (b != bucket) {
            bucket->removeKnownNode(*n);

            b->addKnownNode(*n);
        }
        knownSwap.erase(n);
    }

    auto connectingNodes = bucket->getConnectingNodes();
    std::set<NodeId> connectingSwap(connectingNodes.begin(), connectingNodes.end());

    while (!connectingSwap.empty()) {
        auto n = connectingSwap.begin();
        auto b = findBucket(*n);

        if (b != bucket) {
            bucket->removeConnectingNode(*n);
            b->addConnectingNode(*n);
        }

        connectingSwap.erase(n);
    }

    return true;
}

bool
RoutingTable::addNode(const std::shared_ptr<ChannelSocket>& socket,
                      std::list<Bucket>::iterator& bucket)
{
    if (bucket->hasNode(socket)) {
        return 0;
    }

    if (bucket->isFull()) {
        split(bucket);
    }

    auto newBucket = findBucket(socket->deviceId());
    newBucket->addNode(socket);
    return 1;
}

bool
RoutingTable::deleteNode(const std::shared_ptr<ChannelSocket>& socket)
{
    auto bucket = findBucket(socket->deviceId());

    if (bucket == buckets.end()) {
        return 0;
    }

    bucket->deleteNode(socket);
    return 1;
}

std::vector<NodeId>
RoutingTable::closestNodes(const NodeId& nodeId, int count)
{
    auto bucket = findBucket(nodeId);
    std::vector<NodeId> closestNodes;
    if (bucket == buckets.end()) {
        return closestNodes;
    }

    auto sortedBucketInsert = [&](const std::list<Bucket>::iterator& b) {
        auto nodes = b->getNodeIds();
        for (auto n : nodes) {
            auto here = std::find_if(closestNodes.begin(),
                                     closestNodes.end(),
                                     [&nodeId, &n](NodeId& NodeId) {
                                         return nodeId.xorCmp(n, NodeId) < 0;
                                     });
            closestNodes.insert(here, n);
        }
    };

    auto itn = bucket;
    auto itp = (bucket == buckets.begin()) ? buckets.end() : std::prev(bucket);
    while (closestNodes.size() < count && (itn != buckets.end() || itp != buckets.end())) {
        if (itn != buckets.end()) {
            sortedBucketInsert(itn);
            itn = std::next(itn);
        }
        if (itp != buckets.end()) {
            sortedBucketInsert(itp);
            itp = (itp == buckets.begin()) ? buckets.end() : std::prev(itp);
        }
    }

    return closestNodes;
}

} // namespace jami