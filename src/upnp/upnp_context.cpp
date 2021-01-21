/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#include "upnp_context.h"

namespace jami {
namespace upnp {

std::shared_ptr<UPnPContext>
UPnPContext::getUPnPContext()
{
    // This is the unique shared instance (singleton) of UPnPContext class.
    static auto context = std::make_shared<UPnPContext>();
    return context;
}

UPnPContext::UPnPContext()
{
    if (not isValidThread()) {
        runOnUpnpContextThread([this] { init(); });
        return;
    }
}

UPnPContext::~UPnPContext()
{
    mappingListUpdateTimer_->cancel();

    deleteAllMappings(PortType::UDP);
    deleteAllMappings(PortType::TCP);
}

void
UPnPContext::init()
{
    threadId_ = getCurrentThread();
    CHECK_VALID_THREAD();

#if HAVE_LIBNATPMP
    auto natPmp = std::make_shared<NatPmp>();
    natPmp->setObserver(this);
    protocolList_.emplace(NatProtocolType::NAT_PMP, std::move(natPmp));
#endif

#if HAVE_LIBUPNP
    auto pupnp = std::make_shared<PUPnP>();
    pupnp->setObserver(this);
    protocolList_.emplace(NatProtocolType::PUPNP, std::move(pupnp));
#endif

    // Set port ranges
    portRange_[0] = {UPNP_TCP_PORT_MIN, UPNP_TCP_PORT_MAX};
    portRange_[1] = {UPNP_UDP_PORT_MIN, UPNP_UDP_PORT_MAX};
}

void
UPnPContext::StartUpnp()
{
    assert(not controllerList_.empty());

    CHECK_VALID_THREAD();

    JAMI_DBG("Starting UPNP context");

    // Request a new IGD search.
    for (auto const& [_, protocol] : protocolList_) {
        protocol->searchForIgd();
    }

    started_ = true;
}

void
UPnPContext::StopUpnp()
{
    CHECK_VALID_THREAD();

    JAMI_DBG("Stoping UPNP context");

    // Use temporary list to avoid holding the lock while
    // processing the mapping list.
    std::list<Mapping::sharedPtr_t> toRemoveList;
    {
        std::lock_guard<std::mutex> lock(mappingMutex_);

        PortType types[2] {PortType::TCP, PortType::UDP};
        for (auto& type : types) {
            auto& mappingList = getMappingList(type);
            for (auto const& [_, map] : mappingList) {
                toRemoveList.emplace_back(map);
            }
        }
        // Invalidate the current IGDs.
        validIgdList_.clear();
    }

    for (auto const& map : toRemoveList) {
        map->cancelTimeoutTimer();
        updateMappingState(map, MappingState::FAILED);
        if (map->isAvailable()) {
            unregisterMapping(map);
        }
    }

    for (auto const& [_, protocol] : protocolList_) {
        protocol->clearIgds();
    }

    started_ = false;
}

uint16_t
UPnPContext::generateRandomPort(uint16_t min, uint16_t max, bool mustBeEven)
{
    if (min >= max) {
        JAMI_ERR("Max port number (%i) must be greater than min port number (%i)", max, min);
        return 0;
    }

    int fact = mustBeEven ? 2 : 1;
    if (mustBeEven) {
        min /= fact;
        max /= fact;
    }

    // Seed the generator.
    static std::mt19937 gen(dht::crypto::getSeededRandomEngine());
    // Define the range.
    std::uniform_int_distribution<uint16_t> dist(min, max);
    return dist(gen) * fact;
}

void
UPnPContext::connectivityChanged()
{
    if (not isValidThread()) {
        runOnUpnpContextThread([this] { connectivityChanged(); });
        return;
    }

    CHECK_VALID_THREAD();

    if (controllerList_.empty())
        return;

    JAMI_DBG("Connectivity changed. Reset IGDs and restart.");

    StopUpnp();
    StartUpnp();
}

bool
UPnPContext::hasValidIGD() const
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    return not validIgdList_.empty();
}

IpAddr
UPnPContext::getExternalIP() const
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    // Return the first IGD Ip available.
    if (not validIgdList_.empty()) {
        return (*validIgdList_.begin())->getPublicIp();
    }
    return {};
}

Mapping::sharedPtr_t
UPnPContext::reserveMapping(Mapping& requestedMap)
{
    auto desiredPort = requestedMap.getExternalPort();

    if (desiredPort == 0) {
        JAMI_DBG("Desired port is not set, will provide the first available port for [%s]",
                 requestedMap.getTypeStr());
    } else {
        JAMI_DBG("Try to find mapping for port %i [%s]", desiredPort, requestedMap.getTypeStr());
    }

    Mapping::sharedPtr_t mapRes;

    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto& mappingList = getMappingList(requestedMap.getType());

        // We try to provide a mapping in "OPEN" state. If not found,
        // we provide any available mapping. In this case, it's up to
        // the caller to use it or not.
        for (auto const& [_, map] : mappingList) {
            // If the desired port is null, we pick the first available port.
            if ((desiredPort == 0 or map->getExternalPort() == desiredPort) and map->isAvailable()) {
                // Considere the first available mapping regardless of
                // its state, if we dont have one yet.
                if (not mapRes)
                    mapRes = map;

                if (map->getState() == MappingState::OPEN) {
                    // Found an "OPEN" mapping. We are done.
                    mapRes = map;
                    break;
                }
            }
        }
    }

    // Create a mapping if none was available.
    if (not mapRes) {
        JAMI_WARN("Did not find any available mapping. Will request one now");
        mapRes = registerMapping(requestedMap);
    }

    if (mapRes) {
        // Make the mapping unavailable
        mapRes->setAvailable(false);
        // Copy attributes.
        mapRes->setNotifyCallback(requestedMap.getNotifyCallback());
        mapRes->enableAutoUpdate(requestedMap.getAutoUpdate());
        // Notify the listener.
        if (mapRes->getNotifyCallback())
            mapRes->getNotifyCallback()(mapRes);
    }

    updateMappingList(true);

    return mapRes;
}

bool
UPnPContext::releaseMapping(const Mapping& map)
{
    auto mapPtr = getMappingWithKey(map.getMapKey());

    if (not mapPtr) {
        JAMI_WARN("Trying to release an unknown mapping %s", map.toString().c_str());
        return false;
    }

    if (mapPtr and mapPtr->isAvailable()) {
        JAMI_WARN("Trying to release an unused mapping %s", mapPtr->toString().c_str());
        return false;
    } else if (mapPtr->getState() == MappingState::FAILED) {
        // Remove it if in "FAILED" state.
        unregisterMapping(mapPtr);
    } else {
        // Make the mapping available for future use.
        mapPtr->setAvailable(true);
        mapPtr->setNotifyCallback(nullptr);
        mapPtr->enableAutoUpdate(false);
    }

    return true;
}

void
UPnPContext::registerController(void* controller)
{
    if (not isValidThread()) {
        runOnUpnpContextThread([this, controller] { registerController(controller); });
        return;
    }

    auto ret = controllerList_.emplace(controller);
    if (not ret.second) {
        JAMI_WARN("Controller %p is already registered", this);
        return;
    }

    JAMI_DBG("Successfully registered controller %p", this);

    if (not started_)
        StartUpnp();
}

void
UPnPContext::unregisterController(void* controller)
{
    if (not isValidThread()) {
        runOnUpnpContextThread([this, controller] { unregisterController(controller); });
        return;
    }

    if (controllerList_.erase(controller) == 1) {
        JAMI_DBG("Successfully unregistered controller %p", this);
    } else {
        JAMI_ERR("Trying to unregister an unknown controller %p", this);
    }

    if (controllerList_.empty())
        StopUpnp();
}

uint16_t
UPnPContext::getAvailablePortNumber(PortType type, uint16_t minPort, uint16_t maxPort)
{
    // Only return an availalable random port. No actual
    // reservation is made here.

    if (minPort > maxPort) {
        JAMI_ERR("Min port %u can not be greater than max port %u", minPort, maxPort);
        return 0;
    }

    unsigned typeIdx = type == PortType::TCP ? 0 : 1;
    if (minPort == 0)
        minPort = portRange_[typeIdx].first;
    if (maxPort == 0)
        maxPort = portRange_[typeIdx].second;

    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto& mappingList = getMappingList(type);
    int tryCount = 0;
    while (tryCount++ < MAX_REQUEST_RETRIES) {
        uint16_t port = generateRandomPort(minPort, maxPort);
        Mapping map {port, port, type};
        if (mappingList.find(map.getMapKey()) == mappingList.end())
            return port;
    }

    // Very unlikely to get here.
    JAMI_ERR("Could not find an available port after %i trials", MAX_REQUEST_RETRIES);
    return 0;
}

void
UPnPContext::requestMapping(const std::shared_ptr<IGD>& igd, const Mapping::sharedPtr_t& map)
{
    assert(map);

    if (not isValidThread()) {
        runOnUpnpContextThread([this, igd, map] { requestMapping(igd, map); });
        return;
    }

    CHECK_VALID_THREAD();

    JAMI_DBG("Request mapping %s using protocol [%s]",
             map->toString().c_str(),
             igd->getProtocolName());

    // Register mapping timeout callback and update the state if needed.
    registerAddMappingTimeout(igd, map);
    if (map->getState() != MappingState::IN_PROGRESS)
        updateMappingState(map, MappingState::IN_PROGRESS);

    // Request the mapping.
    auto const& protocol = protocolList_.at(igd->getProtocol());
    protocol->requestMappingAdd(igd, *map);
}

void
UPnPContext::requestMappingOnValidIgds(const Mapping::sharedPtr_t& map)
{
    if (not isValidThread()) {
        runOnUpnpContextThread([this, map] { requestMappingOnValidIgds(map); });
        return;
    }

    CHECK_VALID_THREAD();

    if (not hasValidIGD()) {
        JAMI_WARN("Can not request mapping, no valid IGD available!");
        return;
    }

    // Set to PUPNP by default.
    auto proto = protocolList_.at(NatProtocolType::PUPNP);

#if HAVE_LIBNATPMP
    // Prefere NAT-PMP if available.
    if (protocolList_.at(NatProtocolType::NAT_PMP)->hasValidIgd())
        proto = protocolList_.at(NatProtocolType::NAT_PMP);
#endif

    // Request the mappings
    if (proto->hasValidIgd()) {
        std::list<std::shared_ptr<IGD>> igdList;
        proto->getIgdList(igdList);
        for (auto const& igd : igdList)
            requestMapping(igd, map);
    }
}

bool
UPnPContext::provisionNewMappings(PortType type, int portCount, uint16_t minPort, uint16_t maxPort)
{
    JAMI_DBG("Provision %i new mappings of type [%s]", portCount, Mapping::getTypeStr(type));

    assert(portCount > 0);

    while (portCount > 0) {
        auto port = getAvailablePortNumber(type, minPort, maxPort);
        if (port > 0) {
            // Found an available port number
            portCount--;
            Mapping map {port, port, type, true};
            registerMapping(map);
        } else {
            // Very unlikely to get here!
            JAMI_ERR("Can not find any available port to provision!");
            return false;
        }
    }

    return true;
}

bool
UPnPContext::deleteUnneededMappings(PortType type, int portCount)
{
    JAMI_DBG("Remove %i unneeded mapping of type [%s]", portCount, Mapping::getTypeStr(type));

    assert(portCount > 0);

    CHECK_VALID_THREAD();

    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for (auto it = mappingList.begin(); it != mappingList.end();) {
        auto map = it->second;
        assert(map);

        if (not map->isAvailable()) {
            it++;
            continue;
        }

        if (map->getState() == MappingState::OPEN and portCount > 0) {
            // Close portCount mappings in "OPEN" state.
            deleteMapping(map);
            it = unregisterMapping(it);
            portCount--;
        } else if (map->getState() != MappingState::OPEN) {
            // If this methods is called, it means there are more open
            // mappings than required. So, all mappings in a state other
            // than "OPEN" state (typically in in-progress state) will
            // be deleted as well.
            deleteMapping(map);
            it = unregisterMapping(it);
        } else {
            it++;
        }
    }

    return true;
}

void
UPnPContext::updateMappingList(bool async)
{
    // Run async if requested.
    if (async) {
        runOnUpnpContextThread([this] { updateMappingList(false); });
        return;
    }

    CHECK_VALID_THREAD();

    // Skip if no controller registered.
    if (controllerList_.empty())
        return;

    // Cancel the current timer (if any) and re-schedule.
    if (mappingListUpdateTimer_)
        mappingListUpdateTimer_->cancel();
    mappingListUpdateTimer_ = getScheduler()->scheduleIn([this] { updateMappingList(false); },
                                                         MAP_UPDATE_INTERVAL);

    bool hasValidIgd = false;
    for (auto const& [_, protocol] : protocolList_) {
        auto valid = protocol->hasValidIgd();
        if (valid) {
            JAMI_DBG("Protocol status [%s] has valid IGD(s) %s",
                     protocol->getProtocolName(),
                     valid ? "YES" : "NO");
            hasValidIgd = true;
        }
    }

    if (not hasValidIgd) {
        JAMI_DBG("UPNP/NAT-PMP enabled, but no valid IGDs available");
        std::lock_guard<std::mutex> lock(mappingMutex_);
        // Invalidate the current IGDs.
        validIgdList_.clear();
        // No valid IGD. Nothing to do.
        return;
    }

    PortType typeArray[2] = {PortType::TCP, PortType::UDP};

    for (auto idx : {0, 1}) {
        auto type = typeArray[idx];

        MappingStatus status;
        getMappingStatus(type, status);

        JAMI_DBG("Mapping status [%s] - overall %i: %i open (%i ready + %i in use), %i pending, %i "
                 "in-progress, %i failed",
                 Mapping::getTypeStr(type),
                 status.sum(),
                 status.openCount_,
                 status.readyCount_,
                 status.openCount_ - status.readyCount_,
                 status.pendingCount_,
                 status.inProgressCount_,
                 status.failedCount_);

        int toRequestCount = (int) minOpenPortLimit_[idx]
                             - (int) (status.readyCount_ + status.inProgressCount_
                                      + status.pendingCount_);

        // Provision/release mappings accordingly.
        if (toRequestCount > 0) {
            // Take into account the request in-progress when making
            // requests for new mappings.
            provisionNewMappings(type, toRequestCount);
        } else if (status.readyCount_ > maxOpenPortLimit_[idx]) {
            deleteUnneededMappings(type, status.readyCount_ - maxOpenPortLimit_[idx]);
        }
    }

    // Prune the mapping list if needed
    if (protocolList_.at(NatProtocolType::PUPNP)->hasValidIgd()) {
#if HAVE_LIBNATPMP
        // Dont perform if NAT-PMP is valid.
        if (not protocolList_.at(NatProtocolType::NAT_PMP)->hasValidIgd())
#endif
        {
            pruneMappingList();
        }
    }

#if HAVE_LIBNATPMP
    // Renew nat-pmp allocations
    if (protocolList_.at(NatProtocolType::NAT_PMP)->hasValidIgd())
        renewAllocations();
#endif
}

void
UPnPContext::pruneMappingList()
{
    CHECK_VALID_THREAD();

    MappingStatus status;
    getMappingStatus(status);

    // Prune the list if there are pending/in-progress requests.
    if (status.inProgressCount_ == 0 and status.pendingCount_ == 0) {
        std::list<std::shared_ptr<IGD>> igdsList;

        // Performed only with PUPNP protocol.
        auto protocol = protocolList_.at(NatProtocolType::PUPNP);
        protocol->getIgdList(igdsList);

        for (auto const& igd : igdsList) {
            auto remoteMapList
                = protocol->getMappingsListByDescr(igd, Mapping::UPNP_MAPPING_DESCRIPTION_PREFIX);
            if (remoteMapList.empty())
                continue;

            pruneUnMatchedMappings(igd, remoteMapList);

            pruneUnTrackedMappings(igd, remoteMapList);
        }
    }
}

void
UPnPContext::pruneUnMatchedMappings(const std::shared_ptr<IGD>& igd,
                                    const std::map<Mapping::key_t, Mapping>& remoteMapList)
{
    // Check/synchronize local mapping list with the list
    // returned by the IGD.

    PortType types[2] {PortType::TCP, PortType::UDP};

    for (auto& type : types) {
        // Use a temporary list to avoid processing mappings while holding the lock.
        std::list<Mapping::sharedPtr_t> toRemoveList;
        {
            std::lock_guard<std::mutex> lock(mappingMutex_);
            auto& mappingList = getMappingList(type);
            for (auto const& [_, map] : mappingList) {
                // Only check mappings allocated by UPNP protocol.
                if (map->getProtocol() != NatProtocolType::PUPNP) {
                    continue;
                }
                // Set mapping as failed if not found in the list
                // returned by the IGD.
                if (map->getState() == MappingState::OPEN
                    and remoteMapList.find(map->getMapKey()) == remoteMapList.end()) {
                    toRemoveList.emplace_back(map);

                    JAMI_WARN("Mapping %s (IGD %s) marked as \"OPEN\" but not found in the "
                              "remote list. Mark as failed!",
                              map->toString().c_str(),
                              igd->getLocalIp().toString().c_str());
                }
            }
        }

        for (auto const& map : toRemoveList) {
            updateMappingState(map, MappingState::FAILED);
            // Remove if not used.
            if (map->isAvailable()) {
                unregisterMapping(map);
            }
        }
    }
}

void
UPnPContext::pruneUnTrackedMappings(const std::shared_ptr<IGD>& igd,
                                    const std::map<Mapping::key_t, Mapping>& remoteMapList)
{
    // Use a temporary list to avoid processing mappings while holding the lock.
    std::list<Mapping> toRemoveList;
    {
        std::lock_guard<std::mutex> lock(mappingMutex_);

        for (auto const& [_, map] : remoteMapList) {
            // Must be UPNP protocol.
            assert(map.getIgd()->getProtocol() == NatProtocolType::PUPNP);
            auto& mappingList = getMappingList(map.getType());
            auto it = mappingList.find(map.getMapKey());
            if (it == mappingList.end()) {
                // Not present, request mapping remove.
                JAMI_DBG("Sending a remove request for un-tracked mapping %s on IGD %s",
                         map.toString().c_str(),
                         igd->getLocalIp().toString().c_str());
                // Add to the list.
                toRemoveList.emplace_back(std::move(map));
                // Make only few remove requests at once.
                if (toRemoveList.size() >= MAX_REQUEST_REMOVE_COUNT)
                    break;
            }
        }
    }

    // Remove un-tracked mappings.
    auto protocol = protocolList_.at(NatProtocolType::PUPNP);
    for (auto const& map : toRemoveList) {
        protocol->requestMappingRemove(map);
    }
}

void
UPnPContext::pruneMappingsWithInvalidIgds(const std::shared_ptr<IGD>& igd)
{
    // Use temporary list to avoid holding the lock while
    // processing the mapping list.
    std::list<Mapping::sharedPtr_t> toRemoveList;
    {
        std::lock_guard<std::mutex> lock(mappingMutex_);

        PortType types[2] {PortType::TCP, PortType::UDP};
        for (auto& type : types) {
            auto& mappingList = getMappingList(type);
            for (auto const& [_, map] : mappingList) {
                if (map->getIgd() == igd)
                    toRemoveList.emplace_back(map);
            }
        }
    }

    for (auto const& map : toRemoveList) {
        JAMI_DBG("Remove mapping %s (has an invalid IGD %s [%s])",
                 map->toString().c_str(),
                 igd->getLocalIp().toString().c_str(),
                 igd->getProtocolName());
        map->cancelTimeoutTimer();
        updateMappingState(map, MappingState::FAILED);
        if (map->isAvailable()) {
            unregisterMapping(map);
        }
    }
}

void
UPnPContext::processPendingRequests(const std::shared_ptr<IGD>& igd)
{
    // This list holds the mappings to be requested. This is
    // needed to avoid performing the requests while holding
    // the lock.
    std::list<Mapping::sharedPtr_t> requestsList;

    // Populate the list of requests to perform.
    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        PortType typeArray[2] {PortType::TCP, PortType::UDP};

        for (auto type : typeArray) {
            auto& mappingList = getMappingList(type);
            for (auto const& [_, map] : mappingList) {
                if (map->getState() == MappingState::PENDING) {
                    JAMI_DBG("Send request for pending mapping %s to IGD %s",
                             map->toString().c_str(),
                             igd->getLocalIp().toString(true).c_str());
                    requestsList.emplace_back(map);
                }
            }
        }
    }

    // Process the pending requests.
    for (auto const& map : requestsList) {
        requestMapping(igd, map);
    }
}

void
UPnPContext::onIgdUpdated(const std::shared_ptr<IGD>& igd, UpnpIgdEvent event)
{
    assert(igd);

    if (not isValidThread()) {
        runOnUpnpContextThread([this, igd, event] { onIgdUpdated(igd, event); });
        return;
    }

    CHECK_VALID_THREAD();

    auto const& igdAddr = igd->getLocalIp();
    auto protocolName = igd->getProtocolName();

    // Check if IGD has a valid addresses.
    if (not igdAddr) {
        JAMI_ERR("[%s] IGD has an invalid local address", protocolName);
        return;
    }
    if (not igd->getPublicIp()) {
        JAMI_ERR("[%s] IGD IGD has an invalid public address", protocolName);
        return;
    }

    char const* IgdState = event == UpnpIgdEvent::ADDED
                               ? "ADDED"
                               : event == UpnpIgdEvent::REMOVED ? "REMOVED" : "INVALID";

    JAMI_WARN("State of IGD %s [%s] changed to [%s]",
              igdAddr.toString(true, true).c_str(),
              protocolName,
              IgdState);

    // The IGD was removed or is invalid.
    if (event == UpnpIgdEvent::REMOVED or event == UpnpIgdEvent::INVALID_STATE) {
        pruneMappingsWithInvalidIgds(igd);

        std::lock_guard<std::mutex> lock(mappingMutex_);
        validIgdList_.erase(igd);
        return;
    }

    // Update the IGD list.
    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto ret = validIgdList_.emplace(igd);
        if (ret.second) {
            JAMI_DBG("IGD [%s] on address %s was added. Will process any pending requests",
                     protocolName,
                     igdAddr.toString(true, true).c_str());
        } else {
            // Already in the list.
            JAMI_ERR("IGD [%s] on address %s already in the list",
                     protocolName,
                     igdAddr.toString(true, true).c_str());
            return;
        }
    }

    // Prune the IGD before making new requests.
    if (igd->getProtocol() == NatProtocolType::PUPNP) {
        auto protocol = protocolList_.at(igd->getProtocol());
        auto remoteMapList
            = protocol->getMappingsListByDescr(igd, Mapping::UPNP_MAPPING_DESCRIPTION_PREFIX);
        if (not remoteMapList.empty()) {
            pruneUnMatchedMappings(igd, remoteMapList);
        }
    }

    // Process pending requests if any.
    processPendingRequests(igd);

    // Update.
    updateMappingList(false);
}

void
UPnPContext::onMappingAdded(const std::shared_ptr<IGD>& igd, const Mapping& mapRes)
{
    CHECK_VALID_THREAD();

    if (not mapRes.isValid() or mapRes.getState() == MappingState::FAILED) {
        JAMI_WARN("Mapping request %s failed on IGD %s [%s]",
                  mapRes.toString().c_str(),
                  igd->getLocalIp().toString().c_str(),
                  mapRes.getProtocolName());
        return;
    }

    Mapping::key_t key = 0;

    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto& mappingList = getMappingList(mapRes.getType());
        auto it = mappingList.find(mapRes.getMapKey());
        if (it == mappingList.end()) {
            // We may receive a response for a canceled request. Just
            // ignore it.
            JAMI_WARN("Mapping request %s from IGD  %s [%s] does not match local request",
                      mapRes.toString().c_str(),
                      igd->getLocalIp().toString().c_str(),
                      mapRes.getProtocolName());
            return;
        }
        key = it->second->getMapKey();
        assert(key);
    }

    auto map = getMappingWithKey(key);
    // The mapping pointer must be valid at his point.
    assert(map);

    // Update the state.
    if (map->isValid()) {
        // If both protocols grant the mapping, we prefere using NAT-PMP.
        if (map->getState() == MappingState::OPEN
            and map->getProtocol() == NatProtocolType::NAT_PMP) {
            JAMI_DBG("Mapping %s is already open on IGD %s [NAT-PMP]",
                     map->toString().c_str(),
                     igd->getLocalIp().toString().c_str());
        } else {
            // Set the attributes
            map->setExternalAddress(mapRes.getExternalAddress());
            map->setInternalAddress(mapRes.getInternalAddress());
            map->setIgd(igd);
#if HAVE_LIBNATPMP
            if (mapRes.getProtocol() == NatProtocolType::NAT_PMP) {
                map->setRenewalTime(mapRes.getRenewalTime());
            }
#endif
            updateMappingState(map, MappingState::OPEN);

            JAMI_DBG("Mapping %s (on IGD %s [%s]) successfully performed",
                     map->toString().c_str(),
                     igd->getLocalIp().toString().c_str(),
                     map->getProtocolName());

            if (not igd->isValid()) {
                std::lock_guard<std::mutex> lock(mappingMutex_);
                validIgdList_.emplace(igd);
            }
            // Call setValid() to reset the errors counter.
            igd->setValid(true);
        }
    } else {
        JAMI_ERR("Failed to perform mapping %s (on IGD %s)",
                 map->toString().c_str(),
                 igd->getLocalIp().toString().c_str());
        updateMappingState(map, MappingState::FAILED);
        // Remove if not used.
        if (map->isAvailable()) {
            unregisterMapping(map);
        }
    }

    // We have a response, so cancel the time-out.
    map->cancelTimeoutTimer();
}

#if HAVE_LIBNATPMP
void
UPnPContext::onMappingRenewed(const std::shared_ptr<IGD>& igd, const Mapping& map)
{
    auto mapPtr = getMappingWithKey(map.getMapKey());

    if (not mapPtr) {
        // We may receive a notification for a canceled request. Ignore it.
        JAMI_WARN("Renewed mapping %s from IGD  %s [%s] does not have a match in local list",
                  map.toString().c_str(),
                  igd->getLocalIp().toString().c_str(),
                  map.getProtocolName());
        return;
    }
    if (mapPtr->getProtocol() != NatProtocolType::NAT_PMP or not mapPtr->isValid()
        or mapPtr->getState() != MappingState::OPEN) {
        JAMI_WARN("Renewed mapping %s from IGD %s [%s] is in unexpected state",
                  mapPtr->toString().c_str(),
                  igd->getLocalIp().toString().c_str(),
                  mapPtr->getProtocolName());
        return;
    }

    mapPtr->setRenewalTime(map.getRenewalTime());
}

#endif

void
UPnPContext::deleteMapping(const Mapping::sharedPtr_t& map)
{
    if (not map) {
        JAMI_ERR("Invalid mapping shared pointer");
        return;
    }

    CHECK_VALID_THREAD();

    auto protocol = protocolList_.at(map->getIgd()->getProtocol());
    protocol->requestMappingRemove(*map);
}

void
UPnPContext::deleteAllMappings(PortType type)
{
    if (not isValidThread()) {
        runOnUpnpContextThread([this, type] { deleteAllMappings(type); });
        return;
    }

    CHECK_VALID_THREAD();

    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for (auto it = mappingList.begin(); it != mappingList.end();) {
        auto map = it->second;
        it = unregisterMapping(it);
        auto protocol = protocolList_.at(map->getIgd()->getProtocol());
        protocol->requestMappingRemove(*map);
    }

    getMappingList(PortType::TCP).clear();
    getMappingList(PortType::UDP).clear();
}

void
UPnPContext::onMappingRemoved(const std::shared_ptr<IGD>& igd, const Mapping& mapRes)
{
    if (not mapRes.isValid())
        return;

    if (not isValidThread()) {
        runOnUpnpContextThread([this, igd, mapRes] { onMappingRemoved(igd, mapRes); });
        return;
    }

    CHECK_VALID_THREAD();

    auto map = getMappingWithKey(mapRes.getMapKey());
    // Notify the listener.
    if (map and map->getNotifyCallback())
        map->getNotifyCallback()(map);
}

Mapping::sharedPtr_t
UPnPContext::registerMapping(Mapping& map)
{
    if (map.getExternalPort() == 0) {
        JAMI_DBG("Port number not set. Will set a random port number");
        auto port = getAvailablePortNumber(map.getType());
        map.setExternalPort(port);
        map.setInternalPort(port);
    }

    // Newly added mapping must be in pending state by default.
    map.setState(MappingState::PENDING);

    Mapping::sharedPtr_t mapPtr;

    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto& mappingList = getMappingList(map.getType());
        auto ret = mappingList.emplace(map.getMapKey(), std::make_shared<Mapping>(map));
        if (not ret.second) {
            JAMI_WARN("Mapping request for %s already added!", map.toString().c_str());
            return {};
        }
        mapPtr = ret.first->second;
        assert(mapPtr);
    }

    // No available IGD. The pending mapping requests will be processed
    // when a IGD becomes available (in onIgdAdded() method).
    if (not hasValidIGD()) {
        JAMI_WARN("No IGD available. Mapping will be requested when an IGD becomes available");
    } else {
        requestMappingOnValidIgds(mapPtr);
    }

    return mapPtr;
}

std::map<Mapping::key_t, Mapping::sharedPtr_t>::iterator
UPnPContext::unregisterMapping(std::map<Mapping::key_t, Mapping::sharedPtr_t>::iterator it)
{
    assert(it->second);

    CHECK_VALID_THREAD();
    auto descr = it->second->toString();
    auto& mappingList = getMappingList(it->second->getType());
    auto ret = mappingList.erase(it);
    if (ret != mappingList.end()) {
        JAMI_DBG("Unregister mapping %s succeeded", descr.c_str());
    } else {
        JAMI_ERR("Failed to unregister mapping %s", descr.c_str());
    }

    return ret;
}

void
UPnPContext::unregisterMapping(const Mapping::sharedPtr_t& map)
{
    auto& mappingList = getMappingList(map->getType());

    if (mappingList.erase(map->getMapKey()) == 1) {
        JAMI_DBG("Unregistered mapping %s", map->toString().c_str());
    } else {
        JAMI_ERR("Failed to unregister mapping %s", map->toString().c_str());
    }
}

std::map<Mapping::key_t, Mapping::sharedPtr_t>&
UPnPContext::getMappingList(PortType type)
{
    unsigned typeIdx = type == PortType::TCP ? 0 : 1;
    return mappingList_[typeIdx];
}

Mapping::sharedPtr_t
UPnPContext::getMappingWithKey(Mapping::key_t key)
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto const& mappingList = getMappingList(Mapping::getTypeFromMapKey(key));
    auto it = mappingList.find(key);
    if (it == mappingList.end())
        return nullptr;
    return it->second;
}

void
UPnPContext::getMappingStatus(PortType type, MappingStatus& status)
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for (auto const& [_, map] : mappingList) {
        switch (map->getState()) {
        case MappingState::PENDING: {
            status.pendingCount_++;
            break;
        }
        case MappingState::IN_PROGRESS: {
            status.inProgressCount_++;
            break;
        }
        case MappingState::FAILED: {
            status.failedCount_++;
            break;
        }
        case MappingState::OPEN: {
            status.openCount_++;
            if (map->isAvailable())
                status.readyCount_++;
            break;
        }

        default:
            // Must not get here.
            assert(false);
            break;
        }
    }
}

void
UPnPContext::getMappingStatus(MappingStatus& status)
{
    getMappingStatus(PortType::TCP, status);
    getMappingStatus(PortType::UDP, status);
}

void
UPnPContext::registerAddMappingTimeout(const std::shared_ptr<IGD>& igd,
                                       const Mapping::sharedPtr_t& map)
{
    if (not map) {
        JAMI_ERR("Invalid mapping pointer");
        return;
    }

    MappingStatus status;
    getMappingStatus(status);

    // Schedule the timer and hold a pointer on the task.
    auto timeout = igd->getProtocol() == NatProtocolType::NAT_PMP ? NAT_MAP_REQUEST_TIMEOUT_UNIT
                                                                  : PUPNP_MAP_REQUEST_TIMEOUT_UNIT;
    map->setTimeoutTimer(
        // The time-out is set proportional to the number of "in-progress" requests.
        Manager::instance().scheduler().scheduleIn([this, igd, map] { onRequestTimeOut(igd, map); },
                                                   timeout * (status.inProgressCount_ + 1)));
}

void
UPnPContext::onRequestTimeOut(const std::shared_ptr<IGD>& igd, const Mapping::sharedPtr_t& map)
{
    CHECK_VALID_THREAD();

    if (not map) {
        JAMI_ERR("Invalid mapping pointer");
        return;
    }

    // Ignore time-out if the request is not in-progress state.
    // Should not occur.
    if (map->getState() != MappingState::IN_PROGRESS) {
        JAMI_ERR("Mapping %s timed-out but is not in IN-PROGRESS state (curr %s)",
                 map->toString().c_str(),
                 map->getStateStr());
        return;
    }

    JAMI_WARN("Mapping request for %s timed-out on IGD %s [%s]",
              map->toString().c_str(),
              igd->getLocalIp().toString().c_str(),
              igd->getProtocolName());

    auto protocol = protocolList_.at(igd->getProtocol());
    protocol->incrementErrorsCounter(igd);

    // Considere time-out as failure.
    updateMappingState(map, MappingState::FAILED);

    // Remove it if not used.
    if (map->isAvailable()) {
        unregisterMapping(map);
    }
}

void
UPnPContext::updateMappingState(const Mapping::sharedPtr_t& map, MappingState newState, bool notify)
{
    CHECK_VALID_THREAD();

    assert(map);

    // Ignore if the state did not change.
    if (newState == map->getState()) {
        JAMI_WARN("Mapping %s already in state %s", map->toString().c_str(), map->getStateStr());
        return;
    }

    // Update the state.
    map->setState(newState);

    // Notify the listener if set.
    if (notify and map->getNotifyCallback())
        map->getNotifyCallback()(map);

    // Nothing more to do if there is no valid IGD.
    if (not hasValidIGD())
        return;

    // On fail, request a new mapping if auto-update is enabled.
    if (newState == MappingState::FAILED and map->getAutoUpdate()) {
        JAMI_DBG("Mapping %s has auto-update enabled, a new mapping will be requested",
                 map->toString().c_str());
        // Run async to avoid double locks.
        runOnUpnpContextThread([this, map] {
            // Reserve a new mapping.
            Mapping newMapping(*map);
            newMapping.setExternalPort(0);
            newMapping.setInternalPort(0);
            reserveMapping(newMapping);
            // Release the old one.
            releaseMapping(*map);
        });
    }
}

#if HAVE_LIBNATPMP
void
UPnPContext::renewAllocations()
{
    CHECK_VALID_THREAD();

    // Check if the we have valid PMP IGD.
    auto pmpProto = protocolList_.at(NatProtocolType::NAT_PMP);

    auto now = sys_clock::now();
    std::vector<Mapping::sharedPtr_t> toRenew;

    for (auto type : {PortType::TCP, PortType::UDP}) {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto mappingList = getMappingList(type);
        for (auto const& [_, map] : mappingList) {
            if (not map->isValid())
                continue;
            if (map->getProtocol() != NatProtocolType::NAT_PMP)
                continue;
            if (map->getState() != MappingState::OPEN)
                continue;
            if (now < map->getRenewalTime())
                continue;

            toRenew.emplace_back(map);
        }
    }

    // Quit if there are no mapping to renew
    if (toRenew.empty())
        return;

    for (auto const& map : toRenew) {
        pmpProto->requestMappingRenew(*map);
    }
}
#endif

} // namespace upnp
} // namespace jami
