/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
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

#include <cstdlib>

#include "lib/gnutls.h"
#include "lib/sip-fmt.h"

/*
 * Example fuzzing wrapper that will change the SIP version all the time.
 */

bool
mutate_gnutls_record_recv(std::vector<uint8_t>& data)
{
    SIPFmt sip(data);

    if (not sip.isValid()) {
        return false;
    }

    char version[80];
    char version_fmt[] = "SIP/%u.%u";

    snprintf(version, array_size(version), version_fmt, static_cast<unsigned int>(rand()));

    sip.setVersion(version);
    sip.swap(data);

    return true;
}

bool
mutate_gnutls_record_send(std::vector<uint8_t>& data)
{
    SIPFmt sip(data);

    if (not sip.isValid()) {
        return false;
    }

    char version[80];
    char version_fmt[] = "SIP/%u.%u";

    snprintf(version, array_size(version), version_fmt, static_cast<unsigned int>(rand()));

    sip.setVersion(version);
    sip.swap(data);

    return true;
}
