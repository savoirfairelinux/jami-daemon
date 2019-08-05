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
#include "manager.h"
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

using NotifyControllerMapCallback = std::function<void(Mapping*, bool)>;
using NtfyCtrlrMap = NotifyControllerMapCallback;
using NotifyControllerConnectivityChangeCallback = std::function<void()>;
using NtfyCtrlCnxChg = NotifyControllerConnectivityChangeCallback;

struct ControllerData {
    std::string memAddr;
    bool keepCb;
    bool isNotified;
    NtfyCtrlrMap cbAdd;
    NtfyCtrlrMap cbRm;
    NtfyCtrlCnxChg cbCnx;
};
using pControllerData = std::unique_ptr<ControllerData>;

const constexpr auto MAP_REQUEST_TIMEOUT = std::chrono::seconds(1);

class UPnPContext
{
public:
    struct PendingMapRequest {
        Mapping map;
        std::shared_ptr<Task> cleanupTask;
    };

    UPnPContext();
    ~UPnPContext();

    // Check if there is a valid IGD in the IGD list.
    bool hasValidIgd();

    // Clears callbacks associated with map list given as parameter.
    void clearCallbacks(const PortMapLocal& mapList, const std::string& ctrlId);

    // Adds callback associated with a mapping and its controller.
    void addCallback(Mapping map, pControllerData ctrlData); 

    // Informs the UPnP context that the network status has changed.
    void connectivityChanged();

    // Checks if the desired port is already in use on an IGD.
    bool isMappingInUse(const unsigned int portDesired, PortType type);

    // Increments the number of users for a given port.
    void incrementNbOfUsers(const unsigned int portDesired, PortType type);

    // Sends out a request to a protocol to add a mapping.
    void addMapping(pControllerData ctrlData, uint16_t portDesired, uint16_t portLocal, PortType type, bool unique);

    // Callback function for when mapping is added.
    void onAddMapping(IpAddr igdIp, Mapping* mapping, bool success);

    // Sends out a request to protocol to remove a mapping.
    void removeMapping(const Mapping& mapping);

    // Callback function for when mapping is removed.
    void onRemoveMapping(IpAddr igdIp, Mapping* mapping, bool success);

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
    void addMapping(IGD* igd, uint16_t portExternal, uint16_t portInternal, PortType type);

    // Adds mapping to corresponding IGD. 
    void addMappingToIgd(IpAddr igdIp, Mapping map);

    // Removes mapping from corresponding IGD. 
    void removeMappingFromIgd(IpAddr igdIp, Mapping map);

    // Add callbacks to callback list.
    void registerCallback(Mapping map, pControllerData ctrlData);

    // Removes callback from callback list given a mapping.
    void unregisterCallback(Mapping map, bool force = false);

    // Removes callback from callback list given a mapping and controller Id.
    void unregisterCallback(Mapping map, const std::string& ctrlId);

    // Calls corresponding callback.
    void dispatchOnAddCallback(Mapping map, bool success);

    // Calls corresponding callback.
    void dispatchOnRmCallback(Mapping map, bool success);

    // Registers a timeout for a given pending add map request.
    void registerAddMappingTimeout(Mapping map);

    // Unregisters a timeout for a given pending add map request.
    void unregisterAddMappingTimeout(Mapping map);

    // Registers a timeout for a given pending remove map request.
    void registerRmMappingTimeout(Mapping map);

    // Unregisters a timeout for a given pending remove map request.
    void unregisterRmMappingTimeout(Mapping map);

private:
    NON_COPYABLE(UPnPContext);

    mutable std::mutex igdListMutex_;                           // Mutex that protects IGD list.
    std::vector<std::unique_ptr<UPnPProtocol>> protocolList_;   // Vector of available protocols.
    std::list<std::pair<UPnPProtocol*, IGD*>> igdList_;         // List of IGDs with their corresponding protocols that found them.

    std::mutex cbListMutex_;                                    // Mutex that protects the callback list.
    std::map<Mapping*, pControllerData> mapCbList_;             // List of mappings with their corresponding callbacks.

    std::mutex pendindRequestMutex_;                            // Mutex that protects the pending map request lists.
    std::vector<PendingMapRequest> pendingAddMapList_ {};       // Vector of pending add mapping requests.
    std::vector<PendingMapRequest> pendingRmMapList_ {};        // Vector of pending remove mapping request.
};

std::shared_ptr<UPnPContext> getUPnPContext();

}} // namespace jami::upnp
