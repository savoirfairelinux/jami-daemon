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

#include <ip_utils.h>       // DO NOT CHANGE ORDER OF THIS INCLUDE
#include <opendht/crypto.h> // OR MINGWIN FAILS TO BUILD

#include "tls_session.h"

#include "ice_socket.h"
#include "ice_transport.h"
#include "logger.h"
#include "noncopyable.h"
#include "compiler_intrinsics.h"

#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <algorithm>
#include <cstring> // std::memset

namespace ring { namespace tls {

static constexpr const char* TLS_CERT_PRIORITY_STRING {"SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:-RSA:%SERVER_PRECEDENCE:%SAFE_RENEGOTIATION"};
static constexpr const char* TLS_FULL_PRIORITY_STRING {"SECURE192:-KX-ALL:+ANON-ECDH:+ANON-DH:+SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:-RSA:%SERVER_PRECEDENCE:%SAFE_RENEGOTIATION"};
static constexpr int DTLS_MTU {1232}; // (1280 from IPv6 minimum MTU - 40 IPv6 header - 8 UDP header)
static constexpr std::size_t INPUT_MAX_SIZE {1000}; // Maximum packet to store before dropping (pkt size = DTLS_MTU)
static constexpr ssize_t FLOOD_THRESHOLD {4*1024};
static constexpr auto FLOOD_PAUSE = std::chrono::milliseconds(100); // Time to wait after an invalid cookie packet (anti flood attack)
static constexpr auto DTLS_RETRANSMIT_TIMEOUT = std::chrono::milliseconds(1000); // Delay between two handshake request on DTLS
static constexpr auto COOKIE_TIMEOUT = std::chrono::seconds(10); // Time to wait for a cookie packet from client

// Helper to cast any duration into an integer number of milliseconds
template <class Rep, class Period>
static std::chrono::milliseconds::rep
duration2ms(std::chrono::duration<Rep, Period> d)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

DhParams::DhParams(const std::vector<uint8_t>& data)
{
    gnutls_dh_params_t new_params_;
    int ret = gnutls_dh_params_init(&new_params_);
    if (ret)
        throw std::runtime_error(std::string("Error initializing DH params: ") + gnutls_strerror(ret));
    params_.reset(new_params_);
    const gnutls_datum_t dat {(uint8_t*)data.data(), (unsigned)data.size()};
    if (int ret_pem = gnutls_dh_params_import_pkcs3(params_.get(), &dat, GNUTLS_X509_FMT_PEM))
        if (int ret_der = gnutls_dh_params_import_pkcs3(params_.get(), &dat, GNUTLS_X509_FMT_DER))
            throw std::runtime_error(std::string("Error importing DH params: ") + gnutls_strerror(ret_pem) + " " + gnutls_strerror(ret_der));
}

std::vector<uint8_t>
DhParams::serialize() const
{
    if (!params_) {
        RING_WARN("serialize() called on an empty DhParams");
        return {};
    }
    gnutls_datum_t out;
    if (gnutls_dh_params_export2_pkcs3(params_.get(), GNUTLS_X509_FMT_PEM, &out))
        return {};
    std::vector<uint8_t> ret {out.data, out.data+out.size};
    gnutls_free(out.data);
    return ret;
}

class TlsSession::TlsCertificateCredendials
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

class TlsSession::TlsAnonymousClientCredendials
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

class TlsSession::TlsAnonymousServerCredendials
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

TlsSession::TlsSession(std::shared_ptr<IceTransport> ice, int ice_comp_id,
                       const TlsParams& params, const TlsSessionCallbacks& cbs, bool anonymous)
    : socket_(new IceSocket(ice, ice_comp_id))
    , isServer_(not ice->isInitiator())
    , params_(params)
    , callbacks_(cbs)
    , anonymous_(anonymous)
    , cacred_(nullptr)
    , sacred_(nullptr)
    , xcred_(nullptr)
    , thread_([this] { return setup(); },
              [this] { process(); },
              [this] { cleanup(); })
{
    socket_->setOnRecv([this](uint8_t* buf, size_t len) {
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

    // Run FSM into dedicated thread
    thread_.start();
}

TlsSession::~TlsSession()
{
    shutdown();
    thread_.join();

    socket_->setOnRecv(nullptr);
}

const char*
TlsSession::typeName() const
{
    return isServer_ ? "server" : "client";
}

void
TlsSession::dump_io_stats() const
{
    RING_DBG("[TLS] RxRawPkt=%zu (%zu bytes) - TxRawPkt=%zu (%zu bytes)",
             stRxRawPacketCnt_.load(), stRxRawBytesCnt_.load(),
             stTxRawPacketCnt_.load(), stTxRawBytesCnt_.load());
}

TlsSessionState
TlsSession::setupClient()
{
    auto ret = gnutls_init(&session_, GNUTLS_CLIENT | GNUTLS_DATAGRAM);
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
TlsSession::setupServer()
{
    gnutls_key_generate(&cookie_key_, GNUTLS_COOKIE_KEY_SIZE);
    return TlsSessionState::COOKIE;
}

void
TlsSession::initAnonymous()
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
TlsSession::initCredentials()
{
    int ret;

    // credentials for handshaking and transmission
    xcred_.reset(new TlsCertificateCredendials());

    if (callbacks_.verifyCertificate)
        gnutls_certificate_set_verify_function(*xcred_, [](gnutls_session_t session) -> int {
                auto this_ = reinterpret_cast<TlsSession*>(gnutls_session_get_ptr(session));
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

    // Load user-given identity (key and passwd)
    if (params_.cert) {
        ret = gnutls_certificate_set_x509_key(*xcred_, &params_.cert->cert, 1,
                                              params_.cert_key->x509_key);
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
TlsSession::commonSessionInit()
{
    int ret;

    if (anonymous_) {
        // Force anonymous connection, see handleStateHandshake how we handle failures
        ret = gnutls_priority_set_direct(session_, TLS_FULL_PRIORITY_STRING, nullptr);
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
        ret = gnutls_priority_set_direct(session_, TLS_CERT_PRIORITY_STRING, nullptr);
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

    // DTLS hanshake timeouts
    auto re_tx_timeout = duration2ms(DTLS_RETRANSMIT_TIMEOUT);
    gnutls_dtls_set_timeouts(session_, re_tx_timeout,
                             std::max(duration2ms(params_.timeout), re_tx_timeout));

    // DTLS maximum payload size (raw packet relative)
    gnutls_dtls_set_mtu(session_, DTLS_MTU);

    // Stuff for transport callbacks
    gnutls_session_set_ptr(session_, this);
    gnutls_transport_set_ptr(session_, this);
    gnutls_transport_set_vec_push_function(session_,
                                           [](gnutls_transport_ptr_t t, const giovec_t* iov,
                                              int iovcnt) -> ssize_t {
                                               auto this_ = reinterpret_cast<TlsSession*>(t);
                                               return this_->sendRawVec(iov, iovcnt);
                                           });
    gnutls_transport_set_pull_function(session_,
                                       [](gnutls_transport_ptr_t t, void* d, size_t s) -> ssize_t {
                                           auto this_ = reinterpret_cast<TlsSession*>(t);
                                           return this_->recvRaw(d, s);
                                       });
    gnutls_transport_set_pull_timeout_function(session_,
                                               [](gnutls_transport_ptr_t t, unsigned ms) -> int {
                                                   auto this_ = reinterpret_cast<TlsSession*>(t);
                                                   return this_->waitForRawData(ms);
                                               });

    return true;
}

// Called by anyone to stop the connection and the FSM thread
void
TlsSession::shutdown()
{
    state_ = TlsSessionState::SHUTDOWN;
    rxCv_.notify_one(); // unblock waiting FSM
}

const char*
TlsSession::getCurrentCipherSuiteId(std::array<uint8_t, 2>& cs_id) const
{
    // get current session cipher suite info
    gnutls_cipher_algorithm_t cipher, s_cipher = gnutls_cipher_get(session_);
    gnutls_kx_algorithm_t kx, s_kx = gnutls_kx_get(session_);
    gnutls_mac_algorithm_t mac, s_mac = gnutls_mac_get(session_);

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

ssize_t
TlsSession::send(const void* data, std::size_t size)
{
    std::lock_guard<std::mutex> lk {txMutex_};
    if (state_ != TlsSessionState::ESTABLISHED)
        return GNUTLS_E_INVALID_SESSION;
    return send_(static_cast<const uint8_t*>(data), size);
}

ssize_t
TlsSession::send(const std::vector<uint8_t>& vec)
{
    return send(vec.data(), vec.size());
}

ssize_t
TlsSession::send_(const uint8_t* tx_data, std::size_t tx_size)
{
    std::size_t max_tx_sz = gnutls_dtls_get_data_mtu(session_);

    // Split user data into MTU-suitable chunck
    size_t total_written = 0;
    while (total_written < tx_size) {
        auto chunck_sz = std::min(max_tx_sz, tx_size - total_written);
        ssize_t nwritten;
        auto data_seq = tx_data + total_written;
        do {
            nwritten = gnutls_record_send(session_, data_seq, chunck_sz);
        } while (nwritten == GNUTLS_E_INTERRUPTED or nwritten == GNUTLS_E_AGAIN);
        if (nwritten <= 0) {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            RING_WARN("[TLS] send failed (only %zu bytes sent): %s", total_written,
                      gnutls_strerror(nwritten));
            return nwritten;
        }

        total_written += nwritten;
    }
    return total_written;
}

// Called by GNUTLS to send encrypted packet to low-level transport.
// Should return a positive number indicating the bytes sent, and -1 on error.
ssize_t
TlsSession::sendRaw(const void* buf, size_t size)
{
    auto ret = socket_->send(reinterpret_cast<const unsigned char*>(buf), size);
    if (ret > 0) {
        // log only on success
        ++stTxRawPacketCnt_;
        stTxRawBytesCnt_ += size;
        return ret;
    }
    return -1;
}

// Called by GNUTLS to send encrypted packet to low-level transport.
// Should return a positive number indicating the bytes sent, and -1 on error.
ssize_t
TlsSession::sendRawVec(const giovec_t* iov, int iovcnt)
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
TlsSession::recvRaw(void* buf, size_t size)
{
    std::lock_guard<std::mutex> lk {rxMutex_};
    if (rxQueue_.empty()) {
        gnutls_transport_set_errno(session_, EAGAIN);
        return -1;
    }

    const auto& pkt = rxQueue_.front();
    const std::size_t count = std::min(pkt.size(), size);
    std::copy_n(pkt.begin(), count, reinterpret_cast<uint8_t*>(buf));
    rxQueue_.pop_front();
    return count;
}

// Called by GNUTLS to wait for encrypted packet from low-level transport.
// 'timeout' is in milliseconds.
// Should return 0 on connection termination,
// a positive number indicating the number of bytes received,
// and -1 on error.
int
TlsSession::waitForRawData(unsigned timeout)
{
    std::unique_lock<std::mutex> lk {rxMutex_};
    if (not rxCv_.wait_for(lk, std::chrono::milliseconds(timeout),
                           [this]{ return !rxQueue_.empty() or state_ == TlsSessionState::SHUTDOWN; }))
        return 0;

    // shutdown?
    if (state_ == TlsSessionState::SHUTDOWN) {
        gnutls_transport_set_errno(session_, EINTR);
        return -1;
    }

    return rxQueue_.front().size();
}

bool
TlsSession::setup()
{
    // Setup FSM
    fsmHandlers_[TlsSessionState::SETUP] = [this](TlsSessionState s){ return handleStateSetup(s); };
    fsmHandlers_[TlsSessionState::COOKIE] = [this](TlsSessionState s){ return handleStateCookie(s); };
    fsmHandlers_[TlsSessionState::HANDSHAKE] = [this](TlsSessionState s){ return handleStateHandshake(s); };
    fsmHandlers_[TlsSessionState::ESTABLISHED] = [this](TlsSessionState s){ return handleStateEstablished(s); };
    fsmHandlers_[TlsSessionState::SHUTDOWN] = [this](TlsSessionState s){ return handleStateShutdown(s); };

    return true;
}

void
TlsSession::cleanup()
{
    state_ = TlsSessionState::SHUTDOWN; // be sure to block any user operations

    if (session_) {
        // DTLS: not use GNUTLS_SHUT_RDWR to not wait for a peer answer
        gnutls_bye(session_, GNUTLS_SHUT_WR);
        gnutls_deinit(session_);
        session_ = nullptr;
    }

    if (cookie_key_.data)
        gnutls_free(cookie_key_.data);
}

TlsSessionState
TlsSession::handleStateSetup(UNUSED TlsSessionState state)
{
    RING_DBG("[TLS] Start %s DTLS session", typeName());

    try {
        if (anonymous_)
            initAnonymous();
        initCredentials();
    } catch (const std::exception& e) {
        RING_ERR("[TLS] authentifications init failed: %s", e.what());
        return TlsSessionState::SHUTDOWN;
    }

    if (isServer_)
        return setupServer();
    else
        return setupClient();
}

TlsSessionState
TlsSession::handleStateCookie(TlsSessionState state)
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
                                    auto this_ = reinterpret_cast<TlsSession*>(t);
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

    ret = gnutls_init(&session_, GNUTLS_SERVER | GNUTLS_DATAGRAM);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] session init failed: %s", gnutls_strerror(ret));
        return TlsSessionState::SHUTDOWN;
    }

    gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUIRE);
    gnutls_dtls_prestate_set(session_, &prestate_);

    if (not commonSessionInit())
        return TlsSessionState::SHUTDOWN;

    return TlsSessionState::HANDSHAKE;
}

TlsSessionState
TlsSession::handleStateHandshake(TlsSessionState state)
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
    RING_WARN("[TLS] session established: %s", desc);
    gnutls_free(desc);

    // Anonymous connection? rehandshake immediatly with certificate authentification forced
    auto cred = gnutls_auth_get_type(session_);
    if (cred == GNUTLS_CRD_ANON) {
        RING_DBG("[TLS] renogotiate with certificate authentification");

        // Re-setup TLS algorithms priority list with only certificate based cipher suites
        ret = gnutls_priority_set_direct(session_, TLS_CERT_PRIORITY_STRING, nullptr);
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

    maxPayload_ = gnutls_dtls_get_data_mtu(session_);
    return TlsSessionState::ESTABLISHED;
}

TlsSessionState
TlsSession::handleStateEstablished(TlsSessionState state)
{
    // block until rx/tx packet or state change
    std::unique_lock<std::mutex> lk {rxMutex_};
    rxCv_.wait(lk, [this]{ return !rxQueue_.empty() or state_ != TlsSessionState::ESTABLISHED; });
    state = state_.load();
    if (state != TlsSessionState::ESTABLISHED)
        return state;

    // Handle RX data from network
    if (!rxQueue_.empty()) {
        std::vector<uint8_t> buf(8*1024);
        unsigned char sequence[8];

        lk.unlock();
        auto ret = gnutls_record_recv_seq(session_, buf.data(), buf.size(), sequence);
        if (ret > 0) {
            buf.resize(ret);
            // TODO: handle sequence re-order
            if (callbacks_.onRxData)
                callbacks_.onRxData(std::move(buf));
            return state;
        }

        if (ret == 0) {
            RING_DBG("[TLS] eof");
            return TlsSessionState::SHUTDOWN;
        }

        if (ret == GNUTLS_E_REHANDSHAKE) {
            RING_DBG("[TLS] re-handshake");
            return TlsSessionState::HANDSHAKE;
        }

        if (gnutls_error_is_fatal(ret)) {
            RING_ERR("[TLS] fatal error in recv: %s", gnutls_strerror(ret));
            return TlsSessionState::SHUTDOWN;
        }

        // non-fatal error... let's continue
        lk.lock();
    }

    return state;
}

TlsSessionState
TlsSession::handleStateShutdown(TlsSessionState state)
{
    RING_DBG("[TLS] shutdown");

    // Stop ourself
    thread_.stop();
    return state;
}

void
TlsSession::process()
{
    auto old_state = state_.load();
    auto new_state = fsmHandlers_[old_state](old_state);

    // update state_ with taking care for external state change
    if (not std::atomic_compare_exchange_strong(&state_, &old_state, new_state))
        new_state = old_state;

    if (old_state != new_state and callbacks_.onStateChange)
        callbacks_.onStateChange(new_state);
}

DhParams
DhParams::generate()
{
    using clock = std::chrono::high_resolution_clock;

    auto bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, /* GNUTLS_SEC_PARAM_HIGH */ GNUTLS_SEC_PARAM_HIGH);
    RING_DBG("Generating DH params with %u bits", bits);
    auto start = clock::now();

    gnutls_dh_params_t new_params_;
    int ret = gnutls_dh_params_init(&new_params_);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("Error initializing DH params: %s", gnutls_strerror(ret));
        return {};
    }
    DhParams params {new_params_};

    ret = gnutls_dh_params_generate2(params.get(), bits);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("Error generating DH params: %s", gnutls_strerror(ret));
        return {};
    }

    std::chrono::duration<double> time_span = clock::now() - start;
    RING_DBG("Generated DH params with %u bits in %lfs", bits, time_span.count());
    return params;
}

}} // namespace ring::tls
