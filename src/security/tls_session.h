/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#include "threadloop.h"

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
#include <utility>
#include <vector>
#include <map>

namespace ring {
class IceTransport;
class IceSocket;
} // namespace ring

namespace dht { namespace crypto {
class Certificate;
class PrivateKey;
}} // namespace dht::crypto

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
    std::shared_ptr<dht::crypto::Certificate> cert;
    std::shared_ptr<dht::crypto::PrivateKey> cert_key;
    std::shared_future<std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&>> dh_params;
    std::chrono::steady_clock::duration timeout;
    std::function<int(unsigned status, const gnutls_datum_t* cert_list,
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
    using Blob = std::vector<uint8_t>;
    using OnStateChangeFunc = std::function<void(TlsSessionState)>;
    using OnRxDataFunc = std::function<void(Blob&&)>;
    using OnCertificatesUpdate = std::function<void(const gnutls_datum_t*, const gnutls_datum_t*, unsigned int)>;
    using VerifyCertificate = std::function<int(gnutls_session_t)>;
    using TxDataCompleteFunc = std::function<void(std::size_t bytes_sent)>;

    // ===> WARNINGS <===
    // Following callbacks are called into the FSM thread context
    // Do not call blocking routines inside them.
    using TlsSessionCallbacks = struct {
        OnStateChangeFunc onStateChange;
        OnRxDataFunc onRxData;
        OnCertificatesUpdate onCertificatesUpdate;
        VerifyCertificate verifyCertificate;
    };

    TlsSession(std::shared_ptr<IceTransport> ice, int ice_comp_id, const TlsParams& params,
               const TlsSessionCallbacks& cbs);
    ~TlsSession();

    const char* typeName() const;
    void shutdown();

    // Valid only if state is ESTABLISHED and in a callback context
    gnutls_session_t getGnuTlsSession() { return session_; }

    // Asynchronous sending operation
    ssize_t async_send(void* data, std::size_t size, TxDataCompleteFunc on_send_complete);

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
    struct TxData {
        void* const ptr;
        std::size_t size;
        TxDataCompleteFunc onComplete;
    };

    std::mutex ioMutex_;
    std::condition_variable ioCv_;
    std::list<TxData> txQueue_;
    std::list<std::vector<uint8_t>> rxQueue_;

    ssize_t send(const TxData& tx_data);
    ssize_t sendRaw(const void*, size_t);
    ssize_t sendRawVec(const giovec_t*, int);
    ssize_t recvRaw(void*, size_t);
    int waitForRawData(unsigned);

    // GnuTLS stuff and connection state
    std::atomic<TlsSessionState> state_ {TlsSessionState::SETUP};
    std::unique_ptr<TlsCertificateCredendials> xcred_;

    gnutls_session_t session_ {nullptr};
    gnutls_datum_t cookie_key_ {nullptr, 0};
    gnutls_priority_t priority_cache_ {nullptr};
    gnutls_dtls_prestate_st prestate_;

    ssize_t cookie_count_ {0};

    TlsSessionState setupClient();
    TlsSessionState setupServer();
    void initCredentials();
    bool commonSessionInit();

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

}} // namespace ring::tls
