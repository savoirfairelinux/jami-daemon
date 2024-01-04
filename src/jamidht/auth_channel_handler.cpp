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
// #include <opendht/crypto.h>

// static constexpr const char AUTH_URI[] {"auth://"};

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
AuthChannelHandler::connect(const DeviceId& deviceId, const std::string& channelPath, ConnectCb&& cb)
{
    // auto channelPath = AUTH_URI + deviceId.toString();
    JAMI_DEBUG("[AuthChannel {}] (legacy) channelPath = {}", deviceId.toString(), channelPath);
    JAMI_DEBUG("[AuthChannel {}] connecting to channelPath = {}", deviceId.toString(), channelPath);
    connectionManager_.connectDevice(deviceId,
                                     channelPath,
                                     std::move(cb));
}

void
AuthChannelHandler::connect(const dht::InfoHash& infoHash, const std::string& channelPath, ConnectCallbackLegacy&& cb)
{
    // const std::string channelPath = AUTH_URI + infoHash.toString();
    // JAMI_DEBUG("[AuthChannel {}] channelPath = {}", infoHash.toString(), channelPath);
    // TODO check if this is needed & reimplement if necessary
    // it is probably DEPRECATED
    // TODO implement connectionmanager::isConnecting && connectionmanager::connectDevice for InfoHash instead of DeviceId data type
    // if (connectionManager_.isConnecting(infoHash, channelPath)) {
    //     JAMI_INFO("Already connecting to %s", infoHash.to_c_str());
    //     return;
    // }

    JAMI_DEBUG("[AuthChannel {}] connecting to channelPath = {}", infoHash.toString(), channelPath);
    connectionManager_.connectDevice(infoHash,
                                     channelPath,
                                     std::move(cb));
                                     // TODO make this a ConnectCallbackLegacy and stuff
}


bool
AuthChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                              const std::string& name)
{
    JAMI_DEBUG("[AuthChannel] New auth channel requested for `{}`.", cert->getId().toString());
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;

    // JAMI_DEBUG("[AuthChannel {}]", acc->getAccountID());
    JAMI_DEBUG("[AuthChannel] New auth channel requested with name = `{}`.", name);

    if (auto acc = account_.lock())
        return true;

    return false;
}

void
AuthChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                            const std::string& deviceId,
                            std::shared_ptr<dhtnet::ChannelSocket> channel) {

    JAMI_DBG("[AuthChannel] Auth channel ready.");
    /*auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return;
    if (!channel)
        JAMI_ERR("[AuthChannel] ChannelSocket invalid!");



    if (auto archiveManager = std::static_pointer_cast<ArchiveAccountManager>(acc->accountManager())) {
        archiveManager->onAuthReady(deviceId, std::move(channel));
        // manager->onAuthReady(deviceId, std::move(channel));
    }*/
    // acc->accountManager().get()->onAuthReady(deviceId, std::move(channel));

    // acc->accountManager()->onAuthReady(deviceId, std::move(channel));
    // acc->connectionManager_->linkDevCtx->channel = channel;
}

} // namespace jami
