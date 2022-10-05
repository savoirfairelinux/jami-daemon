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

namespace jami {

class SwarmManager
{
    using ChannelCb = std::function<bool(const std::shared_ptr<ChannelSocket>&)>;
    using NeedSocketCb = std::function<void(const std::string&, ChannelCb&&)>;

public:
    SwarmManager(const NodeId&);
    NeedSocketCb needSocketCb_;

private:
    void setKnownNodes(const std::vector<NodeId>& known_nodes);
    void addKnownNodes(const NodeId& nodeId);
    void sendNodesRequest(const std::shared_ptr<ChannelSocket>& socket);
    void sendNodesAnswer(const std::shared_ptr<ChannelSocket>& socket);
    void receiveMessage();
    void maintainBuckets();
    void tryConnect(const NodeId& nodeId);

    NodeId myId;
    RoutingTable routing_table;
};

} // namespace jami
