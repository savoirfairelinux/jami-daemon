/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include "generic_io.h"

#include <memory>
#include <functional>

#if defined(_MSC_VER)
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

namespace jami {

class IceTransport;
using IceRecvCb = std::function<ssize_t(unsigned char* buf, size_t len)>;

class IceSocket
{
    private:
        std::shared_ptr<IceTransport> ice_transport_ {};
        int compId_ = -1;

    public:
        IceSocket(std::shared_ptr<IceTransport> iceTransport, int compId)
            : ice_transport_(std::move(iceTransport)), compId_(compId) {}

        void close();
        ssize_t recv(unsigned char* buf, size_t len);
        ssize_t send(const unsigned char* buf, size_t len);
        ssize_t waitForData(unsigned int timeout);
        void setOnRecv(IceRecvCb cb);
        uint16_t getTransportOverhead();
};

/// ICE transport as a GenericSocket.
///
/// \warning Simplified version where we assume that ICE protocol
/// always use UDP over IP over ETHERNET, and doesn't add more header to the UDP payload.
///
class IceSocketTransport final : public GenericSocket<uint8_t>
{
public:
    using SocketType = GenericSocket<uint8_t>;

    static constexpr uint16_t STANDARD_MTU_SIZE = 1280; // Size in bytes of MTU for IPv6 capable networks
    static constexpr uint16_t IPV6_HEADER_SIZE = 40; // Size in bytes of IPv6 packet header
    static constexpr uint16_t IPV4_HEADER_SIZE = 20; // Size in bytes of IPv4 packet header
    static constexpr uint16_t UDP_HEADER_SIZE = 8; // Size in bytes of UDP header

    IceSocketTransport(std::shared_ptr<IceTransport>& ice, int comp_id, bool reliable = false)
        : compId_ {comp_id}
        , ice_ {ice}
        , reliable_ {reliable} {}

    bool isReliable() const override {
        return reliable_;
    }

    bool isInitiator() const override;

    int maxPayload() const override;

    int waitForData(unsigned ms_timeout, std::error_code& ec) const override;

    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override;

    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override;

    void setOnRecv(RecvCb&& cb) override;

    IpAddr localAddr() const override;

    IpAddr remoteAddr() const override;

private:
    const int compId_;
    std::shared_ptr<IceTransport> ice_;
    bool reliable_;
};

};
