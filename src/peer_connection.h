/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "data_transfer.h"
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

namespace dht {
namespace crypto {
struct PrivateKey;
struct Certificate;
} // namespace crypto
} // namespace dht

namespace jami {

using OnStateChangeCb = std::function<bool(tls::TlsSessionState state)>;
using OnReadyCb = std::function<void(bool ok)>;
using onShutdownCb = std::function<void(void)>;

class TurnTransport;
class ConnectedTurnTransport;

//==============================================================================

class Stream
{
public:
    virtual ~Stream() { close(); }
    virtual void close() noexcept {}
    virtual DRing::DataTransferId getId() const = 0;
    virtual bool write(std::string_view) { return false; };
    virtual void setOnRecv(std::function<void(std::string_view)>&&)
    {
        // Not implemented
    }
};

//==============================================================================

class IceSocketEndpoint : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    explicit IceSocketEndpoint(std::shared_ptr<IceTransport> ice, bool isSender);
    ~IceSocketEndpoint();

    void shutdown() override;
    bool isReliable() const override { return ice_ ? ice_->isRunning() : false; }
    bool isInitiator() const override { return ice_ ? ice_->isInitiator() : true; }
    int maxPayload() const override
    {
        return 65536 /* The max for a RTP packet used to wrap data here */;
    }
    int waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    std::shared_ptr<IceTransport> underlyingICE() const { return ice_; }

    void setOnRecv(RecvCb&& cb) override
    {
        if (ice_)
            ice_->setOnRecv(compId_, cb);
    }

    void setOnShutdown(onShutdownCb&& cb) { ice_->setOnShutdown(std::move(cb)); }

private:
    std::shared_ptr<IceTransport> ice_ {nullptr};
    std::atomic_bool iceStopped {false};
    std::atomic_bool iceIsSender {false};
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

    TlsSocketEndpoint(std::unique_ptr<IceSocketEndpoint>&& tr,
                      const Identity& local_identity,
                      const std::shared_future<tls::DhParams>& dh_params,
                      const dht::crypto::Certificate& peer_cert);
    TlsSocketEndpoint(std::unique_ptr<IceSocketEndpoint>&& tr,
                      const Identity& local_identity,
                      const std::shared_future<tls::DhParams>& dh_params,
                      std::function<bool(const dht::crypto::Certificate&)>&& cert_check);
    ~TlsSocketEndpoint();

    bool isReliable() const override { return true; }
    bool isInitiator() const override;
    int maxPayload() const override;
    void shutdown() override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&&) override
    {
        throw std::logic_error("TlsSocketEndpoint::setOnRecv not implemented");
    }
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;

    void waitForReady(const std::chrono::milliseconds& timeout = {});

    void setOnStateChange(OnStateChangeCb&& cb);
    void setOnReady(OnReadyCb&& cb);

    std::shared_ptr<IceTransport> underlyingICE() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
