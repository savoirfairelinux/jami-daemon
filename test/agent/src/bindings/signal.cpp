/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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

/* Std */
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

/* Guile */
#include <libguile.h>

/* Jami */
#include "account_const.h"
#include "jami/callmanager_interface.h"
#include "jami/configurationmanager_interface.h"
#include "jami/conversation_interface.h"
#include "jami/datatransfer_interface.h"
#include "jami/presencemanager_interface.h"

/* Agent */
#include "bindings/bindings.h"
#include "utils.h"

static SCM signal_alist = SCM_EOL;

template<typename... Args>
class Handler
{
    std::mutex mutex_;
    std::vector<SCM> callbacks_;

public:
    Handler(const char* symbol_name)
    {
        signal_alist
            = scm_cons(scm_cons(scm_from_utf8_symbol(symbol_name),
                                scm_cons(scm_from_pointer(static_cast<void*>(&callbacks_), nullptr),
                                         scm_from_pointer(static_cast<void*>(&mutex_), nullptr))),
                       signal_alist);
    }

    struct cb_ctx
    {
        Handler<Args...>& me;
        std::tuple<Args...>& args;
    };

    void doExecuteInGuile(Args... args)
    {
        std::unique_lock lck(mutex_);
        std::vector<SCM> old;
        std::vector<SCM> to_keep;

        old = std::move(callbacks_);

        lck.unlock();

        for (SCM cb : old) {
            using namespace std::placeholders;
            using std::bind;

            SCM ret = apply_to_guile(cb, args...);
            if (scm_is_true(ret)) {
                to_keep.emplace_back(cb);
            }
        }

        lck.lock();

        for (SCM cb : to_keep) {
            callbacks_.push_back(cb);
        }
    }

    static void* executeInGuile(void* ctx_raw)
    {
        cb_ctx* ctx = static_cast<cb_ctx*>(ctx_raw);

        auto apply_wrapper = [&](Args... args) {
            ctx->me.doExecuteInGuile(args...);
        };

        std::apply(apply_wrapper, ctx->args);

        return nullptr;
    }

    void execute(Args... args)
    {
        std::tuple<Args...> tuple(args...);

        cb_ctx ctx = {*this, tuple};

        scm_with_guile(executeInGuile, &ctx);
    }
};

static SCM
on_signal_binding(SCM signal_sym, SCM handler_proc)
{
    static SCM bad_signal_sym = scm_from_utf8_symbol("bad-signal");

    SCM handler_pair;

    std::vector<SCM>* callbacks;
    std::mutex* mutex;

    AGENT_ASSERT(scm_is_true(scm_procedure_p(handler_proc)), "handler_proc must be a procedure");

    handler_pair = scm_assq_ref(signal_alist, signal_sym);

    if (scm_is_false(handler_pair)) {
        scm_throw(bad_signal_sym, scm_list_2(signal_sym, handler_proc));
    }

    callbacks = static_cast<std::vector<SCM>*>(scm_to_pointer(scm_car(handler_pair)));
    mutex = static_cast<std::mutex*>(scm_to_pointer(scm_cdr(handler_pair)));

    std::unique_lock lck(*mutex);
    callbacks->push_back(handler_proc);

    return SCM_UNDEFINED;
}

template<typename T, typename... Args>
void
add_handler(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>& handlers,
            const char* name)
{
    static Handler<Args...> handler(name);

    auto fn = [=](Args... args) {
        handler.execute(args...);
    };

    handlers.insert(DRing::exportable_callback<T>(std::move(fn)));
}

void
install_signal_primitives(void*)
{
    define_primitive("on-signal", 2, 0, 0, (void*) on_signal_binding);

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;

    add_handler<DRing::CallSignal::StateChange,
                const std::string&,
                const std::string&,
                const std::string&,
                signed>(handlers, "state-changed");

    add_handler<DRing::CallSignal::TransferFailed>(handlers, "transfer-failed");

    add_handler<DRing::CallSignal::TransferSucceeded>(handlers, "transfer-succeeded");

    add_handler<DRing::CallSignal::RecordPlaybackStopped,
                const std::string&>(handlers, "record-playback-stopped");

    add_handler<DRing::CallSignal::VoiceMailNotify, const std::string&, int32_t, int32_t, int32_t>(
        handlers, "voice-mail-notify");

    add_handler<DRing::CallSignal::IncomingMessage,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "incoming-message");

    add_handler<DRing::CallSignal::IncomingCall,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "incoming-call");

    add_handler<DRing::CallSignal::IncomingCallWithMedia,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::vector<DRing::MediaMap>>(handlers, "incoming-call/media");

    add_handler<DRing::CallSignal::MediaChangeRequested,
                const std::string&,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "media-change-requested");

    add_handler<DRing::CallSignal::RecordPlaybackFilepath, const std::string&, const std::string&>(
        handlers, "record-playback-filepath");

    add_handler<DRing::CallSignal::ConferenceCreated, const std::string&>(handlers,
                                                                          "conference-created");

    add_handler<DRing::CallSignal::ConferenceChanged, const std::string&, const std::string&>(
        handlers, "conference-changed");

    add_handler<DRing::CallSignal::UpdatePlaybackScale, const std::string&, unsigned, unsigned>(
        handlers, "update-playback-scale");

    add_handler<DRing::CallSignal::ConferenceRemoved, const std::string&>(handlers,
                                                                          "conference-removed");

    add_handler<DRing::CallSignal::RecordingStateChanged, const std::string&, int>(
        handlers, "recording-state-changed");

    add_handler<DRing::CallSignal::RtcpReportReceived,
                const std::string&,
                const std::map<std::string, int>&>(handlers, "rtcp-report-received");

    add_handler<DRing::CallSignal::PeerHold, const std::string&, bool>(handlers, "peer-hold");

    add_handler<DRing::CallSignal::VideoMuted, const std::string&, bool>(handlers, "video-muted");

    add_handler<DRing::CallSignal::AudioMuted, const std::string&, bool>(handlers, "audio-muted");

    add_handler<DRing::CallSignal::SmartInfo,
                const std::map<std::string, std::string>&>(handlers, "smart-info");

    add_handler<DRing::CallSignal::ConnectionUpdate, const std::string&, int>(handlers,
                                                                              "connection-update");

    add_handler<DRing::CallSignal::OnConferenceInfosUpdated,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "conference-infos-updated");

    add_handler<DRing::CallSignal::RemoteRecordingChanged,
                const std::string&,
                const std::string&,
                bool>(handlers, "remote-recording-changed");

    add_handler<DRing::CallSignal::MediaNegotiationStatus,
                const std::string&,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "media-negotiation-status");

    /* Configuration */
    add_handler<DRing::ConfigurationSignal::VolumeChanged, const std::string&, double>(
        handlers, "volume-changed");

    add_handler<DRing::ConfigurationSignal::Error, int>(handlers, "configuration-error");

    add_handler<DRing::ConfigurationSignal::AccountsChanged>(handlers, "accounts-changed");

    add_handler<DRing::ConfigurationSignal::AccountDetailsChanged,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "account-details-changed");

    add_handler<DRing::ConfigurationSignal::StunStatusFailed,
                const std::string&>(handlers, "stun-status-failed");

    add_handler<DRing::ConfigurationSignal::RegistrationStateChanged,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "registration-state-changed");

    add_handler<DRing::ConfigurationSignal::VolatileDetailsChanged,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "volatile-details-changed");

    add_handler<DRing::ConfigurationSignal::IncomingAccountMessage,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "incoming-account-message");

    add_handler<DRing::ConfigurationSignal::AccountMessageStatusChanged,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "account-message-status-changed");

    add_handler<DRing::ConfigurationSignal::ProfileReceived,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "profile-received");

    add_handler<DRing::ConfigurationSignal::ComposingStatusChanged,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "composing-status-changed");

    add_handler<DRing::ConfigurationSignal::IncomingTrustRequest,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::vector<uint8_t>&,
                time_t>(handlers, "incoming-trust-request");

    add_handler<DRing::ConfigurationSignal::ContactAdded,
                const std::string&,
                const std::string&,
                bool>(handlers, "contact-added");

    add_handler<DRing::ConfigurationSignal::ContactRemoved,
                const std::string&,
                const std::string&,
                bool>(handlers, "contact-removed");

    add_handler<DRing::ConfigurationSignal::ExportOnRingEnded,
                const std::string&,
                int,
                const std::string&>(handlers, "export-on-ring-ended");

    add_handler<DRing::ConfigurationSignal::NameRegistrationEnded,
                const std::string&,
                int,
                const std::string&>(handlers, "name-registration-ended");

    add_handler<DRing::ConfigurationSignal::KnownDevicesChanged,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "known-devices-changed");

    add_handler<DRing::ConfigurationSignal::RegisteredNameFound,
                const std::string&,
                int,
                const std::string&,
                const std::string&>(handlers, "registered-name-found");

    add_handler<DRing::ConfigurationSignal::UserSearchEnded,
                const std::string&,
                int,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "user-search-ended");

    add_handler<DRing::ConfigurationSignal::CertificatePinned,
                const std::string&>(handlers, "certificate-pinned");

    add_handler<DRing::ConfigurationSignal::CertificatePathPinned,
                const std::string&,
                const std::vector<std::string>&>(handlers, "certificate-path-pinned");

    add_handler<DRing::ConfigurationSignal::CertificateExpired,
                const std::string&>(handlers, "certificate-expired");

    add_handler<DRing::ConfigurationSignal::CertificateStateChanged,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "certificate-state-changed");

    add_handler<DRing::ConfigurationSignal::MediaParametersChanged,
                const std::string&>(handlers, "media-parameters-changed");

    add_handler<DRing::ConfigurationSignal::MigrationEnded, const std::string&, const std::string&>(
        handlers, "migration-ended");

    add_handler<DRing::ConfigurationSignal::DeviceRevocationEnded,
                const std::string&,
                const std::string&,
                int>(handlers, "device-revocation-ended");

    add_handler<DRing::ConfigurationSignal::AccountProfileReceived,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "account-profile-received");

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    add_handler<DRing::ConfigurationSignal::GetHardwareAudioFormat,
                std::vector<int32_t>*>(handlers, "get-hardware-audio-format");
#endif
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || defined(RING_UWP)
    add_handler<DRing::ConfigurationSignal::GetAppDataPath,
                const std::string&,
                std::vector<std::string>*>(handlers, "get-app-data-path");

    add_handler<DRing::ConfigurationSignal::GetDeviceName,
                std::vector<std::string>*>(handlers, "get-device-name");
#endif
    add_handler<DRing::ConfigurationSignal::HardwareDecodingChanged,
                bool>(handlers, "hardware-decoding-changed");

    add_handler<DRing::ConfigurationSignal::HardwareEncodingChanged,
                bool>(handlers, "hardware-encoding-changed");

    add_handler<DRing::ConfigurationSignal::MessageSend, const std::string&>(handlers,
                                                                             "message-send");

    /* Presence */
    add_handler<DRing::PresenceSignal::NewServerSubscriptionRequest,
                const std::string&>(handlers, "new-server-subscription-request");

    add_handler<DRing::PresenceSignal::ServerError,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "server-error");

    add_handler<DRing::PresenceSignal::NewBuddyNotification,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "new-buddy-notification");

    add_handler<DRing::PresenceSignal::NearbyPeerNotification,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "nearby-peer-notification");

    add_handler<DRing::PresenceSignal::SubscriptionStateChanged,
                const std::string&,
                const std::string&,
                int>(handlers, "subscription-state-changed");

    /* Audio */
    add_handler<DRing::AudioSignal::DeviceEvent>(handlers, "audio-device-event");

    add_handler<DRing::AudioSignal::AudioMeter, const std::string&, float>(handlers, "audio-meter");

    /* DataTransfer */
    add_handler<DRing::DataTransferSignal::DataTransferEvent,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "data-transfer-event");

#ifdef ENABLE_VIDEO
    /* MediaPlayer */
    add_handler<DRing::MediaPlayerSignal::FileOpened,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "media-file-opened");

    /* Video */
    add_handler<DRing::VideoSignal::DeviceEvent>(handlers, "device-event");

    add_handler<DRing::VideoSignal::DecodingStarted,
                const std::string&,
                const std::string&,
                int,
                int,
                bool>(handlers, "video-decoding-started");

    add_handler<DRing::VideoSignal::DecodingStopped, const std::string&, const std::string&, bool>(
        handlers, "video-decoding-stopped");

#ifdef __ANDROID__
    add_handler<DRing::VideoSignal::GetCameraInfo,
                const std::string&,
                std::vector<int>*,
                std::vector<unsigned>*,
                std::vector<unsigned>*>(handlers, "video-get-camera-info");

    add_handler<DRing::VideoSignal::SetParameters,
                const std::string&,
                const int,
                const int,
                const int,
                const int>(handlers, "video-set-parameters");

    add_handler<DRing::VideoSignal::RequestKeyFrame>(handlers, "video-request-key-frame");

    add_handler<DRing::VideoSignal::SetBitrate, const std::string&, const int>(handlers,
                                                                               "video-set-bitrate");

#endif
    add_handler<DRing::VideoSignal::StartCapture, const std::string&>(handlers,
                                                                      "video-start-capture");

    add_handler<DRing::VideoSignal::StopCapture>(handlers, "video-stop-capture");

    add_handler<DRing::VideoSignal::DeviceAdded, const std::string&>(handlers, "video-device-added");

    add_handler<DRing::VideoSignal::ParametersChanged,
                const std::string&>(handlers, "video-parameters-changed");
#endif

    /* Conversation */
    add_handler<DRing::ConversationSignal::ConversationLoaded,
                uint32_t,
                const std::string&,
                const std::string&,
                std::vector<std::map<std::string, std::string>>>(handlers, "conversation-loaded");

    add_handler<DRing::ConversationSignal::MessageReceived,
                const std::string&,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "message-received");

    add_handler<DRing::ConversationSignal::ConversationRequestReceived,
                const std::string&,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "conversation-request-received");

    add_handler<DRing::ConversationSignal::ConversationRequestDeclined,
                const std::string&,
                const std::string&>(handlers, "conversation-request-declined");

    add_handler<DRing::ConversationSignal::ConversationReady, const std::string&, const std::string&>(
        handlers, "conversation-ready");

    add_handler<DRing::ConversationSignal::ConversationRemoved,
                const std::string&,
                const std::string&>(handlers, "conversation-removed");

    add_handler<DRing::ConversationSignal::ConversationMemberEvent,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "conversation-member-event");

    add_handler<DRing::ConversationSignal::OnConversationError,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "conversation-error");

    DRing::registerSignalHandlers(handlers);
}
