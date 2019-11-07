/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

namespace jami { namespace upnp {

static uint16_t
generateRandomPort(uint16_t min, uint16_t max)
{
    // Seed the generator.
    static std::mt19937 gen(dht::crypto::getSeededRandomEngine());
    // Define the range.
    std::uniform_int_distribution<uint16_t> dist(min, max);
    return dist(gen);
}

std::shared_ptr<UPnPContext>
getUPnPContext()
{
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
}

UPnPContext::~UPnPContext()
{
    igdList_.clear();
    mapProvisionList_.clear();
}

void
UPnPContext::connectivityChanged()
{
    // TODO detect if old IGD still available (if so, do not relaunch)

    // Trigger controllers onConnectionChanged
    {
        std::lock_guard<std::mutex> lk(cbListMutex_);
        for (auto const& ctrl : mapCbList_)
            ctrl.second.onConnectionChanged();
    }

    // Clean stuctures
    for (auto const& protocol : protocolList_)
        protocol->clearIgds();

    {
        std::lock_guard<std::mutex> lk(igdListMutex_);
        if (not igdList_.empty())
            igdList_.clear();
    }

    {
        std::lock_guard<std::mutex> lk(provisionListMutex_);
        // CLear provisioned mappings.
        mapProvisionList_.clear();
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
    if (!globalMappings) return 0;

    uint16_t port = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);

    // Keep generating random ports until we find one which is not used.
    while(globalMappings->find(port) != globalMappings->end()) {
        port = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
    }

    return port;
}

IpAddr
UPnPContext::getLocalIP() const
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

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
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

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

uint16_t
UPnPContext::selectProvisionedPort(upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(provisionListMutex_);
    for (auto it = mapProvisionList_.begin(); it != mapProvisionList_.end(); it++) {
        if ((it->first.getType() == type) and (not it->second)) {
            it->second = true;
            return it->first.getPortExternal();
        }
    }
    return 0;
}
void
UPnPContext::unselectProvisionedPort(uint16_t port, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(provisionListMutex_);
    for (auto it = mapProvisionList_.begin(); it != mapProvisionList_.end(); it++) {
        if (it->first.getType() == type and it->first.getPortExternal() == port) {
            it->second = false;
            return;
        }
    }
}
void
UPnPContext::generateProvisionPorts()
{
    unsigned int provPortsFound = 0;
    std::vector<uint16_t> portList;
    // Generate the provisioned ports.
    while (provPortsFound < NB_PROVISION_PORTS) {
        uint16_t port = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
        if (std::find(portList.begin(), portList.end(), port) == portList.end()) {
            portList.emplace_back(port);
            provPortsFound++;
        }
    }
    std::lock_guard<std::mutex> lk(provisionListMutex_);
    for (int i = 0; i < portList.size(); i++) {
        provisionPortList_[i] = portList[i];
        JAMI_DBG("UPnPContext: provisioning port %u", provisionPortList_[i]);
        std::lock_guard<std::mutex> lk1(igdListMutex_);
        for (auto const& protocol : protocolList_)
            for (auto const& igd : igdList_)
                protocol->requestMappingAdd(igd.second,
                    provisionPortList_[i],
                    provisionPortList_[i],
                    (i%2 == 0)? upnp::PortType::UDP : upnp::PortType::TCP);
    }

}

bool
UPnPContext::isIgdInList(const IpAddr& publicIpAddr)
{
    for (auto const& item : igdList_) {
        if (item.second->publicIp_) {
            if (item.second->publicIp_ == publicIpAddr) {
                return true;
            }
        }
    }
    return false;
}

UPnPProtocol::Type
UPnPContext::getIgdProtocol(IGD* igd)
{
    for (auto const& item : igdList_) {
        if (item.second->publicIp_ == igd->publicIp_) {
            return item.first->getType();
        }
    }

    return UPnPProtocol::Type::UNKNOWN;
}

bool
UPnPContext::igdListChanged(UPnPProtocol* protocol, IGD* igd, IpAddr publicIpAddr, bool added)
{
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);
    if (added) {
        return addIgdToList(protocol, igd);
    } else {
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
    // igdListMutex_ already locked
    // Check if IGD has a valid public IP.
    if (not igd->publicIp_) {
        JAMI_WARN("UPnPContext: IGD trying to be added has invalid public IpAddress");
        return false;
    }
    {
        // Check if the IGD is in the list.
        if (isIgdInList(igd->publicIp_)) {
            JAMI_DBG("UPnPContext: IGD with public IP %s is already in the list", igd->publicIp_.toString().c_str());
            return false;
        }
        igdList_.emplace_back(protocol, igd);
        JAMI_DBG("UPnP: IGD with public IP %s was added to the list", igd->publicIp_.toString().c_str());
    }
    {
        // Iterate over callback list and dispatch any pending mapping requests
        std::lock_guard<std::mutex> lk(cbListMutex_);
        for (auto const& cbAdd : mapCbList_) {
            JAMI_DBG("[upnp:controller@%ld] sending out request in cb queue for mapping %s", (long)cbAdd.second.id, cbAdd.first.toString().c_str());
            registerAddMappingTimeout(cbAdd.first);
            protocol->requestMappingAdd(igd, cbAdd.first.getPortExternal(), cbAdd.first.getPortInternal(), cbAdd.first.getType());
        }
    }
    return true;
}

bool
UPnPContext::removeIgdFromList(IGD* igd)
{
    // igdListMutex_ already locked
    auto it = igdList_.begin();
    while (it != igdList_.end()) {
        if (it->second->publicIp_ == igd->publicIp_) {
            JAMI_WARN("UPnPContext: IGD with public IP %s was removed from the list", it->second->publicIp_.toString().c_str());
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
    // igdListMutex_ already locked
    auto it = igdList_.begin();
    while (it != igdList_.end()) {
        if (it->second->publicIp_ == publicIpAddr) {
            JAMI_WARN("UPnPContext: IGD with public IP %s was removed from the list", it->second->publicIp_.toString().c_str());
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
    std::lock_guard<std::mutex> lk(igdListMutex_);
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
    std::lock_guard<std::mutex> lk(igdListMutex_);
    for (auto const& igd : igdList_) {
        igd.second->incrementNbOfUsers(portDesired, type);
    }
}

void
UPnPContext::requestMappingAdd(ControllerData&& ctrlData, uint16_t portDesired, uint16_t portLocal, PortType type, bool unique)
{
    std::lock_guard<std::mutex> lk1(igdListMutex_);
    std::lock_guard<std::mutex> lk2(pendindRequestMutex_);
    // If no IGD is found yet, register the callback and exit.
    if (igdList_.empty()) {
        JAMI_WARN("UPnP: Trying to add mapping %u:%u %s with no Internet Gateway Device available", portDesired, portLocal, type == upnp::PortType::UDP ? "UDP" : "TCP");
        Mapping map {portDesired, portLocal, type, unique};
        registerAddMappingTimeout(map);
        registerCallback(map, std::move(ctrlData));
        return;
    }
    // Vector of mappings that cannot be used.
    std::vector<uint16_t> unavailableMappings;
    // Wether the port requested is unique or not, it cannot be a port that was
    // already provisioned.
    {
        std::lock_guard<std::mutex> lk(provisionListMutex_);

        for (unsigned int i = 0; i < provisionPortList_.size(); i++)
            unavailableMappings.emplace_back(provisionPortList_[i]);
    }

    // If the mapping requested is unique, then all current mappings are unavailable.
    if (unique) {
        // Make a list of all currently opened mappings across all IGDs.
        for (auto const& igd : igdList_) {
            auto globalMappings = igd.second->getCurrentMappingList(type);
            for (auto const& map : *globalMappings) {
                unavailableMappings.emplace_back(map.second.getPortExternal());
            }
        }
        // Also take in consideration the pending map requests.
        for (auto const& pendingMap : pendingAddMapList_)
            unavailableMappings.emplace_back(pendingMap.map.getPortExternal());
    }
    // Find a valid port.
    if (std::find(unavailableMappings.begin(), unavailableMappings.end(), portDesired) != unavailableMappings.end()) {
        // Keep searching until you find a valid port.
        bool unique_found = false;
        portDesired = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
        while (not unique_found) {
            if (std::find(unavailableMappings.begin(), unavailableMappings.end(), portDesired) != unavailableMappings.end())
                portDesired = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
            else {
                unique_found = true;
            }
        }
    }
    // Register the callback and the pending timeout.
    Mapping map {portDesired, portDesired, type, unique};
    registerAddMappingTimeout(map);
    registerCallback(map, std::move(ctrlData));
    // Send out request to open the port to all IGDs.
    for (auto const& igd : igdList_)
        requestMappingAdd(igd.second, portDesired, portDesired, type);
}

void
UPnPContext::requestMappingAdd(IGD* igd, uint16_t portExternal, uint16_t portInternal, PortType type)
{
    // Iterate over the IGD list and call add the mapping with the corresponding protocol.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second == igd) {
                item.first->requestMappingAdd(item.second, portExternal, portInternal, type);
                return;
            }
        }
    }
}

void
UPnPContext::onMappingAdded(IpAddr igdIp, const Mapping& map, bool success)
{
    if (map.isValid()) {
        unregisterAddMappingTimeout(map);
        if (success) {
            addMappingToIgd(igdIp, map);
            registerProvisionedMapping(map);    // Will only register if the mapping that was added was a provisioned port
        }
        dispatchOnAddCallback(map, success);
        if (success)
            unregisterCallback(map);
    }
}

void
UPnPContext::addMappingToIgd(IpAddr igdIp, const Mapping& map)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);
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
        std::lock_guard<std::mutex> lk(cbListMutex_);
        auto mmIt = mapCbList_.equal_range(map);
        for (auto it = mmIt.first; it != mmIt.second; it++)
            cbList.emplace_back(it->second.onMapAdded);
    }
    for (auto const& cb : cbList)
        cb(map, success);
}

void
UPnPContext::requestMappingRemove(const Mapping& map)
{

    std::lock_guard<std::mutex> lk(igdListMutex_);
    if (not igdList_.empty()) {
        for (auto const& igd : igdList_) {
            if (igd.second->isMapInUse(map)) {
                if (igd.second->getNbOfUsers(map) > 1) {
                    igd.second->decrementNbOfUsers(map);
                } else {
                    igd.first->requestMappingRemove(map);
                }
            }
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
    std::lock_guard<std::mutex> lk(igdListMutex_);
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
        std::lock_guard<std::mutex> lk(cbListMutex_);
        for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
            if (it->first == map) {
                cbList.emplace_back(it->second.onMapRemoved);
            }
        }
    }
    for (auto const& cb : cbList)
        cb(map, success);
}

void
UPnPContext::registerProvisionedMapping(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(provisionListMutex_);
    if (std::find(provisionPortList_.begin(), provisionPortList_.end(), map.getPortExternal()) != provisionPortList_.end()) {
        mapProvisionList_.insert(std::make_pair<Mapping, bool>(Mapping(map), false));
    }
}
void
UPnPContext::unregisterProvisionedMapping(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(provisionListMutex_);
    if (std::find(provisionPortList_.begin(), provisionPortList_.end(), map.getPortExternal()) != provisionPortList_.end()) {
        mapProvisionList_.erase(map);
    }
}

void
UPnPContext::registerAddMappingTimeout(const Mapping& map)
{
    // Mutex is already locked.
    for (auto it = pendingAddMapList_.cbegin(); it != pendingAddMapList_.cend(); it++) {
        if (it->map == map) {
            return;
        }
    }
    pendingAddMapList_.emplace_back(PendingMapRequest {
        Mapping(map), Manager::instance().scheduler().scheduleIn([this, map] {
            JAMI_WARN("UPnPContext: Add mapping request for %s timed out", map.toString().c_str());
            std::vector<MapCb> cbList;
            {
                std::lock_guard<std::mutex> lk(cbListMutex_);
                auto mmIt = mapCbList_.equal_range(map);
                for (auto it = mmIt.first; it != mmIt.second; it++)
                    cbList.emplace_back(it->second.onMapAdded);
            }
            for (auto const& cb : cbList)
                cb(Mapping(map), false);
            unregisterAddMappingTimeout(map);
    }, MAP_REQUEST_TIMEOUT)});
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
    // Mutex is already locked.
    auto mmIt = mapCbList_.equal_range(map);
    for (auto it = mmIt.first; it != mmIt.second; it++)
        if (it->second.id == ctrlData.id)
            return;
    JAMI_DBG("[upnp:controller@%ld] registering cb for mapping %s", (long)ctrlData.id, map.toString().c_str());
    mapCbList_.insert(std::make_pair<Mapping, ControllerData>(std::move(Mapping(map)), std::move(ctrlData)));
}

void
UPnPContext::unregisterCallback(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);
    auto mmIt = mapCbList_.equal_range(map);
    for (auto it = mmIt.first; it != mmIt.second; it++) {
        if (not it->second.keepCb) {
            JAMI_DBG("[upnp:controller@%ld] unregistering cb for mapping %s", (long)it->second.id, map.toString().c_str());
            mapCbList_.erase(it);
        }
        return;
    }
}
void
UPnPContext::unregisterAllCallbacks(uint64_t ctrlId)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);
    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); ++it) {
        if (it->second.id == ctrlId) {
            JAMI_DBG("[upnp:controller@%ld] unregistering cb", (long)ctrlId);
            mapCbList_.erase(it);
            return;
        }
    }
}

}} // namespace jami::upnp
