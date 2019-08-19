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

    if (listToken_ and upnpContext_) {
        upnpContext_->removeIGDListener(listToken_);
    }

    for (auto& cb : addPortMapCbList_) {
        // Removes ports only of sepcific Controller instance.
        portList_.remove(cb.first.first);
    }

    addPortMapCbList_.clear();
}

bool
Controller::hasValidIGD()
{
    return upnpContext_ and upnpContext_->hasValidIGD();
}

void
Controller::setIGDListener(IgdFoundCallback&& cb)
{
    if (not upnpContext_) {
        return;
    }

    if (listToken_) {
        upnpContext_->removeIGDListener(listToken_);
    }

    listToken_ = cb ? upnpContext_->addIGDListener(std::move(cb)) : 0;
}

bool
Controller::addMapping(NotifyServiceAddMapCallback&& cb, uint16_t port_desired, PortType type, bool unique, uint16_t* port_used, uint16_t port_local)
{
    if (not upnpContext_) {
        return false;
    }

    if (port_local == 0) {
        port_local = port_desired;
    }

    // Add callback with corresponding ports to list if it isn't already in the list.
    if (addPortMapCbList_.find(std::make_pair(port_desired, port_local)) == addPortMapCbList_.end()){
        addPortMapCbList_.emplace(std::make_pair(port_desired, port_local), cb);   
    }

    Mapping mapping = upnpContext_->addMapping(std::bind(&Controller::onAddMapping, this, _1, _2), 
                                               port_desired, port_local, type, unique, id_);
    if (mapping) {
        auto usedPort = mapping.getPortExternal();
        if (port_used) {
            *port_used = usedPort;
        }
        return true;
    }
    return false;
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
        portList_.emplace_back(std::move(mapping->getPortExternal()));
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
    for (auto iter = instanceMappings.begin(); iter != instanceMappings.end();) {
        auto& mapping = iter->second;
        upnpContext_->removeMapping(mapping);
        portList_.remove(mapping.getPortExternal());
        iter = instanceMappings.erase(iter);
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
