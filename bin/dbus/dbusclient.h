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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "dring/def.h"
#include <memory>

class DBusConfigurationManager;
class DBusCallManager;
class DBusNetworkManager;
class DBusInstance;
class DBusPresenceManager;

#ifdef ENABLE_VIDEO
class DBusVideoManager;
#endif

namespace DBus {
    class BusDispatcher;
    class DefaultTimeout;
}

class DRING_PUBLIC DBusClient {
    public:
        DBusClient(int flags, bool persistent);
        ~DBusClient();

        int event_loop() noexcept;
        int exit() noexcept;

    private:
        int initLibrary(int flags);
        void finiLibrary() noexcept;

        std::unique_ptr<DBus::BusDispatcher>  dispatcher_;
        std::unique_ptr<DBus::DefaultTimeout> timeout_;

        std::unique_ptr<DBusCallManager>          callManager_;
        std::unique_ptr<DBusConfigurationManager> configurationManager_;
        std::unique_ptr<DBusPresenceManager>      presenceManager_;
        std::unique_ptr<DBusInstance>             instanceManager_;

#ifdef ENABLE_VIDEO
        std::unique_ptr<DBusVideoManager>         videoManager_;
#endif
};
