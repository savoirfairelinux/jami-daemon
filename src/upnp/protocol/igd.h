/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
 *
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#include <mutex>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "global_mapping.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#ifdef _MSC_VER
typedef uint16_t in_port_t;
#endif

namespace jami { namespace upnp {

// Subclasses to make it easier to differentiate and cast maps of port mappings.
class PortMapLocal : public std::map<uint16_t, Mapping> {};
class PortMapGlobal : public std::map<uint16_t, GlobalMapping> {};

class IGD
{
public:
    IGD(IpAddr&& localIp = {}, IpAddr&& publicIp = {});
    IGD(IGD&&) = default;
    virtual ~IGD() = default;

    IGD& operator=(IGD&&) = default;
    bool operator==(IGD& other) const;

    // Checks if the port is currently being used (i.e. the port is already opened).
    bool isMapInUse(const in_port_t externalPort, upnp::PortType type);
    bool isMapInUse(const Mapping& map);

    // Returns the mapping associated to the given port and type.
    Mapping getMapping(in_port_t externalPort, upnp::PortType type) const;

    // Increments the number of users for a given mapping.
    void incrementNbOfUsers(const in_port_t externalPort, upnp::PortType type);
    void incrementNbOfUsers(const Mapping& map);

    // Removes the mapping from the list.
    void removeMapInUse(const Mapping& map);

    // Returns the list of currently used mappings according to the port type.
    PortMapGlobal* getCurrentMappingList(upnp::PortType type);

    // Returns number of users for a given mapping.
    unsigned int getNbOfUsers(const in_port_t externalPort, upnp::PortType type);
    unsigned int getNbOfUsers(const Mapping& map);

    // Reduces the number of users for a given mapping.
    void decrementNbOfUsers(const in_port_t externalPort, upnp::PortType type);
    void decrementNbOfUsers(const Mapping& map);

    IpAddr localIp_ {};                    // Internal IP interface used to communication with IGD.
    IpAddr publicIp_ {};                   // External IP of IGD.

protected:
    std::mutex mapListMutex_;               // Mutex for protecting map lists.
    PortMapGlobal udpMappings_ {};          // IGD UDP port mappings.
    PortMapGlobal tcpMappings_ {};          // IGD TCP port mappings.

private:
    NON_COPYABLE(IGD);
};

}} // namespace jami::upnp