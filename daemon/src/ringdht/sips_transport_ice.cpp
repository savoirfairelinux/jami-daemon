/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
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

#include "sips_transport_ice.h"
#include "ice_transport.h"
#include "logger.h"

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/lock.h>

#include <algorithm>

namespace ring {

static constexpr int POOL_TP_INIT {512};
static constexpr int POOL_TP_INC {512};
static constexpr int TRANSPORT_INFO_LENGTH {64};
static constexpr int GNUTLS_LOG_LEVEL {9};

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

static void tls_print_logs(int level, const char* msg)
{
    RING_DBG("GnuTLS [%d]: %s", level, msg);
}


static pj_status_t tls_status_from_err(int err)
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

    /* Not thread safe */
    /*tls_last_error = err;
    if (ssock)
        ssock->last_err = err;*/
    return status;
}


pj_status_t
SipsIceTransport::tryHandshake()
{
    RING_DBG("SipsIceTransport::tryHandshake as %s", (is_server_ ? "server" : "client"));
    pj_status_t status;
    int ret = gnutls_handshake(session_);
    if (ret == GNUTLS_E_SUCCESS) {
        /* System are GO */
        RING_DBG("TlsConnectionState::tryHandshake : ESTABLISHED");
        state_ = TlsConnectionState::ESTABLISHED;
        status = PJ_SUCCESS;
    } else if (!gnutls_error_is_fatal(ret)) {
        /* Non fatal error, retry later (busy or again) */
        RING_DBG("TlsConnectionState::tryHandshake : EPENDING");
        status = PJ_EPENDING;
    } else {
        /* Fatal error invalidates session, no fallback */
        RING_DBG("TlsConnectionState::tryHandshake : EINVAL");
        status = PJ_EINVAL;
    }
    last_err = ret;
    return status;
}

int
SipsIceTransport::verifyCertificate()
{
    unsigned int status;
    int ret;

    RING_DBG("TlsConnectionState::verifyCertificate");
    return GNUTLS_E_SUCCESS;

    /* Support only x509 format */
    ret = gnutls_certificate_type_get(session_) != GNUTLS_CRT_X509;
    if (ret < 0) {
        //ssock->verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* Store verification status */
    ret = gnutls_certificate_verify_peers2(session_, &status);
    if (ret < 0) {
        //ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }
    //if (ssock->param.verify_peer) {
    if (status & GNUTLS_CERT_INVALID) {
        if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
            verify_status |= PJ_SSL_CERT_EISSUER_NOT_FOUND;
        else if (status & GNUTLS_CERT_EXPIRED ||
                 status & GNUTLS_CERT_NOT_ACTIVATED)
            verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
        else if (status & GNUTLS_CERT_SIGNER_NOT_CA ||
                 status & GNUTLS_CERT_INSECURE_ALGORITHM)
            verify_status |= PJ_SSL_CERT_EUNTRUSTED;
        else if (status & GNUTLS_CERT_UNEXPECTED_OWNER ||
                 status & GNUTLS_CERT_MISMATCH)
            verify_status |= PJ_SSL_CERT_EISSUER_MISMATCH;
        else if (status & GNUTLS_CERT_REVOKED)
            verify_status |= PJ_SSL_CERT_EREVOKED;
        else
            verify_status |= PJ_SSL_CERT_EUNKNOWN;

        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* When verification is not requested just return ok here, however
     * applications can still get the verification status. */
        gnutls_x509_crt_t cert;
        unsigned int cert_list_size;
        const gnutls_datum_t *cert_list;
        //int ret;

        ret = gnutls_x509_crt_init(&cert);
        if (ret < 0)
            goto out;

        cert_list = gnutls_certificate_get_peers(session_, &cert_list_size);
        if (cert_list == NULL) {
            ret = GNUTLS_E_NO_CERTIFICATE_FOUND;
            goto out;
        }

        /* TODO: verify whole chain perhaps? */
        ret = gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
        if (ret < 0)
            ret = gnutls_x509_crt_import(cert, &cert_list[0],
                                         GNUTLS_X509_FMT_PEM);
        if (ret < 0) {
            verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
            goto out;
        }
        /*ret = gnutls_x509_crt_check_hostname(cert, ssock->param.server_name.ptr);
        if (ret < 0)
            goto out;*/

        gnutls_x509_crt_deinit(cert);

        /* notify GnuTLS to continue handshake normally */
        return GNUTLS_E_SUCCESS;

out:
        last_err = ret;
        verify_status |= PJ_SSL_CERT_EUNKNOWN;
        return GNUTLS_E_CERTIFICATE_ERROR;
    //}

    return GNUTLS_E_SUCCESS;
}


SipsIceTransport::SipsIceTransport(pjsip_endpoint* endpt, pj_pool_t& /* pool */,
                                 const TlsParams& param,
                                 const std::shared_ptr<ring::IceTransport>& ice,
                                 int comp_id, std::function<int()> destroy_cb)
    : base()
    , pool_(nullptr, pj_pool_release)
    , rxPool_(nullptr, pj_pool_release)
    , param_(param)
    , rdata()
    , ice_(ice)
    , comp_id_(comp_id)
    , destroy_cb_(destroy_cb)
    , tlsThread_(
        []{
            RING_WARN("Starting GnuTLS thread");
            return true;},
        std::bind(&SipsIceTransport::onRecv, this),
        [this]{
            RING_WARN("Ending GnuTLS thread");
            pjsip_transport_add_ref(&base);
            state_ = TlsConnectionState::DISCONNECTED;
            close();
            pjsip_tp_state_callback state_cb = pjsip_tpmgr_get_state_cb(base.tpmgr);
            if (state_cb) {
                pjsip_transport_state_info state_info;
                pjsip_tls_state_info tls_info;

                /* Init transport state info */
                pj_bzero(&state_info, sizeof(state_info));
                pj_bzero(&tls_info, sizeof(tls_info));
                pj_ssl_sock_info ssl_info;
                getInfo(&ssl_info);
                tls_info.ssl_sock_info = &ssl_info;
                state_info.ext_info = &tls_info;
                state_info.status = PJ_SUCCESS;

                (*state_cb)(&base, PJSIP_TP_STATE_DISCONNECTED, &state_info);
            }
            if (base.is_shutdown || base.is_destroying) {
                pjsip_transport_dec_ref(&base);
                return;
            }
            pjsip_transport_shutdown(&base);
            /* Now, it is ok to destroy the transport. */
            pjsip_transport_dec_ref(&base);
        })
{
    if (not ice or not ice->isRunning())
        throw std::logic_error("ice transport must exist and negotiation completed");

    RING_DBG("Creating SipsIceTransport");

    pool_.reset(pjsip_endpt_create_pool(endpt, "SipsIceTransport.pool", POOL_TP_INIT, POOL_TP_INC));
    if (not pool_) {
        RING_ERR("Can't create PJSIP pool");
        throw std::bad_alloc();
    }
    auto pool = pool_.get();

    //param_.proto = PJ_SSL_SOCK_PROTO_DTLS1;

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "SipsIceTransport");
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool;

    rdata.tp_info.pool = pool;

    if (pj_atomic_create(pool, 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool, "SipsIceTransport.mutex", &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    is_server_ = not ice->isInitiator();
    local_ = ice->getLocalAddress(comp_id);
    remote_ = ice->getRemoteAddress(comp_id);
    RING_DBG("SipsIceTransport: remote is %s", remote_.toString(true).c_str());
    pj_sockaddr_cp(&base.key.rem_addr, remote_.pjPtr());
    base.key.type = PJSIP_TRANSPORT_UDP;//t_type;
    base.type_name = (char*)pjsip_transport_get_type_name((pjsip_transport_type_e)base.key.type);
    base.flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)base.key.type);
    base.info = (char*) pj_pool_alloc(pool, TRANSPORT_INFO_LENGTH);

    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_ansi_snprintf(base.info, TRANSPORT_INFO_LENGTH, "%s to %s",
                     base.type_name,
                     pj_sockaddr_print(remote_.pjPtr(), print_addr,
                                       sizeof(print_addr), 3));
    base.addr_len = remote_.getLength();
    base.dir = PJSIP_TP_DIR_NONE;//is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;
    base.data = nullptr;

    /* Set initial local address */
    auto local = ice->getDefaultLocalAddress();
    pj_sockaddr_cp(&base.local_addr, local.pjPtr());

    sockaddr_to_host_port(pool, &base.local_name, &base.local_addr);
    sockaddr_to_host_port(pool, &base.remote_name, remote_.pjPtr());

    base.send_msg = [](pjsip_transport *t,
                       pjsip_tx_data *tdata,
                       const pj_sockaddr_t *rem_addr, int addr_len,
                       void *token, pjsip_transport_callback callback) -> pj_status_t {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport *t){
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->shutdown();
    };
    base.destroy = [](pjsip_transport *t){
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->destroy();
    };

    /* Init rdata */
    rxPool_.reset(pjsip_endpt_create_pool(base.endpt,
                                          "SipsIceTransport.rtd%p",
                                          PJSIP_POOL_RDATA_LEN,
                                          PJSIP_POOL_RDATA_INC));
    if (not rxPool_) {
        RING_ERR("Can't create PJSIP rx pool");
        throw std::bad_alloc();
    }
    auto rx_pool = rxPool_.get();

    rdata.tp_info.pool = rx_pool;
    rdata.tp_info.transport = &base;
    rdata.tp_info.tp_data = this;
    rdata.tp_info.op_key.rdata = &rdata;
    pj_ioqueue_op_key_init(&rdata.tp_info.op_key.op_key, sizeof(pj_ioqueue_op_key_t));
    rdata.pkt_info.src_addr = base.key.rem_addr;
    rdata.pkt_info.src_addr_len = sizeof(rdata.pkt_info.src_addr);
    auto rem_addr = &base.key.rem_addr;
    pj_sockaddr_print(rem_addr, rdata.pkt_info.src_name, sizeof(rdata.pkt_info.src_name), 0);
    rdata.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);
    rdata.pkt_info.len  = 0;
    rdata.pkt_info.zero = 0;

    pj_bzero(&local_cert_info_, sizeof(pj_ssl_cert_info));
    pj_bzero(&remote_cert_info_, sizeof(pj_ssl_cert_info));

    /* Register error subsystem */
    /*pj_status_t status = pj_register_strerror(PJ_ERRNO_START_USER +
                                              PJ_ERRNO_SPACE_SIZE * 6,
                                              PJ_ERRNO_SPACE_SIZE,
                                              &tls_strerror);
    pj_assert(status == PJ_SUCCESS);*/

    /* Init GnuTLS library */
    int ret = gnutls_global_init();
    if (ret < 0)
        throw std::runtime_error("Can't initialise GNUTLS : " + std::string(gnutls_strerror(ret)));

    gnutls_global_set_log_level(GNUTLS_LOG_LEVEL);
    gnutls_global_set_log_function(tls_print_logs);

    /* Init available ciphers */
    for (unsigned i = 0; ; i++) {
        std::array<uint8_t, 2> id;
        const char *suite = gnutls_cipher_suite_info(i, id.data(), nullptr, nullptr, nullptr, nullptr);
        if (!suite)
            break;
        cyphers_.emplace_back(SSLCipher{((id[0] << 8) | id[1]), suite});
    }
    RING_ERR("Found %d possible ciphers", cyphers_.size());

    gnutls_key_generate(&cookie_key_, GNUTLS_COOKIE_KEY_SIZE);
    gnutls_priority_init(&priority_cache, "SECURE192:-VERS-TLS-ALL:+VERS-DTLS1.0:%SERVER_PRECEDENCE", nullptr);

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");
    is_registered_ = true;

    if (is_server_)
        state_ = TlsConnectionState::COOKIE;
    else
        startTls();
    tlsThread_.start();
}

SipsIceTransport::~SipsIceTransport()
{
    RING_DBG("~SipsIceTransport");
    reset();
    tlsThread_.join();

    /* Free GnuTLS library */
    gnutls_global_deinit();

    pj_lock_destroy(base.lock);
    pj_atomic_destroy(base.ref_cnt);
}

pj_status_t
SipsIceTransport::startTls()
{
    RING_DBG("SipsIceTransport::startTls as %s", (is_server_ ? "server" : "client"));
    gnutls_init(&session_, (is_server_ ? GNUTLS_SERVER : GNUTLS_CLIENT) | GNUTLS_DATAGRAM);
    gnutls_session_set_ptr(session_, this);
    gnutls_transport_set_ptr(session_, this);

    gnutls_priority_set(session_, priority_cache);

    /* Allocate credentials for handshaking and transmission */
    int ret = gnutls_certificate_allocate_credentials(&xcred_);
    if (ret < 0) {
        RING_ERR("Can't allocate credentials");
        throw std::bad_alloc();
    }

    gnutls_certificate_set_verify_function(xcred_, [](gnutls_session_t session) {
        auto this_ = reinterpret_cast<SipsIceTransport*>(gnutls_session_get_ptr(session));
        return this_->verifyCertificate();
    });

    gnutls_certificate_set_dh_params(xcred_, dh_params_);
    if (not param_.ca_list.empty()) {
        /* Load CA if one is specified. */
        ret = gnutls_certificate_set_x509_trust_file(xcred_, param_.ca_list.c_str(), GNUTLS_X509_FMT_PEM);
        if (ret < 0)
            ret = gnutls_certificate_set_x509_trust_file(xcred_, param_.ca_list.c_str(), GNUTLS_X509_FMT_DER);
        if (ret < 0)
            throw std::runtime_error("Can't load CA.");
        RING_WARN("Loaded %s", param_.ca_list.c_str());

        if (param_.id.first) {
            /* Load certificate, key and pass */
            ret = gnutls_certificate_set_x509_key(xcred_,
                                                  &param_.id.second->cert, 1,
                                                  param_.id.first->x509_key);
            if (ret < 0)
                throw std::runtime_error("Can't load certificate : " + std::string(gnutls_strerror(ret)));
            RING_WARN("Loaded certificate");
        }
    }

    /* Finally set credentials for this session */
    ret = gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, xcred_);

    if (is_server_) {
        /* Require client certificate and valid cookie */
        gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUIRE);
        gnutls_dtls_prestate_set(session_, &prestate_);
    }
    int mtu = 3200;
    gnutls_dtls_set_mtu(session_, mtu);

    gnutls_transport_set_push_function(session_, [](gnutls_transport_ptr_t t, const void* d , size_t s) -> ssize_t {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->tlsSend(d, s);
    });
    gnutls_transport_set_pull_function(session_, [](gnutls_transport_ptr_t t, void* d, size_t s) -> ssize_t {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->tlsRecv(d, s);
    });
    gnutls_transport_set_pull_timeout_function(session_, [](gnutls_transport_ptr_t t, unsigned ms) -> int {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->waitForTlsData(ms);
    });

    // start handshake
    state_ = TlsConnectionState::HANDSHAKING;
}

void
SipsIceTransport::certGetCn(const pj_str_t *gen_name, pj_str_t *cn)
{
    pj_str_t CN_sign = {"CN=", 3};
    char *p, *q;

    pj_bzero(cn, sizeof(cn));

    p = pj_strstr(gen_name, &CN_sign);
    if (!p)
        return;

    p += 3; /* shift pointer to value part */
    pj_strset(cn, p, gen_name->slen - (p - gen_name->ptr));
    q = pj_strchr(cn, ',');
    if (q)
        cn->slen = q - p;
}

/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer and the serial number. */
void
SipsIceTransport::certGetInfo(pj_pool_t *pool, pj_ssl_cert_info *ci, gnutls_x509_crt_t cert)
{
    RING_DBG("SipsIceTransport::certGetInfo");
    char buf[512] = { 0 };
    size_t bufsize = sizeof(buf);
    pj_uint8_t serial_no[64] = { 0 }; /* should be >= sizeof(ci->serial_no) */
    size_t serialsize = sizeof(serial_no);
    size_t len = sizeof(buf);
    int i, ret, seq = 0;
    pj_ssl_cert_name_type type;

    pj_assert(pool && ci && cert);

    /* Get issuer */
    gnutls_x509_crt_get_issuer_dn(cert, buf, &bufsize);

    /* Get serial no */
    gnutls_x509_crt_get_serial(cert, serial_no, &serialsize);

    /* Check if the contents need to be updated */
    pj_bool_t update_needed = pj_strcmp2(&ci->issuer.info, buf) || pj_memcmp(ci->serial_no, serial_no, serialsize);
    if (!update_needed)
        return;

    /* Update cert info */

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
    ci->version = gnutls_x509_crt_get_version(cert);

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
    certGetCn(&ci->issuer.info, &ci->issuer.cn);

    /* Serial number */
    pj_memcpy(ci->serial_no, serial_no, sizeof(ci->serial_no));

    /* Subject */
    bufsize = sizeof(buf);
    gnutls_x509_crt_get_dn(cert, buf, &bufsize);
    pj_strdup2(pool, &ci->subject.info, buf);
    certGetCn(&ci->subject.info, &ci->subject.cn);

    /* Validity */
    ci->validity.end.sec = gnutls_x509_crt_get_expiration_time(cert);
    ci->validity.start.sec = gnutls_x509_crt_get_activation_time(cert);
    ci->validity.gmt = 0;

    /* Subject Alternative Name extension */
    if (ci->version >= 3) {
        char out[256] = { 0 };
        /* Get the number of all alternate names so that we can allocate
         * the correct number of bytes in subj_alt_name */
        while (gnutls_x509_crt_get_subject_alt_name(cert, seq, out, &len, NULL) != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
            seq++;

        ci->subj_alt_name.entry = (decltype(ci->subj_alt_name.entry)) pj_pool_calloc(pool, seq, sizeof(*ci->subj_alt_name.entry));
        if (!ci->subj_alt_name.entry) {
            last_err = GNUTLS_E_MEMORY_ERROR;
            return;
        }

        /* Now populate the alternative names */
        for (i = 0; i < seq; i++) {
            len = sizeof(out) - 1;
            ret = gnutls_x509_crt_get_subject_alt_name(cert, i, out, &len, NULL);
            switch (ret) {
            case GNUTLS_SAN_IPADDRESS:
                type = PJ_SSL_CERT_NAME_IP;
                pj_inet_ntop2(len == sizeof(pj_in6_addr) ? pj_AF_INET6()
                                                         : pj_AF_INET(),
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


/* Update local & remote certificates info. This function should be
 * called after handshake or renegotiation successfully completed. */
void
SipsIceTransport::certUpdate()
{
    RING_DBG("SipsIceTransport::certUpdate");
    gnutls_x509_crt_t cert = NULL;
    const gnutls_datum_t *us;
    const gnutls_datum_t *certs;
    unsigned int certslen = 0;
    int ret = GNUTLS_CERT_INVALID;


    //pj_assert(ssock->connection_state == TLS_STATE_ESTABLISHED);

    /* Get active local certificate */
    us = gnutls_certificate_get_ours(session_);
    if (!us)
        goto us_out;

    ret = gnutls_x509_crt_init(&cert);
    if (ret < 0)
        goto us_out;
    ret = gnutls_x509_crt_import(cert, us, GNUTLS_X509_FMT_DER);
    if (ret < 0)
        ret = gnutls_x509_crt_import(cert, us, GNUTLS_X509_FMT_PEM);
    if (ret < 0)
        goto us_out;

    certGetInfo(pool_.get(), &local_cert_info_, cert);

us_out:
    last_err = ret;
    if (cert)
        gnutls_x509_crt_deinit(cert);
    else
        pj_bzero(&local_cert_info_, sizeof(pj_ssl_cert_info));

    cert = NULL;

    /* Get active remote certificate */
    certs = gnutls_certificate_get_peers(session_, &certslen);
    if (certs == NULL || certslen == 0)
        goto peer_out;

    ret = gnutls_x509_crt_init(&cert);
    if (ret < 0)
        goto peer_out;

    ret = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_PEM);
    if (ret < 0)
        ret = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_DER);
    if (ret < 0)
        goto peer_out;

    certGetInfo(pool_.get(), &remote_cert_info_, cert);

peer_out:
    last_err = ret;
    if (cert)
        gnutls_x509_crt_deinit(cert);
    else
        pj_bzero(&remote_cert_info_, sizeof(pj_ssl_cert_info));
}

pj_status_t
SipsIceTransport::getInfo(pj_ssl_sock_info *info)
{
    RING_DBG("SipsIceTransport::getInfo");
    pj_bzero(info, sizeof(*info));

    /* Established flag */
    info->established = (state_ == TlsConnectionState::ESTABLISHED);

    /* Protocol */
    info->proto = PJ_SSL_SOCK_PROTO_DTLS1;//param_.proto;

    /* Local address */
    pj_sockaddr_cp(&info->local_addr, local_.pjPtr());

    if (info->established) {
        int i;
        gnutls_cipher_algorithm_t lookup;
        gnutls_cipher_algorithm_t cipher;

        /* Current cipher */
        cipher = gnutls_cipher_get(session_);
        for (i = 0; ; i++) {
            unsigned char id[2];
            const char *suite = gnutls_cipher_suite_info(i, (unsigned char *)id, NULL, &lookup, NULL, NULL);
            if (suite) {
                if (lookup == cipher) {
                    info->cipher = (pj_ssl_cipher) ((id[0] << 8) | id[1]);
                    break;
                }
            } else
                break;
        }

        /* Remote address */
        pj_sockaddr_cp(&info->remote_addr, remote_.pjPtr());

        /* Certificates info */
        info->local_cert_info = &local_cert_info_;
        info->remote_cert_info = &remote_cert_info_;

        /* Verification status */
        info->verify_status = verify_status;
    }

    /* Last known GnuTLS error code */
    info->last_native_err = last_err;

    return PJ_SUCCESS;
}


/* When handshake completed:
 * - notify application
 * - if handshake failed, reset SSL state
 * - return PJ_FALSE when SSL socket instance is destroyed by application. */
pj_bool_t
SipsIceTransport::onHandshakeComplete(pj_status_t status)
{
    RING_DBG("SipsIceTransport::onHandshakeComplete %d", status);
    pj_bool_t ret = PJ_TRUE;

    /* Cancel handshake timer */
    /*if (ssock->timer.id == TIMER_HANDSHAKE_TIMEOUT) {
        pj_timer_heap_cancel(param_.timer_heap, &ssock->timer);
        ssock->timer.id = TIMER_NONE;
    }*/

    /* Update certificates info on successful handshake */
    if (status == PJ_SUCCESS)
        certUpdate();

    pj_ssl_sock_info ssl_info;
    getInfo(&ssl_info);

    pjsip_tp_state_callback state_cb = pjsip_tpmgr_get_state_cb(base.tpmgr);
    pjsip_transport_state_info state_info;
    pjsip_tls_state_info tls_info;

    /* Init transport state info */
    pj_bzero(&state_info, sizeof(state_info));
    pj_bzero(&tls_info, sizeof(tls_info));
    tls_info.ssl_sock_info = &ssl_info;
    state_info.ext_info = &tls_info;
    state_info.status = ssl_info.verify_status ? PJSIP_TLS_ECERTVERIF : PJ_SUCCESS;

    /* Accepting */
    if (is_server_) {
        if (status != PJ_SUCCESS) {
            /* Handshake failed in accepting, destroy our self silently. */

            char errmsg[PJ_ERR_MSG_SIZE];
            char buf[PJ_INET6_ADDRSTRLEN + 10];

            pj_strerror(status, errmsg, sizeof(errmsg));
            PJ_LOG(3, (pool_.get()->obj_name,
                       "Handshake failed in accepting %s: %s",
                       pj_sockaddr_print(remote_.pjPtr(), buf, sizeof(buf), 3),
                       errmsg));

            /* Workaround for ticket #985 */
#if (defined(PJ_WIN32) && PJ_WIN32 != 0) || (defined(PJ_WIN64) && PJ_WIN64 != 0)
            if (param_.timer_heap) {
                pj_time_val interval = {0, DELAYED_CLOSE_TIMEOUT};

                reset();

                timer.id = TIMER_CLOSE;
                pj_time_val_normalize(&interval);
                if (pj_timer_heap_schedule(param_.timer_heap, &timer, &interval) != 0) {
                    timer.id = TIMER_NONE;
                    close();
                }
            } else
#endif /* PJ_WIN32 */
            {
                close();
            }

            return PJ_FALSE;
        }
        /* Notify application the newly accepted SSL socket */
        /*if (param_.cb.on_accept_complete)
            ret = (*param_.cb.on_accept_complete)
                      (ssock->parent, ssock, (pj_sockaddr_t*)&ssock->rem_addr,
                       pj_sockaddr_get_len((pj_sockaddr_t*)&ssock->rem_addr));*/
        if (state_cb)
            (*state_cb)(&base, PJSIP_TP_STATE_CONNECTED, &state_info);

    } else { /* Connecting */
        /* On failure, reset SSL socket state first, as app may try to
         * reconnect in the callback. */
        if (status != PJ_SUCCESS) {
            /* Server disconnected us, possibly due to negotiation failure */
            reset();
        }
        if (state_cb)
            (*state_cb)(&base, status ? PJSIP_TP_STATE_DISCONNECTED : PJSIP_TP_STATE_CONNECTED, &state_info);
    }

    return ret;
}

void
SipsIceTransport::generateDhParams()
{
    RING_DBG("SipsIceTransport::generateDhParams");
    unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_HIGH);
    /* Generate Diffie-Hellman parameters - for use with DHE
     * kx algorithms. When short bit length is used, it might
     * be wise to regenerate parameters often.
     */
    gnutls_dh_params_init(&dh_params_);
    gnutls_dh_params_generate2(dh_params_, bits);
}

void
SipsIceTransport::loop()
{
    /*while (ice_->getNextPacketSize(comp_id_) > 0)
        onRecv();
    if (state_ == TlsConnectionState::HANDSHAKING) {
        int status = tryHandshake();
        if (status != PJ_EPENDING)
            onHandshakeComplete(status);
    }*/
}

IpAddr
SipsIceTransport::getLocalAddress() const
{
    return ice_->getLocalAddress(comp_id_);
}

ssize_t
SipsIceTransport::tlsSend(const void* d , size_t s)
{
    RING_DBG("SipsIceTransport::tlsSend %lu", s);
    return ice_->send(comp_id_, (const uint8_t*)d, s);
}

ssize_t
SipsIceTransport::tlsRecv(void* d , size_t s)
{
    if (tlsInputBuff_.empty()) {
        RING_WARN("SipsIceTransport::tlsRecv %lu - EMPTY", s);
        errno = EAGAIN;
        return -1;
    }
    RING_DBG("SipsIceTransport::tlsRecv %lu", s);
    const auto n = std::min(tlsInputBuff_.size(), s);
    std::copy_n(tlsInputBuff_.begin(), n, (uint8_t*)d);
    std::move(tlsInputBuff_.begin()+n, tlsInputBuff_.end(), tlsInputBuff_.begin());
    tlsInputBuff_.resize(tlsInputBuff_.size() - n);
    return n;
}

int
SipsIceTransport::waitForTlsData(unsigned ms)
{
    RING_DBG("SipsIceTransport::waitForTlsData %u", ms);
    if (not tlsInputBuff_.empty())
        return tlsInputBuff_.size();
    return ice_->waitForData(comp_id_, ms);
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
    PJ_ASSERT_RETURN(rem_addr && (addr_len==sizeof(pj_sockaddr_in) ||
                                  addr_len==sizeof(pj_sockaddr_in6)), PJ_EINVAL);

    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;

    pj_status_t status;
    pj_ssize_t total_written = 0;

    pj_lock_acquire(base.lock);

    outputBuff_.emplace_back(DelayedTxData{&tdata->op_key, {}});
    auto& dtd = outputBuff_.back();
    if (tdata->msg && tdata->msg->type == PJSIP_REQUEST_MSG) {
        pj_gettickcount(&dtd.timeout);
        dtd.timeout.msec += pjsip_cfg()->tsx.td;
        pj_time_val_normalize(&dtd.timeout);
        RING_DBG("SipsIceTransport::send adding pending tx data (tot %lu)", outputBuff_.size());
    }

    pj_lock_release(base.lock);

    return flushOutputBuff();
}

pj_status_t
SipsIceTransport::flushOutputBuff()
{
    if (state_ != TlsConnectionState::ESTABLISHED)
        return PJ_EPENDING;

    pj_lock_acquire(base.lock);

    ssize_t status = PJ_SUCCESS;
    while (not outputBuff_.empty()) {
        auto& f = outputBuff_.front();
        status = trySend(f.tdata_op_key);
        pj_lock_release(base.lock);
        if (f.tdata_op_key->callback) {
            RING_DBG("Calling transaction callback");
            (*f.tdata_op_key->callback)(&base, f.tdata_op_key->token, status);
        }
        if (f.tdata_op_key->callback) {
            RING_DBG("Calling tx callback");
            (*f.tdata_op_key->tdata->cb)(f.tdata_op_key->token, f.tdata_op_key->tdata, status);
        }
        pj_lock_acquire(base.lock);
        if (status == PJ_SUCCESS)
            outputBuff_.pop_front();
        else
            break;
    }

    pj_lock_release(base.lock);
    return status > 0 ? PJ_SUCCESS : status;
}

ssize_t
SipsIceTransport::trySend(pjsip_tx_data_op_key *pck)
{
    if (state_ != TlsConnectionState::ESTABLISHED)
        return PJ_EPENDING;

    const size_t max_tx_sz = gnutls_dtls_get_data_mtu(session_);

    pjsip_tx_data *tdata = pck->tdata;
    size_t size = tdata->buf.cur - tdata->buf.start;
    RING_DBG("SipsIceTransport::trySend %p %lu", tdata, size);
    size_t total_written = 0;
    while (total_written < size) {
        /* Ask GnuTLS to encrypt our plaintext now. GnuTLS will use the push
         * callback to actually write the encrypted bytes into our output circular
         * buffer. GnuTLS may refuse to "send" everything at once, but since we are
         * not really sending now, we will just call it again now until it succeeds
         * (or fails in a fatal way). */
        auto tx_size = std::min(max_tx_sz, size - total_written);
        int nwritten = gnutls_record_send(session_, tdata->buf.start + total_written, tx_size);
        RING_DBG("gnutls_record_send %d / %lu (max %lu)", nwritten, size, max_tx_sz);

        if (nwritten > 0) {
            /* Good, some data was encrypted and written */
            total_written += nwritten;
        } else {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            RING_DBG("gnutls_record_send : %s", gnutls_strerror(nwritten));
            return tls_status_from_err(nwritten);
        }
    }

    return total_written;
}

void
SipsIceTransport::onRecv()
{
    if (not ice_->isRunning()) {
        reset();
        return;
    }

    auto next_size = ice_->getNextPacketSize(comp_id_);

    if (state_ == TlsConnectionState::COOKIE && next_size == 0)
        next_size = ice_->waitForData(comp_id_, 2500);

    auto inputBuffEnd = tlsInputBuff_.size();
    if (next_size > 0) {
        tlsInputBuff_.resize(inputBuffEnd + next_size);
        auto len = ice_->recv(comp_id_, tlsInputBuff_.data()+inputBuffEnd, next_size);
        tlsInputBuff_.resize(inputBuffEnd + len);
    }

    int ret;
    if (state_ == TlsConnectionState::COOKIE) {
        if (next_size == 0)
            return;
        memset(&prestate_, 0, sizeof(prestate_));
        ret = gnutls_dtls_cookie_verify(&cookie_key_, &base.key.rem_addr, base.addr_len, tlsInputBuff_.data(), tlsInputBuff_.size(), &prestate_);
        if (ret < 0) {
            RING_DBG("gnutls_dtls_cookie_send");
            gnutls_dtls_cookie_send(&cookie_key_, &base.key.rem_addr, base.addr_len, &prestate_, this, [](gnutls_transport_ptr_t t, const void* d , size_t s) -> ssize_t {
                auto this_ = reinterpret_cast<SipsIceTransport*>(t);
                return this_->tlsSend(d, s);
            });
            tlsInputBuff_.resize(inputBuffEnd);
        } else {
            startTls();
        }
    }
    if (state_ == TlsConnectionState::HANDSHAKING) {
        int status = tryHandshake();
        if (status != PJ_EPENDING)
            ret = onHandshakeComplete(status);
    }
    if (state_ == TlsConnectionState::ESTABLISHED) {
        RING_WARN("state_ == TlsConnectionState::ESTABLISHED");
        ice_->waitForData(comp_id_, 2500);
        while (true) {
            int decrypted_size = gnutls_record_recv(session_,
                    (uint8_t*)rdata.pkt_info.packet + rdata.pkt_info.len,
                    sizeof(rdata.pkt_info.packet)-rdata.pkt_info.len);
            rdata.pkt_info.len += decrypted_size;
            RING_WARN("gnutls_record_recv : %d", decrypted_size);

            if (decrypted_size > 0/* || transport error */) {
                pj_bool_t ret;

                rdata.pkt_info.zero = 0;
                pj_gettimeofday(&rdata.pkt_info.timestamp);
                auto eaten = pjsip_tpmgr_receive_packet(base.tpmgr, &rdata);
                RING_WARN("pjsip_tpmgr_receive_packet : %d", eaten);
                auto rem = rdata.pkt_info.len - eaten;
                if (rem > 0 && rem != rdata.pkt_info.len) {
                    std::move(rdata.pkt_info.packet + eaten,
                                rdata.pkt_info.packet + eaten + rem,
                                rdata.pkt_info.packet);
                }
                rdata.pkt_info.len = rem;
            } else if (decrypted_size == 0) {
                /* Nothing more to read */
                reset();
                break;//  PJ_TRUE;
            } else if (decrypted_size == GNUTLS_E_AGAIN || decrypted_size == GNUTLS_E_INTERRUPTED) {
                break;//  PJ_TRUE;
            } else if (decrypted_size == GNUTLS_E_REHANDSHAKE) {
                /* Seems like we are renegotiating */
                pj_status_t try_handshake_status = tryHandshake();

                /* Not pending is either success or failed */
                if (try_handshake_status != PJ_EPENDING) {
                    if (!onHandshakeComplete(try_handshake_status)) {
                        break;// PJ_FALSE;
                    }
                }

                if (try_handshake_status != PJ_SUCCESS && try_handshake_status != PJ_EPENDING) {
                    break;// PJ_FALSE;
                }
            } else if (!gnutls_error_is_fatal(decrypted_size)) {
                /* non-fatal error, let's just continue */
            } else {
                reset();
                break;// PJ_FALSE;
            }
        }

        flushOutputBuff();
    }
}

void
SipsIceTransport::close()
{
    RING_WARN("SipsIceTransport::close");
    if (session_) {
        gnutls_bye(session_, GNUTLS_SHUT_RDWR);
        gnutls_deinit(session_);
        session_ = nullptr;
    }

    if (xcred_) {
        gnutls_certificate_free_credentials(xcred_);
        xcred_ = nullptr;
    }

    /* Free GnuTLS library */
    gnutls_global_deinit();
}

void
SipsIceTransport::reset()
{
    RING_WARN("SipsIceTransport::reset");
    tlsThread_.stop();
}

pj_status_t
SipsIceTransport::shutdown()
{
    RING_WARN("SIPS transport ICE: shutdown");
}

pj_status_t
SipsIceTransport::destroy()
{
    if (not is_registered_)
        return PJ_SUCCESS;
    RING_WARN("SIPS transport ICE: destroy");
    is_registered_ = false;
    return destroy_cb_ ? destroy_cb_() : PJ_SUCCESS;
}


} // namespace ring
