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

#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>

namespace jami {

/**
 * Global registry for network simulators.
 *
 * Allows test code to attach NetworkSimulator instances to specific
 * media streams (identified by call ID + stream type) without modifying
 * the core media pipeline code beyond the SocketPair integration point.
 *
 * Usage from tests:
 *   auto& reg = NetworkSimRegistry::instance();
 *   auto sim = reg.getOrCreate("call-id", "audio");
 *   sim->setEnabled(true);
 *   sim->setPacketLoss(0.1f);
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
    std::shared_ptr<NetworkSimulator> getOrCreate(const std::string& callId,
                                                  const std::string& streamType)
    {
        std::lock_guard lock(mutex_);
        auto key = makeKey(callId, streamType);
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
    std::shared_ptr<NetworkSimulator> get(const std::string& callId,
                                          const std::string& streamType) const
    {
        std::lock_guard lock(mutex_);
        auto key = makeKey(callId, streamType);
        auto it = simulators_.find(key);
        return (it != simulators_.end()) ? it->second : nullptr;
    }

    /**
     * Remove simulator for the given call/stream.
     */
    void remove(const std::string& callId, const std::string& streamType)
    {
        std::lock_guard lock(mutex_);
        simulators_.erase(makeKey(callId, streamType));
    }

    /**
     * Remove all simulators for a given call (both audio and video).
     */
    void removeAll(const std::string& callId)
    {
        std::lock_guard lock(mutex_);
        simulators_.erase(makeKey(callId, "audio"));
        simulators_.erase(makeKey(callId, "video"));
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
    void forEach(std::function<void(const std::string&, const std::string&,
                                    std::shared_ptr<NetworkSimulator>)> cb) const
    {
        std::vector<std::tuple<std::string, std::string, std::shared_ptr<NetworkSimulator>>> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot.reserve(simulators_.size());
            for (auto& [key, sim] : simulators_) {
                auto sep = key.find(':');
                if (sep != std::string::npos)
                    snapshot.emplace_back(key.substr(0, sep), key.substr(sep + 1), sim);
            }
        }
        for (auto& [callId, streamType, sim] : snapshot)
            cb(callId, streamType, sim);
    }

private:
    NetworkSimRegistry() = default;

    static std::string makeKey(const std::string& callId, const std::string& streamType)
    {
        return callId + ":" + streamType;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<NetworkSimulator>> simulators_;
};

} // namespace jami
