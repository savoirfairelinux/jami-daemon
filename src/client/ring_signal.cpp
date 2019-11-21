/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
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

#include "ring_signal.h"

namespace jami {

SignalHandlerMap&
getSignalHandlers()
{
    static SignalHandlerMap handlers = {
        /* Call */
        exported_callback<DRing::CallSignal::StateChange>(),
        exported_callback<DRing::CallSignal::TransferFailed>(),
        exported_callback<DRing::CallSignal::TransferSucceeded>(),
        exported_callback<DRing::CallSignal::RecordPlaybackStopped>(),
        exported_callback<DRing::CallSignal::VoiceMailNotify>(),
        exported_callback<DRing::CallSignal::IncomingMessage>(),
        exported_callback<DRing::CallSignal::IncomingCall>(),
        exported_callback<DRing::CallSignal::RecordPlaybackFilepath>(),
        exported_callback<DRing::CallSignal::ConferenceCreated>(),
        exported_callback<DRing::CallSignal::ConferenceChanged>(),
        exported_callback<DRing::CallSignal::UpdatePlaybackScale>(),
        exported_callback<DRing::CallSignal::ConferenceRemoved>(),
        exported_callback<DRing::CallSignal::RecordingStateChanged>(),
        exported_callback<DRing::CallSignal::SecureSdesOn>(),
        exported_callback<DRing::CallSignal::SecureSdesOff>(),
        exported_callback<DRing::CallSignal::RtcpReportReceived>(),
        exported_callback<DRing::CallSignal::PeerHold>(),
        exported_callback<DRing::CallSignal::VideoMuted>(),
        exported_callback<DRing::CallSignal::AudioMuted>(),
        exported_callback<DRing::CallSignal::SmartInfo>(),
        exported_callback<DRing::CallSignal::ConnectionUpdate>(),

        /* Configuration */
        exported_callback<DRing::ConfigurationSignal::VolumeChanged>(),
        exported_callback<DRing::ConfigurationSignal::AccountsChanged>(),
        exported_callback<DRing::ConfigurationSignal::AccountDetailsChanged>(),
        exported_callback<DRing::ConfigurationSignal::StunStatusFailed>(),
        exported_callback<DRing::ConfigurationSignal::RegistrationStateChanged>(),
        exported_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(),
        exported_callback<DRing::ConfigurationSignal::CertificatePinned>(),
        exported_callback<DRing::ConfigurationSignal::CertificatePathPinned>(),
        exported_callback<DRing::ConfigurationSignal::CertificateExpired>(),
        exported_callback<DRing::ConfigurationSignal::CertificateStateChanged>(),
        exported_callback<DRing::ConfigurationSignal::IncomingAccountMessage>(),
        exported_callback<DRing::ConfigurationSignal::AccountMessageStatusChanged>(),
        exported_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(),
        exported_callback<DRing::ConfigurationSignal::ContactAdded>(),
        exported_callback<DRing::ConfigurationSignal::ContactRemoved>(),
        exported_callback<DRing::ConfigurationSignal::ExportOnRingEnded>(),
        exported_callback<DRing::ConfigurationSignal::KnownDevicesChanged>(),
        exported_callback<DRing::ConfigurationSignal::NameRegistrationEnded>(),
        exported_callback<DRing::ConfigurationSignal::RegisteredNameFound>(),
        exported_callback<DRing::ConfigurationSignal::MediaParametersChanged>(),
        exported_callback<DRing::ConfigurationSignal::MigrationEnded>(),
        exported_callback<DRing::ConfigurationSignal::DeviceRevocationEnded>(),
        exported_callback<DRing::ConfigurationSignal::Error>(),
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        exported_callback<DRing::ConfigurationSignal::GetHardwareAudioFormat>(),
#endif
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || defined(RING_UWP)
        exported_callback<DRing::ConfigurationSignal::GetAppDataPath>(),
        exported_callback<DRing::ConfigurationSignal::GetDeviceName>(),
#endif
        exported_callback<DRing::ConfigurationSignal::HardwareDecodingChanged>(),
        exported_callback<DRing::ConfigurationSignal::HardwareEncodingChanged>(),

        /* Debug */
        exported_callback<DRing::DebugSignal::MessageSend>(),

        /* Presence */
        exported_callback<DRing::PresenceSignal::NewServerSubscriptionRequest>(),
        exported_callback<DRing::PresenceSignal::NearbyPeerNotification>(),
        exported_callback<DRing::PresenceSignal::ServerError>(),
        exported_callback<DRing::PresenceSignal::NewBuddyNotification>(),
        exported_callback<DRing::PresenceSignal::SubscriptionStateChanged>(),

        /* Audio */
        exported_callback<DRing::AudioSignal::DeviceEvent>(),
        exported_callback<DRing::AudioSignal::AudioMeter>(),

        /* DataTransfer */
        exported_callback<DRing::DataTransferSignal::DataTransferEvent>(),

#ifdef ENABLE_VIDEO
        /* Video */
        exported_callback<DRing::VideoSignal::DeviceEvent>(),
        exported_callback<DRing::VideoSignal::DecodingStarted>(),
        exported_callback<DRing::VideoSignal::DecodingStopped>(),
#if (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        exported_callback<DRing::VideoSignal::UseSoftwareEncoding>(),
#endif
#ifdef __ANDROID__
        exported_callback<DRing::VideoSignal::GetCameraInfo>(),
        exported_callback<DRing::VideoSignal::SetParameters>(),
        exported_callback<DRing::VideoSignal::RequestKeyFrame>(),
        exported_callback<DRing::VideoSignal::SetBitrate>(),
#endif
        exported_callback<DRing::VideoSignal::StartCapture>(),
        exported_callback<DRing::VideoSignal::StopCapture>(),
        exported_callback<DRing::VideoSignal::DeviceAdded>(),
        exported_callback<DRing::VideoSignal::ParametersChanged>(),
#endif
    };

    return handlers;
}

}; // namespace jami

namespace DRing {

void
registerSignalHandlers(const std::map<std::string,
                       std::shared_ptr<CallbackWrapperBase>>&handlers)
{
    auto& handlers_ = jami::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            JAMI_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }
        iter->second = item.second;
    }
}

void
unregisterSignalHandlers()
{
    auto& handlers_ = jami::getSignalHandlers();
    for (auto& item : handlers_) {
        item.second = {};
    }
}


}
