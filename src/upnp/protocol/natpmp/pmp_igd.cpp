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

void
PMPIGD::clear()
{
    toRemove_.clear();
    udpMappings.clear();
    tcpMappings.clear();
}

void
PMPIGD::clearMappings()
{
    clear();
    clearAll_ = true;
}

GlobalMapping*
PMPIGD::getNextMappingToRenew() const
{
    const GlobalMapping* mapping {nullptr};
    for (const auto& m : udpMappings)
    {
        if (!mapping or m.second.renewal_ < mapping->renewal_)
        {
            mapping = &m.second;
        }
    }

    for (const auto& m : tcpMappings)
    {
        if (!mapping or m.second.renewal_ < mapping->renewal_)
        {
            mapping = &m.second;
        }
    }
    return (GlobalMapping*)mapping;
}

time_point
PMPIGD::getRenewalTime() const
{
    const auto next = getNextMappingToRenew();
    auto nextTime = std::min(renewal_, next ? next->renewal_ : time_point::max());
    return toRemove_.empty() ? nextTime : std::min(nextTime, time_point::min());
}

bool
PMPIGD::operator==(PMPIGD& other) const
{
    return publicIp_ == other.publicIp_ and localIp_ == other.localIp_;
}

}} // namespace jami::upnp