/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

namespace ring {

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
        exported_callback<DRing::CallSignal::NewCallCreated>(),
        exported_callback<DRing::CallSignal::SipCallStateChanged>(),
        exported_callback<DRing::CallSignal::RecordingStateChanged>(),
        exported_callback<DRing::CallSignal::SecureSdesOn>(),
        exported_callback<DRing::CallSignal::SecureSdesOff>(),
        exported_callback<DRing::CallSignal::RtcpReportReceived>(),
        exported_callback<DRing::CallSignal::PeerHold>(),
        exported_callback<DRing::CallSignal::VideoMuted>(),
        exported_callback<DRing::CallSignal::AudioMuted>(),
        exported_callback<DRing::CallSignal::SmartInfo>(),

        /* Configuration */
        exported_callback<DRing::ConfigurationSignal::VolumeChanged>(),
        exported_callback<DRing::ConfigurationSignal::AccountsChanged>(),
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
        exported_callback<DRing::ConfigurationSignal::MediaParametersChanged>(),
        exported_callback<DRing::ConfigurationSignal::Error>(),
#ifdef __ANDROID__
        exported_callback<DRing::ConfigurationSignal::GetHardwareAudioFormat>(),
        exported_callback<DRing::ConfigurationSignal::GetAppDataPath>(),
#endif

        /* Presence */
        exported_callback<DRing::PresenceSignal::NewServerSubscriptionRequest>(),
        exported_callback<DRing::PresenceSignal::ServerError>(),
        exported_callback<DRing::PresenceSignal::NewBuddyNotification>(),
        exported_callback<DRing::PresenceSignal::SubscriptionStateChanged>(),

        /* Audio */
        exported_callback<DRing::AudioSignal::DeviceEvent>(),

#ifdef RING_VIDEO
        /* Video */
        exported_callback<DRing::VideoSignal::DeviceEvent>(),
        exported_callback<DRing::VideoSignal::DecodingStarted>(),
        exported_callback<DRing::VideoSignal::DecodingStopped>(),
#ifdef __ANDROID__
        exported_callback<DRing::VideoSignal::GetCameraInfo>(),
        exported_callback<DRing::VideoSignal::SetParameters>(),
        exported_callback<DRing::VideoSignal::StartCapture>(),
        exported_callback<DRing::VideoSignal::StopCapture>(),
#endif
#endif
    };

    return handlers;
}

}; // namespace ring
