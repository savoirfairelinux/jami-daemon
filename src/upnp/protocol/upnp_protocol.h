/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *    Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

namespace jami { namespace upnp {

// UPnP device descriptions.
constexpr static const char* UPNP_ROOT_DEVICE = "upnp:rootdevice";
constexpr static const char* UPNP_IGD_DEVICE = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static const char* UPNP_WAN_DEVICE = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static const char* UPNP_WANCON_DEVICE = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static const char* UPNP_WANIP_SERVICE = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static const char* UPNP_WANPPP_SERVICE = "urn:schemas-upnp-org:service:WANPPPConnection:1";

// Pure virtual interface class that UPnPContext uses to call protocol functions.
class UPnPProtocol
{
public:
    enum class UpnpError : int {
        INVALID_ERR = -1,
        ERROR_OK,
        CONFLICT_IN_MAPPING
    };

    enum class Type {
        UNKNOWN,
        PUPNP,
        NAT_PMP
    };

    using IgdListChangedCallback = std::function<bool(UPnPProtocol*, IGD*, IpAddr, bool)>;
    using NotifyContextCallback = std::function<void(IpAddr, Mapping, bool)>;

    UPnPProtocol(){};
    virtual ~UPnPProtocol(){};

    // Allows each protocol to return it's type.
    virtual Type getType() const = 0;

    // Clear all known IGDs.
    virtual void clearIgds() = 0;

    // Search for IGDs.
    virtual void searchForIgd() = 0;

    // Sends a request to add a mapping.
    virtual void requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type) = 0;

    // Sends a request to remove a mapping.
    virtual void requestMappingRemove(const Mapping& igdMapping) = 0;

    // Removes all local mappings of IGD that we're added by the application.
    virtual void removeAllLocalMappings(IGD* igd) = 0;

    // Set the IGD list callback handler.
    void setOnIgdChanged(IgdListChangedCallback&& cb) { updateIgdListCb_ = std::move(cb); }

    // Set the add port mapping callback handler.
    void setOnPortMapAdd(NotifyContextCallback&& cb) { notifyContextPortOpenCb_ = std::move(cb); }

    // Set the remove port mapping callback handler.
    void setOnPortMapRemove(NotifyContextCallback&& cb) { notifyContextPortCloseCb_ = std::move(cb); }

protected:
    mutable std::mutex validIgdMutex_;                  // Mutex used to protect IGDs.

    IgdListChangedCallback updateIgdListCb_;            // Callback for when the IGD list changes.
    NotifyContextCallback notifyContextPortOpenCb_;     // Callback for when a port mapping is added.
    NotifyContextCallback notifyContextPortCloseCb_;    // Callback for when a port mapping is removed.
};

}} // namespace jami::upnp