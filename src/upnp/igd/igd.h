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

#include "../mapping/global_mapping.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include <string>
#include <map>
#include <functional>
#include <chrono>

namespace jami { namespace upnp {

// Subclasses to make it easier to differentiate and cast maps of port mappings.
class PortMapLocal : public std::map<uint16_t, Mapping> {};
class PortMapGlobal : public std::map<uint16_t, GlobalMapping> {};

using IgdFoundCallback = std::function<void()>;

class IGD
{
public:
    IGD(IpAddr&& localIp = {}, IpAddr&& publicIp = {});
    IGD(IGD&&) = default;
    virtual ~IGD() = default;

    IGD& operator=(IGD&&) = default;
    bool operator==(IGD& other) const;

public:
    IpAddr localIp_ {};                    // Internal IP interface used to communication with IGD.
    IpAddr publicIp_ {};                   // External IP of IGD.

    PortMapGlobal udpMappings {};          // IGD UDP port mappings.
    PortMapGlobal tcpMappings {};          // IGD TCP port mappings.

private:
    NON_COPYABLE(IGD);
};


}} // namespace jami::upnp