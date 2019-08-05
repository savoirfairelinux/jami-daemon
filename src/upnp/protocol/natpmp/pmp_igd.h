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

#include "../igd.h"
#include "../global_mapping.h"

#include "noncopyable.h"
#include "ip_utils.h"

#include <map>
#include <atomic>
#include <string>
#include <chrono>
#include <functional>

namespace jami { namespace upnp {

using clock = std::chrono::system_clock;
using time_point = clock::time_point;

class PMPIGD : public IGD
{
public:
    using mapItr = std::vector<Mapping>::iterator;

    PMPIGD(IpAddr&& localIp = {}, IpAddr&& publicIp = {}):
           IGD(std::move(localIp), std::move(publicIp)){}
    ~PMPIGD();
    bool operator==(IGD& other) const;
    bool operator==(PMPIGD& other) const;

    // Checks if the given mapping was already added.
    bool isMapAdded(const Mapping map);

    // Adds a mapping to the list of mappings we need to open.
    void addMapToAdd(Mapping map);

    // Removes a mapping from the list of mappings we need to open.
    void removeMapToAdd(Mapping map);

    // Adds an added mapping to the list of mappings to be considered for renewal.
    void addMapToRenew(Mapping map);

    // Removes a mapping from the renewal list.
    void removeMapToRenew(Mapping map);

    // Adds a mapping to the list of mappings we need to close.
    void addMapToRemove(Mapping map);

    // Removes a mapping from the list of mappings we need to close.
    void removeMapToRemove(Mapping map);

    // Clears all the mappings.
    void clearMappings();

    // Checks if a given mapping needs to be renewed.
    bool isMapUpForRenewal(Mapping map, time_point now);

    // Gets the next mapping to renew.
    Mapping* getNextMappingToRenew();

    // Gets the next renewal time.
    time_point getRenewalTime();

public:
    std::mutex mapListMutex_;                     // Mutex for protecting map lists.
    std::vector<Mapping> mapToAddList_ {};        // List of maps to add.
    std::vector<Mapping> mapToRenewList_ {};      // List of maps to renew.
    std::vector<Mapping> mapToRemoveList_ {};     // List of maps to remove.

    time_point renewal_ {time_point::min()};      // Renewal time.

    // Upon creation, the thread will clear all the previously opened
    // mappings (if there are any). The NatPmp class will then set the
    // clearAll variable to false.
    std::atomic_bool clearAll_ {true};
};

}} // namespace jami::upnp
