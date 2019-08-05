/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *    Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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
    try {
        upnpContext_ = getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
}

Controller::~Controller()
{
    JAMI_WARN("DESTROYING CONTROLLER");
    if (upnpContext_) {
        upnpContext_->clearCallbacks(udpMappings_);
        upnpContext_->clearCallbacks(tcpMappings_);
    }

    removeAllLocalMappings();
    mapCbList_.clear();
}

bool
Controller::operator==(Controller& other) const
{
    if (keepCb_ != other.keepCb_)
        return false;
    
    for (auto& udpMap : udpMappings_) {
        PortMapLocal::iterator it = other.udpMappings_.find(udpMap.first);
        if (it != other.udpMappings_.end())
            continue;
        else
            return false;
    }

    for (auto& tcpMap : tcpMappings_) {
        PortMapLocal::iterator it = other.tcpMappings_.find(tcpMap.first);
        if (it != other.tcpMappings_.end())
            continue;
        else
            return false;
    }

    for (auto& item : mapCbList_) {
        cbMapItr it = other.mapCbList_.find(std::make_pair(item.first.first, item.first.second));
        if (it != other.mapCbList_.end()) {
            if (&(it->second) == &(item.second))
                continue;
            else 
                return false;
        } else {
            return false;
        }
    }

    return true;
}

bool
Controller::hasValidIgd()
{
    if (not upnpContext_)
        return false;

    return upnpContext_->hasValidIgd();
}

void
Controller::addMapping(NotifyServiceCallback&& cb, uint16_t portDesired, PortType type, bool unique, uint16_t portLocal)
{
    if (not upnpContext_)
        return;

    if (portLocal == 0)
        portLocal = portDesired;
    
    // Check if the mapping requested isn't already in the controllers internal list.
    if (isLocalMapPresent(portDesired, type)) {
        cb(&portDesired, true);
    } else {
        if (not unique) {
            // Check if the mapping requested was already opened by another controller.
            if (upnpContext_->isMappingInUse(portDesired, type)) {
                upnpContext_->incrementNbOfUsers(portDesired, type);
                addLocalMap(std::move(Mapping(portDesired, portLocal, type)));
                cb(&portDesired, true);
                return;
            }
        }

        // Add callback if not already present.
        registerCallback(portDesired, portLocal, std::move(cb));

        // Send out request.
        upnpContext_->addMapping(std::bind(&Controller::onAddMapping, this, _1, _2),
                                 std::bind(&Controller::onRemoveMapping, this, _1, _2), 
                                 portDesired, portLocal, type, unique, keepCb_);
    }
}

void
Controller::onAddMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("Controller: Port mapping added NOTIFY");
    if (success and mapping) 
        JAMI_WARN("Controller: Opened port %s:%s %s", mapping->getPortExternalStr().c_str(),
                                                      mapping->getPortInternalStr().c_str(),
                                                      mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("Controller: Failed to open port %s:%s %s", mapping->getPortExternalStr().c_str(),
                                                              mapping->getPortInternalStr().c_str(),
                                                              mapping->getTypeStr().c_str());
    
    if (mapping) {
        addLocalMap(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()));
        dispatchCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()), success);
        unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()));
    }
}

void
Controller::removeAllLocalMappings()
{
    removeLocalMappings(PortType::UDP);
    removeLocalMappings(PortType::TCP);
}

void Controller::onRemoveMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("Controller: Port mapping removed NOTIFY");
    if (success)
        JAMI_WARN("Controller: Closed port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("Controller: Failed to close port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());

    if (mapping) {
        removeLocalMap(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()));
        unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()));
    }
}

IpAddr
Controller::getExternalIP() const
{
    if (upnpContext_)
        return upnpContext_->getExternalIP();
    return {};
}

IpAddr
Controller::getLocalIP() const
{
    if (upnpContext_)
        return upnpContext_->getLocalIP();
    return {};
}

void
Controller::removeLocalMappings(PortType type) {

    if (not upnpContext_)
        return;

    std::lock_guard<std::mutex> lk(mapListMutex_);
    
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (portMapItr it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
        upnpContext_->removeMapping(it->second);
    }
}

bool
Controller::isLocalMapPresent(const unsigned int portExternal, PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (portMapItr it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
        if (it->second.getPortExternal() == portExternal) {
            return true;
        }
    }
    return false;
}

bool
Controller::isLocalMapPresent(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (portMapItr it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
        if (it->second == map) {
            return true;
        }
    }
    return false;
}

void
Controller::addLocalMap(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    instanceMappings.emplace(std::move(map.getPortExternal()), std::move(map));
}

void
Controller::removeLocalMap(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& instanceMappings = map.getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (portMapItr it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
        if (it->second == map) {
            instanceMappings.erase(it);
        }
    }
}

void
Controller::registerCallback(unsigned int portExternal, unsigned int portInternal, NotifyServiceCallback&& cb)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first.first == portExternal and
            it->first.second == portInternal){
                return;
        }
    }
    mapCbList_.emplace(std::make_pair(portInternal, portInternal), cb);
}

void
Controller::unregisterCallback(Mapping map)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first.first == map.getPortExternal() and
            it->first.second == map.getPortInternal()) {
            if (not keepCb_) {
                mapCbList_.erase(it->first);
            }
            return;
        }
    }
}

void
Controller::dispatchCallback(Mapping map, bool success)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first.first == map.getPortExternal() and
            it->first.second == map.getPortInternal()) {
            uint16_t port = it->first.first;
            it->second(&port, success);
            return;
        }
    }
}

}} // namespace jami::upnp
