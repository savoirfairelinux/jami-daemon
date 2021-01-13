/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <opendht/thread_pool.h>
#include <opendht/http.h>

namespace jami {
namespace upnp {

// Action identifiers.
constexpr static const char* ACTION_ADD_PORT_MAPPING {"AddPortMapping"};
constexpr static const char* ACTION_DELETE_PORT_MAPPING {"DeletePortMapping"};
constexpr static const char* ACTION_GET_GENERIC_PORT_MAPPING_ENTRY {"GetGenericPortMappingEntry"};
constexpr static const char* ACTION_GET_STATUS_INFO {"GetStatusInfo"};
constexpr static const char* ACTION_GET_EXTERNAL_IP_ADDRESS {"GetExternalIPAddress"};

// Helper functions for xml parsing.
static std::string
getElementText(IXML_Node* node)
{
    std::string ret;
    if (node) {
        IXML_Node* textNode = ixmlNode_getFirstChild(node);
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
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&>
        nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
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
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&>
        nodeList(ixmlElement_getElementsByTagName(element, item), ixmlNodeList_free);
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
        JAMI_WARN("PUPnP: Response contains error: %s : %s",
                  errorCode.c_str(),
                  errorDescription.c_str());
        return true;
    }
    return false;
}

PUPnP::PUPnP()
{
    int upnp_err = UpnpInit2(nullptr, 0);

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
        pupnpRun_ = false;
        return;
    } else {
        char* ip_address = UpnpGetServerIpAddress();
        char* ip_address6 = nullptr;
        unsigned short port = UpnpGetServerPort();
        unsigned short port6 = 0;
#if UPNP_ENABLE_IPV6
        ip_address6 = UpnpGetServerIp6Address();
        port6 = UpnpGetServerPort6();
#endif
        if (ip_address6 and port6)
            JAMI_DBG("PUPnP: Initialized on %s:%u | %s:%u", ip_address, port, ip_address6, port6);
        else
            JAMI_DBG("PUPnP: Initialized on %s:%u", ip_address, port);

        // Relax the parser to allow malformed XML text.
        ixmlRelaxParser(1);
    }

    pupnpThread_ = std::thread([this] {
        std::unique_lock<std::mutex> lk(ctrlptMutex_);
        while (pupnpRun_) {
            pupnpCv_.wait(lk, [this] {
                std::lock_guard<std::mutex> lk(validIgdMutex_);
                return not clientRegistered_ or not pupnpRun_ or searchForIgd_
                       or not dwnldlXmlList_.empty();
            });

            if (not clientRegistered_) {
                // Register Upnp control point.
                int upnp_err = UpnpRegisterClient(ctrlPtCallback, this, &ctrlptHandle_);
                if (upnp_err != UPNP_E_SUCCESS) {
                    JAMI_ERR("PUPnP: Can't register client: %s", UpnpGetErrorMessage(upnp_err));
                    pupnpRun_ = false;
                    break;
                } else {
                    JAMI_DBG("PUPnP: Successfully registered client");
                    clientRegistered_ = true;
                }
            }

            if (not pupnpRun_)
                break;

            if (clientRegistered_) {
                if (searchForIgd_.exchange(false)) {
                    // Send out search for multiple types of devices, as some routers may possibly
                    // only reply to one.
                    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
                    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
                    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
                    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
                }

                std::unique_lock<std::mutex> lk2(validIgdMutex_);
                if (not dwnldlXmlList_.empty()) {
                    auto xmlList = std::move(dwnldlXmlList_);
                    decltype(xmlList) finished {};

                    // Wait on futures asynchronously
                    lk2.unlock();
                    lk.unlock();
                    for (auto it = xmlList.begin(); it != xmlList.end();) {
                        if (it->wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
                            finished.splice(finished.end(), xmlList, it++);
                        } else {
                            ++it;
                        }
                    }
                    lk.lock();
                    lk2.lock();

                    // Move back timed-out items to list
                    dwnldlXmlList_.splice(dwnldlXmlList_.begin(), xmlList);
                    // Handle successful downloads
                    for (auto& item : finished) {
                        auto result = item.get();
                        if (not result->document or not validateIgd(*result)) {
                            cpDeviceList_.erase(result->location);
                        }
                    }
                }
                for (auto it = cancelXmlList_.begin(); it != cancelXmlList_.end();) {
                    if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        it = cancelXmlList_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        UpnpFinish();
    });
}

PUPnP::~PUPnP()
{
    // Clear all the lists.
    {
        std::lock_guard<std::mutex> lk(ctrlptMutex_);
        std::lock_guard<std::mutex> lk2(validIgdMutex_);
        for (auto const& it : validIgdList_) {
            if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it.second))
                actionDeletePortMappingsByDesc(*igd, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
        }
        validIgdList_.clear();
        cpDeviceList_.clear();
        dwnldlXmlList_.clear();
        cancelXmlList_.clear();
        pupnpRun_ = false;
    }

    // Notify thread to terminate. UpnpFinish function will get called.
    pupnpCv_.notify_all();
    if (pupnpThread_.joinable())
        pupnpThread_.join();
}

void
PUPnP::clearIgds()
{
    // Lock internal IGD list.
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    // Clear all internal lists.
    cancelXmlList_.splice(cancelXmlList_.end(), dwnldlXmlList_);
    validIgdList_.clear();
    cpDeviceList_.clear();
}

void
PUPnP::searchForIgd()
{
    // Notify thread to execute in non-blocking fashion.
    searchForIgd_ = true;
    pupnpCv_.notify_one();
}

bool
PUPnP::validateIgd(const IGDInfo& info)
{
    auto descDoc = info.document.get();
    // Check device type.
    std::string deviceType = getFirstDocItem(descDoc, "deviceType");
    if (deviceType.empty()) {
        // No device type.
        return false;
    }

    if (deviceType.compare(UPNP_IGD_DEVICE) != 0) {
        // Device type not IGD.
        return false;
    }

    auto igd_candidate = parseIgd(descDoc, info.location);
    if (not igd_candidate) {
        // No valid IGD candidate.
        return false;
    }

    JAMI_DBG("PUPnP: Validating IGD candidate\n"
             "    UDN          : %s\n"
             "    Name         : %s\n"
             "    Service Type : %s\n"
             "    Service ID   : %s\n"
             "    Base URL     : %s\n"
             "    Location URL : %s\n"
             "    control URL  : %s\n"
             "    Event URL    : %s",
             igd_candidate->getUDN().c_str(),
             igd_candidate->getFriendlyName().c_str(),
             igd_candidate->getServiceType().c_str(),
             igd_candidate->getServiceId().c_str(),
             igd_candidate->getBaseURL().c_str(),
             igd_candidate->getLocationURL().c_str(),
             igd_candidate->getControlURL().c_str(),
             igd_candidate->getEventSubURL().c_str());

    // Check if IGD is connected.
    if (not actionIsIgdConnected(*igd_candidate)) {
        JAMI_WARN("PUPnP: IGD candidate %s is not connected", igd_candidate->getUDN().c_str());
        return false;
    }

    // Validate external Ip.
    igd_candidate->publicIp_ = actionGetExternalIP(*igd_candidate);
    if (igd_candidate->publicIp_.toString().empty()) {
        JAMI_WARN("PUPnP: IGD candidate %s has no valid external Ip",
                  igd_candidate->getUDN().c_str());
        return false;
    }

    // Validate internal Ip.
    igd_candidate->localIp_ = ip_utils::getLocalAddr(pj_AF_INET());
    if (igd_candidate->localIp_.toString().empty()) {
        JAMI_WARN("PUPnP: No valid internal Ip.");
        return false;
    }

    JAMI_DBG("PUPnP: Found device with external IP %s", igd_candidate->publicIp_.toString().c_str());

    // Store info for subscription.
    std::string eventSub = igd_candidate->getEventSubURL();
    std::string udn = igd_candidate->getUDN();

    // Remove any local mappings that may be left over from last time used.
    removeAllLocalMappings(igd_candidate.get());

    // Add the igd to the upnp context class list.
    if (updateIgdListCb_(this,
                         std::move(igd_candidate.get()),
                         std::move(igd_candidate.get()->publicIp_),
                         true)) {

        JAMI_DBG("PUPnP: IGD with public IP %s was added to the list",
            igd_candidate->publicIp_.toString().c_str());
    } else {
        JAMI_DBG("PUPnP: IGD with public IP %s is already in the list",
            igd_candidate->publicIp_.toString().c_str());
    }

    // Keep local IGD list internally.
    validIgdList_.emplace(igd_candidate->getUDN(), std::move(igd_candidate));

    // Subscribe to IGD events.
    int upnp_err = UpnpSubscribeAsync(ctrlptHandle_,
                                      eventSub.c_str(),
                                      SUBSCRIBE_TIMEOUT,
                                      subEventCallback,
                                      this);
    if (upnp_err != UPNP_E_SUCCESS)
        JAMI_WARN("PUPnP: Error when trying to request subscription for %s -> %s",
                  udn.c_str(),
                  UpnpGetErrorMessage(upnp_err));

    return true;
}

void
PUPnP::requestMappingAdd(IGD* igd, const Mapping& mapping)
{
    if (auto pupnp_igd = dynamic_cast<const UPnPIGD*>(igd)) {
        JAMI_DBG("PUPnP: Attempting to open port %s", mapping.toString().c_str());
        actionAddPortMappingAsync(*pupnp_igd, mapping);
    }
}

bool
PUPnP::actionAddPortMappingAsync(const UPnPIGD& igd, const Mapping& mapping)
{
    if (not clientRegistered_) {
        return false;
    }
    XMLDocument action(nullptr, ixmlDocument_free); // Action pointer.
    IXML_Document* action_container_ptr = nullptr;
    // Set action sequence.
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewRemoteHost",
                    "");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewExternalPort",
                    mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewProtocol",
                    mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewInternalPort",
                    mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewInternalClient",
                    igd.localIp_.toString().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewEnabled",
                    "1");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewPortMappingDescription",
                    mapping.getDescription().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewLeaseDuration",
                    "0");
    action.reset(action_container_ptr);
    int upnp_err = UpnpSendActionAsync(ctrlptHandle_,
                                       igd.getControlURL().c_str(),
                                       igd.getServiceType().c_str(),
                                       nullptr,
                                       action.get(),
                                       ctrlPtCallback,
                                       this);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send async action %s from: %s, %d: %s",
                  ACTION_ADD_PORT_MAPPING,
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        return false;
    }
    JAMI_DBG("PUPnP: Sent request to open port %s", mapping.toString().c_str());
    return true;
}

void
PUPnP::processAddMapAction(const std::string_view& ctrlURL, IXML_Document* actionRequest)
{
    std::string portExternal(getFirstDocItem(actionRequest, "NewExternalPort"));
    std::string portInternal(getFirstDocItem(actionRequest, "NewInternalPort"));
    std::string protocol(getFirstDocItem(actionRequest, "NewProtocol"));
    std::unique_lock<std::mutex> lk(validIgdMutex_);
    if (portExternal.empty() or portInternal.empty() or protocol.empty()) {
        for (auto const& it : validIgdList_) {
            if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it.second)) {
                if (igd->getControlURL() == ctrlURL) {
                    lk.unlock();
                    notifyContextPortOpenCb_(igd->publicIp_, Mapping(0, 0), false);
                    return;
                }
            }
        }
    }
    Mapping mapToAdd(std::stoi(portExternal),
                     std::stoi(portInternal),
                     protocol == "UDP" ? upnp::PortType::UDP : upnp::PortType::TCP);
    JAMI_DBG("PUPnP: Opened port %s", mapToAdd.toString().c_str());
    for (auto const& it : validIgdList_) {
        if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it.second)) {
            if (igd->getControlURL() == ctrlURL) {
                lk.unlock();
                notifyContextPortOpenCb_(igd->publicIp_, std::move(mapToAdd), true);
                return;
            }
        }
    }
}

void
PUPnP::processRemoveMapAction(const std::string_view& ctrlURL, IXML_Document* actionRequest)
{
    std::string portExternal(getFirstDocItem(actionRequest, "NewExternalPort"));
    std::string protocol(getFirstDocItem(actionRequest, "NewProtocol"));
    std::unique_lock<std::mutex> lk(validIgdMutex_);
    if (portExternal.empty() or protocol.empty()) {
        for (auto const& it : validIgdList_) {
            if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it.second)) {
                if (igd->getControlURL() == ctrlURL) {
                    lk.unlock();
                    notifyContextPortCloseCb_(igd->publicIp_, Mapping(0, 0), false);
                    return;
                }
            }
        }
    }
    JAMI_WARN("PUPnP: Closed port %s %s", portExternal.c_str(), protocol.c_str());
    for (auto const& it : validIgdList_) {
        if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it.second)) {
            if (igd->getControlURL() == ctrlURL) {
                Mapping mapToRemove = igd->getMapping(std::stoi(portExternal),
                                                      protocol == "UDP" ? upnp::PortType::UDP
                                                                        : upnp::PortType::TCP);
                lk.unlock();
                notifyContextPortCloseCb_(igd->publicIp_, std::move(mapToRemove), true);
                return;
            }
        }
    }
}

void
PUPnP::requestMappingRemove(const Mapping& igdMapping)
{
    std::lock_guard<std::mutex> lk1(validIgdMutex_);
    // Iterate over all IGDs in internal list and try to remove selected mapping.
    for (auto const& item : validIgdList_) {
        if (not item.second)
            continue;
        if (auto upnp_igd = dynamic_cast<UPnPIGD*>(item.second.get())) {
            JAMI_DBG("PUPnP: Attempting to close port %s %s",
                     igdMapping.getPortExternalStr().c_str(),
                     igdMapping.getTypeStr().c_str());
            std::lock_guard<std::mutex> lk2(ctrlptMutex_);
            actionDeletePortMappingAsync(*upnp_igd,
                                         igdMapping.getPortExternalStr(),
                                         igdMapping.getTypeStr());
            return;
        }
    }
}

void
PUPnP::removeAllLocalMappings(IGD* igd)
{
    if (auto igd_del_map = dynamic_cast<UPnPIGD*>(igd))
        actionDeletePortMappingsByDesc(*igd_del_map, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
}

const char*
PUPnP::eventTypeToString(Upnp_EventType eventType)
{
    switch(eventType){
        case UPNP_CONTROL_ACTION_REQUEST: return "UPNP_CONTROL_ACTION_REQUEST";
        case UPNP_CONTROL_ACTION_COMPLETE: return "UPNP_CONTROL_ACTION_COMPLETE";
        case UPNP_CONTROL_GET_VAR_REQUEST: return "UPNP_CONTROL_GET_VAR_REQUEST";
        case UPNP_CONTROL_GET_VAR_COMPLETE: return "UPNP_CONTROL_GET_VAR_COMPLETE";
        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE: return "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE";
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: return "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE";
        case UPNP_DISCOVERY_SEARCH_RESULT: return "UPNP_DISCOVERY_SEARCH_RESULT";
        case UPNP_DISCOVERY_SEARCH_TIMEOUT: return "UPNP_DISCOVERY_SEARCH_TIMEOUT";
        case UPNP_EVENT_SUBSCRIPTION_REQUEST: return "UPNP_EVENT_SUBSCRIPTION_REQUEST";
        case UPNP_EVENT_RECEIVED: return "UPNP_EVENT_RECEIVED";
        case UPNP_EVENT_RENEWAL_COMPLETE: return "UPNP_EVENT_RENEWAL_COMPLETE";
        case UPNP_EVENT_SUBSCRIBE_COMPLETE: return "UPNP_EVENT_SUBSCRIBE_COMPLETE";
        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: return "UPNP_EVENT_UNSUBSCRIBE_COMPLETE";
        case UPNP_EVENT_AUTORENEWAL_FAILED: return "UPNP_EVENT_AUTORENEWAL_FAILED";
        case UPNP_EVENT_SUBSCRIPTION_EXPIRED: return "UPNP_EVENT_SUBSCRIPTION_EXPIRED";
        default : return "Unknown UPNP Event !";
    }
}

int
PUPnP::ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    // Filter out less relevant logs.
    if (event_type != UPNP_DISCOVERY_ADVERTISEMENT_ALIVE)
        JAMI_DBG("PUPnP: Control point callback event_type %s", PUPnP::eventTypeToString(event_type));

    if (auto pupnp = static_cast<PUPnP*>(user_data))
        return pupnp->handleCtrlPtUPnPEvents(event_type, event);

    JAMI_WARN("PUPnP: Control point callback without PUPnP");
    return 0;
}

PUPnP::CtrlAction
PUPnP::getAction(const char* xmlNode)
{
    if (strstr(xmlNode, ACTION_ADD_PORT_MAPPING)) {
        return CtrlAction::ADD_PORT_MAPPING;
    } else if (strstr(xmlNode, ACTION_DELETE_PORT_MAPPING)) {
        return CtrlAction::DELETE_PORT_MAPPING;
    } else if (strstr(xmlNode, ACTION_GET_GENERIC_PORT_MAPPING_ENTRY)) {
        return CtrlAction::GET_GENERIC_PORT_MAPPING_ENTRY;
    } else if (strstr(xmlNode, ACTION_GET_STATUS_INFO)) {
        return CtrlAction::GET_STATUS_INFO;
    } else if (strstr(xmlNode, ACTION_GET_EXTERNAL_IP_ADDRESS)) {
        return CtrlAction::GET_EXTERNAL_IP_ADDRESS;
    } else {
        return CtrlAction::UNKNOWN;
    }
}

inline std::string_view
viewFromUpnpString(const UpnpString* ustr) {
    return {UpnpString_get_String(ustr), UpnpString_get_Length(ustr)};
}

int
PUPnP::handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event)
{
    switch (event_type) {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE: // Fall through. Treat advertisements like discovery
                                             // search results.
    case UPNP_DISCOVERY_SEARCH_RESULT: {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*) event;

        // First check the error code.
        auto upnp_status = UpnpDiscovery_get_ErrCode(d_event);
        if (upnp_status != UPNP_E_SUCCESS) {
            JAMI_ERR("PUPnP: UPNP discovery is in erroneous state: %s", UpnpGetErrorMessage(upnp_status));
            break;
        }

        // Check if this device ID is already in the list.
        std::string cpDeviceId = UpnpDiscovery_get_DeviceID_cstr(d_event);
        std::lock_guard<std::mutex> lk(validIgdMutex_);
        if (not cpDeviceList_.emplace(cpDeviceId).second)
            break;

        IpAddr ipSrc(*(const pj_sockaddr*) (UpnpDiscovery_get_DestAddr(d_event)));

        // Check if we already downloaded the xml doc based on the igd location string.
        std::string igdLocationUrl {UpnpDiscovery_get_Location_cstr(d_event)};
        dht::http::Url url(igdLocationUrl);

        // NOTE: here, we check if the location given is related to the source address.
        // If it's not the case, it's certainly a router plugged in the network, but not
        // related to this network. So the given location will be unreachable and this
        // will cause some timeout.

        // Only check the IP address (ignore the port number).
        if (IpAddr(url.host).toString(false) != ipSrc.toString(false)) {
            JAMI_WARN("PUPnP: Returned location %s does not match the source address %s",
                IpAddr(url.host).toString(true, true).c_str(), ipSrc.toString(true, true).c_str());

            break;
        } else {
            JAMI_WARN("PUPnP: Returned IGD location %s", IpAddr(url.host).toString(true, true).c_str());
        }


        dwnldlXmlList_.emplace_back(
            dht::ThreadPool::io().get<pIGDInfo>([this, location = std::move(igdLocationUrl)] {
                IXML_Document* doc_container_ptr = nullptr;
                XMLDocument doc_desc_ptr(nullptr, ixmlDocument_free);
                int upnp_err = UpnpDownloadXmlDoc(location.c_str(), &doc_container_ptr);
                if (doc_container_ptr)
                    doc_desc_ptr.reset(doc_container_ptr);
                pupnpCv_.notify_all();
                if (upnp_err != UPNP_E_SUCCESS or not doc_desc_ptr) {
                    JAMI_WARN("PUPnP: Error downloading device XML document from %s -> %s",
                        location.c_str(), UpnpGetErrorMessage(upnp_err));
                    return std::make_unique<IGDInfo>(
                        IGDInfo {std::move(location), XMLDocument(nullptr, ixmlDocument_free)});
                }
                else {
                    JAMI_WARN("PUPnP: Succeeded to download device XML document from %s",
                        location.c_str());
                    return std::make_unique<IGDInfo>(
                        IGDInfo {std::move(location), std::move(doc_desc_ptr)});
                }
            }));

        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*) event;

        // Remvoe device Id from list.
        std::string cpDeviceId(UpnpDiscovery_get_DeviceID_cstr(d_event));
        std::lock_guard<std::mutex> lk(validIgdMutex_);
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
    case UPNP_DISCOVERY_SEARCH_TIMEOUT: {
        // Nothing to do here.
        break;
    }
    case UPNP_EVENT_RECEIVED: {
        // TODO: Handle event by updating any changed state variables */
        break;
    }
    case UPNP_EVENT_AUTORENEWAL_FAILED:   // Fall through. Treat failed autorenewal like an expired
                                          // subscription.
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED: // This event will occur only if autorenewal is disabled.
    {
        const UpnpEventSubscribe* es_event = (const UpnpEventSubscribe*) event;
        std::string eventSubUrl(UpnpEventSubscribe_get_PublisherUrl_cstr(es_event));
        std::lock_guard<std::mutex> lk1(ctrlptMutex_);
        std::lock_guard<std::mutex> lk(validIgdMutex_);
        auto it = validIgdList_.begin();
        for (; it != validIgdList_.end(); it++) {
            if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it->second))
                if (igd->getEventSubURL() == eventSubUrl) {
                    UpnpSubscribeAsync(ctrlptHandle_,
                                       eventSubUrl.c_str(),
                                       SUBSCRIBE_TIMEOUT,
                                       subEventCallback,
                                       this);
                    break;
                }
        }

        break;
    }
    case UPNP_EVENT_SUBSCRIBE_COMPLETE: {
        break;
    }
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: {
        break;
    }
    case UPNP_CONTROL_ACTION_COMPLETE: {
        const UpnpActionComplete* a_event = (const UpnpActionComplete*) event;
        if (UpnpActionComplete_get_ErrCode(a_event) == UPNP_E_SUCCESS) {
            if (IXML_Document* actionRequest = UpnpActionComplete_get_ActionRequest(a_event)) {
                auto ctrlURL = viewFromUpnpString(UpnpActionComplete_get_CtrlUrl(a_event));
                if (const char* xmlbuff = ixmlPrintNode((IXML_Node*) actionRequest)) {
                    switch (getAction(xmlbuff)) {
                    case CtrlAction::ADD_PORT_MAPPING:
                        processAddMapAction(ctrlURL, actionRequest);
                        break;
                    case CtrlAction::DELETE_PORT_MAPPING:
                        processRemoveMapAction(ctrlURL, actionRequest);
                        break;
                    case CtrlAction::GET_GENERIC_PORT_MAPPING_ENTRY:
                        break;
                    case CtrlAction::GET_STATUS_INFO:
                        break;
                    case CtrlAction::GET_EXTERNAL_IP_ADDRESS:
                        break;
                    case CtrlAction::UNKNOWN:
                    default:
                        break;
                    }
                }
            }
        }
        break;
    }
    default: {
        JAMI_WARN("PUPnP: Unhandled Control Point event");
        break;
    }
    }

    return UPNP_E_SUCCESS;
}

int
PUPnP::subEventCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (auto pupnp = static_cast<PUPnP*>(user_data))
        return pupnp->handleSubscriptionUPnPEvent(event_type, event);
    JAMI_WARN("PUPnP: Subscription callback without service Id string");
    return 0;
}

int
PUPnP::handleSubscriptionUPnPEvent(Upnp_EventType /*event_type */, const void* event)
{
    const UpnpEventSubscribe* es_event = (const UpnpEventSubscribe*) event;
    int upnp_err = UpnpEventSubscribe_get_ErrCode(es_event);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Error when trying to handle subscription callback -> %s",
                  UpnpGetErrorMessage(upnp_err));
        return upnp_err;
    }

    // TODO: Handle subscription event.

    return UPNP_E_SUCCESS;
}

std::unique_ptr<UPnPIGD>
PUPnP::parseIgd(IXML_Document* doc, std::string locationUrl)
{
    if (not(doc and locationUrl.c_str()))
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
    if (baseURL.empty())
        baseURL = locationUrl;

    // Get list of services defined by serviceType.
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> serviceList(nullptr,
                                                                             ixmlNodeList_free);
    serviceList.reset(ixmlDocument_getElementsByTagName(doc, "serviceType"));
    unsigned long list_length = ixmlNodeList_length(serviceList.get());

    // Go through the "serviceType" nodes until we find the the correct service type.
    for (unsigned long node_idx = 0; node_idx < list_length; node_idx++) {
        IXML_Node* serviceType_node = ixmlNodeList_item(serviceList.get(), node_idx);
        std::string serviceType = getElementText(serviceType_node);

        // Only check serviceType of WANIPConnection or WANPPPConnection.
        if (serviceType != std::string(UPNP_WANIP_SERVICE)
            && serviceType != std::string(UPNP_WANPPP_SERVICE)) {
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
        if (strcmp(ixmlNode_getNodeName(service_node), "service") != 0) {
            // IGD "serviceType" parent node is not called "service". Going to next node.
            continue;
        }

        // Get serviceId.
        IXML_Element* service_element = (IXML_Element*) service_node;
        std::string serviceId = getFirstElementItem(service_element, "serviceId");
        if (serviceId.empty()) {
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
        if (upnp_err == UPNP_E_SUCCESS)
            controlURL = absolute_control_url;
        else
            JAMI_WARN("PUPnP: Error resolving absolute controlURL -> %s",
                      UpnpGetErrorMessage(upnp_err));

        std::free(absolute_control_url);

        // Get the relative eventSubURL and turn it into absolute address using the URLBase.
        std::string eventSubURL = getFirstElementItem(service_element, "eventSubURL");
        if (eventSubURL.empty()) {
            JAMI_WARN("PUPnP: IGD event sub URL is empty. Going to next node");
            continue;
        }

        char* absolute_event_sub_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), eventSubURL.c_str(), &absolute_event_sub_url);
        if (upnp_err == UPNP_E_SUCCESS)
            eventSubURL = absolute_event_sub_url;
        else
            JAMI_WARN("PUPnP: Error resolving absolute eventSubURL -> %s",
                      UpnpGetErrorMessage(upnp_err));

        std::free(absolute_event_sub_url);

        new_igd.reset(new UPnPIGD(std::move(UDN),
                                  std::move(baseURL),
                                  std::move(friendlyName),
                                  std::move(serviceType),
                                  std::move(serviceId),
                                  std::move(locationUrl),
                                  std::move(controlURL),
                                  std::move(eventSubURL)));

        return new_igd;
    }

    return nullptr;
}

bool
PUPnP::actionIsIgdConnected(const UPnPIGD& igd)
{
    if (not clientRegistered_)
        return false;

    // Action and response pointers.
    XMLDocument action(nullptr, ixmlDocument_free);   // Action pointer.
    XMLDocument response(nullptr, ixmlDocument_free); // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name {"GetStatusInfo"};

    action_container_ptr = UpnpMakeAction(action_name.c_str(),
                                          igd.getServiceType().c_str(),
                                          0,
                                          nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetStatusInfo action");
        return false;
    }
    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send GetStatusInfo action -> %s", UpnpGetErrorMessage(upnp_err));
        return false;
    }
    response.reset(response_container_ptr);

    if (errorOnResponse(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetStatusInfo from %s -> %d: %s",
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        return false;
    }

    // Parse response.
    std::string status = getFirstDocItem(response.get(), "NewConnectionStatus");
    if (status.compare("Connected") != 0)
        return false;

    return true;
}

IpAddr
PUPnP::actionGetExternalIP(const UPnPIGD& igd)
{
    if (not clientRegistered_)
        return {};

    // Action and response pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
        action(nullptr, ixmlDocument_free); // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
        response(nullptr, ixmlDocument_free); // Response pointer.

    // Set action name.
    static constexpr const char* action_name {"GetExternalIPAddress"};

    IXML_Document* action_container_ptr = nullptr;
    action_container_ptr = UpnpMakeAction(action_name, igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetExternalIPAddress action");
        return {};
    }
    action.reset(action_container_ptr);

    IXML_Document* response_container_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send GetExternalIPAddress action -> %s",
                  UpnpGetErrorMessage(upnp_err));
        return {};
    }
    response.reset(response_container_ptr);

    if (errorOnResponse(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetExternalIPAddress from %s -> %d: %s",
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        return {};
    }

    return {getFirstDocItem(response.get(), "NewExternalIPAddress")};
}

void
PUPnP::actionDeletePortMappingsByDesc(const UPnPIGD& igd, const std::string& description)
{
    if (not(clientRegistered_ and igd.localIp_))
        return;

    // Set action name.
    static constexpr const char* action_name {"GetGenericPortMappingEntry"};

    int entry_idx = 0;
    bool done = false;

    do {
        // Action and resposne pointers.
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
            action(nullptr, ixmlDocument_free); // Action pointer.
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
            response(nullptr, ixmlDocument_free); // Response pointer.
        IXML_Document* action_container_ptr = nullptr;
        IXML_Document* response_container_ptr = nullptr;

        UpnpAddToAction(&action_container_ptr,
                        action_name,
                        igd.getServiceType().c_str(),
                        "NewPortMappingIndex",
                        std::to_string(entry_idx).c_str());
        action.reset(action_container_ptr);

        int upnp_err = UpnpSendAction(ctrlptHandle_,
                                      igd.getControlURL().c_str(),
                                      igd.getServiceType().c_str(),
                                      nullptr,
                                      action.get(),
                                      &response_container_ptr);
        response.reset(response_container_ptr);
        if (not response and upnp_err != UPNP_E_SUCCESS) {
            return;
        }

        // Check error code.
        std::string errorCode = getFirstDocItem(response.get(), "errorCode");
        if (not errorCode.empty()) {
            if (std::stoi(errorCode) == ARRAY_IDX_INVALID
                or std::stoi(errorCode) == CONFLICT_IN_MAPPING) {
                // No more port mapping entries to delete.
                JAMI_DBG("PUPnP: Closed all local port mappings");
            } else {
                std::string errorDescription = getFirstDocItem(response.get(), "errorDescription");
                JAMI_DBG("PUPnP: GetGenericPortMappingEntry returned with error: %s: %s",
                         errorCode.c_str(),
                         errorDescription.c_str());
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
                // No need to increment index if successful since the number of entries will have
                // decreased by one.
            } else {
                ++entry_idx;
            }
        }
    } while (not done);
}

bool
PUPnP::actionDeletePortMapping(const UPnPIGD& igd,
                               const std::string& port_external,
                               const std::string& protocol)
{
    if (not clientRegistered_)
        return false;

    // Action and response pointers.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
        action(nullptr, ixmlDocument_free); // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
        response(nullptr, ixmlDocument_free); // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name {"DeletePortMapping"};

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewRemoteHost",
                    "");
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewExternalPort",
                    port_external.c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewProtocol",
                    protocol.c_str());

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send %s from: %s, %d: %s",
                  action_name.c_str(),
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
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
        JAMI_WARN("PUPnP: %s returned with error: %s: %s",
                  action_name.c_str(),
                  errorCode.c_str(),
                  errorDescription.c_str());
        return false;
    }

    JAMI_WARN("PUPnP: Closed port %s %s", port_external.c_str(), protocol.c_str());

    return true;
}

bool
PUPnP::actionAddPortMapping(const UPnPIGD& igd,
                            const Mapping& mapping,
                            UPnPProtocol::UpnpError& error_code)
{
    if (not clientRegistered_)
        return false;

    error_code = UPnPProtocol::UpnpError::ERROR_OK;

    // Action and response pointers.
    XMLDocument action(nullptr, ixmlDocument_free);   // Action pointer.
    XMLDocument response(nullptr, ixmlDocument_free); // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    // Set action name.
    std::string action_name {"AddPortMapping"};

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewRemoteHost",
                    "");
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewExternalPort",
                    mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewProtocol",
                    mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewInternalPort",
                    mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewInternalClient",
                    igd.localIp_.toString().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewEnabled",
                    "1");
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewPortMappingDescription",
                    mapping.getDescription().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewLeaseDuration",
                    "0");

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send action %s from: %s, %d: %s",
                  action_name.c_str(),
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
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
        JAMI_WARN("PUPnP: %s returned with error: %s: %s",
                  action_name.c_str(),
                  errorCode.c_str(),
                  errorDescription.c_str());
        error_code = UPnPProtocol::UpnpError::INVALID_ERR;
        return false;
    }
    return true;
}

bool
PUPnP::actionDeletePortMappingAsync(const UPnPIGD& igd,
                                    const std::string& port_external,
                                    const std::string& protocol)
{
    if (not clientRegistered_) {
        return false;
    }
    XMLDocument action(nullptr, ixmlDocument_free); // Action pointer.
    IXML_Document* action_container_ptr = nullptr;
    // Set action sequence.
    UpnpAddToAction(&action_container_ptr,
                    ACTION_DELETE_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewRemoteHost",
                    "");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_DELETE_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewExternalPort",
                    port_external.c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_DELETE_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewProtocol",
                    protocol.c_str());
    action.reset(action_container_ptr);
    int upnp_err = UpnpSendActionAsync(ctrlptHandle_,
                                       igd.getControlURL().c_str(),
                                       igd.getServiceType().c_str(),
                                       nullptr,
                                       action.get(),
                                       ctrlPtCallback,
                                       this);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send async action %s from: %s, %d: %s",
                  ACTION_DELETE_PORT_MAPPING,
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        return false;
    }
    JAMI_DBG("PUPnP: Sent request to close port %s %s", port_external.c_str(), protocol.c_str());
    return true;
}

} // namespace upnp
} // namespace jami
