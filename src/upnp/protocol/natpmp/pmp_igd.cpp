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
PMPIGD::isMapAdded(const Mapping map)
{
    return isMapInUse(Mapping(map.getPortExternal(), map.getPortInternal(), map.getType()));
}

void
PMPIGD::addMapToAdd(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    mapToAddList_.push_back(Mapping{std::move(map)});
}

void
PMPIGD::removeMapToAdd(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (mapItr it = mapToAddList_.begin(); it != mapToAddList_.end(); it++) {
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
    for (mapItr it = mapToRenewList_.begin(); it != mapToRenewList_.end(); it++) {
        if (*it == map) {
            return;
        }
    }
    mapToRenewList_.push_back(Mapping{std::move(map)});
}

void
PMPIGD::removeMapToRenew(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (mapItr it = mapToRenewList_.begin(); it != mapToRenewList_.end(); it++) {
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
    for (mapItr it = mapToRemoveList_.begin(); it != mapToRemoveList_.end(); it++) {
        if (*it == map) {
            return;
        }
    }
    mapToRemoveList_.push_back(Mapping{std::move(map)});
}

void
PMPIGD::removeMapToRemove(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (mapItr it = mapToRemoveList_.begin(); it != mapToRemoveList_.end(); it++) {
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
PMPIGD::isMapUpForRenewal(Mapping map, time_point now)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (mapItr it = mapToRenewList_.begin(); it != mapToRenewList_.end(); it++) {
        if (*it == map) {
            auto& element = (*it);
            if (element.renewal_ < now)
                return true;
            else
                return false;
        }
    }
}

Mapping*
PMPIGD::getNextMappingToRenew()
{
    std::lock_guard<std::mutex> lk(mapListMutex_);

    Mapping* mapping {nullptr};
    if (not mapToAddList_.empty()) {
        return (Mapping*)&mapToAddList_.front();
    } else {
        if (not mapToRenewList_.empty()) {
            for (mapItr it = mapToRenewList_.begin(); it != mapToRenewList_.end(); it++) {
                auto& element = (*it);
                if (!mapping or element.renewal_ < mapping->renewal_) {
                    mapping = &element;
                }
            }
        }
    }

    return (Mapping*)mapping;
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