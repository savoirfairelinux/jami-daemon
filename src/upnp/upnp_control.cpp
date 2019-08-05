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
    memAddr_ = this;
    std::ostringstream addr_str;
    addr_str << memAddr_;
    memAddrStr_ = addr_str.str();

    try {
        upnpContext_ = getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
}

Controller::~Controller()
{
    JAMI_DBG("~Controller@%s", memAddrStr_.c_str());

    if (upnpContext_) {
        upnpContext_->clearCallbacks(udpMappings_, memAddrStr_);
        upnpContext_->clearCallbacks(tcpMappings_, memAddrStr_);
    }

    mapCbList_.clear();
    removeAllLocalMappings();
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
    // We lock both mutexes to prevent re-entrance from controllers who request mappings 
    // before the current request is done being sent out.
    std::unique_lock<std::mutex> lk1(cbListMutex_);
    std::unique_lock<std::mutex> lk2(mapListMutex_);

    if (not upnpContext_) {
        lk2.unlock();
        lk1.unlock();
        return;
    }

    if (portLocal == 0)
        portLocal = portDesired;
    
    // Check if the mapping requested isn't already in the controllers internal list.
    lk2.unlock();
    if (isLocalMapPresent(portDesired, type)) {
        lk2.lock();
        cb(&portDesired, true);
        lk2.unlock();
        lk1.unlock();
        return;
    } else {
        lk2.lock();
        if (not unique) {
            // Check if the mapping requested was already opened by another controller.
            if (upnpContext_->isMappingInUse(portDesired, type)) {
                upnpContext_->incrementNbOfUsers(portDesired, type);
                upnpContext_->addCallback(std::move(Mapping(portDesired, portLocal, type, unique)), 
                                          std::make_unique<ControllerData>(ControllerData {
                                          memAddrStr_, keepCb_, false,
                                          std::move(std::bind(&Controller::onAddMapping, this, _1, _2)),
                                          std::move(std::bind(&Controller::onRemoveMapping, this, _1, _2)),
                                          std::move(std::bind(&Controller::onConnectivityChange, this))}));
                lk2.unlock();
                addLocalMap(std::move(Mapping(portDesired, portLocal, type)));
                lk2.lock();
                cb(&portDesired, true);
                lk2.unlock();
                lk1.unlock();
                return;
            }
        }

        // Add callback if not already present.
        lk1.unlock();
        registerCallback(portDesired, portLocal, std::move(cb));
        lk1.lock();

        // Send out request.
        upnpContext_->addMapping(std::make_unique<ControllerData>(ControllerData {
                                 memAddrStr_, keepCb_, false,
                                 std::move(std::bind(&Controller::onAddMapping, this, _1, _2)),
                                 std::move(std::bind(&Controller::onRemoveMapping, this, _1, _2)),
                                 std::move(std::bind(&Controller::onConnectivityChange, this))}),
                                 portDesired, portLocal, type, unique);
        lk2.unlock();
        lk1.unlock();
    }
}

void
Controller::onAddMapping(Mapping* mapping, bool success)
{
    if (mapping) {
        if (success)
            addLocalMap(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
        dispatchCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()), success);
        if (success)
            unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
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
    if (mapping and success) {
        removeLocalMap(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
        unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
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

    std::lock_guard<std::mutex> lk(mapListMutex_);

    if (not upnpContext_)
        return;
    
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
        upnpContext_->removeMapping(it->second);
    }
}

bool
Controller::isLocalMapPresent(const unsigned int portExternal, PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
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
    for (auto it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
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
    for (auto it = instanceMappings.begin(); it != instanceMappings.end(); it++) {
        if (it->second == map) {
            instanceMappings.erase(it);
        }
    }
}

void
Controller::registerCallback(unsigned int portExternal, unsigned int portInternal, NotifyServiceCallback&& cb)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
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

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
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

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first.first == map.getPortExternal() and
            it->first.second == map.getPortInternal()) {
            uint16_t port = it->first.first;
            it->second(&port, success);
            return;
        }
    }
}

}} // namespace jami::upnp
