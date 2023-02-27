/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
 *
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

static constexpr const std::chrono::minutes FIND_PERIOD {10};

struct NodeInfo
{
    bool isMobile_ {false};
    std::shared_ptr<ChannelSocketInterface> socket {};
    asio::steady_timer refresh_timer {*Manager::instance().ioContext(), FIND_PERIOD};
    NodeInfo() = delete;
    NodeInfo(NodeInfo&&) = default;
    NodeInfo(std::shared_ptr<ChannelSocketInterface> socket_)
        : socket(socket_)
    {}
    NodeInfo(bool mobile, std::shared_ptr<ChannelSocketInterface> socket_)
        : isMobile_(mobile)
        , socket(socket_)
    {}
};

class Bucket

{
public:
    static constexpr int BUCKET_MAX_SIZE = 2;

    Bucket() = delete;
    Bucket(const Bucket&) = delete;
    Bucket(const NodeId&);

    /**
     * Add Node socket to bucket
     * @param socket
     * @return true if node was added, false if not
     */
    bool addNode(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Add NodeInfo to bucket
     * @param nodeInfo
     * @return true if node was added, false if not
     */
    bool addNode(NodeInfo&& info);

    /**
     * Remove NodeId socket from bucket and insert it in known_nodes or
     * mobile_nodes depending on its type
     * @param nodeId
     * @return true if node was removed, false if not
     */
    bool removeNode(const NodeId& nodeId);

    /**
     * Get connected nodes from bucket
     * @return map of NodeId and NodeInfo
     */
    std::map<NodeId, NodeInfo>& getNodes() { return nodes; }

    /**
     * Get NodeIds from bucket
     * @return set of NodeIds
     */
    std::set<NodeId> getNodeIds() const;

    /**
     * Test if socket exists in nodes
     * @param nodeId
     * @return true if node exists, false if not
     */
    bool hasNode(const NodeId& nodeId) const;

    /**
     * Add NodeId to known_nodes if it doesn't exist in nodes
     * @param nodeId
     * @return true if known node was added, false if not
     */
    bool addKnownNode(const NodeId& nodeId);

    /**
     * Remove NodeId from known_nodes
     * @param nodeId
     */
    void removeKnownNode(const NodeId& nodeId) { known_nodes.erase(nodeId); }

    /**
     * Get NodeIds from known_nodes
     * @return set of known NodeIds
     */
    const std::set<NodeId>& getKnownNodes() const { return known_nodes; }

    /**
     * Returns NodeId from known_nodes at index
     * @param index
     * @return NodeId
     */
    NodeId getKnownNode(unsigned index) const;

    /**
     * Test if NodeId exist in known_nodes
     * @param nodeId
     * @return true if known node exists, false if not
     */
    bool hasKnownNode(const NodeId& nodeId) const
    {
        return known_nodes.find(nodeId) != known_nodes.end();
    }

    /**
     * Add NodeId to mobile_nodes if it doesn't exist in nodes
     * @param nodeId
     * @return true if mobile node was added, false if not
     */
    bool addMobileNode(const NodeId& nodeId);

    /**
     * Remove NodeId from mobile_nodes
     * @param nodeId
     */
    void removeMobileNode(const NodeId& nodeId) { mobile_nodes.erase(nodeId); }

    /**
     * Test if NodeId exist in mobile_nodes
     * @param nodeId
     * @return true if mobile node exists, false if not
     */
    bool hasMobileNode(const NodeId& nodeId)
    {
        return mobile_nodes.find(nodeId) != mobile_nodes.end();
    }

    /**
     * Get NodeIds from mobile_nodes
     * @return set of mobile NodeIds
     */
    const std::set<NodeId>& getMobileNodes() const { return mobile_nodes; }

    /**
     * Add NodeId to connecting_nodes if it doesn't exist in nodes
     * @param nodeId
     * @param nodeInfo
     * @return true if connecting node was added, false if not
     */
    bool addConnectingNode(const NodeId& nodeId);

    /**
     * Remove NodeId from connecting_nodes
     * @param nodeId
     */
    void removeConnectingNode(const NodeId& nodeId) { connecting_nodes.erase(nodeId); }

    /** Get NodeIds of connecting_nodes
     * @return set of connecting NodeIds
     */
    const std::set<NodeId>& getConnectingNodes() const { return connecting_nodes; };

    /**
     * Test if NodeId exist in connecting_nodes
     * @param nodeId
     * @return true if connecting node exists, false if not
     */
    bool hasConnectingNode(const NodeId& nodeId) const
    {
        return connecting_nodes.find(nodeId) != connecting_nodes.end();
    }

    /**
     * Indicate if bucket is full
     * @return true if bucket is full, false if not
     */
    bool isFull() const { return nodes.size() == BUCKET_MAX_SIZE; };

    /**
     * Returns random numberNodes NodeId from known_nodes
     * @param numberNodes
     * @param rd
     * @return set of numberNodes random known NodeIds
     */
    std::set<NodeId> getKnownNodesRandom(unsigned numberNodes, std::mt19937_64& rd) const;

    /**
     * Returns random NodeId from known_nodes
     * @param rd
     * @return random known NodeId
     */
    NodeId randomId(std::mt19937_64& rd) const
    {
        auto node = getKnownNodesRandom(1, rd);
        return *node.begin();
    }

    /**
     * Returns socket's timer
     * @param socket
     * @return timer
     */
    asio::steady_timer& getNodeTimer(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Shutdowns socket and removes it from nodes.
     * The corresponding node is moved to known_nodes or mobile_nodes
     * @param socket
     * @return true if node was shutdown, false if not found
     */
    bool shutdownNode(const NodeId& nodeId);

    /**
     * Shutdowns all sockets in nodes through shutdownNode
     */
    void shutdownAllNodes();

    /**
     * Prints bucket and bucket's number
     */
    void printBucket(unsigned number) const;

    /**
     * Change mobility of specific node, mobile or not
     */
    void changeMobility(const NodeId& nodeId, bool isMobile);

    /**
     * Returns number of nodes in bucket
     * @return size of nodes
     */
    unsigned getNodesSize() const { return nodes.size(); }

    /**
     * Returns number of knwon_nodes in bucket
     * @return size of knwon_nodes
     */
    unsigned getKnownNodesSize() const { return known_nodes.size(); }

    /**
     * Returns number of mobile_nodes in bucket
     * @return size of mobile_nodes
     */
    unsigned getConnectingNodesSize() const { return connecting_nodes.size(); }

    /**
     * Returns bucket lower limit
     * @return NodeId lower limit
     */
    NodeId getLowerLimit() const { return lowerLimit_; };

    /**
     * Set bucket's lower limit
     * @param nodeId
     */
    void setLowerLimit(const NodeId& nodeId) { lowerLimit_ = nodeId; }

    // For tests

    /**
     * Get sockets from bucket
     * @return set of sockets
     */
    std::set<std::shared_ptr<ChannelSocketInterface>> getNodeSockets() const;

private:
    NodeId lowerLimit_;
    std::map<NodeId, NodeInfo> nodes;
    std::set<NodeId> connecting_nodes;
    std::set<NodeId> known_nodes;
    std::set<NodeId> mobile_nodes;
    mutable std::mutex mutex;
};

// ####################################################################################################

class RoutingTable
{
public:
    RoutingTable();

    /**
     * Add socket to bucket
     * @param socket
     * @return true if socket was added, false if not
     */
    bool addNode(std::shared_ptr<ChannelSocketInterface> socket);

    /**
     * Add socket to specific bucket
     * @param channel
     * @param bucket
     * @return true if socket was added to bucket, false if not
     */
    bool addNode(std::shared_ptr<ChannelSocketInterface> channel,
                 std::list<Bucket>::iterator& bucket);

    /**
     * Removes node from routing table
     * Adds it to known_nodes or mobile_nodes depending on mobility
     * @param socket
     * @return true if node was removed, false if not
     */
    bool removeNode(const NodeId& nodeId);

    /**
     * Check if connected node exsits in routing table
     * @param nodeId
     * @return true if node exists, false if not
     */
    bool hasNode(const NodeId& nodeId);

    /**
     * Add known node to routing table
     * @param nodeId
     * @return true if known node was added, false if not
     */
    bool addKnownNode(const NodeId& nodeId);

    /**
     * Checks if known node exists in routing table
     * @param nodeId
     * @return true if known node exists, false if not
     */
    bool hasKnownNode(const NodeId& nodeId) const
    {
        auto bucket = findBucket(nodeId);
        return bucket->hasKnownNode(nodeId);
    }

    /**
     * Add mobile node to routing table
     * @param nodeId
     * @return true if mobile node was added, false if not
     */
    bool addMobileNode(const NodeId& nodeId);

    /**
     * Remove mobile node to routing table
     * @param nodeId
     * @return true if mobile node was removed, false if not
     */
    void removeMobileNode(const NodeId& nodeId);

    /**
     * Check if mobile node exists in routing table
     * @param nodeId
     * @return true if mobile node exists, false if not
     */
    bool hasMobileNode(const NodeId& nodeId);

    /**
     * Add connecting node to routing table
     * @param nodeId
     * @return true if connecting node was added, false if not
     */
    bool addConnectingNode(const NodeId& nodeId);

    /**
     * Remove connecting connecting node to routing table
     * @param  nodeId
     * @return true if connecting node was removed, false if not
     */
    void removeConnectingNode(const NodeId& nodeId);

    /**
     * Check if Connecting node exists in routing table
     * @param nodeId
     * @return true if connecting node exists, false if not
     */
    bool hasConnectingNode(const NodeId& nodeId) const
    {
        auto bucket = findBucket(nodeId);
        return bucket->hasConnectingNode(nodeId);
    }

    /**
     * Returns bucket iterator containing nodeId
     * @param nodeId
     * @return bucket iterator
     */
    std::list<Bucket>::iterator findBucket(const NodeId& nodeId);

    /**
     * Returns bucket iterator containing nodeId
     * @param nodeId
     * @return bucket iterator
     */
    inline const std::list<Bucket>::const_iterator findBucket(const NodeId& nodeId) const
    {
        return std::list<Bucket>::const_iterator(
            const_cast<RoutingTable*>(this)->findBucket(nodeId));
    }

    /**
     * Returns the count closest nodes to a specific nodeId
     * @param nodeId
     * @param count
     * @return vector of nodeIds
     */
    std::vector<NodeId> closestNodes(const NodeId& nodeId, unsigned count);

    /**
     * Returns number of buckets in routing table
     * @return size of buckets
     */
    unsigned getRoutingTableSize() const { return buckets.size(); }

    /**
     * Returns number of total nodes in routing table
     * @return size of nodes
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
     * @param nodeId
     */
    void shutdownNode(const NodeId& nodeId);

    /**
     * Shutdowns all nodes in routing table and add them to known_nodes or mobile_nodes
     */
    void shutdownAllNodes()
    {
        for (auto& bucket : buckets)
            bucket.shutdownAllNodes();
    }

    /**
     * Sets id for routing table
     * @param node
     */
    void setId(const NodeId& node) { id_ = node; }

    /**
     * Returns id for routing table
     * @return Nodeid
     */
    NodeId getId() const { return id_; }

    /**
     * Returns buckets in routing table
     * @return list buckets
     */
    std::list<Bucket>& getBuckets() { return buckets; }

    /**
     * Returns all routing table's connected nodes
     * @return vector of nodeIds
     */
    std::vector<NodeId> getNodes() const;

    /**
     * Returns all routing table's known nodes
     *@return vector of nodeIds
     */
    std::vector<NodeId> getKnownNodes() const;

    /**
     * Returns all routing table's mobile nodes
     * @return vector of nodeIds
     */
    std::vector<NodeId> getMobileNodes() const;

    /**
     * Returns all routing table's connecting nodes
     * @return vector of nodeIds
     */
    std::vector<NodeId> getConnectingNodes() const;

    /**
     * Returns mobile nodes corresponding to the swarm's id
     * @return vector of nodeIds
     */
    std::vector<NodeId> getBucketMobileNodes() const;

    /**
     * Test if connected nodeId is in specific bucket
     * @param it
     * @param nodeId
     * @return true if nodeId is in bucket, false if not
     */
    bool contains(const std::list<Bucket>::iterator& it, const NodeId& nodeId) const;

private:
    RoutingTable(const RoutingTable&) = delete;
    RoutingTable& operator=(const RoutingTable&) = delete;

    /**
     * Returns middle of routing table
     * @param it
     * @return NodeId
     */
    NodeId middle(std::list<Bucket>::iterator& it) const;

    /**
     * Returns depth of routing table
     * @param bucket
     * @return depth
     */
    unsigned depth(std::list<Bucket>::iterator& bucket) const;

    /**
     * Splits bucket
     * @param bucket
     * @return true if bucket was split, false if not
     */
    bool split(std::list<Bucket>::iterator& bucket);

    NodeId id_;

    std::list<Bucket> buckets;
};
}; // namespace jami
