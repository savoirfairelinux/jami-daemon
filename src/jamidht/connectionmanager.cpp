/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "connectionmanager.h"
#include "logger.h"

namespace jami
{

ConnectionManager::ConnectionManager(const std::shared_ptr<dht::DhtRunner>& dht, const std::string& deviceId)
: dht_(dht)
{
    auto key = dht::InfoHash::get("inbox:" + deviceId);
    dht_->listen<PeerConnectionRequest>(
        dht::InfoHash::get("inbox:"+deviceId),
        [](PeerConnectionRequest&& sync) {
            JAMI_ERR("NEW CONNECTION REQUEST");
            return true;
        });
}


void
ConnectionManager::connectDevice(const std::string& deviceId, const std::string& uri /* OnConnect */)
{
    if (!dht_) return;

    auto key = dht::InfoHash::get("inbox:" + deviceId);
    auto device = dht::InfoHash::get(deviceId);
    PeerConnectionRequest val;
    val.device = device;
    val.uri = uri;

    dht_->putEncrypted(key, device, std::move(val),
        [](bool ok) {
            JAMI_ERR("@@@ PUT ok: %i", ok);
        });
}

}
