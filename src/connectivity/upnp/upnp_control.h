/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

#include "upnp_context.h"

#include "noncopyable.h"
#include "logger.h"
#include "connectivity/ip_utils.h"

#include <memory>
#include <chrono>

namespace jami {
class IpAddr;
}

namespace jami {
namespace upnp {

class UPnPContext;

class Controller
{
public:
    Controller();
    ~Controller();

    // Set known public address
    void setPublicAddress(const IpAddr& addr);
    // Checks if a valid IGD is available.
    bool isReady() const;
    // Gets the external ip of the first valid IGD in the list.
    IpAddr getExternalIP() const;

    // Request port mapping.
    // Returns a shared pointer on the allocated mapping. The shared
    // pointer may point to nothing on failure.
    Mapping::sharedPtr_t reserveMapping(Mapping& map);
    Mapping::sharedPtr_t reserveMapping(uint16_t port, PortType type);

    // Remove port mapping.
    void releaseMapping(const Mapping& map);
    static uint16_t generateRandomPort(PortType);

private:
    // Adds a mapping locally to the list.
    void addLocalMap(const Mapping& map);
    // Removes a mapping from the local list.
    bool removeLocalMap(const Mapping& map);
    // Removes all mappings of the given type.
    void releaseAllMappings();

    std::shared_ptr<UPnPContext> upnpContext_;

    mutable std::mutex mapListMutex_;
    std::map<Mapping::key_t, Mapping> mappingList_;
};

} // namespace upnp
} // namespace jami
