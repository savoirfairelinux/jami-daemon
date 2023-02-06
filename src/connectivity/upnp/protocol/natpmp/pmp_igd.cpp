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

#include "pmp_igd.h"

#include <algorithm>

namespace jami {
namespace upnp {

PMPIGD::PMPIGD()
    : IGD(NatProtocolType::NAT_PMP)
{}

PMPIGD::PMPIGD(const PMPIGD& other)
    : PMPIGD()
{
    assert(protocol_ == NatProtocolType::NAT_PMP);
    // protocol_ = other.protocol_;
    localIp_ = other.localIp_;
    publicIp_ = other.publicIp_;
    uid_ = other.uid_;
}

bool
PMPIGD::operator==(IGD& other) const
{
    return getPublicIp() == other.getPublicIp() and getLocalIp() == other.getLocalIp();
}

bool
PMPIGD::operator==(PMPIGD& other) const
{
    return getPublicIp() == other.getPublicIp() and getLocalIp() == other.getLocalIp();
}

const std::string
PMPIGD::toString() const
{
    return getLocalIp().toString();
}

} // namespace upnp
} // namespace jami
