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

#include "igd.h"
#include "logger.h"

namespace jami {
namespace upnp {

IGD::IGD(IpAddr&& localIp, IpAddr&& publicIp)
{
    localIp_ = std::move(localIp);
    publicIp_ = std::move(publicIp);
}

bool
IGD::operator==(IGD& other) const
{
    if (localIp_ != other.localIp_) return false;
    if (publicIp_ != other.publicIp_) return false;
    if (uid_ != other.uid_) return false;
    return true;
}

Mapping
IGD::getMapping(in_port_t externalPort, upnp::PortType type) const
{
    auto& mapList = type == upnp::PortType::UDP ? udpMappings_ : tcpMappings_;
    auto it = mapList.find(externalPort);
    if (it != mapList.end()) {
        if (it->first == externalPort) {
            return Mapping(it->second.getPortExternal(),
                           it->second.getPortInternal(),
                           it->second.getType());
        }
    }
    return Mapping(0, 0);
}

} // namespace upnp
} // namespace jami