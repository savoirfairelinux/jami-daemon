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

#include "sips_transport_ice.h"
#include "ice_transport.h"
#include "manager.h"
#include "logger.h"

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
static constexpr int DTLS_MTU {6400};

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

SipsIceTransport::SipsIceTransport(pjsip_endpoint* endpt,
                                   const TlsParams& param,
                                   const std::shared_ptr<ring::IceTransport>& ice,
                                   int comp_id)
    : pool_  {nullptr, pj_pool_release}
    , rxPool_ (nullptr, pj_pool_release)
    , trData_ ()
    , ice_ (ice)
    , comp_id_ (comp_id)
    , param_ (param)
    , tlsThread_(
        std::bind(&SipsIceTransport::setup, this),
        std::bind(&SipsIceTransport::loop, this),
        std::bind(&SipsIceTransport::clean, this))
    , xcred_ {nullptr, gnutls_certificate_free_credentials}
{
    RING_DBG("SipIceTransport@%p {tr=%p}", this, &trData_.base);

    trData_.self = this;

    if (not ice or not ice->isRunning())
        throw std::logic_error("ICE transport must exist and negotiation completed");

    pool_.reset(pjsip_endpt_create_pool(endpt, "SipsIceTransport.pool",
                                        POOL_TP_INIT, POOL_TP_INC));
    if (not pool_) {
        RING_ERR("Can't create PJSIP pool");
        throw std::bad_alloc();
    }
    auto pool = pool_.get();

    auto& base = trData_.base;
    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "SipsIceTransport");
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool;

    if (pj_atomic_create(pool, 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool, "SipsIceTransport.mutex",
                                       &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    is_server_ = not ice->isInitiator();
    local_ = ice->getLocalAddress(comp_id);
    remote_ = ice->getRemoteAddress(comp_id);
    pj_sockaddr_cp(&base.key.rem_addr, remote_.pjPtr());
    base.key.type = PJSIP_TRANSPORT_TLS;
    base.type_name = (char*)pjsip_transport_get_type_name((pjsip_transport_type_e)base.key.type);
    base.flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)base.key.type);
    base.info = (char*) pj_pool_alloc(pool, TRANSPORT_INFO_LENGTH);

    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_ansi_snprintf(base.info, TRANSPORT_INFO_LENGTH, "%s to %s",
                     base.type_name,
                     pj_sockaddr_print(remote_.pjPtr(), print_addr,
                                       sizeof(print_addr), 3));
    base.addr_len = remote_.getLength();
    base.dir = PJSIP_TP_DIR_NONE; ///is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;
    base.data = nullptr;

    /* Set initial local address */
    auto local = ice->getDefaultLocalAddress();
    pj_sockaddr_cp(&base.local_addr, local.pjPtr());

    sockaddr_to_host_port(pool, &base.local_name, &base.local_addr);
    sockaddr_to_host_port(pool, &base.remote_name, remote_.pjPtr());

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
        this_->shutdown();
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_WARN("SipsIceTransport@%p: destroy", this_);
        delete this_;
        return PJ_SUCCESS;
    };

    /* Init rdata_ */
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));
    rxPool_.reset(pjsip_endpt_create_pool(base.endpt,
                                          "SipsIceTransport.rxPool",
                                          PJSIP_POOL_RDATA_LEN,
                                          PJSIP_POOL_RDATA_LEN));
    if (not rxPool_) {
        RING_ERR("Can't create PJSIP rx pool");
        throw std::bad_alloc();
    }
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

    /* Register error subsystem */
    /*pj_status_t status = pj_register_strerror(PJ_ERRNO_START_USER +
                                              PJ_ERRNO_SPACE_SIZE * 6,
                                              PJ_ERRNO_SPACE_SIZE,
                                              &tls_strerror);
    pj_assert(status == PJ_SUCCESS);*/

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");

    gnutls_priority_init(&priority_cache,
                         "SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:%SERVER_PRECEDENCE",
                         nullptr);

    tlsThread_.start();
    Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
}

SipsIceTransport::~SipsIceTransport()
{
    RING_DBG("~SipsIceTransport");
    auto event = state_ == TlsConnectionState::ESTABLISHED;
    shutdown();
    tlsThread_.join();
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    handleEvents(); // process latest incoming packets

    pjsip_transport_add_ref(getTransportBase());
    auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr);
    if (state_cb && event) {
        pjsip_transport_state_info state_info;
        pjsip_tls_state_info tls_info;

        /* Init transport state info */
        std::memset(&state_info, 0, sizeof(state_info));
        std::memset(&tls_info, 0, sizeof(tls_info));
        pj_ssl_sock_info ssl_info;
        getInfo(&ssl_info);
        tls_info.ssl_sock_info = &ssl_info;
        state_info.ext_info = &tls_info;
        state_info.status = PJ_SUCCESS;

        (*state_cb)(getTransportBase(), PJSIP_TP_STATE_DISCONNECTED, &state_info);
    }

    if (not trData_.base.is_shutdown and not trData_.base.is_destroying)
        pjsip_transport_shutdown(getTransportBase());

    pjsip_transport_dec_ref(getTransportBase());

    pj_lock_destroy(trData_.base.lock);
    pj_atomic_destroy(trData_.base.ref_cnt);
}

pj_status_t
SipsIceTransport::startTlsSession()
{
    RING_DBG("SipsIceTransport::startTlsSession as %s",
             (is_server_ ? "server" : "client"));
    int ret;

    ret = gnutls_init(&session_, (is_server_ ? GNUTLS_SERVER : GNUTLS_CLIENT) | GNUTLS_DATAGRAM);
    if (ret != GNUTLS_E_SUCCESS) {
        shutdown();
        return tls_status_from_err(ret);
    }

    gnutls_session_set_ptr(session_, this);
    gnutls_transport_set_ptr(session_, this);

    gnutls_priority_set(session_, priority_cache);

    /* Allocate credentials for handshaking and transmission */
    gnutls_certificate_credentials_t certCred;
    ret = gnutls_certificate_allocate_credentials(&certCred);
    if (ret < 0 or not certCred) {
        RING_ERR("Can't allocate credentials");
        shutdown();
        return PJ_ENOMEM;
    }

    xcred_.reset(certCred);

    if (is_server_) {
        auto& dh_params = param_.dh_params.get();
        if (dh_params)
            gnutls_certificate_set_dh_params(certCred, dh_params.get());
        else
            RING_ERR("DH params unavaliable");
    }

    gnutls_certificate_set_verify_function(certCred, [](gnutls_session_t session) {
        auto this_ = reinterpret_cast<SipsIceTransport*>(gnutls_session_get_ptr(session));
        return this_->verifyCertificate();
    });

    if (not param_.ca_list.empty()) {
        /* Load CA if one is specified. */
        ret = gnutls_certificate_set_x509_trust_file(certCred,
                                                     param_.ca_list.c_str(),
                                                     GNUTLS_X509_FMT_PEM);
        if (ret < 0)
            ret = gnutls_certificate_set_x509_trust_file(certCred,
                                                         param_.ca_list.c_str(),
                                                         GNUTLS_X509_FMT_DER);
        if (ret < 0)
            throw std::runtime_error("Can't load CA.");
        RING_WARN("Loaded %s", param_.ca_list.c_str());
    }
    if (param_.id.first) {
        /* Load certificate, key and pass */
        ret = gnutls_certificate_set_x509_key(certCred,
                                              &param_.id.second->cert, 1,
                                              param_.id.first->x509_key);
        if (ret < 0)
            throw std::runtime_error("Can't load certificate : "
                                     + std::string(gnutls_strerror(ret)));
    }

    /* Finally set credentials for this session */
    ret = gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, certCred);
    if (ret != GNUTLS_E_SUCCESS) {
        shutdown();
        return tls_status_from_err(ret);
    }

    if (is_server_) {
        /* Require client certificate and valid cookie */
        gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUIRE);
        gnutls_dtls_prestate_set(session_, &prestate_);
    }

    gnutls_dtls_set_mtu(session_, DTLS_MTU);

    gnutls_transport_set_vec_push_function(session_, [](gnutls_transport_ptr_t t,
                                                    const giovec_t* iov,
                                                    int iovcnt) -> ssize_t {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        ssize_t sent = 0;
        for (int i=0; i<iovcnt; i++) {
            const giovec_t& dat = *(iov+i);
            auto ret = this_->tlsSend(dat.iov_base, dat.iov_len);
            if (ret < 0)
                return ret;
            sent += ret;
        }
        return sent;
    });
    gnutls_transport_set_pull_function(session_, [](gnutls_transport_ptr_t t,
                                                    void* d,
                                                    size_t s) -> ssize_t {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->tlsRecv(d, s);
    });
    gnutls_transport_set_pull_timeout_function(session_, [](gnutls_transport_ptr_t t,
                                                            unsigned ms) -> int {
        auto this_ = reinterpret_cast<SipsIceTransport*>(t);
        return this_->waitForTlsData(ms);
    });

    // start handshake
    handshakeStart_ = clock::now();
    state_ = TlsConnectionState::HANDSHAKING;

    return PJ_SUCCESS;
}

void
SipsIceTransport::closeTlsSession()
{
    state_ = TlsConnectionState::DISCONNECTED;
    if (session_) {
        gnutls_bye(session_, GNUTLS_SHUT_RDWR);
        gnutls_deinit(session_);
        session_ = nullptr;
    }

    xcred_.reset();
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

/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer and the serial number. */
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


/* Update local & remote certificates info. This function should be
 * called after handshake or renegotiation successfully completed. */
void
SipsIceTransport::certUpdate()
{
    /* Get active local certificate */
    if(const auto local_raw = gnutls_certificate_get_ours(session_))
        certGetInfo(pool_.get(), &localCertInfo_, local_raw, 1);
    else
        std::memset(&localCertInfo_, 0, sizeof(pj_ssl_cert_info));

    unsigned int certslen = 0;
    if (const auto remote_raw = gnutls_certificate_get_peers(session_, &certslen))
        certGetInfo(pool_.get(), &remoteCertInfo_, remote_raw, certslen);
    else
        std::memset(&remoteCertInfo_, 0, sizeof(pj_ssl_cert_info));
}

void
SipsIceTransport::getInfo(pj_ssl_sock_info* info)
{
    std::memset(info, 0, sizeof(*info));

    /* Established flag */
    info->established = (state_ == TlsConnectionState::ESTABLISHED);

    /* Protocol */
    info->proto = PJ_SSL_SOCK_PROTO_DTLS1;

    /* Local address */
    pj_sockaddr_cp(&info->local_addr, local_.pjPtr());

    if (info->established) {
        unsigned char id[2];
        gnutls_cipher_algorithm_t cipher;
        gnutls_cipher_algorithm_t lookup;

        /* Current cipher */
        cipher = gnutls_cipher_get(session_);
        for (size_t i=0; ; ++i) {
            const auto suite = gnutls_cipher_suite_info(i, id, nullptr, &lookup,
                                                        nullptr, nullptr);
            if (not suite) {
                RING_ERR("Can't find info for cipher %s (%d)", gnutls_cipher_get_name(cipher), cipher);
                break;
            }

            if (lookup == cipher) {
                info->cipher = (pj_ssl_cipher) ((id[0] << 8) | id[1]);
                break;
            }
        }

        /* Remote address */
        pj_sockaddr_cp(&info->remote_addr, remote_.pjPtr());

        /* Certificates info */
        info->local_cert_info = &localCertInfo_;
        info->remote_cert_info = &remoteCertInfo_;

        /* Verification status */
        info->verify_status = verifyStatus_;
    }

    /* Last known GnuTLS error code */
    info->last_native_err = last_err_;
}

pj_status_t
SipsIceTransport::tryHandshake()
{
    RING_DBG("SipsIceTransport::tryHandshake as %s",
             (is_server_ ? "server" : "client"));
    pj_status_t status;
    int ret = gnutls_handshake(session_);
    if (ret == GNUTLS_E_SUCCESS) {
        /* System are GO */
        RING_DBG("SipsIceTransport::tryHandshake : ESTABLISHED");
        state_ = TlsConnectionState::ESTABLISHED;
        status = PJ_SUCCESS;
    } else if (!gnutls_error_is_fatal(ret)) {
        /* Non fatal error, retry later (busy or again) */
        status = PJ_EPENDING;
    } else {
        /* Fatal error invalidates session, no fallback */
        RING_ERR("TLS fatal error : %s", gnutls_strerror(ret));
        status = PJ_EINVAL;
    }
    last_err_ = ret;
    return status;
}

/* When handshake completed:
 * - notify application
 * - if handshake failed, reset SSL state
 * - return false when SSL socket instance is destroyed by application. */
bool
SipsIceTransport::onHandshakeComplete(pj_status_t status)
{
    /* Update certificates info on successful handshake */
    if (status == PJ_SUCCESS) {
        RING_WARN("Handshake success on remote %s",
                   remote_.toString(true).c_str());
        certUpdate();
    } else {
        /* Handshake failed destroy ourself silently. */
        char errmsg[PJ_ERR_MSG_SIZE];
        RING_WARN("Handshake failed on remote %s: %s",
                  remote_.toString(true).c_str(),
                  pj_strerror(status, errmsg, sizeof(errmsg)).ptr);
    }

    ChangeStateEventData eventData;
    getInfo(&eventData.ssl_info);

    if (status != PJ_SUCCESS)
        shutdown(); // to be done after getInfo() call

    // push event to handleEvents()
    eventData.state_info.status = eventData.ssl_info.verify_status ? PJSIP_TLS_ECERTVERIF : PJ_SUCCESS;
    eventData.state = (status != PJ_SUCCESS) ? PJSIP_TP_STATE_DISCONNECTED : PJSIP_TP_STATE_CONNECTED;

    {
        std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
        stateChangeEvents_.emplace(std::move(eventData));
    }

    return status == PJ_SUCCESS;
}

int
SipsIceTransport::verifyCertificate()
{
    /* Support only x509 format */
    if (gnutls_certificate_type_get(session_) != GNUTLS_CRT_X509) {
        verifyStatus_ = PJ_SSL_CERT_EINVALID_FORMAT;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* Store verification status */
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

    if (param_.cert_check) {
        verifyStatus_ = param_.cert_check(status, cert_list, cert_list_size);
        if (verifyStatus_ != PJ_SUCCESS)
            return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* notify GnuTLS to continue handshake normally */
    return GNUTLS_E_SUCCESS;
}

void
SipsIceTransport::handleEvents()
{
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
    putBuff(std::move(rx));
    rxCv_.notify_all();

    // Report status GnuTLS -> SIP transport
    decltype(outputAckBuff_) ackBuf;
    {
        std::lock_guard<std::mutex> l(outputBuffMtx_);
        ackBuf = std::move(outputAckBuff_);
    }
    for (const auto& pair: ackBuf) {
        const auto& f = pair.first;
        f.tdata_op_key->tdata = nullptr;
        RING_ERR("status: %d", pair.second);
        if (f.tdata_op_key->callback)
            f.tdata_op_key->callback(getTransportBase(), f.tdata_op_key->token,
                                     pair.second);
    }
    cv_.notify_all();
}

bool
SipsIceTransport::setup()
{
    RING_WARN("Starting GnuTLS thread");

    // permit incoming packets
    ice_->setOnRecv(comp_id_, [this](uint8_t* buf, size_t len) {
            {
                std::lock_guard<std::mutex> l(inputBuffMtx_);
                tlsInputBuff_.emplace_back(buf, buf+len);
                canRead_ = true;
                RING_DBG("Ice: got data at %lu",
                         clock::now().time_since_epoch().count());
            }
            cv_.notify_all();
            return len;
        });

    if (is_server_) {
        gnutls_key_generate(&cookie_key_, GNUTLS_COOKIE_KEY_SIZE);
        state_ = TlsConnectionState::COOKIE;
        return true;
    }

    return startTlsSession() == PJ_SUCCESS;
}

void
SipsIceTransport::loop()
{
    if (not ice_->isRunning()) {
        shutdown();
        return;
    }

    if (state_ == TlsConnectionState::COOKIE) {
        {
            std::unique_lock<std::mutex> l(inputBuffMtx_);
            if (tlsInputBuff_.empty()) {
                cv_.wait(l, [&](){
                    return state_ != TlsConnectionState::COOKIE or not tlsInputBuff_.empty();
                });
            }
            if (state_ != TlsConnectionState::COOKIE)
                return;
            const auto& pck = tlsInputBuff_.front();
            std::memset(&prestate_, 0, sizeof(prestate_));
            int ret = gnutls_dtls_cookie_verify(&cookie_key_,
                                                &trData_.base.key.rem_addr,
                                                trData_.base.addr_len,
                                                (char*)pck.data(), pck.size(),
                                                &prestate_);
            if (ret < 0) {
                gnutls_dtls_cookie_send(&cookie_key_,
                                        &trData_.base.key.rem_addr,
                                        trData_.base.addr_len, &prestate_, this,
                                        [](gnutls_transport_ptr_t t,
                                           const void* d, size_t s) -> ssize_t {
                    auto this_ = reinterpret_cast<SipsIceTransport*>(t);
                    return this_->tlsSend(d, s);
                });
                tlsInputBuff_.pop_front();
                if (tlsInputBuff_.empty())
                    canRead_ = false;
                return;
            }
        }
        startTlsSession();
    }

    if (state_ == TlsConnectionState::HANDSHAKING) {
        if (clock::now() - handshakeStart_ > param_.timeout) {
            onHandshakeComplete(PJ_ETIMEDOUT);
            return;
        }
        auto status = tryHandshake();
        if (status != PJ_EPENDING)
            onHandshakeComplete(status);
    }

    if (state_ == TlsConnectionState::ESTABLISHED) {
        {
            std::mutex flagsMtx_ {};
            std::unique_lock<std::mutex> l(flagsMtx_);
            cv_.wait(l, [&](){
                return state_ != TlsConnectionState::ESTABLISHED or canRead_ or canWrite_;
            });
        }

        if (state_ != TlsConnectionState::ESTABLISHED and not getTransportBase()->is_shutdown)
            return;

        decltype(rxPending_) rx;
        while (canRead_ or gnutls_record_check_pending(session_)) {
            if (rx.empty())
                getBuff(rx, PJSIP_MAX_PKT_LEN);
            auto& buf = rx.front();
            const auto decrypted_size = gnutls_record_recv(session_, buf.data(), buf.size());

            if (decrypted_size > 0/* || transport error */) {
                buf.resize(decrypted_size);
                {
                    std::lock_guard<std::mutex> l(rxMtx_);
                    rxPending_.splice(rxPending_.end(), rx, rx.begin());
                }
            } else if (decrypted_size == 0) {
                /* EOF */
                tlsThread_.stop();
                break;
            } else if (decrypted_size == GNUTLS_E_AGAIN or
                       decrypted_size == GNUTLS_E_INTERRUPTED) {
                break;
            } else if (decrypted_size == GNUTLS_E_REHANDSHAKE) {
                /* Seems like we are renegotiating */

                RING_DBG("rehandshake");
                auto try_handshake_status = tryHandshake();

                /* Not pending is either success or failed */
                if (try_handshake_status != PJ_EPENDING) {
                    if (!onHandshakeComplete(try_handshake_status)) {
                        break;
                    }
                }

                if (try_handshake_status != PJ_SUCCESS and
                    try_handshake_status != PJ_EPENDING) {
                    break;
                }
            } else if (!gnutls_error_is_fatal(decrypted_size)) {
                /* non-fatal error, let's just continue */
            } else {
                shutdown();
                break;
            }
        }
        putBuff(std::move(rx));
        flushOutputBuff();
    }
}

void
SipsIceTransport::clean()
{
    RING_WARN("Ending GnuTLS thread");

    // Forbid GnuTLS <-> ICE IOs
    ice_->setOnRecv(comp_id_, nullptr);
    tlsInputBuff_.clear();
    canRead_ = false;
    canWrite_ = false;

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

    // note: incoming packets (rxPending_) will be processed in destructor

    if (cookie_key_.data) {
        gnutls_free(cookie_key_.data);
        cookie_key_.data = nullptr;
        cookie_key_.size = 0;
    }

    closeTlsSession();
}

IpAddr
SipsIceTransport::getLocalAddress() const
{
    return ice_->getLocalAddress(comp_id_);
}

IpAddr
SipsIceTransport::getRemoteAddress() const
{
    return ice_->getRemoteAddress(comp_id_);
}

ssize_t
SipsIceTransport::tlsSend(const void* d, size_t s)
{
    return ice_->send(comp_id_, (const uint8_t*)d, s);
}

ssize_t
SipsIceTransport::tlsRecv(void* d , size_t s)
{
    std::lock_guard<std::mutex> l(inputBuffMtx_);
    if (tlsInputBuff_.empty()) {
        gnutls_transport_set_errno(session_, EAGAIN);
        return -1;
    }
    const auto& front = tlsInputBuff_.front();
    const auto n = std::min(front.size(), s);
    std::copy_n(front.begin(), n, (uint8_t*)d);
    tlsInputBuff_.pop_front();
    if (tlsInputBuff_.empty())
        canRead_ = false;
    return n;
}

int
SipsIceTransport::waitForTlsData(unsigned ms)
{
    std::unique_lock<std::mutex> l(inputBuffMtx_);
    if (tlsInputBuff_.empty()) {
        cv_.wait_for(l, std::chrono::milliseconds(ms), [&]() {
            return state_ == TlsConnectionState::DISCONNECTED or not tlsInputBuff_.empty();
        });
    }
    if (state_ == TlsConnectionState::DISCONNECTED) {
        gnutls_transport_set_errno(session_, EINTR);
        return -1;
    }
    return tlsInputBuff_.empty() ? 0 : tlsInputBuff_.front().size();
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

    tdata->op_key.token = token;
    tdata->op_key.callback = callback;
    if (state_ == TlsConnectionState::ESTABLISHED) {
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
SipsIceTransport::trySend(pjsip_tx_data_op_key *pck)
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
SipsIceTransport::shutdown()
{
    RING_WARN("%s", __PRETTY_FUNCTION__);
    state_ = TlsConnectionState::DISCONNECTED;
    tlsThread_.stop();
    cv_.notify_all();
}

void
SipsIceTransport::getBuff(decltype(buffPool_)& l, const uint8_t* b, const uint8_t* e)
{
    std::lock_guard<std::mutex> lk(buffPoolMtx_);
    if (buffPool_.empty())
        l.emplace_back(b, e);
    else {
        l.splice(l.end(), buffPool_, buffPool_.begin());
        auto& buf = l.back();
        buf.resize(std::distance(b, e));
        std::copy(b, e, buf.begin());
    }
}

void
SipsIceTransport::getBuff(decltype(buffPool_)& l, const size_t s)
{
    std::lock_guard<std::mutex> lk(buffPoolMtx_);
    if (buffPool_.empty())
        l.emplace_back(s);
    else {
        l.splice(l.end(), buffPool_, buffPool_.begin());
        auto& buf = l.back();
        buf.resize(s);
    }
}

void
SipsIceTransport::putBuff(decltype(buffPool_)&& l)
{
    std::lock_guard<std::mutex> lk(buffPoolMtx_);
    buffPool_.splice(buffPool_.end(), l);
}

pj_status_t
SipsIceTransport::tls_status_from_err(int err)
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

}} // namespace ring::tls
