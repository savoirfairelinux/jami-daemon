/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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


// UPNP class implementation

PUPnP::PUPnP()
{
    if (not initUpnpLib())
        return;
    if (not registerClient())
        return;

    pupnpThread_ = std::thread([this] {
        while (pupnpRun_) {
            {
                std::unique_lock<std::mutex> lk(igdListMutex_);
                pupnpCv_.wait(lk, [this] {
                    return not clientRegistered_ or not pupnpRun_ or searchForIgd_
                           or not dwnldlXmlList_.empty();
                });
            }

            if (not pupnpRun_)
                break;

            if (clientRegistered_) {
                if (searchForIgd_.exchange(false)) {
                    searchForDevices();
                }

                std::unique_lock<std::mutex> lk(igdListMutex_);
                if (not dwnldlXmlList_.empty()) {
                    auto xmlList = std::move(dwnldlXmlList_);
                    decltype(xmlList) finished {};

                    // Wait on futures asynchronously
                    lk.unlock();
                    for (auto it = xmlList.begin(); it != xmlList.end();) {
                        if (it->wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
                            finished.splice(finished.end(), xmlList, it++);
                        } else {
                            ++it;
                        }
                    }
                    lk.lock();

                    // Move back timed-out items to list
                    dwnldlXmlList_.splice(dwnldlXmlList_.begin(), xmlList);
                    // Handle successful downloads
                    for (auto& item : finished) {
                        auto result = item.get();
                        if (not result->document or not validateIgd(*result)) {
                            discoveredIgdList_.erase(result->location);
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
    });
}

PUPnP::~PUPnP()
{
    JAMI_DBG("PUPnP: Destroying instance %p", this);
    // Clear all the lists.
    {
        std::lock_guard<std::mutex> lock(validIgdListMutex_);
        std::lock_guard<std::mutex> lk(ctrlptMutex_);
        for (auto const& it : validIgdList_) {
            removeAllLocalMappings(it);
        }
        validIgdList_.clear();
        clientRegistered_ = false;
        UpnpUnRegisterClient(ctrlptHandle_);
    }

    {
        std::lock_guard<std::mutex> lk2(igdListMutex_);
        discoveredIgdList_.clear();
        dwnldlXmlList_.clear();
        cancelXmlList_.clear();
        pupnpRun_ = false;
    }

    UpnpFinish();

    // Notify thread to terminate.
    pupnpCv_.notify_all();
    if (pupnpThread_.joinable())
        pupnpThread_.join();
}

bool
PUPnP::initUpnpLib()
{
    int upnp_err = UpnpInit2(nullptr, 0);

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
        pupnpRun_ = false;
        return false;
    }
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

    return true;
}

bool
PUPnP::registerClient()
{
    // Register Upnp control point.
    std::unique_lock<std::mutex> lk(ctrlptMutex_);
    int upnp_err = UpnpRegisterClient(ctrlPtCallback, this, &ctrlptHandle_);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't register client: %s", UpnpGetErrorMessage(upnp_err));
        pupnpRun_ = false;
        return false;
    } else {
        JAMI_DBG("PUPnP: Successfully registered client");
        clientRegistered_ = true;
    }

    return true;
}

void
PUPnP::setObserver(UpnpMappingObserver* obs)
{
    observer_ = obs;
}

void
PUPnP::searchForDevices()
{
    userLocalIp_ = ip_utils::getLocalAddr(pj_AF_INET());
    // Send out search for multiple types of devices, as some routers may possibly
    // only reply to one.
    std::unique_lock<std::mutex> lk(ctrlptMutex_);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
}

void
PUPnP::clearIgds()
{
    JAMI_DBG("PUPnP: clearing IGDs and devices lists");

    {
        std::lock_guard<std::mutex> lock(validIgdListMutex_);
        validIgdList_.clear();
    }

    {
        std::lock_guard<std::mutex> lk(igdListMutex_);

        // Clear all internal lists.
        cancelXmlList_.splice(cancelXmlList_.end(), dwnldlXmlList_);
        discoveredIgdList_.clear();
    }
}

void
PUPnP::searchForIgd()
{
    // Notify thread to execute in non-blocking fashion.
    searchForIgd_ = true;
    pupnpCv_.notify_one();
}

void
PUPnP::getIgdList(std::list<std::shared_ptr<IGD>>& igdList) const
{
    std::lock_guard<std::mutex> lock(validIgdListMutex_);
    for (auto& it : validIgdList_) {
        // Return only active IGDs.
        if (it->isValid()) {
            igdList.emplace_back(it);
        }
    }
}

bool
PUPnP::hasValidIgd() const
{
    for (auto& it : validIgdList_) {
        if (it->isValid()) {
            return true;
        }
    }
    return false;
}

void
PUPnP::incrementErrorsCounter(const std::shared_ptr<IGD>& igd)
{
    if (not igd->isValid())
        return;
    if (not igd->incrementErrorsCounter()) {
        // Disable this IGD.
        igd->setValid(false);
        // Notify the listener.
        observer_->onIgdUpdated(igd, UpnpIgdEvent::INVALID_STATE);
    }
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

    std::shared_ptr<UPnPIGD> igd_candidate = parseIgd(descDoc, info.location);
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
             igd_candidate->getUID().c_str(),
             igd_candidate->getFriendlyName().c_str(),
             igd_candidate->getServiceType().c_str(),
             igd_candidate->getServiceId().c_str(),
             igd_candidate->getBaseURL().c_str(),
             igd_candidate->getLocationURL().c_str(),
             igd_candidate->getControlURL().c_str(),
             igd_candidate->getEventSubURL().c_str());

    // Check if IGD is connected.
    if (not actionIsIgdConnected(*igd_candidate)) {
        JAMI_WARN("PUPnP: IGD candidate %s is not connected", igd_candidate->getUID().c_str());
        return false;
    }

    // Validate external Ip.
    igd_candidate->setPublicIp(actionGetExternalIP(*igd_candidate));
    if (igd_candidate->getPublicIp().toString().empty()) {
        JAMI_WARN("PUPnP: IGD candidate %s has no valid external Ip",
                  igd_candidate->getUID().c_str());
        return false;
    }

    // Validate internal Ip.
    if (igd_candidate->getBaseURL().empty()) {
        JAMI_WARN("PUPnP: IGD candidate %s has no valid internal Ip",
                  igd_candidate->getUID().c_str());
        return false;
    }

    // Typically the IGD local address should be extracted from the XML
    // document (e.g. parsing the base URL). For simplicity, we assume
    // that it matches the gateway as seen by the local interface.
    if (const auto& localGw = ip_utils::getLocalGateway()) {
        igd_candidate->setLocalIp(localGw);
    } else {
        JAMI_WARN("PUPnP: Could not set internal address for IGD candidate %s",
                  igd_candidate->getUID().c_str());
        return false;
    }

    // Store info for subscription.
    std::string eventSub = igd_candidate->getEventSubURL();

    {
        // Add the IGD if not already present in the list.
        std::lock_guard<std::mutex> lock(validIgdListMutex_);
        for (auto& igd : validIgdList_) {
            // Must not be a null pointer
            assert(igd.get() != nullptr);
            if (*igd == *igd_candidate) {
                JAMI_DBG("PUPnP: Device [%s] with int/ext addresses [%s:%s] is already in the list",
                         igd_candidate->getUID().c_str(),
                         igd_candidate->getLocalIp().toString().c_str(),
                         igd_candidate->getPublicIp().toString().c_str());
                return true;
            }
        }
    }

    JAMI_DBG("PUPnP: Found new valid IGD [%s] with int/ext addresses [%s:%s]",
             igd_candidate->getUID().c_str(),
             igd_candidate->getLocalIp().toString().c_str(),
             igd_candidate->getPublicIp().toString().c_str());

    {
        // This is a new IGD, move it the list.
        std::lock_guard<std::mutex> lock(validIgdListMutex_);
        validIgdList_.emplace_back(igd_candidate);
    }

    // Clear existing mappings
    removeAllLocalMappings(igd_candidate);

    // Report to the listener.
    observer_->onIgdUpdated(igd_candidate, UpnpIgdEvent::ADDED);

    // Subscribe to IGD events.
    int upnp_err = UpnpSubscribeAsync(ctrlptHandle_,
                                      eventSub.c_str(),
                                      SUBSCRIBE_TIMEOUT,
                                      subEventCallback,
                                      this);
    if (upnp_err != UPNP_E_SUCCESS)
        JAMI_WARN("PUPnP: Error when trying to request subscription for %s -> %s",
                  igd_candidate->getUID().c_str(),
                  UpnpGetErrorMessage(upnp_err));

    return true;
}

void
PUPnP::requestMappingAdd(const std::shared_ptr<IGD>& igd, const Mapping& mapping)
{
    if (auto pupnp_igd = std::dynamic_pointer_cast<const UPnPIGD>(igd)) {
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
                    mapping.getExternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewProtocol",
                    mapping.getTypeStr());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewInternalPort",
                    mapping.getInternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewInternalClient",
                    getUserLocalIp().toString().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewEnabled",
                    "1");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd.getServiceType().c_str(),
                    "NewPortMappingDescription",
                    mapping.toString().c_str());
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
PUPnP::processAddMapAction(const std::string& ctrlURL,
                           uint16_t ePort,
                           uint16_t iPort,
                           PortType portType)
{
    Mapping mapToAdd(ePort, iPort, portType);

    JAMI_DBG("PUPnP: Opened port %s", mapToAdd.toString().c_str());

    {
        std::lock_guard<std::mutex> lock(validIgdListMutex_);
        for (auto const& it : validIgdList_) {
            if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it)) {
                if (igd->getControlURL() == ctrlURL) {
                    mapToAdd.setExternalAddress(igd->getPublicIp().toString());
                    mapToAdd.setInternalAddress(getUserLocalIp().toString());
                    mapToAdd.setIgd(igd);
                    break;
                }
            }
        }
    }

    if (mapToAdd.getIgd()) {
        observer_->onMappingAdded(mapToAdd.getIgd(), std::move(mapToAdd));
    } else {
        JAMI_WARN("PUPnP: Did not find matching ctrl URL [%s] for (map %s)",
                  ctrlURL.c_str(),
                  mapToAdd.toString().c_str());
    }
}

void
PUPnP::processRemoveMapAction(const std::string& ctrlURL,
                              uint16_t ePort,
                              uint16_t iPort,
                              PortType portType)
{
    Mapping mapToRemove {ePort, iPort, portType};
    {
        std::lock_guard<std::mutex> lock(validIgdListMutex_);
        for (auto const& it : validIgdList_) {
            if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it)) {
                if (igd->getControlURL() == ctrlURL) {
                    mapToRemove.setIgd(igd);
                    break;
                }
            }
        }
    }

    if (mapToRemove.getIgd()) {
        observer_->onMappingRemoved(mapToRemove.getIgd(), std::move(mapToRemove));
    } else {
        JAMI_WARN("PUPnP: Did not find matching ctrl URL [%s] for (map %s)",
                  ctrlURL.c_str(),
                  mapToRemove.toString().c_str());
    }
}

void
PUPnP::requestMappingRemove(const Mapping& igdMapping)
{
    std::lock_guard<std::mutex> lock(validIgdListMutex_);
    // Iterate over all IGDs in internal list and try to remove selected mapping.
    for (auto const& it : validIgdList_) {
        if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it)) {
            JAMI_DBG("PUPnP: Attempting to close port %s %s",
                     igdMapping.getExternalPortStr().c_str(),
                     igdMapping.getTypeStr());
            std::lock_guard<std::mutex> lk2(ctrlptMutex_);
            actionDeletePortMappingAsync(*igd,
                                         igdMapping.getExternalPortStr(),
                                         igdMapping.getTypeStr());
            return;
        }
    }
}

void
PUPnP::removeAllLocalMappings(const std::shared_ptr<IGD>& igd)
{
    if (auto upnpIgd = std::dynamic_pointer_cast<UPnPIGD>(igd))
        actionDeletePortMappingsByDesc(*upnpIgd, Mapping::UPNP_MAPPING_DESCRIPTION_PREFIX);
}

const char*
PUPnP::eventTypeToString(Upnp_EventType eventType)
{
    switch (eventType) {
    case UPNP_CONTROL_ACTION_REQUEST:
        return "UPNP_CONTROL_ACTION_REQUEST";
    case UPNP_CONTROL_ACTION_COMPLETE:
        return "UPNP_CONTROL_ACTION_COMPLETE";
    case UPNP_CONTROL_GET_VAR_REQUEST:
        return "UPNP_CONTROL_GET_VAR_REQUEST";
    case UPNP_CONTROL_GET_VAR_COMPLETE:
        return "UPNP_CONTROL_GET_VAR_COMPLETE";
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        return "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE";
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        return "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE";
    case UPNP_DISCOVERY_SEARCH_RESULT:
        return "UPNP_DISCOVERY_SEARCH_RESULT";
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        return "UPNP_DISCOVERY_SEARCH_TIMEOUT";
    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        return "UPNP_EVENT_SUBSCRIPTION_REQUEST";
    case UPNP_EVENT_RECEIVED:
        return "UPNP_EVENT_RECEIVED";
    case UPNP_EVENT_RENEWAL_COMPLETE:
        return "UPNP_EVENT_RENEWAL_COMPLETE";
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        return "UPNP_EVENT_SUBSCRIBE_COMPLETE";
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
        return "UPNP_EVENT_UNSUBSCRIBE_COMPLETE";
    case UPNP_EVENT_AUTORENEWAL_FAILED:
        return "UPNP_EVENT_AUTORENEWAL_FAILED";
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
        return "UPNP_EVENT_SUBSCRIPTION_EXPIRED";
    default:
        return "Unknown UPNP Event";
    }
}

int
PUPnP::ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (auto pupnp = static_cast<PUPnP*>(user_data)) {
        std::lock_guard<std::mutex> lk(pupnp->ctrlptMutex_);
        return pupnp->handleCtrlPtUPnPEvents(event_type, event);
    }

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

void
PUPnP::processDiscoverySearchResult(const std::string& cpDeviceId,
                                    const std::string& igdLocationUrl,
                                    const IpAddr& dstAddr)
{
    dht::http::Url url(igdLocationUrl);

    std::lock_guard<std::mutex> lk(igdListMutex_);

    if (not discoveredIgdList_.emplace(cpDeviceId).second)
        return;

    JAMI_DBG("PUPnP: Added IGD %s [%s] to known IGD list",
             cpDeviceId.c_str(),
             IpAddr(url.host).toString(true, true).c_str());

    // NOTE: here, we check if the location given is related to the source address.
    // If it's not the case, it's certainly a router plugged in the network, but not
    // related to this network. So the given location will be unreachable and this
    // will cause some timeout.

    // Only check the IP address (ignore the port number).
    if (IpAddr(url.host).toString(false) != dstAddr.toString(false)) {
        JAMI_DBG("PUPnP: Returned location %s does not match the source address %s",
                 IpAddr(url.host).toString(true, true).c_str(),
                 dstAddr.toString(true, true).c_str());
        return;
    } else {
        JAMI_DBG("PUPnP: Returned IGD location %s", IpAddr(url.host).toString(true, true).c_str());
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
                          location.c_str(),
                          UpnpGetErrorMessage(upnp_err));
                return std::make_unique<IGDInfo>(
                    IGDInfo {std::move(location), XMLDocument(nullptr, ixmlDocument_free)});
            } else {
                JAMI_DBG("PUPnP: Succeeded to download device XML document from %s",
                         location.c_str());
                return std::make_unique<IGDInfo>(
                    IGDInfo {std::move(location), std::move(doc_desc_ptr)});
            }
        }));
}

void
PUPnP::processDiscoveryAdvertisementByebye(const std::string& cpDeviceId)
{
    // Remove device Id from list.
    {
        std::lock_guard<std::mutex> lk(igdListMutex_);
        discoveredIgdList_.erase(cpDeviceId);
    }

    std::shared_ptr<IGD> igd;
    {
        std::lock_guard<std::mutex> lk(validIgdListMutex_);
        for (auto it = validIgdList_.begin(); it != validIgdList_.end();) {
            if ((*it)->getUID() == cpDeviceId) {
                igd = *it;
                JAMI_DBG("PUPnP: Received [%s] for IGD [%s] %s. Will be removed.",
                         PUPnP::eventTypeToString(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE),
                         igd->getUID().c_str(),
                         igd->getLocalIp().toString().c_str());
                igd->setValid(false);
                // Remove the IGD.
                it = validIgdList_.erase(it);
                break;
            } else {
                it++;
            }
        }
    }

    // Notify the listener.
    if (igd) {
        observer_->onIgdUpdated(igd, UpnpIgdEvent::REMOVED);
    }
}

void
PUPnP::processDiscoverySubscriptionExpired(const std::string& eventSubUrl)
{
    std::lock_guard<std::mutex> lk(validIgdListMutex_);
    for (auto& it : validIgdList_) {
        if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it)) {
            if (igd->getEventSubURL() == eventSubUrl) {
                JAMI_DBG("PUPnP: Received [%s] event for IGD [%s] %s. Will be renewed.",
                         PUPnP::eventTypeToString(UPNP_EVENT_SUBSCRIPTION_EXPIRED),
                         igd->getUID().c_str(),
                         igd->getLocalIp().toString().c_str());
                std::lock_guard<std::mutex> lk1(ctrlptMutex_);
                UpnpSubscribeAsync(ctrlptHandle_,
                                   eventSubUrl.c_str(),
                                   SUBSCRIBE_TIMEOUT,
                                   subEventCallback,
                                   this);
                break;
            }
        }
    }
}

int
PUPnP::handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event)
{
    // Ignore if not registered
    if (not PUPnP::clientRegistered_)
        return UPNP_E_SUCCESS;

    switch (event_type) {
    // "ALIVE" events are processed as "SEARCH RESULT". It might be usefull
    // if "SEARCH RESULT" was missed.
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT: {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*) event;

        // First check the error code.
        auto upnp_status = UpnpDiscovery_get_ErrCode(d_event);
        if (upnp_status != UPNP_E_SUCCESS) {
            JAMI_ERR("PUPnP: UPNP discovery is in erroneous state: %s",
                     UpnpGetErrorMessage(upnp_status));
            break;
        }

        // Parse the event's data.
        std::string deviceId {UpnpDiscovery_get_DeviceID_cstr(d_event)};
        std::string location {UpnpDiscovery_get_Location_cstr(d_event)};
        IpAddr dstAddr(*(const pj_sockaddr*) (UpnpDiscovery_get_DestAddr(d_event)));
        runOnUpnpContextThread([w = weak(),
                                deviceId = std::move(deviceId),
                                location = std::move(location),
                                dstAddr = std::move(dstAddr)] {
            if (auto upnpThis = w.lock()) {
                upnpThis->processDiscoverySearchResult(deviceId, location, dstAddr);
            }
        });
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*) event;

        std::string deviceId(UpnpDiscovery_get_DeviceID_cstr(d_event));

        // Process the response on the main thread.
        runOnUpnpContextThread([w = weak(), deviceId = std::move(deviceId)] {
            if (auto upnpThis = w.lock()) {
                upnpThis->processDiscoveryAdvertisementByebye(deviceId);
            }
        });

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
        std::string publisherUrl(UpnpEventSubscribe_get_PublisherUrl_cstr(es_event));

        // Process the response on the main thread.
        runOnUpnpContextThread([w = weak(), publisherUrl = std::move(publisherUrl)] {
            if (auto upnpThis = w.lock()) {
                upnpThis->processDiscoverySubscriptionExpired(publisherUrl);
            }
        });
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
            auto actionRequest = UpnpActionComplete_get_ActionRequest(a_event);
            // Abort if there is no action to process.
            if (actionRequest == nullptr)
                break;
            auto upnpString = UpnpActionComplete_get_CtrlUrl(a_event);
            std::string ctrlUrl {UpnpString_get_String(upnpString),
                                 UpnpString_get_Length(upnpString)};

            char* xmlbuff = ixmlPrintNode((IXML_Node*) actionRequest);
            if (xmlbuff != nullptr) {
                auto ctrlAction = getAction(xmlbuff);
                ixmlFreeDOMString(xmlbuff);

                // Parse the response.
                std::string ePortStr(getFirstDocItem(actionRequest, "NewExternalPort"));
                std::string iPortStr(getFirstDocItem(actionRequest, "NewInternalPort"));
                std::string portTypeStr(getFirstDocItem(actionRequest, "NewProtocol"));

                uint16_t ePort = ePortStr.empty() ? 0 : std::stoi(ePortStr);
                uint16_t iPort = iPortStr.empty() ? 0 : std::stoi(iPortStr);
                PortType portType = portTypeStr == "UDP" ? upnp::PortType::UDP
                                                         : upnp::PortType::TCP;

                // Process the response on the main thread.
                runOnUpnpContextThread([w = weak(),
                                        ctrlAction = std::move(ctrlAction),
                                        ctrlUrl = std::move(ctrlUrl),
                                        ePort,
                                        iPort,
                                        portType] {
                    if (auto upnpThis = w.lock()) {
                        switch (ctrlAction) {
                        case CtrlAction::ADD_PORT_MAPPING:
                            upnpThis->processAddMapAction(ctrlUrl, ePort, iPort, portType);
                            break;
                        case CtrlAction::DELETE_PORT_MAPPING:
                            upnpThis->processRemoveMapAction(ctrlUrl, ePort, iPort, portType);
                            break;
                        default:
                            // All other control actions are ignored.
                            break;
                        }
                    }
                });
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
        std::lock_guard<std::mutex> lk(validIgdListMutex_);
        for (auto& it : validIgdList_) {
            if (it->getUID() == UDN) {
                // We already have this device in our list.
                return nullptr;
            }
        }
    }

    JAMI_DBG("PUPnP: Found new device [%s]", UDN.c_str());

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

std::map<Mapping::key_t, Mapping>
PUPnP::getMappingsListByDescr(const std::shared_ptr<IGD>& igd, const std::string& description) const
{
    auto upnpIgd = std::dynamic_pointer_cast<UPnPIGD>(igd);
    assert(upnpIgd);

    std::map<Mapping::key_t, Mapping> mapList;

    if (not(clientRegistered_ and upnpIgd->getLocalIp()))
        return mapList;

    // Set action name.
    static constexpr const char* action_name {"GetGenericPortMappingEntry"};

    for (int entry_idx = 0;; entry_idx++) {
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
            action(nullptr, ixmlDocument_free); // Action pointer.
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
            response(nullptr, ixmlDocument_free); // Response pointer.
        IXML_Document* action_container_ptr = nullptr;
        IXML_Document* response_container_ptr = nullptr;

        UpnpAddToAction(&action_container_ptr,
                        action_name,
                        upnpIgd->getServiceType().c_str(),
                        "NewPortMappingIndex",
                        std::to_string(entry_idx).c_str());
        action.reset(action_container_ptr);

        int upnp_err = UpnpSendAction(ctrlptHandle_,
                                      upnpIgd->getControlURL().c_str(),
                                      upnpIgd->getServiceType().c_str(),
                                      nullptr,
                                      action.get(),
                                      &response_container_ptr);
        response.reset(response_container_ptr);
        if (not response) {
            // No existing mapping. Abort silently.
            break;
        }

        if (upnp_err != UPNP_E_SUCCESS) {
            JAMI_ERR("PUPnP: GetGenericPortMappingEntry returned with error: %i", upnp_err);
            break;
        }

        // Check error code.
        std::string errorCode = getFirstDocItem(response.get(), "errorCode");
        if (not errorCode.empty()) {
            if (std::stoi(errorCode) == ARRAY_IDX_INVALID
                or std::stoi(errorCode) == CONFLICT_IN_MAPPING) {
                // No more port mapping entries in the response.
                JAMI_DBG("PUPnP: No more mappings (found a total of %i mappings", entry_idx);
                break;
            } else {
                std::string errorDescription = getFirstDocItem(response.get(), "errorDescription");
                JAMI_ERR("PUPnP: GetGenericPortMappingEntry returned with error: %s: %s",
                         errorCode.c_str(),
                         errorDescription.c_str());
                break;
            }
        }

        // Parse the response.
        std::string desc_actual = getFirstDocItem(response.get(), "NewPortMappingDescription");
        std::string client_ip = getFirstDocItem(response.get(), "NewInternalClient");

        if (client_ip != getUserLocalIp().toString()) {
            // Silently ignore un-matching addresses.
            continue;
        }

        if (desc_actual.find(description) == std::string::npos)
            continue;

        const std::string& port_internal = getFirstDocItem(response.get(), "NewInternalPort");
        const std::string& port_external = getFirstDocItem(response.get(), "NewExternalPort");
        std::string transport = getFirstDocItem(response.get(), "NewProtocol");

        if (port_internal.empty() || port_external.empty() || transport.empty()) {
            JAMI_ERR("PUPnP: GetGenericPortMappingEntry returned an invalid entry at index %i",
                     entry_idx);
            continue;
        }

        std::transform(transport.begin(), transport.end(), transport.begin(), ::toupper);
        uint16_t ePort = static_cast<uint16_t>(std::stoi(port_external));
        uint16_t iPort = static_cast<uint16_t>(std::stoi(port_internal));
        PortType type = transport.find("TCP") != std::string::npos ? PortType::TCP : PortType::UDP;

        auto map = Mapping(ePort, iPort, type);
        map.setIgd(igd);

        mapList.emplace(map.getMapKey(), std::move(map));
    }

    JAMI_DBG("PUPnP: Found %lu allocated mappings on IGD %s",
             mapList.size(),
             upnpIgd->getLocalIp().toString().c_str());

    return mapList;
}

// TODO. Rewrite using getMappingsListByDescr.
void
PUPnP::actionDeletePortMappingsByDesc(const UPnPIGD& igd, const std::string& description)
{
    if (not(clientRegistered_ and igd.getLocalIp()))
        return;

    // Set action name.
    static constexpr const char* action_name {"GetGenericPortMappingEntry"};
    int entry_idx = 0;
    bool done = false;

    do {
        // Action and response pointers.
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
            JAMI_ERR("PUPnP: GetGenericPortMappingEntry did not return a response");
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
            if (client_ip == getUserLocalIp().toString()
                and desc_actual.find(description) != std::string::npos) {
                // Get parameters needed for port removal.
                std::string port_internal = getFirstDocItem(response.get(), "NewInternalPort");
                std::string port_external = getFirstDocItem(response.get(), "NewExternalPort");
                std::string protocol = getFirstDocItem(response.get(), "NewProtocol");

                JAMI_DBG("PUPnP: Try remove mapping from previous JAMI instances: [%s] on %s",
                         desc_actual.c_str(),
                         igd.getLocalIp().toString().c_str());

                // Attempt to delete entry.
                if (not actionDeletePortMapping(igd, port_external, protocol)) {
                    // Failed to delete entry, skip it and try the next one.
                    JAMI_WARN("PUPnP: Failed to remove mapping [%s] on %s",
                              desc_actual.c_str(),
                              igd.getLocalIp().toString().c_str());
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
    int upnp_err = UpnpSendActionAsync(ctrlptHandle_,
                                       igd.getControlURL().c_str(),
                                       igd.getServiceType().c_str(),
                                       nullptr,
                                       action.get(),
                                       ctrlPtCallback,
                                       this);

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send %s request from: %s, %d: %s",
                  action_name.c_str(),
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        return false;
    }

    JAMI_DBG("PUPnP: Successfully sent %s request from %s",
             action_name.c_str(),
             igd.getServiceType().c_str());

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
                    mapping.getExternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewProtocol",
                    mapping.getTypeStr());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewInternalPort",
                    mapping.getInternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewInternalClient",
                    getUserLocalIp().toString().c_str());
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewEnabled",
                    "1");
    UpnpAddToAction(&action_container_ptr,
                    action_name.c_str(),
                    igd.getServiceType().c_str(),
                    "NewPortMappingDescription",
                    mapping.toString().c_str());
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
