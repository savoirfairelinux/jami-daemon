/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#include "dbusinstance.adaptor.h"
#include <sdbus-c++/IConnection.h>
#include <cstdint>
#include <memory>

extern bool persistent;
extern std::unique_ptr<sdbus::IConnection> connection;

class DBusInstance : public sdbus::AdaptorInterfaces<cx::ring::Ring::Instance_adaptor>
{
public:
    DBusInstance(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, "/cx/ring/Ring/Instance")
    {
        registerAdaptor();
    }

    ~DBusInstance()
    {
        unregisterAdaptor();
    }

    void
    Register(const int32_t& /*pid*/, const std::string& /*name*/)
    {
        ++count_;
    }

    void
    Unregister(const int32_t& /*pid*/)
    {
        --count_;

        if (!persistent && count_ <= 0) {
            connection->leaveEventLoop();
        }
    }

private:
    int_least16_t count_ {0};
};
