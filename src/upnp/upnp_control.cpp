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

namespace jami { namespace upnp {

Controller::Controller(bool keepCb) : keepCb_(keepCb)
{
    id_ = (uint64_t)this;
    try {
        upnpContext_ = getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
}

Controller::~Controller()
{
    JAMI_DBG("Destroying UPnP Controller %p", this);
    // Avoid to call removeLocalMap on destroy
    if (upnpContext_) {
        upnpContext_->unregisterAllCallbacks(id_);
    }

    requestAllMappingRemove(PortType::UDP);
    requestAllMappingRemove(PortType::TCP);
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

void
Controller::requestMappingAdd(NotifyServiceCallback&& cb, uint16_t portDesired, PortType type, bool unique, uint16_t portLocal)
{
    if (portLocal == 0)
        portLocal = portDesired;

    // Check if the mapping requested isn't already in the controllers internal list.
    if (isLocalMapPresent(portDesired, type)) {
        cb(portDesired, true);
        return;
    }


    // If the port mapping requested not unique, check if it's already being used.
    if (not unique) {
        if (upnpContext_->isMappingInUse(portDesired, type)) {
            upnpContext_->incrementNbOfUsers(portDesired, type);
            Mapping map(portDesired, portLocal, type, unique);
            addLocalMap(map);
            cb(portDesired, true);
            return;
        }
    }


    // Send out request.
    upnpContext_->requestMappingAdd(ControllerData {
            id_, keepCb_,
            [cb, portDesired, this](const Mapping& map, bool success) {
                cb(portDesired, success);
                if (map.isValid())
                    addLocalMap(map);
            },
            [this](const Mapping& map, bool success) {
                if (map.isValid())
                    removeLocalMap(map);
            },
            [this]() {
                // Clear local mappings in case of a connectivity changed
                std::lock_guard<std::mutex> lk(mapListMutex_);
                udpMappings_.clear();
                tcpMappings_.clear();
            }
        },
        portDesired, portLocal, type, unique);
}

bool
Controller::requestMappingRemove(uint16_t portExternal, PortType type)
{
    if (not upnpContext_) return false;
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    auto mapIt = instanceMappings.find(portExternal);
    if (mapIt != instanceMappings.end()) {
        if (!upnpContext_->requestMappingRemove(mapIt->second)) {
            // No port mapped, so the callback will not be removed,
            // remove it anyway
            upnpContext_->unregisterCallback(mapIt->second);
        }
        instanceMappings.erase(mapIt);
        return true;
    }
    return false;
}

bool
Controller::isLocalMapPresent(uint16_t portExternal, PortType type) const
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = instanceMappings.find(portExternal);
    return it != instanceMappings.end();
}

void
Controller::requestAllMappingRemove(PortType type) {
    if (not upnpContext_) return;
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (const auto& map: instanceMappings) {
        if (!upnpContext_->requestMappingRemove(map.second)) {
            // No port mapped, so the callback will not be removed,
            // remove it anyway
            upnpContext_->unregisterCallback(map.second);
        }
    }
}

void
Controller::addLocalMap(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    instanceMappings.emplace(map.getPortExternal(), Mapping(map));
}

bool
Controller::removeLocalMap(const Mapping& map)
{
    if (not upnpContext_) return false;
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = instanceMappings.find(map.getPortExternal());
    if (it != instanceMappings.end()) {
        instanceMappings.erase(it);
        return true;
    }
    return false;
}

}} // namespace jami::upnp
