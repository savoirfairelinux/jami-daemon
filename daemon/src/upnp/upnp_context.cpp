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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp_context.h"

#include <string>
#include <set>
#include <mutex>
#include <memory>

#if HAVE_LIBUPNP
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#endif

#include "logger.h"
#include "ip_utils.h"

namespace ring { namespace upnp {

/* UPnP IGD definitions */
constexpr static char const * UPNP_ROOT_DEVICE = "upnp:rootdevice";
constexpr static char const * UPNP_IGD_DEVICE = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static char const * UPNP_WAN_DEVICE = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static char const * UPNP_WANCON_DEVICE = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static char const * UPNP_WANIP_SERVICE = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static char const * UPNP_WANPPP_SERVICE = "urn:schemas-upnp-org:service:WANPPPConnection:1";

/**
 * This should be used to get a UPnPContext.
 * It only makes sense to have one unless you have separate
 * contexts for multiple internet interfaces, which is not currently
 * supported.
 */
std::shared_ptr<UPnPContext>
getUPnPContext()
{
    static std::shared_ptr<UPnPContext> context;

    if (not context)
        context = std::make_shared<UPnPContext>();

    return context;
}

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
    if (this != otehr) {
        datas_ = other.datas_;
        urls_ = other.urls_;
        resetURLs(other.urls_);
    }
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

#if HAVE_LIBUPNP

/*
 * Local prototypes
 */
int cp_callback(Upnp_EventType, void*, void*);
static std::string get_first_doc_item(IXML_Document *, const char *);

UPnPContext::UPnPContext()
{
    int upnp_err;
    char* ip_address = nullptr;
    unsigned short port = 0;

    /* TODO: allow user to specify interface to be used
     *       by selecting the IP
     */

#ifdef UPNP_ENABLE_IPV6
    RING_DBG("UPnP : using IPv6");
    upnp_err = UpnpInit2(0, 0);
#else
    RING_DBG("UPnP : using IPv4");
    upnp_err = UpnpInit(0, 0);
#endif
    if ( upnp_err != UPNP_E_SUCCESS ) {
        RING_ERR("UPnP : error in UpnpInit(): %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
    }
    initialized_ = true;

    ip_address = UpnpGetServerIpAddress(); /* do not free, it is freed by UpnpFinish() */
    port = UpnpGetServerPort();

    RING_DBG("UPnP: initialiazed on %s:%u", ip_address, port);

    /* relax the parser to allow malformed XML text */
    ixmlRelaxParser( 1 );

    /* Register a control point to start looking for devices right away */
    upnp_err = UpnpRegisterClient( cp_callback, &ctrlpt_handle_, nullptr );
    if ( upnp_err != UPNP_E_SUCCESS ) {
        RING_ERR("UPnP : error registering control point: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
        initialized_ = false;
    }
    client_registered_ = true;

    /* send out async searches;
     * even if no account is using UPnP currently we might as well start
     * gathering a list of available devices;
     * we will probably receive their advertisements either way
     */
    searchForIGD();
}

UPnPContext::~UPnPContext()
{
    if (initialized_){
        /* make sure everything is unregistered, freed, and UpnpFinish() is called */
        if (client_registered_)
            UpnpUnRegisterClient( ctrlpt_handle_ );

        if (device_registered_)
            UpnpUnRegisterRootDevice( device_handle_ );

        UpnpFinish();
    }
}

void
UPnPContext::searchForIGD()
{
    if (client_registered_) {
        /* send out search for both types, as some routers may possibly only reply to one */
        UpnpSearchAsync(ctrlpt_handle_, SEARCH_TIMEOUT_SEC, UPNP_ROOT_DEVICE, nullptr);
        UpnpSearchAsync(ctrlpt_handle_, SEARCH_TIMEOUT_SEC, UPNP_IGD_DEVICE, nullptr);
    } else
        RING_WARN("UPnP: Control Point not registered.");
}

/**
 * Parses the device description and adds desired devices to
 * relevant lists
 */
void
UPnPContext::parseDevice(IXML_Document *doc)
{
    /* check the UDN to see if its already in our device list(s)
     * if it is, then update the device advertisement timeout (expiration)
     */


    /* check if its a valid IGD:
     *      1. check for IGD device
     *      2. check for WAN device
     *      3. check for WANIPConnection service or WANPPPConnection service
     *      4. check if connected to Internet (if not, no point in port forwarding)
     *      5. check that we can get the external IP
     */

    /* if its a valid IGD, add to list of IGDs (ideally there is only one at a time)
     * subscribe to the WANIPConnection or WANPPPConnection service to receive
     * updates about state changes, eg: new external IP
     */
}

static std::string
get_first_doc_item(IXML_Document *doc, const char *item)
{
    std::string ret;

    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
    if (nodeList) {
        /* if there are several nodes which match the tag, we only want the first one */
        IXML_Node *tmpNode = ixmlNodeList_item(nodeList.get(), 0);
        if (tmpNode) {
            IXML_Node *textNode = ixmlNode_getFirstChild(tmpNode);
            if (textNode) {
                const char* value = ixmlNode_getNodeValue(textNode);
                if (value)
                    ret = std::string(value);
            }
        }
    }
    return ret;
}

int
cp_callback(Upnp_EventType event_type, void* event, void* user_data)
{
    auto upnpContext = getUPnPContext();

    switch( event_type )
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        /* RING_DBG("UPnP: CP received a discovery advertisement"); */
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        struct Upnp_Discovery* d_event = ( struct Upnp_Discovery* )event;
        IXML_Document *desc_doc = nullptr;
        int upnp_err;

        /* if (event_type != UPNP_DISCOVERY_ADVERTISEMENT_ALIVE)
             RING_DBG("UPnP: CP received a discovery search result"); */

        /* check if we are already in the process of checking this device */
        std::unique_lock<std::mutex> lock(upnpContext->cp_mutex_);
        std::set<std::string>::iterator it;
        it = upnpContext->cp_checking_devices_.find(std::string(d_event->Location));

        if (it == upnpContext->cp_checking_devices_.end()) {
            upnpContext->cp_checking_devices_.emplace(std::string(d_event->Location));
            lock.unlock();

            if (d_event->ErrCode != UPNP_E_SUCCESS)
                RING_WARN("UPnP: Error in discovery event received by the CP: %s", UpnpGetErrorMessage(d_event->ErrCode));

            RING_DBG("UPnP: Control Point received discovery event from device:\n\tid: %s\n\ttype: %s\n\tservice: %s\n\tversion: %s\n\tlocation: %s\n\tOS: %s",
                     d_event->DeviceId, d_event->DeviceType, d_event->ServiceType, d_event->ServiceVer, d_event->Location, d_event->Os);

            /* note: this thing will block until success for the system socket timeout
             *       unless libupnp is compile with '-disable-blocking-tcp-connections'
             *       in which case it will block for the libupnp specified timeout
             */
            upnp_err = UpnpDownloadXmlDoc( d_event->Location, &desc_doc );
            if ( upnp_err != UPNP_E_SUCCESS ) {
                RING_WARN("UPnP: Error downloading device description: %s", UpnpGetErrorMessage(upnp_err));
            } else {
                /* TODO: parse device description and add desired devices to relevant lists, etc */
            }

            /* finished parsing device; remove it from know devices list,
             * since next time it could be a new device with same URL
             * eg: if we switch routers or if a new device with the same IP appears
             */
            lock.lock();
            upnpContext->cp_checking_devices_.erase(d_event->Location);
            lock.unlock();

            ixmlDocument_free(desc_doc);
        } else {
            lock.unlock();
            /* RING_DBG("UPnP: Control Point is already checking this device"); */
        }
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)event;

        RING_DBG("UPnP: Control Point received ByeBye for device: %s", d_event->DeviceId);

        if (d_event->ErrCode != UPNP_E_SUCCESS)
            RING_WARN("UPnP: Error in ByeBye received by the CP: %s", UpnpGetErrorMessage(d_event->ErrCode));

        /* TODO: check if its a device we care about and remove it from the relevant lists */
    }
    break;

    case UPNP_EVENT_RECEIVED:
    {
        struct Upnp_Event *e_event = (struct Upnp_Event *)event;

        RING_DBG("UPnP: Control Point event received");

        /* TODO: handle event by updating any changed state variables */

        // char* vars = ixmlPrintDocument(e_event->ChangedVariables);
        // std::cout << "Changed variables: " << vars << std::endl;
        // ixmlFreeDOMString(vars);
    }
    break;

    case UPNP_EVENT_AUTORENEWAL_FAILED:
    {
        RING_WARN("UPnP: Control Point subscription auto-renewal failed");
    }
    break;

    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
    {
        RING_DBG("UPnP: Control Point subscription expired");
    }
    break;

    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        RING_DBG("UPnP: Control Point async subscription complete");

        /* TODO: check if successfull */

        break;

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        RING_DBG("UPnP: Control Point search timeout");

        /* TODO: check if we found what we were looking for */

        break;

    case UPNP_CONTROL_ACTION_COMPLETE:
    {
        struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)event;

        RING_DBG("UPnP: Control Point async action complete");

        if (a_event->ErrCode != UPNP_E_SUCCESS)
            RING_WARN("UPnP: Error in action complete event: %s", UpnpGetErrorMessage(a_event->ErrCode));

        /* No need for any processing here, just print out results.
         * Service state table updates are handled by events. */
    }
    break;

    case UPNP_CONTROL_GET_VAR_COMPLETE:
    {
        struct Upnp_State_Var_Complete *sv_event = (struct Upnp_State_Var_Complete *)event;

        RING_DBG("UPnP: Control Point async get variable complete");

        if (sv_event->ErrCode != UPNP_E_SUCCESS)
            RING_WARN("UPnP: Error in get variable complete event: %s", UpnpGetErrorMessage(sv_event->ErrCode));

        /* update state variables ? */
    }
    break;

    default:
        RING_WARN("UPnP: unhandled Control Point event");
        break;
    }

    return UPNP_E_SUCCESS; /* return value currently ignored by SDK */
}

#endif

}} // namespace ring::upnp
