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

#if HAVE_UPNP
#include <miniupnpc/upnperrors.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#endif

#include "logger.h"
#include "ip_utils.h" /* for IpAddr */

namespace upnp {

UPnPIGD *UPnP::defaultIGD_ = new UPnPIGD();

UPnPIGD::~UPnPIGD()
{
#if HAVE_UPNP
    delete datas;
    FreeUPNPUrls((UPNPUrls *)urls);
    delete urls;
#endif
}

bool
UPnPIGD::isEmpty()
{
#if HAVE_UPNP
    if (datas == nullptr or urls == nullptr) {
        return true;
    } else {
        if (urls->controlURL[0] == '\0') {
            return true;
        } else {
            return false;
        }
    }
#else
    return true;
#endif
}

UPnP::UPnP(void)
{
    if (defaultIGD_ == nullptr or defaultIGD_->isEmpty())
        chooseDefaultIGD();
}


bool
UPnP::chooseDefaultIGD(void)
{
#if HAVE_UPNP
    struct UPNPDev * devlist = nullptr;
    struct UPNPDev * dev = nullptr;
    // char * descXML;
    // int descXMLsize = 0;
    int upnperror = 0;

    SFL_DBG("UPnP : finding default IGD");

    // devlist = upnpDiscover(2000, NULL/*multicast interface*/, NULL/*minissdpd socket path*/, 0/*sameport*/, 0/*ipv6*/, &upnperror);

    // if (devlist) {
    //     dev = devlist;
    //     while (dev) {
    //         if (strstr (dev->st, "InternetGatewayDevice"))
    //             break;
    //         dev = dev->pNext;
    //     }
    //     if (!dev)
    //         dev = devlist; /* defaulting to first device */

    //     SFL_DBG("UPnP device :\n"
    //            " desc: %s\n st: %s",
    //            dev->descURL, dev->st);

    //     descXML = (char *)miniwget(dev->descURL, &descXMLsize);
    //     if (descXML) {
    //         parserootdesc(descXML, descXMLsize, newIGDDatas);
    //         free(descXML);
    //         descXML = nullptr;
    //         GetUPNPUrls(newIGDURLs, newIGDDatas, dev->descURL);
    //     }
    //     freeUPNPDevlist(devlist);

    //     /* found a new device */
    //     delete defaultIGD_;
    //     defaultIGD_ = new UPnPIGD(newIGDDatas, newIGDURLs);

    //     return true;
    // } else {
    //     /* error ! */
    //     SFL_DBG("UPnP : could not find any UPnP devices");
    //     /* remove the old device and replace with empty one */
    //     delete defaultIGD_;
    //     defaultIGD_ = new UPnPIGD();
    //     return false;
    // }

    /* look for UPnP devices on the network */
    devlist = upnpDiscover(2000, NULL, NULL, 0, 0, &upnperror);

    if (devlist) {
        struct UPNPDev * device = nullptr;
        struct UPNPUrls * newIGDURLs;
        struct IGDdatas * newIGDDatas;
        char lanaddr[64];

        newIGDURLs = new UPNPUrls();
        newIGDDatas = new IGDdatas();

        SFL_DBG("UPnP devices found on the network");
        for(device = devlist; device; device = device->pNext)
        {
            SFL_DBG(" desc: %s\n st: %s",
                   device->descURL, device->st);
        }

        upnperror = UPNP_GetValidIGD(devlist, newIGDURLs, newIGDDatas, lanaddr, sizeof(lanaddr));

        switch(upnperror) {
            case -1:
                SFL_ERR("UPnP : internal error getting valid IGD");
                delete newIGDURLs;
                delete newIGDDatas;
                newIGDURLs = nullptr;
                newIGDDatas = nullptr;
                break;
            case 0:
                SFL_WARN("UPnP : no valid IGD found");
                FreeUPNPUrls(newIGDURLs);
                delete newIGDURLs;
                delete newIGDDatas;
                newIGDURLs = nullptr;
                newIGDDatas = nullptr;
                break;
            case 1:
                SFL_DBG("UPnP : found valid IGD : %s", newIGDURLs->controlURL);
                break;
            case 2:
                SFL_DBG("UPnP : found a (not connected?) IGD, will try to use it anyway: %s", newIGDURLs->controlURL);
                break;
            case 3:
                SFL_DBG("UPnP : UPnP device found, cannot determine if it is an IGD, will try to use it anyway : %s", newIGDURLs->controlURL);
                break;
            default:
                SFL_DBG("UPnP : device found, cannot determine if it is a UPnP device, will try to use it anyway : %s", newIGDURLs->controlURL);
        }

        if (upnperror > 0)
            SFL_DBG("UPnP : local IP address reported as: %s", lanaddr);

        delete defaultIGD_;
        defaultIGD_ = new UPnPIGD(newIGDDatas, newIGDURLs);

        freeUPNPDevlist(devlist);
        devlist = nullptr;
    }
    else
    {
        SFL_WARN("UPnP : looking for IGD UPnP devices on the network failed with error: %d : %s", upnperror, strupnperror(upnperror));
        return false;
    }

#else
    return false;
#endif
}

/**
 * returns if a default IGD is defined
 */
bool
UPnP::hasDefaultIGD(void) const
{
    return not ( defaultIGD_ == nullptr or defaultIGD_->isEmpty() );
}

/**
     * tries to add redirection
     */
bool
UPnP::addRedirection(unsigned int port_external, unsigned int port_internal, const std::string& type)
{
#if HAVE_UPNP
    if (not hasDefaultIGD()) {
        SFL_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return false;
    }

    char port_ext_str[16];
    char port_int_str[16];
    int r;
    IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
    if (!local_ip) {
        SFL_DBG("UPnP : cannot determine local IP");
        return false;
    }

    SFL_DBG("UPnP : adding port mapping : %s, %u -> %u", local_ip.toString().c_str(), port_external, port_internal);

    sprintf(port_ext_str, "%u", port_external);
    sprintf(port_int_str, "%u", port_internal);
    r = UPNP_AddPortMapping(defaultIGD_->urls->controlURL, defaultIGD_->datas->first.servicetype,
                            port_ext_str, port_int_str, local_ip.toString().c_str(), "ring", type.c_str(), NULL, NULL);
    if(r!=UPNPCOMMAND_SUCCESS) {
        SFL_DBG("UPnP : AddPortMapping(%s, %s, %s, %s) failed with error: %d: %s",
                            port_ext_str, port_int_str, local_ip.toString().c_str(), type.c_str(), r, strupnperror(r));
        return false;
    } else {
        /* TODO: add to list of redirections */
        return true;
    }
#else
    return false;
#endif
}

/**
 * tries to remove redirection
 */
bool
UPnP::removeRedirection(unsigned int port_external, const std::string& type)
{
#if HAVE_UPNP
    if (not hasDefaultIGD()) {
        SFL_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return false;
    }

    char port_str[16];
    int r;
    SFL_DBG("UPnP : removing entry with port = %u, type = %s", port_external, type.c_str());

    sprintf(port_str, "%u", port_external);
    r = UPNP_DeletePortMapping(defaultIGD_->urls->controlURL, defaultIGD_->datas->first.servicetype, port_str, type.c_str(), NULL);
    if(r!=UPNPCOMMAND_SUCCESS) {
        SFL_DBG("UPnP : DeletePortMapping(%s, %s) failed with error: %d: %s",
                            port_str, type.c_str(), r, strupnperror(r));
        return false;
    } else {
        /* TODO: remove from list of redirections */
        return true;
    }
#else
    return false;
#endif
}

/**
 * removes all entries which have given description
 */
void
UPnP::removeEntriesByDescription(const std::string& description)
{
#if HAVE_UPNP
    if (not hasDefaultIGD()) {
        SFL_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return;
    }

    int r;
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

    SFL_DBG("UPnP : removing all port mapping entries with description: \"%s\"", description.c_str());

    do {
        snprintf(index, 6, "%d", i);
        rHost[0] = '\0'; enabled[0] = '\0';
        duration[0] = '\0'; desc[0] = '\0';
        extPort[0] = '\0'; intPort[0] = '\0'; intClient[0] = '\0';
        r = UPNP_GetGenericPortMappingEntry(defaultIGD_->urls->controlURL,
                                       defaultIGD_->datas->first.servicetype,
                                       index,
                                       extPort, intClient, intPort,
                                       protocol, desc, enabled,
                                       rHost, duration);
        if(r==UPNPCOMMAND_SUCCESS) {
            /* remove if matches description
             * once the port mapping is deleted, there will be one less, and the rest will "move down"
             * that is, we don't need to increment the mapping index in that case
             */
            if( strcmp(description.c_str(), desc) == 0 ) {
                SFL_DBG("UPnP : found entry with matching description:\n\t%2d %s %5s->%s:%-5s '%s' '%s' %s",
                    i, protocol, extPort, intClient, intPort, desc, rHost, duration);
                int delete_err = 0;
                delete_err = UPNP_DeletePortMapping(defaultIGD_->urls->controlURL, defaultIGD_->datas->first.servicetype, extPort, protocol, NULL);
                if(delete_err != UPNPCOMMAND_SUCCESS) {
                    SFL_DBG("UPnP : UPNP_DeletePortMapping() failed with error code %d : %s", delete_err, strupnperror(delete_err));
                } else {
                    SFL_DBG("UPnP : deletion success");
                    /* decrement the mapping index since it will be incremented */
                    i--;
                    /* TODO: remove entry from list of entries */
                }
            }
        } else if (r==713) {
            /* 713 : SpecifiedArrayIndexInvalid
             * this means there are no more entries to check, and we're done
             */
        } else {
            SFL_DBG("UPnP : GetGenericPortMappingEntry() failed with error code %d : %s", r, strupnperror(r));
        }
        i++;
    } while(r==0);
#endif
}

/**
 * tries to get the external ip of the router
 */
IpAddr
UPnP::getExternalIP()
{
#if HAVE_UPNP
    if (not hasDefaultIGD()) {
        SFL_WARN("UPnP : cannot perform command as the IGD has either not been chosen or is not available.");
        return IpAddr();
    }

    int r;
    char externalIPAddress[40];

    SFL_DBG("UPnP : getting external IP");

    r = UPNP_GetExternalIPAddress(defaultIGD_->urls->controlURL,
                              defaultIGD_->datas->first.servicetype,
                              externalIPAddress);
    if(r != UPNPCOMMAND_SUCCESS) {
        SFL_DBG("UPnP : GetExternalIPAddress failed with error code %d : %s", r, strupnperror(r));
        return IpAddr();
    } else {
        SFL_DBG("UPnP : got external IP = %s", externalIPAddress);
        return IpAddr(std::string(externalIPAddress));
    }
#else
    /* return empty address */
    return IpAddr();
#endif
}

}