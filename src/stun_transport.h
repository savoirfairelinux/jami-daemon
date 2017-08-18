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

#include "noncopyable.h"
#include "ip_utils.h"

#include <string>
#include <memory>

namespace ring {

class StunTransportPimpl;

struct StunTransportParams {
    IpAddr bound_addr;
};

class StunTransport {
public:
    ///
    /// Constructs a StunTransport connected by TCP to given server.
    ///
    /// Throw std::invalid_argument of peer address is invalid.
    ///
    /// \param server IpAddr representing the stun server
    ///
    /// \note If port is not set, the default STUN port 3478 (rfc5389) is used.
    ///
    StunTransport(const StunTransportParams& params);

    ~StunTransport();

    void connect(const IpAddr& server);

    ///
    /// Wait for successful connection and binding to STUN server.
    ///
    /// StunTransport constructor connects asynchronously on the STUN server.
    /// You need to wait the READY state before calling any other APIs.
    ///
    void waitBinding();

    const IpAddr& getMappedAddr() const;

    ///
    /// Collect pending data.
    ///
    void recvfrom(std::pair<IpAddr, std::vector<uint8_t>>& result);

    ///
    /// Send data to a given peer through the STUN tunnel.
    ///
    bool sendto(const IpAddr& peer, const std::vector<uint8_t>& data);

    // Move semantic
    StunTransport(StunTransport&&) = default;
    StunTransport& operator=(StunTransport&&) = default;

private:
    NON_COPYABLE(StunTransport);
    StunTransport() = delete;

    std::unique_ptr<StunTransportPimpl> pimpl_;
};

} // namespace ring
