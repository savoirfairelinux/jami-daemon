/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
        exported_callback<libjami::CallSignal::StateChange>(),
        exported_callback<libjami::CallSignal::TransferFailed>(),
        exported_callback<libjami::CallSignal::TransferSucceeded>(),
        exported_callback<libjami::CallSignal::RecordPlaybackStopped>(),
        exported_callback<libjami::CallSignal::VoiceMailNotify>(),
        exported_callback<libjami::CallSignal::IncomingMessage>(),
        exported_callback<libjami::CallSignal::IncomingCall>(),
        exported_callback<libjami::CallSignal::IncomingCallWithMedia>(),
        exported_callback<libjami::CallSignal::MediaChangeRequested>(),
        exported_callback<libjami::CallSignal::RecordPlaybackFilepath>(),
        exported_callback<libjami::CallSignal::ConferenceCreated>(),
        exported_callback<libjami::CallSignal::ConferenceChanged>(),
        exported_callback<libjami::CallSignal::UpdatePlaybackScale>(),
        exported_callback<libjami::CallSignal::ConferenceRemoved>(),
        exported_callback<libjami::CallSignal::RecordingStateChanged>(),
        exported_callback<libjami::CallSignal::RtcpReportReceived>(),
        exported_callback<libjami::CallSignal::PeerHold>(),
        exported_callback<libjami::CallSignal::VideoMuted>(),
        exported_callback<libjami::CallSignal::AudioMuted>(),
        exported_callback<libjami::CallSignal::SmartInfo>(),
        exported_callback<libjami::CallSignal::ConnectionUpdate>(),
        exported_callback<libjami::CallSignal::OnConferenceInfosUpdated>(),
        exported_callback<libjami::CallSignal::RemoteRecordingChanged>(),
        exported_callback<libjami::CallSignal::MediaNegotiationStatus>(),

        /* Configuration */
        exported_callback<libjami::ConfigurationSignal::VolumeChanged>(),
        exported_callback<libjami::ConfigurationSignal::AccountsChanged>(),
        exported_callback<libjami::ConfigurationSignal::AccountDetailsChanged>(),
        exported_callback<libjami::ConfigurationSignal::StunStatusFailed>(),
        exported_callback<libjami::ConfigurationSignal::RegistrationStateChanged>(),
        exported_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(),
        exported_callback<libjami::ConfigurationSignal::CertificatePinned>(),
        exported_callback<libjami::ConfigurationSignal::CertificatePathPinned>(),
        exported_callback<libjami::ConfigurationSignal::CertificateExpired>(),
        exported_callback<libjami::ConfigurationSignal::CertificateStateChanged>(),
        exported_callback<libjami::ConfigurationSignal::IncomingAccountMessage>(),
        exported_callback<libjami::ConfigurationSignal::ComposingStatusChanged>(),
        exported_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(),
        exported_callback<libjami::ConfigurationSignal::NeedsHost>(),
        exported_callback<libjami::ConfigurationSignal::ActiveCallsChanged>(),
        exported_callback<libjami::ConfigurationSignal::ProfileReceived>(),
        exported_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(),
        exported_callback<libjami::ConfigurationSignal::ContactAdded>(),
        exported_callback<libjami::ConfigurationSignal::ContactRemoved>(),
        exported_callback<libjami::ConfigurationSignal::ExportOnRingEnded>(),
        exported_callback<libjami::ConfigurationSignal::KnownDevicesChanged>(),
        exported_callback<libjami::ConfigurationSignal::NameRegistrationEnded>(),
        exported_callback<libjami::ConfigurationSignal::RegisteredNameFound>(),
        exported_callback<libjami::ConfigurationSignal::UserSearchEnded>(),
        exported_callback<libjami::ConfigurationSignal::MediaParametersChanged>(),
        exported_callback<libjami::ConfigurationSignal::MigrationEnded>(),
        exported_callback<libjami::ConfigurationSignal::DeviceRevocationEnded>(),
        exported_callback<libjami::ConfigurationSignal::AccountProfileReceived>(),
        exported_callback<libjami::ConfigurationSignal::Error>(),
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        exported_callback<libjami::ConfigurationSignal::GetHardwareAudioFormat>(),
#endif
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || defined(RING_UWP)
        exported_callback<libjami::ConfigurationSignal::GetAppDataPath>(),
        exported_callback<libjami::ConfigurationSignal::GetDeviceName>(),
#endif
        exported_callback<libjami::ConfigurationSignal::HardwareDecodingChanged>(),
        exported_callback<libjami::ConfigurationSignal::HardwareEncodingChanged>(),
        exported_callback<libjami::ConfigurationSignal::MessageSend>(),

        /* Presence */
        exported_callback<libjami::PresenceSignal::NewServerSubscriptionRequest>(),
        exported_callback<libjami::PresenceSignal::NearbyPeerNotification>(),
        exported_callback<libjami::PresenceSignal::ServerError>(),
        exported_callback<libjami::PresenceSignal::NewBuddyNotification>(),
        exported_callback<libjami::PresenceSignal::SubscriptionStateChanged>(),

        /* Audio */
        exported_callback<libjami::AudioSignal::DeviceEvent>(),
        exported_callback<libjami::AudioSignal::AudioMeter>(),

        /* DataTransfer */
        exported_callback<libjami::DataTransferSignal::DataTransferEvent>(),

#ifdef ENABLE_VIDEO
        /* MediaPlayer */
        exported_callback<libjami::MediaPlayerSignal::FileOpened>(),

        /* Video */
        exported_callback<libjami::VideoSignal::DeviceEvent>(),
        exported_callback<libjami::VideoSignal::DecodingStarted>(),
        exported_callback<libjami::VideoSignal::DecodingStopped>(),
#ifdef __ANDROID__
        exported_callback<libjami::VideoSignal::GetCameraInfo>(),
        exported_callback<libjami::VideoSignal::SetParameters>(),
        exported_callback<libjami::VideoSignal::RequestKeyFrame>(),
        exported_callback<libjami::VideoSignal::SetBitrate>(),
#endif
        exported_callback<libjami::VideoSignal::StartCapture>(),
        exported_callback<libjami::VideoSignal::StopCapture>(),
        exported_callback<libjami::VideoSignal::DeviceAdded>(),
        exported_callback<libjami::VideoSignal::ParametersChanged>(),
#endif

        /* Conversation */
        exported_callback<libjami::ConversationSignal::ConversationLoaded>(),
        exported_callback<libjami::ConversationSignal::SwarmLoaded>(),
        exported_callback<libjami::ConversationSignal::MessagesFound>(),
        exported_callback<libjami::ConversationSignal::MessageReceived>(),
        exported_callback<libjami::ConversationSignal::SwarmMessageReceived>(),
        exported_callback<libjami::ConversationSignal::SwarmMessageUpdated>(),
        exported_callback<libjami::ConversationSignal::ReactionAdded>(),
        exported_callback<libjami::ConversationSignal::ReactionRemoved>(),
        exported_callback<libjami::ConversationSignal::ConversationProfileUpdated>(),
        exported_callback<libjami::ConversationSignal::ConversationRequestReceived>(),
        exported_callback<libjami::ConversationSignal::ConversationRequestDeclined>(),
        exported_callback<libjami::ConversationSignal::ConversationReady>(),
        exported_callback<libjami::ConversationSignal::ConversationRemoved>(),
        exported_callback<libjami::ConversationSignal::ConversationMemberEvent>(),
        exported_callback<libjami::ConversationSignal::ConversationSyncFinished>(),
        exported_callback<libjami::ConversationSignal::ConversationCloned>(),
        exported_callback<libjami::ConversationSignal::CallConnectionRequest>(),
        exported_callback<libjami::ConversationSignal::OnConversationError>(),
        exported_callback<libjami::ConversationSignal::ConversationPreferencesUpdated>(),

#ifdef ENABLE_PLUGIN
        exported_callback<libjami::PluginSignal::WebViewMessageReceived>(),
#endif
    };

    return handlers;
}

}; // namespace jami

namespace libjami {

void
registerSignalHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
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

} // namespace libjami
