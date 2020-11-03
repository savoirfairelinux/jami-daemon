/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
#include "upnp_thread_util.h"


using random_device = dht::crypto::random_device;

using IgdFoundCallback = std::function<void()>;

namespace jami {
class IpAddr;
}

namespace jami {
namespace upnp {

const constexpr auto MAP_REQUEST_TIMEOUT_UNIT = std::chrono::seconds(4);
const constexpr auto MAP_UPDATE_INTERVAL = std::chrono::seconds(30);

class  UPnPContext : protected UpnpThreadUtil
{
public:

private:
    struct MappingStatus {
        int openCount_ {0};
        int readyCount_ {0};
        int pendingCount_ {0};
        int inProgressCount_ {0};
        int failedCount_ {0};

        void reset() {
            openCount_ = 0;
            readyCount_ = 0;
            pendingCount_ = 0;
            inProgressCount_ = 0;
            failedCount_ = 0;
        };
        int sum() {
            return openCount_ + readyCount_ + pendingCount_ + inProgressCount_ + failedCount_;
        }
    };

public:
    UPnPContext();
    ~UPnPContext();

    // Retrieve the UPnPContext singleton
    static std::shared_ptr<UPnPContext> getUPnPContext();

    // Check if there is a valid IGD in the IGD list.
    bool hasValidIGD();

    // Get external Ip of a chosen IGD.
    IpAddr getExternalIP() const;

    // Get our local Ip.
    IpAddr getLocalIP() const;

    // Inform the UPnP context that the network status has changed. This clears the list of known
    void connectivityChanged();


    // Returns a shared pointer of the mapping.
    Mapping::sharedPtr_t reserveMapping(Mapping& requestedMap);

    // Release an used mapping (make it available for future use).
    bool releaseMapping(Mapping::sharedPtr_t map);

private:
    // Initialization
    void init();

    // Generate random port numbers
    static uint16_t generateRandomPort(uint16_t min, uint16_t max, bool mustBeEven = false);

    // Create and register a new mapping.
    Mapping::sharedPtr_t registerMapping(Mapping& map);

    // Removes the mapping from the list.
    bool unregisterMapping(Mapping::sharedPtr_t map);

    // Perform the request on the provided IGD.
    void requestMapping(std::shared_ptr<UPnPProtocol> protocol, std::shared_ptr<IGD> igd, Mapping::sharedPtr_t map);

    // Perform the request on all available IGDs
    void requestMappingOnAllIgds(Mapping::sharedPtr_t map);

    // Delete mapping from the list and and send remove request.
    void deleteMapping(Mapping::sharedPtr_t map);

    // Remove all mappings of the given type.
    void deleteAllMappings(PortType type);

    // Schedule a time-out timer for a in-progress request.
    void registerAddMappingTimeout(std::shared_ptr<IGD> igd, Mapping::key_t key);

    // Callback invoked when a request times-out
    void onRequestTimeOut(std::shared_ptr<IGD> igd, Mapping::key_t key);

    // Provision ports.
    uint16_t getAvailablePortNumber(PortType type, uint16_t minPort = 0, uint16_t maxPort = 0);

    // Check and prune the mapping list. Called periodically.
    void updateMappingList(bool async);

    // Provision (pre-allocate) the requested number of mappings.
    bool provisionNewMappings(PortType type, int portCount, uint16_t minPort = 0,
        uint16_t maxPort = 0);

    // Close unused mappings.
    bool closeUnusedMappings(PortType type, int portCount);

    // Remove timed-out or failed requests.
    void pruneFailedMappings(PortType type);

    // Callback the IGD used to report changes in IGD status.
    void onIgdChanged(std::shared_ptr<UPnPProtocol> protocol, std::shared_ptr<IGD> igd,
        const IpAddr publicIpAddr, bool added);

    // Callback from the IGD used to report add request status.
    void onMappingAdded(IpAddr igdIp, const Mapping& map, bool success);

    // Callback from the IGD used to report remove request status.
    void onMappingRemoved(IpAddr igdIp, const Mapping& map, bool success);

    // Get the mapping list
    std::map<Mapping::key_t, Mapping::sharedPtr_t>& getMappingList(PortType type);
    std::map<Mapping::key_t, Mapping::sharedPtr_t>& getMappingList(Mapping::key_t key);

    const Mapping::sharedPtr_t getMappingWithKey(Mapping::key_t key);

    // Get the number of mappings per state.
    size_t getMappingStatus(PortType type, MappingStatus& status);

public:
    constexpr static unsigned MAX_REQUEST_RETRIES = 20;

private:
    NON_COPYABLE(UPnPContext);

    std::atomic_bool hasValidIgd_ {false};

    // Vector of available protocols.
    std::vector<std::shared_ptr<UPnPProtocol>> protocolList_;
    std::mutex mutable protocolListMutex_;

    // List of mappings.
    std::map<Mapping::key_t, Mapping::sharedPtr_t> mappingList_[2] {};
    std::mutex mappingListMutex_;

    // Port ranges for UPD and TCP (in that order).
    std::pair<uint16_t, uint16_t> portRange_[2] {};

    // Min open ports limit
    int minOpenPortLimit_[2] {8, 4};
    // Max open ports limit
    int maxOpenPortLimit_[2] {12, 8};

    std::shared_ptr<Task> mappingListUpdateTimer_ {};
};


} // namespace upnp
} // namespace jami
