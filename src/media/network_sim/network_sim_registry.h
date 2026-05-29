/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "network_simulator.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jami {

/**
 * Global registry for network simulators.
 *
 * Allows test code to attach NetworkSimulator instances to specific
 * media streams (identified by call ID + stream type) without modifying
 * the core media pipeline code beyond the SocketPair integration point.
 *
 * Tests must explicitly wire simulators to SocketPairs via:
 *   socketPair->setNetworkSimulator(sim);
 *
 * Usage from tests:
 *   auto& reg = NetworkSimRegistry::instance();
 *   auto sim = reg.getOrCreate("call-id", "audio");
 *   sim->setEnabled(true);
 *   sim->setPacketLoss(0.1f);
 *   socketPair->setNetworkSimulator(sim);
 *   // ... run call ...
 *   auto stats = sim->getStats();
 */
class NetworkSimRegistry
{
public:
    static NetworkSimRegistry& instance()
    {
        static NetworkSimRegistry inst;
        return inst;
    }

    /**
     * Get or create a simulator for the given call/stream.
     * @param callId The call identifier.
     * @param streamType "audio" or "video".
     */
    std::shared_ptr<NetworkSimulator> getOrCreate(const std::string& callId, const std::string& streamType)
    {
        std::lock_guard lock(mutex_);
        Key key {callId, streamType};
        auto it = simulators_.find(key);
        if (it != simulators_.end())
            return it->second;
        auto sim = std::make_shared<NetworkSimulator>();
        simulators_[key] = sim;
        return sim;
    }

    /**
     * Get existing simulator, or nullptr if none registered.
     */
    std::shared_ptr<NetworkSimulator> get(const std::string& callId, const std::string& streamType) const
    {
        std::lock_guard lock(mutex_);
        Key key {callId, streamType};
        auto it = simulators_.find(key);
        return (it != simulators_.end()) ? it->second : nullptr;
    }

    /**
     * Remove simulator for the given call/stream.
     */
    void remove(const std::string& callId, const std::string& streamType)
    {
        std::lock_guard lock(mutex_);
        simulators_.erase(Key {callId, streamType});
    }

    /**
     * Remove all simulators for a given call (both audio and video).
     */
    void removeAll(const std::string& callId)
    {
        std::lock_guard lock(mutex_);
        simulators_.erase(Key {callId, "audio"});
        simulators_.erase(Key {callId, "video"});
    }

    /**
     * Clear all registered simulators.
     */
    void clear()
    {
        std::lock_guard lock(mutex_);
        simulators_.clear();
    }

    /**
     * Iterate over all registered simulators.
     */
    void forEach(std::function<void(const std::string&, const std::string&, std::shared_ptr<NetworkSimulator>)> cb) const
    {
        std::vector<std::pair<Key, std::shared_ptr<NetworkSimulator>>> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot.reserve(simulators_.size());
            for (auto& [key, sim] : simulators_)
                snapshot.emplace_back(key, sim);
        }
        for (auto& [key, sim] : snapshot)
            cb(key.first, key.second, sim);
    }

private:
    NetworkSimRegistry() = default;

    using Key = std::pair<std::string, std::string>;
    struct KeyHash
    {
        std::size_t operator()(const Key& k) const noexcept
        {
            auto h1 = std::hash<std::string> {}(k.first);
            auto h2 = std::hash<std::string> {}(k.second);
            return h1 ^ (h2 << 1);
        }
    };

    mutable std::mutex mutex_;
    std::unordered_map<Key, std::shared_ptr<NetworkSimulator>, KeyHash> simulators_;
};

} // namespace jami
