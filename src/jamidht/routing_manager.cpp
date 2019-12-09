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
#include "routing_manager.h"

namespace jami {

class RoutingManager::Impl {
public:
    explicit Impl() {}
    ~Impl() {}

};

RoutingManager::RoutingManager(const dht::InfoHash& baseUri, int bucketMaxSize)
    : pimpl_ { new Impl { } }
{

}

RoutingManager::~RoutingManager()
{

}

void
RoutingManager::injectPeers(const dht::InfoHash& uri, std::shared_ptr<ChannelSocket> socket)
{

}

void
RoutingManager::injectPeers(const std::map<dht::InfoHash, std::shared_ptr<ChannelSocket>>& peers)
{

}

void
RoutingManager::removePeers(const dht::InfoHash& uri)
{

}

void
RoutingManager::removePeers(const std::vector<dht::InfoHash>& uris)
{

}

std::vector<dht::InfoHash>
RoutingManager::getPeersToConnect()
{
    return {};
}

void
RoutingManager::onPeerChange(onPeerChangeCb&& cb)
{

}

}