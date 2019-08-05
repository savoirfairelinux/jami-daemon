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
#include "pmp_igd.h"

namespace jami { namespace upnp {

PMPIGD::~PMPIGD()
{
    tcpMappings_.clear();
    udpMappings_.clear();
    mapToRemoveList_.clear();
    mapToRenewList_.clear();
    mapToAddList_.clear();
}

bool
PMPIGD::operator==(IGD& other) const
{
    return publicIp_ == other.publicIp_ and localIp_ == other.localIp_;
}

bool
PMPIGD::operator==(PMPIGD& other) const
{
    return publicIp_ == other.publicIp_ and localIp_ == other.localIp_;
}

bool
PMPIGD::isMapAdded(const Mapping& map)
{
    return isMapInUse(map);
}

void
PMPIGD::addMapToAdd(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    mapToAddList_.push_back(std::move(map));
}

void
PMPIGD::removeMapToAdd(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = mapToAddList_.cbegin(); it != mapToAddList_.cend(); it++) {
        if (*it == map) {
            mapToAddList_.erase(it);
            return;
        }
    }
}

void
PMPIGD::addMapToRenew(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = mapToRenewList_.cbegin(); it != mapToRenewList_.cend(); it++) {
        if (*it == map) {
            return;
        }
    }
    mapToRenewList_.push_back(std::move(map));
}

void
PMPIGD::removeMapToRenew(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = mapToRenewList_.cbegin(); it != mapToRenewList_.cend(); it++) {
        if (*it == map) {
            mapToRenewList_.erase(it);
            return;
        }
    }
}

void
PMPIGD::addMapToRemove(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = mapToRemoveList_.cbegin(); it != mapToRemoveList_.cend(); it++) {
        if (*it == map) {
            return;
        }
    }
    mapToRemoveList_.push_back(std::move(map));
}

void
PMPIGD::removeMapToRemove(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = mapToRemoveList_.cbegin(); it != mapToRemoveList_.cend(); it++) {
        if (*it == map) {
            mapToRemoveList_.erase(it);
            return;
        }
    }
}

void
PMPIGD::clearMappings()
{
    mapToRemoveList_.clear();
    mapToRenewList_.clear();
    mapToAddList_.clear();
    udpMappings_.clear();
    tcpMappings_.clear();
}

bool
PMPIGD::isMapUpForRenewal(const Mapping& map, time_point now)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = mapToRenewList_.begin(); it != mapToRenewList_.end(); it++) {
        if (*it == map) {
            if (map.renewal_ < now)
                return true;
            else
                return false;
        }
    }
    return false;
}

Mapping*
PMPIGD::getNextMappingToRenew()
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    if (mapToRenewList_.empty()) {
        return nullptr;
    }

    Mapping* mapping = &mapToRenewList_.front();

    for (auto it = mapToRenewList_.begin(); it != mapToRenewList_.end(); it++) {
        auto& element = (*it);
        if (element.renewal_ < mapping->renewal_) {
            mapping = &element;
        }
    }

    return mapping;
}

time_point
PMPIGD::getRenewalTime()
{
    std::unique_lock<std::mutex> lk(mapListMutex_);
    lk.unlock();
    const auto next = getNextMappingToRenew();
    lk.lock();
    auto nextTime = std::min(renewal_, next ? next->renewal_ : time_point::max());
    return mapToRenewList_.empty() ? nextTime : std::min(nextTime, time_point::min());
}

}} // namespace jami::upnp