/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ring_types.h"
#include "ip_utils.h"
#include "noncopyable.h"

#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>

#ifdef RING_VIDEO
#include <queue>
#endif
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace ring {

class SIPCall;
class SIPAccountBase;
class SIPVoIPLink;
class SipTransportBroker;

typedef std::map<std::string, std::shared_ptr<SIPCall> > SipCallMap;

extern decltype(getGlobalInstance<SIPVoIPLink>)& getSIPVoIPLink;

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 *          This class is based on the singleton design pattern.
 *          One SIPVoIPLink can handle multiple SIP accounts, but all the SIP accounts have all the same SIPVoIPLink
 */

class SIPVoIPLink {
    public:
        SIPVoIPLink();
        ~SIPVoIPLink();

        /**
         * Event listener. Each event send by the call manager is received and handled from here
         */
        void handleEvents();

        /**
         * Register a new keepalive registration timer to this endpoint
         */
        void registerKeepAliveTimer(pj_timer_entry& timer, pj_time_val& delay);

        /**
         * Abort currently registered timer
         */
        void cancelKeepAliveTimer(pj_timer_entry& timer);

        /**
         * Get the memory pool factory since each calls has its own memory pool
         */
        pj_caching_pool *getMemoryPoolFactory();

        /**
         * Create the default UDP transport according ot Ip2Ip profile settings
         */
        void createDefaultSipUdpTransport();

    public:
        static void createSDPOffer(pjsip_inv_session *inv,
                                   pjmedia_sdp_session **p_offer);

        /**
         * Instance that maintain and manage transport (UDP, TLS)
         */
        std::unique_ptr<SipTransportBroker> sipTransportBroker;

        typedef std::function<void(std::vector<IpAddr>)> SrvResolveCallback;
        void resolveSrvName(const std::string &name, pjsip_transport_type_e type, SrvResolveCallback&& cb);

        /**
         * Guess the account related to an incoming SIP call.
         */
        std::shared_ptr<SIPAccountBase>
        guessAccount(const std::string& userName,
                     const std::string& server,
                     const std::string& fromUri) const;

        int getModId();
        pjsip_endpoint * getEndpoint();
        pjsip_module * getMod();

        pj_caching_pool* getCachingPool() noexcept;
        pj_pool_t* getPool() noexcept;

        /**
         * Get the correct address to use (ie advertised) from
         * a uri. The corresponding transport that should be used
         * with that uri will be discovered.
         *
         * @param uri The uri from which we want to discover the address to use
         * @param transport The transport to use to discover the address
         */
        void findLocalAddressFromTransport(pjsip_transport* transport,
                                           pjsip_transport_type_e transportType,
                                           const std::string& host,
                                           std::string& address,
                                           pj_uint16_t& port) const;

        bool findLocalAddressFromSTUN(pjsip_transport* transport,
                                      pj_str_t* stunServerName,
                                      int stunPort, std::string& address,
                                      pj_uint16_t& port) const;

        /**
         * Initialize the transport selector
         * @param transport     A transport associated with an account
         * @return          	A transport selector structure
         */
        static inline pjsip_tpselector getTransportSelector(pjsip_transport *transport) {
            pjsip_tpselector tp = {PJSIP_TPSELECTOR_TRANSPORT, {transport}};
            return tp;
        }

    private:
        NON_COPYABLE(SIPVoIPLink);

        mutable pj_caching_pool cp_;
        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
        std::atomic_bool running_ {true};
        std::thread sipThread_;

#ifdef RING_VIDEO
        void requestKeyframe(const std::string &callID);
#endif

        friend class SIPTest;
};

} // namespace ring
