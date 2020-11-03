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

IGD::IGD(NatProtocolType proto)
    : protocol_(proto)
    , valid_(true)
{
    if (proto == NatProtocolType::NAT_PMP) {
        protocolName_ = "NAT_PMP";
    } else if (proto == NatProtocolType::PUPNP) {
        protocolName_ = "PUPNP";
    } else {
        assert(false);
    }
}

bool
IGD::operator==(IGD& other) const
{
    if (localIp_ != other.localIp_)
        return false;
    if (publicIp_ != other.publicIp_)
        return false;
    if (uid_ != other.uid_)
        return false;
    return true;
}

void
IGD::setValid(bool valid)
{
    valid_ = valid;

    if (valid) {
        // Reset errors counter.
        errorsCounter_ = 0;
    } else {
        JAMI_WARN("IGD %s [%s] was disabled", localIp_.toString().c_str(), getProtocolName());
    }
}

bool
IGD::incrementErrorsCounter()
{
    if (not valid_)
        return false;

    if (++errorsCounter_ >= MAX_ERRORS_COUNT) {
        JAMI_WARN("IGD %s [%s] has too many errors, it will be disabled",
                  localIp_.toString().c_str(),
                  getProtocolName());
        setValid(false);
        return false;
    }

    return true;
}

} // namespace upnp
} // namespace jami