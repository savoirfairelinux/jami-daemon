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

#include "manager.h"

#include <opendht/infohash.h>
#include <vector>
#include <memory>
#include <list>
#include <set>
#include <algorithm>

#include <asio.hpp>
#include "asio/detail/deadline_timer_service.hpp"

using NodeId = dht::PkId;
constexpr const std::chrono::minutes FIND_PERIOD {10};
using namespace std::placeholders;

namespace jami {

class ChannelSocket;
class io_context;

class Bucket
{
public:
    static constexpr int BUCKET_MAX_SIZE = 8;

    Bucket(const NodeId&);

    /**
     * Add Node socket to bucket
     * @param shared_ptr<ChannelSocket> socket
     */
    bool addNode(const std::shared_ptr<ChannelSocket>& socket);

    /**
     * Delete Node socket from bucket
     * @param NodeId nodeId
     */
    bool deleteNode(const std::shared_ptr<ChannelSocket>& socket);

    /**
     * Get Node sockets from bucket
     */
    std::set<std::shared_ptr<ChannelSocket>> getNodes() const;

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
     * Returns indexed NodeId from Channel Socket
     * @param int index
     */
    NodeId getChannelNodeId(int index) const;

    /**
     * Returns indexed NodeId from known_nodes
     * @param int index
     */
    NodeId getKnownNodeId(int index) const;

    /**
     * Test if socket exists in nodes
     * @param std::shared_ptr<ChannelSocket> socket
     */
    bool hasNode(const std::shared_ptr<ChannelSocket>& socket) const
    {
        return nodes.find(socket) != nodes.end();
    }

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
     * @param int numberNodes
     */
    std::set<NodeId> getKnownNodesRandom(int numberNodes) const;

    NodeId randomId() const
    {
        auto node = getKnownNodesRandom(1);
        return *node.begin();
    }

    /**
     * Returns socket's timer
     * @param td::shared_ptr<ChannelSocket>& socket
     */
    asio::steady_timer& getNodeTimer(const std::shared_ptr<ChannelSocket>& socket)
    {
        auto node = nodes.find(socket);
        return node->second;
    }

    /**
     * Returns bucket lower limit
     * @param int index
     */
    NodeId getLowerLimit() const { return lowerLimit; };

    /**
     * Set bucket lower limit
     * @param NodeId nodeId
     */
    void setLowerLimit(const NodeId& nodeId) { lowerLimit = nodeId; };

    int getNodesSize() const { return nodes.size(); };
    int getKnownNodesSize() const { return known_nodes.size(); };
    int getConnectingNodesSize() const { return connecting_nodes.size(); };

private:
    NodeId lowerLimit;
    std::map<std::shared_ptr<ChannelSocket>, asio::steady_timer> nodes; // (channel, expired)
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
     * @param shared_ptr<ChannelSocket> socket
     * @param list<Bucket>::iterator bucket
     */
    bool addNode(const std::shared_ptr<ChannelSocket>& socket, std::list<Bucket>::iterator& bucket);

    /**
     * Add known node to bucket
     * @param NodeId nodeId
     */
    bool addKnownNode(const NodeId& nodeId);

    /**
     * Deletes node from routing table
     * @param NodeId nodeId
     */
    bool deleteNode(const std::shared_ptr<ChannelSocket>& socket);

    /**
     * Deletes node from routing table
     * @param NodeId nodeId
     */
    std::vector<NodeId> closestNodes(const NodeId& nodeId, int count);

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

    std::list<Bucket> buckets;

    std::list<Bucket>::iterator findBucket(const NodeId& nodeId);

    inline const std::list<Bucket>::const_iterator findBucket(const NodeId& nodeId) const
    {
        return std::list<Bucket>::const_iterator(
            const_cast<RoutingTable*>(this)->findBucket(nodeId));
    }

    int getRoutingTableSize() const { return buckets.size(); }

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
};
}; // namespace jami