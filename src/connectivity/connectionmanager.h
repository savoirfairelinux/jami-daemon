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
    std::string connType {}; // Used for push notifications to know why we open a new connection
    MSGPACK_DEFINE_MAP(id, ice_msg, isAnswer, connType)
};

/**
 * Used to accept or not an incoming ICE connection (default accept)
 */
using onICERequestCallback = std::function<bool(const DeviceId&)>;
/**
 * Used to accept or decline an incoming channel request
 */
using ChannelRequestCallback = std::function<bool(const std::shared_ptr<dht::crypto::Certificate>&,
                                                  const std::string& /* name */)>;
/**
 * Used by connectDevice, when the socket is ready
 */
using ConnectCallback = std::function<void(const std::shared_ptr<ChannelSocket>&, const DeviceId&)>;
/**
 * Used when an incoming connection is ready
 */
using ConnectionReadyCallback = std::function<
    void(const DeviceId&, const std::string& /* channel_name */, std::shared_ptr<ChannelSocket>)>;

/**
 * Manages connections to other devices
 * @note the account MUST be valid if ConnectionManager lives
 */
class ConnectionManager
{
public:
    ConnectionManager(JamiAccount& account);
    ~ConnectionManager();

    /**
     * Open a new channel between the account's device and another device
     * This method will send a message on the account's DHT, wait a reply
     * and then, create a Tls socket with remote peer.
     * @param deviceId       Remote device
     * @param name           Name of the channel
     * @param cb             Callback called when socket is ready ready
     * @param noNewSocket    Do not negotiate a new socket if there is none
     * @param forceNewSocket Negotiate a new socket even if there is one // todo group with previous
     * (enum)
     * @param connType       Type of the connection
     */
    void connectDevice(const DeviceId& deviceId,
                       const std::string& name,
                       ConnectCallback cb,
                       bool noNewSocket = false,
                       bool forceNewSocket = false,
                       const std::string& connType = "");
    void connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                       const std::string& name,
                       ConnectCallback cb,
                       bool noNewSocket = false,
                       bool forceNewSocket = false,
                       const std::string& connType = "");

    /**
     * Check if we are already connecting to a device with a specific name
     * @param deviceId      Remote device
     * @param name          Name of the channel
     * @return if connecting
     * @note isConnecting is not true just after connectDevice() as connectDevice is full async
     */
    bool isConnecting(const DeviceId& deviceId, const std::string& name) const;

    /**
     * Close all connections with a current device
     * @param peerUri      Peer URI
     */
    void closeConnectionsWith(const std::string& peerUri);

    /**
     * Method to call to listen to incoming requests
     * @param deviceId      Account's device
     */
    void onDhtConnected(const dht::crypto::PublicKey& devicePk);

    /**
     * Add a callback to decline or accept incoming ICE connections
     * @param cb    Callback to trigger
     */
    void onICERequest(onICERequestCallback&& cb);

    /**
     * Trigger cb on incoming peer channel
     * @param cb    Callback to trigger
     * @note        The callback is used to validate
     * if the incoming request is accepted or not.
     */
    void onChannelRequest(ChannelRequestCallback&& cb);

    /**
     * Trigger cb when connection with peer is ready
     * @param cb    Callback to trigger
     */
    void onConnectionReady(ConnectionReadyCallback&& cb);

    /**
     * @return the number of active sockets
     */
    std::size_t activeSockets() const;

    /**
     * Log informations for all sockets
     */
    void monitor() const;

    bool hasSwarmChannel(const DeviceId& deviceId, const std::string& convId);

    /**
     * Send beacon on peers supporting it
     */
    void connectivityChanged();

private:
    ConnectionManager() = delete;
    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami