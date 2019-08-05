/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "global_mapping.h"

#include "logger.h"
#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include <mutex>

namespace jami { namespace upnp {

using PortMapLocal = std::map<uint16_t, Mapping>;
using PortMapGlobal = std::map<uint16_t, GlobalMapping>;

using LocalMapItr = std::map<uint16_t, Mapping>::iterator;
using GlobalMaptItr = std::map<uint16_t, GlobalMapping>::iterator;

class IGD
{
public:
    IGD(IpAddr&& localIp = {}, IpAddr&& publicIp = {});
    IGD(IGD&&) = default;
    virtual ~IGD() = default;

    IGD& operator=(IGD&&) = default;
    bool operator==(IGD& other) const;

    // Checks if the port is currently being used (opened) given a port.
    bool isMapInUse(const unsigned int externalPort);

    // Checks if the port is currently being used (opened) given a port and the type.
    bool isMapInUse(const unsigned int externalPort, upnp::PortType type);

    // Checks if the mapping is currently being used (opened).
    bool isMapInUse(const Mapping map);

    // Adds the mapping to the list.
    void addMapInUse(Mapping map);

    // Removes the mapping associated with the given port from the list.
    void removeMapInUse(unsigned int externalPort);

    // Removes the mapping associated with the given port and type from the list.
    void removeMapInUse(unsigned int externalPort, upnp::PortType type);

    // Removes the mapping from the list.
    void removeMapInUse(Mapping map);

    // Returns the mapping associated to the given port and type.
    Mapping* getMapping(unsigned int externalPort, upnp::PortType type);

    // Returns the list of currently used mappings according to the port type.
    PortMapGlobal* getCurrentMappingList(upnp::PortType type);

    // Given a port, returns number of users that are using it.
    unsigned int getNbOfUsers(const unsigned int externalPort);

    // Given a port and type, returns number of users that are using it.
    unsigned int getNbOfUsers(const unsigned int externalPort, upnp::PortType type);

    // Given a mapping, returns number of users that are using it.
    unsigned int getNbOfUsers(const Mapping map);

    // Increments the number of users for a given port.
    void incrementNbOfUsers(const unsigned int externalPort);

    // Increments the number of users for a given port and type.
    void incrementNbOfUsers(const unsigned int externalPort, upnp::PortType type);

    // Increments the number of users for a given mapping.
    void incrementNbOfUsers(const Mapping map);

    // Reduces the number of users for a given port.
    void decrementNbOfUsers(const unsigned int externalPort);

    // Reduces the number of users for a given port and type.
    void decrementNbOfUsers(const unsigned int externalPort, upnp::PortType type);

    // Reduces the number of users for a given mapping.
    void decrementNbOfUsers(const Mapping map);

public:
    IpAddr localIp_ {};                     // Internal IP interface used to communication with IGD.
    IpAddr publicIp_ {};                    // External IP of IGD.

    std::mutex mapListMutex_;               // Mutex for protecting map lists.
    PortMapGlobal udpMappings_ {};          // IGD UDP port mappings.
    PortMapGlobal tcpMappings_ {};          // IGD TCP port mappings.

private:
    NON_COPYABLE(IGD);
};


}} // namespace jami::upnp