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

using NodeId = dht::PkId;

namespace jami {
class ChannelSocket;

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
     * Get Sockets from bucket
     */
    std::set<std::shared_ptr<ChannelSocket>> getNodes() const { return nodes; };

    /**
     * Get NodeIds from bucket as list
     */
    std::list<NodeId> getNodeIds() const;

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
    std::set<NodeId> getKnownNodes() const { return known_nodes; };

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
    std::set<NodeId> getConnectingNodes() const { return connecting_nodes; };

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
     * Returns indexed NodeId from connecting_nodes
     * @param int index
     */
    NodeId getConnectingNodeId(int index) const;

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
     * Returns random NodeId from known_nodes
     * @param int index
     */
    NodeId randomId() const
    {
        srand(time(NULL));
        int a = rand() % known_nodes.size();
        return getKnownNodeId(a);
    }

    /**
     * Returns bucket lower limit
     * @param int index
     */
    NodeId getLowerLimit() const { return lowerLimit; };

    void setLowerLimit(const NodeId& nodeId) { lowerLimit = nodeId; };

    int getNodesSize() const { return nodes.size(); };
    int getKnownNodesSize() const { return known_nodes.size(); };
    int getConnectingNodesSize() const { return connecting_nodes.size(); };

private:
    NodeId lowerLimit;
    std::set<std::shared_ptr<ChannelSocket>> nodes;
    std::set<NodeId> known_nodes;
    std::set<NodeId> connecting_nodes;
};

class RoutingTable
{
public:
    RoutingTable();

    bool contains(std::list<Bucket>::iterator& it, const NodeId& nodeId);

    bool addNode(const std::shared_ptr<ChannelSocket>& socket, std::list<Bucket>::iterator& bucket);

    bool addKnownNode(const NodeId& nodeId);

    std::set<NodeId> getKnownNodesRandom(const NodeId& nodeId, int numberNodes);

    bool deleteNode(const NodeId& nodeId);

    unsigned depth(std::list<Bucket>::iterator& bucket) const;

    NodeId middle(std::list<Bucket>::iterator& it) const;

    // std::vector<NodeId> getNodeNeighbours(const NodeId& nodeId, int nodesNumber);

    int getRoutingTableSize() const { return buckets.size(); }

    std::list<Bucket> buckets;

    std::list<Bucket>::iterator findBucket(const NodeId& nodeId);

    inline const std::list<Bucket>::const_iterator findBucket(const NodeId& nodeId) const
    {
        return std::list<Bucket>::const_iterator(
            const_cast<RoutingTable*>(this)->findBucket(nodeId));
    }

private:
    bool split(std::list<Bucket>::iterator& bucket);
};

}; // namespace jami */