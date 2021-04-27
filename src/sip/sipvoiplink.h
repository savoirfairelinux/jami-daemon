/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "scheduled_executor.h"
#include "sip_events_handler.h"

#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>

#ifdef ENABLE_VIDEO
#include <queue>
#endif
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace jami {

class SIPAccountBase;
class SIPVoIPLink;
class SipTransportBroker;
class SipTransport;

pj_status_t try_respond_stateless(pjsip_endpoint* endpt,
                                  pjsip_rx_data* rdata,
                                  int st_code,
                                  const pj_str_t* st_text,
                                  const pjsip_hdr* hdr_list,
                                  const pjsip_msg_body* body);

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 */

class SIPVoIPLink
{
public:
    SIPVoIPLink();
    ~SIPVoIPLink();

    /**
     * Destroy structures
     */
    void shutdown();

    /**
     * Event listener. Each event send by the call manager is received and handled from here
     */
    void handleEvents(const pj_time_val timeout);

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
    pj_caching_pool* getMemoryPoolFactory();

    /**
     * Create the default UDP transport according ot Ip2Ip profile settings
     */
    void createDefaultSipUdpTransport();

    static void createSDPOffer(pjsip_inv_session* inv, pjmedia_sdp_session** p_offer);

    typedef std::function<void(std::vector<IpAddr>)> SrvResolveCallback;
    void resolveSrvName(const std::string& name,
                        pjsip_transport_type_e type,
                        SrvResolveCallback&& cb);

    /**
     * Guess the account related to an incoming SIP call.
     */
    std::shared_ptr<SIPAccountBase> guessAccount(std::string_view userName,
                                                 std::string_view server,
                                                 std::string_view fromUri) const;

    int getModId();
    pjsip_endpoint* getEndpoint();
    pjsip_module* getMod();

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
                                  int stunPort,
                                  std::string& address,
                                  pj_uint16_t& port) const;

    /**
     * Initialize the transport selector
     * @param transport     A transport associated with an account
     * @return          	A transport selector structure
     */
    static inline pjsip_tpselector getTransportSelector(pjsip_transport* transport)
    {
        pjsip_tpselector tp;
        tp.type = PJSIP_TPSELECTOR_TRANSPORT;
        tp.u.transport = transport;
        return tp;
    }

    std::shared_ptr<SipEventsHandler> getEventsHandler(pjsip_inv_session* inv);

    std::shared_ptr<ScheduledExecutor> getScheduler() const { return sipScheduler_; }

    /**
     * Instance that maintain and manage transport (UDP, TLS)
     */
    std::unique_ptr<SipTransportBroker> sipTransportBroker_;

private:
    NON_COPYABLE(SIPVoIPLink);

    mutable pj_caching_pool cp_;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
    std::atomic_bool running_ {true};
    std::thread sipThread_;
    // Scheduler used for SIP operations. All operations involving
    // SIP should use this scheduler in order to perform all
    // SIP operations on the same thread. This will prevent thread
    // race and reduce the need for mutexes.
    std::shared_ptr<ScheduledExecutor> sipScheduler_;
    std::shared_ptr<RepeatedTask> pollTask_;
};

} // namespace jami
