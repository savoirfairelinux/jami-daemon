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

#include "routing_table.h"
#include "swarm_protocol.h"

#include <iostream>
#include <memory>

namespace jami {

using namespace swarm_protocol;

class SwarmManager : public std::enable_shared_from_this<SwarmManager>
{
    using ChannelCb = std::function<bool(const std::shared_ptr<ChannelSocket>&)>;
    using NeedSocketCb = std::function<void(const std::string&, ChannelCb&&)>;

public:
    SwarmManager(const NodeId&);
    NeedSocketCb needSocketCb_;

    std::weak_ptr<SwarmManager> weak()
    {
        return std::static_pointer_cast<SwarmManager>(shared_from_this());
    }

    NodeId getMyId() const { return myId; }
    /**
     * Add list of nodes to the known nodes list
     * @param std::vector<NodeId>& known_nodes
     */
    void setKnownNodes(const std::vector<NodeId>& known_nodes);

    void addChannel(const std::shared_ptr<ChannelSocket>& channel);

    void resetNodeExpiry(const asio::error_code& ec, const std::shared_ptr<ChannelSocket>& socket);

    const RoutingTable& getRoutingTable() const { return routing_table; }

private:
    /**
     * Add node to the known nodes list
     * @param NodeId nodeId
     */
    void addKnownNodes(const NodeId& nodeId);

    /**
     * Send nodes request to fill known_nodes list
     * @param NodeId nodeId
     */
    void sendRequest(const std::shared_ptr<ChannelSocket>& socket,
                     NodeId& nodeId,
                     Query q,
                     int numberNodes = Bucket::BUCKET_MAX_SIZE);

    /**
     * Send answer to request
     * @param std::shared_ptr<ChannelSocket>& socket
     * @param Message msg
     */
    void sendAnswer(const std::shared_ptr<ChannelSocket>& socket, const Message& msg_);

    /**
     * Interpret received message
     * @param std::shared_ptr<ChannelSocket>& socket
     */
    void receiveMessage(const std::shared_ptr<ChannelSocket>& socket);

    /**
     * Maintain/Update buckets
     */
    void maintainBuckets();

    /**
     * Try to establich connexion with specific node
     * @param NodeId nodeId
     */
    void tryConnect(const NodeId& nodeId);

    NodeId myId;
    RoutingTable routing_table;
};

} // namespace jami
