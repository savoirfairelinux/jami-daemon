/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
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

#include "dring/datatransfer_interface.h"
#include "ip_utils.h"
#include "generic_io.h"
#include "security/diffie-hellman.h"
#include "opendht/crypto.h"

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <future>
#include <utility>

namespace dht { namespace crypto {
struct PrivateKey;
struct Certificate;
}}

namespace jami {

class Account;
class TurnTransport;
class ConnectedTurnTransport;

//==============================================================================

class Stream
{
public:
    virtual ~Stream() { close(); }
    virtual void close() noexcept { }
    virtual DRing::DataTransferId getId() const = 0;
    virtual bool read(std::vector<uint8_t>& buffer) const {
        (void)buffer;
        return false;
    }
    virtual bool write(const std::vector<uint8_t>& buffer) {
        (void)buffer;
        return false;
    };
    virtual bool write(const uint8_t* buffer, std::size_t length) {
        (void)buffer;
        (void)length;
        return false;
    };
};

//==============================================================================

/// Implement a server TLS session IO over a client TURN connection
class TlsTurnEndpoint : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    using Identity = std::pair<std::shared_ptr<dht::crypto::PrivateKey>,
                               std::shared_ptr<dht::crypto::Certificate>>;

    TlsTurnEndpoint(ConnectedTurnTransport& turn,
                    const Identity& local_identity,
                    const std::shared_future<tls::DhParams>& dh_params,
                    std::function<bool(const dht::crypto::Certificate&)>&& cert_check);
    ~TlsTurnEndpoint();

    void shutdown() override;
    bool isReliable() const override { return true; }
    bool isInitiator() const override;
    int maxPayload() const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("TlsTurnEndpoint::setOnRecv not implemented");
    }
    int waitForData(unsigned, std::error_code&) const override;

    void waitForReady(const std::chrono::steady_clock::duration& timeout = {});

    const dht::crypto::Certificate& peerCertificate() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

//==============================================================================

/// Implement system socket IO
class TcpSocketEndpoint : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    explicit TcpSocketEndpoint(const IpAddr& addr);
    ~TcpSocketEndpoint();

    bool isReliable() const override { return true; }
    bool isInitiator() const override { return true; }
    int maxPayload() const override { return 1280; }
    int waitForData(unsigned ms_timeout, std::error_code& ec) const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("TcpSocketEndpoint::setOnRecv not implemented");
    }

    void connect(const std::chrono::steady_clock::duration& timeout = {});

private:
    const IpAddr addr_;
    int sock_ {-1};
};

//==============================================================================

/// Implement a TLS session IO over a system socket
class TlsSocketEndpoint : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    using Identity = std::pair<std::shared_ptr<dht::crypto::PrivateKey>,
                               std::shared_ptr<dht::crypto::Certificate>>;

    TlsSocketEndpoint(TcpSocketEndpoint& parent,
                      const Identity& local_identity,
                      const std::shared_future<tls::DhParams>& dh_params,
                      const dht::crypto::Certificate& peer_cert);
    ~TlsSocketEndpoint();

    bool isReliable() const override { return true; }
    bool isInitiator() const override { return true; }
    int maxPayload() const override { return 1280; }
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("TlsSocketEndpoint::setOnRecv not implemented");
    }
    int waitForData(unsigned, std::error_code&) const override;

    void waitForReady(const std::chrono::steady_clock::duration& timeout = {});

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

//==============================================================================

class PeerConnection
{
public:
    using SocketType = GenericSocket<uint8_t>;

    PeerConnection(std::function<void()>&& done, Account& account, const std::string& peer_uri,
                   std::unique_ptr<SocketType> endpoint);

    ~PeerConnection();

    void attachOutputStream(const std::shared_ptr<Stream>& stream);

    void attachInputStream(const std::shared_ptr<Stream>& stream);

    /**
     * Check if an input or output stream got the given id.
     * NOTE: used by p2p to know which PeerConnection to close
     * @param id to check
     * @return if id is found
     */
    bool hasStreamWithId(const DRing::DataTransferId& id);

    std::string getPeerUri() const;

private:
    class PeerConnectionImpl;
    std::unique_ptr<PeerConnectionImpl> pimpl_;
};

} // namespace jami
