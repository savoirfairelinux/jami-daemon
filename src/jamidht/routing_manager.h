/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include <vector>
#include <functional>
#include <memory>
#include <opendht/infohash.h>

namespace jami
{

class ChannelSocket;

/**
 * This callback is triggered when the RoutingManager asks for the
 * application to remove or add a new connection.
 * @param uri       New peer uri
 * @param add       If the peer should be added or removed
 */
using onPeerChangeCb = std::function<void(const dht::InfoHash& /* uri */, bool /* add */)>;

/**
 * The RoutingManager is a class to manage optimizes connections in a conversation.
 * So, the conversation will got a bootstrap channel to someone of the same conversation
 * then, this manager will exchange its routing table with the one of the peer.
 * Then, we are able to maintain connection to some peers to transmit messages for a conversation.
 */
class RoutingManager
{
public:
    RoutingManager(const dht::InfoHash& baseUri, int bucketMaxSize = 4);
    ~RoutingManager();

    /**
     * Add a peer into the routing table
     * @param uri       The peer to add
     * @param socket    Channel socket to send messages in the routing table
     */
    void injectPeers(const dht::InfoHash& uri, std::shared_ptr<ChannelSocket> socket);

    /**
     * Add some peers into the routing table
     * @param peers     Peers to add
     */
    void injectPeers(const std::map<dht::InfoHash, std::shared_ptr<ChannelSocket>>& peers);

    /**
     * Remove a peer from the routing table
     * @param uri       URI of the peer to remove
     */
    void removePeers(const dht::InfoHash& uri);

    /**
     * Remove peers from the routing table
     * @param uris      URIs to remove from the routing table
     */
    void removePeers(const std::vector<dht::InfoHash>& uris);

    /**
     * Get current peers the user needs to connect
     * @return peers to connect
     */
    std::vector<dht::InfoHash> getPeersToConnect();
    /**
     * Set a callback to detect changes in the connection to maintain
     * @param cb        Callback triggered when a connection needs to be removed or added
     */
    void onPeerChange(onPeerChangeCb&& cb);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}