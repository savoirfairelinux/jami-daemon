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

Controller::Controller(bool keepCb)
    : keepCb_(keepCb)
{
    id_ = (uint64_t) this;
    try {
        upnpContext_ = UPnPContext::getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }

    JAMI_DBG("Controller@%lu: Created UPnP Controller session", id_);
}

Controller::~Controller()
{
    JAMI_DBG("Controller@%ld: Destroying UPnP Controller session", id_);
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

uint16_t Controller::requestMappingAdd(ControllerData&& ctrlData, const Mapping& map)
{
    return upnpContext_->requestMappingAdd(std::move(ctrlData), map);
}

uint16_t
Controller::requestMappingAdd(NotifyServiceCallback&& cb, const Mapping& requestedMap)
{

    if (not upnpContext_)
        return 0;

    JAMI_DBG("Controller@%lu: Trying to find a provisioned mapping %s",
        id_, requestedMap.toString().c_str());

    // Try to get a provisioned port
    const auto map = upnpContext_->selectProvisionedMapping(requestedMap);
    if (map.isValid()) {
        JAMI_DBG("Controller@%lu: Found provisioned mapping %s",
            id_, map.toString().c_str());
        // Found an available port.
        cb(map.getPortExternal(), true);
        addLocalMap(map);
        return map.getPortExternal();
    }

    // No port available.
    // Should not get here if the ports were properly provisioned.

    JAMI_WARN("Controller@%lu: No available UPNP mapping for %s. Trying to request one now.",
        id_, requestedMap.toString().c_str());

    // Build the control data structure.
    ControllerData ctrlData {};
    ctrlData.id = id_;
    ctrlData.keepCb = keepCb_;

    std::weak_ptr<Controller> thisWkPtr = std::static_pointer_cast<Controller>(shared_from_this());
    ctrlData.onMapAdded = [cb, requestedMap, thisWkPtr](const Mapping& map, bool success) {
        if (auto controller = thisWkPtr.lock()) {
            cb(map.getPortExternal(), success);
            if (success and map.isValid())
                controller->addLocalMap(map);
        }
    };
    ctrlData.onMapRemoved = [thisWkPtr](const Mapping& map, bool) {
        if (auto controller = thisWkPtr.lock()) {
            if (map.isValid())
                controller->removeLocalMap(map);
        }
    };
    ctrlData.onConnectionChanged = [thisWkPtr]() {
        if (auto controller = thisWkPtr.lock()) {
            // Clear local mappings in case of a connectivity change
            std::lock_guard<std::mutex> lk(controller->mapListMutex_);
            controller->udpMappings_.clear();
            controller->tcpMappings_.clear();
        }
    };

    // Send the request.
    return requestMappingAdd(std::move(ctrlData), requestedMap);
}

bool
Controller::requestMappingRemove( const Mapping& map)
{
    if (not upnpContext_)
        return false;

    return upnpContext_->requestMappingRemove(map);
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
Controller::requestAllMappingRemove(PortType type)
{
    if (not upnpContext_)
        return;
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (const auto& [_, map] : instanceMappings) {
        if (!upnpContext_->requestMappingRemove(map)) {
            // No port mapped, so the callback will not be removed,
            // remove it anyway
            upnpContext_->unregisterCallback(map);
        }
        upnpContext_->unselectProvisionedPort(map);
    }
}

void
Controller::addLocalMap(const Mapping& map)
{
    JAMI_DBG("Controller@%ld: Added map %s", id_, map.toString().c_str());

    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    instanceMappings.emplace(map.getPortExternal(), Mapping(map));
}

bool
Controller::removeLocalMap(const Mapping& map)
{
    if (not upnpContext_)
        return false;

    JAMI_DBG("Controller@%ld: Remove map %s", id_, map.toString().c_str());

    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = instanceMappings.find(map.getPortExternal());
    if (it != instanceMappings.end()) {
        instanceMappings.erase(it);
        return true;
    }
    return false;
}

bool
Controller::preAllocateProvisionedPorts(upnp::PortType type, unsigned portCount,
                                        uint16_t minPort, uint16_t maxPort)
{
    if (upnpContext_)
        return upnpContext_->preAllocateProvisionedPorts(type, portCount);

    return false;
}

uint16_t
Controller::generateRandomPort(uint16_t min, uint16_t max, bool mustBeEven)
{
    return UPnPContext::generateRandomPort(min, max, mustBeEven);
}

} // namespace upnp
} // namespace jami
