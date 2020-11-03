/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
        runOnUpnpThread([this] { init(); });
    }
}

UPnPContext::~UPnPContext()
{
    mappingListUpdateTimer_->cancel();

    // TODO. Is it really usefull ?
    deleteAllMappings(PortType::UDP);
    deleteAllMappings(PortType::TCP);
}

void
UPnPContext::init()
{
    using namespace std::placeholders;
    threadId_ = getCurrentThread();
    CHECK_VALID_THREAD();

#if HAVE_LIBNATPMP
    auto natPmp = std::make_shared<NatPmp>();
    natPmp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2));
    natPmp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2));
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::onIgdChanged, this, _1, _2, _3, _4));
    natPmp->searchForIgd();
    protocolList_.push_back(natPmp);
#endif

#if HAVE_LIBUPNP
    auto pupnp = std::make_shared<PUPnP>();
    pupnp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2));
    pupnp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2));
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::onIgdChanged, this, _1, _2, _3, _4));
    pupnp->searchForIgd();
    protocolList_.push_back(pupnp);
#endif

    // Set port ranges
    portRange_[0] = {Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX}; // UDP.
    portRange_[1] = {Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX}; // TCP.

    // Provision now.
    updateMappingList(false);
}

uint16_t
UPnPContext::generateRandomPort(uint16_t min, uint16_t max, bool mustBeEven)
{
    if (min >= max) {
        JAMI_ERR("Max port number (%i) must be greater than min port number (%i) !", max, min);
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
        runOnUpnpThread([this] { connectivityChanged(); });
        return;
    }

    CHECK_VALID_THREAD();

    JAMI_DBG("Connectivity changed. Reset IGDs and restart.");

    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        // Move all existing mappings to "FAILED" and notify the owners.
        // New mapping requests will be performed  when new IGDs are
        // discovered.
        PortType types[2] {PortType::TCP, PortType::UDP};
        for (auto& type : types) {
            auto& mappingList = getMappingList(type);
            auto it = mappingList.begin();
            while (it != mappingList.end()) {
                auto& map = it->second;
                map->cancelTimeoutTimer();
                updateMappingState(map, MappingState::PENDING, false);
                it++;
            }
        }

        // Invalidate the current IGDs.
        igdLocalIp_ = std::move(IpAddr());
        igdPublicIp_ = std::move(IpAddr());
    }

    for (auto const& protocol : protocolList_)
        protocol->clearIgds();

    // Request a new IGD search.
    for (auto const& protocol : protocolList_)
        protocol->searchForIgd();
}

bool
UPnPContext::hasValidIGD() const
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    return (igdLocalIp_ and igdPublicIp_);
}

IpAddr
UPnPContext::getLocalIP() const
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    return igdLocalIp_;
}

IpAddr
UPnPContext::getExternalIP() const
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    return igdPublicIp_;
}

Mapping::sharedPtr_t
UPnPContext::reserveMapping(Mapping& requestedMap)
{
    auto desiredPort = requestedMap.getPortExternal();

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

        for (auto const& [_, map] : mappingList) {
            // If the desired port is null, we pick the first available port.
            // The mapping must be available and usable (open or at least in in-progress) !!!
            if ((desiredPort == 0 or map->getPortExternal() == desiredPort) and map->isAvailable()) {
                // Copy attributes.
                map->setNotifyCallback(requestedMap.getNotifyCallback());
                map->enableAutoUpdate(requestedMap.getAutoUpdate());
                mapRes = map;
                break;
            }
        }
    }

    if (mapRes) {
        // Found a mapping, notify the listener.
        if (mapRes->getNotifyCallback())
            mapRes->getNotifyCallback()(mapRes);
    } else {
        // No available mapping.
        JAMI_WARN("Did not find any available mapping. Will request one now");
        mapRes = registerMapping(requestedMap);
    }

    if (mapRes) {
        // Make the mapping unavailable
        mapRes->setAvailable(false);
    }

    updateMappingList(true);

    return mapRes;
}

bool
UPnPContext::releaseMapping(const Mapping& map)
{
    auto mapPtr = getMappingWithKey(map.getMapKey());

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

uint16_t
UPnPContext::getAvailablePortNumber(PortType type, uint16_t minPort, uint16_t maxPort)
{
    // Only return an availalable random port. No actual
    // reservation is made here.

    if (minPort > maxPort) {
        JAMI_ERR("Min port %u can not be greater than max port %u", minPort, maxPort);
        return 0;
    }

    unsigned typeIdx = type == PortType::UDP ? 0 : 1;
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
UPnPContext::requestMapping(const std::shared_ptr<UPnPProtocol>& protocol,
                            const std::shared_ptr<IGD>& igd,
                            const Mapping::sharedPtr_t& map)
{
    assert(map);

    if (not isValidThread()) {
        runOnUpnpThread([this, protocol, igd, map] { requestMapping(protocol, igd, map); });
        return;
    }

    CHECK_VALID_THREAD();
    JAMI_DBG("Request mapping %s on IGD %s",
             map->toString().c_str(),
             igd->getPublicIp().toString(true).c_str());

    // Register mapping timeout callback and update the state.
    registerAddMappingTimeout(igd, map);
    updateMappingState(map, MappingState::IN_PROGRESS);

    // Request the mapping.
    protocol->requestMappingAdd(igd.get(), *map);
}

void
UPnPContext::requestMappingOnAllIgds(const Mapping::sharedPtr_t& map)
{
    if (not isValidThread()) {
        runOnUpnpThread([this, map] { requestMappingOnAllIgds(map); });
        return;
    }

    CHECK_VALID_THREAD();
    // Send out request to all available IGDs.
    std::list<std::shared_ptr<IGD>> igdList;
    for (auto& proto : protocolList_) {
        proto->getIgdList(igdList);
        for (auto const& igd : igdList)
            requestMapping(proto, igd, map);
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
            // Very unlikely to get here !
            JAMI_ERR("Can not find any available port to provision !");
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
        runOnUpnpThread([this] { updateMappingList(false); });
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

    // Prune the mapping list.
    pruneMappingList();

    // Cancel the current timer (if any) and re-schedule.
    if (mappingListUpdateTimer_)
        mappingListUpdateTimer_->cancel();

    mappingListUpdateTimer_ = getScheduler()->scheduleIn([this] { updateMappingList(false); },
                                                         MAP_UPDATE_INTERVAL);
}

void
UPnPContext::pruneMappingList()
{
    // Check if there are allocated mappings from previous
    // instances, and try to close them.
    // To avoid competing with allocation requests, this task
    // is performed only when idle (no requests in progress).

    CHECK_VALID_THREAD();

    MappingStatus status;
    getMappingStatus(status);

    if (status.inProgressCount_ > 0 or status.pendingCount_ > 0) {
        // Not now. May be next time.
        return;
    }

    std::list<std::shared_ptr<IGD>> igdsList;

    for (auto const& protocol : protocolList_) {
        if (protocol->getProtocol() != UPnPProtocol::Type::PUPNP)
            continue;

        protocol->getIgdList(igdsList);

        for (auto const& igd : igdsList) {
            auto remoteMapList
                = protocol->getMappingsListByDescr(igd, Mapping::UPNP_MAPPING_DESCRIPTION_PREFIX);
            if (not remoteMapList)
                continue;

            JAMI_DBG("Found %lu allocated mappings on IGD %s",
                     remoteMapList->size(),
                     igd->getPublicIp().toString().c_str());

            std::lock_guard<std::mutex> lock(mappingMutex_);

            // Check/synchronize local mapping list with the list
            // returned by the IGD.
            PortType types[2] {PortType::TCP, PortType::UDP};
            for (auto& type : types) {
                auto& mappingList = getMappingList(type);
                for (auto it = mappingList.begin(); it != mappingList.end();) {
                    auto& map = it->second;
                    // Set mapping as failed if not found in the list
                    // returned by the IGD.
                    if (map->getState() == MappingState::OPEN
                        and remoteMapList->find(map->getMapKey()) == remoteMapList->end()) {
                        JAMI_WARN("Mapping %s (IGD %s) marked as \"OPEN\" but not found in the "
                                  "remote list. Mark as failed !",
                                  map->toString().c_str(),
                                  igd->getPublicIp().toString().c_str());
                        updateMappingState(map, MappingState::FAILED);
                        if (map->isAvailable()) {
                            it = unregisterMapping(it);
                            continue;
                        }
                    }
                    // Next item;
                    it++;
                }
            }

            int requestCount = 0;

            for (auto itRemote = remoteMapList->begin(); itRemote != remoteMapList->end();) {
                auto& map = itRemote->second;

                auto& mappingList = getMappingList(map.getType());
                auto it = mappingList.find(map.getMapKey());
                if (it == mappingList.end()) {
                    // Not present, request mapping remove.
                    JAMI_DBG("Sending a remove request for un-tracked mapping %s on IGD %s",
                             map.toString().c_str(),
                             igd->getPublicIp().toString().c_str());
                    protocol->requestMappingRemove(map);
                    // Make only few remove requests at once.
                    if (requestCount++ >= MAX_REQUEST_REMOVE_COUNT)
                        break;
                }
                itRemote = remoteMapList->erase(itRemote);
            }
        }
    }
}

void
UPnPContext::onIgdChanged(const std::shared_ptr<UPnPProtocol>& protocol,
                          const std::shared_ptr<IGD>& igd,
                          IpAddr publicIpAddr,
                          bool added)
{
    assert(igd != nullptr);

    if (not isValidThread()) {
        runOnUpnpThread([this, protocol, igd, publicIpAddr, added] {
            onIgdChanged(protocol, igd, publicIpAddr, added);
        });
        return;
    }

    CHECK_VALID_THREAD();

    // Assume invalid IGD, will be updated at the end of this function.
    igdLocalIp_ = std::move(IpAddr());
    igdPublicIp_ = std::move(IpAddr());

    // The IGD was removed. Nothing to do.
    // TODO. Should invalidate all the mappings ? May be merge with connectivity change ?
    if (not added) {
        JAMI_DBG("IGD %p [%s] on address %s was removed",
                 igd.get(),
                 protocol->getProtocolName().c_str(),
                 publicIpAddr.toString(true, true).c_str());
        return;
    }

    // Check if IGD has a valid public IP.
    if (not igd->getPublicIp()) {
        JAMI_ERR("The added IGD has an invalid public IpAddress");
        return;
    }

    JAMI_DBG("IGD %p [%s] on address %s was added. Will process any pending requests",
             igd.get(),
             protocol->getProtocolName().c_str(),
             publicIpAddr.toString(true, true).c_str());

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
                // The mapping list must never have a mapping in "new" state.
                assert(map->getState() != MappingState::NEW);

                // In case of multiple IGDs, pending mapping requests will be
                // performed on each newly discovered IGD unless the  mapping
                // was already granted.
                if (map->getState() != MappingState::OPEN) {
                    JAMI_DBG("Send request for pending mapping %s to IGD %s",
                             map->toString().c_str(),
                             igd->getPublicIp().toString(true).c_str());
                    requestsList.emplace_back(map);
                }
            }
        }
        // Update the addresses. Both must be valid.
        if (igd->getPublicIp() and igd->getLocalIp()) {
            igdPublicIp_ = igd->getPublicIp();
            igdLocalIp_ = igd->getLocalIp();
        }
    }

    // Make the request.
    for (auto const& map : requestsList)
        requestMapping(protocol, igd, map);
}

void
UPnPContext::onMappingAdded(IpAddr igdIp, const Mapping& mapRes)
{
    if (not mapRes.isValid()) {
        JAMI_ERR("PUPnP: Mapping %s is invalid !", mapRes.toString().c_str());
        return;
    }

    if (not isValidThread()) {
        runOnUpnpThread([this, igdIp, mapRes] { onMappingAdded(igdIp, mapRes); });
        return;
    }

    CHECK_VALID_THREAD();

    Mapping::key_t key = 0;

    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto& mappingList = getMappingList(mapRes.getType());
        auto it = mappingList.find(mapRes.getMapKey());
        if (it == mappingList.end()) {
            // We may receive a response for a canceled request. Just
            // ignore it silently.
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
        if (map->getState() == MappingState::OPEN) {
            JAMI_DBG("PUPnP: Mapping %s is already open on IGD %s",
                     map->toString().c_str(),
                     igdIp.toString().c_str());
        } else {
            JAMI_DBG("PUPnP: Mapping %s (on IGD %s) successfully performed",
                     map->toString().c_str(),
                     igdIp.toString().c_str());
            updateMappingState(map, MappingState::OPEN);
        }
    } else {
        JAMI_ERR("PUPnP: Failed to perform mapping %s (on IGD %s)",
                 map->toString().c_str(),
                 igdIp.toString().c_str());
        updateMappingState(map, MappingState::FAILED);
        // Remove if not used.
        if (map->isAvailable()) {
            unregisterMapping(map);
        }
    }

    // We have a response, so cancel the time-out.
    map->cancelTimeoutTimer();
}

void
UPnPContext::deleteMapping(const Mapping::sharedPtr_t& map)
{
    if (not map) {
        JAMI_ERR("Invalid mapping shared pointer");
        return;
    }

    CHECK_VALID_THREAD();

    // TODO. Should make the remove request only
    // on the IGD that granted the mapping.
    for (auto& proto : protocolList_) {
        proto->requestMappingRemove(*map);
    }
}

void
UPnPContext::deleteAllMappings(PortType type)
{
    if (not isValidThread()) {
        runOnUpnpThread([this, type] { deleteAllMappings(type); });
        return;
    }

    CHECK_VALID_THREAD();

    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for (auto it = mappingList.begin(); it != mappingList.end();) {
        auto map = it->second;
        it = unregisterMapping(it);
        for (auto& proto : protocolList_) {
            proto->requestMappingRemove(*map);
        }
    }

    getMappingList(PortType::TCP).clear();
    getMappingList(PortType::UDP).clear();
}

void
UPnPContext::onMappingRemoved(IpAddr igdIp, const Mapping& mapRes)
{
    if (not mapRes.isValid())
        return;

    if (not isValidThread()) {
        runOnUpnpThread([this, igdIp, mapRes] { onMappingRemoved(igdIp, mapRes); });
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
    if (map.getPortExternal() == 0) {
        JAMI_DBG("Port number not set. Will set a random port number");
        auto port = getAvailablePortNumber(map.getType());
        map.setPortExternal(port);
        map.setPortInternal(port);
    }

    Mapping::sharedPtr_t mapPtr;

    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        auto& mappingList = getMappingList(map.getType());
        auto ret = mappingList.emplace(map.getMapKey(), std::make_shared<Mapping>(map));
        if (not ret.second) {
            JAMI_WARN("Mapping request for %s already added !", map.toString().c_str());
            return {};
        }
        mapPtr = ret.first->second;
        assert(mapPtr);
        // Newly added mapping must be in pending state by default.
        updateMappingState(mapPtr, MappingState::PENDING, false);
    }

    // No available IGD. The pending mapping requests will be processed
    // when a IGD becomes available (in onIgdAdded() method).
    if (not hasValidIGD()) {
        JAMI_WARN("No IGD available. Mapping will be requested when an IGD becomes available");
    } else {
        requestMappingOnAllIgds(mapPtr);
    }

    return mapPtr;
}

std::map<Mapping::key_t, Mapping::sharedPtr_t>::iterator
UPnPContext::unregisterMapping(std::map<Mapping::key_t, Mapping::sharedPtr_t>::iterator it)
{
    assert(it->second);

    CHECK_VALID_THREAD();

    auto& mappingList = getMappingList(it->second->getType());
    auto ret = mappingList.erase(it);
    if (ret != mappingList.end()) {
        JAMI_DBG("Unregister mapping %s succeeded", it->second->toString().c_str());
    } else {
        JAMI_ERR("Failed to unregister mapping %s", it->second->toString().c_str());
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

const Mapping::sharedPtr_t
UPnPContext::getMappingWithKey(Mapping::key_t key)
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    auto& mappingList = getMappingList(Mapping::getTypeFromMapKey(key));
    auto it = mappingList.find(key);
    if (it == mappingList.end())
        return nullptr;
    return it->second;
}

void
UPnPContext::getMappingStatus(PortType type, MappingStatus& status)
{
    // TODO. Can we optimize this by kepping the count of open/in-progress/available
    // mappings ?

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
    // Schedule the timer and hold a pointer on the task.
    if (not map) {
        JAMI_ERR("Invalid mapping pointer");
        return;
    }

    MappingStatus status;
    getMappingStatus(status);

    map->setTimeoutTimer(
        Manager::instance()
            .scheduler()
            .scheduleIn([this, map, igd] { onRequestTimeOut(igd, map); },
                        // The time-out is proportional to the number of in-progress requests.
                        MAP_REQUEST_TIMEOUT_UNIT * (status.inProgressCount_ + 1)));
}

void
UPnPContext::onRequestTimeOut(const std::shared_ptr<IGD>& igd, const Mapping::sharedPtr_t& map)
{
    if (not map) {
        JAMI_ERR("Invalid mapping pointer");
        return;
    }

    // Ignore time-out if the request is not in-progress state.
    // Should not occur.
    if (map->getState() != MappingState::IN_PROGRESS) {
        JAMI_ERR("Mapping %s timed-out but is not in IN-PROGRESS state (curr %s).",
                 map->toString().c_str(),
                 map->getStateStr().c_str());
        return;
    }

    JAMI_WARN("Mapping request for %s timed-out on IGD %s",
              map->toString().c_str(),
              igd->getUID().c_str());

    updateMappingState(map, MappingState::FAILED);
    // Remove if not used.
    if (map->isAvailable()) {
        unregisterMapping(map);
    }
}

void
UPnPContext::updateMappingState(const Mapping::sharedPtr_t& map, MappingState newState, bool notify)
{
    if (not map) {
        JAMI_ERR("Invalid mapping pointer");
        return;
    }

    // Ignore if the state did not change
    if (newState == map->getState()) {
        JAMI_WARN("Mapping %s already in state %s",
                  map->toString().c_str(),
                  map->getStateStr().c_str());
        return;
    }

    // Considere time-out as failure.
    map->setState(newState);

    // Notify the listener if set.
    if (notify and map->getNotifyCallback())
        map->getNotifyCallback()(map);

    // On fail, request a new mapping if it was open and auto-update is enabled.
    if (newState == MappingState::FAILED and map->getState() == MappingState::OPEN
        and map->getAutoUpdate()) {
        JAMI_DBG("Mapping %s has auto-update enabled, will request new mapping now",
                 map->toString().c_str());
        // Run async to avoid double locks.
        runOnUpnpThread([this, map] {
            map->setPortExternal(0);
            map->setPortInternal(0);
            reserveMapping(*map);
        });
    }
}
} // namespace upnp
} // namespace jami
