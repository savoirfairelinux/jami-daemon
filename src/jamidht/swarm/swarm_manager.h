/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
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

#include "routing_table.h"
#include "swarm_protocol.h"

#include <iostream>
#include <memory>

namespace jami {

using namespace swarm_protocol;

class SwarmManager : public std::enable_shared_from_this<SwarmManager>
{
    using ChannelCb = std::function<bool(const std::shared_ptr<ChannelSocketInterface>&)>;
    using NeedSocketCb = std::function<void(const std::string&, ChannelCb&&)>;
    using OnConnectionChanged = std::function<void(bool ok)>;

public:
    SwarmManager(const NodeId&);
    ~SwarmManager();

    NeedSocketCb needSocketCb_;

    std::weak_ptr<SwarmManager> weak()
    {
        return std::static_pointer_cast<SwarmManager>(shared_from_this());
    }

    const NodeId& getId() const { return id_; }

    /**
     * Add list of nodes to the known nodes list
     * @param vector<NodeId>& known_nodes
     */
    void setKnownNodes(const std::vector<NodeId>& known_nodes);

    /**
     * Add list of nodes to the mobile nodes list
     * @param vector<NodeId>& mobile_nodes
     */
    void setMobileNodes(const std::vector<NodeId>& mobile_nodes);

    /**
     * Add channel to routing table
     * @param shared_ptr<ChannelSocketInterface>& channel
     */
    void addChannel(std::shared_ptr<ChannelSocketInterface> channel);

    void removeNode(const NodeId& nodeId);
    void changeMobility(const NodeId& nodeId, bool isMobile);

    /** For testing */
    RoutingTable& getRoutingTable() { return routing_table; };
    std::list<Bucket>& getBuckets() { return routing_table.getBuckets(); };

    void shutdown();

    void display()
    {
        JAMI_DEBUG("SwarmManager {:s} has {:d} nodes in table [P = {}]",
                   getId().to_c_str(),
                   routing_table.getRoutingTableNodeCount(),
                   isMobile_);
    }

    void onConnectionChanged(OnConnectionChanged cb) { onConnectionChanged_ = std::move(cb); }

    void setMobility(bool isMobile) { isMobile_ = isMobile; }

    bool isMobile() const { return isMobile_; }

    /**
     * Maintain/Update buckets
     */
    void maintainBuckets();

    bool hasChannel(const NodeId& deviceId);

private:
    /**
     * Add node to the known_nodes list
     * @param NodeId nodeId
     */
    void addKnownNodes(const NodeId& nodeId);

    /**
     * Add node to the mobile_nodes list
     * @param NodeId nodeId
     */
    void addMobileNodes(const NodeId& nodeId);
    /**
     * Send nodes request to fill known_nodes list
     * @param shared_ptr<ChannelSocketInterface>& socket
     * @param NodeId& nodeId
     * @param Query q
     * @param int numberNodes
     */
    void sendRequest(const std::shared_ptr<ChannelSocketInterface>& socket,
                     NodeId& nodeId,
                     Query q,
                     int numberNodes = Bucket::BUCKET_MAX_SIZE);

    /**
     * Send answer to request
     * @param std::shared_ptr<ChannelSocketInterface>& socket
     * @param Message msg
     */
    void sendAnswer(const std::shared_ptr<ChannelSocketInterface>& socket, const Message& msg_);

    /**
     * Interpret received message
     * @param std::shared_ptr<ChannelSocketInterface>& socket
     */
    void receiveMessage(const std::shared_ptr<ChannelSocketInterface>& socket);

    /**
     * Add list of nodes to the known nodes list
     * @param asio::error_code& ec
     * @param shared_ptr<ChannelSocketInterface>& socket
     * @param NodeId node
     */
    void resetNodeExpiry(const asio::error_code& ec,
                         const std::shared_ptr<ChannelSocketInterface>& socket,
                         NodeId node = {});

    /**
     * Try to establich connexion with specific node
     * @param NodeId nodeId
     */
    void tryConnect(const NodeId& nodeId);

    void removeNodeInternal(const NodeId& nodeId);

    const NodeId id_;
    bool isMobile_ {false};
    mutable std::mt19937_64 rd;
    mutable std::mutex mutex;
    RoutingTable routing_table;

    std::atomic_bool isShutdown_ {false};

    OnConnectionChanged onConnectionChanged_ {};
};

} // namespace jami
