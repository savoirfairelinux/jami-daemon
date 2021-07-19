/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#include "src/jamidht/multiplexed_socket.h"

namespace jami {

using ConnectCb = std::function<void(std::shared_ptr<ChannelSocket>, const DeviceId&)>;

/**
 * A Channel handler is used to make the link between JamiAccount and ConnectionManager
 * Its role is to manages channels for a protol (git/sip/etc)
 */
class ChannelHandler
{
public:
    virtual ~ChannelHandler() {};

    /**
     * Ask for a new channel
     * This replaces the connectDevice() in jamiaccount
     * @param deviceId      The device to connect
     * @param channelName   The name of the channel
     * @param cb            The callback to call when connected (can be immediate if already connected)
     */
    virtual void connect(const DeviceId& deviceId, const std::string& channelName, ConnectCb&& cb)
        = 0;

    /**
     * Determine if we accept or not the request
     * @param deviceId      device who asked
     * @param name          name asked
     * @return if we accept or not
     */
    virtual bool onRequest(const DeviceId& deviceId, const std::string& name) = 0;

    /**
     * Handle socket ready
     * @param deviceId      Related device
     * @param name          Name of the handler
     * @param channel       Channel to handle
     */
    virtual void onReady(const DeviceId& deviceId,
                         const std::string& name,
                         std::shared_ptr<ChannelSocket> channel)
        = 0;
};

} // namespace jami