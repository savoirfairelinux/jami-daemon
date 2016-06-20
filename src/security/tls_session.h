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

#include <string>
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
#include <atomic>

namespace ring {
class IceTransport;
class IceSocket;
} // namespace ring

namespace dht { namespace crypto {
class Certificate;
class PrivateKey;
}} // namespace dht::crypto

namespace ring { namespace tls {

enum class TlsSessionState {
    SETUP,
    COOKIE, // server only
    HANDSHAKE,
    ESTABLISHED,
    SHUTDOWN
};

class DhParams {
public:
    DhParams() = default;
    DhParams(DhParams&&) = default;

    /** Take ownership of gnutls_dh_params */
    explicit DhParams(gnutls_dh_params_t p) : params_(p, gnutls_dh_params_deinit) {};

    /** Deserialize DER or PEM encoded DH-params */
    DhParams(const std::vector<uint8_t>& data);

    gnutls_dh_params_t get() {
        return params_.get();
    }
    gnutls_dh_params_t get() const {
        return params_.get();
    }

    explicit inline operator bool() const {
        return bool(params_);
    }

    /** Serialize data in PEM format */
    std::vector<uint8_t> serialize() const;

    static DhParams generate();

private:
    std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&> params_ {nullptr, gnutls_dh_params_deinit};
};

struct TlsParams {
    // User CA list for session credentials
    std::string ca_list;

    // User identity for credential
    std::shared_ptr<dht::crypto::Certificate> cert;
    std::shared_ptr<dht::crypto::PrivateKey> cert_key;

    // Diffie-Hellman computed by gnutls_dh_params_init/gnutls_dh_params_generateX
    std::shared_future<DhParams> dh_params;

    // DTLS timeout
    std::chrono::steady_clock::duration timeout;

    // Callback for certificate checkings
    std::function<int(unsigned status,
                      const gnutls_datum_t* cert_list,
                      unsigned cert_list_size)> cert_check;
};

/**
 * TlsSession
 *
 * Manages a DTLS connection over an ICE transport.
 * This implementation uses a Threadloop to manage IO from ICE and TLS states,
 * so IO are asynchronous.
 */
class TlsSession {
public:
    using OnStateChangeFunc = std::function<void(TlsSessionState)>;
    using OnRxDataFunc = std::function<void(std::vector<uint8_t>&&)>;
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
               const TlsSessionCallbacks& cbs, bool anonymous=true);
    ~TlsSession();

    // Returns the TLS session type ('server' or 'client')
    const char* typeName() const;

    bool isServer() const { return isServer_; }

    // Request TLS thread to stop and quit. IO are not possible after that.
    void shutdown();

    // Return maximum application payload size in bytes
    // Returned value must be checked and considered valid only if not 0 (session is initialized)
    unsigned int getMaxPayload() const { return maxPayload_; }

    // Can be called by onStateChange callback when state == ESTABLISHED
    // to obtain the used cypher suite id.
    // Return the name of current cipher.
    const char* getCurrentCipherSuiteId(std::array<uint8_t, 2>& cs_id) const;

    // Asynchronous sending operation. on_send_complete will be called with a positive number
    // for number of bytes sent, or negative for errors, or 0 in case of shutdown (end of session).
    int async_send(const void* data, std::size_t size, TxDataCompleteFunc on_send_complete);
    int async_send(std::vector<uint8_t>&& data, TxDataCompleteFunc on_send_complete);

    // Synchronous sending operation. Return negative number (gnutls error) or a positive number
    // for bytes sent.
    ssize_t send(const void* data, std::size_t size);
    ssize_t send(const std::vector<uint8_t>& data);

private:
    using clock = std::chrono::steady_clock;
    using StateHandler = std::function<TlsSessionState(TlsSessionState state)>;

    // Constants (ctor init.)
    const std::unique_ptr<IceSocket> socket_;
    const bool isServer_;
    const TlsParams params_;
    const TlsSessionCallbacks callbacks_;
    const bool anonymous_;

    // State machine
    TlsSessionState handleStateSetup(TlsSessionState state);
    TlsSessionState handleStateCookie(TlsSessionState state);
    TlsSessionState handleStateHandshake(TlsSessionState state);
    TlsSessionState handleStateEstablished(TlsSessionState state);
    TlsSessionState handleStateShutdown(TlsSessionState state);
    std::map<TlsSessionState, StateHandler> fsmHandlers_ {};
    std::atomic<TlsSessionState> state_ {TlsSessionState::SETUP};
    std::atomic<unsigned int> maxPayload_ {0};

    // IO GnuTLS <-> ICE
    std::mutex txMutex_ {};
    std::mutex rxMutex_ {};
    std::condition_variable rxCv_ {};
    std::list<std::vector<uint8_t>> rxQueue_ {};

    ssize_t send_(const uint8_t* tx_data, std::size_t tx_size);
    ssize_t sendRaw(const void*, size_t);
    ssize_t sendRawVec(const giovec_t*, int);
    ssize_t recvRaw(void*, size_t);
    int waitForRawData(unsigned);

    // Statistics
    std::atomic<std::size_t> stRxRawPacketCnt_ {0};
    std::atomic<std::size_t> stRxRawBytesCnt_ {0};
    std::atomic<std::size_t> stRxRawPacketDropCnt_ {0};
    std::atomic<std::size_t> stTxRawPacketCnt_ {0};
    std::atomic<std::size_t> stTxRawBytesCnt_ {0};
    void dump_io_stats() const;

    // GnuTLS backend and connection state
    class TlsCertificateCredendials;
    class TlsAnonymousClientCredendials;
    class TlsAnonymousServerCredendials;
    std::unique_ptr<TlsAnonymousClientCredendials> cacred_; // ctor init.
    std::unique_ptr<TlsAnonymousServerCredendials> sacred_; // ctor init.
    std::unique_ptr<TlsCertificateCredendials> xcred_; // ctor init.
    gnutls_session_t session_ {nullptr};
    gnutls_datum_t cookie_key_ {nullptr, 0};
    gnutls_dtls_prestate_st prestate_ {};
    ssize_t cookie_count_ {0};

    TlsSessionState setupClient();
    TlsSessionState setupServer();
    void initAnonymous();
    void initCredentials();
    bool commonSessionInit();

    // FSM thread (TLS states)
    ThreadLoop thread_; // ctor init.
    bool setup();
    void process();
    void cleanup();
};

}} // namespace ring::tls
