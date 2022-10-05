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
#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */
#include <algorithm>

#include "jamidht/multiplexed_socket.h"
#include "opendht/infohash.h"

namespace jami {

Bucket::Bucket(const NodeId& id)
    : lowerLimit(id)
{}

bool
Bucket::addNode(const std::shared_ptr<ChannelSocket>& socket)
{
    if (isFull()) {
        return 0;
    }

    nodes.insert(socket);
    connecting_nodes.erase(socket->deviceId());

    return 1;
}

bool
Bucket::deleteNode(const std::shared_ptr<ChannelSocket>& socket)
{
    auto it = nodes.find(socket);

    if (it != nodes.end()) {
        nodes.erase(it);
        return 1;

    } else {
        JAMI_ERROR("Node doesn't exist");
        return 0;
    }
}
std::list<NodeId>
Bucket::getNodeIds() const

{
    std::list<NodeId> nodesId;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        nodesId.push_back((*it)->deviceId());
    }

    return nodesId;
}

NodeId
Bucket::getChannelNodeId(int index) const
{
    if (index > nodes.size()) {
        return NodeId::zero();
    }
    auto it = nodes.begin();
    std::advance(it, index);

    return (*it)->deviceId();
}

NodeId
Bucket::getKnownNodeId(int index) const
{
    if (index > known_nodes.size()) {
        return NodeId::zero();
    }
    auto it = known_nodes.begin();
    std::advance(it, index);

    return *it;
}

//####################################################################################################

RoutingTable::RoutingTable()
{
    Bucket bucket(NodeId::zero());
    buckets.push_back(bucket);
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
    return std::memcmp(bucket->getLowerLimit().data(), nodeId.data(), nodeId.size()) <= 0
           && (std::next(bucket) == buckets.end()
               || std::memcmp(nodeId.data(),
                              std::next(bucket)->getLowerLimit().data(),
                              nodeId.size())
                      < 0);
}

std::set<NodeId>
RoutingTable::getKnownNodesRandom(const NodeId& nodeId, int numberNodes)
{
    std::set<NodeId> nodesToReturn;
    auto bucket = findBucket(nodeId);

    // Is not supposed to happen
    if (bucket == buckets.end()) {
        JAMI_ERROR("BUCKET NOT FOUND");
        return nodesToReturn;
    }

    if (bucket->getKnownNodesSize() < numberNodes) {
        return bucket->getKnownNodes();
    }

    int counter = 0;
    int a;
    srand(time(NULL));

    while (counter < numberNodes) {
        a = rand() % numberNodes;
        auto found = std::find(nodesToReturn.begin(),
                               nodesToReturn.end(),
                               bucket->getKnownNodeId(a));

        if (found == nodesToReturn.end()) {
            nodesToReturn.insert(bucket->getKnownNodeId(a));
            counter++;
        }
    }

    return nodesToReturn;
}

// finds corresponding bucket for specific device id and returns iterator
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
    /*     if (bucket == buckets.end())
            return 0; */
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
    buckets.insert(std::next(bucket), newBucket);

    // Re-assign nodes
    std::set<std::shared_ptr<ChannelSocket>> nodeSwap;
    nodeSwap.merge(bucket->getNodes());

    while (!nodeSwap.empty()) {
        auto n = nodeSwap.begin();
        auto b = findBucket((*n)->deviceId());

        b->addNode(*n);
        nodeSwap.erase(n);
    }

    std::set<NodeId> knownSwap;
    knownSwap.merge(bucket->getKnownNodes());

    while (!knownSwap.empty()) {
        auto n = knownSwap.begin();
        auto b = findBucket(*n);

        if (b != bucket) {
            bucket->removeKnownNode(*n);

            b->addKnownNode(*n);
        }
        knownSwap.erase(n);
    }

    std::set<NodeId> connectingSwap;

    connectingSwap.merge(bucket->getConnectingNodes());

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
    // if the corresponding bucket is full, we need to check if it already exists and if not

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

} // namespace jami

/* std::vector<NodeId>
RoutingTable::getNodeNeighbours(const NodeId& nodeId, int nodesNumber)
{
    auto bucket = findBucket(nodeId);

    // Not supposed to happen
    if (bucket == buckets.end()) {
        JAMI_ERROR("Bucket not found");
    };

    std::vector<NodeId> neighbours;
    // random nodesNumber nodes

    int counter = 0;
srand(time(NULL));

while (counter < nodesNumber) {
    int a = rand() % nodesNumber;

    auto found = std::find(neighbours.begin(), neighbours.end(), bucket->getChannelNodeId(a));

    if (found == neighbours.end()) {
        neighbours.push_back(bucket->getChannelNodeId(a));
        counter++;
    }
}

return neighbours;
}
* /
}
; // namespace jami */