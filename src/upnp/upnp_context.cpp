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
    mapCbList_.clear();
}

void
UPnPContext::connectivityChanged()
{
    {
        std::lock_guard<std::mutex> lock(igdListMutex_);
        for (auto const& protocol : protocolList_)
            protocol->clearIgds();
        if (not igdList_.empty()) {
            // Clear main IGD list.
            igdList_.clear();
        }
    }
    
    /*
    NOTE: Normally we should clear all the port mappings and their
          corresponding callbacks once there is a connectivity change.
          However, there are some ports that might've already been opened
          that we would like to reuse once we establish connectivity again
          (DHT and SIP account). Therefore we only remove the callbacks
          to the ports that are NOT to be carried over to the next
          network connection. That way, when a new internet gateway device 
          gets discovered, the upnp context will still be able to reopen
          those ports and will then notify the corresponding services via
          callback.
     */
    static std::vector<Mapping*> mapToRemove {};
    if (not mapToRemove.empty()) {
        mapToRemove.clear();
    }
    for (auto& cb : mapCbList_) {
        if (not cb.second.first)
            mapToRemove.emplace_back(std::move(cb.first));
    }
    for (auto& map : mapToRemove) {
        mapCbList_.erase(map);
    }
    mapToRemove.clear();

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
UPnPContext::chooseRandomPort(const IGD& igd, PortType type)
{
    auto globalMappings = type == PortType::UDP ? &igd.udpMappings : &igd.tcpMappings;

    uint16_t port = generateRandomPort();

    // Keep generating random ports until we find one which is not used.
    while(globalMappings->find(port) != globalMappings->end()) {
        port = generateRandomPort();
    }

    return port;
}

void
UPnPContext::addMapping(NotifyControllerAddMapCallback&& cbAdd, NotifyControllerRemoveMapCallback&& cbRm, uint16_t port_desired, uint16_t port_local, PortType type, bool unique, bool keepCb)
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

    Mapping* mapToAdd = new Mapping(std::move(port_desired),
                                    std::move(port_local),
                                    std::move(type));

    bool callbackAlreadyInList = false;
    // Add callback with corresponding ports to list if it isn't already in the list.
    for (auto& item : mapCbList_) {
        if (item.first->getPortExternal() == mapToAdd->getPortExternal() and
            item.first->getPortInternal() == mapToAdd->getPortInternal() and 
            item.first->getType() == mapToAdd->getType() and 
            &(item.second.second.first) == &cbAdd) {
            callbackAlreadyInList = true;
            break;
        }
    }

    if (not callbackAlreadyInList) {
        mapCbList_.emplace(std::move(mapToAdd),
                           std::make_pair(keepCb, 
                           std::make_pair(std::move(cbAdd), std::move(cbRm))));
    }

    // Add the mapping to the first valid IGD we find in the list.
    IGD* igd = nullptr;
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second) {
                igd = item.second;
                break;
            }
        }
    }

    if (not igd) {
        JAMI_WARN("UPnP: Trying to add mapping with no Internet Gateway Device available");
        return;
    }

    // Get mapping type (UDP/TCP).
    auto globalMappings = type == PortType::UDP ? &igd->udpMappings : &igd->tcpMappings;

    // If we want a unique port, we must make sure the client isn't already using the port.
    if (unique) {
        bool unique_found = false;

        // Keep generating random ports until we find a unique one.
        while (not unique_found) {
            auto iter = globalMappings->find(port_desired);     // Check if that port is not already used by the client.
            if (iter != globalMappings->end()) {
                // Port already in use, try another one.
                JAMI_DBG("Port %d is already in use. Finding another unique port...", port_desired);
                for (auto& cb : mapCbList_) {
                    if (cb.first->getPortExternal() == port_desired) {
                        mapCbList_.erase(cb.first);
                        port_desired = (chooseRandomPort(*igd, type));
                        Mapping* mapToReplace = new Mapping(std::move(port_desired),
                                                            std::move(port_local),
                                                            std::move(type));
                        std::swap(mapCbList_[mapToReplace], cb.second);
                        mapCbList_.erase(cb.first);
                        break;
                    }
                }
            } else {
                unique_found = true;
            }
        }
    }

    UPnPProtocol::UpnpError upnp_err = UPnPProtocol::UpnpError::ERROR_OK;

    // Only request mapping if the IGD in question doesn't contain it yet.
    if (type == upnp::PortType::UDP) {
        PortMapGlobal::iterator it = igd->udpMappings.find(port_desired);
        if (it == igd->udpMappings.end())
            addMapping(igd, port_desired, port_local, type, upnp_err);
    } else {
        PortMapGlobal::iterator it = igd->tcpMappings.find(port_desired);
        if (it == igd->tcpMappings.end())
            addMapping(igd, port_desired, port_local, type, upnp_err);
    }
}

void
UPnPContext::onAddMapping(Mapping* mapping, bool success)
{
    JAMI_WARN("UPnPContext: Port mapping added NOTIFY");
    if (success)
        JAMI_WARN("UPnPContext: Opened port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    else 
        JAMI_WARN("UPnPContext: Failed to open port %s:%s %s", mapping->getPortExternalStr().c_str(), mapping->getPortInternalStr().c_str(), mapping->getTypeStr().c_str());
    
    // Iterate over list of ports to find corresponding callback.
    for (const auto& cb: mapCbList_) {
        if (cb.first->getPortExternal() == mapping->getPortExternal() and
            cb.first->getPortInternal() == mapping->getPortInternal() and
            cb.first->getType() == mapping->getType()) {
            // TODO: add mapping instance to corresponding igd.
            cb.second.second.first(mapping, success);
            if (cb.second.first)
                mapCbList_.erase(mapping);
            return;
        }
    }
}

void
UPnPContext::deleteRmCallbacks(const PortMapLocal& mapList)
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

void
UPnPContext::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error)
{
    // Iterate over the IGD list and call add the mapping with the corresponding protocol.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second == igd) {
                item.first->addMapping(item.second, port_external, port_internal, type, upnp_error);
            }
        }
    }
}

void
UPnPContext::removeMapping(const Mapping& mapping)
{
    // Remove wanted mappings from all IGDs in list.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            item.first->removeMapping(mapping);
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

    if (mapping and success) {
        for (auto& mapToRemove : mapCbList_) {
            if (mapToRemove.first->getPortExternal() == mapping->getPortExternal() and
                mapToRemove.first->getPortInternal() == mapping->getPortInternal() and
                mapToRemove.first->getType() == mapping->getType()) {
                if (mapToRemove.second.second.second)
                    mapToRemove.second.second.second(mapping, true);
                }
        }
    }
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

}} // namespace jami::upnp
