/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#include "jamidht/jamiaccount.h"

namespace jami {

class SyncModule
{
public:
    SyncModule(std::weak_ptr<JamiAccount>&& account);
    ~SyncModule() = default;

    /**
     * Store a new Sync connection
     * @param socket    The new sync channel
     * @param peerId    The contact who owns the device
     * @param deviceId  Device linked to that transport
     */
    void cacheSyncConnection(std::shared_ptr<ChannelSocket>&& socket,
                             const std::string& peerId,
                             const DeviceId& deviceId);

    /**
     * Send sync informations to connected device
     * @param deviceId      Connected device
     * @param socket        Related socket
     * @param syncMsg       Default message
     */
    void syncWith(const DeviceId& deviceId,
                  const std::shared_ptr<ChannelSocket>& socket,
                  const std::shared_ptr<SyncMsg>& syncMsg = nullptr);

    /**
     * Send sync to all connected devices
     * @param syncMsg       Default message
     */
    void syncWithConnected(const std::shared_ptr<SyncMsg>& syncMsg = nullptr);

private:
    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami