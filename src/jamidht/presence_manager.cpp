/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
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
    return it != trackedBuddies_.end() && !it->second.deviceValues.empty();
}

std::map<std::string, bool>
PresenceManager::getTrackedBuddyPresence() const
{
    std::lock_guard lock(mutex_);
    std::map<std::string, bool> presence_info;
    for (const auto& [h, buddy] : trackedBuddies_) {
        presence_info.emplace(h.toString(), !buddy.deviceValues.empty());
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
    std::vector<dht::PkId> devices;
    devices.reserve(it->second.deviceValues.size());
    for (const auto& [deviceId, valueIds] : it->second.deviceValues)
        devices.emplace_back(deviceId);
    return devices;
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
        buddy.listenToken = {};
        buddy.deviceValues.clear();
        trackPresence(h, buddy);
    }
}

void
PresenceManager::trackPresence(const dht::InfoHash& h, TrackedBuddy& buddy)
{
    if (!dht_ || !dht_->isRunning())
        return;

    if (buddy.listenToken.valid()) {
        JAMI_ERROR("PresenceManager: Already tracking presence for {}", h.toString());
        return;
    }

    buddy.listenToken = dht_->listen(
        h,
        [this, h](const std::vector<std::shared_ptr<dht::Value>>& values, bool expired) {
            // A contact device can be advertised by more than one DeviceAnnouncement
            // value at the same time (e.g. a freshly re-generated announcement coexisting
            // with a previous one that is still living out its DHT TTL). All of them
            // decode to the same device id. We therefore reference-count, per device, the
            // set of live announcement value ids: a device stays present as long as at
            // least one of its announcement values is alive, and only goes offline once
            // the last one expires. Toggling the device on every single value expiry would
            // mark the contact offline while a healthy announcement is still on the DHT.
            std::vector<std::pair<dht::PkId, bool>> deviceChanges;
            bool wasConnected, isConnected;
            {
                std::lock_guard lock(mutex_);
                auto it = trackedBuddies_.find(h);
                if (it == trackedBuddies_.end())
                    return true;

                auto& deviceValues = it->second.deviceValues;
                wasConnected = !deviceValues.empty();

                for (const auto& value : values) {
                    try {
                        auto dev = dht::Value::unpack<DeviceAnnouncement>(*value);
                        if (!dev.pk) {
                            JAMI_WARNING("PresenceManager: Received DeviceAnnouncement without public "
                                         "key for {}",
                                         h.toString());
                            continue;
                        }
                        const auto& deviceId = dev.pk->getLongId();
                        if (expired) {
                            auto dit = deviceValues.find(deviceId);
                            if (dit != deviceValues.end()) {
                                dit->second.erase(value->id);
                                if (dit->second.empty()) {
                                    deviceValues.erase(dit);
                                    deviceChanges.emplace_back(deviceId, false);
                                }
                            }
                        } else {
                            auto& ids = deviceValues[deviceId];
                            bool deviceWasAbsent = ids.empty();
                            ids.insert(value->id);
                            if (deviceWasAbsent)
                                deviceChanges.emplace_back(deviceId, true);
                        }
                    } catch (const std::exception& e) {
                        JAMI_WARNING("PresenceManager: Failed to decode DeviceAnnouncement {} for {}: {}",
                                     value->id,
                                     h.toString(),
                                     e.what());
                        continue;
                    }
                }

                isConnected = !deviceValues.empty();
            }

            for (const auto& [deviceId, deviceOnline] : deviceChanges)
                notifyDeviceListeners(h.toString(), deviceId, deviceOnline);

            if (isConnected != wasConnected) {
                notifyListeners(h.toString(), isConnected);
            }
            return true;
        },
        dht::getFilterSet<DeviceAnnouncement>());
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
