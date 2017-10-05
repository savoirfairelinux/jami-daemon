/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "ip_utils.h"

#include <string>
#include <memory>
#include <functional>
#include <map>

namespace ring {

class TurnTransportPimpl;

struct TurnTransportParams {
    IpAddr server;

    // Plain Credentials
    std::string realm;
    std::string username;
    std::string password;

    bool isPeerConnection {false};
    uint32_t connectionId {0};
    std::function<void(uint32_t conn_id, const IpAddr& peer_addr)> onPeerConnection;

    std::size_t maxPacketSize {3000}; ///< size of one "logical" packet
};

class TurnTransport {
public:
    ///
    /// Constructs a TurnTransport connected by TCP to given server.
    ///
    /// Throw std::invalid_argument of peer address is invalid.
    ///
    /// \param param parameters to setup the transport
    ///
    /// \note If TURN server port is not set, the default TURN port 3478 (RFC5766) is used.
    ///
    TurnTransport(const TurnTransportParams& param);

    ~TurnTransport();

    ///
    /// Wait for successful connection on the TURN server.
    ///
    /// TurnTransport constructor connects asynchronously on the TURN server.
    /// You need to wait the READY state before calling any other APIs.
    ///
    void waitServerReady();

    bool isReady() const;

    const IpAddr& peerRelayAddr() const;
    const IpAddr& mappedAddr() const;

    ///
    /// Gives server access permission to given peer by its address.
    ///
    /// Throw std::invalid_argument of peer address is invalid.
    /// Throw std::runtime_error if case of backend errors.
    ///
    /// \param addr peer address
    ///
    /// \note The peer address family must be same as the turn server.
    /// \note Must be called only if server is ready.
    /// \see waitServerReady
    ///
    void permitPeer(const IpAddr& addr);

    ///
    /// Collect pending data from a given peer
    ///
    void recvfrom(const IpAddr& peer, std::vector<char>& data);

    void readlinefrom(const IpAddr& peer, std::vector<char>& data);

    ///
    /// Send data to a given peer through the TURN tunnel.
    ///
    bool sendto(const IpAddr& peer, const std::vector<char>& data);

    bool sendto(const IpAddr& peer, const char* const buffer, std::size_t length);

    bool writelineto(const IpAddr& peer, const char* const buffer, std::size_t length);

public:
    // Move semantic
    TurnTransport(TurnTransport&&) = default;
    TurnTransport& operator=(TurnTransport&&) = default;

private:
    TurnTransport() = delete;
    std::unique_ptr<TurnTransportPimpl> pimpl_;
};

} // namespace ring
