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
#include <vector>
#include <system_error>
#include <cstdint>

#if defined(_MSC_VER)
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

namespace ring {

template <typename T=uint8_t>
class GenericTransport
{
public:
    using ValueType = T;

    virtual ~GenericTransport() = default;

    using RecvCb = std::function<ssize_t(const ValueType* buf, std::size_t len)>;

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

    // TODO: make a std::chrono version
    virtual bool waitForData(unsigned ms_timeout) const = 0;

    /// Write a given amount of data
    virtual std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) {
        (void)buf; (void)len; (void)ec;
        ec = std::make_error_code(std::errc::function_not_supported);
        return 0;
    }

    /// write() adaptor for STL containers
    template <typename U>
    std::size_t write(const U& obj, std::error_code& ec) {
        return write(obj.data(), obj.size() * sizeof(typename U::value_type), ec);
    }

    /// Read a given amount of data
    virtual std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) {
        (void)buf; (void)len; (void)ec;
        ec = std::make_error_code(std::errc::function_not_supported);
        return 0;
    }

    /// read() adaptor for STL containers
    template <typename U>
    std::size_t read(U& storage, std::error_code& ec) {
        auto res = read(storage.data(), storage.size() * sizeof(typename U::value_type), ec);
        if (!ec)
            storage.resize(res);
        return res;
    }

    /// Helper
    std::size_t writeline(const ValueType* buf, std::size_t len, std::error_code& ec) {
        static const T endline = '\n';
        auto size = write(buf, len, ec);
        if (not ec) {
            write(&endline, 1, ec);
            if (not ec)
                return size+1;
        }
        return 0;
    }

    /// Helper
    ValueType* readline(ValueType* buf, std::size_t len, std::error_code& ec) {
        std::size_t read_count = 0;
        while (read_count < len) {
            T c;
            read(&c, 1, ec);
            if (!ec) {
                if (c == EOF)
                    break;
                buf[read_count++] = c;
                if (c == '\n')
                    return buf;
            } else
                break;
        }
        return nullptr;
    }

protected:
    GenericTransport() = default;
};

} // namespace ring
