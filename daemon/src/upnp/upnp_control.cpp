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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <memory>
#include <future>

#include "logger.h"
#include "ip_utils.h"
#include "ring_types.h"
#include "upnp_context.h"

namespace ring { namespace upnp {

Controller::Controller()
    : upnpContext_(getUPnPContext())
{
    if (not upnpContext_->isInitialized())
        RING_WARN("UPnP: context is not initialized");
}

Controller::~Controller()
{
    /* remove all mappings */
    removeMappings();
}

/**
 * Return whether or not this controller has a valid IGD,
 * if 'flase' then all requests will fail
 */
bool
Controller::hasValidIGD()
{
#if HAVE_LIBUPNP
    return upnpContext_->hasValidIGD();
#endif
    return false;
}

/**
 * like hasValidIGD, but calls the given callback when the IGD is found
 * or when the search times out without finding one
 */
// void
// Controller::waitForValildIGD(IGDFoundCallback cb)

//     std::thread{
//         bool valid = false;
// #if HAVE_LIBUPNP
//         valid = shared_context->hasValidIGD();
// #endif
//         cb(valid);
//     });
// }


/**
 * tries to add mapping from and to the port_desired
 * if unique == true, makes sure the client is not using this port already
 * if the mapping fails, tries other available ports until success
 *
 * tries to use a random port between 1024 < > 65535 if desired port fails
 *
 * maps port_desired to port_local; if use_same_port == true, makes sure that
 * that the extranl and internal ports are the same
 */
bool
Controller::addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type,
                          bool use_same_port, bool unique, uint16_t *port_used)
{
#if HAVE_LIBUPNP
    Mapping mapping = upnpContext_->addAnyMapping(port_desired, port_local, type,
                                                  use_same_port, unique);
    if (mapping) {
        if (port_used)
            *port_used = mapping.getPortExternal();

        /* add to map */
        auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
        instanceMappings.emplace(*port_used, std::move(mapping));
        return true;
    } else
        return false;
#endif
    return false;
}

/**
 * addAnyMapping with the local port being the same as the external port
 */
bool
Controller::addAnyMapping(uint16_t port_desired, PortType type, bool unique, uint16_t *port_used) {
    addAnyMapping(port_desired, port_desired, type, true, unique, port_used);
}

/**
 * removes mappings added by this instance of the specified port type
 * if an mapping has more than one user in the global list, it is not deleted
 * from the router, but the number of users is decremented
 */
void
Controller::removeMappings(PortType type) {
#if HAVE_LIBUPNP
    auto& instanceMappings = type == PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto iter = instanceMappings.begin(); iter != instanceMappings.end(); ){
        auto& mapping = iter->second;
        upnpContext_->removeMapping(mapping);
        iter = instanceMappings.erase(iter);
    }
#endif
}

/**
 * removes all mappings added by this instance
 */
void
Controller::removeMappings()
{
#if HAVE_LIBUPNP
    removeMappings(PortType::UDP);
    removeMappings(PortType::TCP);
#endif
}

/**
 * tries to get the external ip of the router
 */
IpAddr
Controller::getExternalIP()
{
#if HAVE_LIBUPNP
    return upnpContext_->getExternalIP();
#else
    /* return empty address */
    return {};
#endif
}

}} // namespace ring::upnp
