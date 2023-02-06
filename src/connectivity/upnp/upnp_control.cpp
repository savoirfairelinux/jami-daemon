/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

#include "upnp_control.h"

namespace jami {
namespace upnp {

Controller::Controller()
{
    try {
        upnpContext_ = UPnPContext::getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }

    assert(upnpContext_);
    upnpContext_->registerController(this);

    JAMI_DBG("Controller@%p: Created UPnP Controller session", this);
}

Controller::~Controller()
{
    JAMI_DBG("Controller@%p: Destroying UPnP Controller session", this);

    releaseAllMappings();
    upnpContext_->unregisterController(this);
}

void
Controller::setPublicAddress(const IpAddr& addr)
{
    assert(upnpContext_);

    if (addr and addr.getFamily() == AF_INET) {
        upnpContext_->setPublicAddress(addr);
    }
}

bool
Controller::isReady() const
{
    assert(upnpContext_);
    return upnpContext_->isReady();
}

IpAddr
Controller::getExternalIP() const
{
    assert(upnpContext_);
    if (upnpContext_->isReady()) {
        return upnpContext_->getExternalIP();
    }
    return {};
}

Mapping::sharedPtr_t
Controller::reserveMapping(uint16_t port, PortType type)
{
    Mapping map(type, port, port);
    return reserveMapping(map);
}

Mapping::sharedPtr_t
Controller::reserveMapping(Mapping& requestedMap)
{
    assert(upnpContext_);

    // Try to get a provisioned port
    auto mapRes = upnpContext_->reserveMapping(requestedMap);
    if (mapRes)
        addLocalMap(*mapRes);
    return mapRes;
}

void
Controller::releaseMapping(const Mapping& map)
{
    assert(upnpContext_);

    removeLocalMap(map);
    return upnpContext_->releaseMapping(map);
}

void
Controller::releaseAllMappings()
{
    assert(upnpContext_);

    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto const& [_, map] : mappingList_) {
        upnpContext_->releaseMapping(map);
    }
    mappingList_.clear();
}

void
Controller::addLocalMap(const Mapping& map)
{
    if (map.getMapKey()) {
        std::lock_guard<std::mutex> lock(mapListMutex_);
        auto ret = mappingList_.emplace(map.getMapKey(), map);
        if (not ret.second) {
            JAMI_WARN("Mapping request for %s already in the list!", map.toString().c_str());
        }
    }
}

bool
Controller::removeLocalMap(const Mapping& map)
{
    assert(upnpContext_);

    std::lock_guard<std::mutex> lk(mapListMutex_);
    if (mappingList_.erase(map.getMapKey()) != 1) {
        JAMI_ERR("Failed to remove mapping %s from local list", map.getTypeStr());
        return false;
    }

    return true;
}

uint16_t
Controller::generateRandomPort(PortType type)
{
    return UPnPContext::generateRandomPort(type);
}

} // namespace upnp
} // namespace jami
