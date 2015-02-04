/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef UPNP_IGD_H_
#define UPNP_IGD_H_

#include <string>
#include <map>

#include "noncopyable.h"
#include "logger.h"
#include "ip_utils.h"

namespace ring { namespace upnp {

enum class PortType {UDP,TCP};

/* defines a UPnP port mapping */
class Mapping {
public:
    constexpr static char const * UPNP_DEFAULT_MAPPING_DESCRIPTION = "RING";
    /* TODO: what should the port range really be?
     * Should it be the ephemeral ports as defined by the system?
     */
    constexpr static uint16_t UPNP_PORT_MIN = 1024;
    constexpr static uint16_t UPNP_PORT_MAX = 65535;

    Mapping(
        IpAddr local_ip = IpAddr(),
        uint16_t port_external = 0,
        uint16_t port_internal = 0,
        PortType type = PortType::UDP,
        std::string description = UPNP_DEFAULT_MAPPING_DESCRIPTION)
    : local_ip_(local_ip)
    , port_external_(port_external)
    , port_internal_(port_internal)
    , type_(type)
    , description_(description)
    {};

    /* move constructor and operator */
    Mapping(Mapping&&);
    Mapping& operator=(Mapping&&);

    ~Mapping() = default;

    friend bool operator== (Mapping &cRedir1, Mapping &cRedir2);
    friend bool operator!= (Mapping &cRedir1, Mapping &cRedir2);

    const IpAddr& getLocalIp()         const { return local_ip_; };
    uint16_t      getPortExternal()    const { return port_external_; };
    std::string   getPortExternalStr() const { return std::to_string(port_external_); };
    uint16_t      getPortInternal()    const { return port_internal_; };
    std::string   getPortInternalStr() const { return std::to_string(port_internal_); };
    PortType      getType()            const { return type_; };
    std::string   getTypeStr()         const { return type_ == PortType::UDP ? "UDP" : "TCP"; }
    std::string   getDescripton()      const { return description_; };

    std::string toString() const {
        return local_ip_.toString() + ", " + getPortExternalStr() + ":" + getPortInternalStr() + ", " + getTypeStr();
    };

    bool isValid() const {
        return port_external_ == 0 or port_internal_ == 0 ? false : true;
    };

private:
    NON_COPYABLE(Mapping);

protected:
    IpAddr local_ip_; /* the destination of the mapping */
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
        : Mapping(mapping.getLocalIp(), mapping.getPortExternal(), mapping.getPortInternal(), mapping.getType(), mapping.getDescripton())
        , users(users)
    {};
};

/* subclasses to make it easier to differentiate and cast maps of port mappings */
class PortMapLocal : public std::map<uint16_t, Mapping> {};
class UDPMapLocal : public PortMapLocal {};
class TCPMapLocal : public PortMapLocal {};
class PortMapGlobal : public std::map<uint16_t, GlobalMapping> {};
class UDPMapGlobal: public PortMapGlobal {};
class TCPMapGlobal : public PortMapGlobal {};

/* defines a UPnP capable Internet Gateway Device (a router) */
class IGD {
public:

    /* external IP of IGD; can change */
    IpAddr publicIp;

    /* constructors */
    IGD() {};
    IGD(std::string UDN,
        std::string baseURL,
        std::string friendlyName,
        std::string serviceType,
        std::string serviceId,
        std::string controlURL,
        std::string eventSubURL)
        : UDN_(UDN)
        , baseURL_(baseURL)
        , friendlyName_(friendlyName)
        , serviceType_(serviceType)
        , serviceId_(serviceId)
        , controlURL_(controlURL)
        , eventSubURL_(eventSubURL)
        {};

    /* move constructor and operator */
    IGD(IGD&&) = default;
    IGD& operator=(IGD&&) = default;

    ~IGD() = default;

    const std::string& getUDN() const { return UDN_; };
    const std::string& getBaseURL() const { return baseURL_; };
    const std::string& getFriendlyName() const { return friendlyName_; };
    const std::string& getServiceType() const { return serviceType_; };
    const std::string& getServiceId() const { return serviceId_; };
    const std::string& getControlURL() const { return controlURL_; };
    const std::string& getEventSubURL() const { return eventSubURL_; };

private:
    NON_COPYABLE(IGD);

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

}} // namespace ring::upnp

#endif /* UPNP_IGD_H_ */