/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *    Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#include "pupnp.h"

namespace jami { namespace upnp {

// Helper functions for xml parsing.
static std::string
getElementText(IXML_Node* node)
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
getFirstDocItem(IXML_Document* doc, const char* item)
{
    std::string ret;
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
    if (nodeList) {
        // If there are several nodes which match the tag, we only want the first one.
        ret = getElementText(ixmlNodeList_item(nodeList.get(), 0));
    }
    return ret;
}

static std::string
getFirstElementItem(IXML_Element* element, const char* item)
{
    std::string ret;
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlElement_getElementsByTagName(element, item), ixmlNodeList_free);
    if (nodeList) {
        // If there are several nodes which match the tag, we only want the first one.
        ret = getElementText(ixmlNodeList_item(nodeList.get(), 0));
    }
    return ret;
}

static bool
errorOnResponse(IXML_Document* doc)
{
    if (not doc)
        return true;

    std::string errorCode = getFirstDocItem(doc, "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = getFirstDocItem(doc, "errorDescription");
        JAMI_WARN("PUPnP: Response contains error: %s : %s", errorCode.c_str(), errorDescription.c_str());
        return true;
    }
    return false;
}

PUPnP::PUPnP()
{
    pupnpThread_ = std::thread([this] { registerClientAsync(); });

    int upnp_err = UPNP_E_SUCCESS;
    char* ip_address = nullptr;
    char* ip_address6 = nullptr;
    unsigned short port = 0;
    unsigned short port6 = 0;

#if UPNP_ENABLE_IPV6
    upnp_err = UpnpInit2(0, 0);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: UpnpInit2 Failed to initialize");
        UpnpFinish();                    // Destroy threads before reusing upnp init function.
        upnp_err = UpnpInit(0, 0);      // Deprecated function but fall back on it if UpnpInit2 fails.
    }
#else
    upnp_err = UpnpInit(0, 0);           // Deprecated function but fall back on it if IPv6 not enabled.
#endif

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
    } else {
        ip_address = UpnpGetServerIpAddress();
        port = UpnpGetServerPort();
#if UPNP_ENABLE_IPV6
        ip_address6 = UpnpGetServerIp6Address();
        port6 = UpnpGetServerPort6();
#endif
        if (ip_address6 and port6) {
            JAMI_DBG("PUPnP: Initialiazed on %s:%u | %s:%u", ip_address, port, ip_address6, port6);
        } else {
            JAMI_DBG("PUPnP: Initialiazed on %s:%u", ip_address, port);
        }

        // Relax the parser to allow malformed XML text.
        ixmlRelaxParser(1);
    }
}

PUPnP::~PUPnP()
{
    // Clear all the lists.
    {
        std::lock_guard<std::mutex> lk(validIgdMutex_);
        for(auto const &it : validIgdList_) {
            if (auto igd = dynamic_cast<UPnPIGD*>(it.second.get()))
                actionDeletePortMappingsByDesc(*igd, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
        }
        validIgdList_.clear();
        cpDeviceList_.clear();
    }

    // Notify thread to terminate. UpnpFinish function will get called.
    pupnpRun_ = false;
    pupnpCv_.notify_all();
    if (pupnpThread_.joinable()) {
        pupnpThread_.join();
    }
}

void
PUPnP::clearIGDs()
{
    // Lock internal IGD list.
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    // Clear internal IGD list.
    validIgdList_.clear();
    cpDeviceList_.clear();
}

void
PUPnP::searchForIGD()
{
    // Notify registerClientAsync function running in thread to execute in non-blocking fashion.
    {
        std::lock_guard<std::mutex> lk(ctrlptMutex_);
        pupnpRun_ = true;
    }
    pupnpCv_.notify_one();
}

Mapping
PUPnP::addMapping(const upnp::UPnPProtocol::Service id, IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error)
{
    upnp_error = UPnPProtocol::UpnpError::INVALID_ERR;

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
            upnp_error = UPnPProtocol::UpnpError::ERROR_OK;
            ++(mapping_ptr->users);
            JAMI_DBG("PUPnP: Mapping already exists, incrementing number of users: %d",
                     iter->second.users);
            return mapping;
        } else {
            /* this port is already used by a different mapping */
            JAMI_WARN("PUPnP: Cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            upnp_error = UPnPProtocol::UpnpError::CONFLICT_IN_MAPPING;
            return {};
        }
    }

    if (auto pupnp_igd = dynamic_cast<const UPnPIGD*>(igd)) {
        JAMI_DBG("PUPnP: Attempting to open port %s", mapping.toString().c_str());
        if (actionAddPortMappingAsync(id, *pupnp_igd, mapping, upnp_error)) {
            // Here we add the new mapping to the internal list, and we notify the
            // controller that the port has been opened. However, since it's an
            // asynchronous call, it isn't technically opened yet, only that the
            // asynchronous function call succeeded. For now we keep it this way
            // but eventually the controller will get notified with a callback.
            globalMappings->emplace(port_external, GlobalMapping{mapping});
            return mapping;
        }
    }
    return {};
}

void
PUPnP::removeMapping(const Mapping& igdMapping)
{
    // Lock mutex to protect IGD list.
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    // Iterate over all IGDs in internal list and try to remove selected mapping.
    for (auto const& item : validIgdList_) {

        if (not item.second) {
            continue;
        }

        // Get mappings of IGD depending on type.
        PortMapGlobal* globalMappings;
        if (igdMapping.getType() == PortType::UDP) {
            globalMappings = &item.second->udpMappings;
        } else {
            globalMappings = &item.second->tcpMappings;
        }

        // Check if the mapping we want to remove is present in the IGD.
        auto mapToRemove = globalMappings->find(igdMapping.getPortExternal());
        if (mapToRemove != globalMappings->end()) {
            // Check if the mapping we want to remove is the same as the one that is present.
            GlobalMapping& global_mapping = mapToRemove->second;
            if (igdMapping == global_mapping) {
                // Check the users.
                if (global_mapping.users > 1) {
                    // More than one user so not unique port. Decrement number of users.
                    --(global_mapping.users);
                    JAMI_DBG("PUPnP: Decrementing users of mapping %s, %d users remaining",
                            igdMapping.toString().c_str(), global_mapping.users);
                } else {
                    // No other users so unique port. We can remove it compeltely.
                    if (auto upnp = dynamic_cast<UPnPIGD*>(item.second.get())) {
                        JAMI_DBG("PUPnP: Attempting to close port %s %s", igdMapping.getPortExternalStr().c_str(), igdMapping.getTypeStr().c_str());
                        actionDeletePortMappingAsync(*upnp,
                                                     igdMapping.getPortExternalStr(),
                                                     igdMapping.getTypeStr());
                    }
                    // Remove the mapping locally.
                    globalMappings->erase(mapToRemove);
                }
            }
        }
    }
}

void
PUPnP::removeAllLocalMappings(IGD* igd)
{
    if (auto igd_del_map = dynamic_cast<UPnPIGD*>(igd)) {
        actionDeletePortMappingsByDesc(*igd_del_map, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
    }
}

void
PUPnP::registerClientAsync()
{
    std::unique_lock<std::mutex> lk(ctrlptMutex_);

    while (pupnpRun_) {

        pupnpCv_.wait(lk);

        if (not clientRegistered_) {
            // Register Upnp control point.
            int upnp_err = UpnpRegisterClient(ctrlPtCallback, this, &ctrlptHandle_);
            if (upnp_err != UPNP_E_SUCCESS) {
                JAMI_ERR("PUPnP: Can't register client: %s", UpnpGetErrorMessage(upnp_err));
            } else {
                clientRegistered_ = true;
            }
        }

        if (not pupnpRun_) {
            break;
        }

        if (clientRegistered_) {
            // Send out search for multiple types of devices, as some routers may possibly only reply to one.
            UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
            UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
            UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
            UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
        }
    }

    UpnpFinish();
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
PUPnP::handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event)
{
     // Lock mutex to prevent handling other discovery search results (or advertisements) simultaneously.
    std::lock_guard<std::mutex> lk(ctrlptMutex_);

    switch(event_type)
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE: // Fall through. Treat advertisements like discovery search results.
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*)event;
        int upnp_err;

        // First check the error code.
        if (UpnpDiscovery_get_ErrCode(d_event) != UPNP_E_SUCCESS) {
            break;
        }

        // Check if this device ID is already in the list.
        std::string cpDeviceId = UpnpDiscovery_get_DeviceID_cstr(d_event);
        std::lock_guard<std::mutex> lk(validIgdMutex_);
        if (cpDeviceList_.count(cpDeviceId) > 0) {
            break;
        }
        cpDeviceList_.emplace(std::move(cpDeviceId), std::string{});

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

        if (upnp_err != UPNP_E_SUCCESS or not doc_desc_ptr) {
            JAMI_WARN("PUPnP: Error downloading device XML document -> %s", UpnpGetErrorMessage(upnp_err));
            break;
        }

        // Check device type.
        std::string deviceType = getFirstDocItem(doc_desc_ptr.get(), "deviceType");
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
            JAMI_WARN("PUPnP: IGD candidate %s is not connected", igd_candidate->getUDN().c_str());
            break;
        }

        // Validate external Ip.
        igd_candidate->publicIp_ = actionGetExternalIP(*igd_candidate);
        if (igd_candidate->publicIp_.toString().empty()) {
            JAMI_WARN("PUPnP: IGD candidate %s has no valid external Ip", igd_candidate->getUDN().c_str());
            break;
        }

        // Validate internal Ip.
        igd_candidate->localIp_ = ip_utils::getLocalAddr(pj_AF_INET());
        if (igd_candidate->localIp_.toString().empty()) {
            JAMI_WARN("PUPnP: No valid internal Ip.");
            break;
        }

        JAMI_DBG("PUPnP: Found device with external IP %s", igd_candidate->publicIp_.toString().c_str());

        // Store public IP.
        std::string publicIpStr(std::move(igd_candidate->publicIp_.toString()));

        // Store info for subscription.
        std::string eventSub = igd_candidate->getEventSubURL();
        std::string udn = igd_candidate->getUDN();

        // Remove any local mappings that may be left over from last time used.
        removeAllLocalMappings(igd_candidate.get());

        // Add the igd to the upnp context class list.
        if (updateIgdListCb_(this, std::move(igd_candidate.get()), std::move(igd_candidate.get()->publicIp_), true)) {
            JAMI_DBG("PUPnP: IGD with public IP %s was added to the list", publicIpStr.c_str());
        } else {
            JAMI_DBG("PUPnP: IGD with public IP %s is already in the list", publicIpStr.c_str());
        }

        // Keep local IGD list internally.
        if (cpDeviceList_.count(udn) > 0) {
            cpDeviceList_[udn] = eventSub;
        }
        validIgdList_.emplace(std::move(igd_candidate->getUDN()), std::move(igd_candidate));

        // Subscribe to IGD events.
        upnp_err = UpnpSubscribeAsync(ctrlptHandle_, eventSub.c_str(), SUBSCRIBE_TIMEOUT, subEventCallback, this);
        if (upnp_err != UPNP_E_SUCCESS) {
            JAMI_WARN("PUPnP: Error when trying to request subscription for %s -> %s", udn.c_str(), UpnpGetErrorMessage(upnp_err));
        }
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        const UpnpDiscovery *d_event = (const UpnpDiscovery *)event;

        std::lock_guard<std::mutex> lk(validIgdMutex_);

        // Remvoe device Id from list.
        std::string cpDeviceId(UpnpDiscovery_get_DeviceID_cstr(d_event));
        cpDeviceList_.erase(cpDeviceId);

        IGD* igd_to_remove = nullptr;
        for (auto it = validIgdList_.find(cpDeviceId); it != validIgdList_.end(); it++) {

            // Store igd to remove.
            igd_to_remove = it->second.get();

            // Remove IGD from context list.
            updateIgdListCb_(this, igd_to_remove, igd_to_remove->publicIp_, false);

            // Remove the IGD from the itnternal list and notify the listeners.
            validIgdList_.erase(std::move(it));
            break;
        }
        break;
    }
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        // Nothing to do here.
        break;
    }
    case UPNP_EVENT_AUTORENEWAL_FAILED:     // Fall through. Treat failed autorenewal like an expired subscription.
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:   // This event will occur only if autorenewal is disabled.
    {
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;

        std::string eventSubUrl(UpnpEventSubscribe_get_PublisherUrl_cstr(es_event));

        std::pair<std::string, std::string> foundDevice = std::make_pair("", "");
        bool foundEventSubUrl = false;
        auto it = cpDeviceList_.begin();
        while(it != cpDeviceList_.end()) {
            if(it->second == eventSubUrl) {
                foundEventSubUrl = true;
                foundDevice = std::make_pair(it->first, it->second);
                break;
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
        UpnpSubscribeAsync(ctrlptHandle_, eventSub.c_str(), SUBSCRIBE_TIMEOUT, subEventCallback, this);

        break;
    }
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    {
        break;
    }
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
    {
        break;
    }
    case UPNP_EVENT_RECEIVED:
    {
        break;
    }
    case UPNP_CONTROL_ACTION_COMPLETE:
    {
        const UpnpActionComplete* a_event = (const UpnpActionComplete*)event;

        int errCode = UpnpActionComplete_get_ErrCode(a_event);

        if (errCode == UPNP_E_SUCCESS) {

            IXML_Document* actionRequest = UpnpActionComplete_get_ActionRequest(a_event);

            if (actionRequest) {

                char* xmlbuff = nullptr;
                const char* ctrlURL = UpnpString_get_String(UpnpActionComplete_get_CtrlUrl(a_event));

                std::string serviceId(getFirstDocItem(actionRequest, "NewServiceRequest"));
                std::string port_external(getFirstDocItem(actionRequest, "NewExternalPort"));
                std::string port_internal(getFirstDocItem(actionRequest, "NewInternalPort"));
                std::string protocol(getFirstDocItem(actionRequest, "NewProtocol"));

                xmlbuff = ixmlPrintNode((IXML_Node *)actionRequest);

                if (xmlbuff) {

                    char* addMap = nullptr;
                    addMap = strstr(xmlbuff, (char*)"AddPortMapping");
                    if(addMap) {
                        Mapping* mapping = new Mapping(std::move(std::stoi(port_external)),
                                                       std::move(std::stoi(port_internal)),
                                                       protocol == "UDP" ? upnp::PortType::UDP : upnp::PortType::TCP);
                        JAMI_WARN("PUPnP: Opened port %s:%s %s", port_external.c_str(), port_internal.c_str(), protocol.c_str());
                        upnp::UPnPProtocol::Service id = std::move((upnp::UPnPProtocol::Service)std::stoi(serviceId));
                        uint16_t port_used = std::move(mapping->getPortInternal());
                        notifyPortOpenCb_(id,
                                          mapping,
                                          &port_used,
                                          true);
                    }
                    addMap = nullptr;

                    char* removeMap = nullptr;
                    removeMap = strstr(xmlbuff, (char*)"DeletePortMapping");
                    if(removeMap) {
                        JAMI_WARN("PUPnP: Closed port %s %s", port_external.c_str(), protocol.c_str());
                    }
                    removeMap = nullptr;

                    ixmlFreeDOMString(xmlbuff);
                }

                xmlbuff = nullptr;
            }
        }
        break;
    }
    default:
    {
        JAMI_WARN("PUPnP: Unhandled Control Point event");
        break;
    }
    }

    return UPNP_E_SUCCESS;
}

int
PUPnP::subEventCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (auto pupnp = static_cast<PUPnP*>(user_data)) {
        return pupnp->handleSubscriptionUPnPEvent(event_type, event);
    }
    JAMI_WARN("PUPnP: Subscription callback without service Id string");
    return 0;
}

int
PUPnP::handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event)
{
    std::lock_guard<std::mutex> lk(ctrlptMutex_);

    const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;

    int upnp_err = UpnpEventSubscribe_get_ErrCode(es_event);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Error when trying to handle subscription callback -> %s", UpnpGetErrorMessage(upnp_err));
        return upnp_err;
    }

    // TODO: Handle subscription event.

    return UPNP_E_SUCCESS;
}

std::unique_ptr<UPnPIGD>
PUPnP::parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event)
{
    if (not doc or not d_event)
        return nullptr;

    // Check the UDN to see if its already in our device list.
    std::string UDN = getFirstDocItem(doc, "UDN");
    if (UDN.empty()) {
        JAMI_WARN("PUPnP: could not find UDN in description document of device");
        return nullptr;
    } else {
        auto it = validIgdList_.find(UDN);
        if (it != validIgdList_.end()) {
            // We already have this device in our list.
            return nullptr;
        }
    }

    std::unique_ptr<UPnPIGD> new_igd;
    int upnp_err;

    // Get friendly name.
    std::string friendlyName = getFirstDocItem(doc, "friendlyName");

    // Get base URL.
    std::string baseURL = getFirstDocItem(doc, "URLBase");
    if (baseURL.empty()) {
        baseURL = std::string(UpnpDiscovery_get_Location_cstr(d_event));
    }

    // Get list of services defined by serviceType.
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> serviceList(nullptr, ixmlNodeList_free);
    serviceList.reset(ixmlDocument_getElementsByTagName(doc, "serviceType"));
    unsigned long list_length = ixmlNodeList_length(serviceList.get());

    // Go through the "serviceType" nodes until we find the the correct service type.
    for (unsigned long node_idx = 0; node_idx < list_length; node_idx++) {

        IXML_Node* serviceType_node = ixmlNodeList_item(serviceList.get(), node_idx);
        std::string serviceType = getElementText(serviceType_node);

        // Only check serviceType of WANIPConnection or WANPPPConnection.
        if (serviceType != std::string(UPNP_WANIP_SERVICE) && serviceType != std::string(UPNP_WANPPP_SERVICE)) {
            // IGD is not WANIP or WANPPP service. Going to next node.
            continue;
        }

        // Get parent node.
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
        std::string serviceId = getFirstElementItem(service_element, "serviceId");
        if (serviceId.empty()){
            // IGD "serviceId" is empty. Going to next node.
            continue;
        }

        // Get the relative controlURL and turn it into absolute address using the URLBase.
        std::string controlURL = getFirstElementItem(service_element, "controlURL");
        if (controlURL.empty()) {
            // IGD control URL is empty. Going to next node.
            continue;
        }

        char* absolute_control_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), controlURL.c_str(), &absolute_control_url);
        if (upnp_err == UPNP_E_SUCCESS) {
            controlURL = absolute_control_url;
        } else {
            JAMI_WARN("PUPnP: Error resolving absolute controlURL -> %s", UpnpGetErrorMessage(upnp_err));
        }
        std::free(absolute_control_url);

        // Get the relative eventSubURL and turn it into absolute address using the URLBase.
        std::string eventSubURL = getFirstElementItem(service_element, "eventSubURL");
        if (eventSubURL.empty()) {
            JAMI_WARN("PUPnP: IGD event sub URL is empty. Going to next node");
            continue;
        }

        char* absolute_event_sub_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), eventSubURL.c_str(), &absolute_event_sub_url);
        if (upnp_err == UPNP_E_SUCCESS) {
            eventSubURL = absolute_event_sub_url;
        } else {
            JAMI_WARN("PUPnP: Error resolving absolute eventSubURL -> %s", UpnpGetErrorMessage(upnp_err));
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
    if (not clientRegistered_) {
        return false;
    }

    // Action and response pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name { "GetStatusInfo" };

    action_container_ptr = UpnpMakeAction(action_name.c_str(), igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetStatusInfo action");
        return false;
    }
    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send GetStatusInfo action -> %s", UpnpGetErrorMessage(upnp_err));
        return false;
    }
    response.reset(response_container_ptr);

    if(errorOnResponse(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetStatusInfo from %s -> %d: %s", igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }

    // Parse response.
    std::string status = getFirstDocItem(response.get(), "NewConnectionStatus");
    if (status.compare("Connected") != 0) {
        return false;
    }

    return true;
}

IpAddr
PUPnP::actionGetExternalIP(const UPnPIGD& igd)
{
    if (not clientRegistered_) {
        return {};
    }

    // Action and response pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name { "GetExternalIPAddress" };

    action_container_ptr = UpnpMakeAction(action_name.c_str(), igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetExternalIPAddress action");
        return {};
    }
    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send GetExternalIPAddress action -> %s", UpnpGetErrorMessage(upnp_err));
        return {};
    }
    response.reset(response_container_ptr);

    if(errorOnResponse(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetExternalIPAddress from %s -> %d: %s", igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return {};
    }

    return { getFirstDocItem(response.get(), "NewExternalIPAddress") };
}

void
PUPnP::actionDeletePortMappingsByDesc(const UPnPIGD& igd, const std::string& description)
{
    if (not clientRegistered_) {
        return;
    }

    if (!igd.localIp_) {
        return;
    }

    // Set action name.
    std::string action_name { "GetGenericPortMappingEntry" };

    int entry_idx = 0;
    bool done = false;

    do {
        // Action and resposne pointers.
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
        IXML_Document* action_container_ptr = nullptr;
        IXML_Document* response_container_ptr = nullptr;

        UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewPortMappingIndex", std::to_string(entry_idx).c_str());
        action.reset(action_container_ptr);

        int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
        response.reset(response_container_ptr);
        if(not response and upnp_err != UPNP_E_SUCCESS) {
            return;
        }

        // Check error code.
        std::string errorCode = getFirstDocItem(response.get(), "errorCode");
        if (not errorCode.empty()) {

            if (std::stoi(errorCode) == ARRAY_IDX_INVALID or std::stoi(errorCode) == CONFLICT_IN_MAPPING) {
                // No more port mapping entries to delete.
                JAMI_DBG("PUPnP: Closed all local port mappings");
            } else {
                std::string errorDescription = getFirstDocItem(response.get(), "errorDescription");
                JAMI_DBG("PUPnP: GetGenericPortMappingEntry returned with error: %s: %s",
                        errorCode.c_str(), errorDescription.c_str());
            }
            done = true;

        } else {
            // Parse the rest of the response.
            std::string desc_actual = getFirstDocItem(response.get(), "NewPortMappingDescription");
            std::string client_ip = getFirstDocItem(response.get(), "NewInternalClient");

            // Check IP and description.
            if (IpAddr(client_ip) == igd.localIp_ and desc_actual.compare(description) == 0) {
                // Get parameters needed for port removal.
                std::string port_internal = getFirstDocItem(response.get(), "NewInternalPort");
                std::string port_external = getFirstDocItem(response.get(), "NewExternalPort");
                std::string protocol = getFirstDocItem(response.get(), "NewProtocol");

                // Attempt to delete entry.
                if (not actionDeletePortMapping(igd, port_external, protocol)) {
                    // Failed to delete entry, skip it and try the next one.
                    ++entry_idx;
                }
                // No need to increment index if successful since the number of entries will have decreased by one.
            } else {
                ++entry_idx;
            }
        }
    } while(not done);
}

bool
PUPnP::actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol)
{
    if (not clientRegistered_) {
        return false;
    }

    // Action and response pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name { "DeletePortMapping" };

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewRemoteHost", "");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewExternalPort", port_external.c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewProtocol", protocol.c_str());

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if(upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send %s from: %s, %d: %s", action_name.c_str(), igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }

    if (not response_container_ptr) {
        JAMI_WARN("PUPnP: Failed to get response from %s", action_name.c_str());
        return false;
    }
    response.reset(response_container_ptr);

    // Check if there is an error code.
    std::string errorCode = getFirstDocItem(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = getFirstDocItem(response.get(), "errorDescription");
        JAMI_WARN("PUPnP: %s returned with error: %s: %s", action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        return false;
    }

    JAMI_WARN("PUPnP: Closed port %s %s", port_external.c_str(), protocol.c_str());

    return true;
}

bool
PUPnP::actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, UPnPProtocol::UpnpError& error_code)
{
    if (not clientRegistered_) {
        return false;
    }

    error_code = UPnPProtocol::UpnpError::ERROR_OK;

    // Action and response pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name{"AddPortMapping"};

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewRemoteHost", "");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewExternalPort", mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewProtocol", mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewInternalPort", mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewInternalClient", igd.localIp_.toString().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewEnabled", "1");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewPortMappingDescription", mapping.getDescription().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewLeaseDuration", "0");

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if(upnp_err != UPNP_E_SUCCESS) {

        JAMI_WARN("PUPnP: Failed to send action %s from: %s, %d: %s", action_name.c_str(), igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        error_code = UPnPProtocol::UpnpError::INVALID_ERR;
        return false;
    }

    if (not response_container_ptr) {
        JAMI_WARN("PUPnP: Failed to get response from %s", action_name.c_str());
        return false;
    }
    response.reset(response_container_ptr);

    // Check if there is an error code.
    std::string errorCode = getFirstDocItem(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = getFirstDocItem(response.get(), "errorDescription");
        JAMI_WARN("PUPnP: %s returned with error: %s: %s", action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        error_code = UPnPProtocol::UpnpError::INVALID_ERR;
        return false;
    }
    return true;
}

bool
PUPnP::actionDeletePortMappingAsync(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol)
{
    if (not clientRegistered_) {
        return false;
    }

    // Action pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    IXML_Document* action_container_ptr = nullptr;

    // Set action name.
    std::string action_name { "DeletePortMapping" };

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewRemoteHost", "");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewExternalPort", port_external.c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewProtocol", protocol.c_str());

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendActionAsync(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), ctrlPtCallback, this);
    if(upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send async action %s from: %s, %d: %s", action_name.c_str(), igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }

    JAMI_DBG("PUPnP: Sent request to close port %s %s", port_external.c_str(), protocol.c_str());
    return true;
}

bool
PUPnP::actionAddPortMappingAsync(upnp::UPnPProtocol::Service id, const UPnPIGD& igd, const Mapping& mapping, UPnPProtocol::UpnpError& error_code)
{
    if (not clientRegistered_) {
        return false;
    }

    error_code = UPnPProtocol::UpnpError::ERROR_OK;
    std::string serviceIdStr = std::move(std::to_string((int)id));
    
    // Action pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    IXML_Document* action_container_ptr = nullptr;

    // Set action name.
    std::string action_name{"AddPortMapping"};

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewRemoteHost", "");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewExternalPort", mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewProtocol", mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewInternalPort", mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewInternalClient", igd.localIp_.toString().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewEnabled", "1");
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewPortMappingDescription", mapping.getDescription().c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewServiceRequest", serviceIdStr.c_str());
    UpnpAddToAction(&action_container_ptr, action_name.c_str(), igd.getServiceType().c_str(), "NewLeaseDuration", "0");

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendActionAsync(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), ctrlPtCallback, this);

    if(upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send async action %s from: %s, %d: %s", action_name.c_str(), igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        error_code = UPnPProtocol::UpnpError::INVALID_ERR;
        return false;
    }

    JAMI_DBG("PUPnP: Sent request to open port %s:%s %s", mapping.getPortInternalStr().c_str(), mapping.getPortExternalStr().c_str(), mapping.getTypeStr().c_str());
    return true;
}

}} // namespace jami::upnp