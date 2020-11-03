/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

    JAMI_DBG("Controller@%p: Created UPnP Controller session", this);
}

Controller::~Controller()
{
    JAMI_DBG("Controller@%p: Destroying UPnP Controller session", this);

    releaseAllMappings();
}

bool
Controller::hasValidIGD() const
{
    return upnpContext_ and upnpContext_->hasValidIGD();
}

IpAddr
Controller::getLocalIP() const
{
    if (upnpContext_) {
        return upnpContext_->getLocalIP();
    }
    return {};
}

IpAddr
Controller::getExternalIP() const
{
    if (upnpContext_) {
        return upnpContext_->getExternalIP();
    }
    return {};
}

Mapping::sharedPtr_t
Controller::reserveMapping(uint16_t port, PortType type)
{
    Mapping map {port, port, type};
    return reserveMapping(map);
}

Mapping::sharedPtr_t
Controller::reserveMapping(Mapping& requestedMap)
{

    if (not upnpContext_)
        return 0;

    // Try to get a provisioned port
    auto mapRes = upnpContext_->reserveMapping(requestedMap);
    addLocalMap(mapRes);
    return mapRes;
}

bool
Controller::releaseMapping(const Mapping::sharedPtr_t& map)
{
    if (not upnpContext_)
        return false;

    removeLocalMap(map);
    return upnpContext_->releaseMapping(map);
}

void
Controller::releaseAllMappings()
{
    if (not upnpContext_)
        return;
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto const& [_,map] : mappingList_) {
        upnpContext_->releaseMapping(map);
    }
}

void
Controller::addLocalMap(const Mapping::sharedPtr_t& map)
{
    if (map and map->getMapKey()) {
        std::lock_guard<std::mutex> lock(mapListMutex_);
        auto ret = mappingList_.emplace(map->getMapKey(), map);
        if (not ret.second) {
            JAMI_WARN("Mapping request for %s already in the list !",
                map->toString().c_str());
        }
    }
}

bool
Controller::removeLocalMap(const Mapping::sharedPtr_t& map)
{
    if (not upnpContext_)
        return false;

    std::lock_guard<std::mutex> lk(mapListMutex_);
    if (mappingList_.erase(map->getMapKey()) != 1) {
        JAMI_ERR("Failed to remove mapping %s from local list",
            map->getTypeStr().c_str());
        return false;
    }

    return true;
}

} // namespace upnp
} // namespace jami
