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

#include <memory>
#include <vector>
#include <string>

#include <opendht/dhtrunner.h>
#include <opendht/infohash.h>
#include <opendht/value.h>
#include <opendht/default_types.h>

#include "generic_io.h"

namespace jami {

class JamiAccount;
class ChannelSocket;
class TlsSocketEndpoint;

struct PeerConnectionRequest : public dht::EncryptedValue<PeerConnectionRequest>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr const char* key_prefix = "peer:"; ///< base to compute the DHT listen key
    dht::Value::Id id = dht::Value::INVALID_ID;
    std::string name {};
    std::string ice_msg {};
    uint16_t channel {0};
    bool isResponse {false};
    MSGPACK_DEFINE_MAP(id, name, channel, ice_msg, isResponse)
};

// TODO move
class MultiplexedSocket
{
public:
    MultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint);
    std::shared_ptr<ChannelSocket> addChannel(const std::string name);

    std::string deviceId() const;
    bool isReliable() const{ return true; }
    bool isInitiator() const;
    int maxPayload() const;

    std::size_t read(const uint16_t& channel, uint8_t* buf, std::size_t len, std::error_code& ec);
    std::size_t write(const uint16_t& channel, const uint8_t* buf, std::size_t len, std::error_code& ec);
    int waitForData(const uint16_t& channel, std::chrono::milliseconds timeout, std::error_code&) const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// TODO move
class ChannelSocket : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    ChannelSocket(MultiplexedSocket& endpoint, const std::string& name, const uint16_t& channel);
    std::string deviceId() const;
    std::string name() const;
    uint16_t channel() const;

    bool isReliable() const override;
    bool isInitiator() const override;
    int maxPayload() const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;
    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("ChannelSocket::setOnRecv not implemented");
    }
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

using ConnectCallback = std::function<void(std::shared_ptr<ChannelSocket>)>;
using PeerRequestCallBack = std::function<bool(const std::string&, const std::string&)>;
using ConnectionReadyCallBack = std::function<void(const std::string&, const std::string&, std::shared_ptr<ChannelSocket>)>;

class ConnectionManager {
public:
    ConnectionManager(JamiAccount& account);
    ~ConnectionManager();

    /**
     * Open a new channel between the account's device and another device
     * This method will send a message on the account's DHT, wait a reply
     * and then, create a Tls socket with remote peer.
     * @param deviceId      Remote device
     * @param name          Name of the channel
     * @param cb            Callback called when ready
     */
    void connectDevice(const std::string& deviceId, const std::string& name, ConnectCallback cb);

    /**
     * Method to call to listen to incoming requests
     * @param deviceId      Account's device
     */
    void onDhtConnected(const std::string& deviceId);

    /**
     * Trigger cb on incoming peer requests
     * @param cb    Callback to trigger
     * @note        The callback is used to validate
     * if the incoming request is accepted or not.
     */
    void onPeerRequest(PeerRequestCallBack cb);

    /**
     * Trigger cb when connection with peer is ready
     * @param cb    Callback to trigger
     */
    void onConnectionReady(ConnectionReadyCallBack cb);

private:
    ConnectionManager() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}