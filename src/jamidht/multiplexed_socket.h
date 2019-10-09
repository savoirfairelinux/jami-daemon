/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <opendht/default_types.h>

#include "generic_io.h"

namespace jami {

class ChannelSocket;
class TlsSocketEndpoint;

using onConnectionRequestCb = std::function<bool(const std::string& /* device id */, const uint16_t& /* id */, const std::string& /* name */)>;
using onConnectionReadyCb = std::function<void(const std::string& /* device id */, const std::shared_ptr<ChannelSocket>&)>;
using onChannelReadyCb = std::function<void(void)>;
using onShutdownCb = std::function<void(void)>;

static constexpr uint16_t CONTROL_CHANNEL {0};

/**
 * That msgpack structure is used to request a new channel (id, name)
 * Transmitted over the TLS socket
 */
struct ChannelRequest {
    std::string name {};
    uint16_t channel {0};
    bool isAnswer {false};
    MSGPACK_DEFINE(name, channel, isAnswer)
};

/**
 * A socket divided in channels over a TLS session
 */
class MultiplexedSocket : public std::enable_shared_from_this<MultiplexedSocket> {
public:
    MultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint);
    ~MultiplexedSocket();
    std::shared_ptr<ChannelSocket> addChannel(const std::string name);

    std::shared_ptr<MultiplexedSocket> shared() {
        return std::static_pointer_cast<MultiplexedSocket>(shared_from_this());
    }
    std::shared_ptr<MultiplexedSocket const> shared() const {
        return std::static_pointer_cast<MultiplexedSocket const>(shared_from_this());
    }
    std::weak_ptr<MultiplexedSocket> weak() {
        return std::static_pointer_cast<MultiplexedSocket>(shared_from_this());
    }
    std::weak_ptr<MultiplexedSocket const> weak() const {
        return std::static_pointer_cast<MultiplexedSocket const>(shared_from_this());
    }

    std::string deviceId() const;
    bool isReliable() const;
    bool isInitiator() const;
    int maxPayload() const;

    /**
     * Will be triggered when a new channel is ready
     */
    void setOnReady(onConnectionReadyCb&& cb);
    /**
     * Will be triggered when the peer asks for a new channel
     */
    void setOnRequest(onConnectionRequestCb&& cb);
    /**
     * Triggered when a specific channel is ready
     * Used by ConnectionManager::connectDevice()
     */
    void setOnChannelReady(uint16_t channel, onChannelReadyCb&& cb);

    std::size_t read(const uint16_t& channel, uint8_t* buf, std::size_t len, std::error_code& ec);
    std::size_t write(const uint16_t& channel, const uint8_t* buf, std::size_t len, std::error_code& ec);
    int waitForData(const uint16_t& channel, std::chrono::milliseconds timeout, std::error_code&) const;

    /**
     * This will close all channels and send a TLS EOF on the main socket.
     */
    void shutdown();
    /**
     * Will trigger that callback when shutdown() is called
     */
    void onShutdown(onShutdownCb&& cb);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

/**
 * Represents a channel of the multiplexed socket (channel, name)
 */
class ChannelSocket : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint, const std::string& name, const uint16_t& channel);
    ~ChannelSocket();

    std::string deviceId() const;
    std::string name() const;
    uint16_t channel() const;
    bool isReliable() const override;
    bool isInitiator() const override;
    int maxPayload() const override;
    /**
     * Like shutdown, but don't send any packet on the socket.
     * Used by Multiplexed Socket when the TLS endpoint is already shutting down
     */
    void stop();

    /**
     * This will send an empty buffer as a packet (equivalent to EOF)
     * Will trigger onShutdown's callback
     */
    void shutdown() override;
    /**
     * Will trigger that callback when shutdown() is called
     */
    void onShutdown(onShutdownCb&& cb);

    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    /**
     * @note len should be < UINT8_MAX, else you will get ec = EMSGSIZE
     */
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;

    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("ChannelSocket::setOnRecv not implemented");
    }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}