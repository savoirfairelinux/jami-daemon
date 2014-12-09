/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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

#ifndef UPNP_H_
#define UPNP_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <map>

#include "ip_utils.h" /* for IpAddr */

class IGDdatas; /* forward declaration */
class UPNPUrls; /* forward declaration */

namespace upnp {

/* defines a UPnP capable Internet Gateway Device (a router) */
class UPnPIGD {
public:
    IGDdatas * datas;
    UPNPUrls * urls;

    UPnPIGD(
        IGDdatas * d = nullptr,
        UPNPUrls * u = nullptr)
    : datas(d)
    , urls(u)
    {};

    /* copy constructor */
    UPnPIGD(const UPnPIGD &igdSource);

    ~UPnPIGD();

    bool isEmpty();
};

enum class PortType {UDP,TCP};

/* defines a UPnP redirection (port mapping) */
class UPnPRedirection {
public:
    UPnPIGD igd; /* the IGD associated with this redirection */
    IpAddr local_ip; /* the destination of the redirection */
    uint16_t port_external;
    uint16_t port_internal;
    PortType type; /* UPD or TCP */

    UPnPRedirection(
        UPnPIGD igd = UPnPIGD(),
        IpAddr local_ip = IpAddr(),
        uint16_t port_external = 0,
        uint16_t port_internal = 0,
        PortType type = PortType::UDP)
    : igd(igd)
    , local_ip(local_ip)
    , port_external(port_external)
    , port_internal(port_internal)
    , type(type)
    {};

    std::string getExternalPort() const {return std::to_string(port_external);};
    std::string getInternalPort() const {return std::to_string(port_internal);};
    std::string getType() const { return type == PortType::UDP ? "UDP" : "TCP";};
    std::string toString() const {
        return local_ip.toString() + ", " + getExternalPort() + ":" + getInternalPort() + ", " + getType();
    };
};

class UPnP {

public:
    /* constructor */
    UPnP();
    /* use default destructor */

    /**
     * returns if a default IGD is defined
     */
    bool hasDefaultIGD(void) const;

    /**
     * tries to add redirection
     */
    bool addRedirection(uint16_t port_external, uint16_t port_internal, PortType type = PortType::UDP);

    /**
     * tries to remove redirection
     * if existing is true, only removes entries which were added by this instance
     */
    bool removeRedirection(uint16_t port_external, PortType type = PortType::UDP, bool existing = true);

    /**
     * removes all entries which have given description
     */
    void removeEntriesByDescription(const std::string& description);

    /**
     * removes all entries added by this instance
     */
    void removeEntries();

    /**
     * tries to get the external ip of the router
     */
    IpAddr getExternalIP();

protected:
    /**
     * In general, we want to use the same IGD with all instances
     */
    static UPnPIGD * defaultIGD_;

    /**
     * selects the default IGD to use
     */
    bool chooseDefaultIGD(void);

    /**
     * list of redirections created by this instance
     * the key is the external port number, as there can only be one mapping
     * at a time for each external port
     */
    std::map<uint16_t, UPnPRedirection> udp_mappings_;
    std::map<uint16_t, UPnPRedirection> tcp_mappings_;

    /**
     * Try to remove all entries of the given tyep
     */
    void removeEntries(PortType type);
};

}

#endif /* UPNP_H_ */