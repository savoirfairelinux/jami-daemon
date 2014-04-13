/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#ifndef IP_UTILS_H_
#define IP_UTILS_H_

#include <pjlib.h>

#include <string>
#include <vector>

namespace ip_utils {
    /**
     * Convert a binary IP address to a standard string representation.
     */
    std::string addrToStr(const pj_sockaddr& ip, bool include_port = false, bool force_ipv6_brackets = false);

    /**
     * Format an IP address string. If formating the address fails, the original string is returned.
     */
    std::string addrToStr(const std::string& ip, bool include_port = false, bool force_ipv6_brackets = false);

    /**
     * Convert a string representation of an IP adress to its binary counterpart.
     *
     * Performs hostname resolution if necessary.
     * If conversion fails, returned adress will have its family set to PJ_AF_UNSPEC.
     */
    pj_sockaddr strToAddr(const std::string& str);

    /**
     * Returns true if address is a valid IPv6.
     */
    bool isIPv6(const std::string &address);
    bool isValidAddr(const std::string &address, pj_uint16_t family = pj_AF_UNSPEC());

    std::vector<pj_sockaddr> getAddrList(const std::string &name);

    pj_sockaddr getAnyHostAddr(pj_uint16_t family = pj_AF_UNSPEC());
}

#endif // IP_UTILS_H_
