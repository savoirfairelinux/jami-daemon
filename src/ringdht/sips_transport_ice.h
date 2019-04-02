/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "security/tls_session.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "scheduled_executor.h"

#include <pjsip.h>
#include <pj/pool.h>

#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>
#include <queue>
#include <utility>
#include <vector>

namespace jami {
class IceTransport;
class IceSocketTransport;
} // namespace jami

namespace jami { namespace tls {

/**
 * SipsIceTransport
 *
 * Implements TLS transport as an pjsip_transport
 */
struct SipsIceTransport
{
    using clock = std::chrono::steady_clock;
    using TransportData = struct {
        pjsip_transport base; // do not move, SHOULD be the fist member
        SipsIceTransport* self {nullptr};
    };
    static_assert(std::is_standard_layout<TransportData>::value,
                  "TranportData requires standard-layout");

    SipsIceTransport(pjsip_endpoint* endpt, int tp_type, const TlsParams& param,
                    const std::shared_ptr<IceTransport>& ice, int comp_id);
    ~SipsIceTransport();

    void shutdown();

    std::shared_ptr<IceTransport> getIceTransport() const { return ice_; }
    pjsip_transport* getTransportBase() { return &trData_.base; }

    IpAddr getLocalAddress() const { return local_; }
    IpAddr getRemoteAddress() const { return remote_; }

    // uses the tls_ uniquepointer internal gnutls_session_t, to call its method to get its MTU
    uint16_t getTlsSessionMtu();

private:
    NON_COPYABLE(SipsIceTransport);

    std::shared_ptr<IceTransport> ice_;
    const int comp_id_;
    const std::function<int(unsigned, const gnutls_datum_t*, unsigned)> certCheck_;
    IpAddr local_ {};
    IpAddr remote_ {};

    // PJSIP transport backend

    TransportData trData_; // uplink to "this" (used by PJSIP called C-callbacks)

    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> pool_;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> rxPool_;

    pjsip_rx_data rdata_;

    pj_ssl_cert_info localCertInfo_;
    pj_ssl_cert_info remoteCertInfo_;

    pj_status_t verifyStatus_ {PJ_EUNKNOWN};

    // TlsSession backend

    struct ChangeStateEventData {
        pj_ssl_sock_info ssl_info;
        pjsip_tls_state_info tls_info;
        pjsip_transport_state_info state_info;
        decltype(PJSIP_TP_STATE_DISCONNECTED) state;
    };

    std::unique_ptr<IceSocketTransport> iceSocket_;
    std::unique_ptr<TlsSession> tls_;

    std::mutex txMutex_ {};
    std::condition_variable txCv_ {};
    std::list<pjsip_tx_data*> txQueue_ {};
    bool syncTx_ {false}; // true if we can send data synchronously (cnx established)

    std::mutex stateChangeEventsMutex_ {};
    std::list<ChangeStateEventData> stateChangeEvents_ {};

    std::mutex rxMtx_;
    std::list<std::vector<uint8_t>> rxPending_;

    ScheduledExecutor scheduler_;

    pj_status_t send(pjsip_tx_data*, const pj_sockaddr_t*, int, void*, pjsip_transport_callback);
    void handleEvents();
    void pushChangeStateEvent(ChangeStateEventData&&);
    void updateTransportState(pjsip_transport_state);
    void certGetInfo(pj_pool_t*, pj_ssl_cert_info*, const gnutls_datum_t*, size_t);
    void certGetCn(const pj_str_t*, pj_str_t*);
    void getInfo(pj_ssl_sock_info*, bool);
    void onTlsStateChange(TlsSessionState);
    void onRxData(std::vector<uint8_t>&&);
    void onCertificatesUpdate(const gnutls_datum_t*, const gnutls_datum_t*, unsigned int);
    int verifyCertificate(gnutls_session_t);
};

}} // namespace jami::tls
