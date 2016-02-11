/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include "ip_utils.h"
#include "threadloop.h"

#include <opendht/crypto.h>

#include <pjsip.h>
#include <pj/pool.h>

#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <future>
#include <queue>
#include <utility>

namespace ring {
class IceTransport;
class IceSocket;
} // namespace ring

namespace ring { namespace tls {

class TlsCertificateCredendials;

enum class TlsSessionState {
    SETUP,
    COOKIE, // server only
    HANDSHAKE,
    ESTABLISHED,
    SHUTDOWN
};

struct TlsParams {
    std::string ca_list;
    dht::crypto::Identity id;
    std::shared_future<std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&>> dh_params;
    std::chrono::steady_clock::duration timeout;
    std::function<pj_status_t(unsigned status,
                              const gnutls_datum_t* cert_list,
                              unsigned cert_list_size)> cert_check;
};

/**
 * TlsSession
 *
 * Manages a tls connection over an ICE transport.
 * This implementation uses a Threadloop to manage IO from ICE and tls states,
 * so public API is not blocking.
 */
class TlsSession {
public:
    using OnStateChangeFunc = std::function<void(TlsSessionState)>;
    using OnRxDataFunc = std::function<void(std::vector<uint8_t>&&)>;

    using TlsSessionCallbacks = struct {
        OnStateChangeFunc onStateChange;
        OnRxDataFunc onRxData;
    };

    TlsSession(std::shared_ptr<IceTransport> ice, int ice_comp_id, const TlsParams& params,
               const TlsSessionCallbacks& cbs);
    ~TlsSession();

    const char* typeName() const;

    void shutdown();

    // Valid until shutdown() is called
    gnutls_session_t getGnuTlsSession() { return session_; }
    pj_status_t getVerifyStatus() const { return verifyStatus_; }

private:
    using clock = std::chrono::steady_clock;
    using StateHandler = std::function<TlsSessionState(TlsSessionState state)>;

    // Transport
    const std::unique_ptr<IceSocket> socket_;
    const bool isServer_;
    const TlsParams params_;

    TlsSessionCallbacks callbacks_;

    // State machine
    TlsSessionState handleStateSetup(TlsSessionState state);
    TlsSessionState handleStateCookie(TlsSessionState state);
    TlsSessionState handleStateHandshake(TlsSessionState state);
    TlsSessionState handleStateEstablished(TlsSessionState state);
    TlsSessionState handleStateShutdown(TlsSessionState state);
    std::map<TlsSessionState, StateHandler> fsmHandlers_ {};

    // IO GnuTLS <-> ICE
    std::mutex inputDataMutex_;
    std::condition_variable ioCv_;
    std::list<std::vector<uint8_t>> inputData_;

    ssize_t sendRaw(const void*, size_t);
    ssize_t sendRawVec(const giovec_t*, int);
    ssize_t recvRaw(void*, size_t);
    int waitForRawData(unsigned);

    // GnuTLS stuff and connection state
    std::atomic<TlsSessionState> state_ {TlsSessionState::SETUP};
    gnutls_session_t session_ {nullptr};
    ssize_t cookie_count_ {0};
    gnutls_datum_t cookie_key_ {nullptr, 0};
    gnutls_priority_t priority_cache_ {nullptr};
    gnutls_dtls_prestate_st prestate_;

    std::unique_ptr<TlsCertificateCredendials> xcred_;

    pj_status_t verifyStatus_ {}; // last verifyCertificate() result
    int last_err_;

    clock::time_point handshakeStart_;

    TlsSessionState setupClient();
    TlsSessionState setupServer();
    void initCredentials();
    bool commonSessionInit();

    int verifyCertificate();
    pj_status_t tryHandshake();

    // Statistics
    std::size_t stRxRawPacketCnt_ {0};
    std::size_t stRxRawBytesCnt_ {0};
    std::size_t stRxRawPacketDropCnt_ {0};
    std::size_t stTxRawPacketCnt_ {0};
    std::size_t stTxRawBytesCnt_ {0};
    void dump_io_stats() const;

    // TLS connection state management thread
    ThreadLoop thread_;
    bool setup();
    void process();
    void cleanup();
};

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

    SipsIceTransport(pjsip_endpoint* endpt, const TlsParams& param,
                    const std::shared_ptr<IceTransport>& ice, int comp_id);
    ~SipsIceTransport();

    void shutdown();

    std::shared_ptr<IceTransport> getIceTransport() const { return ice_; }
    pjsip_transport* getTransportBase() { return &trData_.base; }

    IpAddr getLocalAddress() const { return local_; }
    IpAddr getRemoteAddress() const { return remote_; }

private:
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> pool_;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> rxPool_;

    using DelayedTxData = struct {
        pjsip_tx_data_op_key *tdata_op_key;
        clock::time_point timeout;
    };

    TransportData trData_; // uplink to this (used in C callbacks)

    const std::shared_ptr<IceTransport> ice_;
    const int comp_id_;

    IpAddr local_ {};
    IpAddr remote_ {};

    int last_err_;

    pj_ssl_cert_info localCertInfo_;
    pj_ssl_cert_info remoteCertInfo_;

    struct ChangeStateEventData {
        pj_ssl_sock_info ssl_info;
        pjsip_tls_state_info tls_info;
        pjsip_transport_state_info state_info;
        decltype(PJSIP_TP_STATE_DISCONNECTED) state;
    };

    std::mutex stateChangeEventsMutex_;
    std::queue<ChangeStateEventData> stateChangeEvents_;

    // PJSIP transport <-> TlsSession
    pj_status_t send(pjsip_tx_data* tdata, const pj_sockaddr_t* rem_addr,
                     int addr_len, void* token,
                     pjsip_transport_callback callback);
    ssize_t trySend(pjsip_tx_data* tdata);
    pj_status_t flushOutputBuff();
    std::list<DelayedTxData> outputBuff_;
    std::list<std::pair<DelayedTxData, ssize_t>> outputAckBuff_; // acknowledged outputBuff_
    std::list<std::vector<uint8_t>> txBuff_;
    std::mutex outputBuffMtx_;

    std::mutex rxMtx_;
    std::list<std::vector<uint8_t>> rxPending_;
    pjsip_rx_data rdata_;

    void certGetCn(const pj_str_t*, pj_str_t*);
    void certGetInfo(pj_pool_t*, pj_ssl_cert_info*, const gnutls_datum_t*, size_t);
    void getInfo(pj_ssl_sock_info*, bool);

    void handleEvents();
    void pushEvent(ChangeStateEventData&&);

    void onTlsStateChange(TlsSessionState);
    void onRxData(std::vector<uint8_t>&&);

    std::unique_ptr<TlsSession> tls_;
};

}} // namespace ring::tls
