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
IGD::isMapInUse(const unsigned int externalPort)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    for (auto it = udpMappings_.cbegin(); it != udpMappings_.cend(); it++) {
        if (it->first == externalPort) {
            return true;
        }
    }

    for (auto it = tcpMappings_.cbegin(); it != tcpMappings_.cend(); it++) {
        if (it->first == externalPort) {
            return true;
        }
    }

    return false;
}

bool
IGD::isMapInUse(const unsigned int externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.cbegin(); it != mapList.cend(); it++) {
        if (it->first == externalPort) {
            return true;
        }
    }

    return false;
}

bool
IGD::isMapInUse(const Mapping map)
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

void
IGD::addMapInUse(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            it->second.users_++;
            JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            return;
        }
    }

    GlobalMapping globalMap = GlobalMapping(map);
    mapList.insert({std::move(globalMap.getPortExternal()), std::move(globalMap)});
}

void
IGD::removeMapInUse(unsigned int externalPort)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    for (auto it = udpMappings_.begin(); it != udpMappings_.end(); it++) {
        if (it->first == externalPort) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            } else {
                udpMappings_.erase(it->first);
            }
            return;
        }
    }

    for (auto it = tcpMappings_.begin(); it != tcpMappings_.end(); it++) {
        if (it->first == externalPort) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            } else {
                tcpMappings_.erase(it->first);
            }
            return;
        }
    }
}

void
IGD::removeMapInUse(unsigned int externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->first == externalPort) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            } else {
                mapList.erase(it->first);
            }
            return;
        }
    }
}

void
IGD::removeMapInUse(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            } else {
                mapList.erase(it->first);
            }
            return;
        }
    }
}

Mapping*
IGD::getMapping(unsigned int externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.cbegin(); it != mapList.cend(); it++) {
        if (it->first == externalPort) {
            return new Mapping(std::move(it->second.getPortExternal()),
                               std::move(it->second.getPortInternal()),
                               std::move(it->second.getType()));
        }
    }

    return nullptr;
}

PortMapGlobal*
IGD::getCurrentMappingList(upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    return &mapList;
}

unsigned int
IGD::getNbOfUsers(const unsigned int externalPort)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    for (auto it = udpMappings_.cbegin(); it != udpMappings_.cend(); it++) {
        if (it->first == externalPort) {
            return it->second.users_;
        }
    }

    for (auto it = tcpMappings_.cbegin(); it != tcpMappings_.cend(); it++) {
        if (it->first == externalPort) {
            return it->second.users_;
        }
    }

    return 0;
}

unsigned int
IGD::getNbOfUsers(const unsigned int externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.cbegin(); it != mapList.cend(); it++) {
        if (it->first == externalPort) {
            return it->second.users_;
        }
    }

    return 0;
}

unsigned int
IGD::getNbOfUsers(const Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.cbegin(); it != mapList.cend(); it++) {
        if (it->second == map) {
            return it->second.users_;
        }
    }

    return 0;
}

void
IGD::incrementNbOfUsers(const unsigned int externalPort)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    for (auto it = udpMappings_.begin(); it != udpMappings_.end(); it++) {
        if (it->first == externalPort) {
            it->second.users_++;
            JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            return;
        }
    }

    for (auto it = tcpMappings_.begin(); it != tcpMappings_.end(); it++) {
        if (it->first == externalPort) {
            it->second.users_++;
            JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            return;
        }
    }
}

void
IGD::incrementNbOfUsers(const unsigned int externalPort, upnp::PortType type)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->first == externalPort) {
            it->second.users_++;
            JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            return;
        }
    }
}

void
IGD::incrementNbOfUsers(const Mapping map)
{
    std::unique_lock<std::mutex> lk(mapListMutex_);

    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            it->second.users_++;
            JAMI_DBG("IGD: Incrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
            return;
        }
    }

    lk.unlock();
    addMapInUse(Mapping{std::move(map.getPortExternal()), std::move(map.getPortInternal()), std::move(map.getType())});
    lk.lock();
}

void
IGD::decrementNbOfUsers(const unsigned int externalPort)
{
    std::unique_lock<std::mutex> lk(mapListMutex_);

    for (auto it = udpMappings_.begin(); it != udpMappings_.end(); it++) {
        if (it->first == externalPort) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
                return;
            } else {
                lk.unlock();
                removeMapInUse(externalPort);
                lk.lock();
                return;
            }
        }
    }

    for (auto it = tcpMappings_.begin(); it != tcpMappings_.end(); it++) {
        if (it->first == externalPort) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                JAMI_DBG("IGD: Decrementing the number of users for %s to %u", it->second.toString().c_str(), it->second.users_);
                return;
            } else {
                lk.unlock();
                removeMapInUse(externalPort);
                lk.lock();
                return;
            }
        }
    }
}

void
IGD::decrementNbOfUsers(const unsigned int externalPort, upnp::PortType type)
{
    std::unique_lock<std::mutex> lk(mapListMutex_);

    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->first == externalPort) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                return;
            } else {
                lk.unlock();
                removeMapInUse(externalPort, type);
                lk.lock();
                return;
            }
        }
    }
}

void
IGD::decrementNbOfUsers(const Mapping map)
{
    std::unique_lock<std::mutex> lk(mapListMutex_);

    auto& mapList = map.getType() == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    for (auto it = mapList.begin(); it != mapList.end(); it++) {
        if (it->second == map) {
            if (it->second.users_ > 1) {
                it->second.users_--;
                return;
            } else {
                lk.unlock();
                removeMapInUse(map.getPortExternal());
                lk.lock();
                return;
            }
        }
    }
}

}} // namespace jami::upnp