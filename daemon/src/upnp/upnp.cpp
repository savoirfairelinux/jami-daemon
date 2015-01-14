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

#if HAVE_UPNP
#include <miniupnpc/upnperrors.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#endif

#include "logger.h"
#include "ip_utils.h" /* for IpAddr */

namespace upnp {

UPnPIGD UPnP::defaultIGD_ = UPnPIGD();

#if HAVE_UPNP
UPnPIGD::UPnPIGD(const UPnPIGD &igdSource)
{
    /* make a copy of the structs */
    datas = IGDdatas(igdSource.datas);
    urls = UPNPUrls(igdSource.urls);
    /* have to make a copy of the urls */
    copyURLs(igdSource.urls);
}

UPnPIGD&
UPnPIGD::operator= (const UPnPIGD &igdSource)
{
    if (this == &igdSource)
        return *this;

    /* make a copy of the structs */
    datas = IGDdatas(igdSource.datas);
    urls = UPNPUrls(igdSource.urls);
    /* have to make a copy of the urls */
    copyURLs(igdSource.urls);
}

void
UPnPIGD::copyURLs(const UPNPUrls &urlsSource)
{
    if (urlsSource.controlURL != nullptr)
        urls.controlURL = strdup(urlsSource.controlURL);
    if (urlsSource.ipcondescURL != nullptr)
        urls.ipcondescURL = strdup(urlsSource.ipcondescURL);
    if (urlsSource.controlURL_CIF != nullptr)
        urls.controlURL_CIF = strdup(urlsSource.controlURL_CIF);
    if (urlsSource.controlURL_6FC != nullptr)
        urls.controlURL_6FC = strdup(urlsSource.controlURL_6FC);
    if (urlsSource.rootdescURL != nullptr)
        urls.rootdescURL = strdup(urlsSource.rootdescURL);
}

UPnPIGD::~UPnPIGD()
{
    FreeUPNPUrls(&urls);
}
#endif /* HAVE_UPNP */

/**
 * checks if the instance of UPnPIGD is empty
 * ie: not actually an IGD
 */
bool
UPnPIGD::isEmpty() const
{
#if HAVE_UPNP
    if (urls.controlURL != nullptr) {
        if (urls.controlURL[0] == '\0') {
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

UPnP::UPnP(bool enabled)
    : enabled_(enabled)
{
#if HAVE_UPNP
    if (enabled_ and defaultIGD_.isEmpty())
        chooseDefaultIGD();
#else
    enabled_ = false;
#endif
}

UPnP::~UPnP()
{
    /* remove all entries */
    if (enabled_)
        removeEntries();
}

/**
 * tries to find a valid IGD on the network
 * if one is found, UPnP::defaultIGD_ will be set to that
 */
bool
UPnP::chooseDefaultIGD(void)
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return false;
    }

    struct UPNPDev * devlist = nullptr;
    struct UPNPDev * dev = nullptr;
    int upnp_status = 0;

    RING_DBG("UPnP : finding default IGD");

    /* look for UPnP devices on the network */
    devlist = upnpDiscover(2000, NULL, NULL, 0, 0, &upnp_status);

    if (devlist) {
        struct UPNPDev * device = nullptr;
        struct UPNPUrls newIGDURLs = UPNPUrls();
        struct IGDdatas newIGDDatas = IGDdatas();
        char lanaddr[64];

        newIGDURLs = UPNPUrls();
        newIGDDatas = IGDdatas();

        RING_DBG("UPnP devices found on the network");
        for(device = devlist; device; device = device->pNext)
        {
            RING_DBG(" desc: %s\n st: %s",
                     device->descURL, device->st);
        }

        upnp_status = UPNP_GetValidIGD(devlist, &newIGDURLs, &newIGDDatas, lanaddr, sizeof(lanaddr));

        switch(upnp_status) {
            case -1:
                RING_ERR("UPnP : internal error getting valid IGD");
                newIGDURLs = UPNPUrls();
                newIGDDatas = IGDdatas();
                break;
            case 0:
                RING_WARN("UPnP : no valid IGD found");
                FreeUPNPUrls(&newIGDURLs);
                newIGDURLs = UPNPUrls();
                newIGDDatas = IGDdatas();
                break;
            case 1:
                RING_DBG("UPnP : found valid IGD : %s", newIGDURLs.controlURL);
                break;
            case 2:
                RING_DBG("UPnP : found a (not connected?) IGD, will try to use it anyway: %s", newIGDURLs.controlURL);
                break;
            case 3:
                RING_DBG("UPnP : UPnP device found, cannot determine if it is an IGD, will try to use it anyway : %s", newIGDURLs.controlURL);
                break;
            default:
                RING_DBG("UPnP : device found, cannot determine if it is a UPnP device, will try to use it anyway : %s", newIGDURLs.controlURL);
        }

        if (upnp_status > 0)
            RING_DBG("UPnP : local IP address reported as: %s", lanaddr);

        defaultIGD_ = UPnPIGD(newIGDDatas, newIGDURLs);

        freeUPNPDevlist(devlist);
        devlist = nullptr;
    }
    else
    {
        RING_WARN("UPnP : looking for IGD UPnP devices on the network failed with error: %d : %s", upnp_status, strupnperror(upnp_status));
        return false;
    }

#else
    return false;
#endif
}

/**
 * tries to add redirection
 */
bool
UPnP::addRedirection(uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error)
{
    /* initialiaze upnp_error */
    if (upnp_error)
        *upnp_error = -1; /* UPNPCOMMAND_UNKNOWN_ERROR */
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return false;
    }

    if (defaultIGD_.isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return false;
    }

    int upnp_status;

    IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
    if (!local_ip) {
        RING_DBG("UPnP : cannot determine local IP");
        return false;
    }

    UPnPRedirection redir = UPnPRedirection(defaultIGD_, local_ip, port_external, port_internal);

    RING_DBG("UPnP : adding port mapping : %s", redir.toString().c_str());

    upnp_status = UPNP_AddPortMapping(redir.igd.urls.controlURL,
                                      redir.igd.datas.first.servicetype,
                                      redir.getExternalPort().c_str(),
                                      redir.getInternalPort().c_str(),
                                      redir.local_ip.toString().c_str(),
                                      redir.description.c_str(),
                                      redir.getType().c_str(),
                                      NULL, NULL);

    if (upnp_error)
        *upnp_error = upnp_status;

    if(upnp_status!=UPNPCOMMAND_SUCCESS) {
        RING_DBG("UPnP : AddPortMapping(%s) failed with error: %d: %s",
                 redir.toString().c_str(), upnp_status, strupnperror(upnp_status));
        return false;
    } else {
        if (redir.type == PortType::UDP)
            udpInstanceMappings_[redir.port_external] = redir;
        else
            tcpInstanceMappings_[redir.port_external] = redir;

        return true;
    }
#else
    return false;
#endif
}

/**
 * tries to add redirection from and to the port_desired
 * if unique == true, makes sure the client is not using this port already
 * if the redirection fails, tries other available ports until success
 *
 * tries to use a random port between 1024 < > 65535 if desired port fails
 */
bool
UPnP::addAnyRedirection(uint16_t port_desired, PortType type, bool unique, uint16_t *port_used)
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return false;
    }

    int upnp_error;
    if (unique) {
        /* check that port is not already used by the client */

    }
    if (addRedirection(port_desired, port_desired, type, &upnp_error)) {
        // success
    } else {

    }
    return true;
#else
    return false;
#endif
}

/**
 * tries to remove redirection
 * if existing == true, only tries to remove if the entry was added by this instance
 * if existing == false, force parameter is ignored
 * if force == false, only deletes the entry from the router if this UPnP instance was the
 * only one using it; otherwise it reduces the number of users by 1
 * if force == true, deletes the entry from the router even if other UPnP instances are using it
 */
bool
UPnP::removeRedirection(uint16_t port_external, PortType type, bool existing)
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return false;
    }

    if (not existing and defaultIGD_.isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return false;
    }

    int upnp_status;
    UPnPRedirection *redir_ptr;

    if (existing) {
        /* make sure the entry has been added by this instance */
        auto *mappings = type == PortType::UDP ? &udpInstanceMappings_ : &tcpInstanceMappings_;
        auto iterator = mappings->find(port_external);
        if (iterator != mappings->end()) {

            redir_ptr = &iterator->second;
        } else {
            /* could not find mapping */
            RING_DBG("UPnP : could not find existing port mapping to remove.");
            return false;
        }
    } else {
        /* create new entry object */
        IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
        if (!local_ip) {
            RING_DBG("UPnP : cannot determine local IP");
            return false;
        }

        /* we only care about the external port, since we're removing the entry */
        UPnPRedirection redir = UPnPRedirection(defaultIGD_, local_ip, port_external, port_external);
        redir_ptr = &redir;
    }

    RING_DBG("UPnP : trying to remove entry: %s", redir_ptr->toString().c_str());

    upnp_status = UPNP_DeletePortMapping(redir_ptr->igd.urls.controlURL,
                                         redir_ptr->igd.datas.first.servicetype,
                                         redir_ptr->getExternalPort().c_str(),
                                         redir_ptr->getType().c_str(),
                                         NULL);
    if(upnp_status!=UPNPCOMMAND_SUCCESS and upnp_status != 714) {
        RING_DBG("UPnP : DeletePortMapping(%s) failed with error: %d: %s",
                 redir_ptr->toString().c_str(), upnp_status, strupnperror(upnp_status));
        return false;
    } else {
        /* either success, or 714 = no such entry
         * so remove from list of redirections
         * we try to remove whether the entry was indicated as existing or not */
        if (existing) {
            if (type == PortType::UDP)
                udpInstanceMappings_.erase(redir_ptr->port_external);
            else
                tcpInstanceMappings_.erase(redir_ptr->port_external);
        }
        return true;
    }
#else
    return false;
#endif
}

void
UPnP::removeEntries(PortType type) {
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return;
    }

    auto *mappings = type == PortType::UDP ? &udpInstanceMappings_ : &tcpInstanceMappings_;
    for (auto iterator = mappings->begin(); iterator != mappings->end(); ){
        auto redir = iterator->second;
        RING_DBG("UPnP : trying to remove entry: %s", redir.toString().c_str());
        int upnp_status = UPNP_DeletePortMapping(redir.igd.urls.controlURL,
                                                 redir.igd.datas.first.servicetype,
                                                 redir.getExternalPort().c_str(),
                                                 redir.getType().c_str(),
                                                 NULL);
        if(upnp_status!=UPNPCOMMAND_SUCCESS and upnp_status != 714) {
            RING_DBG("UPnP : DeletePortMapping(%s) failed with error: %d: %s",
                     redir.toString().c_str(), upnp_status, strupnperror(upnp_status));
            /* removal failed, so increment itterator to try to remove the next entry */
            iterator++;
        } else {
            /* either success, or 714 = no such entry
             * so remove from list of redirections */
            iterator = mappings->erase(iterator);
        }
    }
#endif
}

/**
 * removes all entries added by this instance
 */
void
UPnP::removeEntries()
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return;
    }
    removeEntries(PortType::UDP);
    removeEntries(PortType::TCP);
#endif
}

/**
 * removes all entries which have given description
 * NOTE: you should probably only use removeEntriesByLocalIPAndDescription
 *       so as not to remove entries added by other clients on the same router
 */
void
UPnP::removeEntriesByDescription(const std::string& description)
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return;
    }

    if (defaultIGD_.isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
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

    RING_DBG("UPnP : removing all port mapping entries with description: \"%s\"", description.c_str());

    do {
        snprintf(index, 6, "%d", i);
        rHost[0] = '\0';
        enabled[0] = '\0';
        duration[0] = '\0';
        desc[0] = '\0';
        extPort[0] = '\0';
        intPort[0] = '\0';
        intClient[0] = '\0';
        upnp_status = UPNP_GetGenericPortMappingEntry(defaultIGD_.urls.controlURL,
                                                      defaultIGD_.datas.first.servicetype,
                                                      index,
                                                      extPort, intClient, intPort,
                                                      protocol, desc, enabled,
                                                      rHost, duration);
        if(upnp_status == UPNPCOMMAND_SUCCESS) {
            /* remove if matches description
             * once the port mapping is deleted, there will be one less, and the rest will "move down"
             * that is, we don't need to increment the mapping index in that case
             */
            if( strcmp(description.c_str(), desc) == 0 ) {
                RING_DBG("UPnP : found entry with matching description:\n\t%s %5s->%s:%-5s '%s'",
                         protocol, extPort, intClient, intPort, desc);
                int delete_err = 0;
                delete_err = UPNP_DeletePortMapping(defaultIGD_.urls.controlURL, defaultIGD_.datas.first.servicetype, extPort, protocol, NULL);
                if(delete_err != UPNPCOMMAND_SUCCESS) {
                    RING_DBG("UPnP : UPNP_DeletePortMapping() failed with error code %d : %s", delete_err, strupnperror(delete_err));
                } else {
                    RING_DBG("UPnP : deletion success");
                    /* decrement the mapping index since it will be incremented */
                    i--;
                    /* TODO: remove entry from list of entries */
                }
            }
        } else if (upnp_status == 713) {
            /* 713 : SpecifiedArrayIndexInvalid
             * this means there are no more entries to check, and we're done
             */
        } else {
            RING_DBG("UPnP : GetGenericPortMappingEntry() failed with error code %d : %s", upnp_status, strupnperror(upnp_status));
        }
        i++;
    } while(upnp_status == UPNPCOMMAND_SUCCESS);
#endif
}

/**
 * removes all entries with the local IP and the given description
 */
void
UPnP::removeEntriesByLocalIPAndDescription(const std::string& description)
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return;
    }

    if (defaultIGD_.isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return;
    }

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

    RING_DBG("UPnP : removing all port mapping entries with description: \"%s\" and local ip: %s",
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
        upnp_status = UPNP_GetGenericPortMappingEntry(defaultIGD_.urls.controlURL,
                                                      defaultIGD_.datas.first.servicetype,
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
                RING_DBG("UPnP : found entry with matching description and ip:\n\t%s %5s->%s:%-5s '%s'",
                         protocol, extPort, intClient, intPort, desc);
                int delete_err = 0;
                delete_err = UPNP_DeletePortMapping(defaultIGD_.urls.controlURL, defaultIGD_.datas.first.servicetype, extPort, protocol, NULL);
                if(delete_err != UPNPCOMMAND_SUCCESS) {
                    RING_DBG("UPnP : UPNP_DeletePortMapping() failed with error code %d : %s", delete_err, strupnperror(delete_err));
                } else {
                    RING_DBG("UPnP : deletion success");
                    /* decrement the mapping index since it will be incremented */
                    i--;
                    /* TODO: remove entry from list of entries */
                }
            }
        } else if (upnp_status == 713) {
            /* 713 : SpecifiedArrayIndexInvalid
             * this means there are no more entries to check, and we're done
             */
        } else {
            RING_DBG("UPnP : GetGenericPortMappingEntry() failed with error code %d : %s", upnp_status, strupnperror(upnp_status));
        }
        i++;
    } while(upnp_status == UPNPCOMMAND_SUCCESS);
#endif
}

/**
 * tries to get the external ip of the router
 */
IpAddr
UPnP::getExternalIP()
{
#if HAVE_UPNP
    if (not enabled_) {
        RING_DBG("UPnP : UPnP is not enabled");
        return false;
    }

    if (defaultIGD_.isEmpty()) {
        RING_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return IpAddr();
    }

    int upnp_status;
    char externalIPAddress[40];

    RING_DBG("UPnP : getting external IP");

    upnp_status = UPNP_GetExternalIPAddress(defaultIGD_.urls.controlURL,
                                            defaultIGD_.datas.first.servicetype,
                                            externalIPAddress);
    if(upnp_status != UPNPCOMMAND_SUCCESS) {
        RING_DBG("UPnP : GetExternalIPAddress failed with error code %d : %s", upnp_status, strupnperror(upnp_status));
        return IpAddr();
    } else {
        RING_DBG("UPnP : got external IP = %s", externalIPAddress);
        return IpAddr(std::string(externalIPAddress));
    }
#else
    /* return empty address */
    return IpAddr();
#endif
}

}