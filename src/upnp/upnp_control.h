/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp_context.h"
#include "protocol/global_mapping.h"

#include "noncopyable.h"
#include "logger.h"
#include "ip_utils.h"

#include <memory>
#include <chrono>
#include <functional>

using OnPortOpenCallback = std::function<void(uint16_t*, bool)>;

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class UPnPContext;

class Controller
{
public:
    enum class Service {
        UNKNOWN,
        JAMI_ACCOUNT,
        ICE_TRANSPORT,
        SIP_ACCOUT,
        SIP_CALL 
    };

    Controller(Service&& id, OnPortOpenCallback&& portOpenCb);
    ~Controller();

    // Checks if a valid IGD is available.
    bool hasValidIGD();

    // Sets or clears a listener for valid IGDs. There is one listener per controller.
    void setIGDListener(IgdFoundCallback&& cb = {});

    // Adds a mapping. Gives option to use unique port (i.e. not one that is already in use).
    bool addMapping(uint16_t port_desired, PortType type, bool unique, uint16_t* port_used, uint16_t port_local = 0);

    // Removes all mappings added by this instance.
    void removeMappings();

    // Gets the external ip of the first valid IGD in the list.
    IpAddr getExternalIP() const;

    // Gets the local ip that interface with the first valid IGD in the list.
    IpAddr getLocalIP() const;

private:
    // Removes all mappings of the given type.
    void removeMappings(PortType type);

private:
    std::shared_ptr<UPnPContext> upnpContext_;		// Context from which the controller executes the wanted commands.

    PortMapLocal udpMappings_;						// List of UDP mappings created by this instance.
    PortMapLocal tcpMappings_;						// List of TCP mappings created by this instance.

    size_t listToken_ {0};							// IGD listener token.

    Service id_ {};

    OnPortOpenCallback notifyOpenPortCb_;
};

// Global callback list.
static std::list<std::pair<Controller::Service, std::function<void(uint16_t*, bool)>>> portOpenCbList;

}} // namespace jami::upnp
