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

#include "ip_utils.h" /* for IpAddr */

class IGDdatas; /* forward declaration */
class UPNPUrls; /* forward declaration */

namespace upnp {

/* defines a UPnP capable Internet Gateway Device (a router) */
class UPnPIGD {
public:
    const IGDdatas * datas;
    const UPNPUrls * urls;

    UPnPIGD(
        const IGDdatas * d = nullptr,
        const UPNPUrls * u = nullptr)
    : datas(d)
    , urls(u)
    {};

    ~UPnPIGD();

    bool isEmpty();
};

/* defines a UPnP redirection (port mapping) */
class UPnPRedirection {
public:
    const UPnPIGD& igd; /* the IGD associated with thie redirection */
    const unsigned int port_external;
    const unsigned int port_internal;
    const std::string type;

    UPnPRedirection(
        const UPnPIGD& igd,
        unsigned int port_external,
        unsigned int port_internal,
        const std::string& type = "UDP")
    : igd(igd)
    , port_external(port_external)
    , port_internal(port_internal)
    , type(type)
    {};
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
    bool addRedirection(unsigned int port_external, unsigned int port_internal, const std::string& type = "UDP");

    /**
     * tries to remove redirection
     */
    bool removeRedirection(unsigned int port_external, const std::string& type = "UDP");

    /**
     * removes all entries which have given description
     */
    void removeEntriesByDescription(const std::string& description);

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
     * TODO: keep list of redirections
     *
     * std::list<UPnPRedirection> redirections_;
     */
};

}

#endif /* UPNP_H_ */