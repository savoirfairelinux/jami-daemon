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

#include "jamidht/channel_handler.h"
#include "connectivity/connectionmanager.h"
#include "jamidht/jamiaccount.h"

namespace jami {

/**
 * Manages channels for syncing informations
 */
class SyncChannelHandler : public ChannelHandlerInterface
{
public:
    SyncChannelHandler(const std::shared_ptr<JamiAccount>& acc, ConnectionManager& cm);
    ~SyncChannelHandler();

    /**
     * Ask for a new sync channel
     * @param deviceId      The device to connect
     * @param name          (Unused, generated from deviceId)
     * @param cb            The callback to call when connected (can be immediate if already connected)
     */
    void connect(const DeviceId& deviceId, const std::string&, ConnectCb&& cb) override;

    /**
     * Determine if we accept or not the sync request
     * @param deviceId      Device who asked
     * @param name          Name asked
     * @return if the channel is for a valid conversation and device not banned
     */
    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer, const std::string& name) override;

    /**
     * Launch sync process
     * @param deviceId      Device who asked
     * @param name          Name asked
     * @param channel       Channel used to sync
     */
    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<ChannelSocket> channel) override;

private:
    std::weak_ptr<JamiAccount> account_;
    ConnectionManager& connectionManager_;
};

} // namespace jami