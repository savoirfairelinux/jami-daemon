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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp_context.h"
#include "protocol/mapping_request_observer.h"

#include "noncopyable.h"
#include "logger.h"
#include "ip_utils.h"

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

    // Checks if a valid IGD is available.
    bool hasValidIGD() const;

    // Gets the external ip of the first valid IGD in the list.
    IpAddr getExternalIP() const;

    // Gets the local ip that interface with the first valid IGD in the list.
    IpAddr getLocalIP() const;

    // Request port mapping.
    // Returns a shared pointer on the allocated mapping. The shared
    // pointer may point to nothing on failure.
    Mapping::sharedPtr_t reserveMapping(Mapping& map);
    Mapping::sharedPtr_t reserveMapping(uint16_t port, PortType type);

    // Remove port mapping.
    bool releaseMapping(Mapping::sharedPtr_t map);

private:
    // Adds a mapping locally to the list.
    void addLocalMap(Mapping::sharedPtr_t map);
    // Removes a mapping from the local list.
    bool removeLocalMap(Mapping::sharedPtr_t map);
    // Removes all mappings of the given type.
    void releaseAllMappings();

    std::shared_ptr<UPnPContext> upnpContext_;

    mutable std::mutex mapListMutex_;
    std::map<Mapping::key_t, Mapping::sharedPtr_t> mappingList_;
};

} // namespace upnp
} // namespace jami
