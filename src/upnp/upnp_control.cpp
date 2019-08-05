/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

Controller::Controller(bool keepCb):
    keepCb_(keepCb)
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
    JAMI_DBG("~Controller@%ld", (long)id_);

    if (upnpContext_) {
        upnpContext_->clearCallbacks(udpMappings_, id_);
        upnpContext_->clearCallbacks(tcpMappings_, id_);
    }

    mapCbList_.clear();

    std::lock_guard<std::mutex> lk(mapListMutex_);
    requestAllMappingRemove();
}

bool
Controller::hasValidIgd()
{
    if (not upnpContext_)
        return false;

    return upnpContext_->hasValidIgd();
}

void
Controller::requestMappingAdd(NotifyServiceCallback&& cb, uint16_t portDesired, PortType type, bool unique, uint16_t portLocal)
{
    std::lock_guard<std::mutex> lk1(mapListMutex_);
    std::lock_guard<std::mutex> lk2(cbListMutex_);

    if (not upnpContext_) {
        return;
    }

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
            upnpContext_->addCallback(map,
                                      ControllerData {id_, keepCb_, false,
                                      std::move(std::bind(&Controller::onMappingAdded, this, _1, _2)),
                                      std::move(std::bind(&Controller::onMappingRemoved, this, _1, _2)),
                                      std::move(std::bind(&Controller::onConnectivityChange, this))});
            addLocalMap(map);
            cb(portDesired, true);
            return;
        }
    }

    // Add callback if not already present.
    registerCallback(portDesired, portLocal, std::move(cb));

    // Send out request.
    upnpContext_->requestMappingAdd(ControllerData {id_, keepCb_, false,
                                    std::move(std::bind(&Controller::onMappingAdded, this, _1, _2)),
                                    std::move(std::bind(&Controller::onMappingRemoved, this, _1, _2)),
                                    std::move(std::bind(&Controller::onConnectivityChange, this))},
                                    portDesired, portLocal, type, unique);
}

void
Controller::onMappingAdded(const Mapping& map, bool success)
{
    if (map.isValid()) {
        if (success)
            addLocalMap(map);
        dispatchCallback(map, success);
        if (success)
            unregisterCallback(map);
    }
}

void
Controller::requestAllMappingRemove()
{
    requestAllMappingRemove(PortType::UDP);
    requestAllMappingRemove(PortType::TCP);
}

void Controller::onMappingRemoved(const Mapping& map, bool success)
{
    if (map.isValid() and success) {
        removeLocalMap(map);
        unregisterCallback(map);
    }
}

void
Controller::onConnectivityChange()
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    if (udpMappings_.empty() and tcpMappings_.empty())
        return;

    // Clear local mappings.
    udpMappings_.clear();
    tcpMappings_.clear();
}

IpAddr
Controller::getExternalIp() const
{
    if (upnpContext_)
        return upnpContext_->getExternalIp();
    return {};
}

IpAddr
Controller::getLocalIp() const
{
    if (upnpContext_)
        return upnpContext_->getLocalIp();
    return {};
}

void
Controller::requestAllMappingRemove(PortType type) {

    // Mutex is already locked.

    if (not upnpContext_)
        return;

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = instanceMappings.cbegin(); it != instanceMappings.cend(); it++)
        upnpContext_->requestMappingRemove(it->second);
}

bool
Controller::isLocalMapPresent(const unsigned int portExternal, PortType type)
{
    // Mutex is already locked.

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = instanceMappings.cbegin(); it != instanceMappings.cend(); it++) {
        if (it->second.getPortExternal() == portExternal)
            return true;
    }
    return false;
}

bool
Controller::isLocalMapPresent(const Mapping& map)
{
    // Mutex is already locked.

    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = instanceMappings.cbegin(); it != instanceMappings.cend(); it++) {
        if (it->second == map)
            return true;
    }
    return false;
}

void
Controller::addLocalMap(const Mapping& map)
{
    // Mutex is already locked.

    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    instanceMappings.emplace(map.getPortExternal(), Mapping(map));
}

void
Controller::removeLocalMap(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = instanceMappings.cbegin(); it != instanceMappings.cend(); it++) {
        if (it->second == map)
            instanceMappings.erase(it);
    }
}

void
Controller::registerCallback(unsigned int portExternal, unsigned int portInternal, NotifyServiceCallback&& cb)
{
    // Mutex is already locked.

    mapCbList_.emplace(std::make_pair(portInternal, portInternal), cb);
}

void
Controller::unregisterCallback(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    if (not keepCb_) {
        auto it = mapCbList_.find(std::make_pair(map.getPortExternal(), map.getPortInternal()));
        if (it != mapCbList_.end())
            mapCbList_.erase(it->first);
    }
}

void
Controller::dispatchCallback(const Mapping& map, bool success)
{
    NotifyServiceCallback cb;
    uint16_t port = 0;

    {
        std::lock_guard<std::mutex> lk(cbListMutex_);

        auto it = mapCbList_.find(std::make_pair(map.getPortExternal(), map.getPortInternal()));
        if (it == mapCbList_.end()) {
            return;
        } else {
            port = it->first.first;
            cb = it->second;
        }
    }

    if (port != 0 and cb) {
        cb(port, success);
    }
}

}} // namespace jami::upnp
