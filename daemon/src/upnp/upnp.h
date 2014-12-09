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
#include <memory>

#include "logger.h"
#include "ip_utils.h"

#if HAVE_UPNP
#include <miniupnpc/miniupnpc.h>
#endif

namespace upnp {

/* defines a UPnP capable Internet Gateway Device (a router) */
class IGD {
public:

#if HAVE_UPNP

    IGD(IGDdatas d = IGDdatas(), UPNPUrls u = UPNPUrls());

    ~IGD();

    IGDdatas* getDatas() {return datas_.get();};
    UPNPUrls* getURLs() {return urls_.get();};
#else
    /* use default constructor and destructor */
#endif
    bool isEmpty() const;

protected:

#if HAVE_UPNP
    void copyURLs(const UPNPUrls &urlsSource);

    std::shared_ptr<IGDdatas> datas_;
    std::shared_ptr<UPNPUrls> urls_;
#endif

};

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

    IpAddr local_ip; /* the destination of the mapping */
    uint16_t port_external;
    uint16_t port_internal;
    PortType type; /* UPD or TCP */
    /* number of users of this mapping;
     * this is only relevant when multiple accounts are using the same SIP port */
    std::string description;
    unsigned users;

    Mapping(
        IGD igd = IGD(),
        IpAddr local_ip = IpAddr(),
        uint16_t port_external = 0,
        uint16_t port_internal = 0,
        PortType type = PortType::UDP,
        std::string description = UPNP_DEFAULT_MAPPING_DESCRIPTION,
        unsigned users = 1)
    : igd_(std::make_shared<IGD>(igd))
    , local_ip(local_ip)
    , port_external(port_external)
    , port_internal(port_internal)
    , type(type)
    , description(description)
    , users(users)
    {};

    friend bool operator== (Mapping &cRedir1, Mapping &cRedir2);
    friend bool operator!= (Mapping &cRedir1, Mapping &cRedir2);

    IGD* getIGD() const { return igd_.get(); };
    std::string getExternalPort() const {return std::to_string(port_external);};
    std::string getInternalPort() const {return std::to_string(port_internal);};
    std::string getType() const { return type == PortType::UDP ? "UDP" : "TCP";};
    std::string toString() const {
        return local_ip.toString() + ", " + getExternalPort() + ":" + getInternalPort() + ", " + getType();
    };

    bool isValid() {
        return igd_.get()->isEmpty() or port_external == 0 or port_internal == 0 ? false : true;
    }
protected:
    std::shared_ptr<IGD> igd_; /* the IGD associated with this mapping */
};

class Controller {

public:
    /* constructor */
    Controller(bool enabled = false);
    /* destructor */
    ~Controller();

    /**
     * tries to add mapping from and to the port_desired
     * if unique == true, makes sure the client is not using this port already
     * if the mapping fails, tries other available ports until success
     *
     * tries to use a random port between 1024 < > 65535 if desired port fails
     *
     * maps port_desired to port_local; if use_same_port == true, makes sure that
     * that the extranl and internal ports are the same
     */
    bool addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool use_same_port, bool unique, uint16_t *port_used);

    /**
     * addAnyMapping with the local port being the same as the external port
     */
    bool addAnyMapping(uint16_t port_desired, PortType type, bool unique, uint16_t *port_used);

    /**
     * removes all mappings added by this instance
     */
    void removeMappings();

    /**
     * removes all mappings with the local IP and the given description
     */
    void removeMappingsByLocalIPAndDescription(const std::string& description = Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);

    /**
     * tries to get the external ip of the IGD (router)
     */
    IpAddr getExternalIP();

protected:
    /**
     * tracks if UPnP is enabled in this instance
     */
    bool enabled_ {false};

    /**
     * In general, we want to use the same IGD with all instances
     */
    std::shared_ptr<std::unique_ptr<IGD>> defaultIGD_;

    /**
     * selects the default IGD to use
     */
    bool chooseDefaultIGD(void);

    /**
     * list of mappings created by this instance
     * the key is the external port number, as there can only be one mapping
     * at a time for each external port
     */
    std::map<uint16_t, Mapping> udpInstanceMappings_;
    std::map<uint16_t, Mapping> tcpInstanceMappings_;

    /**
     * list of all mappings
     */
    std::shared_ptr<std::map<uint16_t, Mapping>> udpGlobalMappings_;
    std::shared_ptr<std::map<uint16_t, Mapping>> tcpGlobalMappings_;

    /**
     * tries to add mapping
     */
    bool addMapping(uint16_t port_external, uint16_t port_internal, PortType type = PortType::UDP, int *upnp_error = nullptr);

    /**
     * Try to remove all mappings of the given type
     */
    void removeMappings(PortType type);

    /**
     * chooses a random port that is not yet used by the daemon for UPnP
     */
    uint16_t chooseRandomPort(PortType type);


    /**
     * Getters for global variables
     */
    std::shared_ptr<std::unique_ptr<IGD>> createDefaultIGD();
    IGD* getDefaultIGD();
};

}

#endif /* UPNP_H_ */