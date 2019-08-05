/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

using namespace std::placeholders;

class UPnPContext
{
public:
    using NotifyControllerCallback = std::function<void(Mapping*, bool)>;
    using cbMap = std::map<Mapping*, std::pair<bool, std::pair<NotifyControllerCallback, NotifyControllerCallback>>>;
    using cbMapItr = std::map<Mapping*, std::pair<bool, std::pair<NotifyControllerCallback, NotifyControllerCallback>>>::iterator;
    
    UPnPContext();
    ~UPnPContext();

    // Check if there is a valid IGD in the IGD list.
    bool hasValidIgd();

    // Clears callbacks associated with map list given as parameter.
    void clearCallbacks(const PortMapLocal& mapList);

    // Informs the UPnP context that the network status has changed.
    void connectivityChanged();

    bool isMappingInUse(const unsigned int portDesired, PortType type);

    void incrementNbOfUsers(const unsigned int portDesired, PortType type);

    // Sends out a request to a protocol to add a mapping.
    void addMapping(NotifyControllerCallback&& cbAdd,
                    NotifyControllerCallback&& cbRm, 
                    uint16_t portDesired, uint16_t portLocal, PortType type, bool unique,
                    bool keepCb = false);

    // Callback function for when mapping is added.
    void onAddMapping(Mapping* mapping, bool success);

    // Sends out a request to protocol to remove a mapping.
    void removeMapping(const Mapping& mapping);

    // Callback function for when mapping is removed.
    void onRemoveMapping(Mapping* mapping, bool success);

    // Get external Ip of a chosen IGD.
    IpAddr getExternalIP() const;

    // Get our local Ip.
    IpAddr getLocalIP() const;

private:
    // Checks if the IGD is in the list by checking the IGD's public Ip.
    bool isIgdInList(const IpAddr& publicIpAddr);

    // Tries to add or remove IGD to the list via callback.
    bool igdListChanged(UPnPProtocol* protocol, IGD* igd, const IpAddr publicIpAddr, bool added);

    // Tries to add IGD to the list by getting it's public Ip address internally.
    bool addIgdToList(UPnPProtocol* protocol, IGD* igd);

    // Removes IGD from list by specifiying the IGD itself.
    bool removeIgdFromList(IGD* igd);

    // Removes IGD from list by specifiying the IGD's public Ip address.
    bool removeIgdFromList(IpAddr publicIpAddr);

    // Returns the protocol of the IGD.
    UPnPProtocol::Type getIgdProtocol(IGD* igd);

    // Tries to add a mapping to a specific IGD.
    void addMapping(IGD* igd, uint16_t portExternal, uint16_t portInternal, PortType type, UPnPProtocol::UpnpError& upnpError);

    // Add callbacks to callback list.
    void registerCallback(Mapping map, bool keepCb, NotifyControllerCallback&& cbAdd, NotifyControllerCallback&& cbRm);

    // Removes callback from callback list.
    void unregisterCallback(Mapping map);

    // Calls corresponding callback.
    void dispatchOnAddCallback(Mapping map, bool success);

    // Calls corresponding callback.
    void dispatchOnRmCallback(Mapping map, bool success);

public:
    constexpr static unsigned MAX_RETRIES = 20;

private:
    NON_COPYABLE(UPnPContext);

    std::vector<std::unique_ptr<UPnPProtocol>> protocolList_;   // Vector of available protocols.
    std::list<std::pair<UPnPProtocol*, IGD*>> igdList_;         // List of IGDs with their corresponding public IPs.
    mutable std::mutex igdListMutex_;                           // Mutex used to access these lists and IGDs in a thread-safe manner.

    cbMap mapCbList_;                                           // List of mappings with their corresponding callbacks.
};

std::shared_ptr<UPnPContext> getUPnPContext();

}} // namespace jami::upnp
