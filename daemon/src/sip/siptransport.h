/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef SIPTRANSPORT_H_
#define SIPTRANSPORT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "noncopyable.h"
#include "logger.h"

#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>

#include <map>
#include <string>
#include <vector>
#include <memory>

class SIPAccount;

/* An IPv4 equivalent to IN6_IS_ADDR_UNSPECIFIED */
#ifndef IN_IS_ADDR_UNSPECIFIED
#define IN_IS_ADDR_UNSPECIFIED(a) (((long int) (a)->s_addr) == 0x00000000)
#endif /* IN_IS_ADDR_UNSPECIFIED */

class SipTransport {
    public:
        SipTransport(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool);

        static pj_sockaddr getSIPLocalIP(pj_uint16_t family = pj_AF_UNSPEC());

        /**
         * Get the IP for the network interface named ifaceName
         * @param forceIPv6 If IPv4 and IPv6 are available, will force to IPv6.
         */
        static std::string getInterfaceAddrFromName(const std::string &ifaceName, bool forceIPv6 = false);
        static pj_sockaddr getInterfaceAddr(const std::string &ifaceName, bool forceIPv6 = false);

        /**
        * List all the interfaces on the system and return
        * a vector list containing their name (eth0, eth0:1 ...).
        * @param void
        * @return std::vector<std::string> A std::string vector
        * of interface name available on all of the interfaces on
        * the system.
        */
        static std::vector<std::string> getAllIpInterfaceByName();

        /**
         * List all the interfaces on the system and return
         * a vector list containing their IP address.
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of IP address available on all of the interfaces on
         * the system.
         */
        static std::vector<std::string> getAllIpInterface();

        /**
         * General Sip transport creation method according to the
         * transport type specified in account settings
         * @param account The account for which a transport must be created.
         */
        void createSipTransport(SIPAccount &account, pj_uint16_t family = pj_AF_UNSPEC());

        /**
         * Initialize the transport selector
         * @param transport		A transport associated with an account
         *
         * @return          	A pointer to the transport selector structure
         */
        pjsip_tpselector *
        createTransportSelector(pjsip_transport *transport, pj_pool_t *tp_pool) const;

        /**
         * This function returns a list of STUN mapped sockets for
         * a given set of socket file descriptors */
        std::vector<pj_sockaddr>
        getSTUNAddresses(const SIPAccount &account,
                         std::vector<long> &socks) const;

        /**
         * Get the correct address to use (ie advertised) from
         * a uri. The corresponding transport that should be used
         * with that uri will be discovered.
         *
         * @param uri The uri from which we want to discover the address to use
         * @param transport The transport to use to discover the address
         */
        void findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, std::string &address, pj_uint16_t &port) const;

        void findLocalAddressFromSTUN(pjsip_transport *transport, pj_str_t *stunServerName,
                int stunPort, std::string &address, pj_uint16_t &port) const;

    private:
        NON_COPYABLE(SipTransport);

#if HAVE_TLS
        /**
         * Create a connection oriented TLS transport and register to the specified remote address.
         * First, initialize the TLS listener sole instance. This means that, for the momment, only one TLS transport
         * is allowed to be created in the application. Any subsequent account attempting to
         * register a new using this transport even if new settings are specified.
         * @param the account that is creating the TLS transport
         */
        pjsip_transport *
        createTlsTransport(SIPAccount &account);

        /**
         * Create The default TLS listener which is global to the application. This means that
         * only one TLS connection can be established for the momment.
         * @param the SIPAccount for which we are creating the TLS listener
         * @param IP protocol version to use, can be pj_AF_INET() or pj_AF_INET6()
         * @return a pointer to the new listener
         */
        pjsip_tpfactory *
        createTlsListener(SIPAccount &account, pj_uint16_t family = pj_AF_UNSPEC());
#endif

        /**
        * Create SIP UDP transport from account's setting
        * @param account The account for which a transport must be created.
        * @param IP protocol version to use, can be pj_AF_INET() or pj_AF_INET6()
        * @return a pointer to the new transport
        */
        pjsip_transport *createUdpTransport(const std::string &interface,
                                            pj_uint16_t port, pj_uint16_t family = pj_AF_UNSPEC());

        /**
         * Go through the transport list and remove unused ones.
         */
        void cleanupTransports();

        /**
         * UDP Transports are stored in this map in order to retreive them in case
         * several accounts would share the same port number.
         */
        std::map<std::string, pjsip_transport*> transportMap_;

        pj_caching_pool& cp_;
        pj_pool_t& pool_;

        pjsip_endpoint *endpt_;
};

void sip_strerror(pj_status_t code);

#endif // SIPTRANSPORT_H_
