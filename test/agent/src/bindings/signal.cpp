/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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
            } else {
                scm_gc_unprotect_object(cb);
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

    if (scm_is_false(scm_procedure_p(handler_proc))) {
        scm_wrong_type_arg_msg("on_signal_binding", 0, handler_proc, "procedure");
    }

    handler_pair = scm_assq_ref(signal_alist, signal_sym);

    if (scm_is_false(handler_pair)) {
        scm_throw(bad_signal_sym, scm_list_2(signal_sym, handler_proc));
    }

    callbacks = static_cast<std::vector<SCM>*>(scm_to_pointer(scm_car(handler_pair)));
    mutex = static_cast<std::mutex*>(scm_to_pointer(scm_cdr(handler_pair)));

    std::unique_lock lck(*mutex);
    scm_gc_protect_object(handler_proc);
    callbacks->push_back(handler_proc);

    return SCM_UNDEFINED;
}

template<typename T, typename... Args>
void
add_handler(std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>>& handlers,
            const char* name)
{
    static Handler<Args...> handler(name);

    auto fn = [=](Args... args) {
        handler.execute(args...);
    };

    handlers.insert(libjami::exportable_callback<T>(std::move(fn)));
}

void
install_signal_primitives(void*)
{
    define_primitive("on-signal", 2, 0, 0, (void*) on_signal_binding);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;

    add_handler<libjami::CallSignal::StateChange,
                const std::string&,
                const std::string&,
                const std::string&,
                signed>(handlers, "state-changed");

    add_handler<libjami::CallSignal::TransferFailed>(handlers, "transfer-failed");

    add_handler<libjami::CallSignal::TransferSucceeded>(handlers, "transfer-succeeded");

    add_handler<libjami::CallSignal::RecordPlaybackStopped,
                const std::string&>(handlers, "record-playback-stopped");

    add_handler<libjami::CallSignal::VoiceMailNotify, const std::string&, int32_t, int32_t, int32_t>(
        handlers, "voice-mail-notify");

    add_handler<libjami::CallSignal::IncomingMessage,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "incoming-message");

    add_handler<libjami::CallSignal::IncomingCall,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "incoming-call");

    add_handler<libjami::CallSignal::IncomingCallWithMedia,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::vector<libjami::MediaMap>>(handlers, "incoming-call/media");

    add_handler<libjami::CallSignal::MediaChangeRequested,
                const std::string&,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "media-change-requested");

    add_handler<libjami::CallSignal::RecordPlaybackFilepath, const std::string&, const std::string&>(
        handlers, "record-playback-filepath");

    add_handler<libjami::CallSignal::ConferenceCreated, const std::string&, const std::string&>(
        handlers, "conference-created");

    add_handler<libjami::CallSignal::ConferenceChanged,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "conference-changed");

    add_handler<libjami::CallSignal::UpdatePlaybackScale, const std::string&, unsigned, unsigned>(
        handlers, "update-playback-scale");

    add_handler<libjami::CallSignal::ConferenceRemoved, const std::string&, const std::string&>(
        handlers, "conference-removed");

    add_handler<libjami::CallSignal::RecordingStateChanged, const std::string&, int>(
        handlers, "recording-state-changed");

    add_handler<libjami::CallSignal::RtcpReportReceived,
                const std::string&,
                const std::map<std::string, int>&>(handlers, "rtcp-report-received");

    add_handler<libjami::CallSignal::PeerHold, const std::string&, bool>(handlers, "peer-hold");

    add_handler<libjami::CallSignal::VideoMuted, const std::string&, bool>(handlers, "video-muted");

    add_handler<libjami::CallSignal::AudioMuted, const std::string&, bool>(handlers, "audio-muted");

    add_handler<libjami::CallSignal::SmartInfo,
                const std::map<std::string, std::string>&>(handlers, "smart-info");

    add_handler<libjami::CallSignal::ConnectionUpdate, const std::string&, int>(handlers,
                                                                              "connection-update");

    add_handler<libjami::CallSignal::OnConferenceInfosUpdated,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "conference-infos-updated");

    add_handler<libjami::CallSignal::RemoteRecordingChanged,
                const std::string&,
                const std::string&,
                bool>(handlers, "remote-recording-changed");

    add_handler<libjami::CallSignal::MediaNegotiationStatus,
                const std::string&,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "media-negotiation-status");

    /* Configuration */
    add_handler<libjami::ConfigurationSignal::VolumeChanged, const std::string&, double>(
        handlers, "volume-changed");

    add_handler<libjami::ConfigurationSignal::Error, int>(handlers, "configuration-error");

    add_handler<libjami::ConfigurationSignal::AccountsChanged>(handlers, "accounts-changed");

    add_handler<libjami::ConfigurationSignal::AccountDetailsChanged,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "account-details-changed");

    add_handler<libjami::ConfigurationSignal::StunStatusFailed,
                const std::string&>(handlers, "stun-status-failed");

    add_handler<libjami::ConfigurationSignal::RegistrationStateChanged,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "registration-state-changed");

    add_handler<libjami::ConfigurationSignal::VolatileDetailsChanged,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "volatile-details-changed");

    add_handler<libjami::ConfigurationSignal::IncomingAccountMessage,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "incoming-account-message");

    add_handler<libjami::ConfigurationSignal::AccountMessageStatusChanged,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "account-message-status-changed");

    add_handler<libjami::ConfigurationSignal::ProfileReceived,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "profile-received");

    add_handler<libjami::ConfigurationSignal::ComposingStatusChanged,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "composing-status-changed");

    add_handler<libjami::ConfigurationSignal::IncomingTrustRequest,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::vector<uint8_t>&,
                time_t>(handlers, "incoming-trust-request");

    add_handler<libjami::ConfigurationSignal::ContactAdded,
                const std::string&,
                const std::string&,
                bool>(handlers, "contact-added");

    add_handler<libjami::ConfigurationSignal::ContactRemoved,
                const std::string&,
                const std::string&,
                bool>(handlers, "contact-removed");

    add_handler<libjami::ConfigurationSignal::ExportOnRingEnded,
                const std::string&,
                int,
                const std::string&>(handlers, "export-on-ring-ended");

    add_handler<libjami::ConfigurationSignal::NameRegistrationEnded,
                const std::string&,
                int,
                const std::string&>(handlers, "name-registration-ended");

    add_handler<libjami::ConfigurationSignal::KnownDevicesChanged,
                const std::string&,
                const std::map<std::string, std::string>&>(handlers, "known-devices-changed");

    add_handler<libjami::ConfigurationSignal::RegisteredNameFound,
                const std::string&,
                int,
                const std::string&,
                const std::string&>(handlers, "registered-name-found");

    add_handler<libjami::ConfigurationSignal::UserSearchEnded,
                const std::string&,
                int,
                const std::string&,
                const std::vector<std::map<std::string, std::string>>&>(handlers,
                                                                        "user-search-ended");

    add_handler<libjami::ConfigurationSignal::CertificatePinned,
                const std::string&>(handlers, "certificate-pinned");

    add_handler<libjami::ConfigurationSignal::CertificatePathPinned,
                const std::string&,
                const std::vector<std::string>&>(handlers, "certificate-path-pinned");

    add_handler<libjami::ConfigurationSignal::CertificateExpired,
                const std::string&>(handlers, "certificate-expired");

    add_handler<libjami::ConfigurationSignal::CertificateStateChanged,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "certificate-state-changed");

    add_handler<libjami::ConfigurationSignal::MediaParametersChanged,
                const std::string&>(handlers, "media-parameters-changed");

    add_handler<libjami::ConfigurationSignal::MigrationEnded, const std::string&, const std::string&>(
        handlers, "migration-ended");

    add_handler<libjami::ConfigurationSignal::DeviceRevocationEnded,
                const std::string&,
                const std::string&,
                int>(handlers, "device-revocation-ended");

    add_handler<libjami::ConfigurationSignal::AccountProfileReceived,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "account-profile-received");

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    add_handler<libjami::ConfigurationSignal::GetHardwareAudioFormat,
                std::vector<int32_t>*>(handlers, "get-hardware-audio-format");
#endif
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || defined(RING_UWP)
    add_handler<libjami::ConfigurationSignal::GetAppDataPath,
                const std::string&,
                std::vector<std::string>*>(handlers, "get-app-data-path");

    add_handler<libjami::ConfigurationSignal::GetDeviceName,
                std::vector<std::string>*>(handlers, "get-device-name");
#endif
    add_handler<libjami::ConfigurationSignal::HardwareDecodingChanged,
                bool>(handlers, "hardware-decoding-changed");

    add_handler<libjami::ConfigurationSignal::HardwareEncodingChanged,
                bool>(handlers, "hardware-encoding-changed");

    add_handler<libjami::ConfigurationSignal::MessageSend, const std::string&>(handlers,
                                                                             "message-send");

    /* Presence */
    add_handler<libjami::PresenceSignal::NewServerSubscriptionRequest,
                const std::string&>(handlers, "new-server-subscription-request");

    add_handler<libjami::PresenceSignal::ServerError,
                const std::string&,
                const std::string&,
                const std::string&>(handlers, "server-error");

    add_handler<libjami::PresenceSignal::NewBuddyNotification,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "new-buddy-notification");

    add_handler<libjami::PresenceSignal::NearbyPeerNotification,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "nearby-peer-notification");

    add_handler<libjami::PresenceSignal::SubscriptionStateChanged,
                const std::string&,
                const std::string&,
                int>(handlers, "subscription-state-changed");

    /* Audio */
    add_handler<libjami::AudioSignal::DeviceEvent>(handlers, "audio-device-event");

    add_handler<libjami::AudioSignal::AudioMeter, const std::string&, float>(handlers, "audio-meter");

    /* DataTransfer */
    add_handler<libjami::DataTransferSignal::DataTransferEvent,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "data-transfer-event");

#ifdef ENABLE_VIDEO
    /* MediaPlayer */
    add_handler<libjami::MediaPlayerSignal::FileOpened,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "media-file-opened");

    /* Video */
    add_handler<libjami::VideoSignal::DeviceEvent>(handlers, "device-event");

    add_handler<libjami::VideoSignal::DecodingStarted,
                const std::string&,
                const std::string&,
                int,
                int,
                bool>(handlers, "video-decoding-started");

    add_handler<libjami::VideoSignal::DecodingStopped, const std::string&, const std::string&, bool>(
        handlers, "video-decoding-stopped");

#ifdef __ANDROID__
    add_handler<libjami::VideoSignal::GetCameraInfo,
                const std::string&,
                std::vector<int>*,
                std::vector<unsigned>*,
                std::vector<unsigned>*>(handlers, "video-get-camera-info");

    add_handler<libjami::VideoSignal::SetParameters,
                const std::string&,
                const int,
                const int,
                const int,
                const int>(handlers, "video-set-parameters");

    add_handler<libjami::VideoSignal::RequestKeyFrame>(handlers, "video-request-key-frame");

    add_handler<libjami::VideoSignal::SetBitrate, const std::string&, const int>(handlers,
                                                                               "video-set-bitrate");

#endif
    add_handler<libjami::VideoSignal::StartCapture, const std::string&>(handlers,
                                                                      "video-start-capture");

    add_handler<libjami::VideoSignal::StopCapture>(handlers, "video-stop-capture");

    add_handler<libjami::VideoSignal::DeviceAdded, const std::string&>(handlers, "video-device-added");

    add_handler<libjami::VideoSignal::ParametersChanged,
                const std::string&>(handlers, "video-parameters-changed");
#endif

    /* Conversation */
    add_handler<libjami::ConversationSignal::ConversationLoaded,
                uint32_t,
                const std::string&,
                const std::string&,
                std::vector<std::map<std::string, std::string>>>(handlers, "conversation-loaded");

    add_handler<libjami::ConversationSignal::MessagesFound,
                uint32_t,
                const std::string&,
                const std::string&,
                std::vector<std::map<std::string, std::string>>>(handlers, "messages-found");

    add_handler<libjami::ConversationSignal::MessageReceived,
                const std::string&,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "message-received");

    add_handler<libjami::ConversationSignal::ConversationRequestReceived,
                const std::string&,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "conversation-request-received");

    add_handler<libjami::ConversationSignal::ConversationRequestDeclined,
                const std::string&,
                const std::string&>(handlers, "conversation-request-declined");

    add_handler<libjami::ConversationSignal::ConversationReady, const std::string&, const std::string&>(
        handlers, "conversation-ready");

    add_handler<libjami::ConversationSignal::ConversationRemoved,
                const std::string&,
                const std::string&>(handlers, "conversation-removed");

    add_handler<libjami::ConversationSignal::ConversationMemberEvent,
                const std::string&,
                const std::string&,
                const std::string&,
                int>(handlers, "conversation-member-event");

    add_handler<libjami::ConversationSignal::OnConversationError,
                const std::string&,
                const std::string&,
                int,
                const std::string&>(handlers, "conversation-error");

    add_handler<libjami::ConversationSignal::ConversationPreferencesUpdated,
                const std::string&,
                const std::string&,
                std::map<std::string, std::string>>(handlers, "conversation-preferences-updated");

    libjami::registerSignalHandlers(handlers);
}
