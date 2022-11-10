/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

#include "protocol/upnp_protocol.h"
#if HAVE_LIBNATPMP
#include "protocol/natpmp/nat_pmp.h"
#endif
#if HAVE_LIBUPNP
#include "protocol/pupnp/pupnp.h"
#endif
#include "protocol/igd.h"

#include "logger.h"
#include "connectivity/ip_utils.h"
#include "noncopyable.h"

#include <opendht/rng.h>

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>
#include <random>
#include <atomic>
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

class UPnPContext : public UpnpMappingObserver, protected UpnpThreadUtil
{
private:
    struct MappingStatus
    {
        int openCount_ {0};
        int readyCount_ {0};
        int pendingCount_ {0};
        int inProgressCount_ {0};
        int failedCount_ {0};

        void reset()
        {
            openCount_ = 0;
            readyCount_ = 0;
            pendingCount_ = 0;
            inProgressCount_ = 0;
            failedCount_ = 0;
        };
        int sum() { return openCount_ + pendingCount_ + inProgressCount_ + failedCount_; }
    };

public:
    UPnPContext();
    ~UPnPContext();

    // Retrieve the UPnPContext singleton.
    static std::shared_ptr<UPnPContext> getUPnPContext();

    // Terminate the instance.
    void shutdown();

    // Set the known public address
    void setPublicAddress(const IpAddr& addr);

    // Check if there is a valid IGD in the IGD list.
    bool isReady() const;

    // Get external Ip of a chosen IGD.
    IpAddr getExternalIP() const;

    // Inform the UPnP context that the network status has changed. This clears the list of known
    void connectivityChanged();

    // Returns a shared pointer of the mapping.
    Mapping::sharedPtr_t reserveMapping(Mapping& requestedMap);

    // Release an used mapping (make it available for future use).
    void releaseMapping(const Mapping& map);

    // Register a controller
    void registerController(void* controller);
    // Unregister a controller
    void unregisterController(void* controller);

    // Generate random port numbers
    static uint16_t generateRandomPort(PortType type, bool mustBeEven = false);

private:
    // Initialization
    void init();

    /**
     * @brief start the search for IGDs activate the mapping
     * list update.
     *
     */
    void startUpnp();

    /**
     * @brief Clear all IGDs and release/delete current mappings
     *
     * @param forceRelease If true, also delete mappings with enabled
     * auto-update feature.
     *
     */
    void stopUpnp(bool forceRelease = false);

    void shutdown(std::condition_variable& cv);

    // Create and register a new mapping.
    Mapping::sharedPtr_t registerMapping(Mapping& map);

    // Removes the mapping from the list.
    std::map<Mapping::key_t, Mapping::sharedPtr_t>::iterator unregisterMapping(
        std::map<Mapping::key_t, Mapping::sharedPtr_t>::iterator it);
    void unregisterMapping(const Mapping::sharedPtr_t& map);

    // Perform the request on the provided IGD.
    void requestMapping(const Mapping::sharedPtr_t& map);

    // Request a mapping remove from the IGD.
    void requestRemoveMapping(const Mapping::sharedPtr_t& map);

    // Remove all mappings of the given type.
    void deleteAllMappings(PortType type);

    // Update the state and notify the listener
    void updateMappingState(const Mapping::sharedPtr_t& map,
                            MappingState newState,
                            bool notify = true);

    // Provision ports.
    uint16_t getAvailablePortNumber(PortType type);

    // Update preferred IGD
    void updatePreferredIgd();

    // Get preferred IGD
    std::shared_ptr<IGD> getPreferredIgd() const;

    // Check and prune the mapping list. Called periodically.
    void updateMappingList(bool async);

    // Provision (pre-allocate) the requested number of mappings.
    bool provisionNewMappings(PortType type, int portCount);

    // Close unused mappings.
    bool deleteUnneededMappings(PortType type, int portCount);

    /**
     * Prune the mapping list.To avoid competing with allocation
     * requests, the pruning is performed only if there are no
     * requests in progress.
     */
    void pruneMappingList();

    /**
     * Check if there are allocated mappings from previous instances,
     * and try to close them.
     * Only done for UPNP protocol. NAT-PMP allocations will expire
     * anyway if not renewed.
     */
    void pruneUnMatchedMappings(const std::shared_ptr<IGD>& igd,
                                const std::map<Mapping::key_t, Mapping>& remoteMapList);

    /**
     * Check the local mapping list against the list returned by the
     * IGD and remove all mappings which do not have a match.
     * Only done for UPNP protocol.
     */
    void pruneUnTrackedMappings(const std::shared_ptr<IGD>& igd,
                                const std::map<Mapping::key_t, Mapping>& remoteMapList);

    void pruneMappingsWithInvalidIgds(const std::shared_ptr<IGD>& igd);

    /**
     * @brief Get the mapping list
     *
     * @param type transport type (TCP/UDP)
     * @return a reference on the map
     * @warning concurrency protection done by the caller
     */
    std::map<Mapping::key_t, Mapping::sharedPtr_t>& getMappingList(PortType type);

    // Get the mapping from the key.
    Mapping::sharedPtr_t getMappingWithKey(Mapping::key_t key);

    // Get the number of mappings per state.
    void getMappingStatus(PortType type, MappingStatus& status);
    void getMappingStatus(MappingStatus& status);

#if HAVE_LIBNATPMP
    void renewAllocations();
#endif

    // Process requests with pending status.
    void processPendingRequests(const std::shared_ptr<IGD>& igd);

    // Process mapping with auto-update flag enabled.
    void processMappingWithAutoUpdate();

    // Implementation of UpnpMappingObserver interface.

    // Callback used to report changes in IGD status.
    void onIgdUpdated(const std::shared_ptr<IGD>& igd, UpnpIgdEvent event) override;
    // Callback used to report add request status.
    void onMappingAdded(const std::shared_ptr<IGD>& igd, const Mapping& map) override;
    // Callback invoked when a request fails. Reported on failures for both
    // new requests and renewal requests (if supported by the the protocol).
    void onMappingRequestFailed(const Mapping& map) override;
#if HAVE_LIBNATPMP
    // Callback used to report renew request status.
    void onMappingRenewed(const std::shared_ptr<IGD>& igd, const Mapping& map) override;
#endif
    // Callback used to report remove request status.
    void onMappingRemoved(const std::shared_ptr<IGD>& igd, const Mapping& map) override;

private:
    NON_COPYABLE(UPnPContext);

    bool started_ {false};

    // The known public address. The external addresses returned by
    // the IGDs will be checked against this address.
    IpAddr knownPublicAddress_ {};

    // Set of registered controllers
    std::set<void*> controllerList_;

    // Map of available protocols.
    std::map<NatProtocolType, std::shared_ptr<UPnPProtocol>> protocolList_;

    // Port ranges for TCP and UDP (in that order).
    std::map<PortType, std::pair<uint16_t, uint16_t>> portRange_ {};

    // Min open ports limit
    int minOpenPortLimit_[2] {4, 8};
    // Max open ports limit
    int maxOpenPortLimit_[2] {8, 12};

    std::shared_ptr<Task> mappingListUpdateTimer_ {};

    // Current preferred IGD. Can be null if there is no valid IGD.
    std::shared_ptr<IGD> preferredIgd_;

    // This mutex must lock only these two members. All other
    // members must be accessed only from the UPNP context thread.
    std::mutex mutable mappingMutex_;
    // List of mappings.
    std::map<Mapping::key_t, Mapping::sharedPtr_t> mappingList_[2] {};
    std::set<std::shared_ptr<IGD>> validIgdList_ {};

    // Shutdown synchronization
    bool shutdownComplete_ {false};
};

} // namespace upnp
} // namespace jami
