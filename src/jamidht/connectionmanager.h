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


namespace jami {

class JamiAccount;

struct PeerConnectionRequest : public dht::EncryptedValue<PeerConnectionRequest>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr const char* key_prefix = "peer:"; ///< base to compute the DHT listen key
    std::string uri {};
    std::string ice_msg {};
    bool isResponse {false};
    MSGPACK_DEFINE_MAP(uri, ice_msg, isResponse)
};

struct PeerSocket {
    // TODO link multiplexed socket
};

using ConnectCallback = std::function<void(std::unique_ptr<PeerSocket>)>;
using PeerRequestCallBack = std::function<bool(const std::string&, const std::string&)>;
using ConnectionReadyCallBack = std::function<void(const std::string&, const std::string&, std::unique_ptr<PeerSocket>)>;

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