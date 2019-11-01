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
    toRemove_.clear();
    toRenew_.clear();
    toAdd_.clear();
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
    toAdd_.push_back(std::move(map));
}
void
PMPIGD::removeMapToAdd(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = toAdd_.cbegin(); it != toAdd_.cend(); it++) {
        if (*it == map) {
            toAdd_.erase(it);
            return;
        }
    }
}
void
PMPIGD::addMapToRenew(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = toRenew_.cbegin(); it != toRenew_.cend(); it++) {
        if (*it == map) {
            return;
        }
    }
    toRenew_.push_back(std::move(map));
}
void
PMPIGD::removeMapToRenew(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = toRenew_.cbegin(); it != toRenew_.cend(); it++) {
        if (*it == map) {
            toRenew_.erase(it);
            return;
        }
    }
}
void
PMPIGD::addMapToRemove(Mapping map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = toRemove_.cbegin(); it != toRemove_.cend(); it++) {
        if (*it == map) {
            return;
        }
    }
    toRemove_.push_back(std::move(map));
}
void
PMPIGD::removeMapToRemove(const Mapping& map)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = toRemove_.cbegin(); it != toRemove_.cend(); it++) {
        if (*it == map) {
            toRemove_.erase(it);
            return;
        }
    }
}
void
PMPIGD::clearMappings()
{
    toRemove_.clear();
    toRenew_.clear();
    toAdd_.clear();
    udpMappings_.clear();
    tcpMappings_.clear();
}
bool
PMPIGD::isMapUpForRenewal(const Mapping& map, time_point now)
{
    std::lock_guard<std::mutex> lk(mapListMutex_);
    for (auto it = toRenew_.cbegin(); it != toRenew_.cend(); it++) {
        if (*it == map) {
            auto& element = (*it);
            if (element.renewal_ < now)
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
    Mapping* mapping {nullptr};
    if (not toAdd_.empty()) {
        return (Mapping*)&toAdd_.front();
    } else {
        if (not toRenew_.empty()) {
            for (auto it = toRenew_.begin(); it != toRenew_.end(); it++) {
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
    return toRenew_.empty() ? nextTime : std::min(nextTime, time_point::min());
}
}} // namespace jami::upnp