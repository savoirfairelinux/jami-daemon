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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <random>
#include <memory>

#if HAVE_UPNP
#include <miniupnpc/upnperrors.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#endif

#include "logger.h"
#include "ip_utils.h"
#include "ring_types.h"

namespace ring { namespace upnp {

#if HAVE_UPNP

static void
resetURLs(UPNPUrls& urls)
{
    urls.controlURL = nullptr;
    urls.ipcondescURL = nullptr;
    urls.controlURL_CIF = nullptr;
    urls.controlURL_6FC = nullptr;
#ifdef MINIUPNPC_VERSION /* if not defined, its version 1.6 */
    urls.rootdescURL = nullptr;
#endif
}

/* move constructor */
IGD::IGD(IGD&& other)
    : datas_(other.datas_)
    , urls_(other.urls_)
{
    resetURLs(other.urls_);
}

/* move operator */
IGD& IGD::operator=(IGD&& other)
{
    datas_ = other.datas_;
    urls_ = other.urls_;
    resetURLs(other.urls_);
    return *this;
}

IGD::~IGD()
{
    /* free the URLs */
    FreeUPNPUrls(&urls_);
}

#endif /* HAVE_UPNP */

/**
 * removes all mappings with the local IP and the given description
 */
void
IGD::removeMappingsByLocalIPAndDescription(const std::string& description)
{
#if HAVE_UPNP
    if (isEmpty())
        return;

    /* need to get the local addr */
    IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
    if (!local_ip) {
        RING_DBG("UPnP : cannot determine local IP");
        return;
    }

    int upnp_status;
    int i = 0;
    char index[6];
    char intClient[40];
    char intPort[6];
    char extPort[6];
    char protocol[4];
    char desc[80];
    char enabled[6];
    char rHost[64];
    char duration[16];

    RING_DBG("UPnP : removing all port mappings with description: \"%s\" and local ip: %s",
             description.c_str(), local_ip.toString().c_str());

    do {
        snprintf(index, 6, "%d", i);
        rHost[0] = '\0';
        enabled[0] = '\0';
        duration[0] = '\0';
        desc[0] = '\0';
        extPort[0] = '\0';
        intPort[0] = '\0';
        intClient[0] = '\0';
        upnp_status = UPNP_GetGenericPortMappingEntry(getURLs().controlURL,
                                                      getDatas().first.servicetype,
                                                      index,
                                                      extPort, intClient, intPort,
                                                      protocol, desc, enabled,
                                                      rHost, duration);
        if(upnp_status == UPNPCOMMAND_SUCCESS) {
            /* remove if matches description and ip
             * once the port mapping is deleted, there will be one less, and the rest will "move down"
             * that is, we don't need to increment the mapping index in that case
             */
            if( strcmp(description.c_str(), desc) == 0 and strcmp(local_ip.toString().c_str(), intClient) == 0) {
                RING_DBG("UPnP : found mapping with matching description and ip:\n\t%s %5s->%s:%-5s '%s'",
                         protocol, extPort, intClient, intPort, desc);
                int delete_err = 0;
                delete_err = UPNP_DeletePortMapping(getURLs().controlURL, getDatas().first.servicetype, extPort, protocol, NULL);
                if(delete_err != UPNPCOMMAND_SUCCESS) {
                    RING_DBG("UPnP : UPNP_DeletePortMapping() failed with error code %d : %s", delete_err, strupnperror(delete_err));
                } else {
                    RING_DBG("UPnP : deletion success");
                    /* decrement the mapping index since it will be incremented */
                    i--;
                }
            }
        } else if (upnp_status == 713) {
            /* 713 : SpecifiedArrayIndexInvalid
             * this means there are no more mappings to check, and we're done
             */
        } else {
            RING_DBG("UPnP : GetGenericPortMappingEntry() failed with error code %d : %s", upnp_status, strupnperror(upnp_status));
        }
        i++;
    } while(upnp_status == UPNPCOMMAND_SUCCESS);
#endif
}

/**
 * checks if the instance of IGD is empty
 * ie: not actually an IGD
 */
bool
IGD::isEmpty() const
{
#if HAVE_UPNP
    if (urls_.controlURL != nullptr) {
        if (urls_.controlURL[0] == '\0') {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
#else
    return true;
#endif
}

bool operator== (Mapping &cMap1, Mapping &cMap2)
{
    /* we don't compare the description because it doesn't change the function of the
     * mapping; we don't compare the IGD because for now we assume that we always
     * use the same one and that all mappings are active
     */
    return (cMap1.local_ip == cMap2.local_ip &&
            cMap1.port_external == cMap2.port_external &&
            cMap1.port_internal == cMap2.port_internal &&
            cMap1.type == cMap2.type);
}

bool operator!= (Mapping &cMap1, Mapping &cMap2)
{
    return !(cMap1 == cMap2);
}

Controller::Controller()
    : defaultIGD_(getIGD())
    , udpGlobalMappings_(getGlobalInstance<UDPMapGlobal>())
    , tcpGlobalMappings_(getGlobalInstance<TCPMapGlobal>())
{}

Controller::~Controller()
{
    /* remove all mappings */
    removeMappings();
}

/**
 * Return whether or not this controller has a valid IGD,
 * if 'flase' then all requests will fail
 */
bool
Controller::hasValidIGD()
{
#if HAVE_UPNP
    return not defaultIGD_->isEmpty();
#endif
    return false;
}

/**
 * tries to add mapping
 */
bool
Controller::addMapping(uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error)
{
    /* initialiaze upnp_error */
    if (upnp_error)
        *upnp_error = -1; /* UPNPCOMMAND_UNKNOWN_ERROR */
#if HAVE_UPNP
    if (defaultIGD_->isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return false;
    }

    int upnp_status;

    IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
    if (!local_ip) {
        RING_DBG("UPnP : cannot determine local IP");
        return false;
    }

    Mapping mapping{defaultIGD_, local_ip, port_external, port_internal};

    /* check if a mapping with the same external port already exists in this instance
     * if one exists, check that it is the same, if so, nothing needs to be done
     * if it is not the same, then we can't add it
     */
    auto& instanceMappings = type == PortType::UDP ? (PortMapLocal&)udpInstanceMappings_ : (PortMapLocal&)tcpInstanceMappings_;
    auto instanceIter = instanceMappings.find(port_external);
    if (instanceIter != instanceMappings.end()) {
        /* mapping exists with the same exteran port */
        Mapping *mapping_ptr = &instanceIter->second;
        if (*mapping_ptr == mapping) {
            /* the same mapping has already been mapped by this instance, nothing to do */
            RING_DBG("UPnP : mapping has already been added previously.");
            if (upnp_error)
                *upnp_error = UPNPCOMMAND_SUCCESS;
            return true;
        } else {
            /* this port is already used by a different mapping */
            RING_WARN("UPnP : cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            if (upnp_error)
                *upnp_error = 718; /* ConflictInMappingEntry */
            return false;
        }
    }

    /* mapping doesn't exist in this instance, check if it was added by another
     * if the mapping is the same, then we just need to increment the number of users globally
     * if the mapping is not the same, then we have to return fail, as the external port is used
     * for something else
     * if the mapping doesn't exist, then try to add it
     */
    auto globalMappings = type == PortType::UDP ? (PortMapGlobal*)udpGlobalMappings_.get() : (PortMapGlobal*)tcpGlobalMappings_.get();
    auto iter = globalMappings->find(port_external);
    if (iter != globalMappings->end()) {
        /* mapping exists with same external port */
        GlobalMapping *mapping_ptr = &iter->second;
        if (*mapping_ptr == mapping) {
            /* the same mapping, so nothing needs to be done */
            RING_DBG("UPnP : mapping was already added.");
            if (upnp_error)
                *upnp_error = UPNPCOMMAND_SUCCESS;
            /* increment users */
            mapping_ptr->users++;
            RING_DBG("UPnp : number of users of mapping should now be incremented: %d", iter->second.users);
            /* add it to map of instance mappings */
            instanceMappings.emplace(std::make_pair(port_external,std::move(mapping)));
            return true;
        } else {
            /* this port is already used by a different mapping */
            RING_WARN("UPnP : cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            if (upnp_error)
                *upnp_error = 718; /* ConflictInMappingEntry */
            return false;
        }
    }

    /* mapping doesn't exist, so try to add it */
    RING_DBG("UPnP : adding port mapping : %s", mapping.toString().c_str());

    upnp_status = UPNP_AddPortMapping(mapping.igd->getURLs().controlURL,
                                      mapping.igd->getDatas().first.servicetype,
                                      mapping.getExternalPort().c_str(),
                                      mapping.getInternalPort().c_str(),
                                      mapping.local_ip.toString().c_str(),
                                      mapping.description.c_str(),
                                      mapping.getType().c_str(),
                                      NULL, NULL);

    if (upnp_error)
        *upnp_error = upnp_status;

    if(upnp_status!=UPNPCOMMAND_SUCCESS) {
        RING_DBG("UPnP : AddPortMapping(%s) failed with error: %d: %s",
                 mapping.toString().c_str(), upnp_status, strupnperror(upnp_status));
        return false;
    } else {
        /* success; add it to global list and local list */
        globalMappings->emplace(std::make_pair(port_external, std::move(GlobalMapping{mapping})));
        instanceMappings.emplace(std::make_pair(port_external,std::move(mapping)));
        return true;
    }
#else
    return false;
#endif
}

/**
 * tries to add mapping from and to the port_desired
 * if unique == true, makes sure the client is not using this port already
 * if the mapping fails, tries other available ports until success
 *defaultIGD_
 * tries to use a random port between 1024 < > 65535 if desired port fails
 *
 * maps port_desired to port_local; if use_same_port == true, makes sure that
 * that the extranl and internal ports are the same
 */
bool
Controller::addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool use_same_port, bool unique, uint16_t *port_used)
{
#if HAVE_UPNP
    if (defaultIGD_->isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return false;
    }

    auto globalMappings = type == PortType::UDP ? (PortMapGlobal*)udpGlobalMappings_.get() : (PortMapGlobal*)tcpGlobalMappings_.get();
    if (unique) {
        /* check that port is not already used by the client */
        auto iter = globalMappings->find(port_desired);
        if (iter != globalMappings->end()) {
            /* port already used, we need a unique port */
            port_desired = chooseRandomPort(type);
        }
    }

    if (use_same_port)
        port_local = port_desired;

    int upnp_error;
    bool result = addMapping(port_desired, port_local, type, &upnp_error);
    /* keep trying to add the mapping as long as the upnp error is 718 == conflicting mapping
     * if adding the mapping fails for any other reason, give up
     */
    while( result == false and (upnp_error == 718 or upnp_error == 402) ) {
        /* acceptable errors to keep trying:
         * 718 : conflictin mapping
         * 402 : invalid args (due to router implementation)
         */
        /* TODO: make sure we don't try sellecting the same random port twice if it fails ? */
        port_desired = chooseRandomPort(type);
        if (use_same_port)
            port_local = port_desired;
        result = addMapping(port_desired, port_local, type, &upnp_error);
    }

    *port_used = port_desired;
    return result;
#else
    return false;
#endif
}

/**
 * addAnyMapping with the local port being the same as the external port
 */
bool
Controller::addAnyMapping(uint16_t port_desired, PortType type, bool unique, uint16_t *port_used) {
    addAnyMapping(port_desired, port_desired, type, true, unique, port_used);
}

/**
 * chooses a random port that is not yet used by the daemon for UPnP
 */
uint16_t
Controller::chooseRandomPort(PortType type)
{
    auto globalMappings = type == PortType::UDP ? (PortMapGlobal*)udpGlobalMappings_.get() : (PortMapGlobal*)tcpGlobalMappings_.get();

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<uint16_t> dist(Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX); // define the range

    uint16_t port = dist(gen);

    /* keep generating random ports until we find one which is not used */
    while(globalMappings->find(port) != globalMappings->end()) {
        port = dist(gen);
    }

    RING_DBG("UPnP : chose random port %u", port);

    return port;
}

/**
 * removes mappings added by this instance of the specified port type
 * if an mapping has more than one user in the global list, it is not deleted
 * from the router, but the number of users is decremented
 */
void
Controller::removeMappings(PortType type) {
#if HAVE_UPNP
    auto& instanceMappings = type == PortType::UDP ? (PortMapLocal&)udpInstanceMappings_ : (PortMapLocal&)tcpInstanceMappings_;
    for (auto instanceIter = instanceMappings.begin(); instanceIter != instanceMappings.end(); ){
        /* first check if there is more than one global user of this mapping */
        int users = 1;
        auto globalMappings = type == PortType::UDP ? (PortMapGlobal*)udpGlobalMappings_.get() : (PortMapGlobal*)tcpGlobalMappings_.get();
        auto iter = globalMappings->find(instanceIter->first);
        if ( iter != globalMappings->end() ) {
            GlobalMapping *mapping_ptr = &iter->second;
            users = mapping_ptr->users;

            if ( users > 1 ) {
                /* just decrement the users */
                mapping_ptr->users--;
            } else {
                /* remove the mapping from the global list */
                globalMappings->erase(iter);
            }
        }

        /* now remove the mapping from the instance list...
         * even if the delete operation fails, we assume that the mapping was deleted
         */
        auto& mapping = instanceIter->second;
        instanceIter = instanceMappings.erase(instanceIter);

        /* delete the mapping from the router if this instance was the only user */
        if (users < 2) {
            RING_DBG("UPnP : deleting mapping: %s", mapping.toString().c_str());
            int upnp_status = UPNP_DeletePortMapping(mapping.igd->getURLs().controlURL,
                                                     mapping.igd->getDatas().first.servicetype,
                                                     mapping.getExternalPort().c_str(),
                                                     mapping.getType().c_str(),
                                                     NULL);
            if(upnp_status != UPNPCOMMAND_SUCCESS) {
                RING_DBG("UPnP : DeletePortMapping(%s) failed with error: %d: %s",
                         mapping.toString().c_str(), upnp_status, strupnperror(upnp_status));
            } else {
                /* RING_DBG("UPnP : deletion success"); */
            }
        } else {
            RING_DBG("UPnP : removing mapping: %s, but not deleting because there are %d other users",
                     mapping.toString().c_str(), users - 1);
        }
    }
#endif
}

/**
 * removes all mappings added by this instance
 */
void
Controller::removeMappings()
{
#if HAVE_UPNP
    removeMappings(PortType::UDP);
    removeMappings(PortType::TCP);
#endif
}

/**
 * tries to get the external ip of the router
 */
IpAddr
Controller::getExternalIP()
{
#if HAVE_UPNP
    if (defaultIGD_->isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return {};
    }

    int upnp_status;
    char externalIPAddress[40];

    RING_DBG("UPnP : getting external IP");

    upnp_status = UPNP_GetExternalIPAddress(defaultIGD_->getURLs().controlURL,
                                            defaultIGD_->getDatas().first.servicetype,
                                            externalIPAddress);
    if(upnp_status != UPNPCOMMAND_SUCCESS) {
        RING_DBG("UPnP : GetExternalIPAddress failed with error code %d : %s", upnp_status, strupnperror(upnp_status));
        return {};
    } else {
        RING_DBG("UPnP : got external IP = %s", externalIPAddress);
        return {std::string(externalIPAddress)};
    }
#else
    /* return empty address */
    return {};
#endif
}

/**
 * tries to find a valid IGD on the network
 */
static IGD
chooseIGD(void)
{
#if HAVE_UPNP
    std::unique_ptr<UPNPDev, decltype(freeUPNPDevlist)&> devlist(nullptr, freeUPNPDevlist);
    int upnp_status = 0;

    RING_DBG("UPnP : finding default IGD");

    /* look for UPnP devices on the network */
    devlist.reset(upnpDiscover(2000, NULL, NULL, 0, 0, &upnp_status));

    if (devlist) {
        UPNPDev * device = nullptr;
        std::unique_ptr<UPNPUrls, decltype(FreeUPNPUrls)&> newIGDURLs(new UPNPUrls, FreeUPNPUrls);
        std::unique_ptr<IGDdatas> newIGDDatas(new IGDdatas);
        char lanaddr[64];

        RING_DBG("UPnP devices found on the network");
        for(device = devlist.get(); device; device = device->pNext)
        {
            RING_DBG(" desc: %s\n st: %s",
                     device->descURL, device->st);
        }

        upnp_status = UPNP_GetValidIGD(devlist.get(), newIGDURLs.get(), newIGDDatas.get(), lanaddr, sizeof(lanaddr));

        switch(upnp_status) {
            case -1:
                RING_ERR("UPnP : internal error getting valid IGD");
                break;
            case 0:
                RING_WARN("UPnP : no valid IGD found");
                break;
            case 1:
                RING_DBG("UPnP : found valid IGD : %s", newIGDURLs->controlURL);
                break;
            case 2:
                RING_WARN("UPnP : found an IGD, but it does not seem to be connected : %s", newIGDURLs->controlURL);
                break;
            case 3:
                RING_WARN("UPnP : UPnP device found, but it does not seem to be an IGD : %s", newIGDURLs->controlURL);
                break;
            default:
                RING_WARN("UPnP : device found, but cannot determine if it is a UPnP device : %s", newIGDURLs->controlURL);
        }

        /* only accept IGD if it was determined to be valid */
        if (upnp_status == 1) {
            RING_DBG("UPnP : local IP address reported as: %s", lanaddr);
            return {*newIGDDatas.release(), *newIGDURLs.release()};
        } else {
            return {};
        }
    } else {
        RING_WARN("UPnP : looking for IGD UPnP devices on the network failed with error: %d : %s", upnp_status, strupnperror(upnp_status));
        return {};
    }

#else
    return {};
#endif
}

static void
clean_igd(IGD* igd)
{
    if (igd and not igd->isEmpty()) {
        RING_DBG("UPnP : removing all RING mappings before deleting shared IGD object");
        igd->removeMappingsByLocalIPAndDescription(Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
    }
    delete igd;
};

std::shared_ptr<IGD>
getIGD(void)
{
    static std::shared_ptr<IGD> igd(nullptr, clean_igd);

    if (not igd) {
        igd = std::make_shared<IGD>(chooseIGD());
        if (not igd->isEmpty()) {
            /* remove any old RING mappings the first time we find an IGD */
            RING_DBG("UPnP : removing any existing RING mappings for new IGD");
            igd->removeMappingsByLocalIPAndDescription(Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
        }
    }

    return igd;
}

}} // namespace ring::upnp
