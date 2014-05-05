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
#include <ccrtp/channel.h> // For ost::IPV*Address

#include <netinet/ip.h>

#include <string>
#include <vector>


/* An IPv4 equivalent to IN6_IS_ADDR_UNSPECIFIED */
#ifndef IN_IS_ADDR_UNSPECIFIED
#define IN_IS_ADDR_UNSPECIFIED(a) (((long int) (a)->s_addr) == 0x00000000)
#endif /* IN_IS_ADDR_UNSPECIFIED */


class IpAddr {
public:
    IpAddr(uint16_t family = AF_UNSPEC) : addr() {
        addr.addr.sa_family = family;
    }

    // From a sockaddr-type structure
    IpAddr(const IpAddr& other) : addr(other.addr) {}
    IpAddr(const pj_sockaddr& ip) : addr(ip) {}
    IpAddr(const sockaddr_in& ip) : addr() {
        memcpy(&addr, &ip, sizeof(sockaddr_in));
    }
    IpAddr(const sockaddr_in6& ip) : addr() {
        memcpy(&addr, &ip, sizeof(sockaddr_in6));
    }
    IpAddr(const sockaddr& ip) : addr() {
        memcpy(&addr, &ip, ip.sa_family == AF_INET6 ? sizeof addr.ipv6 : sizeof addr.ipv4);
    }
    IpAddr(const in6_addr& ip) : addr() {
        addr.addr.sa_family = AF_INET6;
        memcpy(&addr.ipv6.sin6_addr, &ip, sizeof(in6_addr));
    }
    IpAddr(const in_addr& ip) : addr() {
        addr.addr.sa_family = AF_INET;
        memcpy(&addr.ipv4.sin_addr, &ip, sizeof(in_addr));
    }

    // From a string
    IpAddr(const std::string& str, pj_uint16_t family = AF_UNSPEC) : addr() {
        pj_str_t pjstring;
        pj_cstr(&pjstring, str.c_str());
        auto status = pj_sockaddr_parse(family, 0, &pjstring, &addr);
        if (status != PJ_SUCCESS)
            addr.addr.sa_family = AF_UNSPEC;
    }

    inline bool operator==(const IpAddr& other) const {
        return pj_sockaddr_cmp(&addr, &other.addr) == 0;
    }

    // Is defined
    inline operator bool() const {
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

    inline operator sockaddr& (){
        return reinterpret_cast<sockaddr&>(addr);
    }

    inline pj_sockaddr* pjPtr() {
        return &addr;
    }

    inline operator ost::IPV4Host () const {
        assert(addr.addr.sa_family == AF_INET);
        const sockaddr_in& ipv4_addr = *reinterpret_cast<const sockaddr_in*>(&addr);
        return ost::IPV4Host(ipv4_addr.sin_addr);
    }

    inline operator ost::IPV6Host () const {
        assert(addr.addr.sa_family == AF_INET6);
#ifndef __ANDROID__ // hack for the ucommoncpp bug (fixed in our Android repo)
        ost::IPV6Host host = ost::IPV6Host(toString().c_str());
        return host;
#else
        const sockaddr_in6& ipv6_addr = *reinterpret_cast<const sockaddr_in6*>(&addr);
        return ost::IPV6Host(ipv6_addr.sin6_addr);
#endif
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
    pj_sockaddr addr;
};

namespace ip_utils {
    const std::string DEFAULT_INTERFACE = "default";

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
     * If family is unspecified, default to pj_AF_INET6() (if configured
     * with IPv6), or pj_AF_INET() otherwise.
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
}

#endif // IP_UTILS_H_
