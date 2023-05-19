/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "connectivity/generic_io.h"

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
        : ice_transport_(std::move(iceTransport))
        , compId_(compId)
    {}

    void close();
    ssize_t send(const unsigned char* buf, size_t len);
    ssize_t waitForData(std::chrono::milliseconds timeout);
    void setOnRecv(IceRecvCb cb);
    uint16_t getTransportOverhead();
    void setDefaultRemoteAddress(const IpAddr& addr);
    int getCompId() const { return compId_; };
};

}; // namespace jami
