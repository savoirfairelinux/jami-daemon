/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include "upnp/protocol/mapping.h"

#ifdef _MSC_VER
typedef uint16_t in_port_t;
#endif

namespace jami {
namespace upnp {

class IGD
{
public:
    IGD(IpAddr&& localIp = {}, IpAddr&& publicIp = {});
    virtual ~IGD() = default;
    bool operator==(IGD& other) const;

    // Returns the mapping associated to the given port and type.
    Mapping getMapping(in_port_t externalPort, upnp::PortType type) const;

    // Returns the list of currently used mappings according to the port type.
    const IpAddr& getLocalIp() const { return localIp_; };
    const IpAddr& getPublicIp() const { return publicIp_; };
    void setLocalIp(const IpAddr& addr) { localIp_ = addr; }
    void setPublicIp(const IpAddr& addr) { publicIp_ = addr; }

    void setUID(const std::string& uid) { uid_ = uid;}
    const std::string& getUID() const { return uid_; }

protected:
    IpAddr localIp_ {};  // Internal IP interface used to communication with IGD.
    IpAddr publicIp_ {}; // External IP of IGD.
    std::string uid_{};
    std::mutex mapListMutex_;      // Mutex for protecting map lists.
    std::map<uint16_t, Mapping> udpMappings_ {}; // IGD UDP port mappings.
    std::map<uint16_t, Mapping> tcpMappings_ {}; // IGD TCP port mappings.

private:
    NON_COPYABLE(IGD);
};

} // namespace upnp
} // namespace jami
