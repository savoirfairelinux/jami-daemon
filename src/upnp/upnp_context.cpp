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
    using namespace std::placeholders;
#if HAVE_LIBNATPMP
    auto natPmp = std::make_unique<NatPmp>();
    natPmp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2, _3));
    natPmp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2, _3));
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    natPmp->searchForIgd();
    protocolList_.push_back(std::move(natPmp));
#endif

#if HAVE_LIBUPNP
    auto pupnp = std::make_unique<PUPnP>();
    pupnp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2, _3));
    pupnp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2, _3));
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    pupnp->searchForIgd();
    protocolList_.push_back(std::move(pupnp));
#endif

    // Set port ranges
    portRange_[0] = { 20000, 20500 }; // UDP.
    portRange_[1] = { 1000, 9999 }; // TCP.

    // Provision now.
    preAllocateProvisionedPorts(PortType::TCP, 5);
    preAllocateProvisionedPorts(PortType::UDP, 8);
}

UPnPContext::~UPnPContext()
{
    requestAllMappingRemove(PortType::UDP);
    requestAllMappingRemove(PortType::TCP);
    {
        std::lock_guard<std::mutex> lock(igdListMutex_);
        igdList_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(mapProvisionListMutex_);
        for (auto& it : mapProvisionList_)
            it.clear();
    }
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
    // TODO detect if old IGD still available (if so, do not relaunch)

    // Trigger controllers onConnectionChanged
    {
        std::lock_guard<std::mutex> lock(mapCbListMutex_);
        for (auto const& ctrl : mapCbList_)
            ctrl.second.onConnectionChanged();
    }

    // Clean stuctures
    for (auto const& protocol : protocolList_)
        protocol->clearIgds();

    {
        std::lock_guard<std::mutex> lock(igdListMutex_);
        if (not igdList_.empty())
            igdList_.clear();
    }

    {
        std::lock_guard<std::mutex> lk(mapProvisionListMutex_);
        // CLear provisioned mappings.
        for (auto& it : mapProvisionList_)
            it.clear();
    }

    // Restart search for UPnP
    // When IGD ready, this should re-open ports (cf registerCallback)
    for (auto const& protocol : protocolList_)
        protocol->searchForIgd();
}

bool
UPnPContext::hasValidIGD()
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    return not igdList_.empty();
}

uint16_t
UPnPContext::chooseRandomPort(IGD& igd, PortType type)
{
    auto* globalMappings = igd.getCurrentMappingList(type);
    if (!globalMappings)
        return 0;

    uint16_t port = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);

    // Keep generating random ports until we find one which is not used.
    while (globalMappings->find(port) != globalMappings->end()) {
        port = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
    }

    return port;
}

IpAddr
UPnPContext::getLocalIP() const
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> lock(igdListMutex_);

    // Return first valid local Ip.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second) {
                return item.second->localIp_;
            }
        }
    }

    JAMI_WARN("UPnP: No valid IGD available");
    return {};
}

IpAddr
UPnPContext::getExternalIP() const
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> lock(igdListMutex_);

    // Return first valid external Ip.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second) {
                return item.second->publicIp_;
            }
        }
    }

    JAMI_WARN("UPnP: No valid IGD available");
    return {};
}

const Mapping
UPnPContext::selectProvisionedMapping(const Mapping& requestedMap)
{
    std::lock_guard<std::mutex> lk(mapProvisionListMutex_);

    auto desiredPort = requestedMap.getPortExternal();
    auto& provisionList = getMappingList(requestedMap.getType());

    JAMI_DBG("UPnP: Searching for available [%s] port in a list of %li entries",
        requestedMap.getTypeStr().c_str(), provisionList.size());

    if (desiredPort == 0) {
        JAMI_DBG("UPnP: Desired port is not set, will provide the first available port for [%s]",
            requestedMap.getTypeStr().c_str());
    } else {
        JAMI_DBG("UPnP: Try to find mapping for port %i [%s]",
            desiredPort, requestedMap.getTypeStr().c_str());
    }

    for (auto& it : provisionList) {
        auto& map = it.second;
        // If the desired port is null, we pick the first available port.
        if ( (desiredPort == 0 or map.getPortExternal() == desiredPort) and map.isAvailable()) {
            map.setAvailable(false);
            map.setDescription(requestedMap.getDescription());
            return map;
        }
    }

    JAMI_WARN("UPnP: Did not find provisioned mapping for port %i [%s]",
        desiredPort, requestedMap.getTypeStr().c_str());

    return {};
}

void
UPnPContext::unselectProvisionedPort(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapProvisionListMutex_);

    auto& provisionList = getMappingList(map.getType());
    auto it = provisionList.find(map.getPortExternal());

    if (it != provisionList.end()) {
        if(not it->second.isAvailable()) {
            // Make the mapping available for future use.
            it->second.setAvailable(true);
        } else {
            JAMI_WARN("UPnP: Trying to release an unused mapping %s",
                it->second.toString().c_str());
        }
    } else {
        JAMI_WARN("UPnP: Trying to release an un-existing mapping for port %i", map.getPortExternal());
    }
}

uint16_t UPnPContext::getAvailablePortNumber(PortType type, uint16_t minPort, uint16_t maxPort)
{
    // Only return the an availalable random port. No actual
    // reservation is made here.

    if (minPort > maxPort) {
        JAMI_ERR("UPnP: Min port %u can not be greater than max port %u",
            minPort, maxPort);
        return 0;
    }

    unsigned typeIdx = type == PortType::UDP ? 0 : 1;
    if (minPort == 0)
        minPort = portRange_[typeIdx].first;
    if (maxPort == 0)
        maxPort = portRange_[typeIdx].second;

    auto& provisionList = getMappingList(type);
    int tryCount = 0;
    while (tryCount++ < MAX_REQUEST_RETRIES) {
        uint16_t port = generateRandomPort(minPort, maxPort);
        if (provisionList.find(port) == provisionList.end())
            return port;
    }

    // Very unlikely to get here.
    JAMI_ERR("UPnP: Could not find an available port after %i trials", MAX_REQUEST_RETRIES);
    return 0;
}

bool
UPnPContext::provisionPort(IGD* igd, const Mapping& requestedMap)
{
    JAMI_DBG("UPnPContext: Provision for mapping %s on IGD %s",
        requestedMap.toString().c_str(), igd->publicIp_.toString(true).c_str());

    for (auto const& item : igdList_) {
        if (item.second == igd) {
            item.first->requestMappingAdd(igd, requestedMap);
            return true;
        }
    }

    JAMI_ERR("UPnPContext: IGD %p not found", igd);
    return false;
}

bool
UPnPContext::preAllocateProvisionedPorts(PortType type, unsigned portCount, uint16_t minPort, uint16_t maxPort)
{
    JAMI_DBG("UPnPContext: Try provision %i [%s] ports",
        portCount, Mapping::getTypeStr(type).c_str());

    unsigned int provPortsFound = 0;

    while (provPortsFound < portCount) {
        auto port = getAvailablePortNumber(type, minPort, maxPort);
        if (port > 0) {
            // Found an available port number
            provPortsFound++;
            Mapping map {port, port, type, "JAMI", true};
            // Build the control data structure.
            ControllerData ctrlData {};
            ctrlData.id = 0;
            ctrlData.keepCb = true;
            ctrlData.onMapAdded = [](const Mapping& map, bool success) {};
            ctrlData.onMapRemoved = [this](const Mapping& map, bool) {};
            ctrlData.onConnectionChanged = [this]() {};
            requestMappingAdd(ControllerData {ctrlData}, map);
        } else {
            // Very unlikely to get here !
            JAMI_ERR("UPnPContext: Can not find any available port to provision !");
            return false;
        }
    }

    return true;
}

bool
UPnPContext::isIgdInList(const UPnPProtocol* protocol, const IpAddr& publicIpAddr)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& item : igdList_) {
        if (item.second->publicIp_) {
            if (item.first == protocol && item.second->publicIp_ == publicIpAddr) {
                return true;
            }
        }
    }
    return false;
}

bool
UPnPContext::igdListChanged(UPnPProtocol* protocol, IGD* igd, IpAddr publicIpAddr, bool added)
{
    if (added) {
        JAMI_DBG("UPnPContext: IGD %p [%s] added for public address %s",
            igd, protocol->getProtocolName().c_str(), publicIpAddr.toString(true, true).c_str());
        return addIgdToList(protocol, igd);
    } else {
        JAMI_WARN("UPnPContext: Failed to add IGD %p [%s] for public address %s",
            igd, protocol->getProtocolName().c_str(), publicIpAddr.toString(true, true).c_str());
        if (publicIpAddr) {
            return removeIgdFromList(publicIpAddr);
        } else {
            return removeIgdFromList(igd);
        }
    }
}

bool
UPnPContext::addIgdToList(UPnPProtocol* protocol, IGD* igd)
{
    // Check if IGD has a valid public IP.
    if (not igd->publicIp_) {
        JAMI_WARN("UPnPContext: IGD trying to be added has invalid public IpAddress");
        return false;
    }

    // Check if the IGD is in the list.
    if (isIgdInList(protocol, igd->publicIp_)) {
        JAMI_DBG("UPnPContext: IGD with protocol %s and public IP %s is already in the list",
            protocol->getProtocolName().c_str(),
            igd->publicIp_.toString().c_str());
        return false;
    }

    {
        // Add the new IGD to the list.
        std::lock_guard<std::mutex> lock(igdListMutex_);
        igdList_.emplace_back(protocol, igd);
        JAMI_DBG("UPnPContext: IGD with protocol %s public IP %s was added to the list",
            protocol->getProtocolName().c_str(),
            igd->publicIp_.toString().c_str());
    }

    // Iterate over callback list and dispatch any pending mapping requests
    std::lock_guard<std::mutex> lock(mapCbListMutex_);
    for (auto const& cbAdd : mapCbList_) {
        JAMI_DBG("[upnp:controller@%ld] sending out request in cb queue for mapping %s",
                 (long) cbAdd.second.id,
                 cbAdd.first.toString().c_str());
        registerAddMappingTimeout(cbAdd.first);
        provisionPort(igd, cbAdd.first);
    }
    return true;
}

bool
UPnPContext::removeIgdFromList(IGD* igd)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);

    auto it = igdList_.begin();
    while (it != igdList_.end()) {
        if (it->second->publicIp_ == igd->publicIp_) {
            JAMI_WARN("UPnPContext: IGD with public IP %s was removed from the list",
                      it->second->publicIp_.toString().c_str());
            igdList_.erase(it);
            return true;
        } else {
            it++;
        }
    }

    return false;
}

bool
UPnPContext::removeIgdFromList(IpAddr publicIpAddr)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    auto it = igdList_.begin();
    while (it != igdList_.end()) {
        if (it->second->publicIp_ == publicIpAddr) {
            JAMI_WARN("UPnPContext: IGD with public IP %s was removed from the list",
                      it->second->publicIp_.toString().c_str());
            igdList_.erase(it);
            return true;
        } else {
            it++;
        }
    }

    return false;
}

bool
UPnPContext::isMappingInUse(const unsigned int portDesired, PortType type)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (igd.second->isMapInUse(portDesired, type)) {
            return true;
        }
    }
    return false;
}
void
UPnPContext::incrementNbOfUsers(const unsigned int portDesired, PortType type)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        igd.second->incrementNbOfUsers(portDesired, type);
    }
}

uint16_t
UPnPContext::requestMappingAdd(ControllerData&& ctrlData, const Mapping& requestedMap)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);

    auto map { requestedMap };
    if (map.getPortExternal() == 0) {
        JAMI_DBG("UPnPContext: Port number not set. Will request a random port number");
        auto port = getAvailablePortNumber(map.getType());
        map.setPortExternal(port);
        map.setPortInternal(port);
    }

    auto& provisionList = getMappingList(requestedMap.getType());
    auto ret = provisionList.insert(std::make_pair(requestedMap.getPortExternal(), requestedMap));
    if (not ret.second) {
        JAMI_WARN("UPnPContext: Mapping request for %s already performed !",
            requestedMap.toString().c_str());
        return 0;
    }

    // No available IGD. Register the callback and exit. The mapping
    // requests will be performed in when a IGD becomes available (in addIgdToList() method).
    if (igdList_.empty()) {
        JAMI_WARN("UPnPContext: No IGD available. Mapping will be requested when an IGD becomes available");
        registerCallback(map, std::move(ctrlData));
        return 0;
    }

    // Register the callback and the pending timeout.
    registerAddMappingTimeout(map);
    registerCallback(map, std::move(ctrlData));
    // Send out request to to all available IGDs.
    for (auto const& igd : igdList_)
        provisionPort(igd.second, map);

    return map.getPortExternal();
}

void
UPnPContext::onMappingAdded(IpAddr igdIp, const Mapping& map, bool success)
{
    if (not map.isValid()) {
        JAMI_ERR("PUPnP: Mapping %s is invalid !", map.toString().c_str());
        return;
    }

    unregisterAddMappingTimeout(map);

    auto& provisionList = getMappingList(map.getType());
    auto it = provisionList.find(map.getPortExternal());
    if (it == provisionList.end()) {
        JAMI_ERR("PUPnP: Received an response for an unknown mapping %s (on IGD %s)",
            map.toString().c_str(), igdIp.toString().c_str());
        return;
    }

    it->second.setOpen(success);

    if (success) {
        addMappingToIgd(igdIp, map);
        JAMI_DBG("PUPnP: Mapping %s (on IGD %s) successfully performed",
            it->second.toString().c_str(), igdIp.toString().c_str());
    } else {
        JAMI_ERR("PUPnP: Failed to perform mapping %s (on IGD %s)",
            it->second.toString().c_str(), igdIp.toString().c_str());
        unregisterProvisionedMapping(it->second);
    }

    dispatchOnAddCallback(it->second, success);
    // update when the state of the mapping changes.
    if (success)
        unregisterCallback(it->second);
}

void
UPnPContext::addMappingToIgd(IpAddr igdIp, const Mapping& map)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (igd.second->publicIp_ == igdIp) {
            igd.second->incrementNbOfUsers(map);
            return;
        }
    }
}

void
UPnPContext::dispatchOnAddCallback(const Mapping& map, bool success)
{
    std::vector<MapCb> cbList;
    {
        std::lock_guard<std::mutex> lock(mapCbListMutex_);
        auto mmIt = mapCbList_.equal_range(map);
        for (auto it = mmIt.first; it != mmIt.second; it++)
            cbList.emplace_back(it->second.onMapAdded);
    }
    for (auto const& cb : cbList)
        cb(map, success);
}

bool
UPnPContext::requestMappingRemove(const Mapping& map)
{
    // Only decrement the number of users. The actual removing of
    // the mapping will be done when the context is destroyed.
    auto removed = false;
    std::lock_guard<std::mutex> lock(igdListMutex_);
    if (not igdList_.empty()) {
        for (auto const& igd : igdList_) {
            if (igd.second->isMapInUse(map)) {
                if (igd.second->getNbOfUsers(map) > 1) {
                    igd.second->decrementNbOfUsers(map);
                }
                removed = true;
            }
        }
    }
    return removed;
}

void
UPnPContext::requestAllMappingRemove(PortType type)
{
    auto& list = getMappingList(type);

    for (auto& map : list) {
        for (auto& igd : igdList_) {
            igd.first->requestMappingRemove(map.second);
        }
    }
}

void
UPnPContext::onMappingRemoved(IpAddr igdIp, const Mapping& map, bool success)
{
    if (map.isValid()) {
        if (success) {
            removeMappingFromIgd(igdIp, map);
            unregisterProvisionedMapping(map);  // Will only unregister if the mapping that was removed was a provisioned port.
        }
        dispatchOnRmCallback(map, success);
        if (success)
            unregisterCallback(map);
    }
}

void
UPnPContext::removeMappingFromIgd(IpAddr igdIp, const Mapping& map)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (igd.second->publicIp_ == igdIp) {
            igd.second->removeMapInUse(map);
            return;
        }
    }
}

void
UPnPContext::dispatchOnRmCallback(const Mapping& map, bool success)
{
    std::vector<MapCb> cbList;
    {
        std::lock_guard<std::mutex> lk(mapCbListMutex_);
        for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
            if (it->first == map) {
                cbList.emplace_back(it->second.onMapRemoved);
            }
        }
    }
    for (auto const& cb : cbList)
        cb(map, success);
}

bool
UPnPContext::unregisterProvisionedMapping(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapProvisionListMutex_);
    auto& provisionList = getMappingList(map.getType());
    if (provisionList.erase(map.getPortExternal()) != 1) {
        JAMI_WARN("UPnPContext: Trying to unregister an unknown mapping %s ",
            map.toString().c_str());
        return false;
    }
    return true;
}

std::map<uint16_t, Mapping>&
UPnPContext::getMappingList(PortType type)
{
    unsigned typeIdx = type == PortType::UDP ? 0 : 1;
    return mapProvisionList_[typeIdx];
}

void
UPnPContext::registerAddMappingTimeout(const Mapping& map)
{
    std::lock_guard<std::mutex> lock(pendindRequestMutex_);
    for (auto it = pendingAddMapList_.cbegin(); it != pendingAddMapList_.cend(); it++) {
        if (it->map == map) {
            return;
        }
    }
    pendingAddMapList_.emplace_back(
        PendingMapRequest {Mapping(map),
                           Manager::instance().scheduler().scheduleIn(
                               [this, map] {
                                   JAMI_WARN("UPnPContext: Add mapping request for %s timed out",
                                             map.toString().c_str());
                                   std::vector<MapCb> cbList;
                                   {
                                       std::lock_guard<std::mutex> lk(mapCbListMutex_);
                                       auto mmIt = mapCbList_.equal_range(map);
                                       for (auto it = mmIt.first; it != mmIt.second; it++)
                                           cbList.emplace_back(it->second.onMapAdded);
                                   }
                                   for (auto const& cb : cbList)
                                       cb(Mapping(map), false);
                                   unregisterAddMappingTimeout(map);
                               },
                               MAP_REQUEST_TIMEOUT)});
}

void
UPnPContext::unregisterAddMappingTimeout(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(pendindRequestMutex_);
    for (auto it = pendingAddMapList_.cbegin(); it != pendingAddMapList_.cend(); it++) {
        if (it->map == map) {
            it->cleanupMapRequest->cancel();
            pendingAddMapList_.erase(it);
            return;
        }
    }
}

void
UPnPContext::registerCallback(const Mapping& map, ControllerData&& ctrlData)
{
    std::lock_guard<std::mutex> lock(mapCbListMutex_);
    auto mmIt = mapCbList_.equal_range(map);
    for (auto it = mmIt.first; it != mmIt.second; it++)
        if (it->second.id == ctrlData.id)
            return;
    JAMI_DBG("[upnp:controller@%ld] registering cb for mapping %s",
             (long) ctrlData.id,
             map.toString().c_str());
    mapCbList_.insert(
        std::make_pair<Mapping, ControllerData>(std::move(Mapping(map)), std::move(ctrlData)));
}

void
UPnPContext::unregisterCallback(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapCbListMutex_);
    auto mmIt = mapCbList_.equal_range(map);
    for (auto it = mmIt.first; it != mmIt.second; it++) {
        if (not it->second.keepCb) {
            JAMI_DBG("[upnp:controller@%ld] unregistering cb for mapping %s",
                     (long) it->second.id,
                     map.toString().c_str());
            mapCbList_.erase(it);
        }
        return;
    }
}
void
UPnPContext::unregisterAllCallbacks(uint64_t ctrlId)
{
    std::lock_guard<std::mutex> lk(mapCbListMutex_);
    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); ++it) {
        if (it->second.id == ctrlId) {
            JAMI_DBG("[upnp:controller@%ld] unregistering cb", (long) ctrlId);
            mapCbList_.erase(it);
            return;
        }
    }
}

} // namespace upnp
} // namespace jami
