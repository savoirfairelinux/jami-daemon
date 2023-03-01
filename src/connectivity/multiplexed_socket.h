/*
 *  Copyright (C) 2017-2023 Savoir-faire Linux Inc.
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

#include <cstdint>
#include <opendht/default_types.h>

#include "connectivity/generic_io.h"
#include <condition_variable>

namespace asio {
class io_context;
}

namespace jami {

class IceTransport;
class ChannelSocket;
class TlsSocketEndpoint;

using DeviceId = dht::PkId;
using OnConnectionRequestCb
    = std::function<bool(const std::shared_ptr<dht::crypto::Certificate>& /* peer */,
                         const uint16_t& /* id */,
                         const std::string& /* name */)>;
using OnConnectionReadyCb
    = std::function<void(const DeviceId& /* deviceId */, const std::shared_ptr<ChannelSocket>&)>;
using ChannelReadyCb = std::function<void(void)>;
using OnShutdownCb = std::function<void(void)>;

static constexpr auto SEND_BEACON_TIMEOUT = std::chrono::milliseconds(3000);
static constexpr uint16_t CONTROL_CHANNEL {0};
static constexpr uint16_t PROTOCOL_CHANNEL {0xffff};

enum class ChannelRequestState {
    REQUEST,
    ACCEPT,
    DECLINE,
};

/**
 * That msgpack structure is used to request a new channel (id, name)
 * Transmitted over the TLS socket
 */
struct ChannelRequest
{
    std::string name {};
    uint16_t channel {0};
    ChannelRequestState state {ChannelRequestState::REQUEST};
    MSGPACK_DEFINE(name, channel, state)
};

/**
 * A socket divided in channels over a TLS session
 */
class MultiplexedSocket : public std::enable_shared_from_this<MultiplexedSocket>
{
public:
    MultiplexedSocket(const DeviceId& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint);
    ~MultiplexedSocket();
    std::shared_ptr<ChannelSocket> addChannel(const std::string& name);

    std::shared_ptr<MultiplexedSocket> shared()
    {
        return std::static_pointer_cast<MultiplexedSocket>(shared_from_this());
    }
    std::shared_ptr<MultiplexedSocket const> shared() const
    {
        return std::static_pointer_cast<MultiplexedSocket const>(shared_from_this());
    }
    std::weak_ptr<MultiplexedSocket> weak()
    {
        return std::static_pointer_cast<MultiplexedSocket>(shared_from_this());
    }
    std::weak_ptr<MultiplexedSocket const> weak() const
    {
        return std::static_pointer_cast<MultiplexedSocket const>(shared_from_this());
    }

    DeviceId deviceId() const;
    bool isReliable() const;
    bool isInitiator() const;
    int maxPayload() const;

    /**
     * Will be triggered when a new channel is ready
     */
    void setOnReady(OnConnectionReadyCb&& cb);
    /**
     * Will be triggered when the peer asks for a new channel
     */
    void setOnRequest(OnConnectionRequestCb&& cb);

    std::size_t write(const uint16_t& channel,
                      const uint8_t* buf,
                      std::size_t len,
                      std::error_code& ec);

    /**
     * This will close all channels and send a TLS EOF on the main socket.
     */
    void shutdown();

    /**
     * This will wait that eventLoop is stopped and stop it if necessary
     */
    void join();

    /**
     * Will trigger that callback when shutdown() is called
     */
    void onShutdown(OnShutdownCb&& cb);

    /**
     * Get informations from socket (channels opened)
     */
    void monitor() const;

    /**
     * Checks if swarm channel is opened
     * @param DeviceId Device id
     * @param std::string Conversation id
     */
    bool hasSwarmChannel(const DeviceId& device, const std::string& convId);

    /**
     * Send a beacon on the socket and close if no response come
     * @param timeout
     */
    void sendBeacon(const std::chrono::milliseconds& timeout = SEND_BEACON_TIMEOUT);

    /**
     * Get peer's certificate
     */
    std::shared_ptr<dht::crypto::Certificate> peerCertificate() const;

    IpAddr getLocalAddress() const;
    IpAddr getRemoteAddress() const;

    void eraseChannel(uint16_t channel);

#ifdef LIBJAMI_TESTABLE
    /**
     * Check if we can send beacon on the socket
     */
    bool canSendBeacon() const;

    /**
     * Decide if yes or not we answer to beacon
     * @param value     New value
     */
    void answerToBeacon(bool value);

    /**
     * Change version sent to the peer
     */
    void setVersion(int version);

    /**
     * Set a callback to detect beacon messages
     */
    void setOnBeaconCb(const std::function<void(bool)>& cb);

    /**
     * Set a callback to detect version messages
     */
    void setOnVersionCb(const std::function<void(int)>& cb);

    /**
     * Send the version
     */
    void sendVersion();
#endif

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

class ChannelSocketInterface : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;

    virtual DeviceId deviceId() const = 0;
    virtual std::string name() const = 0;
    virtual uint16_t channel() const = 0;
    /**
     * Triggered when a specific channel is ready
     * Used by ConnectionManager::connectDevice()
     */
    virtual void onReady(ChannelReadyCb&& cb) = 0;
    /**
     * Will trigger that callback when shutdown() is called
     */
    virtual void onShutdown(OnShutdownCb&& cb) = 0;

    virtual void onRecv(std::vector<uint8_t>&& pkt) = 0;
};

class ChannelSocketTest : public ChannelSocketInterface
{
public:
    ChannelSocketTest(const DeviceId& deviceId, const std::string& name, const uint16_t& channel);
    ~ChannelSocketTest();

    static void link(const std::shared_ptr<ChannelSocketTest>& socket1,
                     const std::shared_ptr<ChannelSocketTest>& socket2);

    DeviceId deviceId() const override;
    std::string name() const override;
    uint16_t channel() const override;

    bool isReliable() const override { return true; };
    bool isInitiator() const override { return true; };
    int maxPayload() const override { return 0; };

    void shutdown() override;

    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;
    void setOnRecv(RecvCb&&) override;
    void onRecv(std::vector<uint8_t>&& pkt) override;

    /**
     * Triggered when a specific channel is ready
     * Used by ConnectionManager::connectDevice()
     */
    void onReady(ChannelReadyCb&& cb) override;
    /**
     * Will trigger that callback when shutdown() is called
     */
    void onShutdown(OnShutdownCb&& cb) override;

    std::vector<uint8_t> rx_buf {};
    mutable std::mutex mutex {};
    mutable std::condition_variable cv {};
    GenericSocket<uint8_t>::RecvCb cb {};

private:
    const DeviceId pimpl_deviceId;
    const std::string pimpl_name;
    const uint16_t pimpl_channel;
    asio::io_context& ioCtx_;
    std::weak_ptr<ChannelSocketTest> remote;
    OnShutdownCb shutdownCb_ {[&] {
    }};
    std::atomic_bool isShutdown_ {false};
};

/**
 * Represents a channel of the multiplexed socket (channel, name)
 */
class ChannelSocket : public ChannelSocketInterface
{
public:
    ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint,
                  const std::string& name,
                  const uint16_t& channel,
                  bool isInitiator = false,
                  std::function<void()> rmFromMxSockCb = {});
    ~ChannelSocket();

    DeviceId deviceId() const override;
    std::string name() const override;
    uint16_t channel() const override;
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

    void ready();
    /**
     * Triggered when a specific channel is ready
     * Used by ConnectionManager::connectDevice()
     */
    void onReady(ChannelReadyCb&& cb) override;
    /**
     * Will trigger that callback when shutdown() is called
     */
    void onShutdown(OnShutdownCb&& cb) override;

    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    /**
     * @note len should be < UINT8_MAX, else you will get ec = EMSGSIZE
     */
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;

    /**
     * set a callback when receiving data
     * @note: this callback should take a little time and not block
     * but you can move it in a thread
     */
    void setOnRecv(RecvCb&&) override;

    void onRecv(std::vector<uint8_t>&& pkt) override;

    /**
     * Send a beacon on the socket and close if no response come
     * @param timeout
     */
    void sendBeacon(const std::chrono::milliseconds& timeout = SEND_BEACON_TIMEOUT);

    /**
     * Get peer's certificate
     */
    std::shared_ptr<dht::crypto::Certificate> peerCertificate() const;

#ifdef LIBJAMI_TESTABLE
    std::shared_ptr<MultiplexedSocket> underlyingSocket() const;
#endif

    // Note: When a channel is accepted, it can receives data ASAP and when finished will be removed
    // however, onAccept is it's own thread due to the callbacks. In this case, the channel must be
    // deleted in the onAccept.
    void answered();
    bool isAnswered() const;
    void removable();
    bool isRemovable() const;

    IpAddr getLocalAddress() const;
    IpAddr getRemoteAddress() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami

MSGPACK_ADD_ENUM(jami::ChannelRequestState);
