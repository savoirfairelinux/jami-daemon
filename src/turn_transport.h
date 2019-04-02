/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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
#include "generic_io.h"

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <stdexcept>

namespace jami {

class TurnTransportPimpl;

struct TurnTransportParams
{
    IpAddr server;

    // Plain Credentials
    std::string realm;
    std::string username;
    std::string password;

    pj_uint16_t authorized_family {0};

    bool isPeerConnection {false};
    uint32_t connectionId {0};
    std::function<void(uint32_t conn_id, const IpAddr& peer_addr, bool success)> onPeerConnection;

    std::size_t maxPacketSize {4096}; ///< size of one "logical" packet
};

class TurnTransport
{
public:
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

    void shutdown(const IpAddr& addr);

    bool isInitiator() const;

    /// Wait for successful connection on the TURN server.
    ///
    /// TurnTransport constructor connects asynchronously on the TURN server.
    /// You need to wait the READY state before calling any other APIs.
    ///
    void waitServerReady();

    /// \return true if the TURN server is connected and ready to accept peers.
    bool isReady() const;

    /// \return socket address (IP/port) where peers should connect to before doing IO with this client.
    const IpAddr& peerRelayAddr() const;

    /// \return public address of this client as seen by the TURN server.
    const IpAddr& mappedAddr() const;

    /// \return a vector of connected peer addresses
    std::vector<IpAddr> peerAddresses() const;

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

    /// Collect pending data from a given peer.
    ///
    /// Data are read from given \a peer incoming buffer until EOF or \a data size() is reached.
    /// \a data is resized with exact number of characters read.
    /// If \a peer is not connected this function raise an exception.
    /// If \a peer exists but no data are available this method blocks until TURN deconnection
    /// or at first incoming character.
    ///
    /// \param [in] peer target peer address where data are read
    /// \param [in,out] pre-dimensionned character vector to write incoming data
    /// \exception std::out_of_range \a peer is not connected yet
    ///
    void recvfrom(const IpAddr& peer, std::vector<char>& data);

    /// Works as recvfrom() vector version but accept a simple char array.
    ///
    std::size_t recvfrom(const IpAddr& peer, char* buffer, std::size_t size);

    /// Send data to given peer through the TURN tunnel.
    ///
    /// This method blocks until all given characters in \a data are sent to the given \a peer.
    /// If \a peer is not connected this function raise an exception.
    ///
    /// \param [in] peer target peer address where data are read
    /// \param [in,out] pre-dimensionned character vector to write incoming data
    /// \exception std::out_of_range \a peer is not connected yet
    ///
    bool sendto(const IpAddr& peer, const std::vector<char>& data);

    /// Works as sendto() vector version but accept a simple char array.
    ///
    bool sendto(const IpAddr& peer, const char* const buffer, std::size_t size);

    int waitForData(const IpAddr& peer, unsigned ms_timeout, std::error_code& ec) const;

public:
    // Move semantic only, not copiable
    TurnTransport(TurnTransport&&) = default;
    TurnTransport& operator=(TurnTransport&&) = default;

private:
    TurnTransport() = delete;
    std::unique_ptr<TurnTransportPimpl> pimpl_;
};

class ConnectedTurnTransport final : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;

    ConnectedTurnTransport(TurnTransport& turn, const IpAddr& peer);

    void shutdown() override;
    bool isReliable() const override { return true; }
    bool isInitiator() const override { return turn_.isInitiator(); }
    int maxPayload() const override { return 3000; }

    int waitForData(unsigned ms_timeout, std::error_code& ec) const override;
    std::size_t read(ValueType* buf, std::size_t length, std::error_code& ec) override;
    std::size_t write(const ValueType* buf, std::size_t length, std::error_code& ec) override;

    void setOnRecv(RecvCb&&) override { throw std::logic_error("ConnectedTurnTransport bad call"); }

private:
    TurnTransport& turn_;
    const IpAddr peer_;
    RecvCb onRxDataCb_;
};

} // namespace jami
