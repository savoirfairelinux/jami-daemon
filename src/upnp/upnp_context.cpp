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
#if HAVE_LIBNATPMP
    auto natPmp = std::make_unique<NatPmp>();
    natPmp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2, _3));
    natPmp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2, _3));
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::onIgdListChanged, this, _1, _2, _3, _4));
    natPmp->searchForIgd();
    protocolList_.push_back(std::move(natPmp));
#endif
#if HAVE_LIBUPNP
    auto pupnp = std::make_unique<PUPnP>();
    pupnp->setOnPortMapAdd(std::bind(&UPnPContext::onMappingAdded, this, _1, _2, _3));
    pupnp->setOnPortMapRemove(std::bind(&UPnPContext::onMappingRemoved, this, _1, _2, _3));
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::onIgdListChanged, this, _1, _2, _3, _4));
    pupnp->searchForIgd();
    protocolList_.push_back(std::move(pupnp));
#endif
}

UPnPContext::~UPnPContext()
{
    mapCbList_.clear();
    igdList_.clear();
    protocolList_.clear();
    pendingAddMapList_.clear();
    pendingRmMapList_.clear();
}

bool
UPnPContext::hasValidIgd()
{
    std::lock_guard<std::mutex> lk(igdListMutex_);
    return not igdList_.empty();
}

void
UPnPContext::clearCallbacks(const PortMapLocal& mapList, const std::string& ctrlId)
{
    std::vector<Mapping> mapCbToRm;

    {
        std::lock_guard<std::mutex> lk(cbListMutex_);

        // Make list of all mappings that need to be removed from the callback list.
        for (auto const& map : mapList) {
            for (auto const& mapCb : mapCbList_) {
                if (map.second == mapCb.first) {
                    mapCbToRm.emplace_back(Mapping(map.second.getPortExternal(), map.second.getPortInternal(), map.second.getType(), map.second.isUnique()));
                }
            }
        }
    }

    // Unregister the corresponding callbacks.
    for (auto const& map : mapCbToRm) {
        unregisterCallback(map, ctrlId);
    }
}

void
UPnPContext::addCallback(const Mapping& map, const ControllerData& ctrlData)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    registerCallback(map, ctrlData);
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
UPnPContext::connectivityChanged()
{
    // Lock all the mutexes.
    std::unique_lock<std::mutex> lk1(cbListMutex_);
    std::lock_guard<std::mutex> lk2(igdListMutex_);
    std::lock_guard<std::mutex> lk3(pendindRequestMutex_);

    // Notify controllers that a connectivity change has occured.
    std::vector<ControllerData> ctrlList;
    for (auto& cb : mapCbList_) {
        cb.second.isNotified = false;
        ctrlList.emplace_back(cb.second);
    }

    lk1.unlock();
    for (auto const& ctrl : ctrlList)
        ctrl.cbCnx();
    lk1.lock();

    // Clear all IGDs from the protocols.
    for (auto const& protocol : protocolList_)
        protocol->clearIgds();

    // Clear all IGDs stored by the context.
    if (not igdList_.empty()) {
        igdList_.clear();
    }

    // Clear pending requests
    pendingAddMapList_.clear();
    pendingRmMapList_.clear();

    // Make list of callbacks we don't want to keep through a connectivity change.
    std::vector<Mapping> mapToRemove;
    for (auto const& cb : mapCbList_) {
        if (not cb.second.keepCb)
            mapToRemove.emplace_back(Mapping(cb.first.getPortExternal(), cb.first.getPortInternal(), cb.first.getType()));
    }

    // Only remove registered callbacks that we don't want to keep.
    for (auto& map : mapToRemove)
        mapCbList_.erase(map);

    // Restart the search for new IGDs.
    for (auto const& protocol : protocolList_)
        protocol->searchForIgd();

    if (cleanupIgdDiscovery) {
        cleanupIgdDiscovery.reset();
    }

    // Set a timeout on the IGD search.
    cleanupIgdDiscovery = Manager::instance().scheduler().scheduleIn([this] {
        JAMI_WARN("UPnPContext: Internet gateway device timed out");
        std::unique_lock<std::mutex> lk(cbListMutex_);
        for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
            if (not it->second.isNotified) {
                lk.unlock();
                it->second.cbAdd(it->first, false);
                lk.lock();
            }
        }
    }, IGD_SEARCH_TIMEOUT);
}

void
UPnPContext::requestMappingAdd(const ControllerData& ctrlData, uint16_t portDesired, uint16_t portLocal, PortType type, bool unique)
{
    std::lock_guard<std::mutex> lk1(igdListMutex_);
    std::lock_guard<std::mutex> lk2(pendindRequestMutex_);

    // If no IGD is found yet, register the callback and exit.
    if (igdList_.empty()) {
        JAMI_WARN("UPnP: Trying to add mapping %u:%u %s with no Internet Gateway Device available", portDesired, portLocal, type == upnp::PortType::UDP ? "UDP" : "TCP");
        Mapping map {portDesired, portLocal, type, unique};
        registerAddMappingTimeout(map);
        registerCallback(map, ctrlData);
        return;
    }

    // If the mapping requested is unique, find a mapping that isn't already used.
    if (unique) {

        // Make a list of all currently opened mappings across all IGDs.
        std::vector<uint16_t> currentMappings;
        for (auto const& igd : igdList_) {
            auto globalMappings = igd.second->getCurrentMappingList(type);
            for (auto const& map : *globalMappings) {
                currentMappings.emplace_back(map.second.getPortExternal());
            }
        }

        // Also take in consideration the pending map requests.
        for (auto const& pendingMap : pendingAddMapList_) {
            currentMappings.emplace_back(pendingMap.map.getPortExternal());
        }

        if (std::find(currentMappings.begin(), currentMappings.end(), portDesired) != currentMappings.end()) {
            // Keep searching until you find a unique port.
            bool unique_found = false;
            portDesired = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
            while (not unique_found) {
                if (std::find(currentMappings.begin(), currentMappings.end(), portDesired) != currentMappings.end())
                    portDesired = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
                else {
                    unique_found = true;
                }
            }
        }
    }

    // Register the callback and the pending timeout.
    Mapping map {portDesired, portDesired, type, unique};
    registerAddMappingTimeout(map);
    registerCallback(map, ctrlData);

    // Send out request to open the port to all IGDs.
    for (auto const& igd : igdList_)
        requestMappingAdd(igd.second, portDesired, portDesired, type);
}

void
UPnPContext::onMappingAdded(IpAddr igdIp, Mapping map, bool success)
{
    if (map.isValid()) {
        unregisterAddMappingTimeout(map);
        if (success)
            addMappingToIgd(igdIp, map);
        dispatchOnAddCallback(map, success);
        if (success)
            unregisterCallback(map);
    }
}

void
UPnPContext::requestMappingRemove(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

    if (not igdList_.empty()) {
        for (auto const& igd : igdList_) {
            if (igd.second->isMapInUse(map.getPortExternal())) {
                if (igd.second->getNbOfUsers(map.getPortExternal()) > 1) {
                    igd.second->decrementNbOfUsers(map.getPortExternal());
                } else {
                    igd.first->requestMappingRemove(map);
                }
            }
        }
    }
}

void
UPnPContext::onMappingRemoved(IpAddr igdIp, Mapping map, bool success)
{
    if (map.isValid()) {
        if (success)
            removeMappingFromIgd(igdIp, map);
        dispatchOnRmCallback(map, success);
        if (success)
            unregisterCallback(map);
    }
}

IpAddr
UPnPContext::getExternalIp() const
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

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

bool
UPnPContext::onIgdListChanged(UPnPProtocol* protocol, IGD* igd, IpAddr publicIpAddr, bool added)
{
    if (cleanupIgdDiscovery) {
        cleanupIgdDiscovery->cancel();
        cleanupIgdDiscovery.reset();
    }

    if (added) {
        return addIgdToList(protocol, igd);
    } else {
        if (publicIpAddr)
            return removeIgdFromList(publicIpAddr);
        else
            return removeIgdFromList(igd);
    }
}

IpAddr
UPnPContext::getLocalIp() const
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

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

bool
UPnPContext::addIgdToList(UPnPProtocol* protocol, IGD* igd)
{
    // Check if IGD has a valid public IP.
    if (not igd->publicIp_) {
        JAMI_WARN("UPnPContext: IGD trying to be added has invalid public IpAddress");
        return false;
    }

    {
        // Check if the IGD is in the list.
        std::lock_guard<std::mutex> lk(igdListMutex_);
        if (isIgdInList(igd->publicIp_)) {
            JAMI_DBG("UPnPContext: IGD with public IP %s is already in the list", igd->publicIp_.toString().c_str());
            return false;
        }

        igdList_.emplace_back(protocol, igd);
        JAMI_DBG("UPnP: IGD with public IP %s was added to the list", igd->publicIp_.toString().c_str());
    }

    // Iterate over callback list and dispatch any pending mapping requests
    std::lock_guard<std::mutex> lk2(cbListMutex_);
    for (auto const& cbAdd : mapCbList_) {
        JAMI_DBG("[upnp:controller@%s] sending out request in cb queue for mapping %s", cbAdd.second.memAddr.c_str(), cbAdd.first.toString().c_str());
        registerAddMappingTimeout(cbAdd.first);
        protocol->requestMappingAdd(igd, cbAdd.first.getPortExternal(), cbAdd.first.getPortInternal(), cbAdd.first.getType());
    }

    return true;
}

bool
UPnPContext::removeIgdFromList(IGD* igd)
{
    for(auto it = igdList_.cbegin(); it != igdList_.cend(); it++) {
        if (it->second->publicIp_ == igd->publicIp_) {
            JAMI_WARN("UPnPContext: IGD with public IP %s was removed from the list", it->second->publicIp_.toString().c_str());
            igdList_.erase(it);
            return true;
        }
    }

    return false;
}

bool
UPnPContext::removeIgdFromList(IpAddr publicIpAddr)
{
    for(auto it = igdList_.cbegin(); it != igdList_.cend(); it++) {
        if (it->second->publicIp_ == publicIpAddr) {
            JAMI_WARN("UPnPContext: IGD with public IP %s was removed from the list", it->second->publicIp_.toString().c_str());
            igdList_.erase(it);
            return true;
        }
    }

    return false;
}

UPnPProtocol::Type
UPnPContext::getIgdProtocol(IGD* igd)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

    for (auto const& item : igdList_) {
        if (item.second->publicIp_ == igd->publicIp_) {
            return item.first->getType();
        }
    }

    return UPnPProtocol::Type::UNKNOWN;
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
UPnPContext::addMappingToIgd(IpAddr igdIp, Mapping map)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

    for (auto const& igd : igdList_) {
        if (igd.second->publicIp_ == igdIp) {
            igd.second->addMapInUse(map);
            return;
        }
    }
}

void
UPnPContext::removeMappingFromIgd(IpAddr igdIp, Mapping map)
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
UPnPContext::registerCallback(const Mapping& map, const ControllerData& ctrlData)
{
    // Mutex is already locked.

    for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
        if ((it->first == map) and
            (it->second.memAddr == ctrlData.memAddr)) {
            return;
        }
    }

    JAMI_DBG("[upnp:controller@%s] registering cb for mapping %s", ctrlData.memAddr.c_str(), map.toString().c_str());
    mapCbList_.insert(std::make_pair<Mapping, ControllerData>(std::move(Mapping(map)),
                      ControllerData {ctrlData.memAddr, ctrlData.keepCb, ctrlData.isNotified,
                      std::move(ctrlData.cbAdd), std::move(ctrlData.cbRm), std::move(ctrlData.cbCnx)}));
}

void
UPnPContext::unregisterCallback(const Mapping& map, bool force)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
        if ((it->first) == map) {
            if ((not it->second.keepCb) or (force)) {
                JAMI_DBG("[upnp:controller@%s] unregistering cb for mapping %s",  it->second.memAddr.c_str(), map.toString().c_str());
                mapCbList_.erase(it);
            }
            return;
        }
    }
}

void
UPnPContext::unregisterCallback(const Mapping& map, const std::string& ctrlId)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
        if ((it->first == map) and (it->second.memAddr == ctrlId)) {
            JAMI_DBG("[upnp:controller@%s] unregistering cb for mapping %s", ctrlId.c_str(), map.toString().c_str());
            mapCbList_.erase(it);
            return;
        }
    }
}

void
UPnPContext::dispatchOnAddCallback(Mapping map, bool success)
{
    Mapping mapDispatch(map);
    std::vector<mapCb> cbList;

    {
        std::lock_guard<std::mutex> lk(cbListMutex_);
        for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
            if (it->first == map) {
                if (not it->second.isNotified) {
                    it->second.isNotified = true;
                    cbList.emplace_back(it->second.cbAdd);
                }
            }
        }
    }

    for (auto const& cb : cbList)
        cb(mapDispatch, success);
}

void
UPnPContext::dispatchOnRmCallback(Mapping map, bool success)
{
    Mapping mapDispatch(map);
    std::vector<mapCb> cbList;

    {
        std::lock_guard<std::mutex> lk(cbListMutex_);
        for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
            if (it->first == map) {
                cbList.emplace_back(it->second.cbRm);
            }
        }
    }

    for (auto const& cb : cbList)
        cb(mapDispatch, success);
}

void
UPnPContext::registerAddMappingTimeout(Mapping map)
{
    // Mutex is already locked.

    for (auto it = pendingAddMapList_.cbegin(); it != pendingAddMapList_.cend(); it++) {
        if (it->map == map) {
            return;
        }
    }

    pendingAddMapList_.emplace_back(
        PendingMapRequest{Mapping(map), Manager::instance().scheduler().scheduleIn([this, mapReg = map] {
            JAMI_WARN("UPnPContext: Add mapping request for %s timed out", mapReg.toString().c_str());
            std::vector<mapCb> cbList;
            {
                std::lock_guard<std::mutex> lk(cbListMutex_);
                for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
                    if (it->first == mapReg) {
                        cbList.emplace_back(it->second.cbAdd);
                    }
                }
            }
            for (auto const& cb : cbList)
                cb(Mapping(mapReg), false);
    }, MAP_REQUEST_TIMEOUT)});
}

void
UPnPContext::unregisterAddMappingTimeout(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(pendindRequestMutex_);

    for (auto it = pendingAddMapList_.cbegin(); it != pendingAddMapList_.cend(); it++) {
        if (it->map == map) {
            it->cleanupMapRequest->cancel();
        }
    }

    for (auto it = pendingAddMapList_.cbegin(); it != pendingAddMapList_.cend(); it++) {
        if (it->map == map) {
            pendingAddMapList_.erase(it);
            return;
        }
    }
}

void
UPnPContext::registerRmMappingTimeout(Mapping map)
{
    for (auto it = pendingRmMapList_.cbegin(); it != pendingRmMapList_.cend(); it++) {
        if (it->map == map) {
            return;
        }
    }

    pendingRmMapList_.emplace_back(
        PendingMapRequest{Mapping(map), Manager::instance().scheduler().scheduleIn([this, mapReg = map] {
            JAMI_WARN("UPnPContext: Remove mapping request for %s timed out", mapReg.toString().c_str());
            std::vector<mapCb> cbList;
            {
                std::lock_guard<std::mutex> lk(cbListMutex_);
                for (auto it = mapCbList_.cbegin(); it != mapCbList_.cend(); it++) {
                    if (it->first == mapReg) {
                        cbList.emplace_back(it->second.cbRm);
                    }
                }
            }
            for (auto const& cb : cbList)
                cb(Mapping(mapReg), false);
    }, MAP_REQUEST_TIMEOUT)});
}

void
UPnPContext::unregisterRmMappingTimeout(const Mapping& map)
{
    for (auto it = pendingRmMapList_.cbegin(); it != pendingRmMapList_.cend(); it++) {
        if (it->map == map) {
            it->cleanupMapRequest->cancel();
            pendingRmMapList_.erase(it);
            return;
        }
    }
}

}} // namespace jami::upnp
