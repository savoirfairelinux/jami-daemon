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

#include "jamidht/channel_handler.h"
#include "jamidht/jamiaccount.h"
#include <dhtnet/connectionmanager.h>

namespace jami {

/**
 * Manages Conversation's channels
 */
class TransferChannelHandler : public ChannelHandlerInterface
{
public:
    TransferChannelHandler(const std::shared_ptr<JamiAccount>& acc, dhtnet::ConnectionManager& cm);
    ~TransferChannelHandler();

    /**
     * Ask for a new channel
     * This replaces the connectDevice() in jamiaccount
     * @param deviceId      The device to connect
     * @param channelName   The name of the channel
     * @param cb            The callback to call when connected (can be immediate if already connected)
     * @param connectionType  The connection type used by iOS notifications (not used)
     * @param forceNewConnection  If we want a new SIP connection (not used)
     */
    void connect(const DeviceId& deviceId,
                 const std::string& channelName,
                 ConnectCb&& cb,
                 const std::string& connectionType = "",
                 bool forceNewConnection = false) override;

    /**
     * Determine if we accept or not the request
     * @param deviceId      device who asked
     * @param name          name asked
     * @return if we accept or not
     */
    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer, const std::string& name) override;

    /**
     * Handle socket ready
     * @param deviceId      Related device
     * @param name          Name of the handler
     * @param channel       Channel to handle
     */
    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<dhtnet::ChannelSocket> channel) override;

private:
    std::weak_ptr<JamiAccount> account_;
    dhtnet::ConnectionManager& connectionManager_;

    std::filesystem::path idPath_;
};

} // namespace jami
