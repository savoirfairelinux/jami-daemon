/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#pragma once

#include "noncopyable.h"
#include "generic_io.h"

#include <gnutls/gnutls.h>

#include <string>
#include <functional>
#include <memory>
#include <future>
#include <chrono>
#include <vector>
#include <array>

namespace dht { namespace crypto {
struct Certificate;
struct PrivateKey;
}} // namespace dht::crypto

namespace jami { namespace tls {

class DhParams;

enum class TlsSessionState
{
    NONE,
    SETUP,
    COOKIE, // only used with non-initiator and non-reliable transport
    HANDSHAKE,
    MTU_DISCOVERY, // only used with non-reliable transport
    ESTABLISHED,
    SHUTDOWN
};

using clock = std::chrono::steady_clock;
using duration = clock::duration;

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
    duration timeout;

    // Callback for certificate checkings
    std::function<int(unsigned status,
                      const gnutls_datum_t* cert_list,
                      unsigned cert_list_size)> cert_check;
};

/// TlsSession
///
/// Manages a TLS/DTLS data transport overlayed on a given generic socket.
///
/// \note API is not thread-safe.
///
class TlsSession : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    using OnStateChangeFunc = std::function<void(TlsSessionState)>;
    using OnRxDataFunc = std::function<void(std::vector<uint8_t>&&)>;
    using OnCertificatesUpdate = std::function<void(const gnutls_datum_t*,
                                                    const gnutls_datum_t*,
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

    TlsSession(SocketType& transport, const TlsParams& params, const TlsSessionCallbacks& cbs,
               bool anonymous=true);
    ~TlsSession();

    /// Return the name of current cipher.
    /// Can be called by onStateChange callback when state == ESTABLISHED
    /// to obtain the used cypher suite id.
    const char* currentCipherSuiteId(std::array<uint8_t, 2>& cs_id) const;

    /// Request TLS thread to stop and quit.
    /// \note IO operations return error after this call.
    void shutdown() override;

    void setOnRecv(RecvCb&& cb) override {
        (void)cb;
        throw std::logic_error("TlsSession::setOnRecv not implemented");
    }

    /// Return true if the TLS session type is a server.
    bool isInitiator() const override;

    bool isReliable() const override;

    int maxPayload() const override;

    void waitForReady(const duration& timeout = {});

    /// Synchronous writing.
    /// Return a positive number for number of bytes write, or 0 and \a ec set in case of error.
    std::size_t write(const ValueType* data, std::size_t size, std::error_code& ec) override;

    /// Synchronous reading.
    /// Return a positive number for number of bytes read, or 0 and \a ec set in case of error.
    std::size_t read(ValueType* data, std::size_t size, std::error_code& ec) override;

    int waitForData(std::chrono::milliseconds, std::error_code&) const override;

private:
    class TlsSessionImpl;
    std::unique_ptr<TlsSessionImpl> pimpl_;
};

}} // namespace jami::tls
