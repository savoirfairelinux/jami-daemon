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

using namespace std::placeholders;

Controller::Controller(upnp::Service&& id):
    id_(id)
{
    try {
        upnpContext_ = getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
}

Controller::~Controller()
{
    removeMappings();
    addPortMapCbList_.clear();
}

bool
Controller::operator==(Controller& other) const
{
    if (id_ != other.id_)
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

    for (auto& item : addPortMapCbList_) {
        std::map<std::pair<uint16_t, uint16_t>, NotifyServiceAddMapCallback>::iterator it = \
        other.addPortMapCbList_.find(std::make_pair(item.first.first, item.first.second));
        if (it != other.addPortMapCbList_.end()) {
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
Controller::hasValidIGD()
{
    return upnpContext_ and upnpContext_->hasValidIGD();
}

void
Controller::addMapping(NotifyServiceAddMapCallback&& cb, uint16_t port_desired, PortType type, bool unique, uint16_t port_local)
{
    if (not upnpContext_) {
        return;
    }

    if (port_local == 0) {
        port_local = port_desired;
    }

    // Add callback with corresponding ports to list if it isn't already in the list.
    std::map<std::pair<uint16_t, uint16_t>, NotifyServiceAddMapCallback>::iterator it = \
    addPortMapCbList_.find(std::make_pair(port_desired, port_local));
    if (it == addPortMapCbList_.end())
        if (&(it->second) != &cb)
            addPortMapCbList_.emplace(std::make_pair(port_desired, port_local), cb);

    upnpContext_->addMapping(std::bind(&Controller::onAddMapping, this, _1, _2), 
                             port_desired, port_local, type, unique, id_);
}

void
Controller::onAddMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("Controller: Port mapping added NOTIFY");
    if (success)
        JAMI_WARN("Controller: Opened port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("Controller: Failed to open port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    
    if (success and mapping) {
        auto usedPort = mapping->getPortExternal();
        auto& instanceMappings = mapping->getType() == PortType::UDP ? udpMappings_ : tcpMappings_;
        Mapping map = Mapping(mapping->getPortExternal(),
                              mapping->getPortInternal(),
                              mapping->getType());
        instanceMappings.emplace(usedPort, std::move(map));

        for (const auto& cb: addPortMapCbList_) {
            if (cb.first.first == mapping->getPortExternal() and
                cb.first.second == mapping->getPortInternal()) {
                uint16_t port = mapping->getPortExternal();
                cb.second(&port, success);
            }
        }
    }
}

void
Controller::removeMappings(PortType type) {

    if (not upnpContext_) {
        return;
    }

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto& map : instanceMappings) {
        upnpContext_->removeMapping(map.second);
    }
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
        for (auto iter = instanceMappings.begin(); iter != instanceMappings.end();) {
            auto& mapToRemove = iter->second;
            if (mapToRemove.getPortExternal() == mapping->getPortExternal() and 
                mapToRemove.getPortInternal() == mapping->getPortInternal() and 
                mapToRemove.getType() == mapping->getType()) {
                iter = instanceMappings.erase(iter);
                break;
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

}} // namespace jami::upnp
