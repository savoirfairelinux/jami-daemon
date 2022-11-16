/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
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

#include "swarm_channel_handler.h"

namespace jami {

SwarmChannelHandler::SwarmChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                         ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

SwarmChannelHandler::~SwarmChannelHandler() {}

void
SwarmChannelHandler::connect(const DeviceId& deviceId,
                             const std::string& conversationId,
                             ConnectCb&& cb)
{
    connectionManager_.connectDevice(deviceId, fmt::format("swarm://{}", conversationId), cb);
}

bool
SwarmChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                               const std::string& name)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;

    auto sep = name.find_last_of('/');
    auto conversationId = name.substr(sep + 1);
    if (auto acc = account_.lock())
        if (auto convModule = acc->convModule()) {
            auto res = !convModule->isBannedDevice(conversationId,
                                                   cert->issuer->getLongId().toString());
            return res;
        }
    return false;
}

void
SwarmChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>&,
                             const std::string& uri,
                             std::shared_ptr<ChannelSocket> socket)
{
}
} // namespace jami