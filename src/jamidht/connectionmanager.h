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
class PeerSocket;
class TlsSocketEndpoint;

struct PeerConnectionRequest : public dht::EncryptedValue<PeerConnectionRequest>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr const char* key_prefix = "peer:"; ///< base to compute the DHT listen key
    dht::Value::Id id = dht::Value::INVALID_ID;
    std::string uri {};
    std::string ice_msg {};
    bool isResponse {false};
    MSGPACK_DEFINE_MAP(id, uri, ice_msg, isResponse)
};

// TODO move
class MultiplexedSocket
{
public:
    MultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint);
    std::shared_ptr<PeerSocket> addChannel(const std::string uri);

    std::string deviceId() const;
    bool isReliable() const{ return true; }
    bool isInitiator() const;
    int maxPayload() const;

    std::size_t read(const std::string& uri, uint8_t* buf, std::size_t len, std::error_code& ec);
    std::size_t write(const std::string& uri, const uint8_t* buf, std::size_t len, std::error_code& ec);
    int waitForData(const std::string& uri, std::chrono::milliseconds timeout, std::error_code&) const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// TODO move
class PeerSocket : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;
    PeerSocket(MultiplexedSocket& endpoint, const std::string& uri);
    std::string deviceId() const;
    std::string uri() const;

    bool isReliable() const override;
    bool isInitiator() const override;
    int maxPayload() const override;
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;
    void setOnRecv(RecvCb&&) override {
        throw std::logic_error("PeerSocket::setOnRecv not implemented");
    }
    int waitForData(std::chrono::milliseconds timeout, std::error_code&) const override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

using ConnectCallback = std::function<void(std::shared_ptr<PeerSocket>)>;
using PeerRequestCallBack = std::function<bool(const std::string&, const std::string&)>;
using ConnectionReadyCallBack = std::function<void(const std::string&, const std::string&, std::shared_ptr<PeerSocket>)>;

class ConnectionManager {
public:
    ConnectionManager(JamiAccount& account);
    ~ConnectionManager();

    void connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb);
    void onDhtConnected(const std::string& deviceId);

    void onPeerRequest(PeerRequestCallBack cb);
    void onConnectionReady(ConnectionReadyCallBack cb);

private:
    ConnectionManager() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}