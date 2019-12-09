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

#include <list>

namespace jami {

class Bucket {
public:
    Bucket() {}
    Bucket(const dht::InfoHash& f = {}) : first(f) {}

    std::list<std::string> peers;
    dht::InfoHash first {};
};

class RoutingManager::Impl : public std::list<Bucket> {
public:
    using std::list<Bucket>::list;

    explicit Impl() {}
    ~Impl() {}

    iterator findBucket(const dht::InfoHash& h);
    const_iterator findBucket(const dht::InfoHash& h) const;
};

RoutingManager::Impl::iterator
RoutingManager::Impl::findBucket(const dht::InfoHash& h)
{
    if (empty())
        return end();
    auto b = begin();
    while (true) {
        auto next = std::next(b);
        if (next == end())
            return b;
        if (dht::InfoHash::cmp(h, next->first) < 0)
            return b;
        b = next;
    }
}

RoutingManager::Impl::const_iterator
RoutingManager::Impl::findBucket(const InfoHash& h) const
{
    // Avoid code duplication for the const version
    const_iterator it = const_cast<RoutingManager::Impl*>(this)->findBucket(h);
    return it;
}

////////////////////////////////////////////////////////////////

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
    auto b = pimpl_->findBucket(uri);
}

void
RoutingManager::injectPeers(const std::map<dht::InfoHash, std::shared_ptr<ChannelSocket>>& peers)
{
    for (auto& it : peers) {
        injectPeers(it.first, it.second);
    }
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