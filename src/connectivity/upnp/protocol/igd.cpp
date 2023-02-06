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

#include "igd.h"
#include "logger.h"

namespace jami {
namespace upnp {

IGD::IGD(NatProtocolType proto)
    : protocol_(proto)
{}

bool
IGD::operator==(IGD& other) const
{
    return localIp_ == other.localIp_ and publicIp_ == other.publicIp_ and uid_ == other.uid_;
}

void
IGD::setValid(bool valid)
{
    valid_ = valid;

    if (valid) {
        // Reset errors counter.
        errorsCounter_ = 0;
    } else {
        JAMI_WARN("IGD %s [%s] was disabled", toString().c_str(), getProtocolName());
    }
}

bool
IGD::incrementErrorsCounter()
{
    if (not valid_)
        return false;

    if (++errorsCounter_ >= MAX_ERRORS_COUNT) {
        JAMI_WARN("IGD %s [%s] has too many errors, it will be disabled",
                  toString().c_str(),
                  getProtocolName());
        setValid(false);
        return false;
    }

    return true;
}

int
IGD::getErrorsCount() const
{
    return errorsCounter_.load();
}

} // namespace upnp
} // namespace jami