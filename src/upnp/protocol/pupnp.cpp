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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_LIBUPNP

#include "logger.h"
#include "ip_utils.h"
#include "../upnp_context.h"
#include "../igd/upnp_igd.h"
#include "../mapping/global_mapping.h"
#include "compiler_intrinsics.h"

#include <string>
#include <set>
#include <mutex>
#include <thread>
#include <chrono>
#include <memory>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cstdlib> 

#include "pupnp.h"

namespace jami { namespace upnp {

std::shared_ptr<PUPnP>
getPUPnP()
{
    static auto pupnp = std::make_shared<PUPnP>();
    return pupnp;
}

PUPnP::PUPnP()
{
    int upnp_err;
    char* ip_address = nullptr;
    char* ip_address6 = nullptr;
    unsigned short port = 0;
    unsigned short port6 = 0;

    /* TODO: allow user to specify interface to be used
     *       by selecting the IP
     */

#if UPNP_ENABLE_IPV6
    /* IPv6 version seems to fail on some systems with message
     * UPNP_E_SOCKET_BIND: An error occurred binding a socket. 
     * TODO: figure out why ipv6 version doesn't work.  
     */
    JAMI_DBG("PUPnP: Initializing with UpnpInit2.");

    upnp_err = UpnpInit2(0, 0);
    if (upnp_err != PUPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: UpnpInit2 Failed to initialize.");
        JAMI_DBG("PUPnP: Initializing with UpnpInit (deprecated).");
        upnp_err = UpnpInit(0, 0);      // Deprecated function but fall back on it if UpnpInit2 fails. 
    } 
#else
    JAMI_DBG("PUPnP: Initializing with UpnpInit (deprecated).");
    upnp_err = UpnpInit(0, 0);           // Deprecated function but fall back on it if IPv6 not enabled.
#endif

    if (upnp_err != PUPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
    } else {
        JAMI_DBG("PUPnP: Initialization successful.");
        ip_address = UpnpGetServerIpAddress();      
        port = UpnpGetServerPort();
        ip_address6 = UpnpGetServerIp6Address();    
        port6 = UpnpGetServerPort6();
        JAMI_DBG("PUPnP: Initialiazed on %s:%u | %s:%u", ip_address, port, ip_address6, port6);

        // Relax the parser to allow malformed XML text.
        ixmlRelaxParser(1);

        // Register Upnp control point.
        upnp_err = UpnpRegisterClient(ctrlPtCallback, this, &ctrlptHandle_);
        if (upnp_err != PUPNP_E_SUCCESS) {
            JAMI_ERR("PUPnP: Can't register client: %s", UpnpGetErrorMessage(upnp_err));
            UpnpFinish();
        } else {
            JAMI_DBG("PUPnP: Control point registration successful.");
            clientRegistered_ = true;
        }

        if (clientRegistered_){
            searchForIGD();                 // Start gathering a list of available devices.
        }
    }
}

PUPnP::~PUPnP()
{
    /* make sure everything is unregistered, freed, and UpnpFinish() is called */
    {
        std::lock_guard<std::mutex> lock(validIGDMutex_);
        for( auto const &it : validIGDs_) {
            if (auto igd = dynamic_cast<UPnPIGD*>(it.second.get()))
                actionRemoveMappingsByLocalIPAndDescription(*igd, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
        }
        validIGDs_.clear();
    }

    // FIXME : on windows thread have already been destroyed at this point resulting in a deadlock
#ifndef _WIN32
    UpnpFinish();
#endif
}

void 
PUPnP::connectivityChanged()
{
    {
        // Lock internal IGD list.
        std::lock_guard<std::mutex> lock(validIGDMutex_);

        // Clear internal IGD list.
        validIGDs_.clear();

        // Notify.
        validIGDCondVar_.notify_all();
        for (const auto& l : igdListeners_)
            l.second();
    }
}

size_t 
PUPnP::addIGDListener(IGDFoundCallback&& cb)
{
    JAMI_DBG("PUPnP: Adding IGD listener.");

    std::lock_guard<std::mutex> lock(validIGDMutex_);
    auto token = ++listenerToken_;
    igdListeners_.emplace(token, std::move(cb));
    
    return token;
}
    
void 
PUPnP::removeIGDListener(size_t token)
{
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    auto it = igdListeners_.find(token);
    if (it != igdListeners_.end()) {
        JAMI_DBG("PUPnP: Removing igd listener.");
        igdListeners_.erase(it);
    }
}

Mapping 
PUPnP::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error)
{
    *upnp_error = -1;

    Mapping mapping {port_external, port_internal, type};

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
        GlobalMapping* mapping_ptr = &iter->second;
        if (*mapping_ptr == mapping) {
            /* the same mapping, so nothing needs to be done */
            *upnp_error = PUPNP_E_SUCCESS;
            ++(mapping_ptr->users);
            JAMI_DBG("PUPnP: Mapping already exists, incrementing number of users: %d",
                     iter->second.users);
            return mapping;
        } else {
            /* this port is already used by a different mapping */
            JAMI_WARN("PUPnP: Cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            *upnp_error = PUPNP_CONFLICT_IN_MAPPING;
            return {};
        }
    }

    /* mapping doesn't exist, so try to add it */
    JAMI_DBG("PUPnP: Attempting to add port mapping %s.", mapping.toString().c_str());

    auto upnp = dynamic_cast<const UPnPIGD*>(igd);
    if (not upnp or actionAddPortMapping(*upnp, mapping, upnp_error))
    {
        globalMappings->emplace(port_external, GlobalMapping{mapping});
        return mapping;
    }
    return {};
}

void 
PUPnP::removeMapping(const Mapping& mapping)
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */

    std::lock_guard<std::mutex> igdListLock(validIGDMutex_);

    // Iterate over all IGDs in internal list and try to remove selected mapping.
    for (auto const& item : validIGDs_) {
        if (not item.second) {
            continue;
        }

        /* first make sure the mapping exists in the global list of the igd */
        auto globalMappings = mapping.getType() == PortType::UDP ?
                            &item.second->udpMappings : &item.second->tcpMappings;

        auto iter = globalMappings->find(mapping.getPortExternal());
        if ( iter != globalMappings->end() ) {
            /* make sure its the same mapping */
            GlobalMapping& global_mapping = iter->second;
            if (mapping == global_mapping ) {
                /* now check the users */
                if (global_mapping.users > 1) {
                    /* more than one user, simply decrement the number */
                    --(global_mapping.users);
                    JAMI_DBG("PUPnP: Decrementing users of mapping %s, %d users remaining.",
                            mapping.toString().c_str(), global_mapping.users);
                } else {
                    /* no other users, can delete */
                    if (auto upnp = dynamic_cast<UPnPIGD*>(item.second.get())) {
                        JAMI_DBG("PUPnP: removing port mapping %s.",
                                mapping.toString().c_str());
                        actionDeletePortMapping(*upnp,
                                        mapping.getPortExternalStr(),
                                        mapping.getTypeStr());
                    }
                    globalMappings->erase(iter);
                }
            } else {
                JAMI_WARN("PUPnP: Cannot remove mapping which doesn't match the existing one in the IGD list.");
            }
        } else {
            JAMI_WARN("PUPnP: Cannot remove mapping which is not in the list of existing mappings of the IGD.");
        }
    }
}

void 
PUPnP::removeAllLocalMappings(IGD* igd)
{
    if (auto igd_del_map = dynamic_cast<UPnPIGD*>(igd)) {
        actionRemoveMappingsByLocalIPAndDescription(*igd_del_map, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
    }
}

int 
PUPnP::ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (auto pupnp = static_cast<PUPnP*>(user_data)) {
        return pupnp->handleCtrlPtUPnPEvents(event_type, event);
    }
    JAMI_WARN("PUPnP: Control point callback without PUPnP");
    return 0;
}

int 
PUPnP::subEventCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (std::string* udnPtr = static_cast<std::string*>(user_data)) {
        std::string udn = *udnPtr;
        return getPUPnP()->handleSubscriptionUPnPEvent(event_type, event, udn);   
    }
    JAMI_WARN("PUPnP: Subscription callback without service Id string.");
    return 0;
}

int 
PUPnP::handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event)
{
     // Lock mutex to prevent handling other discovery search results (or advertisements) simultaneously.
    std::lock_guard<std::mutex> cp_device_lock(cpDeviceMutex_);

    switch(event_type)
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE: 
    {   
        // Fall through. Treat advertisements like discovery search results.   
    }
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*)event;
        // JAMI_DBG("UPnP: UPNP_DISCOVERY_SEARCH_RESULT from %s", UpnpDiscovery_get_DeviceID_cstr(d_event));

        int upnp_err;

        // First check the error code. 
        if (UpnpDiscovery_get_ErrCode(d_event) != PUPNP_E_SUCCESS) {
            JAMI_WARN("PUPnP: Error in discovery event received by the CP -> %s.", UpnpGetErrorMessage(UpnpDiscovery_get_ErrCode(d_event)));
            break; 
        }

        /*
         * Check if this device ID is already in the list. If we reach the past-the-end
         * iterator of the list, it means we haven't discovered it. So we add it.
         */
        if (cpDeviceList_.count(std::string(UpnpDiscovery_get_DeviceID_cstr(d_event))) > 0) {
            break;
        }

        cpDeviceList_.emplace(std::pair<std::string, std::string>(std::string(UpnpDiscovery_get_DeviceID_cstr(d_event)), ""));
        /*
         * NOTE: This thing will block until success for the system socket timeout
         * unless libupnp is compile with '-disable-blocking-tcp-connections', in
         * which case it will block for the libupnp specified timeout.
         */
        IXML_Document* doc_container_ptr = nullptr;
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> doc_desc_ptr(nullptr, ixmlDocument_free);
        upnp_err = UpnpDownloadXmlDoc(UpnpDiscovery_get_Location_cstr(d_event), &doc_container_ptr);
        if (doc_container_ptr) {
            doc_desc_ptr.reset(doc_container_ptr);
        }
        
        if (upnp_err != PUPNP_E_SUCCESS or not doc_desc_ptr) {
            JAMI_WARN("PUPnP: Error downloading device XML document -> %s.", UpnpGetErrorMessage(upnp_err));
            break;
        } 

        // Check device type.
        std::string deviceType = get_first_doc_item(doc_desc_ptr.get(), "deviceType");
        if (deviceType.empty()) {
            // No device type. Exit.
            break;
        }

        if (deviceType.compare(UPNP_IGD_DEVICE) != 0) {
            // Device type not IGD. Exit.
            break;
        }

        std::unique_ptr<UPnPIGD> igd_candidate;
        igd_candidate = parseIGD(doc_desc_ptr.get(), d_event);
        if (not igd_candidate) {
            // No valid IGD candidate. Exit.
            break;
        } 

        JAMI_DBG("PUPnP: Validating IGD candidate.\n\tUDN: %s\n\tBase URL: %s\n\tName: %s\n\tserviceType: %s\n\tserviceID: %s\n\tcontrolURL: %s\n\teventSubURL: %s",
                    igd_candidate->getUDN().c_str(), 
                    igd_candidate->getBaseURL().c_str(), 
                    igd_candidate->getFriendlyName().c_str(), 
                    igd_candidate->getServiceType().c_str(), 
                    igd_candidate->getServiceId().c_str(), 
                    igd_candidate->getControlURL().c_str(), 
                    igd_candidate->getEventSubURL().c_str()); 

        // Check if IGD is connected.
        if (not actionIsIgdConnected(*igd_candidate)) {
            JAMI_WARN("PUPnP: IGD candidate %s is not connected.", igd_candidate->getUDN().c_str());
            break;
        }

        // Validate external Ip.
        igd_candidate->publicIp_ = actionGetExternalIP(*igd_candidate);
        if (igd_candidate->publicIp_.toString().empty()) {
            JAMI_WARN("PUPnP: IGD candidate %s has no valid external Ip.", igd_candidate->getUDN().c_str());
            break;
        }
        
        // Validate internal Ip.
        igd_candidate->localIp_ = ip_utils::getLocalAddr(pj_AF_INET());
        if (igd_candidate->localIp_.toString().empty()) {
            JAMI_WARN("PUPnP: No valid internal Ip.");
            break;
        }

        JAMI_DBG("PUPnP: Found device with external IP %s", igd_candidate->publicIp_.toString().c_str());

        // Store info for subscription.
        std::string eventSub = igd_candidate->getEventSubURL();
        std::string udn = igd_candidate->getUDN();

        // Add the igd to the upnp context class list.
        addIgdCb_(getPUPnP().get(), std::move(igd_candidate.get()));
        
        if (cpDeviceList_.count(udn) > 0) {
            cpDeviceList_[udn] = eventSub;
        }

        // Keep local IGD list internally.
        std::lock_guard<std::mutex> valid_igd_lock(validIGDMutex_);
        validIGDs_.emplace(std::move(igd_candidate->getUDN()), std::move(igd_candidate));
        validIGDCondVar_.notify_all();

        // Subscribe to IGD events.
        void *vp_to_udn = static_cast<void*>(new std::string(udn));
        upnp_err = UpnpSubscribeAsync(ctrlptHandle_, eventSub.c_str(), SUBSCRIBE_TIMEOUT, subEventCallback, vp_to_udn);
        if (upnp_err != PUPNP_E_SUCCESS) {
            JAMI_WARN("PUPnP: Error when trying to request subscription for %s -> %s.", udn.c_str(), UpnpGetErrorMessage(upnp_err));
        } 

        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        const UpnpDiscovery *d_event = (const UpnpDiscovery *)event;

        // Remvoe device Id from list.
        std::string cpDeviceId(UpnpDiscovery_get_DeviceID_cstr(d_event));
        cpDeviceList_.erase(cpDeviceId);

        IGD* igd_to_remove = nullptr;
        for (auto it = validIGDs_.find(cpDeviceId); it != validIGDs_.end(); it++) {
            
            // Store igd to remove.
            igd_to_remove = it->second.get();
            
            // Remove IGD from context list.
            removeIgdCb_(igd_to_remove);
            
            // Remove the IGD from the itnternal list and notify the listeners.
            std::lock_guard<std::mutex> valid_igd_lock(validIGDMutex_);
            validIGDs_.erase(std::move(it));
            validIGDCondVar_.notify_all();
            for (const auto& l : igdListeners_) {
                l.second();
            }
            break;
        }

        break;
    }
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        // Nothing to do here.
        break;
    }
    case UPNP_EVENT_RECEIVED:
    {
        const UpnpEvent *e_event = (const UpnpEvent *)event;
        
        char *xmlbuff = nullptr;
		xmlbuff = ixmlPrintNode((IXML_Node *)UpnpEvent_get_ChangedVariables(e_event));
		// JAMI_DBG("UPnP: UPNP_EVENT_RECEIVED\n\tSID: %s\n\tEventKey: %d\n\tChangedVars: %s", 
        //     UpnpString_get_String(UpnpEvent_get_SID(e_event)), 
        //     UpnpEvent_get_EventKey(e_event), 
        //     xmlbuff);
        
		ixmlFreeDOMString(xmlbuff);

        // TODO: Handle event by updating any changed state variables */
        break;
    }
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    {
        // Fall through. Treat failed autorenewal like an expired subscription.
    }
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:   // This event will occur only if autorenewal is disabled. 
    {
        int upnp_err;
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        
        std::string eventSubUrl(UpnpEventSubscribe_get_PublisherUrl_cstr(es_event));

        std::pair<std::string, std::string> foundDevice = std::make_pair("", "");
        bool foundEventSubUrl = false;
        auto it = cpDeviceList_.begin();
        while(it != cpDeviceList_.end() and not foundEventSubUrl) {
            if(it->second == eventSubUrl) {
                foundEventSubUrl = true;
                foundDevice = std::make_pair(it->first, it->second);
            }
            it++;
        }

        if (not foundEventSubUrl) {
            // If we don't find event subscription url then exit.
            break;
        }

        std::string udn = foundDevice.first;
        std::string eventSub = foundDevice.second;

        // Renew subscriptons to IGD events.
        void *vp_to_udn = static_cast<void*>(new std::string(udn));
        upnp_err = UpnpSubscribeAsync(ctrlptHandle_, eventSub.c_str(), SUBSCRIBE_TIMEOUT, subEventCallback, vp_to_udn);
        if (upnp_err != PUPNP_E_SUCCESS) {
            JAMI_WARN("PUPnP: Error when trying to renew subscription for %s -> %s.", udn.c_str(), UpnpGetErrorMessage(upnp_err));
        } else {
            JAMI_DBG("PUPnP: Renewed subscription for %s.", udn.c_str());
        }
        std::free(vp_to_udn);       

        break;
    }
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    {
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        JAMI_DBG("PUPnP: UPNP_EVENT_SUBSCRIBE_COMPLETE");
		
        // JAMI_DBG("\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut: %d\n",
		// 	UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
		// 	UpnpEventSubscribe_get_ErrCode(es_event),
		// 	UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
		// 	UpnpEventSubscribe_get_TimeOut(es_event));
        break;
    }
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: 
    {
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        JAMI_DBG("PUPnP: UPNP_EVENT_UNSUBSCRIBE_COMPLETE");
		
        // JAMI_DBG("\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut: %d\n",
		// 	UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
		// 	UpnpEventSubscribe_get_ErrCode(es_event),
		// 	UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
		// 	UpnpEventSubscribe_get_TimeOut(es_event));
        break;
	}
    case UPNP_CONTROL_ACTION_COMPLETE:
    {
        const UpnpActionComplete *a_event = (const UpnpActionComplete *)event;
        JAMI_DBG("PUPnP: UPNP_CONTROL_ACTION_COMPLETE.");
#ifndef _WIN32
		char *xmlbuff = nullptr;
		int errCode = UpnpActionComplete_get_ErrCode(a_event);
		const char *ctrlURL = UpnpString_get_String(UpnpActionComplete_get_CtrlUrl(a_event));
		IXML_Document *actionRequest = UpnpActionComplete_get_ActionRequest(a_event);
		IXML_Document *actionResult = UpnpActionComplete_get_ActionResult(a_event);

		JAMI_DBG("\tErrCode: %d\n\tCtrlUrl: %s", errCode, ctrlURL);
		
        if (actionRequest) {
			xmlbuff = ixmlPrintNode((IXML_Node *)actionRequest);
			if (xmlbuff) {
				JAMI_DBG("\tActRequest: %s\n", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = nullptr;
		} else {
			JAMI_DBG("\tActRequest: (null)");
		}
		if (actionResult) {
			xmlbuff = ixmlPrintNode((IXML_Node *)actionResult);
			if (xmlbuff) {
				JAMI_DBG("\tActResult: %s", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = nullptr;
		} else {
			JAMI_DBG("\tActResult: (null)");
		}
        /* TODO: no need for any processing here, just print out results.
         * Service state table updates are handled by events. */
#endif
        break;
    }
    default:
    {
        JAMI_WARN("PUPnP: Unhandled Control Point event.");
        break;
    }
    }

    return PUPNP_E_SUCCESS; /* return value currently ignored by SDK */
}

int 
PUPnP::handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event, std::string udn)
{
    std::lock_guard<std::mutex> cp_device_lock(cpDeviceMutex_);
    
    int upnp_err;
    const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
    // JAMI_DBG("Handle subscription event for %s.\n\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut %d",
	// 		udn.c_str(),
    //         UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
	// 		UpnpEventSubscribe_get_ErrCode(es_event),
	// 		UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
	// 		UpnpEventSubscribe_get_TimeOut(es_event));
    
    upnp_err = UpnpEventSubscribe_get_ErrCode(es_event);
    if (upnp_err != PUPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Error when trying to handle subscription callback for %s -> %s.", udn.c_str(), UpnpGetErrorMessage(upnp_err));
        return upnp_err;
    }

    // TODO: Handle subscription event.

    return PUPNP_E_SUCCESS;
}

void 
PUPnP::searchForIGD()
{
    if (not clientRegistered_) {
        JAMI_WARN("PUPnP: Control Point not registered.");
        return;
    }

    // Send out search for multiple types of devices, as some routers may possibly only reply to one.
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
}

std::unique_ptr<UPnPIGD> 
PUPnP::parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event)
{
    if (not doc or not d_event)
        return nullptr;

    /*
     * Check the UDN to see if its already in our device list. If it
     * is, then update the device advertisement timeout (expiration).
     */
    std::string UDN = get_first_doc_item(doc, "UDN");
    if (UDN.empty()) {
        JAMI_WARN("PUPnP: could not find UDN in description document of device.");
        return nullptr;
    } else {
        std::lock_guard<std::mutex> lock(validIGDMutex_);
        auto it = validIGDs_.find(UDN);
        if (it != validIGDs_.end()) {
            // We already have this device in our list.
            return nullptr;
        }
    }

    std::unique_ptr<UPnPIGD> new_igd;
    int upnp_err;

    // Get friendly name.
    std::string friendlyName = get_first_doc_item(doc, "friendlyName");

    // Get base URL.
    std::string baseURL = get_first_doc_item(doc, "URLBase");
    if (baseURL.empty()) {
        baseURL = std::string(UpnpDiscovery_get_Location_cstr(d_event));
    }

    // Get list of services defined by serviceType.
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> serviceList(nullptr, ixmlNodeList_free);
    serviceList.reset(ixmlDocument_getElementsByTagName(doc, "serviceType"));
    unsigned long list_length = ixmlNodeList_length(serviceList.get());
    
    /* 
     * Go through the "serviceType" nodes until we find the first service of type
     * WANIPConnection or WANPPPConnection which is connected to an external network. 
     */
    for (unsigned long node_idx = 0; node_idx < list_length; node_idx++) {
        
        IXML_Node* serviceType_node = ixmlNodeList_item(serviceList.get(), node_idx);
        std::string serviceType = get_element_text(serviceType_node);

        // Only check serviceType of WANIPConnection or WANPPPConnection.
        if (not serviceType.compare(UPNP_WANIP_SERVICE) == 0 and not serviceType.compare(UPNP_WANPPP_SERVICE) == 0) {
            // IGD is not WANIP or WANPPP service. Going to next node.
            continue;
        }
        
        /* 
        * Found a correct "serviceType." Now get the parent node because
        * the rest of the service definitions are siblings of "serviceType." 
        */
        IXML_Node* service_node = ixmlNode_getParentNode(serviceType_node);
        if (not service_node) {
            // IGD serviceType has no parent node. Going to next node.
            continue;
        }

        // Perform sanity check. The parent node should be called "service".
        if(strcmp(ixmlNode_getNodeName(service_node), "service") != 0) {
            // IGD "serviceType" parent node is not called "service". Going to next node.
            continue;
        }

        // Get serviceId.
        IXML_Element* service_element = (IXML_Element*)service_node;
        std::string serviceId = get_first_element_item(service_element, "serviceId");
        if (serviceId.empty()){
            // IGD "serviceId" is empty. Going to next node.
            continue;
        }

        // Get the relative controlURL and turn it into absolute address using the URLBase.
        std::string controlURL = get_first_element_item(service_element, "controlURL");
        if (controlURL.empty()) {
            // IGD control URL is empty. Going to next node."
            continue;
        }

        char* absolute_control_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), controlURL.c_str(), &absolute_control_url);
        if (upnp_err == PUPNP_E_SUCCESS) {
            controlURL = absolute_control_url;
        } else {
            JAMI_WARN("PUPnP: Error resolving absolute controlURL -> %s.", UpnpGetErrorMessage(upnp_err));
        }
        std::free(absolute_control_url);

        // Get the relative eventSubURL and turn it into absolute address using the URLBase.
        std::string eventSubURL = get_first_element_item(service_element, "eventSubURL");
        if (eventSubURL.empty()) {
            JAMI_WARN("PUPnP: IGD event sub URL is empty. Going to next node.");
            continue;
        }

        char* absolute_event_sub_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), eventSubURL.c_str(), &absolute_event_sub_url);
        if (upnp_err == PUPNP_E_SUCCESS) {
            eventSubURL = absolute_event_sub_url;
        } else {
            JAMI_WARN("PUPnP: Error resolving absolute eventSubURL -> %s.", UpnpGetErrorMessage(upnp_err));
        }
        std::free(absolute_event_sub_url);

        new_igd.reset(new UPnPIGD(std::move(UDN),
                                  std::move(baseURL),
                                  std::move(friendlyName),
                                  std::move(serviceType),
                                  std::move(serviceId),
                                  std::move(controlURL),
                                  std::move(eventSubURL)));

        return new_igd;
    }

    return nullptr;
}

bool 
PUPnP::actionIsIgdConnected(const UPnPIGD& igd)
{
    int upnp_err;

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    action_container_ptr = UpnpMakeAction("GetStatusInfo", igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetStatusInfo action.");
        return false;
    }
    action.reset(action_container_ptr);
    
    upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if (upnp_err != PUPNP_E_SUCCESS){
        JAMI_WARN("PUPnP: Failed to send GetStatusInfo action -> %s.", UpnpGetErrorMessage(upnp_err));
        return false;
    }
    response.reset(response_container_ptr);
    
    if(error_on_response(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetStatusInfo from %s -> %d: %s.",
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }

    // Parse response.
    std::string status = get_first_doc_item(response.get(), "NewConnectionStatus");
    if (status.compare("Connected") != 0) {
        return false;
    }

    return true;
}

IpAddr 
PUPnP::actionGetExternalIP(const UPnPIGD& igd)
{
    int upnp_err;

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    action_container_ptr = UpnpMakeAction("GetExternalIPAddress", igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetExternalIPAddress action.");
        return {};
    }
    action.reset(action_container_ptr);
    
    upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if (upnp_err != PUPNP_E_SUCCESS){
        JAMI_WARN("PUPnP: Failed to send GetExternalIPAddress action -> %s.", UpnpGetErrorMessage(upnp_err));
        return {};
    }
    response.reset(response_container_ptr);

    if(error_on_response(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetExternalIPAddress from %s -> %d: %s.",
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return {};
    }

    return { get_first_doc_item(response.get(), "NewExternalIPAddress") };
}

void 
PUPnP::actionRemoveMappingsByLocalIPAndDescription(const UPnPIGD& igd, const std::string& description)
{
    if (!igd.localIp_) {
        JAMI_DBG("PUPnP: Cannot determine local IP for IGD in function removeMappingsBylocalIpAndDescription().");
        return;
    }

    int entry_idx = 0;
    bool done = false;

    do {
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
        IXML_Document* action_ptr = nullptr;
        UpnpAddToAction(&action_ptr, "GetGenericPortMappingEntry", igd.getServiceType().c_str(), "NewPortMappingIndex", std::to_string(entry_idx).c_str());
        action.reset(action_ptr);

        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
        IXML_Document* response_ptr = nullptr;
        int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(),
                                      igd.getServiceType().c_str(), nullptr, action.get(), &response_ptr);
        response.reset(response_ptr);
        if( not response and upnp_err != PUPNP_E_SUCCESS) {
            return;
        }

        /* check if there is an error code */
        std::string errorCode = get_first_doc_item(response.get(), "errorCode");

        if (errorCode.empty()) {
            /* no error, prase the rest of the response */
            std::string desc_actual = get_first_doc_item(response.get(), "NewPortMappingDescription");
            std::string client_ip = get_first_doc_item(response.get(), "NewInternalClient");

            /* check if same IP and description */
            if (IpAddr(client_ip) == igd.localIp_ and desc_actual.compare(description) == 0) {
                /* get the rest of the needed parameters */
                std::string port_internal = get_first_doc_item(response.get(), "NewInternalPort");
                std::string port_external = get_first_doc_item(response.get(), "NewExternalPort");
                std::string protocol = get_first_doc_item(response.get(), "NewProtocol");

                JAMI_DBG("PUPnP: deleting entry with matching desciption and ip:\n\t%s %5s->%s:%-5s '%s'",
                         protocol.c_str(), port_external.c_str(), client_ip.c_str(), port_internal.c_str(), desc_actual.c_str());

                /* delete entry */
                if (not actionDeletePortMapping(igd, port_external, protocol)) {
                    /* failed to delete entry, skip it and try the next one */
                    ++entry_idx;
                }
                /* note: in the case that the entry deletion is successful, we do not increment the entry
                 *       idx as the number of entries has decreased by one */
            } else
                ++entry_idx;

        } else if (errorCode.compare(ARRAY_IDX_INVALID_STR) == 0
                   or errorCode.compare(INVALID_ARGS_STR) == 0) {
            /* 713 means there are no more entires, but some routers will return 402 instead */
            done = true;
        } else {
            std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
            JAMI_WARN("PUPnP: GetGenericPortMappingEntry returned with error: %s: %s.",
                      errorCode.c_str(), errorDescription.c_str());
            done = true;
        }
    } while(not done);
}

bool 
PUPnP::actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol)
{
    int upnp_err;

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    std::string action_name { "DeletePortMapping" };

    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewRemoteHost", "");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewExternalPort", port_external.c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewProtocol", protocol.c_str());

    action.reset(action_container_ptr);
    upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if(upnp_err != PUPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send %s from: %s, %d: %s.", action_name.c_str(), igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }

    if (not response_container_ptr) {
        JAMI_WARN("PUPnP: Failed to get response from %s.", action_name.c_str());
        return false;
    }
    response.reset(response_container_ptr);
    
    // Check if there is an error code.
    std::string errorCode = get_first_doc_item(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
        JAMI_WARN("PUPnP: %s returned with error: %s: %s.", action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        return false;
    }

    return true;
}

bool 
PUPnP::actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, int* error_code)
{
    int upnp_err;

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    *error_code = PUPNP_E_SUCCESS;

    std::string action_name{"AddPortMapping"};

    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewRemoteHost", "");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewExternalPort", mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewProtocol", mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewInternalPort", mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewInternalClient", igd.localIp_.toString().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewEnabled", "1");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewPortMappingDescription", mapping.getDescription().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewLeaseDuration", "0");
    
    action.reset(action_container_ptr);
    upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if(upnp_err != PUPNP_E_SUCCESS) {

        JAMI_WARN("PUPnP: Failed to send action %s from: %s, %d: %s.", action_name.c_str(), igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        *error_code = -1; /* make sure to -1 since we didn't get a response */
        return false;
    }

    if (not response_container_ptr) {
        JAMI_WARN("PUPnP: Failed to get response from %s.", action_name.c_str());
        return false;
    }
    response.reset(response_container_ptr);

    // Check if there is an error code.
    std::string errorCode = get_first_doc_item(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
        JAMI_WARN("PUPnP: %s returned with error: %s: %s.", action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        *error_code = jami::stoi(errorCode);
        return false;
    }
    return true;
}

}} // namespace jami::upnp

#endif