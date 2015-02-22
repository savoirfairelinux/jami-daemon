/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#pragma once

#include "ip_utils.h"
#include "threadloop.h"

#include <opendht/crypto.h>

#include <pjsip.h>
#include <pj/pool.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace ring {
class IceTransport;
} // namespace ring

namespace ring { namespace tls {

enum class TlsConnectionState {
    DISCONNECTED,
    COOKIE,
    HANDSHAKING,
    ESTABLISHED
};

struct TlsParams {
    std::string ca_list;
    dht::crypto::Identity id;
    std::shared_ptr<gnutls_dh_params_int> dh_params;
    std::chrono::steady_clock::duration timeout;
    std::function<pj_status_t(unsigned status,
                              const gnutls_datum_t* cert_list,
                              unsigned cert_list_size)> cert_check;
};

struct SipsIceTransport
{
    using clock = std::chrono::steady_clock;
    using TransportData = struct {
        pjsip_transport base; // do not move, SHOULD be the fist member
        SipsIceTransport* self {nullptr};
    };
    static_assert(std::is_standard_layout<TransportData>::value,
                  "TranportData requires standard-layout");

    SipsIceTransport(pjsip_endpoint* endpt, const TlsParams& param,
                    const std::shared_ptr<IceTransport>& ice, int comp_id);
    ~SipsIceTransport();

    void reset();

    IpAddr getLocalAddress() const;

    std::shared_ptr<IceTransport> getIceTransport() const {
        return ice_;
    }

    pjsip_transport* getTransportBase() {
        return &trData_.base;
    }

private:
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> rxPool_;

    using DelayedTxData = struct {
        pjsip_tx_data_op_key *tdata_op_key;
        clock::time_point timeout;
    };

    TransportData trData_;
    bool is_registered_ {false};
    const std::shared_ptr<IceTransport> ice_;
    const int comp_id_;

    TlsParams param_;

    bool is_server_ {false};
    //bool has_pending_connect_ {false};
    IpAddr local_ {};
    IpAddr remote_ {};

    ThreadLoop tlsThread_;
    std::condition_variable_any cv_;
    std::atomic<bool> canRead_ {false};
    std::atomic<bool> canWrite_ {false};

    // TODO
    pj_status_t verify_status_;
    int last_err_;

    pj_ssl_cert_info local_cert_info_;
    pj_ssl_cert_info remote_cert_info_;

    std::atomic<TlsConnectionState> state_ {TlsConnectionState::DISCONNECTED};
    clock::time_point handshakeStart_;

    gnutls_session_t session_ {nullptr};
    gnutls_certificate_credentials_t xcred_;
    gnutls_priority_t priority_cache;
    gnutls_datum_t cookie_key_;
    gnutls_dtls_prestate_st prestate_;

    /**
     * To be called on a regular basis to receive packets
     */
    bool setup();
    void loop();
    void clean();

    // SIP transport <-> GnuTLS
    pj_status_t send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                     int addr_len, void *token,
                     pjsip_transport_callback callback);
    ssize_t trySend(pjsip_tx_data_op_key *tdata);
    pj_status_t flushOutputBuff();
    std::list<DelayedTxData> outputBuff_ {};
    std::mutex outputBuffMtx_ {};
    pjsip_rx_data rdata_;

    // GnuTLS <-> ICE
    ssize_t tlsSend(const void*, size_t);
    ssize_t tlsRecv(void* d , size_t s);
    int waitForTlsData(unsigned ms);
    std::mutex inputBuffMtx_ {};
    std::list<std::vector<uint8_t>> tlsInputBuff_ {};

    pj_status_t startTlsSession();
    pj_status_t tryHandshake();
    void certGetCn(const pj_str_t *gen_name, pj_str_t *cn);
    void certGetInfo(pj_pool_t *pool, pj_ssl_cert_info *ci,
                     gnutls_x509_crt_t cert);
    void certUpdate();
    pj_bool_t onHandshakeComplete(pj_status_t status);
    int verifyCertificate();
    pj_status_t getInfo (pj_ssl_sock_info *info);

    void close();
};

}} // namespace ring::tls
