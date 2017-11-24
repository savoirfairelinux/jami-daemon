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

#include <functional>
#include <system_error>
#include <cstdint>

#if defined(_MSC_VER)
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

namespace ring {

class GenericTransport
{
public:
    using RecvCb = std::function<ssize_t(uint8_t* buf, std::size_t len)>;

    virtual bool isReliable() const = 0;
    virtual bool isInitiator() const = 0;

    /// Return maximum application payload size.
    /// This value is negative if the session is not ready to give a valid answer.
    /// The value is 0 if such information is irrelevant for the session.
    /// If stricly positive, the user must use send() with an input buffer size below or equals
    /// to this value if it want to be sure that the transport sent it in an atomic way.
    /// Example: in case of non-reliable transport using packet oriented IO,
    /// this value gives the maximal size used to send one packet.
    virtual int maxPayload() const = 0;

    virtual void setOnRecv(RecvCb&& cb) = 0;
    virtual std::size_t send(const void* buf, std::size_t len, std::error_code& ec) = 0;

    /// Works as send(data, size, ec) but with C++ standard continuous buffer containers (like vector and array).
    template <typename T>
    std::size_t send(const T& buffer, std::error_code& ec) {
        return send(buffer.data(),
                    buffer.size() * sizeof(decltype(buffer)::value_type),
                    ec);
    }

protected:
    GenericTransport() = default;
    virtual ~GenericTransport() = default;
};

} // namespace ring
