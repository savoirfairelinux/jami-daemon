/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#pragma once

#include <sstream> // include before pjlib.h to fix macros issues with pjlib.h

extern "C" {
#include <pjlib.h>
}

#ifdef HAVE_CONFIG
 #include <config.h>
#endif

#include <ciso646> // fix windows compiler bug

#ifdef _WIN32
    #ifdef RING_UWP
        #define _WIN32_WINNT 0x0A00
    #else
        #define _WIN32_WINNT 0x0601
    #endif
    #include <ws2tcpip.h>

    //define in mingw
    #ifdef interface
    #undef interface
    #endif
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <arpa/nameser.h>
    #include <resolv.h>
    #include <netdb.h>
    #include <netinet/ip.h>
    #include <net/if.h>
    #include <sys/ioctl.h>
#endif

#include <string>
#include <vector>


/* An IPv4 equivalent to IN6_IS_ADDR_UNSPECIFIED */
#ifndef IN_IS_ADDR_UNSPECIFIED
#define IN_IS_ADDR_UNSPECIFIED(a) (((long int) (a)->s_addr) == 0x00000000)
#endif /* IN_IS_ADDR_UNSPECIFIED */

namespace jami {

/**
 * Binary representation of an IP address.
 */
class IpAddr {
public:
    IpAddr() : IpAddr(AF_UNSPEC) {}
    IpAddr(const IpAddr&) = default;
    IpAddr(IpAddr&&) = default;
    IpAddr& operator=(const IpAddr&) = default;
    IpAddr& operator=(IpAddr&&) = default;

    explicit IpAddr(uint16_t family) : addr() {
        addr.addr.sa_family = family;
    }

    IpAddr(const pj_sockaddr& ip) : addr(ip) {}

    IpAddr(const pj_sockaddr& ip, socklen_t len) : addr() {
        if (len > sizeof(addr))
            throw std::invalid_argument("IpAddr(): length overflows internal storage type");
        memcpy(&addr, &ip, len);
    }

    IpAddr(const sockaddr& ip) : addr() {
        memcpy(&addr, &ip, ip.sa_family == AF_INET6 ? sizeof addr.ipv6 : sizeof addr.ipv4);
    }

    IpAddr(const sockaddr_in& ip) : addr() {
        static_assert(sizeof(ip) <= sizeof(addr), "sizeof(sockaddr_in) too large");
        memcpy(&addr, &ip, sizeof(sockaddr_in));
    }

    IpAddr(const sockaddr_in6& ip) : addr() {
        static_assert(sizeof(ip) <= sizeof(addr), "sizeof(sockaddr_in6) too large");
        memcpy(&addr, &ip, sizeof(sockaddr_in6));
    }

    IpAddr(const sockaddr_storage& ip) : IpAddr(*reinterpret_cast<const sockaddr*>(&ip)) {}

    IpAddr(const in_addr& ip) : addr() {
        static_assert(sizeof(ip) <= sizeof(addr), "sizeof(in_addr) too large");
        addr.addr.sa_family = AF_INET;
        memcpy(&addr.ipv4.sin_addr, &ip, sizeof(in_addr));
    }

    IpAddr(const in6_addr& ip) : addr() {
        static_assert(sizeof(ip) <= sizeof(addr), "sizeof(in6_addr) too large");
        addr.addr.sa_family = AF_INET6;
        memcpy(&addr.ipv6.sin6_addr, &ip, sizeof(in6_addr));
    }

    IpAddr(const std::string& str, pj_uint16_t family = AF_UNSPEC) : addr() {
        if (str.empty()) {
            addr.addr.sa_family = AF_UNSPEC;
            return;
        }
        const pj_str_t pjstring {(char*)str.c_str(), (pj_ssize_t)str.size()};
        auto status = pj_sockaddr_parse(family, 0, &pjstring, &addr);
        if (status != PJ_SUCCESS)
            addr.addr.sa_family = AF_UNSPEC;
    }

    // Is defined
    inline explicit operator bool() const {
        return isIpv4() or isIpv6();
    }

    inline explicit operator bool() {
        return isIpv4() or isIpv6();
    }

    inline operator pj_sockaddr& () {
        return addr;
    }

    inline operator const pj_sockaddr& () const {
        return addr;
    }

    inline operator pj_sockaddr_in& () {
        return addr.ipv4;
    }

    inline operator const pj_sockaddr_in& () const {
        assert(addr.addr.sa_family != AF_INET6);
        return addr.ipv4;
    }

    inline operator pj_sockaddr_in6& () {
        return addr.ipv6;
    }

    inline operator const pj_sockaddr_in6& () const {
        assert(addr.addr.sa_family == AF_INET6);
        return addr.ipv6;
    }

    inline operator const sockaddr& () const {
        return reinterpret_cast<const sockaddr&>(addr);
    }

    inline operator const sockaddr* () const {
        return reinterpret_cast<const sockaddr*>(&addr);
    }

    inline operator sockaddr_storage (){
        sockaddr_storage ss;
        memcpy(&ss, &addr, getLength());
        return ss;
    }

    inline const pj_sockaddr* pjPtr() const {
        return &addr;
    }

    inline pj_sockaddr* pjPtr() {
        return &addr;
    }

    inline operator std::string () const {
        return toString();
    }

    std::string toString(bool include_port=false, bool force_ipv6_brackets=false) const {
        std::string str(PJ_INET6_ADDRSTRLEN, (char)0);
        if (include_port) force_ipv6_brackets = true;
        pj_sockaddr_print(&addr, &(*str.begin()), PJ_INET6_ADDRSTRLEN, (include_port?1:0)|(force_ipv6_brackets?2:0));
        str.resize(std::char_traits<char>::length(str.c_str()));
        return str;
    }

    void setPort(uint16_t port) {
        pj_sockaddr_set_port(&addr, port);
    }

    inline uint16_t getPort() const {
        if (not *this)
            return 0;
        return pj_sockaddr_get_port(&addr);
    }

    inline socklen_t getLength() const {
        if (not *this)
            return 0;
        return pj_sockaddr_get_len(&addr);
    }

    inline uint16_t getFamily() const {
        return addr.addr.sa_family;
    }

    inline bool isIpv4() const {
        return addr.addr.sa_family == AF_INET;
    }

    inline bool isIpv6() const {
        return addr.addr.sa_family == AF_INET6;
    }

    /**
     * Return true if address is a loopback IP address.
     */
    bool isLoopback() const;

    /**
     * Return true if address is not a public IP address.
     */
    bool isPrivate() const;

    bool isUnspecified() const;

    /**
     * Return true if address is a valid IPv6.
     */
    inline static bool isIpv6(const std::string& address) {
        return isValid(address, AF_INET6);
    }

    /**
     * Return true if address is a valid IP address of specified family (if provided) or of any kind (default).
     * Does not resolve hostnames.
     */
    static bool isValid(const std::string& address, pj_uint16_t family = pj_AF_UNSPEC());

private:
    pj_sockaddr addr {};
};

// IpAddr helpers
inline bool operator==(const IpAddr& lhs, const IpAddr& rhs) { return !pj_sockaddr_cmp(&lhs, &rhs); }
inline bool operator!=(const IpAddr& lhs, const IpAddr& rhs) { return !(lhs == rhs); }
inline bool operator<(const IpAddr& lhs, const IpAddr& rhs) { return pj_sockaddr_cmp(&lhs, &rhs) < 0; }
inline bool operator>(const IpAddr& lhs, const IpAddr& rhs) { return pj_sockaddr_cmp(&lhs, &rhs) > 0; }
inline bool operator<=(const IpAddr& lhs, const IpAddr& rhs) { return pj_sockaddr_cmp(&lhs, &rhs) <= 0; }
inline bool operator>=(const IpAddr& lhs, const IpAddr& rhs) { return pj_sockaddr_cmp(&lhs, &rhs) >= 0; }

namespace ip_utils {

static const char *const DEFAULT_INTERFACE = "default";

std::string getHostname();

std::string getDeviceName();

/**
 * Return the generic "any host" IP address of the specified family.
 * If family is unspecified, default to pj_AF_INET6() (IPv6).
 */
IpAddr getAnyHostAddr(pj_uint16_t family = pj_AF_UNSPEC());

/**
 * Return the first host IP address of the specified family.
 * If no address of the specified family is found, another family will
 * be tried.
 * Ex. : if family is pj_AF_INET6() (IPv6/default) and the system does not
 * have an IPv6 address, an IPv4 address will be returned if available.
 *
 * If family is unspecified, default to pj_AF_INET6() if compiled
 * with IPv6, or pj_AF_INET() otherwise.
 */
IpAddr getLocalAddr(pj_uint16_t family = pj_AF_UNSPEC());

/**
 * Get the IP address of the network interface interface with the specified
 * address family, or of any address family if unspecified (default).
 */
IpAddr getInterfaceAddr(const std::string &interface, pj_uint16_t family = pj_AF_UNSPEC());

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

std::vector<IpAddr> getAddrList(const std::string &name, pj_uint16_t family = pj_AF_UNSPEC());

bool haveCommonAddr(const std::vector<IpAddr>& a, const std::vector<IpAddr>& b);

std::vector<IpAddr> getLocalNameservers();

} // namespace ip_utils
} // namespace jami
