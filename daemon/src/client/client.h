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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "noncopyable.h"

class ConfigurationManager;
class CallManager;
class NetworkManager;
class Instance;

#ifdef SFL_PRESENCE
class PresenceManager;
#endif

#ifdef SFL_VIDEO
class VideoManager;
#endif

#if HAVE_DBUS
namespace DBus {
    class BusDispatcher;
}

#include <functional>

#endif

class Client {
    public:
        Client();
        ~Client();

        CallManager * getCallManager();

        ConfigurationManager * getConfigurationManager();

#ifdef SFL_PRESENCE
        PresenceManager * getPresenceManager();
#endif

#ifdef SFL_VIDEO
        VideoManager* getVideoManager();
#endif

        int event_loop();
        int exit();
#ifdef HAVE_DBUS
        // DBus provides our event loop
        void registerCallback(const std::function<void()> &callback);
#endif

#if HAVE_DBUS
        void onLastUnregister();
        void setPersistent(bool p) { isPersistent_ = p; }
#endif

    private:
        NON_COPYABLE(Client);
        CallManager*          callManager_;
        ConfigurationManager* configurationManager_;
#ifdef SFL_PRESENCE
        PresenceManager*      presenceManager_;
#endif
#if HAVE_DBUS
        Instance*             instanceManager_;
        DBus::BusDispatcher*  dispatcher_;
        bool isPersistent_ = false;
#endif
#ifdef SFL_VIDEO
        VideoManager *videoManager_;
#endif
#if USE_NETWORKMANAGER
        NetworkManager* networkManager_;
#endif
};

#endif
