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

#include "src/jamidht/conversation_channel_handler.h"

namespace jami {

ConversationChannelHandler::ConversationChannelHandler(ConnectionManager& cm)
    : ChannelHandler()
    , connectionManager_(cm)
{}

ConversationChannelHandler::~ConversationChannelHandler() {}

void
ConversationChannelHandler::connect(const DeviceId& deviceId,
                                    const std::string& channelName,
                                    ConnectCb&& cb)
{
    connectionManager_.connectDevice(deviceId,
                                     "git://" + deviceId.toString() + "/" + channelName,
                                     [cb = std::move(cb)](std::shared_ptr<ChannelSocket> socket,
                                                          const DeviceId& dev) {
                                         if (cb)
                                             cb(socket, dev);
                                     });
}

bool
ConversationChannelHandler::onRequest(const DeviceId&, const std::string&)
{
    // Note: If there, device is already authorized via ICE
    // and we accept all git:// channels.
    return true;
}

void
ConversationChannelHandler::onReady(const DeviceId&,
                                    const std::string&,
                                    std::shared_ptr<ChannelSocket>)
{}

} // namespace jami