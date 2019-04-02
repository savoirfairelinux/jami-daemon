/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp_control.h"

#include <memory>

#include "logger.h"
#include "ip_utils.h"
#include "upnp_context.h"
#include "upnp_igd.h"

namespace jami { namespace upnp {

Controller::Controller()
{
    try {
        upnpContext_ = getUPnPContext();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
}

Controller::~Controller()
{
    /* remove all mappings */
    removeMappings();
    if (listToken_ and upnpContext_)
        upnpContext_->removeIGDListener(listToken_);
}

bool
Controller::hasValidIGD(std::chrono::seconds timeout)
{
    return upnpContext_ and upnpContext_->hasValidIGD(timeout);
}

void
Controller::setIGDListener(IGDFoundCallback&& cb)
{
    if (not upnpContext_)
        return;
    if (listToken_)
        upnpContext_->removeIGDListener(listToken_);
    listToken_ = cb ? upnpContext_->addIGDListener(std::move(cb)) : 0;
}

bool
Controller::addAnyMapping(uint16_t port_desired,
                          uint16_t port_local,
                          PortType type,
                          bool use_same_port,
                          bool unique,
                          uint16_t *port_used)
{
    if (not upnpContext_)
        return false;

    Mapping mapping = upnpContext_->addAnyMapping(port_desired, port_local, type,
                                                  use_same_port, unique);
    if (mapping) {
        auto usedPort = mapping.getPortExternal();
        if (port_used)
            *port_used = usedPort;

        /* add to map */
        auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
        instanceMappings.emplace(usedPort, std::move(mapping));
        return true;
    }
    return false;
}

bool
Controller::addAnyMapping(uint16_t port_desired,
                          PortType type,
                          bool unique,
                          uint16_t *port_used)
{
    return addAnyMapping(port_desired, port_desired, type, true, unique,
                         port_used);
}

void
Controller::removeMappings(PortType type) {
    if (not upnpContext_)
        return;

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto iter = instanceMappings.begin(); iter != instanceMappings.end(); ){
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
    if (upnpContext_)
        return upnpContext_->getLocalIP();
    return {}; //  empty address
}

IpAddr
Controller::getExternalIP() const
{
    if (upnpContext_)
        return upnpContext_->getExternalIP();
    return {}; //  empty address
}

}} // namespace jami::upnp
