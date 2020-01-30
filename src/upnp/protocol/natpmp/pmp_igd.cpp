/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
    toAdd_.emplace_back(std::move(map));
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
    toRenew_.emplace_back(std::move(map));
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
    toRemove_.emplace_back(std::move(map));
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

time_point
PMPIGD::getRenewalTime()
{
    std::unique_lock<std::mutex> lk(mapListMutex_);
    if (clearAll_ or not toAdd_.empty() or not toRemove_.empty())
        return time_point::min();
    auto nextTime = renewal_;
    for (const auto& m : toRenew_)
        nextTime = std::min(nextTime, m.renewal_);
    return nextTime;
}
}} // namespace jami::upnp