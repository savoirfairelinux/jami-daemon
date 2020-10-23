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

namespace jami {
namespace upnp {


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

    // Request port mapping.
    // Returns the port number for which a mapping is being requested. The
    // mapping might not be ready when the method. The final result is
    // returned through the provided callback.
    uint16_t requestMappingAdd(NotifyServiceCallback&& cb, const Mapping& map);

    // Remove port mapping.
    bool requestMappingRemove( const Mapping& map);

    // Checks if the map is present locally given a port and type.
    bool isLocalMapPresent(uint16_t portExternal, PortType type) const;

    // Generates provision ports to be used throughout the application's lifetime.
    bool preAllocateProvisionedPorts(upnp::PortType type, unsigned portCount, uint16_t minPort, uint16_t maxPort);

    // Generates random port number.
    static uint16_t generateRandomPort(uint16_t min, uint16_t max, bool mustBeEven = false);

private:
    std::shared_ptr<UPnPContext>
        upnpContext_; // Context from which the controller executes the wanted commands.

    mutable std::mutex mapListMutex_; // Mutex to protect mappings list.
    // TODO. Unify these maps
    PortMapLocal udpMappings_;        // List of UDP mappings created by this instance.
    PortMapLocal tcpMappings_;        // List of TCP mappings created by this instance.

    size_t listToken_ {0}; // IGD listener token.
    uint64_t id_ {0}; // Variable to store string of address to be used as the unique identifier.
    bool keepCb_ {false}; // Variable that indicates if the controller wants to keep it's callbacks
                          // in the list after a connectivity change.

    // Try to provision a UPNP port.
    uint16_t requestMappingAdd(ControllerData&& ctrlData, const Mapping& map);
private:
    // Adds a mapping locally to the list.
    void addLocalMap(const Mapping& map);
    // TODO. These should be removed when all when all mapping data are removed from the IGDs/Protocols.
    // Removes a mapping locally from the list.
    bool removeLocalMap(const Mapping& map);
    // Removes all mappings of the given type.
    void requestAllMappingRemove(PortType type);
};

} // namespace upnp
} // namespace jami
