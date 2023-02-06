/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

// Error codes returned by router when trying to remove ports.
constexpr static int ARRAY_IDX_INVALID = 713;
constexpr static int CONFLICT_IN_MAPPING = 718;

// Max number of IGD search attempts before failure.
constexpr static unsigned int PUPNP_MAX_RESTART_SEARCH_RETRIES {3};
// IGD search timeout (in seconds).
constexpr static unsigned int SEARCH_TIMEOUT {60};
// Base unit for the timeout between two successive IGD search.
constexpr static auto PUPNP_SEARCH_RETRY_UNIT {std::chrono::seconds(10)};

// Helper functions for xml parsing.
static std::string_view
getElementText(IXML_Node* node)
{
    if (node) {
        if (IXML_Node* textNode = ixmlNode_getFirstChild(node))
            if (const char* value = ixmlNode_getNodeValue(textNode))
                return std::string_view(value);
    }
    return {};
}

static std::string_view
getFirstDocItem(IXML_Document* doc, const char* item)
{
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&>
        nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
    if (nodeList) {
        // If there are several nodes which match the tag, we only want the first one.
        return getElementText(ixmlNodeList_item(nodeList.get(), 0));
    }
    return {};
}

static std::string_view
getFirstElementItem(IXML_Element* element, const char* item)
{
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&>
        nodeList(ixmlElement_getElementsByTagName(element, item), ixmlNodeList_free);
    if (nodeList) {
        // If there are several nodes which match the tag, we only want the first one.
        return getElementText(ixmlNodeList_item(nodeList.get(), 0));
    }
    return {};
}

static bool
errorOnResponse(IXML_Document* doc)
{
    if (not doc)
        return true;

    auto errorCode = getFirstDocItem(doc, "errorCode");
    if (not errorCode.empty()) {
        auto errorDescription = getFirstDocItem(doc, "errorDescription");
        JAMI_WARNING("PUPnP: Response contains error: {:s}: {:s}",
                  errorCode,
                  errorDescription);
        return true;
    }
    return false;
}

// UPNP class implementation

PUPnP::PUPnP()
{
    JAMI_DBG("PUPnP: Creating instance [%p] ...", this);
    runOnPUPnPQueue([this] {
        threadId_ = getCurrentThread();
        JAMI_DBG("PUPnP: Instance [%p] created", this);
    });
}

PUPnP::~PUPnP()
{
    JAMI_DBG("PUPnP: Instance [%p] destroyed", this);
}

void
PUPnP::initUpnpLib()
{
    assert(not initialized_);

    int upnp_err = UpnpInit2(nullptr, 0);

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
        initialized_ = false;
        return;
    }

    // Disable embedded WebServer if any.
    if (UpnpIsWebserverEnabled() == 1) {
        JAMI_WARN("PUPnP: Web-server is enabled. Disabling");
        UpnpEnableWebserver(0);
        if (UpnpIsWebserverEnabled() == 1) {
            JAMI_ERR("PUPnP: Could not disable Web-server!");
        } else {
            JAMI_DBG("PUPnP: Web-server successfully disabled");
        }
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

    initialized_ = true;
}

bool
PUPnP::isRunning() const
{
    std::unique_lock<std::mutex> lk(pupnpMutex_);
    return not shutdownComplete_;
}

void
PUPnP::registerClient()
{
    assert(not clientRegistered_);

    CHECK_VALID_THREAD();

    // Register Upnp control point.
    int upnp_err = UpnpRegisterClient(ctrlPtCallback, this, &ctrlptHandle_);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("PUPnP: Can't register client: %s", UpnpGetErrorMessage(upnp_err));
    } else {
        JAMI_DBG("PUPnP: Successfully registered client");
        clientRegistered_ = true;
    }
}

void
PUPnP::setObserver(UpnpMappingObserver* obs)
{
    if (not isValidThread()) {
        runOnPUPnPQueue([w = weak(), obs] {
            if (auto upnpThis = w.lock()) {
                upnpThis->setObserver(obs);
            }
        });
        return;
    }

    JAMI_DBG("PUPnP: Setting observer to %p", obs);

    observer_ = obs;
}

const IpAddr
PUPnP::getHostAddress() const
{
    std::lock_guard<std::mutex> lock(pupnpMutex_);
    return hostAddress_;
}

void
PUPnP::terminate(std::condition_variable& cv)
{
    JAMI_DBG("PUPnP: Terminate instance %p", this);

    clientRegistered_ = false;
    observer_ = nullptr;

    UpnpUnRegisterClient(ctrlptHandle_);

    if (initialized_) {
        if (UpnpFinish() != UPNP_E_SUCCESS) {
            JAMI_ERR("PUPnP: Failed to properly close lib-upnp");
        }

        initialized_ = false;
    }

    // Clear all the lists.
    discoveredIgdList_.clear();

    {
        std::lock_guard<std::mutex> lock(pupnpMutex_);
        validIgdList_.clear();
        shutdownComplete_ = true;
        cv.notify_one();
    }
}

void
PUPnP::terminate()
{
    std::unique_lock<std::mutex> lk(pupnpMutex_);
    std::condition_variable cv {};

    runOnPUPnPQueue([w = weak(), &cv = cv] {
            if (auto upnpThis = w.lock()) {
                upnpThis->terminate(cv);
            }
    });

    if (cv.wait_for(lk, std::chrono::seconds(10), [this] { return shutdownComplete_; })) {
        JAMI_DBG("PUPnP: Shutdown completed");
    } else {
        JAMI_ERR("PUPnP: Shutdown timed-out");
        // Force stop if the shutdown take too much time.
        shutdownComplete_ = true;
    }
}

void
PUPnP::searchForDevices()
{
    CHECK_VALID_THREAD();

    JAMI_DBG("PUPnP: Send IGD search request");

    // Send out search for multiple types of devices, as some routers may possibly
    // only reply to one.

    auto err = UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
    if (err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Send search for UPNP_ROOT_DEVICE failed. Error %d: %s",
                  err,
                  UpnpGetErrorMessage(err));
    }

    err = UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
    if (err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Send search for UPNP_IGD_DEVICE failed. Error %d: %s",
                  err,
                  UpnpGetErrorMessage(err));
    }

    err = UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
    if (err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Send search for UPNP_WANIP_SERVICE failed. Error %d: %s",
                  err,
                  UpnpGetErrorMessage(err));
    }

    err = UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
    if (err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Send search for UPNP_WANPPP_SERVICE failed. Error %d: %s",
                  err,
                  UpnpGetErrorMessage(err));
    }
}

void
PUPnP::clearIgds()
{
    if (not isValidThread()) {
        runOnPUPnPQueue([w = weak()] {
            if (auto upnpThis = w.lock()) {
                upnpThis->clearIgds();
            }
        });
        return;
    }

    JAMI_DBG("PUPnP: clearing IGDs and devices lists");

    if (searchForIgdTimer_)
        searchForIgdTimer_->cancel();

    igdSearchCounter_ = 0;

    {
        std::lock_guard<std::mutex> lock(pupnpMutex_);
        for (auto const& igd : validIgdList_) {
            igd->setValid(false);
        }
        validIgdList_.clear();
        hostAddress_ = {};
    }

    discoveredIgdList_.clear();
}

void
PUPnP::searchForIgd()
{
    if (not isValidThread()) {
        runOnPUPnPQueue([w = weak()] {
            if (auto upnpThis = w.lock()) {
                upnpThis->searchForIgd();
            }
        });
        return;
    }

    // Update local address before searching.
    updateHostAddress();

    if (isReady()) {
        JAMI_DBG("PUPnP: Already have a valid IGD. Skip the search request");
        return;
    }

    if (igdSearchCounter_++ >= PUPNP_MAX_RESTART_SEARCH_RETRIES) {
        JAMI_WARN("PUPnP: Setup failed after %u trials. PUPnP will be disabled!",
                  PUPNP_MAX_RESTART_SEARCH_RETRIES);
        return;
    }

    JAMI_DBG("PUPnP: Start search for IGD: attempt %u", igdSearchCounter_);

    // Do not init if the host is not valid. Otherwise, the init will fail
    // anyway and may put libupnp in an unstable state (mainly deadlocks)
    // even if the UpnpFinish() method is called.
    if (not hasValidHostAddress()) {
        JAMI_WARN("PUPnP: Host address is invalid. Skipping the IGD search");
    } else {
        // Init and register if needed
        if (not initialized_) {
            initUpnpLib();
        }
        if (initialized_ and not clientRegistered_) {
            registerClient();
        }
        // Start searching
        if (clientRegistered_) {
            assert(initialized_);
            searchForDevices();
        } else {
            JAMI_WARN("PUPnP: PUPNP not fully setup. Skipping the IGD search");
        }
    }

    // Cancel the current timer (if any) and re-schedule.
    // The connectivity change may be received while the the local
    // interface is not fully setup. The rescheduling typically
    // usefull to mitigate this race.
    if (searchForIgdTimer_)
        searchForIgdTimer_->cancel();

    searchForIgdTimer_ = getUpnContextScheduler()->scheduleIn(
        [w = weak()] {
            if (auto upnpThis = w.lock())
                upnpThis->searchForIgd();
        },
        PUPNP_SEARCH_RETRY_UNIT * igdSearchCounter_);
}

std::list<std::shared_ptr<IGD>>
PUPnP::getIgdList() const
{
    std::lock_guard<std::mutex> lock(pupnpMutex_);
    std::list<std::shared_ptr<IGD>> igdList;
    for (auto& it : validIgdList_) {
        // Return only active IGDs.
        if (it->isValid()) {
            igdList.emplace_back(it);
        }
    }
    return igdList;
}

bool
PUPnP::isReady() const
{
    // Must at least have a valid local address.
    if (not getHostAddress() or getHostAddress().isLoopback())
        return false;

    return hasValidIgd();
}

bool
PUPnP::hasValidIgd() const
{
    std::lock_guard<std::mutex> lock(pupnpMutex_);
    for (auto& it : validIgdList_) {
        if (it->isValid()) {
            return true;
        }
    }
    return false;
}

void
PUPnP::updateHostAddress()
{
    std::lock_guard<std::mutex> lock(pupnpMutex_);
    hostAddress_ = ip_utils::getLocalAddr(AF_INET);
}

bool
PUPnP::hasValidHostAddress()
{
    std::lock_guard<std::mutex> lock(pupnpMutex_);
    return hostAddress_ and not hostAddress_.isLoopback();
}

void
PUPnP::incrementErrorsCounter(const std::shared_ptr<IGD>& igd)
{
    if (not igd or not igd->isValid())
        return;
    if (not igd->incrementErrorsCounter()) {
        // Disable this IGD.
        igd->setValid(false);
        // Notify the listener.
        if (observer_)
            observer_->onIgdUpdated(igd, UpnpIgdEvent::INVALID_STATE);
    }
}

bool
PUPnP::validateIgd(const std::string& location, IXML_Document* doc_container_ptr)
{
    CHECK_VALID_THREAD();

    assert(doc_container_ptr != nullptr);

    XMLDocument document(doc_container_ptr, ixmlDocument_free);
    auto descDoc = document.get();
    // Check device type.
    auto deviceType = getFirstDocItem(descDoc, "deviceType");
    if (deviceType != UPNP_IGD_DEVICE) {
        // Device type not IGD.
        return false;
    }

    std::shared_ptr<UPnPIGD> igd_candidate = parseIgd(descDoc, location);
    if (not igd_candidate) {
        // No valid IGD candidate.
        return false;
    }

    JAMI_DBG("PUPnP: Validating the IGD candidate [UDN: %s]\n"
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
        std::lock_guard<std::mutex> lock(pupnpMutex_);
        for (auto& igd : validIgdList_) {
            // Must not be a null pointer
            assert(igd.get() != nullptr);
            if (*igd == *igd_candidate) {
                JAMI_DBG("PUPnP: Device [%s] with int/ext addresses [%s:%s] is already in the list "
                         "of valid IGDs",
                         igd_candidate->getUID().c_str(),
                         igd_candidate->toString().c_str(),
                         igd_candidate->getPublicIp().toString().c_str());
                return true;
            }
        }
    }

    // We have a valid IGD
    igd_candidate->setValid(true);

    JAMI_DBG("PUPnP: Added a new IGD [%s] to the list of valid IGDs",
             igd_candidate->getUID().c_str());

    JAMI_DBG("PUPnP: New IGD addresses [int: %s - ext: %s]",
             igd_candidate->toString().c_str(),
             igd_candidate->getPublicIp().toString().c_str());

    // Subscribe to IGD events.
    int upnp_err = UpnpSubscribeAsync(ctrlptHandle_,
                                      eventSub.c_str(),
                                      UPNP_INFINITE,
                                      subEventCallback,
                                      this);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send subscribe request to %s: error %i - %s",
                  igd_candidate->getUID().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        // return false;
    } else {
        JAMI_DBG("PUPnP: Successfully subscribed to IGD %s", igd_candidate->getUID().c_str());
    }

    {
        // This is a new (and hopefully valid) IGD.
        std::lock_guard<std::mutex> lock(pupnpMutex_);
        validIgdList_.emplace_back(igd_candidate);
    }

    // Report to the listener.
    runOnUpnpContextQueue([w = weak(), igd_candidate] {
        if (auto upnpThis = w.lock()) {
            if (upnpThis->observer_)
                upnpThis->observer_->onIgdUpdated(igd_candidate, UpnpIgdEvent::ADDED);
        }
    });

    return true;
}

void
PUPnP::requestMappingAdd(const Mapping& mapping)
{
    runOnPUPnPQueue([w = weak(), mapping] {
        if (auto upnpThis = w.lock()) {
            if (not upnpThis->isRunning())
                return;
            Mapping mapRes(mapping);
            if (upnpThis->actionAddPortMapping(mapRes)) {
                mapRes.setState(MappingState::OPEN);
                mapRes.setInternalAddress(upnpThis->getHostAddress().toString());
                upnpThis->processAddMapAction(mapRes);
            } else {
                upnpThis->incrementErrorsCounter(mapRes.getIgd());
                mapRes.setState(MappingState::FAILED);
                upnpThis->processRequestMappingFailure(mapRes);
            }
        }
    });
}

void
PUPnP::requestMappingRemove(const Mapping& mapping)
{
    // Send remove request using the matching IGD
    runOnPUPnPQueue([w = weak(), mapping] {
        if (auto upnpThis = w.lock()) {
            // Abort if we are shutting down.
            if (not upnpThis->isRunning())
                return;
            if (upnpThis->actionDeletePortMapping(mapping)) {
                upnpThis->processRemoveMapAction(mapping);
            } else {
                assert(mapping.getIgd());
                // Dont need to report in case of failure.
                upnpThis->incrementErrorsCounter(mapping.getIgd());
            }
        }
    });
}

std::shared_ptr<UPnPIGD>
PUPnP::findMatchingIgd(const std::string& ctrlURL) const
{
    std::lock_guard<std::mutex> lock(pupnpMutex_);

    auto iter = std::find_if(validIgdList_.begin(),
                             validIgdList_.end(),
                             [&ctrlURL](const std::shared_ptr<IGD>& igd) {
                                 if (auto upnpIgd = std::dynamic_pointer_cast<UPnPIGD>(igd)) {
                                     return upnpIgd->getControlURL() == ctrlURL;
                                 }
                                 return false;
                             });

    if (iter == validIgdList_.end()) {
        JAMI_WARN("PUPnP: Did not find the IGD matching ctrl URL [%s]", ctrlURL.c_str());
        return {};
    }

    return std::dynamic_pointer_cast<UPnPIGD>(*iter);
}

void
PUPnP::processAddMapAction(const Mapping& map)
{
    CHECK_VALID_THREAD();

    if (observer_ == nullptr)
        return;

    runOnUpnpContextQueue([w = weak(), map] {
        if (auto upnpThis = w.lock()) {
            if (upnpThis->observer_)
                upnpThis->observer_->onMappingAdded(map.getIgd(), std::move(map));
        }
    });
}

void
PUPnP::processRequestMappingFailure(const Mapping& map)
{
    CHECK_VALID_THREAD();

    if (observer_ == nullptr)
        return;

    runOnUpnpContextQueue([w = weak(), map] {
        if (auto upnpThis = w.lock()) {
            JAMI_DBG("PUPnP: Failed to request mapping %s", map.toString().c_str());
            if (upnpThis->observer_)
                upnpThis->observer_->onMappingRequestFailed(map);
        }
    });
}

void
PUPnP::processRemoveMapAction(const Mapping& map)
{
    CHECK_VALID_THREAD();

    if (observer_ == nullptr)
        return;

    runOnUpnpContextQueue([map, obs = observer_] {
        JAMI_DBG("PUPnP: Closed mapping %s", map.toString().c_str());
        obs->onMappingRemoved(map.getIgd(), std::move(map));
    });
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
    auto pupnp = static_cast<PUPnP*>(user_data);

    if (pupnp == nullptr) {
        JAMI_WARN("PUPnP: Control point callback without PUPnP");
        return UPNP_E_SUCCESS;
    }

    auto upnpThis = pupnp->weak().lock();

    if (not upnpThis)
        return UPNP_E_SUCCESS;

    // Ignore if already unregistered.
    if (not upnpThis->clientRegistered_)
        return UPNP_E_SUCCESS;

    // Process the callback.
    return upnpThis->handleCtrlPtUPnPEvents(event_type, event);
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
    CHECK_VALID_THREAD();

    // Update host address if needed.
    if (not hasValidHostAddress())
        updateHostAddress();

    // The host address must be valid to proceed.
    if (not hasValidHostAddress()) {
        JAMI_WARN("PUPnP: Local address is invalid. Ignore search result for now!");
        return;
    }

    // Use the device ID and the URL as ID. This is necessary as some
    // IGDs may have the same device ID but different URLs.

    auto igdId = cpDeviceId + " url: " + igdLocationUrl;

    if (not discoveredIgdList_.emplace(igdId).second) {
        // JAMI_WARN("PUPnP: IGD [%s] already in the list", igdId.c_str());
        return;
    }

    JAMI_DBG("PUPnP: Discovered a new IGD [%s]", igdId.c_str());

    // NOTE: here, we check if the location given is related to the source address.
    // If it's not the case, it's certainly a router plugged in the network, but not
    // related to this network. So the given location will be unreachable and this
    // will cause some timeout.

    // Only check the IP address (ignore the port number).
    dht::http::Url url(igdLocationUrl);
    if (IpAddr(url.host).toString(false) != dstAddr.toString(false)) {
        JAMI_DBG("PUPnP: Returned location %s does not match the source address %s",
                 IpAddr(url.host).toString(true, true).c_str(),
                 dstAddr.toString(true, true).c_str());
        return;
    }

    // Run a separate thread to prevent blocking this thread
    // if the IGD HTTP server is not responsive.
    dht::ThreadPool::io().run([w = weak(), igdLocationUrl] {
        if (auto upnpThis = w.lock()) {
            upnpThis->downLoadIgdDescription(igdLocationUrl);
        }
    });
}

void
PUPnP::downLoadIgdDescription(const std::string& locationUrl)
{
    IXML_Document* doc_container_ptr = nullptr;
    int upnp_err = UpnpDownloadXmlDoc(locationUrl.c_str(), &doc_container_ptr);

    if (upnp_err != UPNP_E_SUCCESS or not doc_container_ptr) {
        JAMI_WARN("PUPnP: Error downloading device XML document from %s -> %s",
                  locationUrl.c_str(),
                  UpnpGetErrorMessage(upnp_err));
    } else {
        JAMI_DBG("PUPnP: Succeeded to download device XML document from %s", locationUrl.c_str());
        runOnPUPnPQueue([w = weak(), url = locationUrl, doc_container_ptr] {
            if (auto upnpThis = w.lock()) {
                upnpThis->validateIgd(url, doc_container_ptr);
            }
        });
    }
}

void
PUPnP::processDiscoveryAdvertisementByebye(const std::string& cpDeviceId)
{
    CHECK_VALID_THREAD();

    discoveredIgdList_.erase(cpDeviceId);

    std::shared_ptr<IGD> igd;
    {
        std::lock_guard<std::mutex> lk(pupnpMutex_);
        for (auto it = validIgdList_.begin(); it != validIgdList_.end();) {
            if ((*it)->getUID() == cpDeviceId) {
                igd = *it;
                JAMI_DBG("PUPnP: Received [%s] for IGD [%s] %s. Will be removed.",
                         PUPnP::eventTypeToString(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE),
                         igd->getUID().c_str(),
                         igd->toString().c_str());
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
    if (observer_ and igd) {
        observer_->onIgdUpdated(igd, UpnpIgdEvent::REMOVED);
    }
}

void
PUPnP::processDiscoverySubscriptionExpired(Upnp_EventType event_type, const std::string& eventSubUrl)
{
    CHECK_VALID_THREAD();

    std::lock_guard<std::mutex> lk(pupnpMutex_);
    for (auto& it : validIgdList_) {
        if (auto igd = std::dynamic_pointer_cast<UPnPIGD>(it)) {
            if (igd->getEventSubURL() == eventSubUrl) {
                JAMI_DBG("PUPnP: Received [%s] event for IGD [%s] %s. Request a new subscribe.",
                         PUPnP::eventTypeToString(event_type),
                         igd->getUID().c_str(),
                         igd->toString().c_str());
                UpnpSubscribeAsync(ctrlptHandle_,
                                   eventSubUrl.c_str(),
                                   UPNP_INFINITE,
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
        runOnPUPnPQueue([w = weak(),
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
        runOnPUPnPQueue([w = weak(), deviceId = std::move(deviceId)] {
            if (auto upnpThis = w.lock()) {
                upnpThis->processDiscoveryAdvertisementByebye(deviceId);
            }
        });
        break;
    }
    case UPNP_DISCOVERY_SEARCH_TIMEOUT: {
        // Even if the discovery search is successful, it's normal to receive
        // time-out events. This because we send search requests using various
        // device types, which some of them may not return a response.
        break;
    }
    case UPNP_EVENT_RECEIVED: {
        // Nothing to do.
        break;
    }
    // Treat failed autorenewal like an expired subscription.
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED: // This event will occur only if autorenewal is disabled.
    {
        JAMI_WARN("PUPnP: Received Subscription Event %s", eventTypeToString(event_type));
        const UpnpEventSubscribe* es_event = (const UpnpEventSubscribe*) event;
        if (es_event == nullptr) {
            JAMI_WARN("PUPnP: Received Subscription Event with null pointer");
            break;
        }
        std::string publisherUrl(UpnpEventSubscribe_get_PublisherUrl_cstr(es_event));

        // Process the response on the main thread.
        runOnPUPnPQueue([w = weak(), event_type, publisherUrl = std::move(publisherUrl)] {
            if (auto upnpThis = w.lock()) {
                upnpThis->processDiscoverySubscriptionExpired(event_type, publisherUrl);
            }
        });
        break;
    }
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: {
        UpnpEventSubscribe* es_event = (UpnpEventSubscribe*) event;
        if (es_event == nullptr) {
            JAMI_WARN("PUPnP: Received Subscription Event with null pointer");
        } else {
            UpnpEventSubscribe_delete(es_event);
        }
        break;
    }
    case UPNP_CONTROL_ACTION_COMPLETE: {
        const UpnpActionComplete* a_event = (const UpnpActionComplete*) event;
        if (a_event == nullptr) {
            JAMI_WARN("PUPnP: Received Action Complete Event with null pointer");
            break;
        }
        auto res = UpnpActionComplete_get_ErrCode(a_event);
        if (res != UPNP_E_SUCCESS and res != UPNP_E_TIMEDOUT) {
            auto err = UpnpActionComplete_get_ErrCode(a_event);
            JAMI_WARN("PUPnP: Received Action Complete error %i %s", err, UpnpGetErrorMessage(err));
        } else {
            auto actionRequest = UpnpActionComplete_get_ActionRequest(a_event);
            // Abort if there is no action to process.
            if (actionRequest == nullptr) {
                JAMI_WARN("PUPnP: Can't get the Action Request data from the event");
                break;
            }

            auto actionResult = UpnpActionComplete_get_ActionResult(a_event);
            if (actionResult != nullptr) {
                ixmlDocument_free(actionResult);
            } else {
                JAMI_WARN("PUPnP: Action Result document not found");
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
PUPnP::handleSubscriptionUPnPEvent(Upnp_EventType, const void* event)
{
    UpnpEventSubscribe* es_event = static_cast<UpnpEventSubscribe*>(const_cast<void*>(event));

    if (es_event == nullptr) {
        JAMI_ERR("PUPnP: Unexpected null pointer!");
        return UPNP_E_INVALID_ARGUMENT;
    }
    std::string publisherUrl(UpnpEventSubscribe_get_PublisherUrl_cstr(es_event));
    int upnp_err = UpnpEventSubscribe_get_ErrCode(es_event);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Subscription error %s from %s",
                  UpnpGetErrorMessage(upnp_err),
                  publisherUrl.c_str());
        return upnp_err;
    }

    return UPNP_E_SUCCESS;
}

std::unique_ptr<UPnPIGD>
PUPnP::parseIgd(IXML_Document* doc, std::string locationUrl)
{
    if (not(doc and locationUrl.c_str()))
        return nullptr;

    // Check the UDN to see if its already in our device list.
    std::string UDN(getFirstDocItem(doc, "UDN"));
    if (UDN.empty()) {
        JAMI_WARN("PUPnP: could not find UDN in description document of device");
        return nullptr;
    } else {
        std::lock_guard<std::mutex> lk(pupnpMutex_);
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
    std::string friendlyName(getFirstDocItem(doc, "friendlyName"));

    // Get base URL.
    std::string baseURL(getFirstDocItem(doc, "URLBase"));
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
        std::string serviceType(getElementText(serviceType_node));

        // Only check serviceType of WANIPConnection or WANPPPConnection.
        if (serviceType != UPNP_WANIP_SERVICE
            && serviceType != UPNP_WANPPP_SERVICE) {
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
        std::string serviceId(getFirstElementItem(service_element, "serviceId"));
        if (serviceId.empty()) {
            // IGD "serviceId" is empty. Going to next node.
            continue;
        }

        // Get the relative controlURL and turn it into absolute address using the URLBase.
        std::string controlURL(getFirstElementItem(service_element, "controlURL"));
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
        std::string eventSubURL(getFirstElementItem(service_element, "eventSubURL"));
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

    // Set action name.
    IXML_Document* action_container_ptr = UpnpMakeAction("GetStatusInfo",
                                          igd.getServiceType().c_str(),
                                          0,
                                          nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("PUPnP: Failed to make GetStatusInfo action");
        return false;
    }
    XMLDocument action(action_container_ptr, ixmlDocument_free); // Action pointer.

    IXML_Document* response_container_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    if (not response_container_ptr or upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send GetStatusInfo action -> %s", UpnpGetErrorMessage(upnp_err));
        return false;
    }
    XMLDocument response(response_container_ptr, ixmlDocument_free);

    if (errorOnResponse(response.get())) {
        JAMI_WARN("PUPnP: Failed to get GetStatusInfo from %s -> %d: %s",
                  igd.getServiceType().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        return false;
    }

    // Parse response.
    auto status = getFirstDocItem(response.get(), "NewConnectionStatus");
    return status == "Connected";
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
    action.reset(action_container_ptr);

    if (not action) {
        JAMI_WARN("PUPnP: Failed to make GetExternalIPAddress action");
        return {};
    }

    IXML_Document* response_container_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    response.reset(response_container_ptr);

    if (not response or upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send GetExternalIPAddress action -> %s",
                  UpnpGetErrorMessage(upnp_err));
        return {};
    }

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

    if (not clientRegistered_ or not upnpIgd->isValid() or not upnpIgd->getLocalIp())
        return mapList;

    // Set action name.
    static constexpr const char* action_name {"GetGenericPortMappingEntry"};

    for (int entry_idx = 0;; entry_idx++) {
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
            action(nullptr, ixmlDocument_free); // Action pointer.
        IXML_Document* action_container_ptr = nullptr;

        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&>
            response(nullptr, ixmlDocument_free); // Response pointer.
        IXML_Document* response_container_ptr = nullptr;

        UpnpAddToAction(&action_container_ptr,
                        action_name,
                        upnpIgd->getServiceType().c_str(),
                        "NewPortMappingIndex",
                        std::to_string(entry_idx).c_str());
        action.reset(action_container_ptr);

        if (not action) {
            JAMI_WARN("PUPnP: Failed to add NewPortMappingIndex action");
            break;
        }

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
        auto errorCode = getFirstDocItem(response.get(), "errorCode");
        if (not errorCode.empty()) {
            auto error = to_int<int>(errorCode);
            if (error == ARRAY_IDX_INVALID or error == CONFLICT_IN_MAPPING) {
                // No more port mapping entries in the response.
                JAMI_DBG("PUPnP: No more mappings (found a total of %i mappings", entry_idx);
                break;
            } else {
                auto errorDescription = getFirstDocItem(response.get(), "errorDescription");
                JAMI_ERROR("PUPnP: GetGenericPortMappingEntry returned with error: {:s}: {:s}",
                         errorCode,
                         errorDescription);
                break;
            }
        }

        // Parse the response.
        auto desc_actual = getFirstDocItem(response.get(), "NewPortMappingDescription");
        auto client_ip = getFirstDocItem(response.get(), "NewInternalClient");

        if (client_ip != getHostAddress().toString()) {
            // Silently ignore un-matching addresses.
            continue;
        }

        if (desc_actual.find(description) == std::string::npos)
            continue;

        auto port_internal = getFirstDocItem(response.get(), "NewInternalPort");
        auto port_external = getFirstDocItem(response.get(), "NewExternalPort");
        std::string transport(getFirstDocItem(response.get(), "NewProtocol"));

        if (port_internal.empty() || port_external.empty() || transport.empty()) {
            JAMI_ERR("PUPnP: GetGenericPortMappingEntry returned an invalid entry at index %i",
                     entry_idx);
            continue;
        }

        std::transform(transport.begin(), transport.end(), transport.begin(), ::toupper);
        PortType type = transport.find("TCP") != std::string::npos ? PortType::TCP : PortType::UDP;
        auto ePort = to_int<uint16_t>(port_external);
        auto iPort = to_int<uint16_t>(port_internal);

        Mapping map(type, ePort, iPort);
        map.setIgd(igd);

        mapList.emplace(map.getMapKey(), std::move(map));
    }

    JAMI_DEBUG("PUPnP: Found {:d} allocated mappings on IGD {:s}",
             mapList.size(),
             upnpIgd->toString());

    return mapList;
}

void
PUPnP::deleteMappingsByDescription(const std::shared_ptr<IGD>& igd, const std::string& description)
{
    if (not(clientRegistered_ and igd->getLocalIp()))
        return;

    JAMI_DBG("PUPnP: Remove all mappings (if any) on IGD %s matching descr prefix %s",
             igd->toString().c_str(),
             Mapping::UPNP_MAPPING_DESCRIPTION_PREFIX);

    auto mapList = getMappingsListByDescr(igd, description);

    for (auto const& [_, map] : mapList) {
        requestMappingRemove(map);
    }
}

bool
PUPnP::actionAddPortMapping(const Mapping& mapping)
{
    CHECK_VALID_THREAD();

    if (not clientRegistered_)
        return false;

    auto igdIn = std::dynamic_pointer_cast<UPnPIGD>(mapping.getIgd());
    if (not igdIn)
        return false;

    // The requested IGD must be present in the list of local valid IGDs.
    auto igd = findMatchingIgd(igdIn->getControlURL());

    if (not igd or not igd->isValid())
        return false;

    // Action and response pointers.
    XMLDocument action(nullptr, ixmlDocument_free);
    IXML_Document* action_container_ptr = nullptr;
    XMLDocument response(nullptr, ixmlDocument_free);
    IXML_Document* response_container_ptr = nullptr;

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewRemoteHost",
                    "");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewExternalPort",
                    mapping.getExternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewProtocol",
                    mapping.getTypeStr());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewInternalPort",
                    mapping.getInternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewInternalClient",
                    getHostAddress().toString().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewEnabled",
                    "1");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewPortMappingDescription",
                    mapping.toString().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_ADD_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewLeaseDuration",
                    "0");

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd->getControlURL().c_str(),
                                  igd->getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    response.reset(response_container_ptr);

    bool success = true;

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send action %s for mapping %s. %d: %s",
                  ACTION_ADD_PORT_MAPPING,
                  mapping.toString().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        JAMI_WARN("PUPnP: IGD ctrlUrl %s", igd->getControlURL().c_str());
        JAMI_WARN("PUPnP: IGD service type %s", igd->getServiceType().c_str());

        success = false;
    }

    // Check if an error has occurred.
    auto errorCode = getFirstDocItem(response.get(), "errorCode");
    if (not errorCode.empty()) {
        success = false;
        // Try to get the error description.
        std::string errorDescription;
        if (response) {
            errorDescription = getFirstDocItem(response.get(), "errorDescription");
        }

        JAMI_WARNING("PUPnP: {:s} returned with error: {:s} {:s}",
                  ACTION_ADD_PORT_MAPPING,
                  errorCode,
                  errorDescription);
    }
    return success;
}

bool
PUPnP::actionDeletePortMapping(const Mapping& mapping)
{
    CHECK_VALID_THREAD();

    if (not clientRegistered_)
        return false;

    auto igdIn = std::dynamic_pointer_cast<UPnPIGD>(mapping.getIgd());
    if (not igdIn)
        return false;

    // The requested IGD must be present in the list of local valid IGDs.
    auto igd = findMatchingIgd(igdIn->getControlURL());

    if (not igd or not igd->isValid())
        return false;

    // Action and response pointers.
    XMLDocument action(nullptr, ixmlDocument_free);
    IXML_Document* action_container_ptr = nullptr;
    XMLDocument response(nullptr, ixmlDocument_free);
    IXML_Document* response_container_ptr = nullptr;

    // Set action sequence.
    UpnpAddToAction(&action_container_ptr,
                    ACTION_DELETE_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewRemoteHost",
                    "");
    UpnpAddToAction(&action_container_ptr,
                    ACTION_DELETE_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewExternalPort",
                    mapping.getExternalPortStr().c_str());
    UpnpAddToAction(&action_container_ptr,
                    ACTION_DELETE_PORT_MAPPING,
                    igd->getServiceType().c_str(),
                    "NewProtocol",
                    mapping.getTypeStr());

    action.reset(action_container_ptr);

    int upnp_err = UpnpSendAction(ctrlptHandle_,
                                  igd->getControlURL().c_str(),
                                  igd->getServiceType().c_str(),
                                  nullptr,
                                  action.get(),
                                  &response_container_ptr);
    response.reset(response_container_ptr);

    bool success = true;

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("PUPnP: Failed to send action %s for mapping from %s. %d: %s",
                  ACTION_DELETE_PORT_MAPPING,
                  mapping.toString().c_str(),
                  upnp_err,
                  UpnpGetErrorMessage(upnp_err));
        JAMI_WARN("PUPnP: IGD ctrlUrl %s", igd->getControlURL().c_str());
        JAMI_WARN("PUPnP: IGD service type %s", igd->getServiceType().c_str());

        success = false;
    }

    if (not response) {
        JAMI_WARN("PUPnP: Failed to get response for %s", ACTION_DELETE_PORT_MAPPING);
        success = false;
    }

    // Check if there is an error code.
    auto errorCode = getFirstDocItem(response.get(), "errorCode");
    if (not errorCode.empty()) {
        auto errorDescription = getFirstDocItem(response.get(), "errorDescription");
        JAMI_WARNING("PUPnP: {:s} returned with error: {:s}: {:s}",
                  ACTION_DELETE_PORT_MAPPING,
                  errorCode,
                  errorDescription);
        success = false;
    }

    return success;
}

} // namespace upnp
} // namespace jami
