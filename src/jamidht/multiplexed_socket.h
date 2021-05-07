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

class IceTransport;
class ChannelSocket;
class TlsSocketEndpoint;

using DeviceId = dht::InfoHash;
using OnConnectionRequestCb = std::function<
    bool(const DeviceId& /* device id */, const uint16_t& /* id */, const std::string& /* name */)>;
using OnConnectionReadyCb
    = std::function<void(const DeviceId& /* device id */, const std::shared_ptr<ChannelSocket>&)>;
using onChannelReadyCb = std::function<void(void)>;
using OnShutdownCb = std::function<void(void)>;

static constexpr uint16_t CONTROL_CHANNEL {0};
static constexpr uint16_t PROTOCOL_CHANNEL {0xffff};
static constexpr int MULTIPLEXED_SOCKET_VERSION {1};

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

struct ChanneledMessage
{
    uint16_t channel;
    std::vector<uint8_t> data;
    MSGPACK_DEFINE(channel, data)
};

struct BeaconMsg
{
    bool isRequest;
    MSGPACK_DEFINE(isRequest)
};

struct VersionMsg
{
    int version;
    MSGPACK_DEFINE(version)
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
    /**
     * Triggered when a specific channel is ready
     * Used by ConnectionManager::connectDevice()
     */
    void setOnChannelReady(uint16_t channel, onChannelReadyCb&& cb);

    std::size_t read(const uint16_t& channel, uint8_t* buf, std::size_t len, std::error_code& ec);
    std::size_t write(const uint16_t& channel,
                      const uint8_t* buf,
                      std::size_t len,
                      std::error_code& ec);
    int waitForData(const uint16_t& channel,
                    std::chrono::milliseconds timeout,
                    std::error_code&) const;
    void setOnRecv(const uint16_t& channel, GenericSocket<uint8_t>::RecvCb&& cb);

    /**
     * This will close all channels and send a TLS EOF on the main socket.
     */
    void shutdown();
    /**
     * Will trigger that callback when shutdown() is called
     */
    void onShutdown(OnShutdownCb&& cb);

    std::shared_ptr<IceTransport> underlyingICE() const;

    /**
     * Get informations from socket (channels opened)
     */
    void monitor() const;

    /**
     * Send a beacon on the socket and close if no response come
     * @param timeout
     */
    void sendBeacon(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(3000));

#ifdef DRING_TESTABLE
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
    void setOnBeaconCb(const std::function<void(BeaconMsg)>& cb);

    /**
     * Set a callback to detect version messages
     */
    void setOnVersionCb(const std::function<void(VersionMsg)>& cb);

    /**
     * Send the version
     */
    void sendVersion();
#endif

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
    ChannelSocket(std::weak_ptr<MultiplexedSocket> endpoint,
                  const std::string& name,
                  const uint16_t& channel);
    ~ChannelSocket();

    DeviceId deviceId() const;
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
    void onShutdown(OnShutdownCb&& cb);

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

    std::shared_ptr<IceTransport> underlyingICE() const;

    /**
     * Send a beacon on the socket and close if no response come
     * @param timeout
     */
    void sendBeacon(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(3000));

#ifdef DRING_TESTABLE
    std::shared_ptr<MultiplexedSocket> underlyingSocket() const;
#endif

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami

MSGPACK_ADD_ENUM(jami::ChannelRequestState);