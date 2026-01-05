/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include <opendht/dhtrunner.h>
#include <opendht/infohash.h>

#include <map>
#include <set>
#include <mutex>
#include <string>
#include <functional>
#include <vector>
#include <future>
#include <atomic>

namespace jami {

class PresenceManager
{
public:
    using PresenceCallback = std::function<void(const std::string& uri, bool online)>;
    using DevicePresenceCallback = std::function<void(const std::string& uri, const dht::PkId& deviceId, bool online)>;

    PresenceManager(const std::shared_ptr<dht::DhtRunner>& dht);
    ~PresenceManager();

    /**
     * Start tracking a buddy.
     * Increments the reference count for the given URI.
     * If the buddy was not tracked, it starts listening on the DHT.
     */
    void trackBuddy(const std::string& uri);

    /**
     * Stop tracking a buddy.
     * Decrements the reference count.
     * If the count reaches zero, stops listening on the DHT.
     */
    void untrackBuddy(const std::string& uri);

    /**
     * Check if a buddy is currently online.
     */
    bool isOnline(const std::string& uri) const;

    /**
     * Get the presence status of all tracked buddies.
     */
    std::map<std::string, bool> getTrackedBuddyPresence() const;

    /**
     * Get the list of devices for a tracked buddy.
     */
    std::vector<dht::PkId> getDevices(const std::string& uri) const;

    /**
     * Add a listener for presence changes.
     * The callback will be called whenever a tracked buddy goes online or offline.
     * @return A token that can be used to remove the listener.
     */
    uint64_t addListener(PresenceCallback cb);

    /**
     * Remove a listener using the token returned by addListener.
     */

    void removeListener(uint64_t token);

    /**
     * Add a listener for device presence changes.
     * The callback will be called whenever a device of a tracked buddy goes online or offline.
     * @return A token that can be used to remove the listener.
     */
    uint64_t addDeviceListener(DevicePresenceCallback cb);

    /**
     * Remove a listener using the token returned by addDeviceListener.
     */
    void removeDeviceListener(uint64_t token);

    /**
     * Refresh all tracked buddies.
     * This should be called when the DHT is restarted.
     */
    void refresh();

private:
    struct TrackedBuddy
    {
        dht::InfoHash id;
        std::set<dht::PkId> devices;
        std::future<size_t> listenToken;
        int refCount {0};

        TrackedBuddy(dht::InfoHash h)
            : id(h)
        {}
    };

    void trackPresence(const dht::InfoHash& h, TrackedBuddy& buddy);
    void notifyListeners(const std::string& uri, bool online);
    void notifyDeviceListeners(const std::string& uri, const dht::PkId& deviceId, bool online);

    std::shared_ptr<dht::DhtRunner> dht_;

    mutable std::mutex mutex_;
    std::map<dht::InfoHash, TrackedBuddy> trackedBuddies_;

    std::mutex listenersMutex_;
    std::map<uint64_t, PresenceCallback> listeners_;
    std::map<uint64_t, DevicePresenceCallback> deviceListeners_;
    std::atomic_uint64_t nextListenerId_ {1};
};

} // namespace jami
