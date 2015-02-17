/*
 *  Copyright (C) 2014-2015 Savoir-Faire Linux Inc.
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "dbusclient.h"
#include "dbus_cpp.h"

#include "dbusinstance.h"

#include "callmanager_interface.h"
#include "dbuscallmanager.h"

#include "dbusconfigurationmanager.h"
#include "configurationmanager_interface.h"

#include "dbuspresencemanager.h"
#include "presencemanager_interface.h"

#ifdef RING_VIDEO
#include "dbusvideomanager.h"
#include "videomanager_interface.h"
#endif

class EventCallback :
    public DBus::Callback_Base<void, DBus::DefaultTimeout&>
{
    public:
        EventCallback(const std::function<void()>&& func)
            : callback_ {std::forward<const std::function<void()>>(func)}
            {}

        void call(DBus::DefaultTimeout&) const {
            callback_();
        }

    private:
        const std::function<void()> callback_;
};

DBusClient::DBusClient(int sflphFlags, bool persistent)
    : dispatcher_(new DBus::BusDispatcher)
{
    try {
        DBus::_init_threading();
        DBus::default_dispatcher = dispatcher_.get();

        // timeout and expired are deleted internally by dispatcher_'s
        // destructor, so we must NOT delete them ourselves.
        timeout_.reset(new DBus::DefaultTimeout {10 /* ms */, true, dispatcher_.get()});
        // Poll for Deamon events
        timeout_->expired = new EventCallback {DRing::poll_events};

        DBus::Connection sessionConnection {DBus::Connection::SessionBus()};
        sessionConnection.request_name("cx.ring.Ring");

        callManager_.reset(new DBusCallManager {sessionConnection});
        configurationManager_.reset(new DBusConfigurationManager {sessionConnection});
        presenceManager_.reset(new DBusPresenceManager {sessionConnection});

        DBusInstance::OnNoMoreClientFunc onNoMoreClientFunc;
        if (!persistent)
            onNoMoreClientFunc = [this] {this->exit();};

        instanceManager_.reset(new DBusInstance {sessionConnection, onNoMoreClientFunc});

#ifdef RING_VIDEO
        videoManager_.reset(new DBusVideoManager {sessionConnection});
#endif
    } catch (const DBus::Error &err) {
        throw std::runtime_error {"cannot initialize DBus stuff"};
    }

    if (initLibrary(sflphFlags) < 0)
        throw std::runtime_error {"cannot initialize libring"};

    instanceManager_->started();
}

DBusClient::~DBusClient()
{
    dispatcher_.reset(); // force dispatcher reset first
}

int DBusClient::initLibrary(int sflphFlags)
{
    using namespace std::placeholders;

    using std::bind;
    using DRing::exportable_callback;
    using DRing::CallSignal;
    using DRing::ConfigurationSignal;
    using DRing::PresenceSignal;
    using DRing::VideoSignal;
    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

    auto callM = callManager_.get();
    auto confM = configurationManager_.get();
    auto presM = presenceManager_.get();
    auto videoM = videoManager_.get();

    const std::map<DRing::EventHandlerKey, std::map<std::string, SharedCallback>> evHandlers = {
        { // Call event handlers
            DRing::EventHandlerKey::CALL, {
                exportable_callback<CallSignal::StateChange>(bind(&DBusCallManager::callStateChanged, callM, _1, _2)),
                exportable_callback<CallSignal::TransferFailed>(bind(&DBusCallManager::transferFailed, callM)),
                exportable_callback<CallSignal::TransferSucceeded>(bind(&DBusCallManager::transferSucceeded, callM)),
                exportable_callback<CallSignal::RecordPlaybackStopped>(bind(&DBusCallManager::recordPlaybackStopped, callM, _1)),
                exportable_callback<CallSignal::VoiceMailNotify>(bind(&DBusCallManager::voiceMailNotify, callM, _1, _2)),
                exportable_callback<CallSignal::IncomingMessage>(bind(&DBusCallManager::incomingMessage, callM, _1, _2, _3)),
                exportable_callback<CallSignal::IncomingCall>(bind(&DBusCallManager::incomingCall, callM, _1, _2, _3)),
                exportable_callback<CallSignal::RecordPlaybackFilepath>(bind(&DBusCallManager::recordPlaybackFilepath, callM, _1, _2)),
                exportable_callback<CallSignal::ConferenceCreated>(bind(&DBusCallManager::conferenceCreated, callM, _1)),
                exportable_callback<CallSignal::ConferenceChanged>(bind(&DBusCallManager::conferenceChanged, callM, _1, _2)),
                exportable_callback<CallSignal::UpdatePlaybackScale>(bind(&DBusCallManager::updatePlaybackScale, callM, _1, _2, _3)),
                exportable_callback<CallSignal::ConferenceRemoved>(bind(&DBusCallManager::conferenceRemoved, callM, _1)),
                exportable_callback<CallSignal::NewCallCreated>(bind(&DBusCallManager::newCallCreated, callM, _1, _2, _3)),
                exportable_callback<CallSignal::SipCallStateChanged>(bind(&DBusCallManager::sipCallStateChanged, callM, _1, _2, _3)),
                exportable_callback<CallSignal::RecordingStateChanged>(bind(&DBusCallManager::recordingStateChanged, callM, _1, _2)),
                exportable_callback<CallSignal::SecureSdesOn>(bind(&DBusCallManager::secureSdesOn, callM, _1)),
                exportable_callback<CallSignal::SecureSdesOff>(bind(&DBusCallManager::secureSdesOff, callM, _1)),
                exportable_callback<CallSignal::SecureZrtpOn>(bind(&DBusCallManager::secureZrtpOn, callM, _1, _2)),
                exportable_callback<CallSignal::SecureZrtpOff>(bind(&DBusCallManager::secureZrtpOff, callM, _1)),
                exportable_callback<CallSignal::ShowSAS>(bind(&DBusCallManager::showSAS, callM, _1, _2, _3)),
                exportable_callback<CallSignal::ZrtpNotSuppOther>(bind(&DBusCallManager::zrtpNotSuppOther, callM, _1)),
                exportable_callback<CallSignal::ZrtpNegotiationFailed>(bind(&DBusCallManager::zrtpNegotiationFailed, callM, _1, _2, _3)),
                exportable_callback<CallSignal::RtcpReportReceived>(bind(&DBusCallManager::onRtcpReportReceived, callM, _1, _2)),
            }
        },
        { // Configuration event handlers
            DRing::EventHandlerKey::CONFIG, {
                exportable_callback<ConfigurationSignal::VolumeChanged>(bind(&DBusConfigurationManager::volumeChanged, confM, _1, _2)),
                exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&DBusConfigurationManager::accountsChanged, confM)),
                exportable_callback<ConfigurationSignal::HistoryChanged>(bind(&DBusConfigurationManager::historyChanged, confM)),
                exportable_callback<ConfigurationSignal::StunStatusFailed>(bind(&DBusConfigurationManager::stunStatusFailure, confM, _1)),
                exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&DBusConfigurationManager::registrationStateChanged, confM, _1, _2)),
                exportable_callback<ConfigurationSignal::SipRegistrationStateChanged>(bind(&DBusConfigurationManager::sipRegistrationStateChanged, confM, _1, _2, _3)),
                exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(bind(&DBusConfigurationManager::volatileAccountDetailsChanged, confM, _1, _2)),
                exportable_callback<ConfigurationSignal::Error>(bind(&DBusConfigurationManager::errorAlert, confM, _1)),
            }
        },
        { // Presence event handlers
            DRing::EventHandlerKey::PRESENCE, {
                exportable_callback<PresenceSignal::NewServerSubscriptionRequest>(bind(&DBusPresenceManager::newServerSubscriptionRequest, presM, _1)),
                exportable_callback<PresenceSignal::ServerError>(bind(&DBusPresenceManager::serverError, presM, _1, _2, _3)),
                exportable_callback<PresenceSignal::NewBuddyNotification>(bind(&DBusPresenceManager::newBuddyNotification, presM, _1, _2, _3, _4)),
                exportable_callback<PresenceSignal::SubscriptionStateChanged>(bind(&DBusPresenceManager::subscriptionStateChanged, presM, _1, _2, _3)),
            }
        },
#ifdef RING_VIDEO
        { // Video event handlers
            DRing::EventHandlerKey::VIDEO, {
                exportable_callback<VideoSignal::DeviceEvent>(bind(&DBusVideoManager::deviceEvent, videoM)),
                exportable_callback<VideoSignal::DecodingStarted>(bind(&DBusVideoManager::startedDecoding, videoM, _1, _2, _3, _4, _5)),
                exportable_callback<VideoSignal::DecodingStopped>(bind(&DBusVideoManager::stoppedDecoding, videoM, _1, _2, _3)),
            }
        },
#endif
    };

    // Initialize now
    return (unsigned)DRing::init(evHandlers, static_cast<DRing::InitFlag>(sflphFlags));
}

void
DBusClient::finiLibrary() noexcept
{
    DRing::fini();
}

int
DBusClient::event_loop() noexcept
{
    try {
        dispatcher_->enter();
    } catch (const DBus::Error& err) {
        std::cerr << "quitting: " << err.name() << ": " << err.what() << std::endl;
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}

int
DBusClient::exit() noexcept
{
    try {
        dispatcher_->leave();
        timeout_->expired = new EventCallback([] {});
        finiLibrary();
    } catch (const DBus::Error& err) {
        std::cerr << "quitting: " << err.name() << ": " << err.what() << std::endl;
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
