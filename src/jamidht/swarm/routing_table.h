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

static constexpr const std::chrono::minutes FIND_PERIOD {10};

struct NodeInfo
{
    bool isPersistent {true};
    std::shared_ptr<ChannelSocketInterface> socket {};
    asio::steady_timer refresh_timer {*Manager::instance().ioContext(), FIND_PERIOD};
    NodeInfo() = delete;
    NodeInfo(NodeInfo&&) = default;
    NodeInfo(std::shared_ptr<ChannelSocketInterface> socket_)
        : socket(socket_)
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
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool addNode(std::shared_ptr<ChannelSocketInterface> socket);
    /**
     * Add Node socket to bucket
     * @param NodeInfo&& nodeInfo
     */
    bool addNode(NodeInfo&& info);

    /**
     * Remove Node socket from bucket
     * @param NodeId& nodeId
     */
    bool removeNode(const NodeId& nodeId);

    /**
     * Get Nodes from bucket
     */
    std::map<NodeId, NodeInfo>& getNodes() { return nodes; }

    /**
     * Get NodeIds from bucket as set
     */
    std::set<NodeId> getNodeIds() const;

    /**
     * Test if socket exists in nodes as Id
     * @param const NodeId& nodeId
     */
    bool hasNode(const NodeId& nodeId) const;

    /**
     * Add NodeId to known_nodes
     * @param NodeId nodeId
     */
    bool addKnownNode(const NodeId& nodeId);

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
     * Returns indexed NodeId from known_nodes
     * @param unsigned index
     */
    NodeId getKnownNode(unsigned index) const;

    /**
     * Test if NodeId exist in known_nodes
     * @param NodeId nodeId
     */
    bool hasKnownNode(const NodeId& nodeId) const
    {
        return known_nodes.find(nodeId) != known_nodes.end();
    }

    /**
     * Add NodeId to mobile_nodes
     * @param NodeId nodeId
     */
    bool addMobileNode(const NodeId& nodeId);

    /**
     * Remove NodeId from mobile_nodes
     * @param NodeId nodeId
     */
    void removeMobileNode(const NodeId& nodeId) { mobile_nodes.erase(nodeId); }

    /**
     * Test if NodeId exist in mobile_nodes
     * @param NodeId nodeId
     */
    bool hasMobileNode(const NodeId& nodeId)
    {
        return mobile_nodes.find(nodeId) != mobile_nodes.end();
    }

    /**
     * Get NodeIds of mobile_nodes
     */
    const std::set<NodeId>& getMobileNodes() const { return mobile_nodes; }

    /**
     * Add NodeId to connecting_nodes
     * @param NodeId& nodeId
     * @param NodeInfo&& nodeInfo
     */
    bool addConnectingNode(const NodeId& nodeId) { return connecting_nodes.insert(nodeId).second; }

    /**
     * Remove NodeId from connecting_nodes
     * @param NodeId nodeId
     */
    void removeConnectingNode(const NodeId& nodeId) { connecting_nodes.erase(nodeId); }

    /** Get NodeIds of connecting_nodes
     */
    std::set<NodeId>& getConnectingNodes() { return connecting_nodes; };

    /**
     * Test if NodeId exist in connecting_nodes
     * @param NodeId nodeId
     */
    bool hasConnectingNode(const NodeId& nodeId) const
    {
        return connecting_nodes.find(nodeId) != connecting_nodes.end();
    }

    /**
     * Indicate if bucket is full
     */
    bool isFull() const { return nodes.size() == BUCKET_MAX_SIZE; };

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
    bool shutdownNode(const NodeId& nodeId);

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

    /**
     * Change persistency of specific node
     */
    void changePersistency(const NodeId& nodeId, bool isPersistent);

    // For tests

    /**
     * Get sockets from bucket
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
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool addNode(std::shared_ptr<ChannelSocketInterface> socket);

    /**
     * Add socket to bucket
     * @param shared_ptr<ChannelSocketInterface> socket
     * @param list<Bucket>::iterator bucket
     */
    bool addNode(std::shared_ptr<ChannelSocketInterface> channel,
                 std::list<Bucket>::iterator& bucket);

    /**
     * Removes node from routing table
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool removeNode(const NodeId& nodeId);

    /**
     * Check if node in routing table
     * @param shared_ptr<ChannelSocketInterface>& socket
     */
    bool hasNode(const NodeId& nodeId);

    /**
     * Add known node to bucket
     * @param NodeId nodeId
     */
    bool addKnownNode(const NodeId& nodeId);

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
     * Add mobile node to bucket
     * @param NodeId nodeId
     */
    bool addMobileNode(const NodeId& nodeId);

    /**
     * Remove mobile node to bucket
     * @param NodeId nodeId
     */
    bool removeMobileNode(const NodeId& nodeId);

    /**
     * Check if mobile node exists in routing table
     * @param NodeId nodeId
     */
    bool hasMobileNode(const NodeId& nodeId);

    /**
     * Add connecting node to bucket
     * @param NodeId nodeId
     */

    bool addConnectingNode(const NodeId& nodeId);
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
     * Returns number nodes of closest nodes to specific nodeId
     * @param NodeId nodeId
     * @param int count
     */
    std::vector<NodeId> closestNodes(const NodeId& nodeId, unsigned count);

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
    void shutdownNode(const NodeId& nodeId);

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
    std::vector<NodeId> getNodes() const;

    /**
     * Returns all routing table's mobile nodes
     */
    std::vector<NodeId> getMobileNodes() const;

    /**
     * Returns mobile nodes corresponding to the swarm's id
     */
    std::vector<NodeId> getBucketMobileNodes() const;

    /**
     * Test if nodeId is in specific bucket
     * @param list<Bucket>::iterator it
     * @param NodeId nodeId
     */
    bool contains(const std::list<Bucket>::iterator& it, const NodeId& nodeId) const;

private:
    RoutingTable(const RoutingTable&) = delete;
    RoutingTable& operator=(const RoutingTable&) = delete;

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
