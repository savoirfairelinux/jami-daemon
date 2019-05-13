// /*
//  *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
//  *
//  *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
//  *
//  *  This program is free software; you can redistribute it and/or modify
//  *  it under the terms of the GNU General Public License as published by
//  *  the Free Software Foundation; either version 3 of the License, or
//  *  (at your option) any later version.
//  *
//  *  This program is distributed in the hope that it will be useful,
//  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  *  GNU General Public License for more details.
//  *
//  *  You should have received a copy of the GNU General Public License
//  *  along with this program; if not, write to the Free Software
//  *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
//  */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <map>
#include <functional>
#include <chrono>

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include "global_mapping.h"

namespace jami { namespace upnp {

/* subclasses to make it easier to differentiate and cast maps of port mappings */
class PortMapLocal : public std::map<uint16_t, Mapping> {};
class PortMapGlobal : public std::map<uint16_t, GlobalMapping> {};

using IGDFoundCallback = std::function<void()>;

/* defines a UPnP capable Internet Gateway Device (a router) */
class IGD {
public:
    /* device address seen by IGD */
    IpAddr localIp;

    /* external IP of IGD; can change */
    IpAddr publicIp;

    /* port mappings associated with this IGD */
    PortMapGlobal udpMappings;
    PortMapGlobal tcpMappings;

    /* constructors */
    IGD() {}

    /* move constructor and operator */
    IGD(IGD&&) = default;
    IGD& operator=(IGD&&) = default;

    virtual ~IGD() = default;

private:
    NON_COPYABLE(IGD);
};

#if HAVE_LIBUPNP

class UPnPIGD : public IGD {
public:
    UPnPIGD(std::string&& UDN,
        std::string&& baseURL,
        std::string&& friendlyName,
        std::string&& serviceType,
        std::string&& serviceId,
        std::string&& controlURL,
        std::string&& eventSubURL)
        : UDN_(std::move(UDN))
        , baseURL_(std::move(baseURL))
        , friendlyName_(std::move(friendlyName))
        , serviceType_(std::move(serviceType))
        , serviceId_(std::move(serviceId))
        , controlURL_(std::move(controlURL))
        , eventSubURL_(std::move(eventSubURL))
        {}

    const std::string& getUDN() const { return UDN_; };
    const std::string& getBaseURL() const { return baseURL_; };
    const std::string& getFriendlyName() const { return friendlyName_; };
    const std::string& getServiceType() const { return serviceType_; };
    const std::string& getServiceId() const { return serviceId_; };
    const std::string& getControlURL() const { return controlURL_; };
    const std::string& getEventSubURL() const { return eventSubURL_; };

private:
    /* root device info */
    std::string UDN_ {}; /* used to uniquely identify this UPnP device */
    std::string baseURL_ {};
    std::string friendlyName_ {};

    /* port forwarding service info */
    std::string serviceType_ {};
    std::string serviceId_ {};
    std::string controlURL_ {};
    std::string eventSubURL_ {};
};

#endif

#if HAVE_LIBNATPMP

using clock = std::chrono::system_clock;
using time_point = clock::time_point;

class PMPIGD : public IGD {
public:
    void clear() {
        toRemove_.clear();
        udpMappings.clear();
        tcpMappings.clear();
    }

    void clearMappings() {
        clear();
        clearAll_ = true;
    }

    GlobalMapping* getNextMappingToRenew() const {
        const GlobalMapping* mapping {nullptr};
        for (const auto& m : udpMappings)
            if (!mapping or m.second.renewal_ < mapping->renewal_)
                mapping = &m.second;
        for (const auto& m : tcpMappings)
            if (!mapping or m.second.renewal_ < mapping->renewal_)
                mapping = &m.second;
        return (GlobalMapping*)mapping;
    }

    time_point getRenewalTime() const {
        const auto next = getNextMappingToRenew();
        auto nextTime = std::min(renewal_, next ? next->renewal_ : time_point::max());
        return toRemove_.empty() ? nextTime : std::min(nextTime, time_point::min());
    }

    time_point renewal_ {time_point::min()};
    std::vector<GlobalMapping> toRemove_ {};
    bool clearAll_ {false};
};

#endif

}} // namespace jami::upnp
