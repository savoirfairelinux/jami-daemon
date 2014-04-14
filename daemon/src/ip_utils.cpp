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

#include "ip_utils.h"
#include "logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

std::vector<pj_sockaddr>
ip_utils::getAddrList(const std::string &name)
{
    std::vector<pj_sockaddr> ipList;
    if (name.empty())
        return ipList;

    static const unsigned MAX_ADDR_NUM = 128;
    pj_addrinfo res[MAX_ADDR_NUM];
    unsigned addr_num = MAX_ADDR_NUM;
    pj_str_t pjname;
    pj_cstr(&pjname, name.c_str());
    auto status = pj_getaddrinfo(pj_AF_UNSPEC(), &pjname, &addr_num, res);
    if (status != PJ_SUCCESS) {
        ERROR("Error resolving %s :", name.c_str());
        //sip_strerror(status);
        return ipList;
    }

    for (unsigned i=0; i<addr_num; i++) {
        bool found = false;
        for (const auto& ip : ipList)
            if (!pj_sockaddr_cmp(&ip, &res[i].ai_addr)) {
                found = true;
                break;
            }
        if (!found)
            ipList.push_back(res[i].ai_addr);
    }

    return ipList;
}

bool
ip_utils::haveCommonAddr(const std::vector<pj_sockaddr>& a, const std::vector<pj_sockaddr>& b)
{
    for (const auto &i : a) {
        for (const auto &j : b) {
            if (pj_sockaddr_cmp(&i, &j) == 0) return true;
        }
    }
    return false;
}

std::string
ip_utils::addrToStr(const pj_sockaddr& ip, bool include_port, bool force_ipv6_brackets)
{
    std::string str(PJ_INET6_ADDRSTRLEN, (char)0);
    if(include_port) force_ipv6_brackets = true;
    pj_sockaddr_print(&ip, &(*str.begin()), PJ_INET6_ADDRSTRLEN, (include_port?1:0)|(force_ipv6_brackets?2:0));
    return str;
}

std::string
ip_utils::addrToStr(const std::string& ip_str, bool include_port, bool force_ipv6_brackets)
{
    pj_sockaddr ip = strToAddr(ip_str);
    if (ip.addr.sa_family == pj_AF_UNSPEC())
        return ip_str;
    return addrToStr(ip, include_port, force_ipv6_brackets);
}

pj_sockaddr
ip_utils::strToAddr(const std::string& str)
{
    pj_str_t pjstring;
    pj_cstr(&pjstring, str.c_str());
    pj_sockaddr ip;
    auto status = pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &pjstring, &ip);
    if (status != PJ_SUCCESS)
        ip.addr.sa_family = pj_AF_UNSPEC();
    return ip;
}

pj_sockaddr
ip_utils::getAnyHostAddr(pj_uint16_t family)
{
    if (family == pj_AF_UNSPEC()) family = pj_AF_INET();
    pj_sockaddr addr = {};
    addr.addr.sa_family = family;
    return addr;
}

bool
ip_utils::isIPv6(const std::string &address)
{
    return isValidAddr(address, pj_AF_INET6());
}

bool
ip_utils::isValidAddr(const std::string &address, pj_uint16_t family)
{
    pj_str_t pjstring;
    pj_cstr(&pjstring, address.c_str());
    pj_str_t ret_str;
    pj_uint16_t ret_port;
    int ret_family;
    auto status = pj_sockaddr_parse2(pj_AF_UNSPEC(), 0, &pjstring, &ret_str, &ret_port, &ret_family);
    if (status != PJ_SUCCESS || (family != pj_AF_UNSPEC() && ret_family != family))
        return false;

    char buf[PJ_INET6_ADDRSTRLEN];
    pj_str_t addr_with_null = {buf, 0};
    pj_strncpy_with_null(&addr_with_null, &ret_str, sizeof(buf));
    struct sockaddr sa;
    return inet_pton(ret_family==pj_AF_INET6()?AF_INET6:AF_INET, buf, &(sa.sa_data)) == 1;
}
