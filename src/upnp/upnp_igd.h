/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include <string>
#include <map>
#include <functional>
#include <chrono>

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

namespace jami { namespace upnp {

enum class PortType {UDP,TCP};

/* defines a UPnP port mapping */
class Mapping {
public:
    constexpr static const char * UPNP_DEFAULT_MAPPING_DESCRIPTION = "RING";
    /* TODO: what should the port range really be?
     * Should it be the ephemeral ports as defined by the system?
     */
    constexpr static uint16_t UPNP_PORT_MIN = 1024;
    constexpr static uint16_t UPNP_PORT_MAX = 65535;

    Mapping(
        uint16_t port_external = 0,
        uint16_t port_internal = 0,
        PortType type = PortType::UDP,
        const std::string& description = UPNP_DEFAULT_MAPPING_DESCRIPTION)
    : port_external_(port_external)
    , port_internal_(port_internal)
    , type_(type)
    , description_(description)
    {};

    /* move constructor and operator */
    Mapping(Mapping&&) noexcept;
    Mapping& operator=(Mapping&&) noexcept;

    ~Mapping() = default;

    friend bool operator== (const Mapping& cRedir1, const Mapping& cRedir2);
    friend bool operator!= (const Mapping& cRedir1, const Mapping& cRedir2);

    uint16_t      getPortExternal()    const { return port_external_; }
    std::string   getPortExternalStr() const { return jami::to_string(port_external_); }
    uint16_t      getPortInternal()    const { return port_internal_; }
    std::string   getPortInternalStr() const { return jami::to_string(port_internal_); }
    PortType      getType()            const { return type_; }
    std::string   getTypeStr()         const { return type_ == PortType::UDP ? "UDP" : "TCP"; }
    std::string   getDescription()     const { return description_; }

    std::string toString() const {
        return getPortExternalStr() + ":" + getPortInternalStr() + ", " + getTypeStr();
    };

    bool isValid() const {
        return port_external_ == 0 or port_internal_ == 0 ? false : true;
    };

    inline explicit operator bool() const {
        return isValid();
    }

#if HAVE_LIBNATPMP
    std::chrono::system_clock::time_point renewal_ {std::chrono::system_clock::time_point::min()};
    bool remove {false};
#endif

private:
    NON_COPYABLE(Mapping);

protected:
    uint16_t port_external_;
    uint16_t port_internal_;
    PortType type_; /* UPD or TCP */
    std::string description_;
};

/**
 * GlobalMapping is like a mapping, but it tracks the number of global users,
 * ie: the number of upnp:Controller which are using this mapping
 * this is usually only relevant for accounts (not calls) as multiple SIP accounts
 * can use the same SIP port and we don't want to delete a mapping from the router
 * if other accounts are using it
 */
class GlobalMapping : public Mapping {
public:
    /* number of users of this mapping;
     * this is only relevant when multiple accounts are using the same SIP port */
    unsigned users;
    GlobalMapping(const Mapping& mapping, unsigned users = 1)
        : Mapping(mapping.getPortExternal()
        , mapping.getPortInternal()
        , mapping.getType()
        , mapping.getDescription())
        , users(users)
    {};
};

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
