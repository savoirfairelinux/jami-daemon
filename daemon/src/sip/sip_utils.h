/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef SIP_UTILS_H_
#define SIP_UTILS_H_

#include <pjsip/sip_msg.h>
#include <pjlib.h>

#include <utility>
#include <string>
#include <vector>

struct pjsip_msg;

namespace sip_utils {
    /**
     * Helper function to parser header from incoming sip messages
     * @return Header from SIP message
     */
    std::string fetchHeaderValue(pjsip_msg *msg, const std::string &field);

    pjsip_route_hdr *
    createRouteSet(const std::string &route, pj_pool_t *hdr_pool);

    void stripSipUriPrefix(std::string& sipUri);

    std::string parseDisplayName(const char * buffer);

    std::string getHostFromUri(const std::string& sipUri);

    void addContactHeader(const pj_str_t *contactStr, pjsip_tx_data *tdata);

    void sip_strerror(pj_status_t code);
}

#endif // SIP_UTILS_H_
