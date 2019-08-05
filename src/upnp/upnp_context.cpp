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
    natPmp->setOnPortMapAdd(std::bind(&UPnPContext::onAddMapping, this, _1, _2, _3));
    natPmp->setOnPortMapRemove(std::bind(&UPnPContext::onRemoveMapping, this, _1, _2, _3));
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    natPmp->searchForIgd();
    protocolList_.push_back(std::move(natPmp));
#endif
#if HAVE_LIBUPNP
    auto pupnp = std::make_unique<PUPnP>();
    pupnp->setOnPortMapAdd(std::bind(&UPnPContext::onAddMapping, this, _1, _2, _3));
    pupnp->setOnPortMapRemove(std::bind(&UPnPContext::onRemoveMapping, this, _1, _2, _3));
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
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
    std::unique_lock<std::mutex> lk(cbListMutex_);

    static std::vector<Mapping> mapCbToRm;
    if (not mapCbToRm.empty())
        mapCbToRm.clear();

    // Make list of all mappings that need to be removed from the callback list.
    for (auto& map : mapList) {
        for (auto& mapCb : mapCbList_) {
            if (map.second == *(mapCb.first)) {
                mapCbToRm.emplace_back(Mapping(map.second.getPortExternal(), map.second.getPortInternal(), map.second.getType(), map.second.isUnique()));
            }
        }
    }

    // Unregister the corresponding callbacks.
    for (auto& map : mapCbToRm) {
        lk.unlock();
        unregisterCallback(Mapping(map.getPortExternal(), map.getPortInternal(), map.getType(), map.isUnique()), ctrlId);
        lk.lock();
    }
}

void
UPnPContext::addCallback(Mapping map, pControllerData ctrlData)
{
    registerCallback(std::move(map), std::move(ctrlData));
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
    std::lock_guard<std::mutex> lk1(cbListMutex_);
    std::lock_guard<std::mutex> lk2(igdListMutex_);
    std::lock_guard<std::mutex> lk3(pendindRequestMutex_);

    // Notify controllers that a connectivity change has occured.
    for (auto& cb : mapCbList_) {
        cb.second.get()->isNotified = false;
        cb.second.get()->cbCnx();
    }

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
    static std::vector<Mapping*> mapToRemove {};
    if (not mapToRemove.empty())
        mapToRemove.clear();
    for (auto& cb : mapCbList_) {
        if (not cb.second.get()->keepCb)
            mapToRemove.emplace_back(std::move(cb.first));
    }

    // Only remove registered callbacks that we don't want to keep.
    for (auto& map : mapToRemove)
        mapCbList_.erase(map);
    mapToRemove.clear();

    // Restart the search for new IGDs.
    for (auto const& protocol : protocolList_)
        protocol->searchForIgd();
}

void
UPnPContext::addMapping(pControllerData ctrlData, uint16_t portDesired, uint16_t portLocal, PortType type, bool unique)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

    // If no IGD is found yet, register the callback and exit.
    if (igdList_.empty()) {
        JAMI_WARN("UPnP: Trying to add mapping %u:%u %s with no Internet Gateway Device available", portDesired, portLocal, type == upnp::PortType::UDP ? "UDP" : "TCP");
        registerAddMappingTimeout(Mapping(portDesired, portLocal, type, unique));
        registerCallback(Mapping(portDesired, portLocal, type, unique), std::move(ctrlData));
        return;
    }

    // If the mapping requested is unique, find a mapping that isn't already used.
    if (unique) {
        static std::vector<uint16_t> currentMappings {};
        if (not currentMappings.empty()) {
            currentMappings.clear();
        }

        // Make a list of all currently used mappings across all IGDs.
        for (auto const& igd : igdList_) {
            auto globalMappings = igd.second->getCurrentMappingList(type);    
            for (auto const& map : *globalMappings) {
                currentMappings.emplace_back(map.second.getPortExternal());
            }
        }

        // Keep searching until you find a unique port.
        bool unique_found = false;
        while (not unique_found) {
            if (std::find(currentMappings.begin(), currentMappings.end(), portDesired) != currentMappings.end())
                portDesired = generateRandomPort(upnp::Mapping::UPNP_PORT_MIN, upnp::Mapping::UPNP_PORT_MAX);
            else
                unique_found = true;
        }
    }

    // Register the callback and the pending timeout.
    registerAddMappingTimeout(Mapping(portDesired, portDesired, type, unique));
    registerCallback(Mapping(portDesired, portDesired, type, unique), std::move(ctrlData));

    // Send out request to open the port to all IGDs.
    for (auto const& igd : igdList_)
        addMapping(igd.second, portDesired, portDesired, type);
}

void
UPnPContext::onAddMapping(IpAddr igdIp, Mapping* mapping, bool success)
{
    if (mapping) {
        unregisterAddMappingTimeout(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
        if (success)
            addMappingToIgd(igdIp, Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
        dispatchOnAddCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()), success);
        if (success)
            unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
    }
}

void
UPnPContext::removeMapping(const Mapping& mapping)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

    if (not igdList_.empty()) {
        for (auto const& igd : igdList_) {
            if (igd.second->isMapInUse(mapping.getPortExternal())) {
                if (igd.second->getNbOfUsers(mapping.getPortExternal()) > 1) {
                    igd.second->decrementNbOfUsers(mapping.getPortExternal());
                } else {
                    igd.first->removeMapping(mapping);
                }
            }
        }
    }
}

void
UPnPContext::onRemoveMapping(IpAddr igdIp, Mapping* mapping, bool success)
{
    if (mapping) {
        if (success)
            removeMappingFromIgd(igdIp, Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
        dispatchOnRmCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()), success);
        if (success)
            unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType(), mapping->isUnique()));
    }
}

IpAddr
UPnPContext::getExternalIP() const
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

IpAddr
UPnPContext::getLocalIP() const
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
    std::lock_guard<std::mutex> lk(igdListMutex_);

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
UPnPContext::igdListChanged(UPnPProtocol* protocol, IGD* igd, IpAddr publicIpAddr, bool added)
{
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
    std::unique_lock<std::mutex> lk1(igdListMutex_);

    // Check if IGD has a valid public IP.
    if (not igd->publicIp_) {
        JAMI_WARN("UPnPContext: IGD trying to be added has invalid public IpAddress");
        return false;
    }

    // Check if the IGD is in the list.
    lk1.unlock();
    if (isIgdInList(igd->publicIp_)) {
        JAMI_DBG("UPnPContext: IGD with public IP %s is already in the list", igd->publicIp_.toString().c_str());
        lk1.lock();
        return false;
    }
    lk1.lock();

    igdList_.emplace_back(protocol, igd);
    JAMI_DBG("UPnP: IGD with public IP %s was added to the list", igd->publicIp_.toString().c_str());
    
    // Iterate over callback list and dispatch any pending mapping requests
    std::lock_guard<std::mutex> lk2(cbListMutex_);
    for (auto& cbAdd : mapCbList_) {
        JAMI_DBG("UPnPContext: Sending out request in cb queue for mapping %s of controller %s", cbAdd.first->toString().c_str(), cbAdd.second.get()->memAddr.c_str());
        protocol->addMapping(igd, cbAdd.first->getPortExternal(), cbAdd.first->getPortInternal(), cbAdd.first->getType());
    }

    return true;
}

bool
UPnPContext::removeIgdFromList(IGD* igd)
{
    std::lock_guard<std::mutex> lk(igdListMutex_);

    for(auto it = igdList_.begin(); it != igdList_.end(); it++) {
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
    std::lock_guard<std::mutex> lk(igdListMutex_);

    for(auto it = igdList_.begin(); it != igdList_.end(); it++) {
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
UPnPContext::addMapping(IGD* igd, uint16_t portExternal, uint16_t portInternal, PortType type)
{
    // Iterate over the IGD list and call add the mapping with the corresponding protocol.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second == igd) {
                item.first->addMapping(item.second, portExternal, portInternal, type);
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
            igd.second->addMapInUse(Mapping(map.getPortExternal(), map.getPortInternal(), map.getType(), map.isUnique()));
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
            igd.second->removeMapInUse(Mapping(map.getPortExternal(), map.getPortInternal(), map.getType(), map.isUnique()));
            return;
        }
    }
}

void
UPnPContext::registerCallback(Mapping map, pControllerData ctrlData)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if ((*(it->first) == map) and 
            (it->second.get()->memAddr == ctrlData.get()->memAddr)) {
            return;
        }
    } 

    JAMI_DBG("UPnPContext: Registering cb for mapping %s of %s", map.toString().c_str(), ctrlData.get()->memAddr.c_str());
    mapCbList_.emplace(std::move(new Mapping(map.getPortExternal(), map.getPortInternal(), map.getType(), map.isUnique())), 
                       std::make_unique<ControllerData>(ControllerData {
                       ctrlData->memAddr, ctrlData->keepCb, ctrlData->isNotified,
                       std::move(ctrlData->cbAdd), std::move(ctrlData->cbRm), std::move(ctrlData->cbCnx)}));
}

void
UPnPContext::unregisterCallback(Mapping map, bool force)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (*(it->first) == map) {
            if ((not it->second.get()->keepCb) or (force)) {
                JAMI_DBG("UPnPContext: Unregistering cb for mapping %s", map.toString().c_str());
                mapCbList_.erase(it);
            }
            return;
        }
    }
}

void
UPnPContext::unregisterCallback(Mapping map, const std::string& ctrlId)
{
    std::lock_guard<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if ((*(it->first) == map) and (it->second.get()->memAddr == ctrlId)) {
            JAMI_DBG("UPnPContext: Unregistering cb for mapping %s of controller %s", map.toString().c_str(), ctrlId.c_str());
            mapCbList_.erase(it);
            return;
        }
    }
}

void
UPnPContext::dispatchOnAddCallback(Mapping map, bool success)
{
    std::unique_lock<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (*(it->first) == map) {
            if (not it->second.get()->isNotified) {
                it->second.get()->isNotified = true;
                lk.unlock();
                it->second.get()->cbAdd(it->first, success);
                lk.lock();
            }
        }
    }
    lk.unlock();
}

void
UPnPContext::dispatchOnRmCallback(Mapping map, bool success)
{
    std::unique_lock<std::mutex> lk(cbListMutex_);

    for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (*(it->first) == map) {
            lk.unlock();
            it->second.get()->cbRm(it->first, success);
            lk.lock();
        }
    }
    lk.unlock();
}

void
UPnPContext::registerAddMappingTimeout(Mapping map)
{
    std::lock_guard<std::mutex> lk(pendindRequestMutex_);
    
    for (auto it = pendingAddMapList_.begin(); it != pendingAddMapList_.end(); it++) {
        if (it->map == map) {
            return;
        }
    }

    pendingAddMapList_.emplace_back(
        PendingMapRequest{Mapping(map.getPortExternal(), map.getPortInternal(), map.getType(), map.isUnique()),
        Manager::instance().scheduler().scheduleIn(
            [this, portExt = map.getPortExternal(), portInt = map.getPortInternal(), type = map.getType()] {
            JAMI_WARN("UPnPContext: Add mapping request for %u:%u %s timed out", portExt, portInt, type == upnp::PortType::UDP ? "UDP" : "TCP");
            std::unique_lock<std::mutex> lk(cbListMutex_);
            for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
                if (*(it->first) == Mapping(portExt, portInt, type)) {
                    lk.unlock();
                    it->second.get()->cbAdd(it->first, false);
                    lk.lock();
                }
            }

    }, MAP_REQUEST_TIMEOUT)});
}

void
UPnPContext::unregisterAddMappingTimeout(Mapping map)
{
    std::lock_guard<std::mutex> lk(pendindRequestMutex_);

    for (auto it = pendingAddMapList_.begin(); it != pendingAddMapList_.end(); it++) {
        if (it->map == map) {
            it->cleanupTask->cancel();
        }
    }

    for (auto it = pendingAddMapList_.begin(); it != pendingAddMapList_.end(); it++) {
        if (it->map == map) {
            pendingAddMapList_.erase(it);
            return;
        }
    }
}

void
UPnPContext::registerRmMappingTimeout(Mapping map)
{
    std::lock_guard<std::mutex> lk(pendindRequestMutex_);

    pendingRmMapList_.emplace_back(
        PendingMapRequest{Mapping(map.getPortExternal(), map.getPortInternal(), map.getType()),
        Manager::instance().scheduler().scheduleIn(
            [this, portExt = map.getPortExternal(), portInt = map.getPortInternal(), type = map.getType()] {
            JAMI_WARN("UPnPContext: Remove mapping request for %u:%u %s timed out", portExt, portInt, type == upnp::PortType::UDP ? "UDP" : "TCP");
            std::unique_lock<std::mutex> lk(cbListMutex_);
            for (auto it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
                if (*(it->first) == Mapping(portExt, portInt, type)) {
                    lk.unlock();
                    it->second.get()->cbRm(it->first, false);
                    lk.lock();
                }
            }
    }, MAP_REQUEST_TIMEOUT)});
}

void
UPnPContext::unregisterRmMappingTimeout(Mapping map)
{
    std::lock_guard<std::mutex> lk(pendindRequestMutex_);

    for (auto it = pendingRmMapList_.begin(); it != pendingRmMapList_.end(); it++) {
        if (it->map == map) {
            it->cleanupTask->cancel();
            pendingRmMapList_.erase(it);
            return;
        }
    }
}

}} // namespace jami::upnp
