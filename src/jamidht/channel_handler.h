/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include <dhtnet/multiplexed_socket.h>

namespace jami {

using DeviceId = dht::PkId;
using ConnectCb = std::function<void(std::shared_ptr<dhtnet::ChannelSocket>, const DeviceId&)>;

/**
 * A Channel handler is used to make the link between JamiAccount and ConnectionManager
 * Its role is to manage channels for a protocol (git/sip/etc)
 */
class ChannelHandlerInterface
{
public:
    virtual ~ChannelHandlerInterface() {};

    /**
     * Ask for a new channel
     * @param deviceId      The device to connect
     * @param name          The name of the channel
     * @param cb            The callback to call when connected (can be immediate if already connected)
     * @param connectionType  The connection type used by iOS notifications (not used)
     * @param forceNewConnection  If we want a new SIP connection (not used)
     */
    virtual void connect(const DeviceId& deviceId,
                         const std::string& name,
                         ConnectCb&& cb,
                         const std::string& connectionType = "",
                         bool forceNewConnection = false)
        = 0;

    /**
     * Determine if we accept or not the request. Called when ConnectionManager receives a request
     * @param peer          Peer who asked
     * @param name          The name of the channel
     * @return if we accept or not
     */
    virtual bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer,
                           const std::string& name)
        = 0;

    /**
     * Called when ConnectionManager has a new channel ready
     * @param peer          Connected peer
     * @param name          The name of the channel
     * @param channel       Channel to handle
     */
    virtual void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                         const std::string& name,
                         std::shared_ptr<dhtnet::ChannelSocket> channel)
        = 0;
};

} // namespace jami
