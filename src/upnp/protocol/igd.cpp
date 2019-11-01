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

#include "igd.h"
#include "logger.h"

namespace jami { namespace upnp {

IGD::IGD(IpAddr&& localIp, IpAddr&& publicIp)
{
    localIp_ = std::move(localIp);
    publicIp_ = std::move(publicIp);
}

bool
IGD::operator==(IGD& other) const
{
    return (localIp_ == other.localIp_ and publicIp_ == other.publicIp_);
}

bool
IGD::isMapInUse(const in_port_t externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    return mapList.find(externalPort) != mapList.end();
}

bool
IGD::isMapInUse(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.cbegin(); it != mapList.cend(); it++) {
        if (it->second == map) {
            return true;
        }
    }
    return false;
}

Mapping
IGD::getMapping(in_port_t externalPort, upnp::PortType type) const
{
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = mapList.find(externalPort);
    if (it != mapList.end()) {
        if (it->first == externalPort) {
            return Mapping(it->second.getPortExternal(),
                           it->second.getPortInternal(),
                           it->second.getType(),
                           it->second.isUnique());
        }
    }
    return Mapping(0, 0);
}

unsigned int
IGD::getNbOfUsers(const in_port_t externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = mapList.find(externalPort);
    if (it != mapList.end())
        return it->second.users;
    return 0;
}

unsigned int
IGD::getNbOfUsers(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.cbegin(); it != mapList.cend(); it++) {
        if (it->second == map) {
            return it->second.users;
        }
    }
    return 0;
}

PortMapGlobal*
IGD::getCurrentMappingList(upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    return &mapList;
}

void
IGD::incrementNbOfUsers(const in_port_t externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = mapList.find(externalPort);
    if (it != mapList.end()) {
        it->second.users++;
        JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users);
        return;
    }
}

void
IGD::incrementNbOfUsers(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            it->second.users++;
            JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users);
            return;
        }
    }
    // If not found, add to mapList
    GlobalMapping globalMap = GlobalMapping(map);
    mapList.insert({globalMap.getPortExternal(), globalMap});
}

void
IGD::decrementNbOfUsers(const in_port_t externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = mapList.find(externalPort);
    if (it != mapList.end()) {
        if (it->second.users > 1) {
            it->second.users--;
            JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users);
            return;
        } else {
            mapList.erase(it);
            return;
        }
    }
}
void
IGD::decrementNbOfUsers(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            if (it->second.users > 1) {
                it->second.users--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users);
                return;
            } else {
                mapList.erase(it);
                return;
            }
        }
    }
}

void
IGD::removeMapInUse(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            if (it->second.users > 1) {
                it->second.users--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users);
            } else {
                mapList.erase(it->first);
            }
            return;
        }
    }
}

}} // namespace jami::upnp