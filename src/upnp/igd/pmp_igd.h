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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "igd.h"
#include "../mapping/mapping.h"
#include "../mapping/global_mapping.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>

namespace jami { namespace upnp {

#if HAVE_LIBNATPMP

using clock = std::chrono::system_clock;
using time_point = clock::time_point;

class PMPIGD : public IGD
{
public:
    PMPIGD(IpAddr&& localIp = {}, IpAddr&& publicIp = {}):
        IGD(std::move(localIp), std::move(publicIp)){}

    bool operator==(PMPIGD& other) const;

    void clear();
    void clearMappings();

    GlobalMapping* getNextMappingToRenew() const;

    time_point getRenewalTime() const;

public:
    time_point renewal_ {time_point::min()};
    std::vector<GlobalMapping> toRemove_ {};
    bool clearAll_ {false};
};

#endif

}} // namespace jami::upnp
