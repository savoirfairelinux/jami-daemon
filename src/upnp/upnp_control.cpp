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

Controller::Controller(upnp::UPnPProtocol::Service&& id, PortOpenNotifyServiceCallback&& cb):
    id_(std::move(id)),
    notifyPortOpenCb_(std::move(cb))
{
    portOpenCbList.emplace_back(id_, notifyPortOpenCb_);
    try {
        upnpContext_ = getUPnPContext();
        using namespace std::placeholders;
        upnpContext_->setOnPortOpenComplete(std::bind(&Controller::onPortOpenComplete, this, _1, _2, _3));
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
Controller::addMapping(uint16_t port_desired, PortType type, bool unique, uint16_t* port_used, uint16_t port_local)
{
    if (not upnpContext_) {
        return false;
    }

    if (port_local == 0) {
        port_local = port_desired;
    }

    Mapping mapping = upnpContext_->addMapping(port_desired, port_local, type, unique);
    if (mapping) {
        auto usedPort = mapping.getPortExternal();
        if (port_used) {
            *port_used = usedPort;
        }
        auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
        instanceMappings.emplace(usedPort, std::move(mapping));
        return true;
    }
    return false;
}

void
Controller::onPortOpenComplete(upnp::UPnPProtocol::Service id, uint16_t* port_used, bool success)
{
    switch(id)
    {
    case upnp::UPnPProtocol::Service::JAMI_ACCOUNT: JAMI_WARN("Controller: %s port %u for Jami Account.", success ? "Opened" : "Failed to open",  *port_used); break;
    case upnp::UPnPProtocol::Service::ICE_TRANSPORT: JAMI_WARN("Controller: Opened port %u for Ice Transport.", success ? "Opened" : "Failed to open", *port_used); break;
    case upnp::UPnPProtocol::Service::SIP_ACCOUT: JAMI_WARN("Controller: Opened port %u for Sip Account.", success ? "Opened" : "Failed to open", *port_used); break;
    case upnp::UPnPProtocol::Service::SIP_CALL: JAMI_WARN("Controller: Opened port %u for Sip Call.", success ? "Opened" : "Failed to open", *port_used); break;
    default: break;
    }

    for (const auto& cb : portOpenCbList) {
        if (cb.first == id)
            cb.second(port_used, success);
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
