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

#include "presence_manager.h"
#include "jami_contact.h" // For DeviceAnnouncement
#include "logger.h"

namespace jami {

PresenceManager::PresenceManager(const std::shared_ptr<dht::DhtRunner>& dht)
    : dht_(dht)
{}

PresenceManager::~PresenceManager()
{
    std::lock_guard lock(mutex_);
    for (auto& [h, buddy] : trackedBuddies_) {
        if (dht_ && dht_->isRunning()) {
            dht_->cancelListen(h, std::move(buddy.listenToken));
        }
    }
}

void
PresenceManager::trackBuddy(const std::string& uri)
{
    dht::InfoHash h(uri);
    if (!h)
        return;

    std::lock_guard lock(mutex_);
    auto it = trackedBuddies_.find(h);
    if (it == trackedBuddies_.end()) {
        it = trackedBuddies_.emplace(h, TrackedBuddy {h}).first;
    }

    it->second.refCount++;
    if (it->second.refCount == 1) {
        trackPresence(h, it->second);
    }
}

void
PresenceManager::untrackBuddy(const std::string& uri)
{
    dht::InfoHash h(uri);
    if (!h)
        return;

    std::lock_guard lock(mutex_);
    auto it = trackedBuddies_.find(h);
    if (it != trackedBuddies_.end()) {
        it->second.refCount--;
        if (it->second.refCount <= 0) {
            if (dht_ && dht_->isRunning()) {
                dht_->cancelListen(h, std::move(it->second.listenToken));
            }
            trackedBuddies_.erase(it);
        }
    }
}

bool
PresenceManager::isOnline(const std::string& uri) const
{
    dht::InfoHash h(uri);
    if (!h)
        return false;
    std::lock_guard lock(mutex_);
    auto it = trackedBuddies_.find(h);
    return it != trackedBuddies_.end() && !it->second.devices.empty();
}

std::map<std::string, bool>
PresenceManager::getTrackedBuddyPresence() const
{
    std::lock_guard lock(mutex_);
    std::map<std::string, bool> presence_info;
    for (const auto& [h, buddy] : trackedBuddies_) {
        presence_info.emplace(h.toString(), !buddy.devices.empty());
    }
    return presence_info;
}

std::vector<dht::PkId>
PresenceManager::getDevices(const std::string& uri) const
{
    dht::InfoHash h(uri);
    if (!h)
        return {};
    std::lock_guard lock(mutex_);
    auto it = trackedBuddies_.find(h);
    if (it == trackedBuddies_.end())
        return {};
    return {it->second.devices.begin(), it->second.devices.end()};
}

uint64_t
PresenceManager::addListener(PresenceCallback cb)
{
    std::lock_guard lock(listenersMutex_);
    auto id = nextListenerId_++;
    listeners_.emplace(id, std::move(cb));
    return id;
}

void
PresenceManager::removeListener(uint64_t token)
{
    std::lock_guard lock(listenersMutex_);
    listeners_.erase(token);
}

uint64_t
PresenceManager::addDeviceListener(DevicePresenceCallback cb)
{
    std::lock_guard lock(listenersMutex_);
    auto id = nextListenerId_++;
    deviceListeners_.emplace(id, std::move(cb));
    return id;
}

void
PresenceManager::removeDeviceListener(uint64_t token)
{
    std::lock_guard lock(listenersMutex_);
    deviceListeners_.erase(token);
}

void
PresenceManager::refresh()
{
    std::lock_guard lock(mutex_);
    for (auto& [h, buddy] : trackedBuddies_) {
        buddy.devices.clear();
        trackPresence(h, buddy);
    }
}

void
PresenceManager::trackPresence(const dht::InfoHash& h, TrackedBuddy& buddy)
{
    if (!dht_ || !dht_->isRunning())
        return;

    buddy.listenToken = dht_->listen<DeviceAnnouncement>(h, [this, h](DeviceAnnouncement&& dev, bool expired) {
        if (!dev.pk) {
            JAMI_WARNING("PresenceManager: Received DeviceAnnouncement without public key for {}", h.toString());
            return true;
        }
        bool wasConnected, isConnected;
        auto deviceId = dev.pk->getLongId();
        bool deviceOnline = !expired;
        {
            std::lock_guard lock(mutex_);
            auto it = trackedBuddies_.find(h);
            if (it == trackedBuddies_.end())
                return true;

            wasConnected = !it->second.devices.empty();
            if (expired) {
                it->second.devices.erase(deviceId);
            } else {
                it->second.devices.insert(deviceId);
            }
            isConnected = !it->second.devices.empty();
        }

        notifyDeviceListeners(h.toString(), deviceId, deviceOnline);

        if (isConnected != wasConnected) {
            notifyListeners(h.toString(), isConnected);
        }
        return true;
    });
}

void
PresenceManager::notifyListeners(const std::string& uri, bool online)
{
    std::vector<PresenceCallback> cbs;
    {
        std::lock_guard lock(listenersMutex_);
        cbs.reserve(listeners_.size());
        for (const auto& [id, cb] : listeners_) {
            cbs.emplace_back(cb);
        }
    }
    for (const auto& cb : cbs) {
        cb(uri, online);
    }
}

void
PresenceManager::notifyDeviceListeners(const std::string& uri, const dht::PkId& deviceId, bool online)
{
    std::vector<DevicePresenceCallback> cbs;
    {
        std::lock_guard lock(listenersMutex_);
        cbs.reserve(deviceListeners_.size());
        for (const auto& [id, cb] : deviceListeners_) {
            cbs.emplace_back(cb);
        }
    }
    for (const auto& cb : cbs) {
        cb(uri, deviceId, online);
    }
}

} // namespace jami
