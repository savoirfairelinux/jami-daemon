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

#include "sips_transport_ice.h"

#include "ice_socket.h"
#include "ice_transport.h"

#include "manager.h"
#include "sip/sip_utils.h"
#include "logger.h"
#include "compiler_intrinsics.h"

#include <opendht/crypto.h>

#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/compat/socket.h>
#include <pj/lock.h>

#include <algorithm>
#include <system_error>
#include <cstring> // std::memset

namespace jami { namespace tls {

static constexpr int POOL_TP_INIT {512};
static constexpr int POOL_TP_INC {512};
static constexpr int TRANSPORT_INFO_LENGTH {64};

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

SipsIceTransport::SipsIceTransport(pjsip_endpoint* endpt,
                                   int tp_type,
                                   const TlsParams& param,
                                   const std::shared_ptr<jami::IceTransport>& ice,
                                   int comp_id)
    : ice_ (ice)
    , comp_id_ (comp_id)
    , certCheck_(param.cert_check)
    , trData_ ()
    , pool_  {nullptr, pj_pool_release}
    , rxPool_ (nullptr, pj_pool_release)
{
    JAMI_DBG("SipIceTransport@%p {tr=%p}", this, &trData_.base);

    if (not ice or not ice->isRunning())
        throw std::logic_error("ICE transport must exist and negotiation completed");

    trData_.self = this; // up-link for PJSIP callbacks

    pool_ = sip_utils::smart_alloc_pool(endpt, "dtls.pool",
                                        POOL_TP_INIT, POOL_TP_INC);

    auto& base = trData_.base;
    std::memset(&base, 0, sizeof(base));

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "dtls%p", &base);
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool_.get();

    if (pj_atomic_create(pool_.get(), 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool_.get(), "dtls",
                                       &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    local_ = ice->getLocalAddress(comp_id);
    remote_ = ice->getRemoteAddress(comp_id);
    pj_sockaddr_cp(&base.key.rem_addr, remote_.pjPtr());
    base.key.type = tp_type;
    auto reg_type = static_cast<pjsip_transport_type_e>(tp_type);
    base.type_name = const_cast<char*>(pjsip_transport_get_type_name(reg_type));
    base.flag = pjsip_transport_get_flag_from_type(reg_type);
    base.info = static_cast<char*>(pj_pool_alloc(pool_.get(), TRANSPORT_INFO_LENGTH));

    auto remote_addr = remote_.toString();
    pj_ansi_snprintf(base.info, TRANSPORT_INFO_LENGTH, "%s to %s", base.type_name,
                     remote_addr.c_str());
    base.addr_len = remote_.getLength();
    base.dir = PJSIP_TP_DIR_NONE;

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
        JAMI_DBG("SipsIceTransport@%p {tr=%p {rc=%ld}}: shutdown", this_,
                 transport, pj_atomic_get(transport->ref_cnt));
        // Nothing to do here, tls session is not shutdown as some messages could be pending
        // and application can continue to do IO (if they already own the transport)
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        JAMI_DBG("SipsIceTransport@%p: destroying", this_);
        delete this_;
        return PJ_SUCCESS;
    };

    /* Init rdata_ */
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));
    rxPool_ = sip_utils::smart_alloc_pool(endpt, "dtls.rxPool",
                                          PJSIP_POOL_RDATA_LEN, PJSIP_POOL_RDATA_LEN);
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

    iceSocket_ = std::make_unique<IceSocketTransport>(ice_, comp_id, PJSIP_TRANSPORT_IS_RELIABLE(&trData_.base));

    TlsSession::TlsSessionCallbacks cbs = {
        /*.onStateChange = */[this](TlsSessionState state){ onTlsStateChange(state); },
        /*.onRxData = */[this](std::vector<uint8_t>&& buf){ onRxData(std::move(buf)); },
        /*.onCertificatesUpdate = */[this](const gnutls_datum_t* l, const gnutls_datum_t* r,
                                           unsigned int n){ onCertificatesUpdate(l, r, n); },
        /*.verifyCertificate = */[this](gnutls_session_t session){ return verifyCertificate(session); }
    };
    tls_ = std::make_unique<TlsSession>(*iceSocket_, param, cbs);

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");

    if (PJSIP_TRANSPORT_IS_RELIABLE(&trData_.base)) {
        eventLoopFut_ = {std::async(std::launch::async, [this] {
            try {
                if (!stopLoop_) eventLoop();
            } catch (const std::exception& e) {
                JAMI_ERR() << "SipIceTransport: eventLoop() failure: " << e.what();
            }
        })};
    }
}

SipsIceTransport::~SipsIceTransport()
{
    JAMI_DBG("~SipIceTransport@%p {tr=%p}", this, &trData_.base);
    stopLoop_ = true;

    // Flush send queue with ENOTCONN error
    for (auto tdata : txQueue_) {
        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(&trData_.base, tdata->op_key.token,
                                   -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN));
    }

    auto base = getTransportBase();

    // Stop low-level transport first
    tls_.reset();

    // If delete not trigged by pjsip_transport_destroy (happen if objet not given to pjsip)
    if (not base->is_shutdown and not base->is_destroying)
        pjsip_transport_shutdown(base);

    pj_lock_destroy(base->lock);
    pj_atomic_destroy(base->ref_cnt);
    JAMI_DBG("~SipIceTransport@%p {tr=%p} bye", this, &trData_.base);
}

void
SipsIceTransport::handleEvents()
{
    // Notify transport manager about state changes first
    // Note: stop when disconnected event is encountered
    // and differ its notification AFTER pending rx msg to let
    // them a chance to be delivered to application before closing
    // the transport.
    decltype(stateChangeEvents_) eventDataQueue;
    {
        std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
        eventDataQueue = std::move(stateChangeEvents_);
        stateChangeEvents_.clear();
    }

    ChangeStateEventData disconnectedEvent;
    bool disconnected = false;
    auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr);
    if (state_cb) {
        for (auto& evdata : eventDataQueue) {
            evdata.tls_info.ssl_sock_info = &evdata.ssl_info;
            evdata.state_info.ext_info = &evdata.tls_info;
            if (evdata.state != PJSIP_TP_STATE_DISCONNECTED) {
                (*state_cb)(&trData_.base, evdata.state, &evdata.state_info);
            } else {
                JAMI_WARN("[SIPS] got disconnected event!");
                disconnectedEvent = std::move(evdata);
                disconnected = true;
                break;
            }
        }
    }

    // Handle SIP transport -> TLS
    decltype(txQueue_) tx_queue;
    {
        std::lock_guard<std::mutex> l(txMutex_);
        if (syncTx_) {
            tx_queue = std::move(txQueue_);
            txQueue_.clear();
        }
    }

    bool fatal = false;
    for (auto tdata : tx_queue) {
        pj_status_t status;
        if (!fatal) {
            const std::size_t size = tdata->buf.cur - tdata->buf.start;
            std::error_code ec;
            status = tls_->write(reinterpret_cast<const uint8_t*>(tdata->buf.start), size, ec);
            if (ec) {
                status = tls_status_from_err(ec.value());
                if (gnutls_error_is_fatal(ec.value())) {
                    JAMI_ERR("[TLS] fatal error during sending: %s", gnutls_strerror(ec.value()));
                    tls_->shutdown();
                    fatal = true;
                }
            }
        } else {
            status = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);
        }

        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(&trData_.base, tdata->op_key.token, status);
    }

    // Handle TLS -> SIP transport
    decltype(rxPending_) rx;
    {
        std::lock_guard<std::mutex> l(rxMtx_);
        rx = std::move(rxPending_);
        rxPending_.clear();
    }

    sip_utils::register_thread();
    for (auto it = rx.begin(); it != rx.end(); ++it) {
        auto& pck = *it;
        pj_pool_reset(rdata_.tp_info.pool);
        pj_gettimeofday(&rdata_.pkt_info.timestamp);
        rdata_.pkt_info.len = std::min(pck.size(), (size_t) PJSIP_MAX_PKT_LEN);
        std::copy_n(pck.data(), rdata_.pkt_info.len, rdata_.pkt_info.packet);
        auto eaten = pjsip_tpmgr_receive_packet(trData_.base.tpmgr, &rdata_);

        // Uncomplet parsing? (may be a partial sip packet received)
        if (eaten != (pj_ssize_t)pck.size()) {
            auto npck_it = std::next(it);
            if (npck_it != rx.end()) {
                // drop current packet, merge reminder with next one
                auto& npck = *npck_it;
                npck.insert(npck.begin(), pck.begin()+eaten, pck.end());
            } else {
                // erase eaten part, keep remainder
                pck.erase(pck.begin(), pck.begin()+eaten);
                {
                    std::lock_guard<std::mutex> l(rxMtx_);
                    rxPending_.splice(rxPending_.begin(), rx, it);
                }
                break;
            }
        }
    }

    // Time to deliver disconnected event if exists
    if (disconnected and state_cb) {
        JAMI_WARN("[SIPS] process disconnect event");
        (*state_cb)(&trData_.base, disconnectedEvent.state, &disconnectedEvent.state_info);
    }
}

void
SipsIceTransport::pushChangeStateEvent(ChangeStateEventData&& ev)
{
    std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
    stateChangeEvents_.emplace_back(std::move(ev));
    scheduler_.run([this]{ handleEvents(); });
}

// - DO NOT BLOCK - (Called in TlsSession thread)
void
SipsIceTransport::onTlsStateChange(UNUSED TlsSessionState state)
{
    if (state == TlsSessionState::ESTABLISHED)
        updateTransportState(PJSIP_TP_STATE_CONNECTED);
    else if (state == TlsSessionState::SHUTDOWN)
        updateTransportState(PJSIP_TP_STATE_DISCONNECTED);
}

// - DO NOT BLOCK - (Called in TlsSession thread)
void
SipsIceTransport::onRxData(std::vector<uint8_t>&& buf)
{
    std::lock_guard<std::mutex> l(rxMtx_);
    rxPending_.emplace_back(std::move(buf));
    scheduler_.run([this]{ handleEvents(); });
}

/* Update local & remote certificates info. This function should be
 * called after handshake or re-negotiation successfully completed.
 *
 * - DO NOT BLOCK - (Called in TlsSession thread)
 */
void
SipsIceTransport::onCertificatesUpdate(const gnutls_datum_t* local_raw,
                                       const gnutls_datum_t* remote_raw,
                                       unsigned int remote_count)
{
    // local certificate
    if (local_raw)
        certGetInfo(pool_.get(), &localCertInfo_, local_raw, 1);
    else
        std::memset(&localCertInfo_, 0, sizeof(pj_ssl_cert_info));

    // Remote certificates
    if (remote_raw)
        certGetInfo(pool_.get(), &remoteCertInfo_, remote_raw, remote_count);
    else
        std::memset(&remoteCertInfo_, 0, sizeof(pj_ssl_cert_info));
}

int
SipsIceTransport::verifyCertificate(gnutls_session_t session)
{
    // Support only x509 format
    if (gnutls_certificate_type_get(session) != GNUTLS_CRT_X509) {
        verifyStatus_ = PJ_SSL_CERT_EINVALID_FORMAT;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    // Store verification status
    unsigned int status = 0;
    auto ret = gnutls_certificate_verify_peers2(session, &status);
    if (ret < 0 or (status & GNUTLS_CERT_SIGNATURE_FAILURE) != 0) {
        verifyStatus_ = PJ_SSL_CERT_EUNTRUSTED;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    unsigned int cert_list_size = 0;
    auto cert_list = gnutls_certificate_get_peers(session, &cert_list_size);
    if (cert_list == nullptr) {
        verifyStatus_ = PJ_SSL_CERT_EISSUER_NOT_FOUND;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    if (certCheck_) {
        verifyStatus_ = certCheck_(status, cert_list, cert_list_size);
        if (verifyStatus_ != PJ_SUCCESS)
            return GNUTLS_E_CERTIFICATE_ERROR;
    }

    // notify GnuTLS to continue handshake normally
    return GNUTLS_E_SUCCESS;
}

void
SipsIceTransport::updateTransportState(pjsip_transport_state state)
{
    ChangeStateEventData ev;

    std::memset(&ev.state_info, 0, sizeof(ev.state_info));
    std::memset(&ev.tls_info, 0, sizeof(ev.tls_info));

    ev.state = state;
    bool connected = state == PJSIP_TP_STATE_CONNECTED;
    {
        std::lock_guard<std::mutex> lk {txMutex_};
        syncTx_ = connected;
    }
    getInfo(&ev.ssl_info, connected);
    if (connected)
        ev.state_info.status = ev.ssl_info.verify_status ? PJSIP_TLS_ECERTVERIF : PJ_SUCCESS;
    else
        ev.state_info.status = PJ_SUCCESS; // TODO: use last gnu error

    pushChangeStateEvent(std::move(ev));
}

void
SipsIceTransport::getInfo(pj_ssl_sock_info* info, bool established)
{
    std::memset(info, 0, sizeof(*info));

    info->established = established;
    if (PJSIP_TRANSPORT_IS_RELIABLE(&trData_.base))
        info->proto = PJSIP_SSL_DEFAULT_PROTO;
    else
        info->proto = PJ_SSL_SOCK_PROTO_DTLS1;

    pj_sockaddr_cp(&info->local_addr, local_.pjPtr());

    if (established) {
        // Cipher Suite Id
        std::array<uint8_t, 2> cs_id;
        if (auto cipher_name = tls_->currentCipherSuiteId(cs_id)) {
            info->cipher = static_cast<pj_ssl_cipher>((cs_id[0] << 8) | cs_id[1]);
            JAMI_DBG("[TLS] using cipher %s (0x%02X%02X)", cipher_name, cs_id[0], cs_id[1]);
        } else
            JAMI_ERR("[TLS] Can't find info on used cipher");

        info->local_cert_info = &localCertInfo_;
        info->remote_cert_info = &remoteCertInfo_;
        info->verify_status = verifyStatus_;

        pj_sockaddr_cp(&info->remote_addr, remote_.pjPtr());
    }

    // Last known GnuTLS error code
    info->last_native_err = GNUTLS_E_SUCCESS;
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
            //last_err_ = GNUTLS_E_MEMORY_ERROR;
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

pj_status_t
SipsIceTransport::send(pjsip_tx_data* tdata, const pj_sockaddr_t* rem_addr,
                       int addr_len, void* token,
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

    // Check in we are able to send it in synchronous way first
    const std::size_t size = tdata->buf.cur - tdata->buf.start;
    std::unique_lock<std::mutex> lk {txMutex_};
    if (syncTx_ and txQueue_.empty()) {
        std::error_code ec;
        tls_->write(reinterpret_cast<const uint8_t*>(tdata->buf.start), size, ec);
        lk.unlock();

        // Shutdown on fatal error, else ignore it
        if (ec and gnutls_error_is_fatal(ec.value())) {
            JAMI_ERR("[TLS] fatal error during sending: %s", gnutls_strerror(ec.value()));
            tls_->shutdown();
            return tls_status_from_err(ec.value());
        }

        return PJ_SUCCESS;
    }

    // Asynchronous sending
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;
    txQueue_.push_back(tdata);
    scheduler_.run([this]{ handleEvents(); });
    return PJ_EPENDING;
}

uint16_t
SipsIceTransport::getTlsSessionMtu()
{
    return tls_->maxPayload();
}

void
SipsIceTransport::eventLoop()
{
    while(!stopLoop_) {
        std::error_code err;
        if (tls_->waitForData(100, err)) {
            std::vector<uint8_t> pkt;
            pkt.resize(PJSIP_MAX_PKT_LEN);
            auto read = tls_->read(pkt.data(), PJSIP_MAX_PKT_LEN, err);
            if (read > 0) {
                pkt.resize(read);
                std::lock_guard<std::mutex> l(rxMtx_);
                rxPending_.emplace_back(std::move(pkt));
                scheduler_.run([this]{ handleEvents(); });
            }
        }
    }
}

}} // namespace jami::tls
