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
#include <condition_variable>
#include <random>

#if HAVE_LIBUPNP
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#endif

#include "logger.h"
#include "ip_utils.h"
#include "upnp_igd.h"

namespace ring { namespace upnp {

/**
 * This should be used to get a UPnPContext.
 * It only makes sense to have one unless you have separate
 * contexts for multiple internet interfaces, which is not currently
 * supported.
 */
std::shared_ptr<UPnPContext>
getUPnPContext()
{
    static auto context = std::make_shared<UPnPContext>();
    return context;
}

#ifdef HAVE_LIBUPNP

/* UPnP IGD definitions */
constexpr static char const * UPNP_ROOT_DEVICE = "upnp:rootdevice";
constexpr static char const * UPNP_IGD_DEVICE = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static char const * UPNP_WAN_DEVICE = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static char const * UPNP_WANCON_DEVICE = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static char const * UPNP_WANIP_SERVICE = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static char const * UPNP_WANPPP_SERVICE = "urn:schemas-upnp-org:service:WANPPPConnection:1";

/*
 * Local prototypes
 */
int cp_callback(Upnp_EventType, void*, void*);
static std::string get_element_text(IXML_Node*);
static std::string get_first_doc_item(IXML_Document*, const char*);
static std::string get_first_element_item(IXML_Element*, const char*);
static void checkResponseError(IXML_Document*);

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
    upnp_err = UpnpRegisterClient( cp_callback, nullptr, &ctrlpt_handle_ );
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

        std::unique_lock<std::mutex> lock(igd_mutex_);
        for( auto const &it : valid_igds_) {
            removeMappingsByLocalIPAndDescription(it.second.get(), Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
        }
        lock.unlock();

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
        UpnpSearchAsync(ctrlpt_handle_, SEARCH_TIMEOUT_SEC, UPNP_WANIP_SERVICE, nullptr);
        UpnpSearchAsync(ctrlpt_handle_, SEARCH_TIMEOUT_SEC, UPNP_WANPPP_SERVICE, nullptr);

        {
            std::lock_guard<std::mutex> lock(igd_search_mutex_);
            pending_igd_search_requests_ = 4;
        }
    } else
        RING_WARN("UPnP: Control Point not registered.");
}

/**
 * returns 'true' if there is at least one valid (connected) IGD
 * note: this function will until an IGD has been found or SEARCH_TIMEOUT_SEC
 *       have expired; the timeout starts when the context is created as we start
 *       searchign for IGDs immediately
 */
bool
UPnPContext::hasValidIGD()
{
    if (not client_registered_) {
        RING_WARN("UPnP: Control Point not registered.");
        return false;
    }

    std::unique_lock<std::mutex> lock(igd_search_mutex_);
    igd_search_condition_.wait(lock, [this] { return pending_igd_search_requests_ <= 0;});
    lock.unlock();

    std::lock_guard<std::mutex> lock2(igd_mutex_);
    return not valid_igds_.empty();
}

/**
 * chooses the IGD to use,
 * assumes you already have a lock on igd_mutex_
 */
IGD*
UPnPContext::chooseIGD_unlocked()
{
    auto it = valid_igds_.begin();
    if (it == valid_igds_.end())
        return nullptr;
    else
        return it->second.get();
}

/**
 * tries to add mapping
 */
Mapping
UPnPContext::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error)
{
    *upnp_error = -1;

    IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
    if (!local_ip) {
        RING_DBG("UPnP : cannot determine local IP");
        return {};
    }

    Mapping mapping{local_ip, port_external, port_internal};

    /* check if this mapping already exists
     * if the mapping is the same, then we just need to increment the number of users globally
     * if the mapping is not the same, then we have to return fail, as the external port is used
     * for something else
     * if the mapping doesn't exist, then try to add it
     */
    auto globalMappings = type == PortType::UDP ? &igd->udpMappings : &igd->tcpMappings;
    auto iter = globalMappings->find(port_external);
    if (iter != globalMappings->end()) {
        /* mapping exists with same external port */
        GlobalMapping *mapping_ptr = &iter->second;
        if (*mapping_ptr == mapping) {
            /* the same mapping, so nothing needs to be done */
            RING_DBG("UPnP : mapping was already added.");
            *upnp_error = UPNP_E_SUCCESS;
            /* increment users */
            mapping_ptr->users++;
            RING_DBG("UPnp : number of users of mapping should now be incremented: %d", iter->second.users);
            return mapping;
        } else {
            /* this port is already used by a different mapping */
            RING_WARN("UPnP : cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            *upnp_error = 718; /* ConflictInMappingEntry */
            return {};
        }
    }

    /* mapping doesn't exist, so try to add it */
    RING_DBG("UPnP : adding port mapping : %s", mapping.toString().c_str());

    if(addPortMapping(igd, mapping, upnp_error)) {
        /* success; add it to global list */
        globalMappings->emplace(port_external, std::move(GlobalMapping{mapping}));
        return mapping;
    } else
        return {};
}

/**
 * chooses a random port that is not yet used by the daemon for UPnP
 */
uint16_t
UPnPContext::chooseRandomPort(const IGD* igd, PortType type)
{
    auto globalMappings = type == PortType::UDP ? &igd->udpMappings : &igd->tcpMappings;

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
 * tries to add mapping from and to the port_desired
 * if unique == true, makes sure the client is not using this port already
 * if the mapping fails, tries other available ports until success
 *
 * tries to use a random port between 1024 < > 65535 if desired port fails
 *
 * maps port_desired to port_local; if use_same_port == true, makes sure that
 * that the external and internal ports are the same
 *
 * returns a valid mapping on success and an invalid mapping on failure
 */
Mapping
UPnPContext::addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool use_same_port, bool unique)
{
    if (not hasValidIGD()) {
        RING_WARN("UPnP: no valid IGD availabe");
        return {};
    }

    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(igd_mutex_);
    IGD* igd = chooseIGD_unlocked();
    if (not igd) {
        RING_WARN("UPnP: no valid IGD availabe");
        return {};
    }

    auto globalMappings = type == PortType::UDP ? &igd->udpMappings : &igd->tcpMappings;
    if (unique) {
        /* check that port is not already used by the client */
        auto iter = globalMappings->find(port_desired);
        if (iter != globalMappings->end()) {
            /* port already used, we need a unique port */
            port_desired = chooseRandomPort(igd, type);
        }
    }

    if (use_same_port)
        port_local = port_desired;

    int upnp_error;
    Mapping mapping = addMapping(igd, port_desired, port_local, type, &upnp_error);
    /* keep trying to add the mapping as long as the upnp error is 718 == conflicting mapping
     * if adding the mapping fails for any other reason, give up
     */
    while( not mapping and (upnp_error == 718 or upnp_error == 402) ) {
        /* acceptable errors to keep trying:
         * 718 : conflictin mapping
         * 402 : invalid args (due to router implementation)
         */
        /* TODO: make sure we don't try sellecting the same random port twice if it fails ? */
        port_desired = chooseRandomPort(igd, type);
        if (use_same_port)
            port_local = port_desired;
        mapping = addMapping(igd, port_desired, port_local, type, &upnp_error);
    }

    return mapping;
}

/**
 * tries to remove the given mapping
 */
void
UPnPContext::removeMapping(const Mapping& mapping)
{
    if (not hasValidIGD()) {
        RING_WARN("UPnP: no valid IGD availabe");
        return;
    }

    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(igd_mutex_);
    IGD* igd = chooseIGD_unlocked();
    if (not igd) {
        RING_WARN("UPnP: no valid IGD availabe");
        return;;
    }

    /* first make sure the mapping exists in the global list of the igd */
    auto globalMappings = mapping.getType() == PortType::UDP ? &igd->udpMappings : &igd->tcpMappings;
    auto iter = globalMappings->find(mapping.getPortExternal());
    if ( iter != globalMappings->end() ) {
        /* make sure its the same mapping */
        GlobalMapping *global_mapping = &iter->second;
        if (mapping == *global_mapping ) {
            /* now check the users */
            if (global_mapping->users > 1) {
                /* more than one user, simply decrement the number */
                global_mapping->users--;
                RING_DBG("UPnP : not deleting mapping: %s, from IGD as there are still %d other users",
                     mapping.toString().c_str(), global_mapping->users);
            } else {
                /* no other users, can delete */
                deletePortMapping(igd, mapping.getPortExternalStr(), mapping.getTypeStr());
                globalMappings->erase(iter);
            }
        } else {
            RING_WARN("UPnP: cannot remove mapping which doesn't match the existing one in the IGD list");
        }
    } else {
        RING_WARN("UPnP: cannot remove mapping which is not in the list of existing mappings of the IGD");
    }
}

IpAddr
UPnPContext::getExternalIP()
{
    if (not hasValidIGD()) {
        RING_WARN("UPnP: no valid IGD availabe");
        return {};
    }

    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(igd_mutex_);
    IGD* igd = chooseIGD_unlocked();
    if (not igd) {
        RING_WARN("UPnP: no valid IGD availabe");
        return {};
    }

    return getExternalIP(igd);
}

/**
 * Parses the device description and adds desired devices to
 * relevant lists
 */
void
UPnPContext::parseDevice(IXML_Document* doc, const Upnp_Discovery* d_event)
{
    if (not doc or not d_event)
        return;

    /* check to see the device type */
    std::string deviceType = get_first_doc_item(doc, "deviceType");
    if (deviceType.empty()) {
        /* RING_DBG("UPnP: could not find deviceType in the description document of the device"); */
        return;
    }

    if (deviceType.compare(UPNP_IGD_DEVICE) == 0) {
        parseIGD(doc, d_event);
    }

    /* TODO: check if its a ring device */
}

void
UPnPContext::parseIGD(IXML_Document* doc, const Upnp_Discovery* d_event)
{
    if (not doc or not d_event)
        return;

    /* check the UDN to see if its already in our device list(s)
     * if it is, then update the device advertisement timeout (expiration)
     */
    std::string UDN = get_first_doc_item(doc, "UDN");
    if (UDN.empty()) {
        RING_DBG("UPnP: could not find UDN in description document of device");
        return;
    }

    std::unique_lock<std::mutex> lock(igd_mutex_);
    auto it = valid_igds_.find(UDN);

    if (it != valid_igds_.end()) {
        /* we already have this device in our list */
        /* TODO: update expiration */
        return;
    }
    lock.unlock();

    std::unique_ptr<IGD> new_igd;
    int upnp_err;

    std::string friendlyName = get_first_doc_item(doc, "friendlyName");
    if (not friendlyName.empty() )
        RING_DBG("UPnP: checking new device of type IGD: '%s'", friendlyName.c_str());

    /* determine baseURL */
    std::string baseURL = get_first_doc_item(doc, "URLBase");
    if (baseURL.empty()) {
        /* get it from the discovery event location */
        baseURL = std::string(d_event->Location);
    }

    /* check if its a valid IGD:
     *      1. check for IGD device... already done if this function is called
     *      2. check for WAN device... skip checking for this and check for the services directly
     *      3. check for WANIPConnection service or WANPPPConnection service
     *      4. check if connected to Internet (if not, no point in port forwarding)
     *      5. check that we can get the external IP
     */

    /* get list of services */
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> serviceList(nullptr, ixmlNodeList_free);
    serviceList.reset(ixmlDocument_getElementsByTagName(doc, "serviceType"));
    /* check for WANIPConnection or WANPPPConnection service */
    bool found_connected_IGD = false;
    unsigned long list_length = ixmlNodeList_length(serviceList.get());
    for (unsigned long node_idx = 0; node_idx < list_length and not found_connected_IGD; node_idx++) {
        IXML_Node* serviceType_node = ixmlNodeList_item(serviceList.get(), node_idx);
        std::string serviceType = get_element_text(serviceType_node);
        if (serviceType.compare(UPNP_WANIP_SERVICE) == 0 or serviceType.compare(UPNP_WANPPP_SERVICE) == 0) {
            IXML_Node* service_node = ixmlNode_getParentNode(serviceType_node);
            if (service_node) {
                /* perform sanity check; this node should be called "service" */
                if( strcmp(ixmlNode_getNodeName(service_node), "service") == 0) {
                    /* get the service definitions */
                    IXML_Element* service_element = (IXML_Element*)service_node;
                    std::string serviceId = get_first_element_item(service_element, "serviceId");

                    std::string controlURL = get_first_element_item(service_element, "controlURL");
                    if (not controlURL.empty()) {
                        std::unique_ptr<char> absolute_url(new char[baseURL.size() + controlURL.size() + 1]);
                        upnp_err = UpnpResolveURL(baseURL.c_str(), controlURL.c_str(), absolute_url.get());
                        if (upnp_err == UPNP_E_SUCCESS) {
                            controlURL = std::string(absolute_url.get());
                        } else {
                            RING_WARN("UPnP: error resolving absolute controlURL: %s", UpnpGetErrorMessage(upnp_err));
                        }
                    }
                    std::string eventSubURL = get_first_element_item(service_element, "eventSubURL");
                    if (not eventSubURL.empty()) {
                        std::unique_ptr<char> absolute_url(new char[baseURL.size() + controlURL.size() + 1]);
                        upnp_err = UpnpResolveURL(baseURL.c_str(), eventSubURL.c_str(), absolute_url.get());
                        if (upnp_err == UPNP_E_SUCCESS) {
                            eventSubURL = std::string(absolute_url.get());
                        } else {
                            RING_WARN("UPnP: error resolving absolute eventSubURL: %s", UpnpGetErrorMessage(upnp_err));
                        }
                    }

                    if (not (serviceId.empty() and controlURL.empty() and eventSubURL.empty()) ) {
                        RING_DBG("UPnP: got service info from device:\n\tserviceType: %s\n\tserviceID: %s\n\tcontrolURL: %s\n\teventSubURL: %s",
                                 serviceType.c_str(), serviceId.c_str(), controlURL.c_str(), eventSubURL.c_str());
                        new_igd.reset(new IGD(UDN, baseURL, friendlyName, serviceType, serviceId, controlURL, eventSubURL));
                        if (isIGDConnected(new_igd.get())) {
                            new_igd->publicIp = getExternalIP(new_igd.get());
                            if (new_igd->publicIp) {
                                RING_DBG("UPnP: got external IP: %s", new_igd->publicIp.toString().c_str());
                                found_connected_IGD = true;
                            }
                        }
                    }
                    /* TODO: subscribe to the service to get events */
                } else
                     RING_WARN("UPnP: IGD \"serviceType\" parent node is not called \"service\"!");
            } else
                RING_WARN("UPnP: IGD \"serviceType\" has no parent node!");
        }
    }

    /* if its a valid IGD, add to list of IGDs (ideally there is only one at a time)
     * subscribe to the WANIPConnection or WANPPPConnection service to receive
     * updates about state changes, eg: new external IP
     */
    if (found_connected_IGD) {
        lock.lock();
        /* delete all  RING entries */
        removeMappingsByLocalIPAndDescription(new_igd.get(), Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);

        valid_igds_.emplace(UDN, std::move(new_igd));
        lock.unlock();

        /* check if we have pending search requests and notify anyone waiting */
        {
            std::lock_guard<std::mutex> search_lock(igd_search_mutex_);
            if (pending_igd_search_requests_ > 0) {
                pending_igd_search_requests_ = 0;
            }
        }
        igd_search_condition_.notify_all();
    }
}

static std::string
get_element_text(IXML_Node* node)
{
    std::string ret;
    if (node) {
        IXML_Node *textNode = ixmlNode_getFirstChild(node);
        if (textNode) {
            const char* value = ixmlNode_getNodeValue(textNode);
            if (value)
                ret = std::string(value);
        }
    }
    return ret;
}

static std::string
get_first_doc_item(IXML_Document* doc, const char* item)
{
    std::string ret;

    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
    if (nodeList) {
        /* if there are several nodes which match the tag, we only want the first one */
        ret = get_element_text( ixmlNodeList_item(nodeList.get(), 0) );
    }
    return ret;
}

static std::string
get_first_element_item(IXML_Element* element, const char* item)
{
    std::string ret;

    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlElement_getElementsByTagName(element, item), ixmlNodeList_free);
    if (nodeList) {
        /* if there are several nodes which match the tag, we only want the first one */
        ret = get_element_text( ixmlNodeList_item(nodeList.get(), 0) );
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
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> desc_doc(nullptr, ixmlDocument_free);
        int upnp_err;

        /* if (event_type != UPNP_DISCOVERY_ADVERTISEMENT_ALIVE)
             RING_DBG("UPnP: CP received a discovery search result"); */

        /* check if we are already in the process of checking this device */
        std::unique_lock<std::mutex> lock(upnpContext->cp_mutex_);
        auto it = upnpContext->cp_checking_devices_.find(std::string(d_event->Location));

        if (it == upnpContext->cp_checking_devices_.end()) {
            upnpContext->cp_checking_devices_.emplace(std::string(d_event->Location));
            lock.unlock();

            if (d_event->ErrCode != UPNP_E_SUCCESS)
                RING_WARN("UPnP: Error in discovery event received by the CP: %s", UpnpGetErrorMessage(d_event->ErrCode));

            /* RING_DBG("UPnP: Control Point received discovery event from device:\n\tid: %s\n\ttype: %s\n\tservice: %s\n\tversion: %s\n\tlocation: %s\n\tOS: %s",
                     d_event->DeviceId, d_event->DeviceType, d_event->ServiceType, d_event->ServiceVer, d_event->Location, d_event->Os);
            */

            /* note: this thing will block until success for the system socket timeout
             *       unless libupnp is compile with '-disable-blocking-tcp-connections'
             *       in which case it will block for the libupnp specified timeout
             */
            IXML_Document* desc_doc_ptr = nullptr;
            upnp_err = UpnpDownloadXmlDoc( d_event->Location, &desc_doc_ptr);
            desc_doc.reset(desc_doc_ptr);
            if ( upnp_err != UPNP_E_SUCCESS ) {
                RING_WARN("UPnP: Error downloading device description: %s", UpnpGetErrorMessage(upnp_err));
            } else {
                /* TODO: parse device description and add desired devices to relevant lists, etc */
                upnpContext->parseDevice(desc_doc.get(), d_event);
            }

            /* finished parsing device; remove it from know devices list,
             * since next time it could be a new device with same URL
             * eg: if we switch routers or if a new device with the same IP appears
             */
            lock.lock();
            upnpContext->cp_checking_devices_.erase(d_event->Location);
            lock.unlock();
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
    {
        RING_DBG("UPnP: Control Point search timeout");
        {
            std::lock_guard<std::mutex> lock(upnpContext->igd_search_mutex_);
            if (upnpContext->pending_igd_search_requests_ > 0)
                upnpContext->pending_igd_search_requests_--;
        }
        upnpContext->igd_search_condition_.notify_all();
    }
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

static void
checkResponseError(IXML_Document* doc)
{
    if (not doc)
        return;

    std::string errorCode = get_first_doc_item(doc, "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(doc, "errorDescription");
        RING_WARN("UPnP: response contains error: %s : %s", errorCode.c_str(), errorDescription.c_str());
    }
}

bool
UPnPContext::isIGDConnected(const IGD* igd)
{
    bool connected = false;
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    action.reset(UpnpMakeAction("GetStatusInfo", igd->getServiceType().c_str(), 0, nullptr));

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlpt_handle_, igd->getControlURL().c_str(), igd->getServiceType().c_str(),
                                  nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    checkResponseError(response.get());
    if( upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we chck if the igd is disconnected? */
        RING_WARN("UPnP: Failed to get GetStatusInfo from: %s, %s",
                  igd->getServiceType().c_str(), UpnpGetErrorMessage(upnp_err));

        return false;
    }

    /* for debugging, print response */
    // char* response_doc = ixmlPrintDocument(response.get());
    // RING_DBG("UPnP: GetStatusInfo response:\n%s", response_doc);
    // ixmlFreeDOMString(response_doc);

    /* parse response */
    std::string status = get_first_doc_item(response.get(), "NewConnectionStatus");
    if (status.compare("Connected") == 0)
        connected = true;

    /* response should also contain the following elements, but we don't care for now:
     *  "NewLastConnectionError"
     *  "NewUptime"
     */
    return connected;
}

IpAddr
UPnPContext::getExternalIP(const IGD* igd)
{
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    action.reset(UpnpMakeAction("GetExternalIPAddress", igd->getServiceType().c_str(), 0, nullptr));

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlpt_handle_, igd->getControlURL().c_str(), igd->getServiceType().c_str(),
                                  nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    checkResponseError(response.get());
    if( upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we chck if the igd is disconnected? */
        RING_WARN("UPnP: Failed to get GetExternalIPAddress from: %s, %s",
                  igd->getServiceType().c_str(), UpnpGetErrorMessage(upnp_err));
        return {};
    }

    /* parse response */
    return {get_first_doc_item(response.get(), "NewExternalIPAddress")};
}

void
UPnPContext::removeMappingsByLocalIPAndDescription(const IGD* igd, const std::string& description)
{
    /* need to get the local addr */
    IpAddr local_ip = ip_utils::getLocalAddr(pj_AF_INET());
    if (!local_ip) {
        RING_DBG("UPnP : cannot determine local IP in function removeMappingsByLocalIPAndDescription()");
        return;
    }

    RING_DBG("UPnP : removing all port mappings with description: \"%s\" and local ip: %s",
             description.c_str(), local_ip.toString().c_str());

    int upnp_err;
    int entry_idx = 0;
    bool done = false;

    do {
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
        IXML_Document* action_ptr = nullptr;
        UpnpAddToAction(&action_ptr, "GetGenericPortMappingEntry" , igd->getServiceType().c_str(),
                        "NewPortMappingIndex", std::to_string(entry_idx).c_str());
        action.reset(action_ptr);

        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
        IXML_Document* response_ptr = nullptr;
        int upnp_err = UpnpSendAction(ctrlpt_handle_, igd->getControlURL().c_str(), igd->getServiceType().c_str(),
                                      nullptr, action.get(), &response_ptr);
        response.reset(response_ptr);
        if( not response and upnp_err != UPNP_E_SUCCESS) {
            /* TODO: if failed, should we chck if the igd is disconnected? */
            RING_WARN("UPnP: Failed to get GetGenericPortMappingEntry from: %s, %s",
                      igd->getServiceType().c_str(), UpnpGetErrorMessage(upnp_err));
            return;
        }

        /* check if there is an error code */
        std::string errorCode = get_first_doc_item(response.get(), "errorCode");

        if (errorCode.empty()) {
            /* no error, prase the rest of the response */
            std::string desc_actual = get_first_doc_item(response.get(), "NewPortMappingDescription");
            std::string client_ip = get_first_doc_item(response.get(), "NewInternalClient");

            /* check if same IP and description */
            if (IpAddr(client_ip) == local_ip and desc_actual.compare(description) == 0) {
                /* get the rest of the needed parameters */
                std::string port_internal = get_first_doc_item(response.get(), "NewInternalPort");
                std::string port_external = get_first_doc_item(response.get(), "NewExternalPort");
                std::string protocol = get_first_doc_item(response.get(), "NewProtocol");

                RING_DBG("UPnP: deleting entry with matching desciption and ip:\n\t%s %5s->%s:%-5s '%s'",
                         protocol.c_str(), port_external.c_str(), client_ip.c_str(), port_internal.c_str(), protocol.c_str());

                /* delete entry */
                if (not deletePortMapping(igd, port_external, protocol)) {
                    /* failed to delete entry, skip it and increment the entry counter */
                    entry_idx++;
                }
                /* note: in the case that the entry deletion is successfull, we do not increment the entry
                 *       idx as the number of entries has decreased by one */
            } else {
                /* increment index */
                entry_idx++;
            }
        } else if (errorCode.compare("713") == 0) {
            /* 713 means there are no more entires */
            done = true;
        } else {
            std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
            RING_WARN("UPnP: GetGenericPortMappingEntry returned with error: %s: %s",
                      errorCode.c_str(), errorDescription.c_str());
            done = true;
        }
    } while(not done);
}

bool
UPnPContext::deletePortMapping(const IGD* igd, const std::string& port_external, const std::string& protocol)
{
    std::string action_name{"DeletePortMapping"};
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    IXML_Document* action_ptr = nullptr;
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewRemoteHost", "");
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewExternalPort", port_external.c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewProtocol", protocol.c_str());
    action.reset(action_ptr);

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlpt_handle_, igd->getControlURL().c_str(), igd->getServiceType().c_str(),
                                  nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    if( upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we check if the igd is disconnected? */
        RING_WARN("UPnP: Failed to get %s from: %s, %s",
                  action_name.c_str(), igd->getServiceType().c_str(), UpnpGetErrorMessage(upnp_err));
        return false;
    }
    /* check if there is an error code */
    std::string errorCode = get_first_doc_item(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
        RING_WARN("UPnP: %s returned with error: %s: %s",
                  action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        return false;
    } else
        return true;
}

bool
UPnPContext::addPortMapping(const IGD* igd, const Mapping& mapping, int* error_code)
{
    *error_code = UPNP_E_SUCCESS;

    std::string action_name{"AddPortMapping"};
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    IXML_Document* action_ptr = nullptr;
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewRemoteHost", "");
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewExternalPort", mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewProtocol", mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewInternalPort", mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewInternalClient", mapping.getLocalIp().toString().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewEnabled", "1");
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewPortMappingDescription", mapping.getDescription().c_str());
    /* for now assume lease duration is always infinite */
    UpnpAddToAction(&action_ptr, action_name.c_str() , igd->getServiceType().c_str(),
                    "NewLeaseDuration", "0");
    action.reset(action_ptr);

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlpt_handle_, igd->getControlURL().c_str(), igd->getServiceType().c_str(),
                                  nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    if( not response and upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we chck if the igd is disconnected? */
        RING_WARN("UPnP: Failed to get GetGenericPortMappingEntry from: %s, %s",
                  igd->getServiceType().c_str(), UpnpGetErrorMessage(upnp_err));
        *error_code = upnp_err;
        return false;
    }

    /* check if there is an error code */
    std::string errorCode = get_first_doc_item(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
        RING_WARN("UPnP: %s returned with error: %s: %s",
                  action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        *error_code = std::stoi(errorCode);
        return false;
    } else
        return true;
}


#endif /* HAVE_LIBUPNP */

}} // namespace ring::upnp
