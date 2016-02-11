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

#include "sips_transport_ice.h"
#include "ice_transport.h"
#include "ice_socket.h"
#include "manager.h"
#include "sip/sip_utils.h"
#include "logger.h"
#include "noncopyable.h"
#include "intrin.h"

#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/compat/socket.h>
#include <pj/lock.h>

#include <algorithm>
#include <cstring> // std::memset

namespace ring { namespace tls {

static constexpr int POOL_TP_INIT {512};
static constexpr int POOL_TP_INC {512};
static constexpr int TRANSPORT_INFO_LENGTH {64};
static constexpr int DTLS_MTU {1400}; // limit for networks like ADSL
static constexpr const char* TLS_PRIORITY_STRING {"SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:%SERVER_PRECEDENCE"};
static constexpr ssize_t FLOOD_THRESHOLD {4*1024};
static constexpr auto FLOOD_PAUSE = std::chrono::milliseconds(100); // Time to wait after an invalid cookie packet (anti flood attack)
static constexpr std::size_t INPUT_MAX_SIZE {1000}; // Maximum packet to store before dropping (pkt size = DTLS_MTU)

static void
sockaddr_to_host_port(pj_pool_t* pool,
                      pjsip_host_port* host_port,
                      const pj_sockaddr* addr)
{
    host_port->host.ptr = (char*) pj_pool_alloc(pool, PJ_INET6_ADDRSTRLEN+4);
    pj_sockaddr_print(addr, host_port->host.ptr, PJ_INET6_ADDRSTRLEN+4, 0);
    host_port->host.slen = pj_ansi_strlen(host_port->host.ptr);
    host_port->port = pj_sockaddr_get_port(addr);
}

static pj_status_t
tls_status_from_err(int err)
{
    pj_status_t status;

    switch (err) {
        case GNUTLS_E_SUCCESS:
            status = PJ_SUCCESS;
            break;
        case GNUTLS_E_MEMORY_ERROR:
            status = PJ_ENOMEM;
            break;
        case GNUTLS_E_LARGE_PACKET:
            status = PJ_ETOOBIG;
            break;
        case GNUTLS_E_NO_CERTIFICATE_FOUND:
            status = PJ_ENOTFOUND;
            break;
        case GNUTLS_E_SESSION_EOF:
            status = PJ_EEOF;
            break;
        case GNUTLS_E_HANDSHAKE_TOO_LARGE:
            status = PJ_ETOOBIG;
            break;
        case GNUTLS_E_EXPIRED:
            status = PJ_EGONE;
            break;
        case GNUTLS_E_TIMEDOUT:
            status = PJ_ETIMEDOUT;
            break;
        case GNUTLS_E_PREMATURE_TERMINATION:
            status = PJ_ECANCELLED;
            break;
        case GNUTLS_E_INTERNAL_ERROR:
        case GNUTLS_E_UNIMPLEMENTED_FEATURE:
            status = PJ_EBUG;
            break;
        case GNUTLS_E_AGAIN:
        case GNUTLS_E_INTERRUPTED:
        case GNUTLS_E_REHANDSHAKE:
            status = PJ_EPENDING;
            break;
        case GNUTLS_E_TOO_MANY_EMPTY_PACKETS:
        case GNUTLS_E_TOO_MANY_HANDSHAKE_PACKETS:
        case GNUTLS_E_RECORD_LIMIT_REACHED:
            status = PJ_ETOOMANY;
            break;
        case GNUTLS_E_UNSUPPORTED_VERSION_PACKET:
        case GNUTLS_E_UNSUPPORTED_SIGNATURE_ALGORITHM:
        case GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE:
        case GNUTLS_E_X509_UNSUPPORTED_ATTRIBUTE:
        case GNUTLS_E_X509_UNSUPPORTED_EXTENSION:
        case GNUTLS_E_X509_UNSUPPORTED_CRITICAL_EXTENSION:
            status = PJ_ENOTSUP;
            break;
        case GNUTLS_E_INVALID_SESSION:
        case GNUTLS_E_INVALID_REQUEST:
        case GNUTLS_E_INVALID_PASSWORD:
        case GNUTLS_E_ILLEGAL_PARAMETER:
        case GNUTLS_E_RECEIVED_ILLEGAL_EXTENSION:
        case GNUTLS_E_UNEXPECTED_PACKET:
        case GNUTLS_E_UNEXPECTED_PACKET_LENGTH:
        case GNUTLS_E_UNEXPECTED_HANDSHAKE_PACKET:
        case GNUTLS_E_UNWANTED_ALGORITHM:
        case GNUTLS_E_USER_ERROR:
            status = PJ_EINVAL;
            break;
        default:
            status = PJ_EUNKNOWN;
            break;
    }

    return status;
}

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

TlsSession::TlsSession(std::shared_ptr<IceTransport> ice, int ice_comp_id,
                       const TlsParams& params, const TlsSessionCallbacks& cbs)
    : socket_(new IceSocket(ice, ice_comp_id))
    , isServer_(not ice->isInitiator())
    , params_(params)
    , xcred_(nullptr)
    , thread_([this] { return setup(); },
              [this] { process(); },
              [this] { cleanup(); })
{
    // Setup TLS algorithms priority list
    auto ret = gnutls_priority_init(&priority_cache_, TLS_PRIORITY_STRING, nullptr);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] priority setup failed: %s", gnutls_strerror(ret));
        throw std::runtime_error("TlsSession");
    }

    callbacks_ = cbs;

    socket_->setOnRecv([this](uint8_t* buf, size_t len) {
            std::lock_guard<std::mutex> lk {inputDataMutex_};
            if (inputData_.size() == INPUT_MAX_SIZE) {
                inputData_.pop_front(); // drop oldest packet if input buffer is full
                ++stRxRawPacketDropCnt_;
            }
            inputData_.emplace_back(buf, buf+len);
            ioCv_.notify_one();
            ++stRxRawPacketCnt_;
            stRxRawBytesCnt_ += len;
            return len;
        });

    // Run FSM into dedicated thread
    thread_.start();
}

TlsSession::~TlsSession()
{
    shutdown();
    thread_.join();

    if (priority_cache_)
        gnutls_priority_deinit(priority_cache_);
}

const char*
TlsSession::typeName() const
{
    return isServer_ ? "server" : "client";
}

void
TlsSession::dump_io_stats() const
{
    RING_WARN("[TLS] RxRawPckt=%zu (%zu bytes)", stRxRawPacketCnt_, stRxRawBytesCnt_);
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
TlsSession::initCredentials()
{
    int ret;

    // credentials for handshaking and transmission
    xcred_.reset(new TlsCertificateCredendials());

    gnutls_certificate_set_verify_function(*xcred_, [](gnutls_session_t session) -> int {
            auto this_ = reinterpret_cast<TlsSession*>(gnutls_session_get_ptr(session));
            return this_->verifyCertificate();
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
    if (params_.id.first) {
        ret = gnutls_certificate_set_x509_key(*xcred_, &params_.id.second->cert, 1,
                                              params_.id.first->x509_key);
        if (ret < 0)
            throw std::runtime_error("can't load certificate: "
                                     + std::string(gnutls_strerror(ret)));

        RING_DBG("[TLS] User identity loaded");
    }

    // Setup DH-params (server only, may block on dh_params.get())
    if (isServer_) {
        if (auto& dh_params = params_.dh_params.get())
            gnutls_certificate_set_dh_params(*xcred_, dh_params.get());
        else
            RING_WARN("[TLS] DH params unavailable"); // YOMGUI: need to stop?
    }
}

bool
TlsSession::commonSessionInit()
{
    int ret;

    ret = gnutls_priority_set(session_, priority_cache_);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] session TLS priority set failed: %s", gnutls_strerror(ret));
        return false;
    }

    ret = gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, *xcred_);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("[TLS] session credential set failed: %s", gnutls_strerror(ret));
        return false;
    }

    // Stuff for transport callbacks
    gnutls_session_set_ptr(session_, this);
    gnutls_transport_set_ptr(session_, this);
    gnutls_dtls_set_mtu(session_, DTLS_MTU);

    // TODO: minimize user timeout
    auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(params_.timeout).count();
    gnutls_dtls_set_timeouts(session_, 1000, timeout);

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

void
TlsSession::shutdown()
{
    state_ = TlsSessionState::SHUTDOWN;
    ioCv_.notify_one(); // unblock input data wait
}

ssize_t
TlsSession::sendRaw(const void* buf, size_t size)
{
    auto ret = socket_->send(reinterpret_cast<const unsigned char*>(buf), size);
    if (ret > 0) {
        // log only on success
        ++stTxRawPacketCnt_;
        stTxRawBytesCnt_ += size;
    }
    return ret;
}

ssize_t
TlsSession::sendRawVec(const giovec_t* iov, int iovcnt)
{
    ssize_t sent = 0;
    for (int i=0; i<iovcnt; ++i) {
        const giovec_t& dat = iov[i];
        ssize_t ret = sendRaw(dat.iov_base, dat.iov_len);
        if (ret < 0)
            return ret;
        sent += ret;
    }
    return sent;
}

ssize_t
TlsSession::recvRaw(void* buf, size_t size)
{
    std::lock_guard<std::mutex> lk {inputDataMutex_};
    if (inputData_.empty()) {
        gnutls_transport_set_errno(session_, EAGAIN);
        return -1;
    }

    const auto& pkt = inputData_.front();
    const std::size_t count = std::min(pkt.size(), size);
    std::copy_n(pkt.begin(), count, reinterpret_cast<uint8_t*>(buf));
    inputData_.pop_front();
    return count;
}

int
TlsSession::waitForRawData(unsigned timeout)
{
    std::unique_lock<std::mutex> lk {inputDataMutex_};
    if (not ioCv_.wait_for(lk, std::chrono::milliseconds(timeout),
                           [this]{ return !inputData_.empty() or state_ == TlsSessionState::SHUTDOWN; }))
        return 0;

    // shutdown?
    if (state_ == TlsSessionState::SHUTDOWN) {
        gnutls_transport_set_errno(session_, EINTR);
        return -1;
    }

    return inputData_.front().size();
}

int
TlsSession::verifyCertificate()
{
    // Support only x509 format
    if (gnutls_certificate_type_get(session_) != GNUTLS_CRT_X509) {
        verifyStatus_ = PJ_SSL_CERT_EINVALID_FORMAT;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    // Store verification status
    unsigned int status = 0;
    auto ret = gnutls_certificate_verify_peers2(session_, &status);
    if (ret < 0 or (status & GNUTLS_CERT_SIGNATURE_FAILURE) != 0) {
        verifyStatus_ = PJ_SSL_CERT_EUNTRUSTED;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    unsigned int cert_list_size = 0;
    auto cert_list = gnutls_certificate_get_peers(session_, &cert_list_size);
    if (cert_list == nullptr) {
        verifyStatus_ = PJ_SSL_CERT_EISSUER_NOT_FOUND;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    if (params_.cert_check) {
        verifyStatus_ = params_.cert_check(status, cert_list, cert_list_size);
        if (verifyStatus_ != PJ_SUCCESS)
            return GNUTLS_E_CERTIFICATE_ERROR;
    }

    // notify GnuTLS to continue handshake normally
    return GNUTLS_E_SUCCESS;
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
#if 0
    {
        // Reply all SIP transport send requests with ENOTCONN status error
        std::unique_lock<std::mutex> l(outputBuffMtx_);

        for (const auto& f : outputBuff_) {
            f.tdata_op_key->tdata = nullptr;
            if (f.tdata_op_key->callback)
                outputAckBuff_.emplace_back(std::move(f), -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN)); // see handleEvents()
        }
        outputBuff_.clear();

        // make sure that all callbacks are called before closing
        cv_.wait(l, [&](){
                return outputAckBuff_.empty();
            });
    }
#endif

    if (cookie_key_.data)
        gnutls_free(cookie_key_.data);

    if (session_) {
        gnutls_bye(session_, GNUTLS_SHUT_WR); // DTLS: not recommended to use GNUTLS_SHUT_RDWR
        gnutls_deinit(session_);
    }
}

TlsSessionState
TlsSession::handleStateSetup(UNUSED TlsSessionState state)
{
    RING_DBG("[TLS] Start %s DTLS session", typeName());

    try {
        initCredentials();
    } catch (const std::exception& e) {
        RING_ERR("[TLS] credential init failed: %s", e.what());
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
        // block until packet or shutdown
        std::unique_lock<std::mutex> lk {inputDataMutex_};
        ioCv_.wait(lk, [this]{ return !inputData_.empty() or state_ == TlsSessionState::SHUTDOWN; });
        if (inputData_.empty())
            return TlsSessionState::SHUTDOWN;
        count = inputData_.front().size();
    }

    // Total bytes rx during cookie checking (see flood protection below)
    cookie_count_ += count;

    int ret;

    // Peek and verify front packet
    {
        std::lock_guard<std::mutex> lk {inputDataMutex_};
        auto& pkt = inputData_.front();
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
            std::lock_guard<std::mutex> lk {inputDataMutex_};
            inputData_.pop_front();
        }

        // Cookie may be sent on multiple network packets
        // So we retry until we get a valid cookie.
        // To protect against a flood attack we delay each retry after FLOOD_THRESHOLD rx bytes.
        if (cookie_count_ >= FLOOD_THRESHOLD) {
            RING_WARN("[TLS] flood threshold reach (retry in %lds)",
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

    if (ret == GNUTLS_E_SUCCESS) {
        auto desc = gnutls_session_get_desc(session_);
        RING_DBG("[TLS] Session established: %s", desc);
        gnutls_free(desc);

        return TlsSessionState::ESTABLISHED;
    }

    if (gnutls_error_is_fatal(ret)) {
        RING_ERR("[TLS] handshake failed: %s", gnutls_strerror(ret));
        dump_io_stats();
        return TlsSessionState::SHUTDOWN;
    }

    // TODO: send EPENDING status
    // TODO: handle GNUTLS_E_LARGE_PACKET (MTU must be lowered)
    return state;
}

TlsSessionState
TlsSession::handleStateEstablished(TlsSessionState state)
{
    unsigned char sequence[8];

    if (not gnutls_record_check_pending(session_)) {
        // block until packet or state change
        std::unique_lock<std::mutex> lk {inputDataMutex_};
        ioCv_.wait(lk, [this]{ return !inputData_.empty() or state_ != TlsSessionState::ESTABLISHED; });
        state = state_.load();
        if (state != TlsSessionState::ESTABLISHED)
            return state;
        if (inputData_.empty())
            return state;
    }

    std::vector<uint8_t> buf(8*1024);
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
    return state;
}

TlsSessionState
TlsSession::handleStateShutdown(TlsSessionState state)
{
    RING_DBG("[TLS] shutdown");
    thread_.stop();
    return state;
}

void
TlsSession::process()
{
    auto old_state = state_.load();
    auto new_state = fsmHandlers_[old_state](old_state);

    // check for external state change
    if (not std::atomic_compare_exchange_strong(&state_, &old_state, new_state))
        new_state = state_;

    if (old_state != new_state and callbacks_.onStateChange)
        callbacks_.onStateChange(new_state);
}

//==================================================================================================
//==================================================================================================
//==================================================================================================
//==================================================================================================
//==================================================================================================
//==================================================================================================

SipsIceTransport::SipsIceTransport(pjsip_endpoint* endpt,
                                   const TlsParams& param,
                                   const std::shared_ptr<ring::IceTransport>& ice,
                                   int comp_id)
    : pool_  {nullptr, pj_pool_release}
    , rxPool_ (nullptr, pj_pool_release)
    , trData_ ()
    , ice_ (ice)
    , comp_id_ (comp_id)
{
    RING_DBG("SipIceTransport@%p {tr=%p}", this, &trData_.base);

    if (not ice or not ice->isRunning())
        throw std::logic_error("ICE transport must exist and negotiation completed");

    trData_.self = this; // up-link for PJSIP callbacks

    pool_ = std::move(sip_utils::smart_alloc_pool(endpt, "SipsIceTransport.pool",
                                                  POOL_TP_INIT, POOL_TP_INC));

    auto& base = trData_.base;
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "SipsIceTransport");
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool_.get();

    if (pj_atomic_create(pool_.get(), 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool_.get(), "SipsIceTransport.mutex",
                                       &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    local_ = ice->getLocalAddress(comp_id);
    remote_ = ice->getRemoteAddress(comp_id);
    pj_sockaddr_cp(&base.key.rem_addr, remote_.pjPtr());
    base.key.type = PJSIP_TRANSPORT_TLS;
    base.type_name = (char*)pjsip_transport_get_type_name((pjsip_transport_type_e)base.key.type);
    base.flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)base.key.type);
    base.info = (char*) pj_pool_alloc(pool_.get(), TRANSPORT_INFO_LENGTH);

    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_ansi_snprintf(base.info, TRANSPORT_INFO_LENGTH, "%s to %s", base.type_name,
                     pj_sockaddr_print(remote_.pjPtr(), print_addr, sizeof(print_addr), 3));
    base.addr_len = remote_.getLength();
    base.dir = PJSIP_TP_DIR_NONE;
    base.data = nullptr;

    /* Set initial local address */
    auto local = ice->getDefaultLocalAddress();
    pj_sockaddr_cp(&base.local_addr, local.pjPtr());

    sockaddr_to_host_port(pool_.get(), &base.local_name, &base.local_addr);
    sockaddr_to_host_port(pool_.get(), &base.remote_name, remote_.pjPtr());

    base.send_msg = [](pjsip_transport *transport,
                       pjsip_tx_data *tdata,
                       const pj_sockaddr_t *rem_addr, int addr_len,
                       void *token, pjsip_transport_callback callback) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_DBG("SipsIceTransport@%p: shutdown", this_);
        this_->tls_->shutdown();
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_DBG("SipsIceTransport@%p: destroy", this_);
        delete this_;
        return PJ_SUCCESS;
    };

    /* Init rdata_ */
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));
    rxPool_ = std::move(sip_utils::smart_alloc_pool(endpt, "SipsIceTransport.rxPool",
                                                    PJSIP_POOL_RDATA_LEN, PJSIP_POOL_RDATA_LEN));
    rdata_.tp_info.pool = rxPool_.get();
    rdata_.tp_info.transport = &base;
    rdata_.tp_info.tp_data = this;
    rdata_.tp_info.op_key.rdata = &rdata_;
    pj_ioqueue_op_key_init(&rdata_.tp_info.op_key.op_key,
                           sizeof(pj_ioqueue_op_key_t));
    rdata_.pkt_info.src_addr = base.key.rem_addr;
    rdata_.pkt_info.src_addr_len = sizeof(rdata_.pkt_info.src_addr);
    auto rem_addr = &base.key.rem_addr;
    pj_sockaddr_print(rem_addr, rdata_.pkt_info.src_name,
                      sizeof(rdata_.pkt_info.src_name), 0);
    rdata_.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);

    std::memset(&localCertInfo_, 0, sizeof(pj_ssl_cert_info));
    std::memset(&remoteCertInfo_, 0, sizeof(pj_ssl_cert_info));

    TlsSession::TlsSessionCallbacks cbs = {
        .onStateChange = [this](TlsSessionState state){ onTlsStateChange(state); },
        .onRxData = [this](std::vector<uint8_t>&& buf){ onRxData(std::move(buf)); }
    };
    tls_.reset(new TlsSession(ice, comp_id, param, cbs));

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");

    Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
}

SipsIceTransport::~SipsIceTransport()
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);

    // Stop low-level transport first
    tls_.reset();

    // Flush PJSIP/TLS events
    handleEvents();

    pjsip_transport_add_ref(getTransportBase());

    if (auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr)) {
        pj_ssl_sock_info ssl_info;
        std::memset(&ssl_info, 0, sizeof(ssl_info));
        //getInfo(&ssl_info);

        pjsip_tls_state_info tls_info;
        std::memset(&tls_info, 0, sizeof(tls_info));
        tls_info.ssl_sock_info = &ssl_info;

        pjsip_transport_state_info state_info;
        std::memset(&state_info, 0, sizeof(state_info));
        state_info.ext_info = &tls_info;
        state_info.status = PJ_SUCCESS;

        try {
            (*state_cb)(getTransportBase(), PJSIP_TP_STATE_DISCONNECTED, &state_info);
        } catch (...) {
            // silent
        }
    }

    if (not trData_.base.is_shutdown and not trData_.base.is_destroying)
        pjsip_transport_shutdown(getTransportBase());

    pjsip_transport_dec_ref(getTransportBase());
    pj_lock_destroy(trData_.base.lock);
    pj_atomic_destroy(trData_.base.ref_cnt);
}

void
SipsIceTransport::handleEvents()
{
#if 0
    // Process state changes first
    decltype(stateChangeEvents_) eventDataQueue = stateChangeEvents_;
    {
        std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
        eventDataQueue = std::move(stateChangeEvents_);
    }
    if (auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr)) {
        // application notification
        while (not eventDataQueue.empty()) {
            auto& evdata = eventDataQueue.front();
            evdata.tls_info.ssl_sock_info = &evdata.ssl_info;
            evdata.state_info.ext_info = &evdata.tls_info;
            (*state_cb)(&trData_.base, evdata.state, &evdata.state_info);
            eventDataQueue.pop();
        }
    }
#endif

    // Handle stream GnuTLS -> SIP transport
    decltype(rxPending_) rx;
    {
        std::lock_guard<std::mutex> l(rxMtx_);
        rx = std::move(rxPending_);
    }

    for (auto it = rx.begin(); it != rx.end(); ++it) {
        auto& pck = *it;
        pj_pool_reset(rdata_.tp_info.pool);
        pj_gettimeofday(&rdata_.pkt_info.timestamp);
        rdata_.pkt_info.len = pck.size();
        std::copy_n(pck.data(), pck.size(), rdata_.pkt_info.packet);
        auto eaten = pjsip_tpmgr_receive_packet(trData_.base.tpmgr, &rdata_);
#if 0
        if (eaten >= 0) {
            RING_WARN("rx %zu, eat %zu", pck.size(), eaten);
            if (eaten > 0)
                RING_ERR("%s", std::string(std::begin(pck), std::begin(pck)+eaten).c_str());
        }
#endif
        if (eaten != rdata_.pkt_info.len) {
            // partial sip packet received
            auto npck_it = std::next(it);
            if (npck_it != rx.end()) {
                // drop current packet, merge reminder with next one
                auto& npck = *npck_it;
                npck.insert(npck.begin(), pck.begin()+eaten, pck.end());
            } else {
                // erase eaten part, keep reminder
                pck.erase(pck.begin(), pck.begin()+eaten);
                {
                    std::lock_guard<std::mutex> l(rxMtx_);
                    rxPending_.splice(rxPending_.begin(), rx, it);
                }
                break;
            }
        }
    }

#if 0
    // Report status GnuTLS -> SIP transport
    decltype(outputAckBuff_) ackBuf;
    {
        std::lock_guard<std::mutex> l(outputBuffMtx_);
        ackBuf = std::move(outputAckBuff_);
    }
    for (const auto& pair: ackBuf) {
        const auto& f = pair.first;
        f.tdata_op_key->tdata = nullptr;
        RING_DBG("status: %ld", pair.second);
        if (f.tdata_op_key->callback)
            f.tdata_op_key->callback(getTransportBase(), f.tdata_op_key->token,
                                     pair.second);
    }
    cv_.notify_all();
#endif
}

void
SipsIceTransport::onTlsStateChange(UNUSED TlsSessionState state)
{
    if (state == TlsSessionState::ESTABLISHED) {
        if (auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr)) {
            pj_ssl_sock_info ssl_info;
            getInfo(&ssl_info, true);
            pjsip_transport_state_info state_info;
            std::memset(&state_info, 0, sizeof(state_info));
            state_info.status = ssl_info.verify_status ? PJSIP_TLS_ECERTVERIF : PJ_SUCCESS;
            pjsip_tls_state_info tls_info;
            std::memset(&tls_info, 0, sizeof(tls_info));
            tls_info.ssl_sock_info = &ssl_info;
            state_info.ext_info = &tls_info;
            (*state_cb)(&trData_.base, PJSIP_TP_STATE_CONNECTED, &state_info);
        }
    } else if (state == TlsSessionState::SHUTDOWN) {
        if (auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr)) {
            pjsip_transport_state_info state_info;
            std::memset(&state_info, 0, sizeof(state_info));
            state_info.status = PJ_SUCCESS;
            (*state_cb)(&trData_.base, PJSIP_TP_STATE_DISCONNECTED, &state_info);
        }
    }
#if 0
    if (state == TlsSessionState::ESTABLISHED) {
        certUpdate();

        ChangeStateEventData ev;
        ev.state = PJSIP_TP_STATE_CONNECTED;
        pushEvent(ev);
    } else if (state == TlsSessionState::SHUTDOWN) {
        ChangeStateEventData ev;
        ev.state = PJSIP_TP_STATE_DISCONNECTED;
        pushEvent(ev);
    }
#endif
}

void
SipsIceTransport::onRxData(std::vector<uint8_t>&& buf)
{
    std::lock_guard<std::mutex> l(rxMtx_);
    if (rxPending_.empty())
        rxPending_.emplace_back(std::move(buf));
    else {
        auto& last = rxPending_.back();
        last.insert(std::end(last), std::begin(buf), std::end(buf));
    }
}

void
SipsIceTransport::getInfo(pj_ssl_sock_info* info, bool established)
{
    std::memset(info, 0, sizeof(*info));

    // Established flag
    info->established = established;

    // Protocol
    info->proto = PJ_SSL_SOCK_PROTO_DTLS1;

    // Local address
    pj_sockaddr_cp(&info->local_addr, local_.pjPtr());

    if (established) {
        const auto cipher = gnutls_cipher_get(tls_->getGnuTlsSession()); // Current cipher
        gnutls_cipher_algorithm_t lookup;
        unsigned char id[2];

        for (size_t i=0; ; ++i) {
            const auto suite = gnutls_cipher_suite_info(i, id, nullptr, &lookup, nullptr, nullptr);
            if (not suite) {
                RING_ERR("Can't find info for cipher %s (%d)", gnutls_cipher_get_name(cipher), cipher);
                break;
            }

            if (lookup == cipher) {
                info->cipher = (pj_ssl_cipher) ((id[0] << 8) | id[1]);
                break;
            }
        }

        // Remote address
        pj_sockaddr_cp(&info->remote_addr, remote_.pjPtr());

        // Certificates info
        info->local_cert_info = &localCertInfo_;
        info->remote_cert_info = &remoteCertInfo_;

        // Verification status
        info->verify_status = tls_->getVerifyStatus();
    }

    // Last known GnuTLS error code
    info->last_native_err = GNUTLS_E_SUCCESS;
}

pj_status_t
SipsIceTransport::send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                       int addr_len, void *token,
                       pjsip_transport_callback callback)
{
    // Sanity check
    PJ_ASSERT_RETURN(tdata, PJ_EINVAL);

    // Check that there's no pending operation associated with the tdata
    PJ_ASSERT_RETURN(tdata->op_key.tdata == nullptr, PJSIP_EPENDINGTX);

    // Check the address is supported
    PJ_ASSERT_RETURN(rem_addr and
                     (addr_len==sizeof(pj_sockaddr_in) or
                      addr_len==sizeof(pj_sockaddr_in6)),
                     PJ_EINVAL);

    if (!tls_->getGnuTlsSession()) {
        RING_ERR("no session!");
        return PJ_EPENDING;
    }

    tdata->op_key.token = token;
    tdata->op_key.callback = callback;
    auto ret = trySend(tdata);
    if (ret < 0) {
        RING_ERR("[TLS] send failed: %s", gnutls_strerror(ret));
        return tls_status_from_err(ret);
    }
    tdata->op_key.tdata = nullptr;
    if (tdata->op_key.callback)
        tdata->op_key.callback(getTransportBase(), token, ret);

    return PJ_EPENDING;
}

ssize_t
SipsIceTransport::trySend(pjsip_tx_data* tdata)
{
    auto session = tls_->getGnuTlsSession();
    const size_t size = tdata->buf.cur - tdata->buf.start;
    const size_t max_tx_sz = gnutls_dtls_get_data_mtu(session);

    size_t total_written = 0;
    while (total_written < size) {
        /* Ask GnuTLS to encrypt our plaintext now. GnuTLS will use the push
         * callback to actually send the encrypted bytes. */
        const auto tx_size = std::min(max_tx_sz, size - total_written);
        const auto nwritten = gnutls_record_send(session,
                                                 tdata->buf.start + total_written,
                                                 tx_size);
        if (nwritten <= 0) {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            return nwritten;
        }

        /* Good, some data was encrypted and written */
        total_written += nwritten;
    }
    return total_written;
}

//==================================================================================================
//==================================================================================================
//==================================================================================================
//==================================================================================================
//==================================================================================================
//==================================================================================================

#if 0
pj_status_t
SipsIceTransport::send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                       int addr_len, void *token,
                       pjsip_transport_callback callback)
{
    // Sanity check
    PJ_ASSERT_RETURN(tdata, PJ_EINVAL);

    // Check that there's no pending operation associated with the tdata
    PJ_ASSERT_RETURN(tdata->op_key.tdata == nullptr, PJSIP_EPENDINGTX);

    // Check the address is supported
    PJ_ASSERT_RETURN(rem_addr and
                     (addr_len==sizeof(pj_sockaddr_in) or
                      addr_len==sizeof(pj_sockaddr_in6)),
                     PJ_EINVAL);

    tdata->op_key.token = token;
    tdata->op_key.callback = callback;
    if (state_ == TlsSessionState::ESTABLISHED) {
        decltype(txBuff_) tx;
        size_t size = tdata->buf.cur - tdata->buf.start;
        getBuff(tx, (uint8_t*)tdata->buf.start, (uint8_t*)tdata->buf.cur);
        {
            std::lock_guard<std::mutex> l(outputBuffMtx_);
            txBuff_.splice(txBuff_.end(), std::move(tx));
        }
        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(getTransportBase(), token, size);
    } else {
        std::lock_guard<std::mutex> l(outputBuffMtx_);
        tdata->op_key.tdata = tdata;
        outputBuff_.emplace_back(DelayedTxData{&tdata->op_key, {}});
        if (tdata->msg && tdata->msg->type == PJSIP_REQUEST_MSG) {
            auto& dtd = outputBuff_.back();
            dtd.timeout = clock::now();
            dtd.timeout += std::chrono::milliseconds(pjsip_cfg()->tsx.td);
        }
    }
    canWrite_ = true;
    cv_.notify_all();
    return PJ_EPENDING;
}

pj_status_t
SipsIceTransport::flushOutputBuff()
{
    ssize_t status = PJ_SUCCESS;

    // send delayed data first
    for (;;) {
        bool fatal = true;
        DelayedTxData f;

        {
            std::lock_guard<std::mutex> l(outputBuffMtx_);
            if (outputBuff_.empty())
                break;
            f = std::move(outputBuff_.front());
            outputBuff_.pop_front();
        }

        // Too late?
        if (f.timeout != clock::time_point() && f.timeout < clock::now()) {
            status = GNUTLS_E_TIMEDOUT;
            fatal = false;
        } else {
            status = trySend(f.tdata_op_key);
            fatal = status < 0;
            if (fatal) {
                if (gnutls_error_is_fatal(status))
                    tlsThread_.stop();
                else {
                    // Failed but non-fatal. Retry later.
                    std::lock_guard<std::mutex> l(outputBuffMtx_);
                    outputBuff_.emplace_front(std::move(f));
                }
            }
        }

        f.tdata_op_key->tdata = nullptr;

        if (f.tdata_op_key->callback) {
            std::lock_guard<std::mutex> l(outputBuffMtx_);
            outputAckBuff_.emplace_back(std::move(f), status > 0 ? status : -tls_status_from_err(status)); // see handleEvents()
        }

        if (fatal)
            return -tls_status_from_err(status);
    }

    decltype(txBuff_) tx;
    {
        std::lock_guard<std::mutex> l(outputBuffMtx_);
        tx = std::move(txBuff_);
        canWrite_ = false;
    }
    for (auto it = tx.begin(); it != tx.end(); ++it) {
        const auto& msg = *it;
        const auto nwritten = gnutls_record_send(session_, msg.data(), msg.size());
        if (nwritten <= 0) {
            status = nwritten;
            if (gnutls_error_is_fatal(status))
                tlsThread_.stop();
            {
                std::lock_guard<std::mutex> l(outputBuffMtx_);
                txBuff_.splice(txBuff_.begin(), tx, it, tx.end());
                canWrite_ = true;
            }
            break;
        }
    }
    putBuff(std::move(tx));
    return status > 0 ? PJ_SUCCESS : -tls_status_from_err(status);
}

ssize_t
SipsIceTransport::trySend(pjsip_tx_data* tdata)
{
    const auto tdata = pck->tdata;
    const size_t size = tdata->buf.cur - tdata->buf.start;
    const size_t max_tx_sz = gnutls_dtls_get_data_mtu(session_);

    size_t total_written = 0;
    while (total_written < size) {
        /* Ask GnuTLS to encrypt our plaintext now. GnuTLS will use the push
         * callback to actually send the encrypted bytes. */
        const auto tx_size = std::min(max_tx_sz, size - total_written);
        const auto nwritten = gnutls_record_send(session_,
                                                 tdata->buf.start + total_written,
                                                 tx_size);
        if (nwritten <= 0) {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            return nwritten;
        }

        /* Good, some data was encrypted and written */
        total_written += nwritten;
    }
    return total_written;
}

void
SipsIceTransport::pushEvent(ChangeStateEventData&& ev)
{
    getInfo(&ev.ssl_info);
    ev.state_info.status = ev.ssl_info.verify_status ? PJSIP_TLS_ECERTVERIF : PJ_SUCCESS;
    std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
    stateChangeEvents_.emplace(std::move(ev));
}

void
SipsIceTransport::certGetCn(const pj_str_t* gen_name, pj_str_t* cn)
{
    pj_str_t CN_sign = {(char*)"CN=", 3};
    char *p, *q;

    std::memset(cn, 0, sizeof(*cn));

    p = pj_strstr(gen_name, &CN_sign);
    if (!p)
        return;

    p += 3; /* shift pointer to value part */
    pj_strset(cn, p, gen_name->slen - (p - gen_name->ptr));
    q = pj_strchr(cn, ',');
    if (q)
        cn->slen = q - p;
}

/* Update local & remote certificates info. This function should be
 * called after handshake or re-negotiation successfully completed.
 */
void
SipsIceTransport::certUpdate()
{
    // Get active local certificate
    if(const auto local_raw = gnutls_certificate_get_ours(tls_.getGnuTlsSession()))
        certGetInfo(pool_.get(), &localCertInfo_, local_raw, 1);
    else
        std::memset(&localCertInfo_, 0, sizeof(pj_ssl_cert_info));

    unsigned int certslen = 0;
    if (const auto remote_raw = gnutls_certificate_get_peers(session_, &certslen))
        certGetInfo(pool_.get(), &remoteCertInfo_, remote_raw, certslen);
    else
        std::memset(&remoteCertInfo_, 0, sizeof(pj_ssl_cert_info));
}

/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer and the serial number.
 */
void
SipsIceTransport::certGetInfo(pj_pool_t* pool, pj_ssl_cert_info* ci,
                              const gnutls_datum_t* crt_raw, size_t crt_raw_num)
{
    char buf[512] = { 0 };
    size_t bufsize = sizeof(buf);
    std::array<uint8_t, sizeof(ci->serial_no)> serial_no; /* should be >= sizeof(ci->serial_no) */
    size_t serialsize = serial_no.size();
    size_t len = sizeof(buf);
    int i, ret, seq = 0;
    pj_ssl_cert_name_type type;

    pj_assert(pool && ci && crt_raw);

    dht::crypto::Certificate crt(crt_raw[0].data, crt_raw[0].size);

    /* Get issuer */
    gnutls_x509_crt_get_issuer_dn(crt.cert, buf, &bufsize);

    /* Get serial no */
    gnutls_x509_crt_get_serial(crt.cert, serial_no.data(), &serialsize);

    /* Check if the contents need to be updated */
    if (not pj_strcmp2(&ci->issuer.info, buf) and
        not std::memcmp(ci->serial_no, serial_no.data(), serialsize))
        return;

    /* Update cert info */
    std::memset(ci, 0, sizeof(pj_ssl_cert_info));

    /* Full raw certificate */
    ci->raw_chain.cert_raw = (pj_str_t*)pj_pool_calloc(pool, crt_raw_num, sizeof(*ci->raw_chain.cert_raw));
    ci->raw_chain.cnt = crt_raw_num;
    for (size_t i=0; i < crt_raw_num; ++i) {
        const pj_str_t cert = {(char*)crt_raw[i].data, (pj_ssize_t)crt_raw[i].size};
        pj_strdup(pool, ci->raw_chain.cert_raw+i, &cert);
    }

    /* Version */
    ci->version = gnutls_x509_crt_get_version(crt.cert);

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
    certGetCn(&ci->issuer.info, &ci->issuer.cn);

    /* Serial number */
    std::copy(serial_no.cbegin(), serial_no.cend(), (uint8_t*)ci->serial_no);

    /* Subject */
    bufsize = sizeof(buf);
    gnutls_x509_crt_get_dn(crt.cert, buf, &bufsize);
    pj_strdup2(pool, &ci->subject.info, buf);
    certGetCn(&ci->subject.info, &ci->subject.cn);

    /* Validity */
    ci->validity.end.sec = gnutls_x509_crt_get_expiration_time(crt.cert);
    ci->validity.start.sec = gnutls_x509_crt_get_activation_time(crt.cert);
    ci->validity.gmt = 0;

    /* Subject Alternative Name extension */
    if (ci->version >= 3) {
        char out[256] = { 0 };
        /* Get the number of all alternate names so that we can allocate
         * the correct number of bytes in subj_alt_name */
        while (gnutls_x509_crt_get_subject_alt_name(crt.cert, seq, out, &len, nullptr) != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
            seq++;

        ci->subj_alt_name.entry = \
            (decltype(ci->subj_alt_name.entry))pj_pool_calloc(pool, seq, sizeof(*ci->subj_alt_name.entry));
        if (!ci->subj_alt_name.entry) {
            last_err_ = GNUTLS_E_MEMORY_ERROR;
            return;
        }

        /* Now populate the alternative names */
        for (i = 0; i < seq; i++) {
            len = sizeof(out) - 1;
            ret = gnutls_x509_crt_get_subject_alt_name(crt.cert, i, out, &len, nullptr);
            switch (ret) {
                case GNUTLS_SAN_IPADDRESS:
                    type = PJ_SSL_CERT_NAME_IP;
                    pj_inet_ntop2(len == sizeof(pj_in6_addr) ? pj_AF_INET6() : pj_AF_INET(),
                                  out, buf, sizeof(buf));
                    break;
                case GNUTLS_SAN_URI:
                    type = PJ_SSL_CERT_NAME_URI;
                    break;
                case GNUTLS_SAN_RFC822NAME:
                    type = PJ_SSL_CERT_NAME_RFC822;
                    break;
                case GNUTLS_SAN_DNSNAME:
                    type = PJ_SSL_CERT_NAME_DNS;
                    break;
                default:
                    type = PJ_SSL_CERT_NAME_UNKNOWN;
                    break;
            }

            if (len && type != PJ_SSL_CERT_NAME_UNKNOWN) {
                ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
                pj_strdup2(pool,
                           &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
                           type == PJ_SSL_CERT_NAME_IP ? buf : out);
                ci->subj_alt_name.cnt++;
            }
        }
        /* TODO: if no DNS alt. names were found, we could check against
         * the commonName as per RFC3280. */
    }
}

#endif

}} // namespace ring::tls
