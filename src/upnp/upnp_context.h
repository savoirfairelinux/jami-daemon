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

#include "protocol/upnp_protocol.h"
#if HAVE_LIBNATPMP
#include "protocol/natpmp/nat_pmp.h"
#endif
#if HAVE_LIBUPNP
#include "protocol/pupnp/pupnp.h"
#endif
#include "protocol/igd.h"
#include "protocol/global_mapping.h"

#include "logger.h"
#include "ip_utils.h"
#include "noncopyable.h"

#include <opendht/rng.h>

#include <set>
#include <map>
#include <list>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>
#include <random>
#include <atomic>
#include <thread>
#include <vector>
#include <condition_variable>
#include <cstdlib>

using random_device = dht::crypto::random_device;

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class UPnPContext
{
public:
    UPnPContext();
    ~UPnPContext();

    // Check if there is a valid IGD in the IGD list.
    bool hasValidIGD();

    // Add IGD listener.
    size_t addIGDListener(IgdFoundCallback&& cb);

    // Remove IGD listener.
    void removeIGDListener(size_t token);

    // Tries to add a valid mapping. Will return it if successful.
    Mapping addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool use_same_port, bool unique);

    // Removes a mapping.
    void removeMapping(const Mapping& mapping);

    // Get external Ip of a chosen IGD.
    IpAddr getExternalIP() const;

    // Get our local Ip.
    IpAddr getLocalIP() const;

    // Inform the UPnP context that the network status has changed. This clears the list of known
    void connectivityChanged();

    // Tries to add or remove IGD to the list via callback.
    bool igdListChanged(UPnPProtocol* protocol, IGD* igd, const IpAddr publicIpAddr, bool added);

    // Tries to add IGD to the list by getting it's public Ip address internally.
    bool addIgdToList(UPnPProtocol* protocol, IGD* igd);

    // Removes IGD from list by specifiying the IGD itself.
    bool removeIgdFromList(IGD* igd);

    // Removes IGD from list by specifiying the IGD's public Ip address.
    bool removeIgdFromList(IpAddr publicIpAddr);

    // Tries to add mapping. Assumes mutex is already locked.
    Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error);

private:
    // Checks if the IGD is in the list by checking the IGD itself.
    bool isIgdInList(IGD* igd);

    // Checks if the IGD is in the list by checking the IGD's public Ip.
    bool isIgdInList(IpAddr publicIpAddr);

    // Returns a random port that is not yet used by the daemon for UPnP.
    uint16_t chooseRandomPort(const IGD& igd, PortType type);

public:
    constexpr static unsigned MAX_RETRIES = 20;

private:
    NON_COPYABLE(UPnPContext);

    std::map<size_t, IgdFoundCallback> igdListeners_;           // Map of valid IGD listeners with their tokens.
    size_t listenerToken_{ 0 };                                 // Last provided token for valid IGD listeners (0 is the invalid token).

    std::vector<std::unique_ptr<UPnPProtocol>> protocolList_;	// Vector of available protocols.
    std::list<std::pair<UPnPProtocol*, IGD*>> igdList_;			// List of IGDs with their corresponding public IPs.
    mutable std::mutex igdListMutex_;							// Mutex used to access these lists and IGDs in a thread-safe manner.

};

std::shared_ptr<UPnPContext> getUPnPContext();

}} // namespace jami::upnp
