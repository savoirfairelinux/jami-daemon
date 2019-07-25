/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
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

#include "dbusclient.h"
#include "dring.h"

#include <sigc++/sigc++.h>
#include <iostream>
#include <stdexcept>
//#include <cstdlib>
//#include <cstring>

DBusClient::DBusClient(int flags, bool persistent)
{
    connection_ = sdbus::createSessionBusConnection("net.jami.daemon1");
    daemon_ = std::make_unique<DBusDaemon1>(*connection_, "/net/jami/daemon1");

    if (!persistent)
        daemon_->signal_numberOfClientsChanged().connect(sigc::mem_fun(this, &DBusClient::onNumberOfClientsChanged));

    if (initLibrary(flags) < 0)
        throw std::runtime_error {"cannot initialize libring"};

    daemon_->emitStarted();
}

DBusClient::~DBusClient()
{
    DRing::unregisterSignalHandlers();
    DRing::fini();
}

int
DBusClient::initLibrary(int flags)
{
    using namespace std::placeholders;

    using std::bind;
    using DRing::exportable_callback;
    using DRing::CallSignal;
    using DRing::ConfigurationSignal;
    using DRing::DataTransferSignal;
    using DRing::PresenceSignal;
    using DRing::AudioSignal;
#ifdef ENABLE_VIDEO
    using DRing::VideoSignal;
#endif

    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

    auto daemon = daemon_.get();

    // Call event handlers
    const std::map<std::string, SharedCallback> callEvHandlers = {
        exportable_callback<CallSignal::StateChange>(bind(&DBusDaemon1::emitCallStateChanged, daemon, _1, _2, _3)),
        exportable_callback<CallSignal::TransferFailed>(bind(&DBusDaemon1::emitTransferFailed, daemon)),
        exportable_callback<CallSignal::TransferSucceeded>(bind(&DBusDaemon1::emitTransferSucceeded, daemon)),
        exportable_callback<CallSignal::RecordPlaybackStopped>(bind(&DBusDaemon1::emitRecordPlaybackStopped, daemon, _1)),
        exportable_callback<CallSignal::VoiceMailNotify>(bind(&DBusDaemon1::emitVoiceMailNotify, daemon, _1, _2)),
        exportable_callback<CallSignal::IncomingMessage>(bind(&DBusDaemon1::emitIncomingMessage, daemon, _1, _2, _3)),
        exportable_callback<CallSignal::IncomingCall>(bind(&DBusDaemon1::emitIncomingCall, daemon, _1, _2, _3)),
        exportable_callback<CallSignal::RecordPlaybackFilepath>(bind(&DBusDaemon1::emitRecordPlaybackFilepath, daemon, _1, _2)),
        exportable_callback<CallSignal::ConferenceCreated>(bind(&DBusDaemon1::emitConferenceCreated, daemon, _1)),
        exportable_callback<CallSignal::ConferenceChanged>(bind(&DBusDaemon1::emitConferenceChanged, daemon, _1, _2)),
        exportable_callback<CallSignal::UpdatePlaybackScale>(bind(&DBusDaemon1::emitUpdatePlaybackScale, daemon, _1, _2, _3)),
        exportable_callback<CallSignal::ConferenceRemoved>(bind(&DBusDaemon1::emitConferenceRemoved, daemon, _1)),
        exportable_callback<CallSignal::NewCallCreated>(bind(&DBusDaemon1::emitNewCallCreated, daemon, _1, _2, _3)),
        exportable_callback<CallSignal::RecordingStateChanged>(bind(&DBusDaemon1::emitRecordingStateChanged, daemon, _1, _2)),
        exportable_callback<CallSignal::SecureSdesOn>(bind(&DBusDaemon1::emitSecureSdesOn, daemon, _1)),
        exportable_callback<CallSignal::SecureSdesOff>(bind(&DBusDaemon1::emitSecureSdesOff, daemon, _1)),
        exportable_callback<CallSignal::RtcpReportReceived>(bind(&DBusDaemon1::emitOnRtcpReportReceived, daemon, _1, _2)),
        exportable_callback<CallSignal::PeerHold>(bind(&DBusDaemon1::emitPeerHold, daemon, _1, _2)),
        exportable_callback<CallSignal::AudioMuted>(bind(&DBusDaemon1::emitAudioMuted, daemon, _1, _2)),
        exportable_callback<CallSignal::VideoMuted>(bind(&DBusDaemon1::emitVideoMuted, daemon, _1, _2)),
        exportable_callback<CallSignal::SmartInfo>(bind(&DBusDaemon1::emitSmartInfo, daemon, _1))
    };

    // Configuration event handlers
    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::VolumeChanged>(bind(&DBusDaemon1::emitVolumeChanged, daemon, _1, _2)),
        exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&DBusDaemon1::emitAccountsChanged, daemon)),
        exportable_callback<ConfigurationSignal::AccountDetailsChanged>(bind(&DBusDaemon1::emitAccountDetailsChanged, daemon, _1, _2)),
        exportable_callback<ConfigurationSignal::StunStatusFailed>(bind(&DBusDaemon1::emitStunStatusFailure, daemon, _1)),
        exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&DBusDaemon1::emitRegistrationStateChanged, daemon, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(bind(&DBusDaemon1::emitVolatileAccountDetailsChanged, daemon, _1, _2)),
        exportable_callback<ConfigurationSignal::Error>(bind(&DBusDaemon1::emitErrorAlert, daemon, _1)),
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>(bind(&DBusDaemon1::emitIncomingAccountMessage, daemon, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::AccountMessageStatusChanged>(bind(&DBusDaemon1::emitAccountMessageStatusChanged, daemon, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::IncomingTrustRequest>(bind(&DBusDaemon1::emitIncomingTrustRequest, daemon, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::ContactAdded>(bind(&DBusDaemon1::emitContactAdded, daemon, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ContactRemoved>(bind(&DBusDaemon1::emitContactRemoved, daemon, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ExportOnRingEnded>(bind(&DBusDaemon1::emitExportOnRingEnded, daemon, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::KnownDevicesChanged>(bind(&DBusDaemon1::emitKnownDevicesChanged, daemon, _1, _2 )),
        exportable_callback<ConfigurationSignal::NameRegistrationEnded>(bind(&DBusDaemon1::emitNameRegistrationEnded, daemon, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::RegisteredNameFound>(bind(&DBusDaemon1::emitRegisteredNameFound, daemon, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::DeviceRevocationEnded>(bind(&DBusDaemon1::emitDeviceRevocationEnded, daemon, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::CertificatePinned>(bind(&DBusDaemon1::emitCertificatePinned, daemon, _1 )),
        exportable_callback<ConfigurationSignal::CertificatePathPinned>(bind(&DBusDaemon1::emitCertificatePathPinned, daemon, _1, _2 )),
        exportable_callback<ConfigurationSignal::CertificateExpired>(bind(&DBusDaemon1::emitCertificateExpired, daemon, _1 )),
        exportable_callback<ConfigurationSignal::CertificateStateChanged>(bind(&DBusDaemon1::emitCertificateStateChanged, daemon, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::MediaParametersChanged>(bind(&DBusDaemon1::emitMediaParametersChanged, daemon, _1 )),
        exportable_callback<ConfigurationSignal::MigrationEnded>(bind(&DBusDaemon1::emitMigrationEnded, daemon, _1, _2 )),
        exportable_callback<ConfigurationSignal::HardwareDecodingChanged>(bind(&DBusDaemon1::emitHardwareDecodingChanged, daemon, _1 )),
        exportable_callback<ConfigurationSignal::HardwareEncodingChanged>(bind(&DBusDaemon1::emitHardwareEncodingChanged, daemon, _1 )),
    };

    // Presence event handlers
    const std::map<std::string, SharedCallback> presEvHandlers = {
        exportable_callback<PresenceSignal::NewServerSubscriptionRequest>(bind(&DBusDaemon1::emitNewServerSubscriptionRequest, daemon, _1)),
        exportable_callback<PresenceSignal::ServerError>(bind(&DBusDaemon1::emitServerError, daemon, _1, _2, _3)),
        exportable_callback<PresenceSignal::NewBuddyNotification>(bind(&DBusDaemon1::emitNewBuddyNotification, daemon, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::NearbyPeerNotification>(bind(&DBusDaemon1::emitNearbyPeerNotification, daemon, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::SubscriptionStateChanged>(bind(&DBusDaemon1::emitSubscriptionStateChanged, daemon, _1, _2, _3)),
    };

    // Audio event handlers
    const std::map<std::string, SharedCallback> audioEvHandlers = {
        exportable_callback<AudioSignal::DeviceEvent>(bind(&DBusDaemon1::emitAudioDeviceEvent, daemon)),
        exportable_callback<AudioSignal::AudioMeter>(bind(&DBusDaemon1::emitAudioMeter, daemon, _1, _2)),
    };

    const std::map<std::string, SharedCallback> dataXferEvHandlers = {
        exportable_callback<DataTransferSignal::DataTransferEvent>(bind(&DBusDaemon1::emitDataTransferEvent, daemon, _1, _2)),
    };

#ifdef ENABLE_VIDEO
    // Video event handlers
    const std::map<std::string, SharedCallback> videoEvHandlers = {
        exportable_callback<VideoSignal::DeviceEvent>(bind(&DBusDaemon1::emitDeviceEvent, daemon)),
        exportable_callback<VideoSignal::DecodingStarted>(bind(&DBusDaemon1::emitStartedDecoding, daemon, _1, _2, _3, _4, _5)),
        exportable_callback<VideoSignal::DecodingStopped>(bind(&DBusDaemon1::emitStoppedDecoding, daemon, _1, _2, _3)),
    };
#endif

    if (!DRing::init(static_cast<DRing::InitFlag>(flags)))
        return -1;

    DRing::registerSignalHandlers(callEvHandlers);
    DRing::registerSignalHandlers(configEvHandlers);
    DRing::registerSignalHandlers(presEvHandlers);
    DRing::registerSignalHandlers(audioEvHandlers);
    DRing::registerSignalHandlers(dataXferEvHandlers);
#ifdef ENABLE_VIDEO
    DRing::registerSignalHandlers(videoEvHandlers);
#endif

    if (!DRing::start())
        return -1;
    return 0;
}

void
DBusClient::event_loop()
{
    connection_->enterProcessingLoop();
}

void
DBusClient::exit()
{
    connection_->leaveProcessingLoop();
}

void
DBusClient::onNumberOfClientsChanged(uint_fast16_t newNumberOfClients)
{
    if (newNumberOfClients == 0) {
        exit();
    }
}
