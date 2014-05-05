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

#include <functional>
#include <mutex>
#include <condition_variable>
#include <map>
#include <string>
#include <vector>
#include <memory>

class SIPAccount;


class SipTransport {
    public:
        SipTransport(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool);
        ~SipTransport();

        /**
         * General Sip transport creation method according to the
         * transport type specified in account settings
         * @param account The account for which a transport must be created.
         */
        void createSipTransport(SIPAccount &account);

        /**
         * Initialize the transport selector
         * @param transport     A transport associated with an account
         * @return          	A transport selector structure
         */
        static inline pjsip_tpselector getTransportSelector(pjsip_transport *transport) {
            pjsip_tpselector tp = {PJSIP_TPSELECTOR_TRANSPORT, {transport}};
            return tp;
        }

        /**
         * This function returns a list of STUN mapped sockets for
         * a given set of socket file descriptors */
        std::vector<pj_sockaddr> getSTUNAddresses(const SIPAccount &account, std::vector<long> &socks) const;

        /**
         * Get the correct address to use (ie advertised) from
         * a uri. The corresponding transport that should be used
         * with that uri will be discovered.
         *
         * @param uri The uri from which we want to discover the address to use
         * @param transport The transport to use to discover the address
         */
        void findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, const std::string &host, std::string &address, pj_uint16_t &port) const;

        void findLocalAddressFromSTUN(pjsip_transport *transport, pj_str_t *stunServerName,
                int stunPort, std::string &address, pj_uint16_t &port) const;

        /**
         * Go through the transport list and remove unused ones.
         */
        void cleanupTransports();

        /**
         * Call released_cb(success) when transport tp is destroyed, making the
         * socket available for a new similar transport.
         * success is true if the transport is actually released.
         * TODO: make this call non-blocking.
         */
        void waitForReleased(pjsip_transport* tp, std::function<void(bool)> released_cb);

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
         * Returns a list of LOCKED transports that have to be processed and unlocked.
         */
        std::vector<pjsip_transport*> _cleanupTransports();

        static void tp_state_callback(pjsip_transport *, pjsip_transport_state, const pjsip_transport_state_info *);

        void transportStateChanged(pjsip_transport* tp, pjsip_transport_state state);

        /**
         * UDP Transports are stored in this map in order to retreive them in case
         * several accounts would share the same port number.
         */
        std::map<std::string, pjsip_transport*> transportMap_;
        std::mutex transportMapMutex_;
        std::condition_variable transportDestroyedCv_;

        pj_caching_pool& cp_;
        pj_pool_t& pool_;

        pjsip_endpoint *endpt_;
};

void sip_strerror(pj_status_t code);

#endif // SIPTRANSPORT_H_
