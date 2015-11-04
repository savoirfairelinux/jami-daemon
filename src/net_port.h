/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

// Ring
#include "noncopyable.h"

// Std
#include <utility>
#include <cstdint>
#include <netinet/in.h>

namespace ring { namespace Net {

constexpr static unsigned MAX_PORT {65536};

using port_t = in_port_t;
using PortRange = std::pair<port_t, port_t>;

// Low-level API
port_t acquirePort(port_t port);
void releasePort(port_t port);

// RAII class arroud acquirePort/releasePort API
// This implementation doesn't protect against concurrent access on instances
class UniquePort
{
public:
    UniquePort() = default;

    UniquePort(UniquePort&& o) = default;

    UniquePort(port_t p) {
        if (p)
            port_ = acquirePort(p);
    }

    ~UniquePort() noexcept {
        release();
    }

    void release() noexcept {
        if (port_) {
            releasePort(port_);
            port_ = 0;
        }
    }

    void swap(UniquePort& o) noexcept {
        std::swap(port_, o.port_);
    }

    UniquePort& operator=(UniquePort&& o) noexcept {
        if (port_ != o.port_) {
            swap(o);
            o.release();
        }
        return *this;
    }

#if 0
    UniquePort& operator=(port_t p) {
        if (port_ != p) {
            if (p)
                p = acquirePort(p);
            release();
            port_ = p;
        }
        return *this;
    }
#endif

    operator port_t() const noexcept {
        return port_;
    }

    explicit operator bool() const noexcept {
        return port_ != 0;
    }

private:
    NON_COPYABLE(UniquePort);
    port_t port_ {0};
};

// Generators
UniquePort getRandomEvenPort(const PortRange& range);
UniquePort acquireRandomEvenPort(const PortRange& range);

}} // namespace ring::Net
