/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
 */

#include "ip_utils.h"
#ifdef s_addr
    #undef s_addr
#endif
#include "logger.h"

#include "sip/sip_utils.h"

#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#include "client/ring_signal.h"
#endif

#ifdef _WIN32
#define InetPtonA inet_pton
WINSOCK_API_LINKAGE INT WSAAPI InetPtonA(INT Family, LPCSTR pStringBuf, PVOID pAddr);
#else
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#endif

#ifndef HOST_NAME_MAX
#ifdef MAX_COMPUTERNAME_LENGTH
#define HOST_NAME_MAX MAX_COMPUTERNAME_LENGTH
#else
// Max 255 chars as per RFC 1035
#define HOST_NAME_MAX 255
#endif
#endif

namespace jami {

std::string
ip_utils::getHostname()
{
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, HOST_NAME_MAX))
        return {};
    return hostname;
}

int
ip_utils::getHostName(char *out, size_t out_len)
{
    char tempstr[INET_ADDRSTRLEN];
    const char *p = NULL;
#ifdef _WIN32
    struct hostent* h = NULL;
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    gethostname(out, out_len);
    h = gethostbyname(out);
    if (h != NULL) {
        memcpy(&localAddr.sin_addr, h->h_addr_list[0], 4);
        p = inet_ntop(AF_INET, &localAddr.sin_addr, tempstr, sizeof(tempstr));
        if (p)
            strncpy(out, p, out_len);
        else
            return -1;
    } else {
        return -1;
    }
#elif (defined(BSD) && BSD >= 199306) || defined(__FreeBSD_kernel__)
    int retVal = 0;
    struct ifaddrs* ifap;
    struct ifaddrs* ifa;
    if (getifaddrs(&ifap) != 0)
        return -1;
    // Cycle through available interfaces.
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        // Skip loopback, point-to-point and down interfaces.
        // except don't skip down interfaces if we're trying to get
        // a list of configurable interfaces.
        if ((ifa->ifa_flags & IFF_LOOPBACK) || (!( ifa->ifa_flags & IFF_UP)))
            continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                // We don't want the loopback interface. Go to next one.
                continue;
            }
            p = inet_ntop(AF_INET, &((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr, tempstr, sizeof(tempstr));
            if (p)
                strncpy(out, p, out_len);
            else
                retVal = -1;
            break;
        }
    }
    freeifaddrs(ifap);
    retVal = ifa ? 0 : -1;
    return retVal;
#else
    struct ifconf ifConf;
    struct ifreq ifReq;
    struct sockaddr_in localAddr;
    char szBuffer[MAX_INTERFACE * sizeof (struct ifreq)];
    int nResult;
    int localSock;
    memset(&ifConf,  0, sizeof(ifConf));
    memset(&ifReq,   0, sizeof(ifReq));
    memset(szBuffer, 0, sizeof(szBuffer));
    memset(&localAddr, 0, sizeof(localAddr));
    // Create an unbound datagram socket to do the SIOCGIFADDR ioctl on.
    localSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (localSock == INVALID_SOCKET)
        return -1;
    /* Get the interface configuration information... */
    ifConf.ifc_len = (int)sizeof szBuffer;
    ifConf.ifc_ifcu.ifcu_buf = (caddr_t) szBuffer;
    nResult = ioctl(localSock, SIOCGIFCONF, &ifConf);
    if (nResult < 0) {
        close(localSock);
        return -1;
    }
    unsigned int i;
    unsigned int j = 0;
    // Cycle through the list of interfaces looking for IP addresses.
    for (i = 0u; i < (unsigned int)ifConf.ifc_len && j < MIN_INTERFACE; ) {
        struct ifreq *pifReq = (struct ifreq *)((caddr_t)ifConf.ifc_req + i);
        i += sizeof *pifReq;
        // See if this is the sort of interface we want to deal with.
        memset(ifReq.ifr_name, 0, sizeof(ifReq.ifr_name));
        strncpy(ifReq.ifr_name, pifReq->ifr_name, sizeof(ifReq.ifr_name) - 1);
        ioctl(localSock, SIOCGIFFLAGS, &ifReq);
        // Skip loopback, point-to-point and down interfaces.
        // except don't skip down interfaces if we're trying to get
        // a list of configurable interfaces.
        if ((ifReq.ifr_flags & IFF_LOOPBACK) || (!(ifReq.ifr_flags & IFF_UP)))
            continue;
        if (pifReq->ifr_addr.sa_family == AF_INET) {
            memcpy(&localAddr, &pifReq->ifr_addr, sizeof pifReq->ifr_addr);
            if (localAddr.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                // We don't want the loopback interface. Go to the next one.
                continue;
            }
        }
        j++;    // Increment j if we found an address which is not loopback and is up.
    }
    close(localSock);
    p = inet_ntop(AF_INET, &localAddr.sin_addr, tempstr, sizeof(tempstr));
    if (p)
        strncpy(out, p, out_len);
    else
        return -1;
#endif
    return 0;
}
std::string
ip_utils::getGateway(char* localHost, ip_utils::subnet_mask prefix)
{
    std::string localHostStr(localHost);
    if (prefix == ip_utils::subnet_mask::prefix_32bit)
        return localHostStr;
    std::string defaultGw {};
    std::istringstream iss(localHostStr);
    // Make a vector of each individual number in the ip address.
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(iss, token, '.')) {
        if (!token.empty())
            tokens.push_back(token);
    }
    // Build a gateway address from the individual ip components.
    for (unsigned int i = 0; i <=(unsigned int)prefix; i++)
        defaultGw += tokens[i] + ".";
    for (unsigned int i = (unsigned int)ip_utils::subnet_mask::prefix_32bit; i > (unsigned int)prefix+1; i--)
        defaultGw += "0.";
    defaultGw += "1";
    return defaultGw;
}

std::string
ip_utils::getDeviceName()
{
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> deviceNames;
    emitSignal<DRing::ConfigurationSignal::GetDeviceName>(&deviceNames);
    if (not deviceNames.empty()) {
        return deviceNames[0];
    }
#endif
    return getHostname();
}

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

    static constexpr unsigned MAX_ADDR_NUM = 128;
    pj_addrinfo res[MAX_ADDR_NUM];
    unsigned addr_num = MAX_ADDR_NUM;
    const pj_str_t pjname(sip_utils::CONST_PJ_STR(name));
    auto status = pj_getaddrinfo(family, &pjname, &addr_num, res);
    if (status != PJ_SUCCESS) {
        JAMI_ERR("Error resolving %.*s : %s", (int)name.size(), name.c_str(),
                 sip_utils::sip_strerror(status).c_str());
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
ip_utils::getLocalAddr(pj_uint16_t family)
{
    IpAddr ip_addr {};
    pj_status_t status = pj_gethostip(family, ip_addr.pjPtr());
    if (status == PJ_SUCCESS) {
        return ip_addr;
    }
    JAMI_WARN("Could not get preferred address familly (%s)", (family == pj_AF_INET6()) ? "IPv6" : "IPv4");
    family = (family == pj_AF_INET()) ? pj_AF_INET6() : pj_AF_INET();
    status = pj_gethostip(family, ip_addr.pjPtr());
    if (status == PJ_SUCCESS) {
        return ip_addr;
    }
    JAMI_ERR("Could not get local IP");
    return ip_addr;
}

IpAddr
ip_utils::getInterfaceAddr(const std::string &interface, pj_uint16_t family)
{
    if (interface == DEFAULT_INTERFACE)
        return getLocalAddr(family);

    IpAddr addr {};

#ifndef _WIN32
    const auto unix_family = family == pj_AF_INET() ? AF_INET : AF_INET6;

    int fd = socket(unix_family, SOCK_DGRAM, 0);
    if (fd < 0) {
        JAMI_ERR("Could not open socket: %m");
        return addr;
    }

    if (unix_family == AF_INET6) {
        int val = family != pj_AF_UNSPEC();
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &val, sizeof(val)) < 0) {
            JAMI_ERR("Could not setsockopt: %m");
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
#else // _WIN32
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct sockaddr_in  *sockaddr_ipv4;
    struct sockaddr_in6 *sockaddr_ipv6;

    ZeroMemory(&hints, sizeof(hints));

    DWORD dwRetval = getaddrinfo(interface.c_str(), "0", &hints, &result);
    if (dwRetval != 0) {
        JAMI_ERR("getaddrinfo failed with error: %lu", dwRetval);
        return addr;
    }

    switch (result->ai_family) {
        sockaddr_ipv4 = (struct sockaddr_in *) result->ai_addr;
        addr = sockaddr_ipv4->sin_addr;
        break;
        case AF_INET6:
        sockaddr_ipv6 = (struct sockaddr_in6 *) result->ai_addr;
        addr = sockaddr_ipv6->sin6_addr;
        break;
        default:
        break;
    }

    if (addr.isUnspecified())
            return getLocalAddr(addr.getFamily());
#endif // !_WIN32

    return addr;
}

std::vector<std::string>
ip_utils::getAllIpInterfaceByName()
{
    std::vector<std::string> ifaceList;
    ifaceList.push_back("default");
#ifndef _WIN32
    static ifreq ifreqs[20];
    ifconf ifconf;

    ifconf.ifc_buf = (char*) (ifreqs);
    ifconf.ifc_len = sizeof(ifreqs);

    int sock = socket(AF_INET6, SOCK_STREAM, 0);

    if (sock >= 0) {
        if (ioctl(sock, SIOCGIFCONF, &ifconf) >= 0)
            for (unsigned i = 0; i < ifconf.ifc_len / sizeof(ifreq); ++i)
                ifaceList.push_back(std::string(ifreqs[i].ifr_name));

        close(sock);
    }

#else
        JAMI_ERR("Not implemented yet. (iphlpapi.h problem)");
#endif
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

std::vector<IpAddr>
ip_utils::getLocalNameservers()
{
    std::vector<IpAddr> res;
#if defined __ANDROID__ || defined _WIN32 || TARGET_OS_IPHONE
#ifdef _MSC_VER
#pragma message (__FILE__ "(" STR2(__LINE__) ") : -NOTE- " "Not implemented")
#else
#warning "Not implemented"
#endif
#else
    if (not (_res.options & RES_INIT))
        res_init();
    res.insert(res.end(), _res.nsaddr_list, _res.nsaddr_list + _res.nscount);
#endif
    return res;
}

bool
IpAddr::isValid(const std::string &address, pj_uint16_t family)
{
    const pj_str_t pjstring(sip_utils::CONST_PJ_STR(address));
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
        return IN_IS_ADDR_UNSPECIFIED(&addr.ipv4.sin_addr);
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
        auto addr_host = ntohl(addr.ipv4.sin_addr.s_addr);
        uint8_t b1 = (uint8_t)(addr_host >> 24);
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
        return true;
    }
    switch (addr.addr.sa_family) {
    case AF_INET: {
        auto addr_host = ntohl(addr.ipv4.sin_addr.s_addr);
        uint8_t b1, b2;
        b1 = (uint8_t)(addr_host >> 24);
        b2 = (uint8_t)((addr_host >> 16) & 0x0ff);
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
    }
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

} // namespace jami
