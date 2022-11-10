/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "peer_connection.h"

#include "manager.h"
#include "connectivity/security/tls_session.h"

#include <opendht/thread_pool.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <istream>
#include <ostream>
#include <unistd.h>
#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#endif

#ifndef _MSC_VER
#include <sys/time.h>
#endif

static constexpr int ICE_COMP_ID_SIP_TRANSPORT {1};

namespace jami {

int
init_crt(gnutls_session_t session, dht::crypto::Certificate& crt)
{
    // Support only x509 format
    if (gnutls_certificate_type_get(session) != GNUTLS_CRT_X509) {
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    // Store verification status
    unsigned int status = 0;
    auto ret = gnutls_certificate_verify_peers2(session, &status);
    if (ret < 0 or (status & GNUTLS_CERT_SIGNATURE_FAILURE) != 0) {
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    unsigned int cert_list_size = 0;
    auto cert_list = gnutls_certificate_get_peers(session, &cert_list_size);
    if (cert_list == nullptr) {
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    // Check if received peer certificate is awaited
    std::vector<std::pair<uint8_t*, uint8_t*>> crt_data;
    crt_data.reserve(cert_list_size);
    for (unsigned i = 0; i < cert_list_size; i++)
        crt_data.emplace_back(cert_list[i].data, cert_list[i].data + cert_list[i].size);
    crt = dht::crypto::Certificate {crt_data};

    return GNUTLS_E_SUCCESS;
}

using lock = std::lock_guard<std::mutex>;

//==============================================================================

IceSocketEndpoint::IceSocketEndpoint(std::shared_ptr<IceTransport> ice, bool isSender)
    : ice_(std::move(ice))
    , iceIsSender(isSender)
{}

IceSocketEndpoint::~IceSocketEndpoint()
{
    shutdown();
    if (ice_)
        dht::ThreadPool::io().run([ice = std::move(ice_)] {});
}

void
IceSocketEndpoint::shutdown()
{
    // Sometimes the other peer never send any packet
    // So, we cancel pending read to avoid to have
    // any blocking operation.
    if (ice_)
        ice_->cancelOperations();
}

int
IceSocketEndpoint::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (ice_) {
        if (!ice_->isRunning())
            return -1;
        return ice_->waitForData(compId_, timeout, ec);
    }
    return -1;
}

std::size_t
IceSocketEndpoint::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (ice_) {
        if (!ice_->isRunning())
            return 0;
        try {
            auto res = ice_->recvfrom(compId_, reinterpret_cast<char*>(buf), len, ec);
            if (res < 0)
                shutdown();
            return res;
        } catch (const std::exception& e) {
            JAMI_ERR("IceSocketEndpoint::read exception: %s", e.what());
        }
        return 0;
    }
    return -1;
}

std::size_t
IceSocketEndpoint::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (ice_) {
        if (!ice_->isRunning())
            return 0;
        auto res = 0;
        res = ice_->send(compId_, reinterpret_cast<const unsigned char*>(buf), len);
        if (res < 0) {
            ec.assign(errno, std::generic_category());
            shutdown();
        } else {
            ec.clear();
        }
        return res;
    }
    return -1;
}

//==============================================================================

class TlsSocketEndpoint::Impl
{
public:
    static constexpr auto TLS_TIMEOUT = std::chrono::seconds(40);

    Impl(std::unique_ptr<IceSocketEndpoint>&& ep,
         const dht::crypto::Certificate& peer_cert,
         const Identity& local_identity,
         const std::shared_future<tls::DhParams>& dh_params)
        : peerCertificate {peer_cert}
        , ep_ {ep.get()}
    {
        tls::TlsSession::TlsSessionCallbacks tls_cbs
            = {/*.onStateChange = */ [this](tls::TlsSessionState state) { onTlsStateChange(state); },
               /*.onRxData = */ [this](std::vector<uint8_t>&& buf) { onTlsRxData(std::move(buf)); },
               /*.onCertificatesUpdate = */
               [this](const gnutls_datum_t* l, const gnutls_datum_t* r, unsigned int n) {
                   onTlsCertificatesUpdate(l, r, n);
               },
               /*.verifyCertificate = */
               [this](gnutls_session_t session) {
                   return verifyCertificate(session);
               }};
        tls::TlsParams tls_param = {
            /*.ca_list = */ "",
            /*.peer_ca = */ nullptr,
            /*.cert = */ local_identity.second,
            /*.cert_key = */ local_identity.first,
            /*.dh_params = */ dh_params,
            /*.timeout = */ TLS_TIMEOUT,
            /*.cert_check = */ nullptr,
        };
        tls = std::make_unique<tls::TlsSession>(std::move(ep), tls_param, tls_cbs);
    }

    Impl(std::unique_ptr<IceSocketEndpoint>&& ep,
         std::function<bool(const dht::crypto::Certificate&)>&& cert_check,
         const Identity& local_identity,
         const std::shared_future<tls::DhParams>& dh_params)
        : peerCertificateCheckFunc {std::move(cert_check)}
        , peerCertificate {null_cert}
        , ep_ {ep.get()}
    {
        tls::TlsSession::TlsSessionCallbacks tls_cbs
            = {/*.onStateChange = */ [this](tls::TlsSessionState state) { onTlsStateChange(state); },
               /*.onRxData = */ [this](std::vector<uint8_t>&& buf) { onTlsRxData(std::move(buf)); },
               /*.onCertificatesUpdate = */
               [this](const gnutls_datum_t* l, const gnutls_datum_t* r, unsigned int n) {
                   onTlsCertificatesUpdate(l, r, n);
               },
               /*.verifyCertificate = */
               [this](gnutls_session_t session) {
                   return verifyCertificate(session);
               }};
        tls::TlsParams tls_param = {
            /*.ca_list = */ "",
            /*.peer_ca = */ nullptr,
            /*.cert = */ local_identity.second,
            /*.cert_key = */ local_identity.first,
            /*.dh_params = */ dh_params,
            /*.timeout = */ std::chrono::duration_cast<decltype(tls::TlsParams::timeout)>(TLS_TIMEOUT),
            /*.cert_check = */ nullptr,
        };
        tls = std::make_unique<tls::TlsSession>(std::move(ep), tls_param, tls_cbs);
    }

    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lk(cbMtx_);
            onStateChangeCb_ = {};
            onReadyCb_ = {};
        }
        tls.reset();
    }

    std::shared_ptr<IceTransport> underlyingICE() const
    {
        if (ep_)
            if (const auto* iceSocket = reinterpret_cast<const IceSocketEndpoint*>(ep_))
                return iceSocket->underlyingICE();
        return {};
    }

    // TLS callbacks
    int verifyCertificate(gnutls_session_t);
    void onTlsStateChange(tls::TlsSessionState);
    void onTlsRxData(std::vector<uint8_t>&&);
    void onTlsCertificatesUpdate(const gnutls_datum_t*, const gnutls_datum_t*, unsigned int);

    std::mutex cbMtx_ {};
    OnStateChangeCb onStateChangeCb_;
    dht::crypto::Certificate null_cert;
    std::function<bool(const dht::crypto::Certificate&)> peerCertificateCheckFunc;
    const dht::crypto::Certificate& peerCertificate;
    std::atomic_bool isReady_ {false};
    OnReadyCb onReadyCb_;
    std::unique_ptr<tls::TlsSession> tls;
    const IceSocketEndpoint* ep_;
};

int
TlsSocketEndpoint::Impl::verifyCertificate(gnutls_session_t session)
{
    dht::crypto::Certificate crt;
    auto verified = init_crt(session, crt);
    if (verified != GNUTLS_E_SUCCESS)
        return verified;
    if (peerCertificateCheckFunc) {
        if (!peerCertificateCheckFunc(crt)) {
            JAMI_ERR() << "[TLS-SOCKET] Refusing peer certificate";
            return GNUTLS_E_CERTIFICATE_ERROR;
        }

        null_cert = std::move(crt);
    } else {
        if (crt.getPacked() != peerCertificate.getPacked()) {
            JAMI_ERR() << "[TLS-SOCKET] Unexpected peer certificate";
            return GNUTLS_E_CERTIFICATE_ERROR;
        }
    }

    return GNUTLS_E_SUCCESS;
}

void
TlsSocketEndpoint::Impl::onTlsStateChange(tls::TlsSessionState state)
{
    std::lock_guard<std::mutex> lk(cbMtx_);
    if ((state == tls::TlsSessionState::SHUTDOWN || state == tls::TlsSessionState::ESTABLISHED)
        && !isReady_) {
        isReady_ = true;
        if (onReadyCb_)
            onReadyCb_(state == tls::TlsSessionState::ESTABLISHED);
    }
    if (onStateChangeCb_ && !onStateChangeCb_(state))
        onStateChangeCb_ = {};
}

void
TlsSocketEndpoint::Impl::onTlsRxData(UNUSED std::vector<uint8_t>&& buf)
{}

void
TlsSocketEndpoint::Impl::onTlsCertificatesUpdate(UNUSED const gnutls_datum_t* local_raw,
                                                 UNUSED const gnutls_datum_t* remote_raw,
                                                 UNUSED unsigned int remote_count)
{}

TlsSocketEndpoint::TlsSocketEndpoint(std::unique_ptr<IceSocketEndpoint>&& tr,
                                     const Identity& local_identity,
                                     const std::shared_future<tls::DhParams>& dh_params,
                                     const dht::crypto::Certificate& peer_cert)
    : pimpl_ {std::make_unique<Impl>(std::move(tr), peer_cert, local_identity, dh_params)}
{}

TlsSocketEndpoint::TlsSocketEndpoint(
    std::unique_ptr<IceSocketEndpoint>&& tr,
    const Identity& local_identity,
    const std::shared_future<tls::DhParams>& dh_params,
    std::function<bool(const dht::crypto::Certificate&)>&& cert_check)
    : pimpl_ {
        std::make_unique<Impl>(std::move(tr), std::move(cert_check), local_identity, dh_params)}
{}

TlsSocketEndpoint::~TlsSocketEndpoint() {}

bool
TlsSocketEndpoint::isInitiator() const
{
    if (!pimpl_->tls) {
        return false;
    }
    return pimpl_->tls->isInitiator();
}

int
TlsSocketEndpoint::maxPayload() const
{
    if (!pimpl_->tls) {
        return -1;
    }
    return pimpl_->tls->maxPayload();
}

std::size_t
TlsSocketEndpoint::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (!pimpl_->tls) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    return pimpl_->tls->read(buf, len, ec);
}

std::size_t
TlsSocketEndpoint::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (!pimpl_->tls) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    return pimpl_->tls->write(buf, len, ec);
}

std::shared_ptr<dht::crypto::Certificate>
TlsSocketEndpoint::peerCertificate() const
{
    if (!pimpl_->tls)
        return {};
    return pimpl_->tls->peerCertificate();
}

void
TlsSocketEndpoint::waitForReady(const std::chrono::milliseconds& timeout)
{
    if (!pimpl_->tls) {
        return;
    }
    pimpl_->tls->waitForReady(timeout);
}

int
TlsSocketEndpoint::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (!pimpl_->tls) {
        ec = std::make_error_code(std::errc::broken_pipe);
        return -1;
    }
    return pimpl_->tls->waitForData(timeout, ec);
}

void
TlsSocketEndpoint::setOnStateChange(std::function<bool(tls::TlsSessionState state)>&& cb)
{
    std::lock_guard<std::mutex> lk(pimpl_->cbMtx_);
    pimpl_->onStateChangeCb_ = std::move(cb);
}

void
TlsSocketEndpoint::setOnReady(std::function<void(bool ok)>&& cb)
{
    std::lock_guard<std::mutex> lk(pimpl_->cbMtx_);
    pimpl_->onReadyCb_ = std::move(cb);
}

void
TlsSocketEndpoint::shutdown()
{
    pimpl_->tls->shutdown();
    if (pimpl_->ep_) {
        const auto* iceSocket = reinterpret_cast<const IceSocketEndpoint*>(pimpl_->ep_);
        if (iceSocket && iceSocket->underlyingICE())
            iceSocket->underlyingICE()->cancelOperations();
    }
}

void
TlsSocketEndpoint::monitor() const
{
    if (auto ice = pimpl_->underlyingICE())
        JAMI_DBG("\t- Ice connection: %s", ice->link().c_str());
}

IpAddr
TlsSocketEndpoint::getLocalAddress() const
{
    if (auto ice = pimpl_->underlyingICE())
        return ice->getLocalAddress(ICE_COMP_ID_SIP_TRANSPORT);
    return {};
}

IpAddr
TlsSocketEndpoint::getRemoteAddress() const
{
    if (auto ice = pimpl_->underlyingICE())
        return ice->getRemoteAddress(ICE_COMP_ID_SIP_TRANSPORT);
    return {};
}

} // namespace jami
