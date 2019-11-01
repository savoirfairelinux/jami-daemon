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

#include "manager.h"

using random_device = dht::crypto::random_device;

using IgdFoundCallback = std::function<void()>;

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

using MapCb = std::function<void(const Mapping&, bool)>;
using ConnectionChangeCb = std::function<void()>;
struct ControllerData {
    uint64_t id;
    bool keepCb;
    MapCb onMapAdded;
    MapCb onMapRemoved;
    ConnectionChangeCb onConnectionChanged;
};

const constexpr auto MAP_REQUEST_TIMEOUT = std::chrono::seconds(1);

class UPnPContext
{
public:
    struct PendingMapRequest {
        Mapping map;
        std::shared_ptr<Task> cleanupMapRequest;
    };

    UPnPContext();
    ~UPnPContext();

    // Check if there is a valid IGD in the IGD list.
    bool hasValidIGD();

    // Get external Ip of a chosen IGD.
    IpAddr getExternalIP() const;

    // Get our local Ip.
    IpAddr getLocalIP() const;

    // Inform the UPnP context that the network status has changed. This clears the list of known
    void connectivityChanged();

    // Checks if the desired port is1 already in use by an IGD.
    bool isMappingInUse(const unsigned int portDesired, PortType type);

    // Increments the number of users for a given port.
    void incrementNbOfUsers(const unsigned int portDesired, PortType type);

    // Sends out a request to a protocol to add a mapping.
    void requestMappingAdd(ControllerData&& ctrlData, uint16_t portDesired, uint16_t portLocal, PortType type, bool unique);
    // Adds mapping to corresponding IGD.
    void addMappingToIgd(IpAddr igdIp, const Mapping& map);
    // Tries to add a mapping to a specific IGD.
    void requestMappingAdd(IGD* igd, uint16_t portExternal, uint16_t portInternal, PortType type);
    // Callback function for when mapping is added.
    void onMappingAdded(IpAddr igdIp, const Mapping& map, bool success);
    // Calls corresponding callback.
    void dispatchOnAddCallback(const Mapping& map, bool success);

    // Registers a timeout for a given pending add map request.
    void registerAddMappingTimeout(const Mapping& map);
    // Unregisters a timeout for a given pending add map request.
    void unregisterAddMappingTimeout(const Mapping& map);

    // Sends out a request to protocol to remove a mapping.
    void requestMappingRemove(const Mapping& map);
    // Removes mapping from corresponding IGD.
    void removeMappingFromIgd(IpAddr igdIp, const Mapping& map);
    // Callback function for when mapping is removed.
    void onMappingRemoved(IpAddr igdIp, const Mapping& map, bool success);
    // Calls corresponding callback.
    void dispatchOnRmCallback(const Mapping& map, bool success);

    // Add callbacks to callback list.
    void registerCallback(const Mapping& map, ControllerData&& ctrlData);
    // Removes callback from callback list given a mapping.
    void unregisterCallback(const Mapping& map);
    // Removes all callback with a specific controller Id.
    void unregisterAllCallbacks(uint64_t ctrlId);

private:
    // Checks if the IGD is in the list by checking the IGD's public Ip.
    bool isIgdInList(const IpAddr& publicIpAddr);

    // Returns the protocol of the IGD.
    UPnPProtocol::Type getIgdProtocol(IGD* igd);

    // Returns a random port that is not yet used by the daemon for UPnP.
    uint16_t chooseRandomPort(IGD& igd, PortType type);

    // Tries to add or remove IGD to the list via callback.
    bool igdListChanged(UPnPProtocol* protocol, IGD* igd, const IpAddr publicIpAddr, bool added);

    // Tries to add IGD to the list by getting it's public Ip address internally.
    bool addIgdToList(UPnPProtocol* protocol, IGD* igd);

    // Removes IGD from list by specifiying the IGD itself.
    bool removeIgdFromList(IGD* igd);

    // Removes IGD from list by specifiying the IGD's public Ip address.
    bool removeIgdFromList(IpAddr publicIpAddr);

public:
    constexpr static unsigned MAX_RETRIES = 20;

private:
    NON_COPYABLE(UPnPContext);

    std::vector<std::unique_ptr<UPnPProtocol>> protocolList_;	// Vector of available protocols.
    mutable std::mutex igdListMutex_;							// Mutex used to access these lists and IGDs in a thread-safe manner.
    std::list<std::pair<UPnPProtocol*, IGD*>> igdList_;			// List of IGDs with their corresponding public IPs.

    std::mutex pendindRequestMutex_;                            // Mutex that protects the pending map request lists.
    std::vector<PendingMapRequest> pendingAddMapList_ {};       // Vector of pending add mapping requests.
    std::mutex cbListMutex_;                                    // Mutex that protects the callback list.
    std::multimap<Mapping, ControllerData> mapCbList_;          // List of mappings with their corresponding callbacks.

};

std::shared_ptr<UPnPContext> getUPnPContext();

}} // namespace jami::upnp
