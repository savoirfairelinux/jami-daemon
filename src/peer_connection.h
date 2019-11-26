/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#pragma once

#include "dring/datatransfer_interface.h"
#include "ip_utils.h"
#include "generic_io.h"
#include "security/diffie-hellman.h"
#include "opendht/crypto.h"
#include "ice_transport.h"
#include "security/tls_session.h"

#include <functional>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dht { namespace crypto {
struct PrivateKey;
struct Certificate;
}}

namespace jami {

using OnStateChangeCb = std::function<void(tls::TlsSessionState state)>;

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
    int waitForData(std::chrono::milliseconds, std::error_code&) const override;
    void waitForReady(const std::chrono::steady_clock::duration& timeout = {});

    const dht::crypto::Certificate& peerCertificate() const;
    void setOnStateChange(OnStateChangeCb&& cb);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

//==============================================================================

class AbstractSocketEndpoint : public GenericSocket<uint8_t>
{
public:
    virtual void connect(const std::chrono::milliseconds& = {}) {};

    void setOnRecv(RecvCb &&) override {
      throw std::logic_error("AbstractSocketEndpoint::setOnRecv not implemented");
    }
};

/// Implement system socket IO
class TcpSocketEndpoint : public AbstractSocketEndpoint
{
public:
    using SocketType = GenericSocket<uint8_t>;
    explicit TcpSocketEndpoint(const IpAddr& addr);
    ~TcpSocketEndpoint();

    bool isReliable() const override { return true; }
    bool isInitiator() const override { return true; }
    int maxPayload() const override { return 1280; }
    int waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;
    void connect(const std::chrono::milliseconds& timeout = {}) override;

private:
    const IpAddr addr_;
    int sock_ {-1};
};

class IceSocketEndpoint : public AbstractSocketEndpoint
{
public:
    using SocketType = GenericSocket<uint8_t>;
    explicit IceSocketEndpoint(std::shared_ptr<IceTransport> ice, bool isSender);
    ~IceSocketEndpoint();

    void shutdown() override;
    bool isReliable() const override { return ice_ ? ice_->isRunning() : false; }
    bool isInitiator() const override { return ice_ ? ice_->isInitiator() : true; }
    int maxPayload() const override { return 65536 /* The max for a RTP packet used to wrap data here */; }
    int waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&& cb) override {
        if (ice_) {
            ice_->setOnRecv(compId_, cb);
        }
    }

private:
    std::shared_ptr<IceTransport> ice_ {nullptr};
    std::atomic_bool iceStopped{false};
    std::atomic_bool iceIsSender{false};
    uint8_t compId_ {0};
};

//==============================================================================

/// Implement a TLS session IO over a system socket
class TlsSocketEndpoint : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    using Identity = std::pair<std::shared_ptr<dht::crypto::PrivateKey>,
                               std::shared_ptr<dht::crypto::Certificate>>;

    TlsSocketEndpoint(AbstractSocketEndpoint& tr,
                      const Identity& local_identity,
                      const std::shared_future<tls::DhParams>& dh_params,
                      const dht::crypto::Certificate& peer_cert);
    TlsSocketEndpoint(AbstractSocketEndpoint& tr,
                    const Identity& local_identity,
                    const std::shared_future<tls::DhParams>& dh_params,
                    std::function<bool(const dht::crypto::Certificate&)>&& cert_check);
    ~TlsSocketEndpoint();

    bool isReliable() const override { return true; }
    bool isInitiator() const override;
    int maxPayload() const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("TlsSocketEndpoint::setOnRecv not implemented");
    }
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;

    void waitForReady(const std::chrono::milliseconds& timeout = {});

    void setOnStateChange(OnStateChangeCb&& cb);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

//==============================================================================

class PeerConnection
{
public:
    using SocketType = GenericSocket<uint8_t>;
    PeerConnection(std::function<void()>&& done, const std::string& peer_uri,
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
