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
#pragma once

#include "routing_table.h"
#include "swarm_protocol.h"

#include <iostream>
#include <memory>

namespace jami {

using namespace swarm_protocol;

class SwarmManager : public std::enable_shared_from_this<SwarmManager>
{
    using ChannelCb = std::function<bool(const std::shared_ptr<dhtnet::ChannelSocketInterface>&)>;
    using NeedSocketCb = std::function<void(const std::string&, ChannelCb&&)>;
    using ToConnectCb = std::function<bool(const NodeId&)>;
    using OnConnectionChanged = std::function<void(bool ok)>;

public:
    explicit SwarmManager(const NodeId&, const std::mt19937_64& rand, ToConnectCb&& toConnectCb);
    ~SwarmManager();

    NeedSocketCb needSocketCb_;

    std::weak_ptr<SwarmManager> weak() { return weak_from_this(); }

    /**
     * Get swarm manager id
     * @return NodeId
     */
    const NodeId& getId() const { return id_; }

    /**
     * Set list of nodes to the routing table known_nodes
     * @param known_nodes
     * @return if some are new
     */
    bool setKnownNodes(const std::vector<NodeId>& known_nodes);

    /**
     * Set list of nodes to the routing table mobile_nodes
     * @param mobile_nodes
     */
    void setMobileNodes(const std::vector<NodeId>& mobile_nodes);

    /**
     * Add channel to routing table
     * @param channel
     */
    void addChannel(const std::shared_ptr<dhtnet::ChannelSocketInterface>& channel);

    /**
     * Remove channel from routing table
     * @param channel
     */
    void removeNode(const NodeId& nodeId);

    /**
     * Change mobility of specific node
     * @param nodeId
     * @param isMobile
     */
    void changeMobility(const NodeId& nodeId, bool isMobile);

    /**
     * get all nodes from the different tables in bucket
     */
    std::vector<NodeId> getAllNodes() const;

    /**
     * Delete nodes from the different tables in bucket
     */
    void deleteNode(std::vector<NodeId> nodes);

    // For tests

    /**
     * Get routing table
     * @return RoutingTable
     */
    RoutingTable& getRoutingTable() { return routing_table; };

    /**
     * Get buckets of routing table
     * @return buckets list
     */
    std::list<Bucket>& getBuckets() { return routing_table.getBuckets(); };

    /**
     * Shutdown swarm manager
     */
    void shutdown();

    /**
     * Restart the swarm manager.
     *
     * This function must be called in situations where we want
     * to use a swarm manager that was previously shut down.
     */
    void restart();

    /**
     * Display swarm manager info
     */
    void display()
    {
        JAMI_DEBUG("SwarmManager {:s} has {:d} nodes in table [P = {}]",
                   getId().to_c_str(),
                   routing_table.getRoutingTableNodeCount(),
                   isMobile_);
        // print nodes of routingtable
        for (auto& bucket : routing_table.getBuckets()) {
            for (auto& node : bucket.getNodes()) {
                JAMI_DEBUG("Node {:s}", node.first.toString());
            }
        }
    }

    /*
     * Callback for connection changed
     * @param cb
     */
    void onConnectionChanged(OnConnectionChanged cb) { onConnectionChanged_ = std::move(cb); }

    /**
     * Set mobility of swarm manager
     * @param isMobile
     */
    void setMobility(bool isMobile) { isMobile_ = isMobile; }

    /**
     * Get mobility of swarm manager
     * @return true if mobile, false if not
     */
    bool isMobile() const { return isMobile_; }

    /**
     * Maintain/Update buckets
     * @param toConnect         Nodes to connect
     */
    void maintainBuckets(const std::set<NodeId>& toConnect = {});

    /**
     * Check if we're connected with a specific device
     * @param deviceId
     * @return true if connected, false if not
     */
    bool isConnectedWith(const NodeId& deviceId);

    /**
     * Check if swarm manager is shutdown
     * @return true if shutdown, false if not
     */
    bool isShutdown() { return isShutdown_; };

private:
    /**
     * Add node to the known_nodes list
     * @param nodeId
     * @return if node inserted
     */
    bool addKnownNode(const NodeId& nodeId);

    /**
     * Add node to the mobile_Nodes list
     * @param nodeId
     */
    void addMobileNodes(const NodeId& nodeId);

    /**
     * Send nodes request to fill known_nodes list
     * @param socket
     * @param nodeId
     * @param q
     * @param numberNodes
     */
    void sendRequest(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                     NodeId& nodeId,
                     Query q,
                     int numberNodes = Bucket::BUCKET_MAX_SIZE);

    /**
     * Send answer to request
     * @param socket
     * @param msg
     */
    void sendAnswer(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, const Message& msg_);

    /**
     * Interpret received message
     * @param socket
     */
    void receiveMessage(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket);

    /**
     * Reset node's timer expiry
     * @param ec
     * @param socket
     * @param node
     */
    void resetNodeExpiry(const asio::error_code& ec,
                         const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                         NodeId node = {});

    /**
     * Try to establich connexion with specific node
     * @param nodeId
     */
    void tryConnect(const NodeId& nodeId);

    /**
     * Remove node from routing table
     * @param nodeId
     */
    void removeNodeInternal(const NodeId& nodeId);

    const NodeId id_;
    bool isMobile_ {false};
    std::mt19937_64 rd;
    mutable std::mutex mutex;
    RoutingTable routing_table;

    std::atomic_bool isShutdown_ {false};

    OnConnectionChanged onConnectionChanged_ {};

    ToConnectCb toConnectCb_;
};

} // namespace jami
