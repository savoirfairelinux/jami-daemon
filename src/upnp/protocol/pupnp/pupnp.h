/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#ifdef _WIN32
#define UPNP_USE_MSVCPP
#define UPNP_STATIC_LIB
#endif

#include "../upnp_protocol.h"
#include "../igd.h"
#include "upnp_igd.h"

#include "logger.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "compiler_intrinsics.h"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include <atomic>
#include <thread>
#include <list>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <future>

namespace jami {
class IpAddr;
}

namespace jami {
namespace upnp {

// Error codes returned by router when trying to remove ports.
constexpr static int ARRAY_IDX_INVALID = 713;
constexpr static int CONFLICT_IN_MAPPING = 718;

// Timeout values (in seconds).
constexpr static unsigned int SEARCH_TIMEOUT {60};
constexpr static unsigned int SUBSCRIBE_TIMEOUT {300};
// Max number of IGD search attempts before failure.
constexpr static unsigned int PUPNP_MAX_RESTART_SEARCH_RETRIES {5};
// Time-out between two successive IGD search.
constexpr static auto PUPNP_TIMEOUT_BEFORE_IGD_SEARCH_RETRY {std::chrono::seconds(60)};

class PUPnP : public UPnPProtocol
{
public:
    using XMLDocument = std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>;
    struct IGDInfo
    {
        std::string location;
        XMLDocument document;
    };
    enum class CtrlAction {
        UNKNOWN,
        ADD_PORT_MAPPING,
        DELETE_PORT_MAPPING,
        GET_GENERIC_PORT_MAPPING_ENTRY,
        GET_STATUS_INFO,
        GET_EXTERNAL_IP_ADDRESS
    };

    using pIGDInfo = std::unique_ptr<IGDInfo>;

    PUPnP();
    ~PUPnP();

    // Set the observer
    void setObserver(UpnpMappingObserver* obs) override;

    // Returns the protocol type.
    NatProtocolType getProtocol() const override { return NatProtocolType::PUPNP; }

    // Get protocol type as string.
    char const* getProtocolName() const override { return "PUPNP"; }

    // Notifies a change in network.
    void clearIgds() override;

    // Sends out async search for IGD.
    void searchForIgd() override;

    // Get the IGD list.
    void getIgdList(std::list<std::shared_ptr<IGD>>& igdList) const override;

    // Return true if the it's fully setup.
    bool isReady() const override;

    // Increment IGD errors counter.
    void incrementErrorsCounter(const std::shared_ptr<IGD>& igd) override;

    // Get from the IGD the list of already allocated mappings if any.
    std::map<Mapping::key_t, Mapping> getMappingsListByDescr(
        const std::shared_ptr<IGD>& igd, const std::string& descr) const override;

    // Request a new mapping.
    void requestMappingAdd(const std::shared_ptr<IGD>& igd, const Mapping& mapping) override;

    // Renew an allocated mapping.
    // Not implemented. Currently, UPNP allocations do not have expiration time.
    void requestMappingRenew([[maybe_unused]] const Mapping& mapping) override { assert(false); };

    // Removes a mapping.
    void requestMappingRemove(const Mapping& igdMapping) override;

    void terminate() override;

private:
    NON_COPYABLE(PUPnP);

    ScheduledExecutor* getUpnContextScheduler() { return UpnpThreadUtil::getScheduler(); }

    // Init lib-upnp
    void initUpnpLib();

    // Register the client
    void registerClient();

    // Start the internal thread.
    void startPUPnP();

    // Start search for UPNP devices
    void searchForDevices();

    // Return true if it has at least one valid IGD.
    bool hasValidIgd() const;

    // Update and check the host (local) address. Returns true
    // if the address is valid.
    bool updateAndCheckHostAddress();

    // Delete mappings matching the description
    void deleteMappingsByDescription(const std::shared_ptr<IGD>& igd,
                                     const std::string& description);

    // Process the reception of an add mapping action answer.
    void processAddMapAction(const std::string& ctrlURL,
                             uint16_t ePort,
                             uint16_t iPort,
                             PortType portType);

    // Process the reception of a remove mapping action answer.
    void processRemoveMapAction(const std::string& ctrlURL,
                                uint16_t ePort,
                                uint16_t iPort,
                                PortType portType);

    // Validate IGD from the xml document received from the router.
    bool validateIgd(const IGDInfo&);

    // Returns control point action callback based on xml node.
    static CtrlAction getAction(const char* xmlNode);

    // Control point callback.
    static int ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
    static inline int ctrlPtCallback(Upnp_EventType event_type, void* event, void* user_data)
    {
        return ctrlPtCallback(event_type, (const void*) event, user_data);
    };
#endif
    // Process IGD responses.
    void processDiscoverySearchResult(const std::string& deviceId,
                                      const std::string& igdUrl,
                                      const IpAddr& dstAddr);
    void processDiscoveryAdvertisementByebye(const std::string& deviceId);
    void processDiscoverySubscriptionExpired(Upnp_EventType event_type,
                                             const std::string& eventSubUrl);

    // Callback event handler function for the UPnP client (control point).
    int handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event);

    // Subscription event callback.
    static int subEventCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
    static inline int subEventCallback(Upnp_EventType event_type, void* event, void* user_data)
    {
        return subEventCallback(event_type, (const void*) event, user_data);
    };
#endif

    // Callback subscription event function for handling subscription request.
    int handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event);

    // Parses the IGD candidate.
    std::unique_ptr<UPnPIGD> parseIgd(IXML_Document* doc, std::string locationUrl);

    // These functions directly create UPnP actions and make synchronous UPnP control point calls.
    // Assumes mutex is already locked.
    bool actionIsIgdConnected(const UPnPIGD& igd);
    IpAddr actionGetExternalIP(const UPnPIGD& igd);

    bool actionAddPortMapping(const std::shared_ptr<UPnPIGD>& igd, const Mapping& mapping);
    bool actionAddPortMappingAsync(const std::shared_ptr<UPnPIGD>& igd, const Mapping& mapping);
    bool actionDeletePortMappingAsync(const UPnPIGD& igd,
                                      const std::string& port_external,
                                      const std::string& protocol);
    bool actionDeletePortMapping(const std::shared_ptr<UPnPIGD>& igd, const Mapping& mapping);
    // Event type to string
    static const char* eventTypeToString(Upnp_EventType eventType);

    std::weak_ptr<PUPnP> weak() { return std::static_pointer_cast<PUPnP>(shared_from_this()); }

    // Initialization status.
    std::atomic_bool initialized_ {false};
    // Client registration status.
    std::atomic_bool clientRegistered_ {false};

    std::condition_variable pupnpCv_ {}; // Condition variable for thread-safe signaling.
    std::atomic_bool pupnpRun_ {false};  // Variable to allow the thread to run.
    std::thread pupnpThread_ {};         // PUPnP thread for non-blocking client registration.
    std::shared_ptr<Task> searchForIgdTimer_ {};
    unsigned int igdSearchCounter_ {0};

    mutable std::mutex validIgdListMutex_;
    std::list<std::shared_ptr<IGD>> validIgdList_; // List of valid IGDs.

    std::set<std::string> discoveredIgdList_; // UDN list of discovered IGDs.
    std::list<std::future<pIGDInfo>>
        dwnldlXmlList_; // List of futures for blocking xml download function calls.
    std::list<std::future<pIGDInfo>> cancelXmlList_; // List of abandoned documents

    mutable std::mutex igdListMutex_;     // Mutex used to protect IGD instances.
    std::mutex ctrlptMutex_;              // Mutex for client handle protection.
    UpnpClient_Handle ctrlptHandle_ {-1}; // Control point handle.

    std::atomic_bool searchForIgd_ {false}; // Variable to signal thread for a search.
};

} // namespace upnp
} // namespace jami
