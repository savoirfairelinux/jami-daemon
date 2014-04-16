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


/* An IPv4 equivalent to IN6_IS_ADDR_UNSPECIFIED */
#ifndef IN_IS_ADDR_UNSPECIFIED
#define IN_IS_ADDR_UNSPECIFIED(a) (((long int) (a)->s_addr) == 0x00000000)
#endif /* IN_IS_ADDR_UNSPECIFIED */

namespace ip_utils {
    const std::string DEFAULT_INTERFACE = "default";

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
     * Performs hostname resolution if necessary (with given address family).
     * If conversion fails, returned adress will have its family set to PJ_AF_UNSPEC.
     */
    pj_sockaddr strToAddr(const std::string& str, pj_uint16_t family = pj_AF_UNSPEC());

    /**
     * Return the generic "any host" IP address of the specified family.
     * If family is unspecified, default to pj_AF_INET6() (IPv6).
     */
    pj_sockaddr getAnyHostAddr(pj_uint16_t family = pj_AF_UNSPEC());

    /**
     * Return the first host IP address of the specified family.
     * If no address of the specified family is found, another family will
     * be tried.
     * Ex. : if family is pj_AF_INET6() (IPv6/default) and the system does not
     * have an IPv6 address, an IPv4 address will be returned if available.
     *
     * If family is unspcified, default to pj_AF_INET6() (IPv6).
     */
    pj_sockaddr getLocalAddr(pj_uint16_t family = pj_AF_UNSPEC());

    /**
     * Get the IP address of the network interface interface with the specified
     * address family, or of any address family if unspecified (default).
     */
    pj_sockaddr getInterfaceAddr(const std::string &interface, pj_uint16_t family = pj_AF_UNSPEC());

    /**
    * List all the interfaces on the system and return
    * a vector list containing their name (eth0, eth0:1 ...).
    * @param void
    * @return std::vector<std::string> A std::string vector
    * of interface name available on all of the interfaces on
    * the system.
    */
    std::vector<std::string> getAllIpInterfaceByName();

    /**
    * List all the interfaces on the system and return
    * a vector list containing their IP address.
    * @param void
    * @return std::vector<std::string> A std::string vector
    * of IP address available on all of the interfaces on
    * the system.
    */
    std::vector<std::string> getAllIpInterface();

    std::vector<pj_sockaddr> getAddrList(const std::string &name);

    bool haveCommonAddr(const std::vector<pj_sockaddr>& a, const std::vector<pj_sockaddr>& b);

    /**
     * Return true if address is a valid IP address of specified family (if provided) or of any kind (default).
     */
    bool isValidAddr(const std::string &address, pj_uint16_t family = pj_AF_UNSPEC());

    /**
     * Return true if address is a valid IPv6.
     */
    bool isIPv6(const std::string &address);


}

#endif // IP_UTILS_H_
