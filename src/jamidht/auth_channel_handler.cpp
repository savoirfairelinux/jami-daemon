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
#include "jamidht/auth_channel_handler.h"
#include "archive_account_manager.h"

#include <opendht/thread_pool.h>

namespace jami {

AuthChannelHandler::AuthChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                       dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

AuthChannelHandler::~AuthChannelHandler() {}

// deprecated
void
AuthChannelHandler::connect(const DeviceId& deviceId,
                            const std::string& name,
                            ConnectCb&& cb,
                            const std::string& connectionType,
                            bool forceNewConnection)
{
    JAMI_DEBUG("[AuthChannel {}] connecting to name = {}", deviceId.toString(), name);
    connectionManager_.connectDevice(deviceId, name, std::move(cb));
}

void
AuthChannelHandler::connect(const dht::InfoHash& infoHash,
                            const std::string& channelPath,
                            ConnectCallbackLegacy&& cb)
{
    JAMI_DEBUG("[AuthChannel {}] connecting to channelPath = {}", infoHash.toString(), channelPath);
    connectionManager_.connectDevice(infoHash, channelPath, std::move(cb));
}

bool
AuthChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                              const std::string& name)
{
    JAMI_DEBUG("[AuthChannel] New auth channel requested for `{}`.", cert->getId().toString());
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;

    JAMI_DEBUG("[AuthChannel] New auth channel requested with name = `{}`.", name);

    if (auto acc = account_.lock())
        return true;

    return false;
}

void
AuthChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                            const std::string& deviceId,
                            std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    JAMI_DEBUG("[AuthChannel] Auth channel with {}/{} ready.", cert->getId().toString(), deviceId);
}

} // namespace jami
