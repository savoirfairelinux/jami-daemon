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

#include "logger.h"

namespace jami {

class Bucket {
public:
    Bucket() {}
    Bucket(const dht::InfoHash& f = {}) : first(f) {}

    std::map<dht::InfoHash, std::shared_ptr<ChannelSocket>> peers;
    dht::InfoHash first {};
    std::pair<dht::InfoHash, std::shared_ptr<ChannelSocket>> cached {"", nullptr}; // Likely to be a candidate
};

// TODO explain
class RoutingManager::Impl : public std::list<Bucket> {
public:
    using std::list<Bucket>::list;

    explicit Impl(const dht::InfoHash& baseUri, int bucketMaxSize)
        : baseUri_(baseUri), bucketMaxSize_(bucketMaxSize) {}
    ~Impl() {}

    unsigned depth(const const_iterator& it) const;

    iterator findBucket(const dht::InfoHash& h);
    const_iterator findBucket(const dht::InfoHash& h) const;

    dht::InfoHash middle(const const_iterator& it) const;

    void refreshClosest();

    bool split(const iterator& b);

    dht::InfoHash baseUri_;
    std::vector<dht::InfoHash> closestPeers_;
    unsigned closestPeersSize_ {4}; // TODO configure + mutex on vector
    int bucketMaxSize_ {};
    onPeerChangeCb changeCb_;
};

unsigned
RoutingManager::Impl::depth(const RoutingManager::Impl::const_iterator& it) const
{
    if (it == end())
        return 0;
    int bit1 = it->first.lowbit();
    int bit2 = std::next(it) != end() ? std::next(it)->first.lowbit() : -1;
    return std::max(bit1, bit2)+1;
}

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
RoutingManager::Impl::findBucket(const dht::InfoHash& h) const
{
    // Avoid code duplication for the const version
    const_iterator it = const_cast<RoutingManager::Impl*>(this)->findBucket(h);
    return it;
}

dht::InfoHash
RoutingManager::Impl::middle(const RoutingManager::Impl::const_iterator& it) const
{
    unsigned bit = depth(it);
    if (bit >= 8*HASH_LEN)
        throw std::out_of_range("End of table");

    dht::InfoHash id = it->first;
    id.setBit(bit, true);
    return id;
}

void
RoutingManager::Impl::refreshClosest()
{
    std::vector<dht::InfoHash> closest;
    closest.reserve(closestPeersSize_);
    auto bucket = findBucket(baseUri_);
    if (bucket == end()) {
        closestPeers_ = closest;
        return;
    }

    auto sortedBucketInsert = [&](const Bucket &b) {
        for (auto p : b.peers) {
            auto here = std::find_if(closest.begin(), closest.end(),
                [&](dht::InfoHash& h) {
                    return baseUri_.xorCmp(p.first, h) < 0;
                }
            );
            closest.insert(here, p.first);
        }
    };

    auto itn = bucket;
    auto itp = (bucket == begin()) ? end() : std::prev(bucket);
    while (closest.size() < closestPeersSize_ && (itn != end() || itp != end())) {
        if (itn != end()) {
            sortedBucketInsert(*itn);
            itn = std::next(itn);
        }
        if (itp != end()) {
            sortedBucketInsert(*itp);
            itp = (itp == begin()) ? end() : std::prev(itp);
        }
    }

    if (closest.size() > closestPeersSize_)
        closest.resize(closestPeersSize_);

    closestPeers_ = closest;
}

bool
RoutingManager::Impl::split(const RoutingManager::Impl::iterator& b)
{
    dht::InfoHash new_id;
    try {
        new_id = middle(b);
    } catch (const std::out_of_range& e) {
        return false;
    }

    // Insert new bucket
    insert(std::next(b), Bucket {new_id});

    // Re-assign nodes
    // TODO cpp 17, extract
    std::map<dht::InfoHash, std::shared_ptr<ChannelSocket>> peers;
    peers.insert(std::make_move_iterator(std::begin(b->peers)),
                 std::make_move_iterator(std::end(b->peers)));
    b->peers.clear();
    while (!peers.empty()) {
        auto p = peers.begin();
        auto b = findBucket(p->first);
        if (b == end())
            peers.erase(p);
        else {
            // TODO cpp 17, extract
            b->peers.emplace(p->first, p->second);
            peers.erase(p);
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////

RoutingManager::RoutingManager(const dht::InfoHash& baseUri, int bucketMaxSize)
    : pimpl_ { new Impl { baseUri, bucketMaxSize } }
{
    pimpl_->emplace_back(Bucket(baseUri));
}

RoutingManager::~RoutingManager() = default;

void
RoutingManager::injectPeers(const dht::InfoHash& uri, std::shared_ptr<ChannelSocket> socket)
{
    // Check if a bucket is available
    auto b = pimpl_->findBucket(uri);
    if (b == pimpl_->end()) return;

    // Check if peer is not present
    for (auto& p : b->peers) {
        if (p.first == uri)
            return;
    }

    if (b->peers.size() >= pimpl_->bucketMaxSize_) {
        // Split or cache for later
        if (pimpl_->depth(b) < 6) {
            pimpl_->split(b);
            return injectPeers(uri, socket);
        }
        if (!b->cached.second) {
            b->cached = std::make_pair<dht::InfoHash, std::shared_ptr<ChannelSocket>>(dht::InfoHash(uri), std::move(socket));
        }
        return;
    } else {
        b->peers[uri] = std::move(socket);
    }

    pimpl_->refreshClosest();
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
    for (auto& b : *pimpl_) {
        b.peers.erase(uri);
        if (b.cached.first == uri) {
            b.cached = std::make_pair<dht::InfoHash, std::shared_ptr<ChannelSocket>>(dht::InfoHash(), nullptr);
        }
    }
    pimpl_->refreshClosest();
}

void
RoutingManager::removePeers(const std::vector<dht::InfoHash>& uris)
{
    for (const auto& uri : uris) {
        removePeers(uri);
    }
}

std::vector<dht::InfoHash>
RoutingManager::getClosestPeers()
{
    return pimpl_->closestPeers_;
}

void
RoutingManager::onPeerChange(onPeerChangeCb&& cb)
{
    pimpl_->changeCb_ = std::move(cb);
}

}