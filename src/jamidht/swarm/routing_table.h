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
#pragma once

#include "manager.h"

#include <opendht/infohash.h>
#include <vector>
#include <memory>
#include <list>
#include <set>
#include <algorithm>

#include <asio.hpp>
#include <asio/detail/deadline_timer_service.hpp>

using NodeId = dht::PkId;

namespace jami {

class ChannelSocketInterface;
class io_context;

class Bucket
{
public:
    static constexpr int BUCKET_MAX_SIZE = 2;

    Bucket(const NodeId&);

    /**
     * Add Node socket to bucket
     * @param shared_ptr<ChannelSocket> socket
     */
    bool addNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Delete Node socket from bucket
     * @param NodeId nodeId
     */
    bool deleteNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Delete Node socket from bucket
     * @param shared_ptr<ChannelSocket> socket
     */
    bool deleteNodeId(const NodeId& nodeId);

    /**
     * Get Node sockets from bucket
     */
    std::set<std::shared_ptr<ChannelSocketInterface>> getNodes() const;

    /**
     * Get NodeIds from bucket as set
     */
    std::set<NodeId> getNodeIds() const;

    /**
     * Add NodeId to known_nodes
     * @param NodeId nodeId
     */
    void addKnownNode(const NodeId& nodeId) { known_nodes.insert(nodeId); }

    /**
     * Remove NodeId from known_nodes
     * @param NodeId nodeId
     */
    void removeKnownNode(const NodeId& nodeId) { known_nodes.erase(nodeId); }

    /**
     * Get NodeIds of known_nodes
     */
    const std::set<NodeId>& getKnownNodes() const { return known_nodes; }

    /**
     * Add NodeId to connecting_nodes
     * @param NodeId nodeId
     */
    void addConnectingNode(const NodeId& nodeId) { connecting_nodes.insert(nodeId); }

    /**
     * Remove NodeId from connecting_nodes
     * @param NodeId nodeId
     */
    void removeConnectingNode(const NodeId& nodeId) { connecting_nodes.erase(nodeId); }

    /** Get NodeIds of connecting_nodes
     */
    const std::set<NodeId>& getConnectingNodes() const { return connecting_nodes; };

    /**
     * Indicate if bucket is full
     */
    bool isFull() const { return nodes.size() == BUCKET_MAX_SIZE; };

    /**
     * Returns indexed NodeId from known_nodes
     * @param unsigned index
     */
    NodeId getKnownNodeId(unsigned index) const;

    /**
     * Test if socket exists in nodes
     * @param shared_ptr<ChannelSocketInterface> socket
     */
    bool hasNode(const std::shared_ptr<ChannelSocketInterface>& socket) const
    {
        return nodes.find(socket) != nodes.end();
    }

    /**
     * Test if socket exists in nodes as Id
     * @param const NodeId& nodeId
     */
    bool hasNodeId(const NodeId& nodeId) const;

    /**
     * Test if NodeId exist in known_nodes
     * @param NodeId nodeId
     */
    bool hasKnownNode(const NodeId& nodeId) const
    {
        return known_nodes.find(nodeId) != known_nodes.end();
    }

    /**
     * Test if NodeId exist in connecting_nodes
     * @param NodeId nodeId
     */
    bool hasConnectingNode(const NodeId& nodeId) const
    {
        return connecting_nodes.find(nodeId) != connecting_nodes.end();
    }

    /**
     * Returns random numberNodes NodeId from known_nodes
     * @param unsigned numberNodes
     */
    std::set<NodeId> getKnownNodesRandom(unsigned numberNodes, std::mt19937_64& rd) const;

    /**
     * Returns random NodeId from known_nodes
     * @param std::mt19937_64& rd
     */
    NodeId randomId(std::mt19937_64& rd) const
    {
        auto node = getKnownNodesRandom(1, rd);
        return *node.begin();
    }

    /**
     * Returns socket's timer
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    asio::steady_timer& getNodeTimer(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Returns bucket lower limit
     * @param int index
     */
    NodeId getLowerLimit() const { return lowerLimit_; };

    /**
     * Set bucket lower limit
     * @param NodeId nodeId
     */
    void setLowerLimit(const NodeId& nodeId) { lowerLimit_ = nodeId; }

    /**
     * Shutdowns node
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool shutdownNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Shutdowns all nodes
     */
    void shutdownAllNodes();

    /**
     * Prints bucket
     */
    void printBucket(unsigned number) const;

    /**
     * Returns number of nodes in bucket
     */
    unsigned getNodesSize() const { return nodes.size(); }

    /**
     * Returns number of known_nodes in bucket
     */
    unsigned getKnownNodesSize() const { return known_nodes.size(); }

    /**
     * Returns number of connecting_nodes in bucket
     */
    unsigned getConnectingNodesSize() const { return connecting_nodes.size(); }

private:
    NodeId lowerLimit_;
    std::map<std::shared_ptr<ChannelSocketInterface>, asio::steady_timer> nodes; // (channel, expired)
    std::set<NodeId> known_nodes;
    std::set<NodeId> connecting_nodes;
};

//####################################################################################################

class RoutingTable
{
public:
    RoutingTable();

    /**
     * Test if nodeId is in specific bucket
     * @param list<Bucket>::iterator it
     * @param NodeId nodeId
     */
    bool contains(std::list<Bucket>::iterator& it, const NodeId& nodeId);

    /**
     * Add socket to bucket
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool addNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Add socket to bucket
     * @param shared_ptr<ChannelSocketInterface> socket
     * @param list<Bucket>::iterator bucket
     */
    bool addNode(const std::shared_ptr<ChannelSocketInterface>& socket,
                 std::list<Bucket>::iterator& bucket);

    /**
     * Add known node to bucket
     * @param NodeId nodeId
     */
    bool addKnownNode(const NodeId& nodeId);

    /**
     * Deletes node from routing table
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool deleteNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Check if node in routing table
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool hasNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Check if node in routing table through its id
     * @param NodeId nodeId
     */
    bool hasNodeId(const NodeId nodeId)
    {
        auto bucket = findBucket(nodeId);
        return bucket->hasNodeId(nodeId);
    }

    /**
     * Returns number nodes of closest nodes to specific nodeId
     * @param NodeId nodeId
     * @param int count
     */
    std::vector<NodeId> closestNodes(const NodeId& nodeId, unsigned count);

    /**
     * Check if known node exists in routing table
     * @param NodeId nodeId
     */
    bool hasKnownNode(const NodeId& nodeId) const
    {
        auto bucket = findBucket(nodeId);
        return bucket->hasKnownNode(nodeId);
    }

    /**
     * Check if Connecting node exists in routing table
     * @param NodeId nodeId
     */
    bool hasConnectingNode(const NodeId& nodeId) const
    {
        auto bucket = findBucket(nodeId);
        return bucket->hasConnectingNode(nodeId);
    }

    /**
     * Returns bucket iterator containing nodeId
     * @param NodeId nodeId
     */
    std::list<Bucket>::iterator findBucket(const NodeId& nodeId);

    inline const std::list<Bucket>::const_iterator findBucket(const NodeId& nodeId) const
    {
        return std::list<Bucket>::const_iterator(
            const_cast<RoutingTable*>(this)->findBucket(nodeId));
    }

    /**
     * Returns number of buckets in routing table
     */
    unsigned getRoutingTableSize() const { return buckets.size(); }

    /**
     * Returns number of total nodes in routing table
     */
    unsigned getRoutingTableNodeCount() const
    {
        size_t count = 0;
        for (const auto& b : buckets)
            count += b.getNodesSize();
        return count;
    }

    /**
     * Prints routing table
     */
    void printRoutingTable() const;

    /**
     * Shutdowns a node
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    void shutdownNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Shutdowns all nodes
     */
    void shutdownAllNodes()
    {
        for (auto& bucket : buckets)
            bucket.shutdownAllNodes();
    }

    /**
     * Sets id for routing table
     * @param NodeId& node
     */
    void setId(const NodeId& node) { id_ = node; }

    /**
     * Returns id for routing table
     * @param NodeId& node
     */
    NodeId getId() const { return id_; }

    /**
     * Returns buckets
     */
    std::list<Bucket>& getBuckets() { return buckets; }

    /**
     * Returns all routing table's nodes
     */
    std::vector<NodeId> getNodes() const
    {
        std::vector<NodeId> ret;
        for (const auto& b : buckets) {
            auto nodes = b.getNodeIds();
            ret.insert(ret.end(), nodes.begin(), nodes.end());
        }
        return ret;
    }

private:
    /**
     * Returns middle of routing table
     * @param list<Bucket>::iterator bucket
     */
    NodeId middle(std::list<Bucket>::iterator& it) const;

    /**
     * Returns depth of routing table
     * @param list<Bucket>::iterator bucket
     */
    unsigned depth(std::list<Bucket>::iterator& bucket) const;

    /**
     * Splits bucket
     * @param list<Bucket>::iterator bucket
     */
    bool split(std::list<Bucket>::iterator& bucket);

    NodeId id_;

    std::list<Bucket> buckets;
};
}; // namespace jami