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

#include "jamidht/jamiaccount.h"

namespace jami {

class SyncModule
{
public:
    SyncModule(const std::shared_ptr<JamiAccount>& account);
    ~SyncModule() = default;

    /**
     * Store a new Sync connection
     * @param socket    The new sync channel
     * @param peerId    The contact who owns the device
     * @param deviceId  Device linked to that transport
     */
    void cacheSyncConnection(std::shared_ptr<dhtnet::ChannelSocket>&& socket,
                             const std::string& peerId,
                             const DeviceId& deviceId);

    bool isConnected(const DeviceId& deviceId) const;

    /**
     * Send sync to all connected devices
     * @param syncMsg       Default message
     * @param deviceId      If we need to filter on a device
     */
    void syncWithConnected(const std::shared_ptr<SyncMsg>& syncMsg = nullptr, const DeviceId& deviceId = {});

    /**
     * Local sync-version tracking.
     *
     * The version counter and the per-device watermarks are purely local: they
     * are never transmitted on the network. They are only used to decide
     * whether a sync connection must be (re)established with a given device, so
     * that devices (especially mobiles) are not woken up when there is nothing
     * new to synchronize.
     */

    /// Increment the local sync version (to be called on any contact or
    /// conversation list change, whether local or learned from a peer) and
    /// persist it. Returns the new version.
    uint64_t bumpVersion();

    /// Current local sync version.
    uint64_t currentVersion() const;

    /// True if @p deviceId may be missing local list state, i.e. it was never
    /// synced or was last synced at an older version than the current one.
    bool needsSync(const DeviceId& deviceId) const;

    /// Record that @p deviceId has received local state up to @p version.
    void markSynced(const DeviceId& deviceId, uint64_t version);

private:
    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami