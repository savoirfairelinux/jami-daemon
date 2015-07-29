/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
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

namespace ring { namespace upnp {

Controller::Controller()
{
    try {
        upnpContext_ = getUPnPContext();
    } catch (std::runtime_error& e) {
        RING_ERR("UPnP context error: %s", e.what());
    }
}

Controller::~Controller()
{
    /* remove all mappings */
    removeMappings();
}

bool
Controller::hasValidIGD(std::chrono::seconds timeout)
{
#if HAVE_LIBUPNP
    return upnpContext_ and upnpContext_->hasValidIGD(timeout);
#endif
    return false;
}

bool
Controller::addAnyMapping(uint16_t port_desired,
                          uint16_t port_local,
                          PortType type,
                          bool use_same_port,
                          bool unique,
                          uint16_t *port_used)
{
#if HAVE_LIBUPNP
    if (not upnpContext_)
        return false;

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    Mapping target(port_desired, port_local, type);
    for (const auto& m : instanceMappings)
        if (m.second == target) {
            RING_DBG("UPnP maping already existed: %s", m.second.toString().c_str());
            return true;
        }

    Mapping mapping = upnpContext_->addAnyMapping(port_desired, port_local, type,
                                                  use_same_port, unique);
    if (mapping) {
        auto usedPort = mapping.getPortExternal();
        if (port_used)
            *port_used = usedPort;

        /* add to map */
        instanceMappings.emplace(usedPort, std::move(mapping));
        return true;
    }
#endif
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
#if HAVE_LIBUPNP
    if (not upnpContext_)
        return;

    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto iter = instanceMappings.begin(); iter != instanceMappings.end(); ){
        auto& mapping = iter->second;
        upnpContext_->removeMapping(mapping);
        iter = instanceMappings.erase(iter);
    }
#endif
}
void
Controller::removeMappings()
{
#if HAVE_LIBUPNP
    removeMappings(PortType::UDP);
    removeMappings(PortType::TCP);
#endif
}

IpAddr
Controller::getLocalIP() const
{
#if HAVE_LIBUPNP
    if (upnpContext_)
        return upnpContext_->getLocalIP();
#endif
    return {}; //  empty address
}

IpAddr
Controller::getExternalIP() const
{
#if HAVE_LIBUPNP
    if (upnpContext_)
        return upnpContext_->getExternalIP();
#endif
    return {}; //  empty address
}

}} // namespace ring::upnp
