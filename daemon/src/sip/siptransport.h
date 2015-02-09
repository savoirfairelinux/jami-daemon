/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
#include <pjnath/stun_config.h>

#include <functional>
#include <mutex>
#include <condition_variable>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <memory>

#define DEFAULT_SIP_PORT    5060
#define DEFAULT_SIP_TLS_PORT 5061

namespace ring {

struct SipTransportDescr
{
    SipTransportDescr() {}
    SipTransportDescr(pjsip_transport_type_e t)
     : type(t), listenerPort(pjsip_transport_get_default_port_for_type(t)) {}
    SipTransportDescr(pjsip_transport_type_e t, pj_uint16_t port, std::string i)
     : type(t), listenerPort(port), interface(i) {}

    static inline pjsip_transport_type_e actualType(pjsip_transport_type_e t) {
        return (t == PJSIP_TRANSPORT_START_OTHER) ? PJSIP_TRANSPORT_UDP : t;
    }

    inline bool operator==(SipTransportDescr const& o) const {
        return actualType(type) == actualType(o.type)
         && listenerPort == o.listenerPort
         && interface == o.interface;
    }

    inline bool operator<(SipTransportDescr const& o) const {
        return actualType(type) < actualType(o.type)
         || listenerPort < o.listenerPort
         || std::hash<std::string>()(interface) < std::hash<std::string>()(o.interface);
    }

    std::string toString() const;

    pjsip_transport_type_e type {PJSIP_TRANSPORT_UNSPECIFIED};
    pj_uint16_t listenerPort {DEFAULT_SIP_PORT};
    std::string interface {"default"};
};

struct TlsListener
{
    TlsListener() {}
    TlsListener(pjsip_tpfactory* f) : listener(f) {}
    virtual ~TlsListener() {
        RING_DBG("Destroying listener");
        listener->destroy(listener);
    }
    pjsip_tpfactory* get() {
        return listener;
    }
private:
    NON_COPYABLE(TlsListener);
    pjsip_tpfactory* listener {nullptr};
};

using SipTransportStateCallback = std::function<void(pjsip_transport_state, const pjsip_transport_state_info*)>;

/**
 * SIP transport wraps pjsip_transport.
 */
class SipTransport
{
    public:
        SipTransport(pjsip_transport*);
        SipTransport(pjsip_transport*, const std::shared_ptr<TlsListener>&);

        ~SipTransport();

        static const char* stateToStr(pjsip_transport_state state);

        void stateCallback(pjsip_transport_state state, const pjsip_transport_state_info *info);

        pjsip_transport* get() {
            return transport_.get();
        }

        void addStateListener(uintptr_t lid, SipTransportStateCallback cb);
        bool removeStateListener(uintptr_t lid);

        bool isSecure() const {
            return PJSIP_TRANSPORT_IS_SECURE(transport_);
        }

        static bool isAlive(const std::shared_ptr<SipTransport>&, pjsip_transport_state state);

    private:
        NON_COPYABLE(SipTransport);

        static void deleteTransport(pjsip_transport* t);

        std::unique_ptr<pjsip_transport, decltype(deleteTransport)&> transport_;
        std::shared_ptr<TlsListener> tlsListener_;
        std::map<uintptr_t, SipTransportStateCallback> stateListeners_;
        std::mutex stateListenersMutex_;
};

class IpAddr;
class IceTransport;
class SipIceTransport;
class SipsIceTransport;
class TlsParams;

/**
 * Manages the transports and receive callbacks from PJSIP
 */
class SipTransportBroker
{
public:
    SipTransportBroker(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool);
    ~SipTransportBroker();

    std::shared_ptr<SipTransport> getUdpTransport(const SipTransportDescr&);

#if HAVE_TLS
    std::shared_ptr<TlsListener> getTlsListener(const SipTransportDescr&, const pjsip_tls_setting*);

    std::shared_ptr<SipTransport> getTlsTransport(const std::shared_ptr<TlsListener>&, const IpAddr& remote);
#endif

#if HAVE_DHT
    std::shared_ptr<SipTransport> getIceTransport(const std::shared_ptr<IceTransport>, unsigned comp_id);
    std::shared_ptr<SipTransport> getTlsIceTransport(const std::shared_ptr<IceTransport>, unsigned comp_id, const TlsParams&);
#endif

    std::shared_ptr<SipTransport> addTransport(pjsip_transport*);

    /**
     * Start gracefull shutdown procedure for all transports
     */
    void shutdown();

    void transportStateChanged(pjsip_transport*, pjsip_transport_state, const pjsip_transport_state_info*);

private:
    NON_COPYABLE(SipTransportBroker);

    /**
    * Create SIP UDP transport from account's setting
    * @param account The account for which a transport must be created.
    * @param IP protocol version to use, can be pj_AF_INET() or pj_AF_INET6()
    * @return a pointer to the new transport
    */
    std::shared_ptr<SipTransport> createUdpTransport(const SipTransportDescr&);

    /**
     * List of transports so we can bubble the events up.
     */
    std::map<pjsip_transport*, std::weak_ptr<SipTransport>> transports_ {};
    std::mutex transportMapMutex_ {};

    /**
     * Transports are stored in this map in order to retreive them in case
     * several accounts would share the same port number.
     */
    std::map<SipTransportDescr, pjsip_transport*> udpTransports_;

    /**
     * Storage for SIP/ICE transport instances.
     */
#if HAVE_DHT
    int ice_pj_transport_type_ {PJSIP_TRANSPORT_START_OTHER};
#endif

    pj_caching_pool& cp_;
    pj_pool_t& pool_;
    pjsip_endpoint *endpt_;

};

} // namespace ring

#endif // SIPTRANSPORT_H_
