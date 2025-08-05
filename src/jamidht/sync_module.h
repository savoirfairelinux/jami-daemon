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
    void syncWithConnected(const std::shared_ptr<SyncMsg>& syncMsg = nullptr,
                           const DeviceId& deviceId = {});

private:
    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami