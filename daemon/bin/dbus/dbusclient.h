/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __DBUSCLIENT_H__
#define __DBUSCLIENT_H__

#include <sflphone.h>

class DBusConfigurationManager;
class DBusCallManager;
class DBusNetworkManager;
class DBusInstance;

#ifdef SFL_PRESENCE
class DBusPresenceManager;
#endif

#ifdef SFL_VIDEO
class DBusVideoManager;
#endif

namespace DBus {
    class BusDispatcher;
    class DefaultTimeout;
}

class DBusClient {
    public:
        DBusClient(int sflphFlags, bool persistent);
        ~DBusClient();

        int event_loop();
        int exit();

    private:

        int initLibrary(int sflphFlags);
        void finiLibrary();

        DBusCallManager*          callManager_;
        DBusConfigurationManager* configurationManager_;

#ifdef SFL_PRESENCE
        DBusPresenceManager*      presenceManager_;
#endif

        DBusInstance*             instanceManager_;
        DBus::BusDispatcher*  dispatcher_;

#ifdef SFL_VIDEO
        DBusVideoManager *videoManager_;
#endif
        DBus::DefaultTimeout *timeout_;
};

#endif
