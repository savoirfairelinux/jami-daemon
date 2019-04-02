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

#ifndef UPNP_H_
#define UPNP_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory>
#include <chrono>

#include "noncopyable.h"
#include "upnp_igd.h"

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class UPnPContext;

class Controller {
public:
    /* constructor */
    Controller();
    /* destructor */
    ~Controller();

    /**
     * Return whether or not this controller has a valid IGD.
     * @param timeout Time to wait until a valid IGD is found.
     * If timeout is not given or 0, the function pool (non-blocking).
     */
    bool hasValidIGD(std::chrono::seconds timeout = {});

    /**
     * Set or clear a listener for valid IGDs.
     * For simplicity there is one listener per controller.
     */
    void setIGDListener(IGDFoundCallback&& cb = {});

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
    bool addAnyMapping(uint16_t port_desired,
                       uint16_t port_local,
                       PortType type,
                       bool use_same_port,
                       bool unique,
                       uint16_t *port_used);

    /**
     * addAnyMapping with the local port being the same as the external port
     */
    bool addAnyMapping(uint16_t port_desired,
                       PortType type,
                       bool unique,
                       uint16_t *port_used);

    /**
     * removes all mappings added by this instance
     */
    void removeMappings();

    /**
     * tries to get the external ip of the IGD (router)
     */
    IpAddr getExternalIP() const;

    /**
     * tries to get the local ip of the IGD (router)
     */
    IpAddr getLocalIP() const;

private:

    /**
     * All UPnP commands require an initialized upnpContext
     */
    std::shared_ptr<UPnPContext> upnpContext_;

    /**
     * list of mappings created by this instance
     * the key is the external port number, as there can only be one mapping
     * at a time for each external port
     */
    PortMapLocal udpMappings_;
    PortMapLocal tcpMappings_;

    /**
     * IGD listener token
     */
    size_t listToken_ {0};

    /**
     * Try to remove all mappings of the given type
     */
    void removeMappings(PortType type);
};

}} // namespace jami::upnp

#endif /* UPNP_H_ */
