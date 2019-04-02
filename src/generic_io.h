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

#include <functional>
#include <vector>
#include <system_error>
#include <cstdint>

#if defined(_MSC_VER)
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

namespace jami {

template <typename T>
class GenericSocket
{
public:
    using ValueType = T;

    virtual ~GenericSocket() { shutdown(); }

    using RecvCb = std::function<ssize_t(const ValueType* buf, std::size_t len)>;

    /// Close established connection
    /// \note Terminate outstanding blocking read operations with an empty error code, but a 0 read size.
    virtual void shutdown() {}

    /// Set Rx callback
    /// \warning This method is here for backward compatibility
    /// and because async IO are not implemented yet.
    virtual void setOnRecv(RecvCb&& cb) = 0;

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

    /// Wait until data to read available, timeout or io error
    /// \param ec error code set in case of error (if return value is < 0)
    /// \return positive number if data ready for read, 0 in case of timeout or error.
    /// \note error code is not set in case of timeout, but set only in case of io error
    /// (i.e. socket deconnection).
    /// \todo make a std::chrono version for the timeout
    virtual int waitForData(unsigned ms_timeout, std::error_code& ec) const = 0;

    /// Write a given amount of data.
    /// \param buf data to write.
    /// \param len number of bytes to write.
    /// \param ec error code set in case of error.
    /// \return number of bytes written, 0 is valid.
    /// \warning error checking consists in checking if \a !ec is true, not if returned size is 0
    /// as a write of 0 could be considered a valid operation.
    virtual std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) = 0;

    /// Read a given amount of data.
    /// \param buf data to read.
    /// \param len number of bytes to read.
    /// \param ec error code set in case of error.
    /// \return number of bytes read, 0 is valid.
    /// \warning error checking consists in checking if \a !ec is true, not if returned size is 0
    /// as a read of 0 could be considered a valid operation (i.e. non-blocking IO).
    virtual std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) = 0;

    /// write() adaptor for STL containers
    template <typename U>
    std::size_t write(const U& obj, std::error_code& ec) {
        return write(obj.data(), obj.size() * sizeof(typename U::value_type), ec);
    }

    /// read() adaptor for STL containers
    template <typename U>
    std::size_t read(U& storage, std::error_code& ec) {
        auto res = read(storage.data(), storage.size() * sizeof(typename U::value_type), ec);
        if (!ec)
            storage.resize(res);
        return res;
    }

    /// Return the local IP address if known.
    /// \note The address is not valid (addr.isUnspecified() returns true) if it's not known
    /// or not available.
    virtual IpAddr localAddr() const { return {}; }

    /// Return the remote IP address if known.
    /// \note The address is not valid (addr.isUnspecified() returns true) if it's not known
    /// or not available.
    virtual IpAddr remoteAddr() const { return {}; }

protected:
    GenericSocket() = default;
};

} // namespace jami
