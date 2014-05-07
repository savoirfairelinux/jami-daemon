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

#include "sip/sip_utils.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

std::vector<IpAddr>
ip_utils::getAddrList(const std::string &name, pj_uint16_t family)
{
    std::vector<IpAddr> ipList;
    if (name.empty())
        return ipList;
    if (IpAddr::isValid(name, family)) {
        ipList.push_back(name);
        return ipList;
    }

    static const unsigned MAX_ADDR_NUM = 128;
    pj_addrinfo res[MAX_ADDR_NUM];
    unsigned addr_num = MAX_ADDR_NUM;
    pj_str_t pjname;
    pj_cstr(&pjname, name.c_str());
    auto status = pj_getaddrinfo(family, &pjname, &addr_num, res);
    if (status != PJ_SUCCESS) {
        ERROR("Error resolving %s :", name.c_str());
        sip_utils::sip_strerror(status);
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
ip_utils::haveCommonAddr(const std::vector<IpAddr>& a, const std::vector<IpAddr>& b)
{
    for (const auto &i : a) {
        for (const auto &j : b) {
            if (i == j) return true;
        }
    }
    return false;
}

IpAddr
ip_utils::getAnyHostAddr(pj_uint16_t family)
{
    if (family == pj_AF_UNSPEC()) {
#if HAVE_IPV6
        family = pj_AF_INET6();
#else
        family = pj_AF_INET();
#endif
    }
    return IpAddr(family);
}

IpAddr
ip_utils::getLocalAddr(pj_uint16_t family)
{
    if (family == pj_AF_UNSPEC()) {
#if HAVE_IPV6
        family = pj_AF_INET6();
#else
        family = pj_AF_INET();
#endif
    }
    IpAddr ip_addr = {};
    pj_status_t status = pj_gethostip(family, ip_addr.pjPtr());
    if (status == PJ_SUCCESS) {
        return ip_addr;
    }
#if HAVE_IPV6
    WARN("Could not get preferred address familly (%s)", (family == pj_AF_INET6()) ? "IPv6" : "IPv4");
    family = (family == pj_AF_INET()) ? pj_AF_INET6() : pj_AF_INET();
    status = pj_gethostip(family, ip_addr);
    if (status == PJ_SUCCESS) return ip_addr;
#endif
    ERROR("Could not get local IP");
    return ip_addr;
}

IpAddr
ip_utils::getInterfaceAddr(const std::string &interface, pj_uint16_t family)
{
    if (interface == DEFAULT_INTERFACE)
        return getLocalAddr(family);

    const auto unix_family = family == pj_AF_INET() ? AF_INET : AF_INET6;
    IpAddr addr = {};

    int fd = socket(unix_family, SOCK_DGRAM, 0);
    if (fd < 0) {
        ERROR("Could not open socket: %m");
        return addr;
    }

    if (unix_family == AF_INET6) {
        int val = family != pj_AF_UNSPEC();
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &val, sizeof(val)) < 0) {
            ERROR("Could not setsockopt: %m");
            close(fd);
            return addr;
        }
    }

    ifreq ifr;
    strncpy(ifr.ifr_name, interface.c_str(), sizeof ifr.ifr_name);
    // guarantee that ifr_name is NULL-terminated
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    ifr.ifr_addr.sa_family = unix_family;

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    addr = ifr.ifr_addr;
    if (addr.isUnspecified())
        return getLocalAddr(addr.getFamily());

    ERROR("ip_utils::getInterfaceAddr %s %s", interface.c_str(), addr.toString().c_str());
    return addr;
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
IpAddr::isValid(const std::string &address, pj_uint16_t family)
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

bool
IpAddr::isUnspecified() const
{
    switch (addr.addr.sa_family) {
    case AF_INET:
        return IN_IS_ADDR_UNSPECIFIED(reinterpret_cast<const in_addr*>(&addr.ipv4.sin_addr));
    case AF_INET6:
        return IN6_IS_ADDR_UNSPECIFIED(reinterpret_cast<const in6_addr*>(&addr.ipv6.sin6_addr));
    default:
        return true;
    }
}

bool
IpAddr::isLoopback() const
{
    switch (addr.addr.sa_family) {
    case AF_INET: {
        uint8_t b1 = (uint8_t)(addr.ipv4.sin_addr.s_addr >> 24);
        return b1 == 127;
    }
    case AF_INET6:
        return IN6_IS_ADDR_LOOPBACK(reinterpret_cast<const in6_addr*>(&addr.ipv6.sin6_addr));
    default:
        return false;
    }
}

bool
IpAddr::isPrivate() const
{
    if (isLoopback()) {
        ERROR("IpAddr::isPrivate: %s LOOPBACK", toString().c_str());
        return true;
    }
    switch (addr.addr.sa_family) {
    case AF_INET:
        uint8_t b1, b2;
        b1 = (uint8_t)(addr.ipv4.sin_addr.s_addr >> 24);
        b2 = (uint8_t)((addr.ipv4.sin_addr.s_addr >> 16) & 0x0ff);
        // 10.x.y.z
        if (b1 == 10)
            return true;
        // 172.16.0.0 - 172.31.255.255
        if ((b1 == 172) && (b2 >= 16) && (b2 <= 31))
            return true;
        // 192.168.0.0 - 192.168.255.255
        if ((b1 == 192) && (b2 == 168))
            return true;
        return false;
    case AF_INET6: {
        const pj_uint8_t* addr6 = reinterpret_cast<const pj_uint8_t*>(&addr.ipv6.sin6_addr);
        if (addr6[0] == 0xfc)
            return true;
        return false;
    }
    default:
        return false;
    }
}
