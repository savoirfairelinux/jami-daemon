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

// Ring
#include "net_port.h"
#include "logger.h"

// Std
#include <mutex>
#include <random>
#include <type_traits>
#include <array>

namespace ring { namespace Net {

static std::mutex&
getPortsReservationMutex() noexcept
{
    static std::mutex mutex;
    return mutex;
}

using PortArray = std::array<bool, MAX_PORT / 2>;

static PortArray&
getPortsReservation() noexcept
{
    // Note: static arrays are zero-initialized
    static PortArray portsInUse;
    return portsInUse;
}

static std::mt19937_64&
getRandomEngine() noexcept
{
    static std::random_device rdev;
    static std::seed_seq seed {rdev(), rdev()};
    static std::mt19937_64 randengine {seed};
    return randengine;
}

port_t
acquirePort(port_t port)
{
    std::lock_guard<std::mutex> lk {getPortsReservationMutex()};
    RING_ERR("acquirePort port %d", port);
    getPortsReservation()[port / 2] = true;
    return port;
}

void
releasePort(port_t port)
{
    std::lock_guard<std::mutex> lk {getPortsReservationMutex()};
    RING_ERR("releasePort port %d", port);
    getPortsReservation()[port / 2] = false;
}

UniquePort
getRandomEvenPort(const PortRange& range)
{
    const port_t a = range.first / 2;
    const port_t b = range.second / 2;
    std::uniform_int_distribution<port_t> dist {a, b};
    port_t result;
    do {
        result = 2 * dist(getRandomEngine());
    } while (getPortsReservation()[result / 2]);
    return result;
}

UniquePort
acquireRandomEvenPort(const PortRange& range)
{
    const port_t a = range.first / 2;
    const port_t b = range.second / 2;
    std::uniform_int_distribution<port_t> dist {a, b};
    port_t result;
    do {
        result = 2 * dist(getRandomEngine());
    } while (acquirePort(result) != result);
    return result;
}

}} // namespace ring::Net
