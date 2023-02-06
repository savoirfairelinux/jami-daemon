/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "connectivity/ip_utils.h"

#include <functional>
#include <memory>
#include <string>

namespace jami {

struct TurnTransportParams
{
    IpAddr server;
    std::string domain; // Used by cache_turn
    // Plain Credentials
    std::string realm;
    std::string username;
    std::string password;
};

/**
 * This class is used to test connection to TURN servers
 * No other logic is implemented.
 */
class TurnTransport
{
public:
    TurnTransport(const TurnTransportParams& param, std::function<void(bool)>&& cb);
    ~TurnTransport();
    void shutdown();

private:
    TurnTransport() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
