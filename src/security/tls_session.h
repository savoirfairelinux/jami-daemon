/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

#include "noncopyable.h"

#include <gnutls/gnutls.h>

#include <string>
#include <functional>
#include <memory>
#include <future>
#include <chrono>
#include <vector>
#include <array>
#include <system_error>
#include <cstdint>

namespace dht { namespace crypto {
struct Certificate;
struct PrivateKey;
}} // namespace dht::crypto

namespace ring {

class GenericTransport
{
public:
    using RecvCb = std::function<ssize_t(uint8_t* buf, std::size_t len)>;

    virtual bool isReliable() const = 0;
    virtual bool isInitiator() const = 0;

    /// Return maximum application payload size.
    /// This value is negative if the session is not ready to give a valid answer.
    /// The value is 0 if such information is irrelevant for the session.
    /// If stricly positive, the user must use send() with an input buffer size below or equals
    /// to this value if it want to be sure that the transport sent it in an atomic way.
    /// Example: in case of non-reliable transport using packet oriented IO,
    /// this value gives the maximal size used to send one packet.
    virtual int maxPayload() const = 0;

    virtual void setOnRecv(RecvCb&& cb) = 0;
    virtual std::size_t send(const void* buf, std::size_t len, std::error_code& ec) = 0;

    /// Works as send(data, size, ec) but with C++ standard continuous buffer containers (like vector and array).
    template <typename T>
    std::size_t send(const T& buffer, std::error_code& ec) {
        return send(buffer.data(),
                    buffer.size() * sizeof(decltype(buffer)::value_type),
                    ec);
    }

protected:
    GenericTransport() = default;
    virtual ~GenericTransport() = default;
};

} // namespace ring

namespace ring { namespace tls {

class DhParams;

enum class TlsSessionState
{
    SETUP,
    COOKIE, // only used with non-initiator and non-reliable transport
    HANDSHAKE,
    MTU_DISCOVERY, // only used with non-reliable transport
    ESTABLISHED,
    SHUTDOWN
};

struct TlsParams
{
    // User CA list for session credentials
    std::string ca_list;

    std::shared_ptr<dht::crypto::Certificate> peer_ca;

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

/// TlsSession
///
/// Manages a TLS/DTLS data transport overlayed on a given generic transport.
///
/// /note API is not thread-safe.
///
class TlsSession final : public GenericTransport
{
public:
    using OnStateChangeFunc = std::function<void(TlsSessionState)>;
    using OnRxDataFunc = std::function<void(std::vector<uint8_t>&&)>;
    using OnCertificatesUpdate = std::function<void(const gnutls_datum_t*, const gnutls_datum_t*,
                                                    unsigned int)>;
    using VerifyCertificate = std::function<int(gnutls_session_t)>;

    // ===> WARNINGS <===
    // Following callbacks are called into the FSM thread context
    // Do not call blocking routines inside them.
    using TlsSessionCallbacks = struct {
        OnStateChangeFunc onStateChange;
        OnRxDataFunc onRxData;
        OnCertificatesUpdate onCertificatesUpdate;
        VerifyCertificate verifyCertificate;
    };

    TlsSession(GenericTransport& transport, const TlsParams& params,
               const TlsSessionCallbacks& cbs, bool anonymous=true);
    ~TlsSession();

    /// Return the name of current cipher.
    /// Can be called by onStateChange callback when state == ESTABLISHED
    /// to obtain the used cypher suite id.
    const char* currentCipherSuiteId(std::array<uint8_t, 2>& cs_id) const;

    /// Request TLS thread to stop and quit. IO are not possible after that.
    void shutdown();

    /// Return true if the TLS session type is a server.
    bool isInitiator() const override;

    bool isReliable() const;

    void setOnRecv(RecvCb&&) { }

    int maxPayload() const;

    /// Synchronous sending operation.
    /// Return a negative number (gnutls error) or a positive number for bytes sent.
    /// If data length is bigger than maxPayload() value (if > 0) then data are splitted into
    /// chunks of maxPayload() bytes at maximum.
    std::size_t send(const void* data, std::size_t size, std::error_code& ec) override;

private:
    class TlsSessionImpl;
    std::unique_ptr<TlsSessionImpl> pimpl_;
};

}} // namespace ring::tls
