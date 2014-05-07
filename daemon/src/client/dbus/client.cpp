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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include "client/client.h"
#include "global.h"
#include "manager.h"
#include "logger.h"
#include "instance.h"

#include "dbus_cpp.h"

#include "callmanager.h"
#include "configurationmanager.h"
#include "networkmanager.h"

#ifdef SFL_PRESENCE
#include "presencemanager.h"
#endif

#ifdef SFL_VIDEO
#include "videomanager.h"
#endif

struct EventCallback : DBus::Callback_Base<void, DBus::DefaultTimeout&>
{
    EventCallback(const std::function<void()> &func) :
        callback_(func)
    {}

    void call(DBus::DefaultTimeout &) const
    {
        callback_();
    }

private:
    std::function<void()> callback_;
};

Client::Client() : callManager_(0)
    , configurationManager_(0)
#ifdef SFL_PRESENCE
    , presenceManager_(0)
#endif
    , instanceManager_(0)
    , dispatcher_(new DBus::BusDispatcher)
#ifdef SFL_VIDEO
    , videoManager_(0)
#endif
#ifdef USE_NETWORKMANAGER
    , networkManager_(0)
#endif
{
    try {
        DEBUG("DBUS init threading");
        DBus::_init_threading();
        DEBUG("DBUS instantiate default dispatcher");
        DBus::default_dispatcher = dispatcher_;

        DEBUG("DBUS session connection to session bus");
        DBus::Connection sessionConnection(DBus::Connection::SessionBus());
        DEBUG("DBUS request org.sflphone.SFLphone from session connection");
        sessionConnection.request_name("org.sflphone.SFLphone");

        DEBUG("DBUS create call manager from session connection");
        callManager_ = new CallManager(sessionConnection);
        DEBUG("DBUS create configuration manager from session connection");
        configurationManager_ = new ConfigurationManager(sessionConnection);
        DEBUG("DBUS create presence manager from session connection");

#ifdef SFL_PRESENCE
        presenceManager_ = new PresenceManager(sessionConnection);
        DEBUG("DBUS create instance manager from session connection");
#endif
        instanceManager_ = new Instance(sessionConnection);

#ifdef SFL_VIDEO
        videoManager_ = new VideoManager(sessionConnection);
#endif

#ifdef USE_NETWORKMANAGER
        DEBUG("DBUS system connection to system bus");
        DBus::Connection systemConnection(DBus::Connection::SystemBus());
        DEBUG("DBUS create the network manager from the system bus");
        networkManager_ = new NetworkManager(systemConnection, "/org/freedesktop/NetworkManager", "");
#endif

    } catch (const DBus::Error &err) {
        ERROR("%s: %s, exiting\n", err.name(), err.what());
        ::exit(EXIT_FAILURE);
    }

    DEBUG("DBUS registration done");
    instanceManager_->started();
}

Client::~Client()
{
#ifdef USE_NETWORKMANAGER
    delete networkManager_;
#endif
#ifdef SFL_VIDEO
    delete videoManager_;
#endif
    delete instanceManager_;
#ifdef SFL_PRESENCE
    delete presenceManager_;
#endif
    delete configurationManager_;
    delete callManager_;
    delete dispatcher_;
}

void
Client::registerCallback(const std::function<void()> &callback)
{
        // timeout and expired are deleted internally by dispatcher_'s
        // destructor, so we must NOT delete them ourselves.
        DBus::DefaultTimeout *timeout = new DBus::DefaultTimeout(10 /* ms */,
                                                                 true,
                                                                 dispatcher_);
        timeout->expired = new EventCallback(callback);
}


int Client::event_loop()
{
    try {
        dispatcher_->enter();
    } catch (const DBus::Error &err) {
        ERROR("%s: %s, quitting\n", err.name(), err.what());
        return 1;
    } catch (const std::exception &err) {
        ERROR("%s: quitting\n", err.what());
        return 1;
    }

    return 0;
}

int Client::exit()
{
    try {
        dispatcher_->leave();
    } catch (const DBus::Error &err) {
        ERROR("%s: %s, quitting\n", err.name(), err.what());
        return 1;
    } catch (const std::exception &err) {
        ERROR("%s: quitting\n", err.what());
        return 1;
    }
    return 0;
}

CallManager *
Client::getCallManager()
{
    return callManager_;
}

ConfigurationManager *
Client::getConfigurationManager()
{
    return configurationManager_;
}

#ifdef SFL_PRESENCE
PresenceManager *
Client::getPresenceManager()
{
    return presenceManager_;
}
#endif

#ifdef SFL_VIDEO
VideoManager*
Client::getVideoManager()
{
    return videoManager_;
}
#endif

void
Client::onLastUnregister()
{
    if (not isPersistent_)
        exit();
}
