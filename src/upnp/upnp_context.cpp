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

#define LOCK_MUTEX(mutexToLock)  \
    std::lock_guard<std::mutex> lock(mutexToLock);

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
        runOnUpnpThread([this] {
            init();
        });
    }
}

UPnPContext::~UPnPContext()
{
    mappingListUpdateTimer_->cancel();

    deleteAllMappings(PortType::UDP);
    deleteAllMappings(PortType::TCP);
}

void UPnPContext::init()
{
    using namespace std::placeholders;
    threadId_ = getCurrentThread();
    CHECK_VALID_THREAD();

#if HAVE_LIBNATPMP
    auto natPmp = std::make_shared<NatPmp>();
    natPmp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2, _3));
    natPmp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2, _3));
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::onIgdChanged, this, _1, _2, _3, _4));
    natPmp->searchForIgd();
    protocolList_.push_back(natPmp);
#endif

#if HAVE_LIBUPNP
    auto pupnp = std::make_shared<PUPnP>();
    pupnp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2, _3));
    pupnp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2, _3));
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::onIgdChanged, this, _1, _2, _3, _4));
    pupnp->searchForIgd();
    protocolList_.push_back(pupnp);
#endif

    // Set port ranges
    portRange_[0] = { Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX }; // UDP.
    portRange_[1] = { Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX }; // TCP.

    // Provision now.
    updateMappingList(false);
}

uint16_t
UPnPContext::generateRandomPort(uint16_t min, uint16_t max, bool mustBeEven)
{
    if (min >= max){
        JAMI_ERR("Max port number (%i) must be greater than min port number (%i) !",
            max, min);
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
        runOnUpnpThread([this] {
            connectivityChanged();
        });
        return;
    }

    CHECK_VALID_THREAD();

    JAMI_DBG("Connectivity changed. Reset IGDs and restart.");

    {
        LOCK_MUTEX(mappingMutex_);
        // Move all existing mappings to pending state. Mapping requests
        // will be performed anew when new IGDs are discovered.
        PortType types[2] { PortType::TCP, PortType::UDP };
        for (auto& type : types) {
            auto& mappingList = getMappingList(type);
            for (auto [_, map] : mappingList)
                map->setState(MappingState::PENDING);
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
    LOCK_MUTEX(mappingMutex_);
    return (igdLocalIp_ and igdPublicIp_);
}

IpAddr
UPnPContext::getLocalIP() const
{
    LOCK_MUTEX(mappingMutex_);
    return igdLocalIp_;
}

IpAddr
UPnPContext::getExternalIP() const
{
    LOCK_MUTEX(mappingMutex_);
    return igdPublicIp_;
}

Mapping::sharedPtr_t
UPnPContext::reserveMapping(Mapping& requestedMap)
{
    auto desiredPort = requestedMap.getPortExternal();

    if (desiredPort == 0) {
        JAMI_DBG("Desired port is not set, will provide the first available port for [%s]",
            requestedMap.getTypeStr().c_str());
    } else {
        JAMI_DBG("Try to find mapping for port %i [%s]",
            desiredPort, requestedMap.getTypeStr().c_str());
    }

    Mapping::sharedPtr_t mapRes;

    {
        LOCK_MUTEX(mappingMutex_);
        auto& mappingList = getMappingList(requestedMap.getType());

        for (auto [_, map] : mappingList) {
            // If the desired port is null, we pick the first available port.
            // TODO. The mapping must be available and usable (open or at least in in-progress) !!!
            if ( (desiredPort == 0 or map->getPortExternal() == desiredPort) and map->isAvailable()) {
                map->setAvailable(false);
                map->setNotifyCallback(requestedMap.getNotifyCallback());
                mapRes = map;
                break;
            }
        }
    }

    if (mapRes) {
        assert(mapRes->isValid());
        // Found a mapping, notify the listener.
        if (mapRes->getNotifyCallback())
            mapRes->getNotifyCallback()(mapRes);
    } else {
        // No available mapping.
        JAMI_ERR("Did not find provisioned mapping for port %i [%s]. Will request one now",
            desiredPort, requestedMap.getTypeStr().c_str());
        mapRes = registerMapping(requestedMap);
    }

    updateMappingList(true);

    return mapRes;
}

// TODO. Use shared pointer instead of key.
bool
UPnPContext::releaseMapping(Mapping::sharedPtr_t map)
{
    bool res = true;

    {
        LOCK_MUTEX(mappingMutex_);
        auto& mappingList = getMappingList(map->getType());

        auto it = mappingList.find(map->getMapKey());

        if (it == mappingList.end()) {
            JAMI_WARN("Trying to release an un-existing mapping %s",
                map->toString().c_str());
            res = false;
        } else if(it->second->isAvailable()) {
            JAMI_WARN("Trying to release an unused mapping %s",
                it->second->toString().c_str());
            res = false;
        } else {
            // Make the mapping available for future use.
            it->second->setAvailable(true);
            it->second->setNotifyCallback(nullptr);
        }
    }

    // Prune the mapping list.
    updateMappingList(true);

    return res;
}

uint16_t UPnPContext::getAvailablePortNumber(PortType type, uint16_t minPort, uint16_t maxPort)
{
    // Only return an availalable random port. No actual
    // reservation is made here.


    if (minPort > maxPort) {
        JAMI_ERR("Min port %u can not be greater than max port %u",
            minPort, maxPort);
        return 0;
    }

    unsigned typeIdx = type == PortType::UDP ? 0 : 1;
    if (minPort == 0)
        minPort = portRange_[typeIdx].first;
    if (maxPort == 0)
        maxPort = portRange_[typeIdx].second;

    LOCK_MUTEX(mappingMutex_);
    auto& mappingList = getMappingList(type);
    unsigned int tryCount = 0;
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
UPnPContext::requestMapping(std::shared_ptr<UPnPProtocol> protocol,
    std::shared_ptr<IGD> igd, Mapping::sharedPtr_t map)
{
    assert(map);

    if (not isValidThread()) {
        runOnUpnpThread([this, protocol, igd, map] {
            requestMapping(protocol, igd, map);
        });
        return;
    }

    CHECK_VALID_THREAD();
    JAMI_DBG("Request mapping %s on IGD %s",
        map->toString().c_str(), igd->getPublicIp().toString(true).c_str());

    // Register mapping timeout callback and update the state.
    registerAddMappingTimeout(igd, map->getMapKey());
    map->setState(MappingState::IN_PROGRESS);

    // Request the mapping.
    protocol->requestMappingAdd(igd.get(), *map);
}

void UPnPContext::requestMappingOnAllIgds(Mapping::sharedPtr_t map)
{
    if(not isValidThread()) {
        runOnUpnpThread([this, map] {
            requestMappingOnAllIgds(map);
        });
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
    JAMI_DBG("[pruning %s] Provision %i new mappings",
        Mapping::getTypeStr(type).c_str(), portCount);

    assert(portCount > 0);

    while (portCount > 0) {
        auto port = getAvailablePortNumber(type, minPort, maxPort);
        if (port > 0) {
            // Found an available port number
            portCount --;
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

void UPnPContext::pruneFailedMappings(PortType type)
{
    CHECK_VALID_THREAD();

    LOCK_MUTEX(mappingMutex_);
    auto& mappingList = getMappingList(type);

    auto count = 0;

    for(auto it = mappingList.begin(); it != mappingList.end(); ) {
        auto map = it->second;
        if(map->getState() == MappingState::FAILED and
            map->isAvailable()){
                it = mappingList.erase(it);
                count++;
        } else {
            it++;
        }
    }

    if (count)
        JAMI_DBG("[pruning %s] Removed %i failed mapping request(s)",
            Mapping::getTypeStr(type).c_str(), count);
}

bool UPnPContext::deleteUnneededMappings(PortType type, int portCount)
{
    JAMI_DBG("[pruning %s] Remove %i unneeded mapping",
        Mapping::getTypeStr(type).c_str(), portCount);

    assert(portCount > 0);

    CHECK_VALID_THREAD();

    LOCK_MUTEX(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for(auto it = mappingList.begin(); it != mappingList.end();) {
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
            portCount --;
        } else if (map->getState() != MappingState::OPEN) {
            // If this methods is called, it means there are more open
            // mappings than required. So, all mappings in a state other
            // than "OPEN" state (typically in in-progress state) will
            // be deleted as well.
            deleteMapping(map);
            it = unregisterMapping(it);
        } else {
            it ++;
        }
    }

    return true;
}

void
UPnPContext::updateMappingList(bool async)
{
    // Run async if requested.
    if (async) {
        runOnUpnpThread([this] {
            updateMappingList(false);
            });
        return;
    }

    PortType typeArray[2] = { PortType::UDP, PortType::TCP};

    for (auto idx : {0, 1}) {
        auto type = typeArray[idx];

        MappingStatus status;
        getMappingStatus(type, status);

        JAMI_DBG("[pruning %s] Mapping status: %i open (%i ready + %i in use), %i pending, %i in-progress, %i failed",
            Mapping::getTypeStr(type).c_str(), status.openCount_, status.readyCount_,
            status.openCount_- status.readyCount_, status.pendingCount_, status.inProgressCount_,
            status.failedCount_);

        int toRequestCount = (int)minOpenPortLimit_[idx] -
            (int)(status.readyCount_ + status.inProgressCount_ + status.pendingCount_);

        // Provision/release mappings accordingly.
        if (toRequestCount > 0) {
            // Take into account the request in-progress when making
            // requests for new mappings.
            provisionNewMappings(type, toRequestCount);
        } else if (status.readyCount_ > maxOpenPortLimit_[idx]) {
            deleteUnneededMappings(type, status.readyCount_ - maxOpenPortLimit_[idx]);
        }

        // Prune failed requests.
        pruneFailedMappings(type);
    }

    // Cancel the current timer (if any) and re-schedule.
    if (mappingListUpdateTimer_)
        mappingListUpdateTimer_->cancel();

    mappingListUpdateTimer_ = getScheduler()->scheduleIn( [this] {
        updateMappingList(false);
    },
    MAP_UPDATE_INTERVAL);
}

void
UPnPContext::onIgdChanged(std::shared_ptr<UPnPProtocol> protocol, std::shared_ptr<IGD> igd,
    IpAddr publicIpAddr, bool added)
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
    if (not added) {
        JAMI_DBG("IGD %p [%s] on address %s was removed",
            igd.get(), protocol->getProtocolName().c_str(), publicIpAddr.toString(true, true).c_str());
        return;
    }

    // Check if IGD has a valid public IP.
    if (not igd->getPublicIp()) {
        JAMI_ERR("The added IGD has an invalid public IpAddress");
        return;
    }

    JAMI_DBG("IGD %p [%s] on address %s was added. Will process any pending requests",
        igd.get(), protocol->getProtocolName().c_str(), publicIpAddr.toString(true, true).c_str());

    // This list holds the mappings to be requested. This is
    // needed to avoid performing the requests while holding
    // the lock.
    std::list<Mapping::sharedPtr_t> requestsList;

    // Populate the list of requests to perform.
    {
        LOCK_MUTEX(mappingMutex_);
        PortType typeArray [2] {PortType::UDP, PortType::TCP};

        for (auto type : typeArray) {
            auto& mappingList = getMappingList(type);
            for (auto [_, map] : mappingList) {
                // The mapping list must never have a mapping in "new" state.
                assert(map->getState() != MappingState::NEW);

                // In case of multiple IGDs, pending mapping requests will be
                // performed on each newly discovered IGD unless the  mapping
                // was already granted.
                if (map->getState() != MappingState::OPEN) {
                    JAMI_DBG("Send request for pending mapping %s to IGD %s",
                        map->toString().c_str(), igd->getPublicIp().toString(true).c_str());
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
    for (auto map : requestsList)
        requestMapping(protocol, igd, map);
}

void
UPnPContext::onMappingAdded(IpAddr igdIp, const Mapping& mapRes, bool success)
{
    if (not mapRes.isValid()) {
        JAMI_ERR("PUPnP: Mapping %s is invalid !", mapRes.toString().c_str());
        return;
    }

    if (not isValidThread()) {
        runOnUpnpThread([this, igdIp, mapRes, success] {
            onMappingAdded(igdIp, mapRes, success);
        });
        return;
    }

    CHECK_VALID_THREAD();

    Mapping::key_t key = 0;

    {
        LOCK_MUTEX(mappingMutex_);
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
    // The mapping must be valid at his point.
    assert(map);
    assert(map->isValid());

    // Update the state.
    if (success) {
        if (map->getState() == MappingState::OPEN) {
            JAMI_DBG("PUPnP: Mapping %s is already open on IGD %s",
                map->toString().c_str(), igdIp.toString().c_str());
        } else {
            JAMI_DBG("PUPnP: Mapping %s (on IGD %s) successfully performed",
                map->toString().c_str(), igdIp.toString().c_str());
            map->setState(MappingState::OPEN);
        }
    } else {
        JAMI_ERR("PUPnP: Failed to perform mapping %s (on IGD %s)",
            map->toString().c_str(), igdIp.toString().c_str());
        map->setState(MappingState::FAILED);
    }

    // Cancel the time-out.
    map->cancelTimeoutTimer();

    // Notify the listener.
    if (map->getNotifyCallback())
        map->getNotifyCallback()(map);
}

void
UPnPContext::deleteMapping(Mapping::sharedPtr_t map)
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
        runOnUpnpThread([this, type] {
            deleteAllMappings(type);
        });
        return;
    }

    CHECK_VALID_THREAD();

    LOCK_MUTEX(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for (auto [_, map] : mappingList) {
        mappingList.erase(map->getMapKey());
        for (auto& proto : protocolList_) {
            proto->requestMappingRemove(*map);
        }
    }

    getMappingList(PortType::TCP).clear();
    getMappingList(PortType::UDP).clear();
}

void
UPnPContext::onMappingRemoved(IpAddr igdIp, const Mapping& mapRes, bool success)
{
    if (not mapRes.isValid())
        return;

     if (not isValidThread()) {
        runOnUpnpThread([this, igdIp, mapRes, success] {
            onMappingRemoved(igdIp, mapRes, success);
        });
        return;
    }

    CHECK_VALID_THREAD();

    {
        if (success) {
            // TODO. Do we care ?
        }
        auto map = getMappingWithKey(mapRes.getMapKey());
        // Notify the listener.
        if (map and map->getNotifyCallback())
            map->getNotifyCallback()(map);
    }
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
        LOCK_MUTEX(mappingMutex_);
        auto& mappingList = getMappingList(map.getType());
        auto ret = mappingList.emplace(map.getMapKey(), std::make_shared<Mapping>(map));
        if (not ret.second) {
            JAMI_WARN("Mapping request for %s already added !",
                map.toString().c_str());
            return {};
        }
        mapPtr = ret.first->second;
        assert(mapPtr);
        // Newly added mapping must be in pending state by default.
        mapPtr->setState(MappingState::PENDING);
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
    return mappingList.erase(it);
}

std::map<Mapping::key_t, Mapping::sharedPtr_t>&
UPnPContext::getMappingList(PortType type)
{
    unsigned typeIdx = type == PortType::UDP ? 0 : 1;
    return mappingList_[typeIdx];
}

std::map<Mapping::key_t, Mapping::sharedPtr_t>&
UPnPContext::getMappingList(Mapping::key_t key)
{
    return getMappingList(Mapping::getTypeFromMapKey(key));
}

const Mapping::sharedPtr_t
UPnPContext::getMappingWithKey(Mapping::key_t key)
{
    LOCK_MUTEX(mappingMutex_);
    auto& mappingList = getMappingList(Mapping::getTypeFromMapKey(key));
    auto it = mappingList.find(key);
    if (it == mappingList.end())
        return nullptr;
    return it->second;
}

size_t
UPnPContext::getMappingStatus(PortType type, MappingStatus& status)
{
    // TODO. Can we optimize this by kepping the count of open/in-progress/available
    // mappings ?

    LOCK_MUTEX(mappingMutex_);
    auto& mappingList = getMappingList(type);

    for (auto [_, map] : mappingList) {
        switch (map->getState())
        {
            case MappingState::PENDING : {
                status.pendingCount_++;
                break;
            }
            case MappingState::IN_PROGRESS : {
                status.inProgressCount_++;
                break;
            }
            case MappingState::FAILED : {
                status.failedCount_++;
                break;
            }
            case MappingState::OPEN : {
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

    return mappingList.size();
}

void
UPnPContext::registerAddMappingTimeout(std::shared_ptr<IGD> igd, Mapping::key_t key)
{
    // Schedule the timer and hold a pointer on the task.
    auto map = getMappingWithKey(key);
    if (not map) {
        JAMI_ERR("Trying to register time-out callback for a un existing Mapping with key %lu", key);
        return;
    }

    MappingStatus status;
    getMappingStatus(PortType::TCP, status);
    getMappingStatus(PortType::UDP, status);

    map->setTimeoutTimer( Manager::instance().scheduler().scheduleIn(
        [this, key, igd] {
            onRequestTimeOut(igd, key);
        },
        // The time-out is proportional to the number of in-progress requests.
        MAP_REQUEST_TIMEOUT_UNIT * (status.inProgressCount_ + 1)));
}

void
UPnPContext::onRequestTimeOut(std::shared_ptr<IGD> igd, Mapping::key_t key)
{
    auto map = getMappingWithKey(key);
    if (not map)
        return;

    // Ignore time-out if the request is not in-progress state.
    // Should not occur.
    if (map->getState() != MappingState::IN_PROGRESS) {
        JAMI_ERR("Mapping %s timed-out but is not in IN-PROGRESS state (curr %s)",
            map->toString().c_str(), map->getStateStr().c_str());
        return;
    }

    JAMI_WARN("Mapping request for %s timed-out on IGD %s",
        map->toString().c_str(), igd->getUID().c_str());

    // Considere time-out as failure.
    map->setState(MappingState::FAILED);

    // Notify the listener.
    if (map->getNotifyCallback())
        map->getNotifyCallback()(map);
}

} // namespace upnp
} // namespace jami
