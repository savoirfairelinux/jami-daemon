/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class UPnPContext;

class Controller
{
public:
    using NotifyServiceCallback = std::function<void(uint16_t, bool)>;
    Controller(bool keepCb = false);
    ~Controller();

    // Checks if a valid IGD is available.
    bool hasValidIGD() const;

    // Gets the external ip of the first valid IGD in the list.
    IpAddr getExternalIP() const;

    // Gets the local ip that interface with the first valid IGD in the list.
    IpAddr getLocalIP() const;

    void requestMappingAdd(NotifyServiceCallback&& cb, uint16_t portDesired, PortType type, bool unique, uint16_t portLocal = 0);
    bool requestMappingRemove(uint16_t portExternal, PortType type);

    // Checks if the map is present locally given a port and type.
    bool isLocalMapPresent(uint16_t portExternal, PortType type) const;

private:
    std::shared_ptr<UPnPContext> upnpContext_;		// Context from which the controller executes the wanted commands.

    mutable std::mutex mapListMutex_;                       // Mutex to protect mappings list.
    PortMapLocal udpMappings_;						// List of UDP mappings created by this instance.
    PortMapLocal tcpMappings_;						// List of TCP mappings created by this instance.

    size_t listToken_ {0};							// IGD listener token.
    uint64_t id_ {0};                           // Variable to store string of address to be used as the unique identifier.
    bool keepCb_ {false};                       // Variable that indicates if the controller wants to keep it's callbacks in the list after a connectivity change.

    // Adds a mapping locally to the list.
    void addLocalMap(const Mapping& map);
    // Removes a mapping locally from the list.
    bool removeLocalMap(const Mapping& map);
    // Removes all mappings of the given type.
    void requestAllMappingRemove(PortType type);
};

}} // namespace jami::upnp
