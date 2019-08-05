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
generateRandomPort()
{
    // Seed the generator.
    static std::mt19937 gen(dht::crypto::getSeededRandomEngine());

    // Define the range.
    std::uniform_int_distribution<uint16_t> dist(Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX);

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
    natPmp->setOnPortMapAdd(std::bind(&UPnPContext::onAddMapping, this, _1, _2));
    natPmp->setOnPortMapRemove(std::bind(&UPnPContext::onRemoveMapping, this, _1, _2));
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    natPmp->searchForIgd();
    protocolList_.push_back(std::move(natPmp));
#endif
#if HAVE_LIBUPNP
    auto pupnp = std::make_unique<PUPnP>();
    pupnp->setOnPortMapAdd(std::bind(&UPnPContext::onAddMapping, this, _1, _2));
    pupnp->setOnPortMapRemove(std::bind(&UPnPContext::onRemoveMapping, this, _1, _2));
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    pupnp->searchForIgd();
    protocolList_.push_back(std::move(pupnp));
#endif
}

UPnPContext::~UPnPContext()
{
    igdList_.clear();
    protocolList_.clear();
    mapCbList_.clear();
}

bool
UPnPContext::hasValidIgd()
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    return not igdList_.empty();
}

void
UPnPContext::clearCallbacks(const PortMapLocal& mapList)
{
    for (auto& map : mapList) {
        for (auto& mapCb : mapCbList_) {
            if (map.second.getPortExternal() == mapCb.first->getPortExternal() and
                map.second.getPortInternal() == mapCb.first->getPortInternal() and
                map.second.getType() == mapCb.first->getType()) {
                JAMI_WARN("UPnPContext: Removing callback for %s:%s %s", 
                                map.second.getPortExternalStr().c_str(),
                                map.second.getPortInternalStr().c_str(),
                                map.second.getType() == upnp::PortType::UDP ? "UDP" : "TCP");
                mapCbList_.erase(mapCb.first);
            }
        }
    }
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

void
UPnPContext::connectivityChanged()
{
    {
        std::lock_guard<std::mutex> lock(igdListMutex_);
        for (auto const& protocol : protocolList_)
            protocol->clearIgds();
        if (not igdList_.empty()) {
            igdList_.clear();
        }
    }
    
    // Normally we should clear all the port mappings and their
    // corresponding callbacks once there is a connectivity change.
    // However, there are some ports that might've already been opened
    // that we would like to reuse once we establish connectivity again
    // (DHT and SIP account). Therefore we only remove the callbacks
    // to the ports that are NOT to be carried over to the next
    // network connection. That way, when a new internet gateway device
    // gets discovered, the upnp context will still be able to reopen
    // those ports and will then notify the corresponding services via
    // callback.
    static std::vector<Mapping*> mapToRemove {};
    if (not mapToRemove.empty())
        mapToRemove.clear();
    for (auto& cb : mapCbList_) {
        if (not cb.second.first)
            mapToRemove.emplace_back(std::move(cb.first));
    }
    for (auto& map : mapToRemove)
        mapCbList_.erase(map);
    mapToRemove.clear();

    for (auto const& protocol : protocolList_)
        protocol->searchForIgd();
}

void
UPnPContext::addMapping(NotifyControllerCallback&& cbAdd, NotifyControllerCallback&& cbRm, uint16_t portDesired, uint16_t portLocal, PortType type, bool unique, bool keepCb)
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

    // If no IGD is found yet, register the callback and exit.
    if (igdList_.empty()) {
        JAMI_WARN("UPnP: Trying to add mapping with no Internet Gateway Device available");
        registerCallback(Mapping(portDesired, portLocal, type), keepCb, std::move(cbAdd), std::move(cbRm));
        return;    
    }

    if (unique) {
        static std::vector<uint16_t> currentMappings {};
        if (not currentMappings.empty()) {
            currentMappings.clear();
        }

        // Make a list of all currently used mappings across all IGDs.
        for (auto const& igd : igdList_) {
            auto& globalMappings = type == PortType::UDP ? igd.second->udpMappings_ : igd.second->tcpMappings_;    
            for (auto const& map : globalMappings) {
                currentMappings.emplace_back(map.second.getPortExternal());
            }
        }

        // Keep searching until you find a unique port.
        bool unique_found = false;
        while (not unique_found) {
            if (std::find(currentMappings.begin(), currentMappings.end(), portDesired) != currentMappings.end()) {
                portDesired = generateRandomPort();
            } else {
                unique_found = true;
            }
        }
    }

    registerCallback(Mapping(portDesired, portDesired, type), keepCb, std::move(cbAdd), std::move(cbRm));

    UPnPProtocol::UpnpError upnp_err = UPnPProtocol::UpnpError::ERROR_OK;
    for (auto const& igd : igdList_)
        addMapping(igd.second, portDesired, portDesired, type, upnp_err);
}

void
UPnPContext::onAddMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("UPnPContext: Port mapping added NOTIFY");
    if (success)
        JAMI_WARN("UPnPContext: Opened port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("UPnPContext: Failed to open port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    
    if (mapping) {
        dispatchOnAddCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()), success);
        unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()));
    }
}

void
UPnPContext::removeMapping(const Mapping& mapping)
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

    // Remove wanted mappings from all IGDs in list.
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
UPnPContext::onRemoveMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("UPnPContext: Port mapping removed NOTIFY");
    if (success)
        JAMI_WARN("UPnPContext: Closed port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("UPnPContext: Failed to close port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    
    // Notify all controllers.
    if (mapping and success) {

        Mapping map {std::move(mapping->getPortExternal()),
                     std::move(mapping->getPortInternal()),
                     std::move(mapping->getType())};
        
        for (auto const& item : igdList_) {
            if (not item.second)
                continue;

            auto istanceMapppings = mapping->getType() == upnp::PortType::UDP ? 
                                    &item.second->udpMappings_ : &item.second->tcpMappings_;
            auto mapToRemove = istanceMapppings->find(map.getPortExternal());
            if (mapToRemove != istanceMapppings->end()) {
                // Check if the mapping we want to remove is the same as the one that is present.
                GlobalMapping& globalMap = mapToRemove->second;
                if (map == globalMap) {
                    istanceMapppings->erase(map.getPortExternal());
                }
            }
        }

        dispatchOnRmCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()), success);
        unregisterCallback(Mapping(mapping->getPortExternal(), mapping->getPortInternal(), mapping->getType()));
    }
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
    std::unique_lock<std::mutex> igdListLock(igdListMutex_);

    // Check if IGD has a valid public IP.
    if (not igd->publicIp_) {
        JAMI_WARN("UPnPContext: IGD trying to be added has invalid public IpAddress");
        return false;
    }

    if (isIgdInList(igd->publicIp_)) {
        JAMI_DBG("UPnPContext: IGD with public IP %s is already in the list", igd->publicIp_.toString().c_str());
        return false;
    }

    igdList_.emplace_back(protocol, igd);
    JAMI_DBG("UPnP: IGD with public IP %s was added to the list", igd->publicIp_.toString().c_str());
    
    igdListLock.unlock();
    for (auto& cbAdd : mapCbList_) {
        // TODO: fix forced unique port passed as parameter.
        addMapping(std::move(cbAdd.second.second.first),
                   std::move(cbAdd.second.second.second),
                   cbAdd.first->getPortExternal(), cbAdd.first->getPortInternal(), cbAdd.first->getType(), false);
    }
    igdListLock.lock();

    return true;
}

bool
UPnPContext::removeIgdFromList(IGD* igd)
{
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

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
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

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

void
UPnPContext::addMapping(IGD* igd, uint16_t portExternal, uint16_t portInternal, PortType type, UPnPProtocol::UpnpError& upnpError)
{
    // Iterate over the IGD list and call add the mapping with the corresponding protocol.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second == igd) {
                item.first->addMapping(item.second, portExternal, portInternal, type, upnpError);
            }
        }
    }
}

void
UPnPContext::registerCallback(Mapping map, bool keepCb, NotifyControllerCallback&& cbAdd, NotifyControllerCallback&& cbRm)
{
    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first->getPortExternal() == map.getPortExternal() and
            it->first->getPortInternal() == map.getPortInternal() and 
            it->first->getType() == map.getType() and
            &(it->second.second.first) == &cbAdd) {
            return;
        }
    }
    mapCbList_.emplace(std::move(new Mapping(map.getPortExternal(), map.getPortInternal(), map.getType())),
                       std::make_pair(keepCb, 
                       std::make_pair(std::move(cbAdd), std::move(cbRm))));
}

void
UPnPContext::unregisterCallback(Mapping map)
{

    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first->getPortExternal() == map.getPortExternal() and
            it->first->getPortInternal() == map.getPortInternal() and 
            it->first->getType() == map.getType()) {
            if (not it->second.first) {
                mapCbList_.erase(it);
            }
            return;
        }
    }
}

void
UPnPContext::dispatchOnAddCallback(Mapping map, bool success)
{
    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first->getPortExternal() == map.getPortExternal() and
            it->first->getPortInternal() == map.getPortInternal() and
            it->first->getType() == map.getType()) {
            it->second.second.first(it->first, success);
        }
    }
}

void
UPnPContext::dispatchOnRmCallback(Mapping map, bool success)
{
    // Notify all controllers.
    for (cbMapItr it = mapCbList_.begin(); it != mapCbList_.end(); it++) {
        if (it->first->getPortExternal() == map.getPortExternal() and
            it->first->getPortInternal() == map.getPortInternal() and
            it->first->getType() == map.getType()) {
            if (it->second.second.second)
                it->second.second.second(it->first, success);
        }
    }
}

}} // namespace jami::upnp
