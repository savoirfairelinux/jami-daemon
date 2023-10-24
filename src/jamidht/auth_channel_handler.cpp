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
#include "archive_account_manager.h"

#include <opendht/thread_pool.h>

static constexpr const char AUTH_URI[] {"auth://"};

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

// KESS reference sync_module.cpp for help
void
AuthChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                            const std::string& deviceId,
                            std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return;

    if (auto manager = dynamic_cast<ArchiveAccountManager*>(acc->accountManager())) {
        manager->onAuthReady(deviceId, std::move(channel));
    }

}

} // namespace jami
