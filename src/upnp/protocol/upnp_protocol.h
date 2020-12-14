/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
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

#include "manager.h"
#include "igd.h"
#include "mapping.h"

#include "logger.h"
#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <condition_variable>
#include <list>

#include "upnp/upnp_thread_util.h"

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

// Pure virtual interface class that UPnPContext uses to call protocol functions.
class UPnPProtocol : public std::enable_shared_from_this<UPnPProtocol>, protected UpnpThreadUtil
{
public:
    enum class UpnpError : int { INVALID_ERR = -1, ERROR_OK, CONFLICT_IN_MAPPING };

    using IgdUpdateCallback
        = std::function<void(std::shared_ptr<UPnPProtocol>, std::shared_ptr<IGD>, UpnpIgdEvent)>;
    using NotifyCallback = std::function<void(std::shared_ptr<IGD>, Mapping)>;

    UPnPProtocol() {};
    virtual ~UPnPProtocol() {};

    // Get protocol type.
    virtual NatProtocolType getProtocol() const = 0;

    // Get protocol type as string.
    virtual std::string getProtocolName() const = 0;

    // Clear all known IGDs.
    virtual void clearIgds() = 0;

    // Search for IGD.
    virtual void searchForIgd() = 0;

    // Get the IGD instance.
    virtual void getIgdList(std::list<std::shared_ptr<IGD>>& igdList) const = 0;

    // Return true if it has at least one valid IGD.
    virtual bool hasValidIgd() const = 0;

    // Increment IGD errors counter.
    virtual void incrementErrorsCounter(const std::shared_ptr<IGD>& igd) = 0;

    // Get the list of already allocated mappings if any.
    virtual std::unique_ptr<std::map<Mapping::key_t, Mapping>> getMappingsListByDescr(
        [[maybe_unused]] const std::shared_ptr<IGD>& igd,
        [[maybe_unused]] const std::string& descr) const
    {
        return nullptr;
    }

    // Sends a request to add a mapping.
    virtual void requestMappingAdd(const IGD* igd, const Mapping& map) = 0;
    // Sends a request to remove a mapping.
    virtual void requestMappingRemove(const Mapping& igdMapping) = 0;

    // Set the IGD list callback handler.
    void setOnIgdUpdate(IgdUpdateCallback&& cb) { userCallbacks_.onIgdUpdate_ = std::move(cb); }

    // Set the add port mapping callback handler.
    void setOnPortMapAdd(NotifyCallback&& cb)
    {
        userCallbacks_.notifyRequestAddCb_ = std::move(cb);
    }

    // Set the remove port mapping callback handler.
    void setOnPortMapRemove(NotifyCallback&& cb)
    {
        userCallbacks_.notifyRequestRemoveCb_ = std::move(cb);
    }

    struct UserCallbacks
    {
        IgdUpdateCallback onIgdUpdate_;
        NotifyCallback notifyRequestAddCb_;
        NotifyCallback notifyRequestRemoveCb_;
    };

    struct UserCallbacks userCallbacks_;
};

} // namespace upnp
} // namespace jami
