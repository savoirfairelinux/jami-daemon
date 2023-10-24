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

#include "jamidht/auth_channel_handler.h"

#include <opendht/thread_pool.h>

static constexpr const char AUTH_URI[] {"auth://"};

static const uint8_t MAX_OPEN_CHANNELS {1}; // TODO enforce this in ::connect

namespace jami {

AuthChannelHandler::AuthChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                       dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

AuthChannelHandler::~AuthChannelHandler() {}

void
AuthChannelHandler::connect(const DeviceId& deviceId, const std::string&, ConnectCb&& cb)
{
    auto channelName = AUTH_URI + deviceId.toString();
    if (connectionManager_.isConnecting(deviceId, channelName)) {
        JAMI_INFO("Already connecting to %s", deviceId.to_c_str());
        return;
    }

    // create an archive auth context to track the state

    connectionManager_.connectDevice(deviceId,
                                     channelName,
                                     std::move(cb));
}

bool
AuthChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                              const std::string& name)
{
    return false;
}

// TODO standardize here, remove from acm
struct AuthDecodingCtx
{
    msgpack::unpacker pac {
        [](msgpack::type::object_type, std::size_t, void*) { return true; },
        nullptr,
        64
    };
};

void
AuthChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                            const std::string& deviceId,
                            std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return;

    auto ldc = std::make_shared<ArhiveAccountManager::LinkDeviceContext>();
    ldc->maxOpenChannels = MAX_OPEN_CHANNELS;

    channel->setOnRecv([ldc](const uint8_t* buf, size_t len) {
        ArchiveAccountManager::onAuthRecv(ldc, buf, len);
    });
}

} // namespace jami
