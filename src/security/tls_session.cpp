/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
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

#include <ip_utils.h>       // DO NOT CHANGE ORDER OF THIS INCLUDE OR MINGWIN FAILS TO BUILD

#include "tls_session.h"

#include "threadloop.h"
#include "logger.h"
#include "noncopyable.h"
#include "compiler_intrinsics.h"
#include "manager.h"
#include "certstore.h"
#include "array_size.h"
#include "diffie-hellman.h"

#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <list>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <map>
#include <atomic>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#include <cstring> // std::memset

#include <cstdlib>
#include <unistd.h>

namespace ring { namespace tls {

static constexpr const char* DTLS_CERT_PRIORITY_STRING {"SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:-RSA:%SERVER_PRECEDENCE:%SAFE_RENEGOTIATION"};
static constexpr const char* DTLS_FULL_PRIORITY_STRING {"SECURE192:-KX-ALL:+ANON-ECDH:+ANON-DH:+SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:-RSA:%SERVER_PRECEDENCE:%SAFE_RENEGOTIATION"};
static constexpr const char* TLS_CERT_PRIORITY_STRING {"SECURE192:-RSA:%SERVER_PRECEDENCE:%SAFE_RENEGOTIATION"};
static constexpr const char* TLS_FULL_PRIORITY_STRING {"SECURE192:-KX-ALL:+ANON-ECDH:+ANON-DH:+SECURE192:-RSA:%SERVER_PRECEDENCE:%SAFE_RENEGOTIATION"};
static constexpr uint16_t INPUT_BUFFER_SIZE {16*1024}; // to be coherent with the maximum size advised in path mtu discovery
static constexpr std::size_t INPUT_MAX_SIZE {1000}; // Maximum number of packets to store before dropping (pkt size = DTLS_MTU)
static constexpr ssize_t FLOOD_THRESHOLD {4*1024};
static constexpr auto FLOOD_PAUSE = std::chrono::milliseconds(100); // Time to wait after an invalid cookie packet (anti flood attack)
static constexpr auto DTLS_RETRANSMIT_TIMEOUT = std::chrono::milliseconds(1000); // Delay between two handshake request on DTLS
static constexpr auto COOKIE_TIMEOUT = std::chrono::seconds(10); // Time to wait for a cookie packet from client
static constexpr int MIN_MTU {512 - 20 - 8}; // minimal payload size of a DTLS packet carried by an IPv4 packet
static constexpr uint8_t HEARTBEAT_TRIES = 1; // Number of tries at each heartbeat ping send
static constexpr auto HEARTBEAT_RETRANS_TIMEOUT = std::chrono::milliseconds(700); // gnutls heartbeat retransmission timeout for each ping (in milliseconds)
static constexpr auto HEARTBEAT_TOTAL_TIMEOUT = HEARTBEAT_RETRANS_TIMEOUT * HEARTBEAT_TRIES; // gnutls heartbeat time limit for heartbeat procedure (in milliseconds)
static constexpr int MISS_ORDERING_LIMIT = 32; // maximal accepted distance of out-of-order packet (note: must be a signed type)
static constexpr auto RX_OOO_TIMEOUT = std::chrono::milliseconds(1500);
static constexpr int ASYMETRIC_TRANSPORT_MTU_OFFSET = 20; // when client, if your local IP is IPV4 and server is IPV6; you must reduce your MTU to avoid packet too big error on server side. the offset is the difference in size of IP headers

// Helper to cast any duration into an integer number of milliseconds
template <class Rep, class Period>
static std::chrono::milliseconds::rep
duration2ms(std::chrono::duration<Rep, Period> d)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

static inline uint64_t
array2uint(const std::array<uint8_t, 8>& a)
{
    uint64_t res = 0;
    for (int i=0; i < 8; ++i)
        res = (res << 8) + a[i];
    return res;
}

//==============================================================================

namespace {

class TlsCertificateCredendials
{
    using T = gnutls_certificate_credentials_t;
public:
    TlsCertificateCredendials() {
        int ret = gnutls_certificate_allocate_credentials(&creds_);
        if (ret < 0) {
            RING_ERR("gnutls_certificate_allocate_credentials() failed with ret=%d", ret);
            throw std::bad_alloc();
        }
    }

    ~TlsCertificateCredendials() {
        gnutls_certificate_free_credentials(creds_);
    }

    operator T() { return creds_; }

private:
    NON_COPYABLE(TlsCertificateCredendials);
    T creds_;
};

class TlsAnonymousClientCredendials
{
    using T = gnutls_anon_client_credentials_t;
public:
    TlsAnonymousClientCredendials() {
        int ret = gnutls_anon_allocate_client_credentials(&creds_);
        if (ret < 0) {
            RING_ERR("gnutls_anon_allocate_client_credentials() failed with ret=%d", ret);
            throw std::bad_alloc();
        }
    }

    ~TlsAnonymousClientCredendials() {
        gnutls_anon_free_client_credentials(creds_);
    }

    operator T() { return creds_; }

private:
    NON_COPYABLE(TlsAnonymousClientCredendials);
    T creds_;
};

class TlsAnonymousServerCredendials
{
    using T = gnutls_anon_server_credentials_t;
public:
    TlsAnonymousServerCredendials() {
        int ret = gnutls_anon_allocate_server_credentials(&creds_);
        if (ret < 0) {
            RING_ERR("gnutls_anon_allocate_server_credentials() failed with ret=%d", ret);
            throw std::bad_alloc();
        }
    }

    ~TlsAnonymousServerCredendials() {
        gnutls_anon_free_server_credentials(creds_);
    }

    operator T() { return creds_; }

private:
    NON_COPYABLE(TlsAnonymousServerCredendials);
    T creds_;
};

} // namespace <anonymous>

//==============================================================================

class TlsSession::TlsSessionImpl
{
public:
    using clock = std::chrono::steady_clock;
    using StateHandler = std::function<TlsSessionState(TlsSessionState state)>;

    // Constants (ctor init.)
    const bool isServer_;
    const TlsParams params_;
    const TlsSessionCallbacks callbacks_;
    const bool anonymous_;

    TlsSessionImpl(SocketType& transport, const TlsParams& params,
                   const TlsSessionCallbacks& cbs, bool anonymous);

    ~TlsSessionImpl();

    const char* typeName() const;

    SocketType& transport_;

    // State machine
    TlsSessionState handleStateSetup(TlsSessionState state);
    TlsSessionState handleStateCookie(TlsSessionState state);
    TlsSessionState handleStateHandshake(TlsSessionState state);
    TlsSessionState handleStateMtuDiscovery(TlsSessionState state);
    TlsSessionState handleStateEstablished(TlsSessionState state);
    TlsSessionState handleStateShutdown(TlsSessionState state);
    std::map<TlsSessionState, StateHandler> fsmHandlers_ {};
    std::atomic<TlsSessionState> state_ {TlsSessionState::SETUP};
    std::atomic<int> maxPayload_ {-1};

    // IO GnuTLS <-> ICE
    std::mutex rxMutex_ {};
    std::condition_variable rxCv_ {};
    std::list<std::vector<ValueType>> rxQueue_ {};

    std::mutex reorderBufMutex_;
    bool flushProcessing_ {false}; ///< protect against recursive call to flushRxQueue
    std::vector<ValueType> rawPktBuf_; ///< gnutls incoming packet buffer
    uint64_t baseSeq_ {0}; ///< sequence number of first application data packet received
    uint64_t lastRxSeq_ {0}; ///< last received and valid packet sequence number
    uint64_t gapOffset_ {0}; ///< offset of first byte not received yet
    clock::time_point lastReadTime_;
    std::map<uint64_t, std::vector<ValueType>> reorderBuffer_ {};

    std::size_t send(const ValueType*, std::size_t, std::error_code&);
    ssize_t sendRaw(const void*, size_t);
    ssize_t sendRawVec(const giovec_t*, int);
    ssize_t recvRaw(void*, size_t);
    int waitForRawData(unsigned);

    bool initFromRecordState(int offset=0);
    void handleDataPacket(std::vector<ValueType>&&, uint64_t);
    void flushRxQueue();

    // Statistics
    std::atomic<std::size_t> stRxRawPacketCnt_ {0};
    std::atomic<std::size_t> stRxRawBytesCnt_ {0};
    std::atomic<std::size_t> stRxRawPacketDropCnt_ {0};
    std::atomic<std::size_t> stTxRawPacketCnt_ {0};
    std::atomic<std::size_t> stTxRawBytesCnt_ {0};
    void dump_io_stats() const;

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

    // Path mtu discovery
    std::array<int, 3> MTUS_;
    int mtuProbe_;
    int hbPingRecved_ {0};
    bool pmtudOver_ {false};
    void pathMtuHeartbeat();
};

TlsSession::TlsSessionImpl::TlsSessionImpl(SocketType& transport,
                                           const TlsParams& params,
                                           const TlsSessionCallbacks& cbs,
                                           bool anonymous)
    : isServer_(not transport.isInitiator())
    , params_(params)
    , callbacks_(cbs)
    , anonymous_(anonymous)
    , transport_ { transport }
    , cacred_(nullptr)
    , sacred_(nullptr)
    , xcred_(nullptr)
    , thread_([this] { return setup(); },
              [this] { process(); },
              [this] { cleanup(); })
{
    if (not transport_.isReliable()) {
        transport_.setOnRecv([this](const ValueType* buf, size_t len) {
                std::lock_guard<std::mutex> lk {rxMutex_};
                if (rxQueue_.size() == INPUT_MAX_SIZE) {
                    rxQueue_.pop_front(); // drop oldest packet if input buffer is full
                    ++stRxRawPacketDropCnt_;
                }
                rxQueue_.emplace_back(buf, buf+len);
                ++stRxRawPacketCnt_;
                stRxRawBytesCnt_ += len;
                rxCv_.notify_one();
                return len;
            });
    }

    Manager::instance().registerEventHandler((uintptr_t)this, [this]{ flushRxQueue(); });

    // Run FSM into dedicated thread
    thread_.start();
}

TlsSession::TlsSessionImpl::~TlsSessionImpl()
{
    thread_.join();
    if (not transport_.isReliable())
        transport_.setOnRecv(nullptr);
    Manager::instance().unregisterEventHandler((uintptr_t)this);
}

const char*
TlsSession::TlsSessionImpl::typeName() const
{
    return isServer_ ? "server" : "client";
}

void
TlsSession::TlsSessionImpl::dump_io_stats() const
{
    RING_DBG("[TLS] RxRawPkt=%zu (%zu bytes) - TxRawPkt=%zu (%zu bytes)",
             stRxRawPacketCnt_.load(), stRxRawBytesCnt_.load(),
             stTxRawPacketCnt_.load(), stTxRawBytesCnt_.load());
}

TlsSessionState
TlsSession::TlsSessionImpl::setupClient()
{
    int ret;

    if (not transport_.isReliable()) {
        ret = gnutls_init(&session_, GNUTLS_CLIENT | GNUTLS_DATAGRAM);
        RING_DBG("[TLS] set heartbeat reception for retrocompatibility check on server");
        gnutls_heartbeat_enable(session_,GNUTLS_HB_PEER_ALLOWED_TO_SEND);
    } else {
        ret = gnutls_init(&session_, GNUTLS_CLIENT);
    }

    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] session init failed: %s", gnutls_strerror(ret));
        return TlsSessionState::SHUTDOWN;
    }

    if (not commonSessionInit()) {
        return TlsSessionState::SHUTDOWN;
    }

    return TlsSessionState::HANDSHAKE;
}

TlsSessionState
TlsSession::TlsSessionImpl::setupServer()
{
    int ret;

    if (not transport_.isReliable()) {
        ret = gnutls_init(&session_, GNUTLS_SERVER | GNUTLS_DATAGRAM);

        RING_DBG("[TLS] set heartbeat reception");
        gnutls_heartbeat_enable(session_, GNUTLS_HB_PEER_ALLOWED_TO_SEND);

        gnutls_dtls_prestate_set(session_, &prestate_);
    } else {
        ret = gnutls_init(&session_, GNUTLS_SERVER);
    }

    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] session init failed: %s", gnutls_strerror(ret));
        return TlsSessionState::SHUTDOWN;
    }

    gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUIRE);

    if (not commonSessionInit())
        return TlsSessionState::SHUTDOWN;

    return TlsSessionState::HANDSHAKE;
}

void
TlsSession::TlsSessionImpl::initAnonymous()
{
    // credentials for handshaking and transmission
    if (isServer_)
        sacred_.reset(new TlsAnonymousServerCredendials());
    else
        cacred_.reset(new TlsAnonymousClientCredendials());

    // Setup DH-params for anonymous authentification
    if (isServer_) {
        if (const auto& dh_params = params_.dh_params.get().get())
            gnutls_anon_set_server_dh_params(*sacred_, dh_params);
        else
            RING_WARN("[TLS] DH params unavailable"); // YOMGUI: need to stop?
    }
}

void
TlsSession::TlsSessionImpl::initCredentials()
{
    int ret;

    // credentials for handshaking and transmission
    xcred_.reset(new TlsCertificateCredendials());

    if (callbacks_.verifyCertificate)
        gnutls_certificate_set_verify_function(*xcred_, [](gnutls_session_t session) -> int {
                auto this_ = reinterpret_cast<TlsSessionImpl*>(gnutls_session_get_ptr(session));
                return this_->callbacks_.verifyCertificate(session);
            });

    // Load user-given CA list
    if (not params_.ca_list.empty()) {
        // Try PEM format first
        ret = gnutls_certificate_set_x509_trust_file(*xcred_, params_.ca_list.c_str(),
                                                     GNUTLS_X509_FMT_PEM);

        // Then DER format
        if (ret < 0)
            ret = gnutls_certificate_set_x509_trust_file(*xcred_, params_.ca_list.c_str(),
                                                         GNUTLS_X509_FMT_DER);
        if (ret < 0)
            throw std::runtime_error("can't load CA " + params_.ca_list + ": "
                                     + std::string(gnutls_strerror(ret)));

        RING_DBG("[TLS] CA list %s loadev", params_.ca_list.c_str());
    }
    if (params_.peer_ca) {
        auto chain = params_.peer_ca->getChainWithRevocations();
        auto ret = gnutls_certificate_set_x509_trust(*xcred_, chain.first.data(), chain.first.size());
        if (not chain.second.empty())
            gnutls_certificate_set_x509_crl(*xcred_, chain.second.data(), chain.second.size());
        RING_DBG("[TLS] Peer CA list %lu (%lu CRLs): %d", chain.first.size(), chain.second.size(), ret);
    }

    // Load user-given identity (key and passwd)
    if (params_.cert) {
        std::vector<gnutls_x509_crt_t> certs;
        certs.reserve(3);
        auto crt = params_.cert;
        while (crt) {
            certs.emplace_back(crt->cert);
            crt = crt->issuer;
        }

        ret = gnutls_certificate_set_x509_key(*xcred_, certs.data(), certs.size(), params_.cert_key->x509_key);
        if (ret < 0)
            throw std::runtime_error("can't load certificate: "
                                     + std::string(gnutls_strerror(ret)));

        RING_DBG("[TLS] User identity loaded");
    }

    // Setup DH-params (server only, may block on dh_params.get())
    if (isServer_) {
        if (const auto& dh_params = params_.dh_params.get().get())
            gnutls_certificate_set_dh_params(*xcred_, dh_params);
        else
            RING_WARN("[TLS] DH params unavailable"); // YOMGUI: need to stop?
    }
}

bool
TlsSession::TlsSessionImpl::commonSessionInit()
{
    int ret;

    if (anonymous_) {
        // Force anonymous connection, see handleStateHandshake how we handle failures
        ret = gnutls_priority_set_direct(session_,
                                         transport_.isReliable() ? TLS_FULL_PRIORITY_STRING : DTLS_FULL_PRIORITY_STRING,
                                         nullptr);
        if (ret != GNUTLS_E_SUCCESS) {
            RING_ERR("[TLS] TLS priority set failed: %s", gnutls_strerror(ret));
            return false;
        }

        // Add anonymous credentials
        if (isServer_)
            ret = gnutls_credentials_set(session_, GNUTLS_CRD_ANON, *sacred_);
        else
            ret = gnutls_credentials_set(session_, GNUTLS_CRD_ANON, *cacred_);

        if (ret != GNUTLS_E_SUCCESS) {
            RING_ERR("[TLS] anonymous credential set failed: %s", gnutls_strerror(ret));
            return false;
        }
    } else {
        // Use a classic non-encrypted CERTIFICATE exchange method (less anonymous)
        ret = gnutls_priority_set_direct(session_,
                                         transport_.isReliable() ? TLS_CERT_PRIORITY_STRING : DTLS_CERT_PRIORITY_STRING,
                                         nullptr);
        if (ret != GNUTLS_E_SUCCESS) {
            RING_ERR("[TLS] TLS priority set failed: %s", gnutls_strerror(ret));
            return false;
        }
    }

    // Add certificate credentials
    ret = gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, *xcred_);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] certificate credential set failed: %s", gnutls_strerror(ret));
        return false;
    }
    gnutls_certificate_send_x509_rdn_sequence(session_, 0);

    if (not transport_.isReliable()) {
        // DTLS hanshake timeouts
        auto re_tx_timeout = duration2ms(DTLS_RETRANSMIT_TIMEOUT);
        gnutls_dtls_set_timeouts(session_, re_tx_timeout,
                                 std::max(duration2ms(params_.timeout), re_tx_timeout));

        // gnutls DTLS mtu = maximum payload size given by transport
        gnutls_dtls_set_mtu(session_, transport_.maxPayload());
    }

    // Stuff for transport callbacks
    gnutls_session_set_ptr(session_, this);
    gnutls_transport_set_ptr(session_, this);
    gnutls_transport_set_vec_push_function(session_,
                                           [](gnutls_transport_ptr_t t, const giovec_t* iov,
                                              int iovcnt) -> ssize_t {
                                               auto this_ = reinterpret_cast<TlsSessionImpl*>(t);
                                               return this_->sendRawVec(iov, iovcnt);
                                           });
    gnutls_transport_set_pull_function(session_,
                                       [](gnutls_transport_ptr_t t, void* d, size_t s) -> ssize_t {
                                           auto this_ = reinterpret_cast<TlsSessionImpl*>(t);
                                           return this_->recvRaw(d, s);
                                       });
    gnutls_transport_set_pull_timeout_function(session_,
                                               [](gnutls_transport_ptr_t t, unsigned ms) -> int {
                                                   auto this_ = reinterpret_cast<TlsSessionImpl*>(t);
                                                   return this_->waitForRawData(ms);
                                               });

    return true;
}

std::size_t
TlsSession::TlsSessionImpl::send(const ValueType* tx_data, std::size_t tx_size, std::error_code& ec)
{
    if (state_ != TlsSessionState::ESTABLISHED) {
        ec = std::error_code(GNUTLS_E_INVALID_SESSION, std::system_category());
        return 0;
    }

    std::size_t total_written = 0;
    std::size_t max_tx_sz;

    if (transport_.isReliable())
        max_tx_sz = tx_size;
    else
        max_tx_sz = gnutls_dtls_get_data_mtu(session_);

    // Split incoming data into chunck suitable for the underlying transport
    while (total_written < tx_size) {
        auto chunck_sz = std::min(max_tx_sz, tx_size - total_written);
        auto data_seq = tx_data + total_written;
        ssize_t nwritten;
        do {
            nwritten = gnutls_record_send(session_, data_seq, chunck_sz);
        } while ((nwritten == GNUTLS_E_INTERRUPTED and state_ != TlsSessionState::SHUTDOWN) or nwritten == GNUTLS_E_AGAIN);
        if (nwritten <= 0) {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            RING_ERR() << "[TLS] send failed (only " << total_written << " bytes sent): "
                       << gnutls_strerror(nwritten);
            ec = std::error_code(nwritten, std::system_category());
            return 0;
        }

        total_written += nwritten;
    }

    ec.clear();
    return total_written;
}

// Called by GNUTLS to send encrypted packet to low-level transport.
// Should return a positive number indicating the bytes sent, and -1 on error.
ssize_t
TlsSession::TlsSessionImpl::sendRaw(const void* buf, size_t size)
{
    std::error_code ec;
    auto n = transport_.write(reinterpret_cast<const ValueType*>(buf), size, ec);
    if (!ec) {
        // log only on success
        ++stTxRawPacketCnt_;
        stTxRawBytesCnt_ += n;
        return n;
    }

    // Must be called to pass errno value to GnuTLS on Windows (cf. GnuTLS doc)
    gnutls_transport_set_errno(session_, ec.value());
    RING_ERR() << "[TLS] transport failure on tx: errno = " << ec.value();
    return -1;
}

// Called by GNUTLS to send encrypted packet to low-level transport.
// Should return a positive number indicating the bytes sent, and -1 on error.
ssize_t
TlsSession::TlsSessionImpl::sendRawVec(const giovec_t* iov, int iovcnt)
{
    ssize_t sent = 0;
    for (int i=0; i<iovcnt; ++i) {
        const giovec_t& dat = iov[i];
        ssize_t ret = sendRaw(dat.iov_base, dat.iov_len);
        if (ret < 0)
            return -1;
        sent += ret;
    }
    return sent;
}

// Called by GNUTLS to receive encrypted packet from low-level transport.
// Should return 0 on connection termination,
// a positive number indicating the number of bytes received,
// and -1 on error.
ssize_t
TlsSession::TlsSessionImpl::recvRaw(void* buf, size_t size)
{
    if (transport_.isReliable()) {
        std::error_code ec;
        auto count = transport_.read(reinterpret_cast<ValueType*>(buf), size, ec);
        if (!ec)
            return count;
        gnutls_transport_set_errno(session_, ec.value());
        return -1;
    }

    std::lock_guard<std::mutex> lk {rxMutex_};
    if (rxQueue_.empty()) {
        gnutls_transport_set_errno(session_, EAGAIN);
        return -1;
    }

    const auto& pkt = rxQueue_.front();
    const std::size_t count = std::min(pkt.size(), size);
    std::copy_n(pkt.begin(), count, reinterpret_cast<ValueType*>(buf));
    rxQueue_.pop_front();
    return count;
}

// Called by GNUTLS to wait for encrypted packet from low-level transport.
// 'timeout' is in milliseconds.
// Should return 0 on timeout, a positive number if data are available for read, or -1 on error.
int
TlsSession::TlsSessionImpl::waitForRawData(unsigned timeout)
{
    if (transport_.isReliable()) {
        std::error_code ec;
        if (transport_.waitForData(timeout, ec) <= 0) {
            // shutdown?
            if (state_ == TlsSessionState::SHUTDOWN) {
                gnutls_transport_set_errno(session_, EINTR);
                return -1;
            }
            return 0;
        }
        return 1;
    }

    // non-reliable uses callback installed with setOnRecv()
    std::unique_lock<std::mutex> lk {rxMutex_};
    rxCv_.wait(lk, [this]{ return !rxQueue_.empty() or state_ == TlsSessionState::SHUTDOWN; });
    if (state_ == TlsSessionState::SHUTDOWN) {
        gnutls_transport_set_errno(session_, EINTR);
        return -1;
    }
    return 1;
}

bool
TlsSession::TlsSessionImpl::initFromRecordState(int offset)
{
    std::array<uint8_t, 8> seq;
    if (gnutls_record_get_state(session_, 1, nullptr, nullptr, nullptr, &seq[0]) != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] Fatal-error Unable to read initial state");
        return false;
    }

    baseSeq_ = array2uint(seq) + offset;
    gapOffset_ = baseSeq_;
    lastRxSeq_ = baseSeq_ - 1;
    RING_DBG("[TLS] Initial sequence number: %lx", baseSeq_);
    return true;
}

bool
TlsSession::TlsSessionImpl::setup()
{
    // Setup FSM
    fsmHandlers_[TlsSessionState::SETUP] = [this](TlsSessionState s){ return handleStateSetup(s); };
    fsmHandlers_[TlsSessionState::COOKIE] = [this](TlsSessionState s){ return handleStateCookie(s); };
    fsmHandlers_[TlsSessionState::HANDSHAKE] = [this](TlsSessionState s){ return handleStateHandshake(s); };
    fsmHandlers_[TlsSessionState::MTU_DISCOVERY] = [this](TlsSessionState s){ return handleStateMtuDiscovery(s); };
    fsmHandlers_[TlsSessionState::ESTABLISHED] = [this](TlsSessionState s){ return handleStateEstablished(s); };
    fsmHandlers_[TlsSessionState::SHUTDOWN] = [this](TlsSessionState s){ return handleStateShutdown(s); };

    return true;
}

void
TlsSession::TlsSessionImpl::cleanup()
{
    state_ = TlsSessionState::SHUTDOWN; // be sure to block any user operations

    if (session_) {
        if (transport_.isReliable())
            gnutls_bye(session_, GNUTLS_SHUT_RDWR);
        else
            gnutls_bye(session_, GNUTLS_SHUT_WR); // not wait for a peer answer
        gnutls_deinit(session_);
        session_ = nullptr;
    }

    if (cookie_key_.data)
        gnutls_free(cookie_key_.data);
}

TlsSessionState
TlsSession::TlsSessionImpl::handleStateSetup(UNUSED TlsSessionState state)
{
    RING_DBG("[TLS] Start %s session", typeName());

    try {
        if (anonymous_)
            initAnonymous();
        initCredentials();
    } catch (const std::exception& e) {
        RING_ERR("[TLS] authentifications init failed: %s", e.what());
        return TlsSessionState::SHUTDOWN;
    }

    if (not isServer_)
        return setupClient();

    // Extra step for DTLS-like transports
    if (not transport_.isReliable()) {
        gnutls_key_generate(&cookie_key_, GNUTLS_COOKIE_KEY_SIZE);
        return TlsSessionState::COOKIE;
    }
    return setupServer();
}

TlsSessionState
TlsSession::TlsSessionImpl::handleStateCookie(TlsSessionState state)
{
    RING_DBG("[TLS] SYN cookie");

    std::size_t count;
    {
        // block until rx packet or shutdown
        std::unique_lock<std::mutex> lk {rxMutex_};
        if (!rxCv_.wait_for(lk, COOKIE_TIMEOUT,
                            [this]{ return !rxQueue_.empty()
                                    or state_ == TlsSessionState::SHUTDOWN; })) {
            RING_ERR("[TLS] SYN cookie failed: timeout");
            return TlsSessionState::SHUTDOWN;
        }
        // Shutdown state?
        if (rxQueue_.empty())
            return TlsSessionState::SHUTDOWN;
        count = rxQueue_.front().size();
    }

    // Total bytes rx during cookie checking (see flood protection below)
    cookie_count_ += count;

    int ret;

    // Peek and verify front packet
    {
        std::lock_guard<std::mutex> lk {rxMutex_};
        auto& pkt = rxQueue_.front();
        std::memset(&prestate_, 0, sizeof(prestate_));
        ret = gnutls_dtls_cookie_verify(&cookie_key_, nullptr, 0,
                                        pkt.data(), pkt.size(), &prestate_);
    }

    if (ret < 0) {
        gnutls_dtls_cookie_send(&cookie_key_, nullptr, 0, &prestate_,
                                this,
                                [](gnutls_transport_ptr_t t, const void* d,
                                   size_t s) -> ssize_t {
                                    auto this_ = reinterpret_cast<TlsSessionImpl*>(t);
                                    return this_->sendRaw(d, s);
                                });

        // Drop front packet
        {
            std::lock_guard<std::mutex> lk {rxMutex_};
            rxQueue_.pop_front();
        }

        // Cookie may be sent on multiple network packets
        // So we retry until we get a valid cookie.
        // To protect against a flood attack we delay each retry after FLOOD_THRESHOLD rx bytes.
        if (cookie_count_ >= FLOOD_THRESHOLD) {
            RING_WARN("[TLS] flood threshold reach (retry in %zds)",
                      std::chrono::duration_cast<std::chrono::seconds>(FLOOD_PAUSE).count());
            dump_io_stats();
            std::this_thread::sleep_for(FLOOD_PAUSE); // flood attack protection
        }
        return state;
    }

    RING_DBG("[TLS] cookie ok");

    return setupServer();
}

TlsSessionState
TlsSession::TlsSessionImpl::handleStateHandshake(TlsSessionState state)
{
    RING_DBG("[TLS] handshake");

    auto ret = gnutls_handshake(session_);

    // Stop on fatal error
    if (gnutls_error_is_fatal(ret)) {
        RING_ERR("[TLS] handshake failed: %s", gnutls_strerror(ret));
        return TlsSessionState::SHUTDOWN;
    }

    // Continue handshaking on non-fatal error
    if (ret != GNUTLS_E_SUCCESS) {
        // TODO: handle GNUTLS_E_LARGE_PACKET (MTU must be lowered)
        if (ret != GNUTLS_E_AGAIN)
            RING_DBG("[TLS] non-fatal handshake error: %s", gnutls_strerror(ret));
        return state;
    }

    // Safe-Renegotiation status shall always be true to prevent MiM attack
    if (!gnutls_safe_renegotiation_status(session_)) {
        RING_ERR("[TLS] server identity changed! MiM attack?");
        return TlsSessionState::SHUTDOWN;
    }

    auto desc = gnutls_session_get_desc(session_);
    RING_DBG("[TLS] session established: %s", desc);
    gnutls_free(desc);

    // Anonymous connection? rehandshake immediatly with certificate authentification forced
    auto cred = gnutls_auth_get_type(session_);
    if (cred == GNUTLS_CRD_ANON) {
        RING_DBG("[TLS] renogotiate with certificate authentification");

        // Re-setup TLS algorithms priority list with only certificate based cipher suites
        ret = gnutls_priority_set_direct(session_,
                                         transport_.isReliable() ? TLS_CERT_PRIORITY_STRING : DTLS_CERT_PRIORITY_STRING,
                                         nullptr);
        if (ret != GNUTLS_E_SUCCESS) {
            RING_ERR("[TLS] session TLS cert-only priority set failed: %s", gnutls_strerror(ret));
            return TlsSessionState::SHUTDOWN;
        }

        // remove anon credentials and re-enable certificate ones
        gnutls_credentials_clear(session_);
        ret = gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, *xcred_);
        if (ret != GNUTLS_E_SUCCESS) {
            RING_ERR("[TLS] session credential set failed: %s", gnutls_strerror(ret));
            return TlsSessionState::SHUTDOWN;
        }

        return state; // handshake

    } else if (cred != GNUTLS_CRD_CERTIFICATE) {
        RING_ERR("[TLS] spurious session credential (%u)", cred);
        return TlsSessionState::SHUTDOWN;
    }

    // Aware about certificates updates
    if (callbacks_.onCertificatesUpdate) {
        unsigned int remote_count;
        auto local = gnutls_certificate_get_ours(session_);
        auto remote = gnutls_certificate_get_peers(session_, &remote_count);
        callbacks_.onCertificatesUpdate(local, remote, remote_count);
    }

    return transport_.isReliable() ? TlsSessionState::ESTABLISHED : TlsSessionState::MTU_DISCOVERY;
}

TlsSessionState
TlsSession::TlsSessionImpl::handleStateMtuDiscovery(UNUSED TlsSessionState state)
{
    mtuProbe_ = transport_.maxPayload();
    assert(mtuProbe_ >= MIN_MTU);
    MTUS_ = {MIN_MTU, std::max((mtuProbe_ + MIN_MTU)/2, MIN_MTU), mtuProbe_};

    // retrocompatibility check
    if (gnutls_heartbeat_allowed(session_, GNUTLS_HB_LOCAL_ALLOWED_TO_SEND) == 1) {
        if (!isServer_) {
            pathMtuHeartbeat();
            if (state_ == TlsSessionState::SHUTDOWN) {
                RING_ERR("[TLS] session destroyed while performing PMTUD, shuting down");
                return TlsSessionState::SHUTDOWN;
            }
            pmtudOver_ = true;
        }
    } else {
        RING_ERR() << "[TLS] PEER HEARTBEAT DISABLED: using transport MTU value " << mtuProbe_;
        pmtudOver_ = true;
    }

    gnutls_dtls_set_mtu(session_, mtuProbe_);
    maxPayload_ = gnutls_dtls_get_data_mtu(session_);

    if (pmtudOver_) {
        RING_DBG() << "[TLS] maxPayload: " << maxPayload_.load();
        if (!initFromRecordState())
            return TlsSessionState::SHUTDOWN;
    }

    return TlsSessionState::ESTABLISHED;
}

/*
 * Path MTU discovery heuristic
 * heuristic description:
 * The two members of the current tls connection will exchange dtls heartbeat messages
 * of increasing size until the heartbeat times out which will be considered as a packet
 * drop from the network due to the size of the packet. (one retry to test for a buffer issue)
 * when timeout happens or all the values have been tested, the mtu will be returned.
 * In case of unexpected error the first (and minimal) value of the mtu array
 */
void
TlsSession::TlsSessionImpl::pathMtuHeartbeat()
{
    RING_DBG() << "[TLS] PMTUD: starting probing with " << HEARTBEAT_RETRANS_TIMEOUT.count()
               << "ms of retransmission timeout";

    gnutls_heartbeat_set_timeouts(session_,
                                  HEARTBEAT_RETRANS_TIMEOUT.count(),
                                  HEARTBEAT_TOTAL_TIMEOUT.count());

    int errno_send = GNUTLS_E_SUCCESS;
    int mtuOffset = 0;

    // when the remote (server) has a IPV6 interface selected by ICE, and local (client) has a IPV4 selected,
    // the path MTU discovery triggers errors for packets too big on server side because of different IP headers overhead.
    // Hence we have to signal to the TLS session to reduce the MTU on client size accordingly.
    if (transport_.localAddr().isIpv4() and transport_.remoteAddr().isIpv6()) {
        mtuOffset = ASYMETRIC_TRANSPORT_MTU_OFFSET;
        RING_WARN() << "[TLS] local/remote IP protocol version not alike, use an MTU offset of "
                    << ASYMETRIC_TRANSPORT_MTU_OFFSET << " bytes to compensate";
    }

    mtuProbe_ = MTUS_[0];

    for (auto mtu: MTUS_) {
        gnutls_dtls_set_mtu(session_, mtu);
        auto data_mtu = gnutls_dtls_get_data_mtu(session_);
        RING_DBG() << "[TLS] PMTUD: mtu " << mtu
                   << ", payload " << data_mtu;
        auto bytesToSend = data_mtu - mtuOffset - 3; // want to know why -3? ask gnutls!

        do {
            errno_send = gnutls_heartbeat_ping(session_, bytesToSend, HEARTBEAT_TRIES, GNUTLS_HEARTBEAT_WAIT);
        } while (errno_send == GNUTLS_E_AGAIN || (errno_send == GNUTLS_E_INTERRUPTED && state_ != TlsSessionState::SHUTDOWN));

        if (errno_send != GNUTLS_E_SUCCESS) {
            RING_DBG() << "[TLS] PMTUD: mtu " << mtu << " [FAILED]";
            break;
        }

        mtuProbe_ = mtu;
        RING_DBG() << "[TLS] PMTUD: mtu " << mtu << " [OK]";
    }

    if (errno_send == GNUTLS_E_TIMEDOUT) { // timeout is considered as a packet loss, then the good mtu is the precedent
        if (mtuProbe_ == MTUS_[0]) {
            RING_WARN() << "[TLS] PMTUD: no response on first ping, using minimal MTU value "
                        << mtuProbe_;
        } else {
            RING_WARN() << "[TLS] PMTUD: timed out, using last working mtu "
                        << mtuProbe_;
        }
    } else if (errno_send != GNUTLS_E_SUCCESS) {
        RING_ERR() << "[TLS] PMTUD: failed with gnutls error '"
                   << gnutls_strerror(errno_send) << '\'';
    } else {
        RING_DBG() << "[TLS] PMTUD: reached maximal value";
    }
}

void
TlsSession::TlsSessionImpl::handleDataPacket(std::vector<ValueType>&& buf, uint64_t pkt_seq)
{
    // Check for a valid seq. num. delta
    int64_t seq_delta = pkt_seq - lastRxSeq_;
    if (seq_delta > 0) {
        lastRxSeq_ = pkt_seq;
    } else {
        // too old?
        if (seq_delta <= -MISS_ORDERING_LIMIT) {
            RING_WARN("[TLS] drop old pkt: 0x%lx", pkt_seq);
            return;
        }

        // No duplicate check as DTLS prevents that for us (replay protection)

        // accept Out-Of-Order pkt - will be reordered by queue flush operation
        RING_WARN("[TLS] OOO pkt: 0x%lx", pkt_seq);
    }

    {
        std::lock_guard<std::mutex> lk {reorderBufMutex_};
        if (reorderBuffer_.empty())
            lastReadTime_ = clock::now();
        reorderBuffer_.emplace(pkt_seq, std::move(buf));
    }

    // Try to flush right now as a new packet is available
    flushRxQueue();
}

///
/// Reorder and push received packet to upper layer
///
/// \note This method must be called continously, faster than RX_OOO_TIMEOUT
///
void
TlsSession::TlsSessionImpl::flushRxQueue()
{
    // RAII bool swap
    class GuardedBoolSwap {
    public:
        explicit GuardedBoolSwap(bool& var) : var_ {var} { var_ = !var_; }
        ~GuardedBoolSwap() { var_ = !var_; }
    private:
        bool& var_;
    };

    std::unique_lock<std::mutex> lk {reorderBufMutex_};
    if (reorderBuffer_.empty())
        return;

    // Prevent re-entrant access as the callbacks_.onRxData() is called in unprotected region
    if (flushProcessing_)
        return;

    GuardedBoolSwap swap_flush_processing {flushProcessing_};

    auto item = std::begin(reorderBuffer_);
    auto next_offset = item->first;

    // Wait for next continous packet until timeout
    if ((clock::now() - lastReadTime_) >= RX_OOO_TIMEOUT) {
        // OOO packet timeout - consider waited packets as lost
        if (auto lost = next_offset - gapOffset_)
            RING_WARN("[TLS] %lu lost since 0x%lx", lost, gapOffset_);
        else
            RING_WARN("[TLS] slow flush");
    } else if (next_offset != gapOffset_)
        return;

    // Loop on offset-ordered received packet until a discontinuity in sequence number
    while (item != std::end(reorderBuffer_) and item->first <= next_offset) {
        auto pkt_offset = item->first;
        auto pkt = std::move(item->second);

        // Remove item before unlocking to not trash the item' relationship
        next_offset = pkt_offset + 1;
        item = reorderBuffer_.erase(item);

        if (callbacks_.onRxData) {
            lk.unlock();
            callbacks_.onRxData(std::move(pkt));
            lk.lock();
        }
    }

    gapOffset_ = std::max(gapOffset_, next_offset);
    lastReadTime_ = clock::now();
}

TlsSessionState
TlsSession::TlsSessionImpl::handleStateEstablished(TlsSessionState state)
{
    // Nothing to do in reliable mode, so just wait for state change
    if (transport_.isReliable()) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            state = state_.load();
            if (state != TlsSessionState::ESTABLISHED)
                return state;
        }
        return TlsSessionState::SHUTDOWN;
    }

    // block until rx packet or state change
    {
        std::unique_lock<std::mutex> lk {rxMutex_};
        rxCv_.wait(lk, [this]{ return !rxQueue_.empty() or state_ != TlsSessionState::ESTABLISHED; });
        state = state_.load();
        if (state != TlsSessionState::ESTABLISHED)
            return state;
    }

    std::array<uint8_t, 8> seq;
    rawPktBuf_.resize(maxPayload_);
    auto ret = gnutls_record_recv_seq(session_, rawPktBuf_.data(), rawPktBuf_.size(), &seq[0]);

    if (ret > 0) {
        // Are we in PMTUD phase?
        if (!pmtudOver_) {
            mtuProbe_ = MTUS_[std::max(0, hbPingRecved_ - 1)];
            gnutls_dtls_set_mtu(session_, mtuProbe_);
            maxPayload_ = gnutls_dtls_get_data_mtu(session_);
            pmtudOver_ = true;
            RING_DBG() << "[TLS] maxPayload: " << maxPayload_.load();

            if (!initFromRecordState(-1))
                return TlsSessionState::SHUTDOWN;
        }

        rawPktBuf_.resize(ret);
        handleDataPacket(std::move(rawPktBuf_), array2uint(seq));
        // no state change
    } else if (ret == GNUTLS_E_HEARTBEAT_PING_RECEIVED) {
        RING_DBG("[TLS] PMTUD: ping received sending pong");
        auto errno_send = gnutls_heartbeat_pong(session_, 0);

        if (errno_send != GNUTLS_E_SUCCESS){
            RING_ERR("[TLS] PMTUD: failed on pong with error %d: %s", errno_send,
                      gnutls_strerror(errno_send));
        } else {
            ++hbPingRecved_;
        }
        // no state change
    } else if (ret == 0) {
        RING_DBG("[TLS] eof");
        state = TlsSessionState::SHUTDOWN;
    } else if (ret == GNUTLS_E_REHANDSHAKE) {
        RING_DBG("[TLS] re-handshake");
        state = TlsSessionState::HANDSHAKE;
    } else if (gnutls_error_is_fatal(ret)) {
        RING_ERR("[TLS] fatal error in recv: %s", gnutls_strerror(ret));
        state = TlsSessionState::SHUTDOWN;
    } // else non-fatal error... let's continue

    return state;
}

TlsSessionState
TlsSession::TlsSessionImpl::handleStateShutdown(TlsSessionState state)
{
    RING_DBG("[TLS] shutdown");

    // Stop ourself
    thread_.stop();
    return state;
}

void
TlsSession::TlsSessionImpl::process()
{
    auto old_state = state_.load();
    auto new_state = fsmHandlers_[old_state](old_state);

    // update state_ with taking care for external state change
    if (not std::atomic_compare_exchange_strong(&state_, &old_state, new_state))
        new_state = old_state;

    if (old_state != new_state and callbacks_.onStateChange)
        callbacks_.onStateChange(new_state);
}

//==============================================================================

TlsSession::TlsSession(SocketType& transport, const TlsParams& params,
                       const TlsSessionCallbacks& cbs, bool anonymous)

    : pimpl_ { std::make_unique<TlsSessionImpl>(transport, params, cbs, anonymous) }
{}

TlsSession::~TlsSession()
{
    shutdown();
}

bool
TlsSession::isInitiator() const
{
    return !pimpl_->isServer_;
}

bool
TlsSession::isReliable() const
{
    return pimpl_->transport_.isReliable();
}

int
TlsSession::maxPayload() const
{
    if (pimpl_->state_ == TlsSessionState::SHUTDOWN)
        throw std::runtime_error("Getting MTU from non-valid TLS session");
    return gnutls_dtls_get_data_mtu(pimpl_->session_);
}

const char*
TlsSession::currentCipherSuiteId(std::array<uint8_t, 2>& cs_id) const
{
    // get current session cipher suite info
    gnutls_cipher_algorithm_t cipher, s_cipher = gnutls_cipher_get(pimpl_->session_);
    gnutls_kx_algorithm_t kx, s_kx = gnutls_kx_get(pimpl_->session_);
    gnutls_mac_algorithm_t mac, s_mac = gnutls_mac_get(pimpl_->session_);

    // Loop on all known cipher suites until matching with session data, extract it's cs_id
    for (std::size_t i=0; ; ++i) {
        const char* const suite = gnutls_cipher_suite_info(i, cs_id.data(), &kx, &cipher, &mac,
                                                           nullptr);
        if (!suite)
          break;
        if (cipher == s_cipher && kx == s_kx && mac == s_mac)
            return suite;
    }

    auto name = gnutls_cipher_get_name(s_cipher);
    RING_WARN("[TLS] No Cipher Suite Id found for cipher %s", name ? name : "<null>");
    return {};
}

// Called by anyone to stop the connection and the FSM thread
void
TlsSession::shutdown()
{
    pimpl_->state_ = TlsSessionState::SHUTDOWN;
    pimpl_->rxCv_.notify_one(); // unblock waiting FSM
    pimpl_->transport_.shutdown();
}

std::size_t
TlsSession::write(const ValueType* data, std::size_t size, std::error_code& ec)
{
    if (pimpl_->state_ != TlsSessionState::ESTABLISHED) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return 0;
    }

    return pimpl_->send(data, size, ec);
}

std::size_t
TlsSession::read(ValueType* data, std::size_t size, std::error_code& ec)
{
    std::errc error;

    if (pimpl_->state_ != TlsSessionState::ESTABLISHED) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return 0;
    }

    while (true) {
        auto ret = gnutls_record_recv(pimpl_->session_, data, size);
        if (ret > 0) {
            ec.clear();
            return ret;
        }

        if (ret == 0) {
            RING_DBG("[TLS] eof");
            shutdown();
            error = std::errc::broken_pipe;
            break;
        } else if (ret == GNUTLS_E_REHANDSHAKE) {
            RING_DBG("[TLS] re-handshake");
            pimpl_->state_ = TlsSessionState::HANDSHAKE;
            pimpl_->rxCv_.notify_one(); // unblock waiting FSM
        } else if (gnutls_error_is_fatal(ret)) {
            RING_ERR("[TLS] fatal error in recv: %s", gnutls_strerror(ret));
            shutdown();
            error = std::errc::io_error;
            break;
        }
    }

    ec = std::make_error_code(error);
    return 0;
}

void
TlsSession::connect()
{
    TlsSessionState state;
    do {
        state = pimpl_->state_.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (state != TlsSessionState::ESTABLISHED and state != TlsSessionState::SHUTDOWN);
}

int
TlsSession::waitForData(unsigned ms_timeout, std::error_code& ec) const
{
    if (!pimpl_->transport_.waitForData(ms_timeout, ec))
        return 0;
    return 1;
}

}} // namespace ring::tls
