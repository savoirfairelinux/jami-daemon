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

#include "multiplexed_socket.h"

namespace jami {

class JamiAccount;
class ChannelSocket;
class ConnectionManager;

/**
 * A PeerConnectionRequest is a request which ask for an initial connection
 * It contains the ICE request an ID and if it's an answer
 * Transmitted via the UDP DHT
 */
struct PeerConnectionRequest : public dht::EncryptedValue<PeerConnectionRequest>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr const char* key_prefix = "peer:"; ///< base to compute the DHT listen key
    dht::Value::Id id = dht::Value::INVALID_ID;
    std::string ice_msg {};
    bool isAnswer {false};
    MSGPACK_DEFINE_MAP(id, ice_msg, isAnswer)
};

/**
 * Used to accept or not an incoming ICE connection (default accept)
 */
using onICERequestCallback = std::function<bool(const std::string& /* deviceId */)>;
/**
 * Used to accept or decline an incoming channel request
 */
using ChannelRequestCallBack = std::function<bool(const std::string& /* deviceId */, const std::string& /* name */)>;
/**
 * Used by connectDevice, when the socket is ready
 */
using ConnectCallback = std::function<void(const std::shared_ptr<ChannelSocket>&)>;
/**
 * Used when an incoming connection is ready
 */
using ConnectionReadyCallBack = std::function<void(const std::string& /* deviceId */, const std::string& /* channel_name */, std::shared_ptr<ChannelSocket>)>;

/**
 * Manages connections to other devices
 */
class ConnectionManager : public std::enable_shared_from_this<ConnectionManager> {
public:
    ConnectionManager(JamiAccount& account);
    ~ConnectionManager();

    /**
     * Open a new channel between the account's device and another device
     * This method will send a message on the account's DHT, wait a reply
     * and then, create a Tls socket with remote peer.
     * @param deviceId      Remote device
     * @param name          Name of the channel
     * @param cb            Callback called when socket is ready ready
     */
    void connectDevice(const std::string& deviceId, const std::string& name, ConnectCallback cb);

    /**
     * Method to call to listen to incoming requests
     * @param deviceId      Account's device
     */
    void onDhtConnected(const std::string& deviceId);

    /**
     * Add a callback to decline or accept incoming ICE connections
     * @note by default the ConnectionManager accepts all request
     * @param cb    Callback to trigger
     */
    void onICERequest(onICERequestCallback&& cb);

    /**
     * Trigger cb on incoming peer channel
     * @param cb    Callback to trigger
     * @note        The callback is used to validate
     * if the incoming request is accepted or not.
     */
    void onChannelRequest(ChannelRequestCallBack&& cb);

    /**
     * Trigger cb when connection with peer is ready
     * @param cb    Callback to trigger
     */
    void onConnectionReady(ConnectionReadyCallBack&& cb);

    std::shared_ptr<ConnectionManager> shared() {
        return std::static_pointer_cast<ConnectionManager>(shared_from_this());
    }
    std::shared_ptr<ConnectionManager const> shared() const {
        return std::static_pointer_cast<ConnectionManager const>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager> weak() {
        return std::static_pointer_cast<ConnectionManager>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager const> weak() const {
        return std::static_pointer_cast<ConnectionManager const>(shared_from_this());
    }

private:
    ConnectionManager() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}