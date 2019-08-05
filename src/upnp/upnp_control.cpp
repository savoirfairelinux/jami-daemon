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
    removeMappings();
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
Controller::addMapping(NotifyServiceCallback&& cb, uint16_t port_desired, PortType type, bool unique, uint16_t port_local)
{
    if (not upnpContext_)
        return;

    if (port_local == 0)
        port_local = port_desired;
    
    // First check if the mapping requested isn't already opened. If it is, call the callback to notify the service.
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    portMapItr portItr = instanceMappings.find(port_desired);
    if (portItr != instanceMappings.end()) {
        cb(&port_desired, true);
    } else {
        // If a controller requests a mapping, it is up to the UPnP stack to decide what to do with it.
        // This means that the controller doesn't know the state of the opened or closed ports at the 
        // time the request is made. The reason for this is because we want the services to be able to make
        // requests at any time, including such times where an IGD isn't discovered yet. Therefore, the 
        // controller needs to keep track (internally) of the requests it makes and remove them only once
        // they have been answered. Therefore, we only add the request to the callback list if there isn't
        // already an exact identical request already pending.
        cbMapItr it = mapCbList_.find(std::make_pair(port_desired, port_local));
        if (it == mapCbList_.end())
            mapCbList_.emplace(std::make_pair(port_desired, port_local), cb);

        // We send out the request to the context independently of wether or not the mapping request is already
        // in the list (which means the controller has already asked for this port mapping). The reason for
        // this is because if the controller requests a mapping that's already in the list, it means the UPnP
        // stack hasn't treated it yet.
        upnpContext_->addMapping(std::bind(&Controller::onAddMapping, this, _1, _2),
                                 std::bind(&Controller::onRemoveMapping, this, _1, _2), 
                                 port_desired, port_local, type, unique, keepCb_);
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
    
    if (success and mapping) {
        auto usedPort = mapping->getPortExternal();
        auto& instanceMappings = mapping->getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
        Mapping map = Mapping(mapping->getPortExternal(),
                              mapping->getPortInternal(),
                              mapping->getType());
        // Insert used mapping in controllers internal mapping list.
        instanceMappings.emplace(usedPort, std::move(map));

        // Call corresponding callback to notify service.
        for (const auto& cb: mapCbList_) {
            if (cb.first.first == mapping->getPortExternal() and
                cb.first.second == mapping->getPortInternal()) {
                uint16_t port = mapping->getPortExternal();
                cb.second(&port, success);
                if (not keepCb_) {
                    // Erase callback if we don't need to keep it through a connectivity change.
                    mapCbList_.erase(cb.first);
                }
                return;
            }
        }
    }
}

void
Controller::removeMappings()
{
    removeMappings(PortType::UDP);
    removeMappings(PortType::TCP);
}

void Controller::onRemoveMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("Controller: Port mapping removed NOTIFY");
    if (success)
        JAMI_WARN("Controller: Closed port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("Controller: Failed to close port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());

    if (mapping and success) {
        auto& instanceMappings = mapping->getType() == PortType::UDP ? udpMappings_ : tcpMappings_;

        for (auto& map : instanceMappings) {
            if (map.second.getPortExternal() == mapping->getPortExternal() and
                map.second.getPortInternal() == mapping->getPortInternal() and
                map.second.getType() == mapping->getType()) {
                instanceMappings.erase(map.first);
                return;
            }
        }
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
Controller::removeMappings(PortType type) {

    if (not upnpContext_)
        return;

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto& map : instanceMappings) {
        upnpContext_->removeMapping(map.second);
    }
}

}} // namespace jami::upnp
