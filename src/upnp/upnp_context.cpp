/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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
    natPmp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    natPmp->searchForIGD();
    protocolList_.push_back(std::move(natPmp));
#endif
#if HAVE_LIBUPNP
    auto pupnp = std::make_unique<PUPnP>();
    pupnp->setOnIgdChanged(std::bind(&UPnPContext::igdListChanged, this, _1, _2, _3, _4));
    pupnp->searchForIGD();
    protocolList_.push_back(std::move(pupnp));
#endif
}

UPnPContext::~UPnPContext()
{
    igdList_.clear();
}

void
UPnPContext::connectivityChanged()
{
    if (not igdList_.empty()) {

        // Clear main IGD list.
        std::lock_guard<std::mutex> lock(igdListMutex_);
        igdList_.clear();

        for (const auto& item : igdListeners_) {
            item.second();
        }
        
        for (auto const& item : protocolList_) {
            item->connectivityChanged();
            item->searchForIGD();
        }
    }
}

bool
UPnPContext::hasValidIGD()
{
    return not igdList_.empty();
}

size_t
UPnPContext::addIGDListener(IgdFoundCallback&& cb)
{
   JAMI_DBG("UPnP Context: Adding IGD listener.");

    std::lock_guard<std::mutex> lock(igdListMutex_);
    auto token = ++listenerToken_;
    igdListeners_.emplace(token, std::move(cb));

    return token;
}

void
UPnPContext::removeIGDListener(size_t token)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    if (igdListeners_.erase(token) > 0) {
        JAMI_DBG("UPnP Context: Removing igd listener.");
    }
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

Mapping
UPnPContext::addMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool unique)
{
    // Lock mutex on the igd list.
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

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
        JAMI_WARN("UPnPContext: no valid IGD available");
        return {};
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
                port_desired = chooseRandomPort(*igd, type);    // Port already used, try another one.
                JAMI_DBG("Port %d is already in use. Finding another unique port...", port_desired);
            } else {
                unique_found = true;
            }
        }
    }

    UPnPProtocol::UpnpError upnp_err = UPnPProtocol::UpnpError::ERROR_OK;
    unsigned numberRetries = 0;

    Mapping mapping = addMapping(igd, port_desired, port_local, type, upnp_err);

    while (not mapping and
           upnp_err == UPnPProtocol::UpnpError::CONFLICT_IN_MAPPING and
           numberRetries < MAX_RETRIES) {

        port_desired = chooseRandomPort(*igd, type);

        upnp_err = UPnPProtocol::UpnpError::ERROR_OK;
        mapping = addMapping(igd, port_desired, port_local, type, upnp_err);
        ++numberRetries;
    }

    if (not mapping and numberRetries >= MAX_RETRIES) {
        JAMI_ERR("UPnPContext: Could not add mapping after %u retries, giving up.", MAX_RETRIES);
    }

    return mapping;
}

Mapping
UPnPContext::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error)
{
    // Iterate over the IGD list and call add the mapping with the corresponding protocol.
    if (not igdList_.empty()) {
        for (auto const& item : igdList_) {
            if (item.second == igd) {
                return item.first->addMapping(item.second, port_external, port_internal, type, upnp_error);
            }
        }
    }

    return {};
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

    JAMI_WARN("UPnP: no valid IGD available");
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

    JAMI_WARN("UPnP: no valid IGD available");
    return {};
}

bool
UPnPContext::isIgdInList(IGD* igd)
{
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

    for (auto const& item : igdList_) {
        if (item.second->publicIp_ == igd->publicIp_) {
            return true;
        }
    }

    return false;
}

bool
UPnPContext::isIgdInList(IpAddr publicIpAddr)
{
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

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
    if (isIgdInList(igd)) {
        return false;
    }

    if (not igd->publicIp_.isValid(igd->publicIp_.toString().c_str())) {
        JAMI_WARN("UPnPContext: IGD trying to be added has invalid public IpAddress.");
        return false;
    }

    IpAddr publicIp = igd->publicIp_;
    igdList_.push_back(std::make_pair(protocol, igd));
    
    for (const auto& item : igdListeners_) {
        item.second();
    }

    return true;
}

bool
UPnPContext::removeIgdFromList(IGD* igd)
{
    std::lock_guard<std::mutex> igdListLock(igdListMutex_);

    std::list<std::pair<UPnPProtocol*, IGD*>>::iterator it = igdList_.begin();
    while (it != igdList_.begin()) {
        if (it->second == igd) {
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

    std::list<std::pair<UPnPProtocol*, IGD*>>::iterator it = igdList_.begin();
    while (it != igdList_.begin()) {
        if (it->second->publicIp_ == publicIpAddr) {
            igdList_.erase(it);
            return true;
        } else {
            it++;
        }
    }

    return false;

}

}} // namespace jami::upnp
