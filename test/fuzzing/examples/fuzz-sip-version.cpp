/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstdlib>

#include "lib/gnutls.h"
#include "lib/sip-fmt.h"

/*
 * Example fuzzing wrapper that will change the SIP version sent randomly
 * between 1.0 and 2.0
 */
bool
mutate_gnutls_record_send(ChanneledMessage& msg)
{
    static int version_cnt = 0;

    SIPFmt sip(msg.data);

    if (not sip.isValid()) {
        return false;
    }

    char version[] = "SIP/2.0";
    char version_fmt[] = "SIP/%d.0";

    snprintf(version, array_size(version), version_fmt, (version_cnt++ % 2) + 1);

    sip.setVersion(version);

    /* Commit changes! */
    sip.swap(msg.data);

    return true;
}
