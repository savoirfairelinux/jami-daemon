/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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

#include "manager.h"
#include "igd.h"
#include "mapping.h"

#include "logger.h"
#include "noncopyable.h"
#include "connectivity/ip_utils.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <condition_variable>
#include <list>

#include "connectivity/upnp/upnp_thread_util.h"

namespace jami {
namespace upnp {

// UPnP device descriptions.
constexpr static const char* UPNP_ROOT_DEVICE = "upnp:rootdevice";
constexpr static const char* UPNP_IGD_DEVICE
    = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static const char* UPNP_WAN_DEVICE = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static const char* UPNP_WANCON_DEVICE
    = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static const char* UPNP_WANIP_SERVICE = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static const char* UPNP_WANPPP_SERVICE
    = "urn:schemas-upnp-org:service:WANPPPConnection:1";

enum class UpnpIgdEvent { ADDED, REMOVED, INVALID_STATE };

// Interface used to report mapping event from the protocol implementations.
// This interface is meant to be implemented only by UPnPConext class. Sincce
// this class is a singleton, it's assumed that it out-lives the protocol
// implementations. In other words, the observer is always assumed to point to a
// valid instance.
class UpnpMappingObserver
{
public:
    UpnpMappingObserver() {};
    virtual ~UpnpMappingObserver() {};

    virtual void onIgdUpdated(const std::shared_ptr<IGD>& igd, UpnpIgdEvent event) = 0;
    virtual void onMappingAdded(const std::shared_ptr<IGD>& igd, const Mapping& map) = 0;
    virtual void onMappingRequestFailed(const Mapping& map) = 0;
#if HAVE_LIBNATPMP
    virtual void onMappingRenewed(const std::shared_ptr<IGD>& igd, const Mapping& map) = 0;
#endif
    virtual void onMappingRemoved(const std::shared_ptr<IGD>& igd, const Mapping& map) = 0;
};

// Pure virtual interface class that UPnPContext uses to call protocol functions.
class UPnPProtocol : public std::enable_shared_from_this<UPnPProtocol>, protected UpnpThreadUtil
{
public:
    enum class UpnpError : int { INVALID_ERR = -1, ERROR_OK, CONFLICT_IN_MAPPING };

    UPnPProtocol() {};
    virtual ~UPnPProtocol() {};

    // Get protocol type.
    virtual NatProtocolType getProtocol() const = 0;

    // Get protocol type as string.
    virtual char const* getProtocolName() const = 0;

    // Clear all known IGDs.
    virtual void clearIgds() = 0;

    // Search for IGD.
    virtual void searchForIgd() = 0;

    // Get the IGD instance.
    virtual std::list<std::shared_ptr<IGD>> getIgdList() const = 0;

    // Return true if it has at least one valid IGD.
    virtual bool isReady() const = 0;

    // Get the list of already allocated mappings if any.
    virtual std::map<Mapping::key_t, Mapping> getMappingsListByDescr(const std::shared_ptr<IGD>&,
                                                                     const std::string&) const
    {
        return {};
    }

    // Sends a request to add a mapping.
    virtual void requestMappingAdd(const Mapping& map) = 0;

    // Renew an allocated mapping.
    virtual void requestMappingRenew(const Mapping& mapping) = 0;

    // Sends a request to remove a mapping.
    virtual void requestMappingRemove(const Mapping& igdMapping) = 0;

    // Set the user callbacks.
    virtual void setObserver(UpnpMappingObserver* obs) = 0;

    // Get the current host (local) address
    virtual const IpAddr getHostAddress() const = 0;

    // Terminate
    virtual void terminate() = 0;
};

} // namespace upnp
} // namespace jami
