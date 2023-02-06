/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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
 * Insert a HTML payload in a SIP request if no content-type
 */
bool
mutate_gnutls_record_send(ChanneledMessage& msg)
{
    SIPFmt sip(msg.data);

    if (not sip.isRequest()) {
        return false;
    }

    if (not sip.getField("content-type").empty()) {
        return false;
    }

    char htmlBody[] = "<html><h1>FUZZ</h1></html>\n";
    std::vector<uint8_t> body;

    body.reserve(array_size(htmlBody));

    for (size_t i=0; i<array_size(htmlBody); ++i) {
            body.emplace_back(htmlBody[i]);
    }

    sip.swapBody(body);
    sip.setFieldValue("content-type", "text/html");

    /* Commit changes! */
    sip.swap(msg.data);

    return true;
}
