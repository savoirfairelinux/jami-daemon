/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <sflphone.h>

#include "dbusclient.h"
#include "dbus_cpp.h"

#include "dbusinstance.h"
#include "dbuscallmanager.h"
#include "dbusconfigurationmanager.h"

#ifdef SFL_PRESENCE
#include "dbuspresencemanager.h"
#endif

#ifdef SFL_VIDEO
#include "dbusvideomanager.h"
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

DBusClient::DBusClient(int sflphFlags, bool persistent) :
    callManager_(nullptr)
    , configurationManager_(nullptr)
#ifdef SFL_PRESENCE
    , presenceManager_(nullptr)
#endif
    , instanceManager_(nullptr)
    , dispatcher_(new DBus::BusDispatcher)
#ifdef SFL_VIDEO
    , videoManager_(nullptr)
#endif
    , timeout_(nullptr)
{
    try {
        DBus::_init_threading();
        DBus::default_dispatcher = dispatcher_;

        // timeout and expired are deleted internally by dispatcher_'s
        // destructor, so we must NOT delete them ourselves.
        timeout_ = new DBus::DefaultTimeout(10 /* ms */, true, dispatcher_);
        // Poll for SIP/IAX events
        timeout_->expired = new EventCallback(sflph_poll_events);

        DBus::Connection sessionConnection(DBus::Connection::SessionBus());
        sessionConnection.request_name("org.sflphone.SFLphone");

        callManager_ = new DBusCallManager(sessionConnection);
        configurationManager_ = new DBusConfigurationManager(sessionConnection);

#ifdef SFL_PRESENCE
        presenceManager_ = new DBusPresenceManager(sessionConnection);
#endif

        DBusInstance::OnNoMoreClientFunc onNoMoreClientFunc;

        if (!persistent) {
            onNoMoreClientFunc = [this] () {
                this->exit();
            };
        }

        instanceManager_ = new DBusInstance(sessionConnection, onNoMoreClientFunc);

#ifdef SFL_VIDEO
        videoManager_ = new DBusVideoManager(sessionConnection);
#endif
    } catch (const DBus::Error &err) {
        throw std::runtime_error("cannot initialize DBus stuff");
    }

    auto ret = initLibrary(sflphFlags);

    if (ret < 0) {
        throw std::runtime_error("cannot initialize libsflphone");
    }

    instanceManager_->started();
}

DBusClient::~DBusClient()
{
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

int DBusClient::initLibrary(int sflphFlags)
{
    using namespace std::placeholders;
    using std::bind;

    auto callM = callManager_; // just an alias

    // Call event handlers
    sflph_call_ev_handlers callEvHandlers = {
        bind(&DBusCallManager::callStateChanged, callM, _1, _2),
        bind(&DBusCallManager::transferFailed, callM),
        bind(&DBusCallManager::transferSucceeded, callM),
        bind(&DBusCallManager::recordPlaybackStopped, callM, _1),
        bind(&DBusCallManager::voiceMailNotify, callM, _1, _2),
        bind(&DBusCallManager::incomingMessage, callM, _1, _2, _3),
        bind(&DBusCallManager::incomingCall, callM, _1, _2, _3),
        bind(&DBusCallManager::recordPlaybackFilepath, callM, _1, _2),
        bind(&DBusCallManager::conferenceCreated, callM, _1),
        bind(&DBusCallManager::conferenceChanged, callM, _1, _2),
        bind(&DBusCallManager::updatePlaybackScale, callM, _1, _2, _3),
        bind(&DBusCallManager::conferenceRemoved, callM, _1),
        bind(&DBusCallManager::newCallCreated, callM, _1, _2, _3),
        bind(&DBusCallManager::sipCallStateChanged, callM, _1, _2, _3),
        bind(&DBusCallManager::recordingStateChanged, callM, _1, _2),
        bind(&DBusCallManager::secureSdesOn, callM, _1),
        bind(&DBusCallManager::secureSdesOff, callM, _1),
        bind(&DBusCallManager::secureZrtpOn, callM, _1, _2),
        bind(&DBusCallManager::secureZrtpOff, callM, _1),
        bind(&DBusCallManager::showSAS, callM, _1, _2, _3),
        bind(&DBusCallManager::zrtpNotSuppOther, callM, _1),
        bind(&DBusCallManager::zrtpNegotiationFailed, callM, _1, _2, _3),
        bind(&DBusCallManager::onRtcpReportReceived, callM, _1, _2)
    };

    auto confM = configurationManager_; // just an alias

    // Configuration event handlers
    sflph_config_ev_handlers configEvHandlers = {
        bind(&DBusConfigurationManager::volumeChanged, confM, _1, _2),
        bind(&DBusConfigurationManager::accountsChanged, confM),
        bind(&DBusConfigurationManager::historyChanged, confM),
        bind(&DBusConfigurationManager::stunStatusFailure, confM, _1),
        bind(&DBusConfigurationManager::registrationStateChanged, confM, _1, _2),
        bind(&DBusConfigurationManager::sipRegistrationStateChanged, confM, _1, _2, _3),
        bind(&DBusConfigurationManager::errorAlert, confM, _1),
    };

#ifdef SFL_PRESENCE
    auto presM = presenceManager_;
    // Presence event handlers
    sflph_pres_ev_handlers presEvHandlers = {
        bind(&DBusPresenceManager::newServerSubscriptionRequest, presM, _1),
        bind(&DBusPresenceManager::serverError, presM, _1, _2, _3),
        bind(&DBusPresenceManager::newBuddyNotification, presM, _1, _2, _3, _4),
        bind(&DBusPresenceManager::subscriptionStateChanged, presM, _1, _2, _3)
    };
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    auto videoM = videoManager_;

    // Video event handlers
    sflph_video_ev_handlers videoEvHandlers = {
        bind(&DBusVideoManager::deviceEvent, videoM),
        bind(&DBusVideoManager::startedDecoding, videoM, _1, _2, _3, _4, _5),
        bind(&DBusVideoManager::stoppedDecoding, videoM, _1, _2, _3)
    };
#endif // SFL_VIDEO

    // All event handlers
    sflph_ev_handlers evHandlers = {};

    evHandlers.call_ev_handlers = callEvHandlers;
    evHandlers.config_ev_handlers = configEvHandlers;

#ifdef SFL_PRESENCE
    evHandlers.pres_ev_handlers = presEvHandlers;
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    evHandlers.video_ev_handlers = videoEvHandlers;
#endif // SFL_VIDEO

    // Initialize now
    return sflph_init(&evHandlers, static_cast<sflph_init_flag>(sflphFlags));
}

void DBusClient::finiLibrary()
{
    sflph_fini();
}

int DBusClient::event_loop()
{
    try {
        dispatcher_->enter();
    } catch (const DBus::Error &err) {
        std::cerr << "quitting: " << err.name() << ": " << err.what() << std::endl;
        return 1;
    } catch (const std::exception &err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}

int DBusClient::exit()
{
    try {
        dispatcher_->leave();
        timeout_->expired = new EventCallback([] {});
        finiLibrary();
    } catch (const DBus::Error &err) {
        std::cerr << "quitting: " << err.name() << ": " << err.what() << std::endl;
        return 1;
    } catch (const std::exception &err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
