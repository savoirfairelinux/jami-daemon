/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
#include <sdbus-c++/sdbus-c++.h>
#include "dbusdaemon1.hpp"
#include <memory>

class DBusClient {
public:
    DBusClient(int flags, bool persistent);
    ~DBusClient();

    void event_loop();
    void exit();

protected:
    void onNumberOfClientsChanged(uint_fast16_t newNumberOfClients);

private:
    int initLibrary(int flags);

    std::unique_ptr<sdbus::IConnection> connection_;
    std::unique_ptr<DBusDaemon1> daemon_;
};
