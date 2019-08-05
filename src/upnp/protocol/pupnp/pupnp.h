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

#ifdef _WIN32
#define UPNP_USE_MSVCPP
#define UPNP_STATIC_LIB
#endif

#include "../upnp_protocol.h"
#include "../global_mapping.h"
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

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

// Error codes returned by router when trying to remove ports.
constexpr static int ARRAY_IDX_INVALID = 713;
constexpr static int CONFLICT_IN_MAPPING = 718;

// Timeout values (in seconds).
constexpr static unsigned int SEARCH_TIMEOUT {30};
constexpr static unsigned int SUBSCRIBE_TIMEOUT {300};

class PUPnP : public UPnPProtocol
{
public:
    PUPnP();
    ~PUPnP();

    // Returns the protocol type.
    Type getType() const override { return Type::PUPNP; }

    // Notifies a change in network.
    void clearIGDs() override;

    // Sends out async search for IGD.
    void searchForIGD() override;

    // Tries to add mapping. Assumes mutex is already locked.
    Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error) override;

    // Removes a mapping.
    void removeMapping(const Mapping& igdMapping) override;

    // Removes all local mappings of IGD that we're added by the application.
    void removeAllLocalMappings(IGD* igd) override;

private:
    // Register client in an async manner using a thread.
    void registerClientAsync();

    // Callback event handler function for the UPnP client (control point).
    int handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event);

    // Callback subscription event function for handling subscription request.
    int handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event);

    // Parses the IGD candidate.
    std::unique_ptr<UPnPIGD> parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event);

    // These functions directly create UPnP actions and make synchronous UPnP control point calls. Assumes mutex is already locked.
    bool   actionIsIgdConnected(const UPnPIGD& igd);
    IpAddr actionGetExternalIP(const UPnPIGD& igd);
    void   actionDeletePortMappingsByDesc(const UPnPIGD& igd, const std::string& description);
    bool   actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol);
    bool   actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, UPnPProtocol::UpnpError& error_code);

    // These functions directly create UPnP actions and make asynchronous UPnP control point calls. Assumes mutex is already locked.
    bool   actionDeletePortMappingAsync(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol);
    bool   actionAddPortMappingAsync(const UPnPIGD& igd, const Mapping& mapping, UPnPProtocol::UpnpError& error_code);

    // Control point callback.
    static int ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data);

    // Subscription event callback.
    static int subEventCallback(Upnp_EventType event_type, const void* event, void* user_data);

private:
    NON_COPYABLE(PUPnP);

    std::condition_variable pupnpCv_ {};                        // Condition variable for thread-safe signaling.
    std::atomic_bool pupnpRun_ { true };                        // Variable to allow the thread to run.
    std::thread pupnpThread_ {};                                // PUPnP thread for non-blocking client registration.

    std::map<std::string, std::shared_ptr<IGD>> validIgdList_;  // Map of valid IGDs with their UDN (universal Id).
    std::map<std::string, std::string> cpDeviceList_;           // Control point device list containing the device ID and device subscription event url.

    std::mutex ctrlptMutex_;                                    // Mutex for client handle protection.
    UpnpClient_Handle ctrlptHandle_ {-1};                       // Control point handle.
    std::atomic_bool clientRegistered_ { false };               // Indicates of the client is registered.
};

}} // namespace jami::upnp