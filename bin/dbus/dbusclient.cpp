/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "dbusclient.h"
#include "dbus_cpp.h"
#include "dring.h"

#include "dbusinstance.h"

#include "callmanager_interface.h"
#include "dbuscallmanager.h"

#include "dbusconfigurationmanager.h"
#include "configurationmanager_interface.h"

#include "dbuspresencemanager.h"
#include "presencemanager_interface.h"

#ifdef ENABLE_PLUGIN
#include "dbuspluginmanagerinterface.h"
#include "plugin_manager_interface.h"
#endif // ENABLE_PLUGIN

#include "datatransfer_interface.h"
#include "conversation_interface.h"

#ifdef ENABLE_VIDEO
#include "dbusvideomanager.h"
#include "videomanager_interface.h"
#endif

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

static constexpr const char* const JAMI_DBUS_NAME = "cx.ring.Ring";

class EventCallback : public DBus::Callback_Base<void, DBus::DefaultTimeout&>
{
public:
    EventCallback(const std::function<void()>&& func)
        : callback_ {std::forward<const std::function<void()>>(func)}
    {}

    void call(DBus::DefaultTimeout&) const { callback_(); }

private:
    const std::function<void()> callback_;
};

DBusClient::DBusClient(int flags, bool persistent)
    : dispatcher_(new DBus::BusDispatcher)
{
    try {
        DBus::_init_threading();
        DBus::default_dispatcher = dispatcher_.get();

        DBus::Connection sessionConnection {DBus::Connection::SessionBus()};
        if (sessionConnection.has_name(JAMI_DBUS_NAME)) {
            throw std::runtime_error {"Another daemon is detected"};
        }
        sessionConnection.request_name(JAMI_DBUS_NAME);

        callManager_.reset(new DBusCallManager {sessionConnection});
        configurationManager_.reset(new DBusConfigurationManager {sessionConnection});
        presenceManager_.reset(new DBusPresenceManager {sessionConnection});
#ifdef ENABLE_PLUGIN
        pluginManagerInterface_.reset(new DBusPluginManagerInterface {sessionConnection});
#endif
        DBusInstance::OnNoMoreClientFunc onNoMoreClientFunc;
        if (!persistent)
            onNoMoreClientFunc = [this] {
                this->exit();
            };

        instanceManager_.reset(new DBusInstance {sessionConnection, onNoMoreClientFunc});

#ifdef ENABLE_VIDEO
        videoManager_.reset(new DBusVideoManager {sessionConnection});
#endif
    } catch (const DBus::Error& err) {
        throw std::runtime_error {"cannot initialize DBus stuff"};
    }

    if (initLibrary(flags) < 0)
        throw std::runtime_error {"cannot initialize libring"};

    instanceManager_->started();
}

DBusClient::~DBusClient()
{
    // instances destruction order is important
    // so we enforce it here
    DRing::unregisterSignalHandlers();
#ifdef ENABLE_VIDEO
    videoManager_.reset();
#endif
    instanceManager_.reset();
    presenceManager_.reset();
    configurationManager_.reset();
    callManager_.reset();
#ifdef ENABLE_PLUGIN
    pluginManagerInterface_.reset();
#endif
}

int
DBusClient::initLibrary(int flags)
{
    using namespace std::placeholders;

    using std::bind;
    using DRing::exportable_callback;
    using DRing::CallSignal;
    using DRing::ConfigurationSignal;
    using DRing::PresenceSignal;
    using DRing::AudioSignal;
    using DRing::DataTransferSignal;
    using DRing::ConversationSignal;

    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

    auto callM = callManager_.get();
    auto confM = configurationManager_.get();
    auto presM = presenceManager_.get();

#ifdef ENABLE_VIDEO
    using DRing::VideoSignal;
    auto videoM = videoManager_.get();
#endif

    // Call event handlers
    const std::map<std::string, SharedCallback> callEvHandlers
        = {exportable_callback<CallSignal::StateChange>(
               bind(&DBusCallManager::callStateChanged, callM, _1, _2, _3)),
           exportable_callback<CallSignal::TransferFailed>(
               bind(&DBusCallManager::transferFailed, callM)),
           exportable_callback<CallSignal::TransferSucceeded>(
               bind(&DBusCallManager::transferSucceeded, callM)),
           exportable_callback<CallSignal::RecordPlaybackStopped>(
               bind(&DBusCallManager::recordPlaybackStopped, callM, _1)),
           exportable_callback<CallSignal::VoiceMailNotify>(
               bind(&DBusCallManager::voiceMailNotify, callM, _1, _2, _3, _4)),
           exportable_callback<CallSignal::IncomingMessage>(
               bind(&DBusCallManager::incomingMessage, callM, _1, _2, _3)),
           exportable_callback<CallSignal::IncomingCall>(
               bind(&DBusCallManager::incomingCall, callM, _1, _2, _3)),
           exportable_callback<CallSignal::RecordPlaybackFilepath>(
               bind(&DBusCallManager::recordPlaybackFilepath, callM, _1, _2)),
           exportable_callback<CallSignal::ConferenceCreated>(
               bind(&DBusCallManager::conferenceCreated, callM, _1)),
           exportable_callback<CallSignal::ConferenceChanged>(
               bind(&DBusCallManager::conferenceChanged, callM, _1, _2)),
           exportable_callback<CallSignal::UpdatePlaybackScale>(
               bind(&DBusCallManager::updatePlaybackScale, callM, _1, _2, _3)),
           exportable_callback<CallSignal::ConferenceRemoved>(
               bind(&DBusCallManager::conferenceRemoved, callM, _1)),
           exportable_callback<CallSignal::RecordingStateChanged>(
               bind(&DBusCallManager::recordingStateChanged, callM, _1, _2)),
           exportable_callback<CallSignal::SecureSdesOn>(
               bind(&DBusCallManager::secureSdesOn, callM, _1)),
           exportable_callback<CallSignal::SecureSdesOff>(
               bind(&DBusCallManager::secureSdesOff, callM, _1)),
           exportable_callback<CallSignal::RtcpReportReceived>(
               bind(&DBusCallManager::onRtcpReportReceived, callM, _1, _2)),
           exportable_callback<CallSignal::OnConferenceInfosUpdated>(
               bind(&DBusCallManager::onConferenceInfosUpdated, callM, _1, _2)),
           exportable_callback<CallSignal::PeerHold>(
               bind(&DBusCallManager::peerHold, callM, _1, _2)),
           exportable_callback<CallSignal::AudioMuted>(
               bind(&DBusCallManager::audioMuted, callM, _1, _2)),
           exportable_callback<CallSignal::VideoMuted>(
               bind(&DBusCallManager::videoMuted, callM, _1, _2)),
           exportable_callback<CallSignal::SmartInfo>(bind(&DBusCallManager::SmartInfo, callM, _1)),
           exportable_callback<CallSignal::RemoteRecordingChanged>(
               bind(&DBusCallManager::remoteRecordingChanged, callM, _1, _2, _3)),
           exportable_callback<CallSignal::MediaNegotiationStatus>(
               bind(&DBusCallManager::mediaNegotiationStatus, callM, _1, _2))};

    // Configuration event handlers
    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::VolumeChanged>(
            bind(&DBusConfigurationManager::volumeChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::AccountsChanged>(
            bind(&DBusConfigurationManager::accountsChanged, confM)),
        exportable_callback<ConfigurationSignal::AccountDetailsChanged>(
            bind(&DBusConfigurationManager::accountDetailsChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::StunStatusFailed>(
            bind(&DBusConfigurationManager::stunStatusFailure, confM, _1)),
        exportable_callback<ConfigurationSignal::RegistrationStateChanged>(
            bind(&DBusConfigurationManager::registrationStateChanged, confM, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(
            bind(&DBusConfigurationManager::volatileAccountDetailsChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::Error>(
            bind(&DBusConfigurationManager::errorAlert, confM, _1)),
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>(
            bind(&DBusConfigurationManager::incomingAccountMessage, confM, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::AccountMessageStatusChanged>(
            bind(&DBusConfigurationManager::accountMessageStatusChanged, confM, _1, _2, _3, _4, _5)),
        exportable_callback<ConfigurationSignal::ProfileReceived>(
            bind(&DBusConfigurationManager::profileReceived, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::ComposingStatusChanged>(
            bind(&DBusConfigurationManager::composingStatusChanged, confM, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::IncomingTrustRequest>(
            bind(&DBusConfigurationManager::incomingTrustRequest, confM, _1, _2, _3, _4, _5)),
        exportable_callback<ConfigurationSignal::ContactAdded>(
            bind(&DBusConfigurationManager::contactAdded, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::ContactRemoved>(
            bind(&DBusConfigurationManager::contactRemoved, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::ExportOnRingEnded>(
            bind(&DBusConfigurationManager::exportOnRingEnded, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::KnownDevicesChanged>(
            bind(&DBusConfigurationManager::knownDevicesChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::NameRegistrationEnded>(
            bind(&DBusConfigurationManager::nameRegistrationEnded, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::UserSearchEnded>(
            bind(&DBusConfigurationManager::userSearchEnded, confM, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::RegisteredNameFound>(
            bind(&DBusConfigurationManager::registeredNameFound, confM, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::DeviceRevocationEnded>(
            bind(&DBusConfigurationManager::deviceRevocationEnded, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::AccountProfileReceived>(
            bind(&DBusConfigurationManager::accountProfileReceived, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::CertificatePinned>(
            bind(&DBusConfigurationManager::certificatePinned, confM, _1)),
        exportable_callback<ConfigurationSignal::CertificatePathPinned>(
            bind(&DBusConfigurationManager::certificatePathPinned, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::CertificateExpired>(
            bind(&DBusConfigurationManager::certificateExpired, confM, _1)),
        exportable_callback<ConfigurationSignal::CertificateStateChanged>(
            bind(&DBusConfigurationManager::certificateStateChanged, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::MediaParametersChanged>(
            bind(&DBusConfigurationManager::mediaParametersChanged, confM, _1)),
        exportable_callback<ConfigurationSignal::MigrationEnded>(
            bind(&DBusConfigurationManager::migrationEnded, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::HardwareDecodingChanged>(
            bind(&DBusConfigurationManager::hardwareDecodingChanged, confM, _1)),
        exportable_callback<ConfigurationSignal::HardwareEncodingChanged>(
            bind(&DBusConfigurationManager::hardwareEncodingChanged, confM, _1)),
        exportable_callback<ConfigurationSignal::MessageSend>(
            bind(&DBusConfigurationManager::messageSend, confM, _1)),
    };

    // Presence event handlers
    const std::map<std::string, SharedCallback> presEvHandlers = {
        exportable_callback<PresenceSignal::NewServerSubscriptionRequest>(
            bind(&DBusPresenceManager::newServerSubscriptionRequest, presM, _1)),
        exportable_callback<PresenceSignal::ServerError>(
            bind(&DBusPresenceManager::serverError, presM, _1, _2, _3)),
        exportable_callback<PresenceSignal::NewBuddyNotification>(
            bind(&DBusPresenceManager::newBuddyNotification, presM, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::NearbyPeerNotification>(
            bind(&DBusPresenceManager::nearbyPeerNotification, presM, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::SubscriptionStateChanged>(
            bind(&DBusPresenceManager::subscriptionStateChanged, presM, _1, _2, _3)),
    };

    // Audio event handlers
    const std::map<std::string, SharedCallback> audioEvHandlers = {
        exportable_callback<AudioSignal::DeviceEvent>(
            bind(&DBusConfigurationManager::audioDeviceEvent, confM)),
        exportable_callback<AudioSignal::AudioMeter>(
            bind(&DBusConfigurationManager::audioMeter, confM, _1, _2)),
    };

    const std::map<std::string, SharedCallback> dataXferEvHandlers = {
        exportable_callback<DataTransferSignal::DataTransferEvent>(
            bind(&DBusConfigurationManager::dataTransferEvent, confM, _1, _2, _3, _4)),
    };

    const std::map<std::string, SharedCallback> convEvHandlers = {
        exportable_callback<ConversationSignal::ConversationLoaded>(
            bind(&DBusConfigurationManager::conversationLoaded, confM, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::MessageReceived>(
            bind(&DBusConfigurationManager::messageReceived, confM, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestReceived>(
            bind(&DBusConfigurationManager::conversationRequestReceived, confM, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationReady>(
            bind(&DBusConfigurationManager::conversationReady, confM, _1, _2)),
        exportable_callback<ConversationSignal::ConversationRemoved>(
            bind(&DBusConfigurationManager::conversationRemoved, confM, _1, _2)),
        exportable_callback<ConversationSignal::ConversationMemberEvent>(
            bind(&DBusConfigurationManager::conversationMemberEvent, confM, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::OnConversationError>(
            bind(&DBusConfigurationManager::onConversationError, confM, _1, _2, _3, _4)),
    };

#ifdef ENABLE_VIDEO
    // Video event handlers
    const std::map<std::string, SharedCallback> videoEvHandlers = {
        exportable_callback<VideoSignal::DeviceEvent>(bind(&DBusVideoManager::deviceEvent, videoM)),
        exportable_callback<VideoSignal::DecodingStarted>(
            bind(&DBusVideoManager::startedDecoding, videoM, _1, _2, _3, _4, _5)),
        exportable_callback<VideoSignal::DecodingStopped>(
            bind(&DBusVideoManager::stoppedDecoding, videoM, _1, _2, _3)),
    };
#endif

    if (!DRing::init(static_cast<DRing::InitFlag>(flags)))
        return -1;

    DRing::registerSignalHandlers(callEvHandlers);
    DRing::registerSignalHandlers(configEvHandlers);
    DRing::registerSignalHandlers(presEvHandlers);
    DRing::registerSignalHandlers(audioEvHandlers);
    DRing::registerSignalHandlers(dataXferEvHandlers);
    DRing::registerSignalHandlers(convEvHandlers);
#ifdef ENABLE_VIDEO
    DRing::registerSignalHandlers(videoEvHandlers);
#endif

    if (!DRing::start())
        return -1;
    return 0;
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
