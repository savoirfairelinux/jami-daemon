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

#include "jamidht/sync_channel_handler.h"

static constexpr const char SYNC_URI[] {"sync://"};

namespace jami {

SyncChannelHandler::SyncChannelHandler(std::weak_ptr<JamiAccount>&& acc, ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

SyncChannelHandler::~SyncChannelHandler() {}

void
SyncChannelHandler::connect(const DeviceId& deviceId, const std::string&, ConnectCb&& cb)
{
    auto channelName = SYNC_URI + deviceId.toString();
    if (connectionManager_.isConnecting(deviceId, channelName)) {
        JAMI_INFO("Already connecting to %s", deviceId.to_c_str());
        return;
    }
    connectionManager_.connectDevice(deviceId,
                                     channelName,
                                     [cb = std::move(cb)](std::shared_ptr<ChannelSocket> socket,
                                                          const DeviceId& dev) {
                                         if (cb)
                                             cb(socket, dev);
                                     });
}

bool
SyncChannelHandler::onRequest(const DeviceId& deviceId, const std::string& name)
{
    auto cert = tls::CertificateStore::instance().getCertificate(deviceId.toString());
    auto acc = account_.lock();
    if (!cert || !acc)
        return false;
    return cert->getIssuerUID() == acc->getUsername();
}

void
SyncChannelHandler::onReady(const DeviceId& deviceId,
                            const std::string&,
                            std::shared_ptr<ChannelSocket> channel)
{
    auto cert = tls::CertificateStore::instance().getCertificate(deviceId.toString());
    auto acc = account_.lock();
    if (!cert || !acc || !acc->syncModule())
        return;
    acc->syncModule()->cacheSyncConnection(std::move(channel), cert->getIssuerUID(), deviceId);
}

} // namespace jami