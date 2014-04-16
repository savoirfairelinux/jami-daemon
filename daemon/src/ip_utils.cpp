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

#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
    str.resize(std::char_traits<char>::length(str.c_str()));
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
    if (family == pj_AF_UNSPEC()) family = pj_AF_INET6();
    pj_sockaddr addr = {};
    addr.addr.sa_family = family;
    return addr;
}

pj_sockaddr
ip_utils::getLocalAddr(pj_uint16_t family)
{
    if (family == pj_AF_UNSPEC()) family = pj_AF_INET6();
    pj_sockaddr ip_addr;
    pj_status_t status = pj_gethostip(family, &ip_addr);
    if (status == PJ_SUCCESS) return ip_addr;
    WARN("Could not get preferred address familly (%s)", (family == pj_AF_INET6()) ? "IPv6" : "IPv4");
    family = (family == pj_AF_INET()) ? pj_AF_INET6() : pj_AF_INET();
    status = pj_gethostip(family, &ip_addr);
    if (status == PJ_SUCCESS) return ip_addr;
    ERROR("Could not get local IP");
    ip_addr.addr.sa_family = pj_AF_UNSPEC();
    return ip_addr;
}

pj_sockaddr
ip_utils::getInterfaceAddr(const std::string &interface, pj_uint16_t family)
{
    ERROR("getInterfaceAddr: %s %d", interface.c_str(), family);
    if (interface == DEFAULT_INTERFACE)
        return getLocalAddr(family);
    auto unix_family = (family == pj_AF_INET()) ? AF_INET : AF_INET6;
    int fd = socket(unix_family, SOCK_DGRAM, 0);
    if(unix_family == AF_INET6) {
        int val = (family == pj_AF_UNSPEC()) ? 0 : 1;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&val, sizeof(val));
    }
    pj_sockaddr saddr;
    if(fd < 0) {
        ERROR("Could not open socket: %m", fd);
        saddr.addr.sa_family = pj_AF_UNSPEC();
        return saddr;
    }
    ifreq ifr;
    strncpy(ifr.ifr_name, interface.c_str(), sizeof ifr.ifr_name);
    // guarantee that ifr_name is NULL-terminated
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    ifr.ifr_addr.sa_family = unix_family;

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    sockaddr* unix_addr = &ifr.ifr_addr;
    memcpy(&saddr, &ifr.ifr_addr, sizeof(pj_sockaddr));
    if ((ifr.ifr_addr.sa_family == AF_INET  &&  IN_IS_ADDR_UNSPECIFIED(&((sockaddr_in *)unix_addr)->sin_addr ))
    || (ifr.ifr_addr.sa_family == AF_INET6 && IN6_IS_ADDR_UNSPECIFIED(&((sockaddr_in6*)unix_addr)->sin6_addr))) {
        return getLocalAddr(saddr.addr.sa_family);
    }
    return saddr;
}

std::vector<std::string>
ip_utils::getAllIpInterfaceByName()
{
    static ifreq ifreqs[20];
    ifconf ifconf;

    std::vector<std::string> ifaceList;
    ifaceList.push_back("default");

    ifconf.ifc_buf = (char*) (ifreqs);
    ifconf.ifc_len = sizeof(ifreqs);

    int sock = socket(AF_INET6, SOCK_STREAM, 0);

    if (sock >= 0) {
        if (ioctl(sock, SIOCGIFCONF, &ifconf) >= 0)
            for (unsigned i = 0; i < ifconf.ifc_len / sizeof(ifreq); ++i)
                ifaceList.push_back(std::string(ifreqs[i].ifr_name));

        close(sock);
    }

    return ifaceList;
}

std::vector<std::string>
ip_utils::getAllIpInterface()
{
    pj_sockaddr addrList[16];
    unsigned addrCnt = PJ_ARRAY_SIZE(addrList);

    std::vector<std::string> ifaceList;

    if (pj_enum_ip_interface(pj_AF_UNSPEC(), &addrCnt, addrList) == PJ_SUCCESS) {
        for (unsigned i = 0; i < addrCnt; i++) {
            char addr[PJ_INET6_ADDRSTRLEN];
            pj_sockaddr_print(&addrList[i], addr, sizeof(addr), 0);
            ifaceList.push_back(std::string(addr));
        }
    }

    return ifaceList;
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
