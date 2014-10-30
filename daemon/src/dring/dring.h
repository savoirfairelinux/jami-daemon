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
#ifndef RING_H
#define RING_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <functional>
#include <string>
#include <map>

const char *
ring_version();

/* presence events */
struct ring_pres_ev_handlers
{
    std::function<void (const std::string& /*remote*/)> on_new_server_subscription_request;
    std::function<void (const std::string& /*account_id*/, const std::string& /*error*/, const std::string& /*msg*/)> on_server_error;
    std::function<void (const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*status*/, const std::string& /*line_status*/)> on_new_buddy_notification;
    std::function<void (const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*state*/)> on_subscription_state_change;
};

/* configuration events */
struct ring_config_ev_handlers
{
    std::function<void (const std::string& /*device*/, double /*value*/)> on_volume_change;
    std::function<void ()> on_accounts_change;
    std::function<void ()> on_history_change;
    std::function<void (const std::string& /*account_id*/)> on_stun_status_fail;
    std::function<void (const std::string& /*account_id*/, int /*state*/)> on_registration_state_change;
    std::function<void (const std::string& /*account_id*/, const std::string& /*state*/, int /*code*/)> on_sip_registration_state_change;
    std::function<void (const std::string& /*account_id*/, const std::map<std::string, std::string>& /* details */)> on_volatile_details_change;
    std::function<void (int /*alert*/)> on_error;
};

/* call events */
struct ring_call_ev_handlers
{
    std::function<void (const std::string& /*call_id*/, const std::string& /*state*/)> on_state_change;
    std::function<void ()> on_transfer_fail;
    std::function<void ()> on_transfer_success;
    std::function<void (const std::string& /*path*/)> on_record_playback_stopped;
    std::function<void (const std::string& /*call_id*/, int /*nd_msg*/)> on_voice_mail_notify;
    std::function<void (const std::string& /*id*/, const std::string& /*from*/, const std::string& /*msg*/)> on_incoming_message;
    std::function<void (const std::string& /*account_id*/, const std::string& /*call_id*/, const std::string& /*from*/)> on_incoming_call;
    std::function<void (const std::string& /*id*/, const std::string& /*filename*/)> on_record_playback_filepath;
    std::function<void (const std::string& /*conf_id*/)> on_conference_created;
    std::function<void (const std::string& /*conf_id*/, const std::string& /*state*/)> on_conference_changed;
    std::function<void (const std::string& /*filepath*/, int /*position*/, int /*scale*/)> on_update_playback_scale;
    std::function<void (const std::string& /*conf_id*/)> on_conference_remove;
    std::function<void (const std::string& /*account_id*/, const std::string& /*call_id*/, const std::string& /*to*/)> on_new_call;
    std::function<void (const std::string& /*call_id*/, const std::string& /*state*/, int /*code*/)> on_sip_call_state_change;
    std::function<void (const std::string& /*call_id*/, int /*state*/)> on_record_state_change;
    std::function<void (const std::string& /*call_id*/)> on_secure_sdes_on;
    std::function<void (const std::string& /*call_id*/)> on_secure_sdes_off;
    std::function<void (const std::string& /*call_id*/, const std::string& /*cipher*/)> on_secure_zrtp_on;
    std::function<void (const std::string& /*call_id*/)> on_secure_zrtp_off;
    std::function<void (const std::string& /*call_id*/, const std::string& /*sas*/, int /*verified*/)> on_show_sas;
    std::function<void (const std::string& /*call_id*/)> on_zrtp_not_supp_other;
    std::function<void (const std::string& /*call_id*/, const std::string& /*reason*/, const std::string& /*severity*/)> on_zrtp_negotiation_fail;
    std::function<void (const std::string& /*call_id*/, const std::map<std::string, int>& /*stats*/)> on_rtcp_receive_report;
};

/* video events */
#ifdef RING_VIDEO
struct ring_video_ev_handlers
{
    std::function<void ()> on_device_event;
    std::function<void (const std::string& /*id*/, const std::string& /*shm_path*/, int /*w*/, int /*h*/, bool /*is_mixer*/)> on_start_decoding;
    std::function<void (const std::string& /*id*/, const std::string& /*shm_path*/, bool /*is_mixer*/)> on_stop_decoding;
};
#endif /* RING_VIDEO */

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/* all handlers */
struct ring_ev_handlers
{
    ring_call_ev_handlers call_ev_handlers;
    ring_config_ev_handlers config_ev_handlers;
    ring_pres_ev_handlers pres_ev_handlers;

#ifdef RING_VIDEO
    ring_video_ev_handlers video_ev_handlers;
#endif /* RING_VIDEO */
};

#pragma GCC diagnostic warning "-Wmissing-field-initializers"

/* error codes returned by functions of this API */
enum ring_error {
    RING_ERR_MANAGER_INIT,
    RING_ERR_UNKNOWN,
};

/* flags for initialization */
enum ring_init_flag {
    RING_FLAG_DEBUG = 1,
    RING_FLAG_CONSOLE_LOG = 2,
};

/**
 * Initializes libring.
 *
 * @param ev_handlers Event handlers
 * @param flags       Flags to customize this initialization
 * @returns           0 if successful or a negative error code
 */
int ring_init(struct ring_ev_handlers* ev_handlers, enum ring_init_flag flags);

/**
 * Finalizes libring, freeing any resource allocated by the library.
 */
void ring_fini(void);

/**
 * Poll for SIP/IAX events
 */
void ring_poll_events(void);

/* call API */
bool ring_call_place(const std::string& account_id, const std::string& call_id, const std::string& to);
bool ring_call_refuse(const std::string& call_id);
bool ring_call_accept(const std::string& call_id);
bool ring_call_hang_up(const std::string& call_id);
bool ring_call_hold(const std::string& call_id);
bool ring_call_unhold(const std::string& call_id);
bool ring_call_transfer(const std::string& call_id, const std::string& to);
bool ring_call_attended_transfer(const std::string& transfer_id, const std::string& target_id);
std::map<std::string, std::string> ring_call_get_call_details(const std::string& call_id);
std::vector<std::string> ring_call_get_call_list(void);
void ring_call_remove_conference(const std::string& conf_id);
bool ring_call_join_participant(const std::string& sel_call_id, const std::string& drag_call_id);
void ring_call_create_conf_from_participant_list(const std::vector<std::string>& participants);
bool ring_call_is_conference_participant(const std::string& call_id);
bool ring_call_add_participant(const std::string& call_id, const std::string& conf_id);
bool ring_call_add_main_participant(const std::string& conf_id);
bool ring_call_detach_participant(const std::string& call_id);
bool ring_call_join_conference(const std::string& sel_conf_id, const std::string& drag_conf_id);
bool ring_call_hang_up_conference(const std::string& conf_id);
bool ring_call_hold_conference(const std::string& conf_id);
bool ring_call_unhold_conference(const std::string& conf_id);
std::vector<std::string> ring_call_get_conference_list(void);
std::vector<std::string> ring_call_get_participant_list(const std::string& conf_id);
std::vector<std::string> ring_call_get_display_names(const std::string& conf_id);
std::string ring_call_get_conference_id(const std::string& call_id);
std::map<std::string, std::string> ring_call_get_conference_details(const std::string& call_id);
bool ring_call_play_recorded_file(const std::string& path);
void ring_call_stop_recorded_file(const std::string& path);
bool ring_call_toggle_recording(const std::string& call_id);
void ring_call_set_recording(const std::string& call_id);
void ring_call_record_playback_seek(double pos);
bool ring_call_is_recording(const std::string& call_id);
std::string ring_call_get_current_audio_codec_name(const std::string& call_id);
void ring_call_play_dtmf(const std::string& key);
void ring_call_start_tone(int start, int type);
void ring_call_set_sas_verified(const std::string& call_id);
void ring_call_reset_sas_verified(const std::string& call_id);
void ring_call_set_confirm_go_clear(const std::string& call_id);
void ring_call_request_go_clear(const std::string& call_id);
void ring_call_accept_enrollment(const std::string& call_id, bool accepted);
void ring_call_send_text_message(const std::string& call_id, const std::string& message);

/* configuration API */
std::map<std::string, std::string> ring_config_get_account_details(const std::string& account_id);
std::map<std::string, std::string> ring_config_get_volatile_account_details(const std::string& account_id);
void ring_config_set_account_details(const std::string& account_id, const std::map<std::string, std::string>& details);
std::map<std::string, std::string> ring_config_get_account_template(void);
std::string ring_config_add_account(const std::map<std::string, std::string>& details);
void ring_config_remove_account(const std::string& account_id);
std::vector<std::string> ring_config_get_account_list(void);
void ring_config_send_register(const std::string& account_id, bool enable);
void ring_config_register_all_accounts(void);
std::map<std::string, std::string> ring_config_get_tls_default_settings(void);
std::vector<int> ring_config_get_audio_codec_list(void);
std::vector<std::string> ring_config_get_supported_tls_method(void);
std::vector<std::string> ring_config_get_supported_ciphers(const std::string& account_id);
std::vector<std::string> ring_config_get_audio_codec_details(int payload);
std::vector<int> ring_config_get_active_audio_codec_list(const std::string& account_id);
void ring_config_set_active_audio_codec_list(const std::vector<std::string>& list, const std::string& account_id);
std::vector<std::string> ring_config_get_audio_plugin_list(void);
void ring_config_set_audio_plugin(const std::string& audio_plugin);
std::vector<std::string> ring_config_get_audio_output_device_list();
void ring_config_set_audio_output_device(int index);
void ring_config_set_audio_input_device(int index);
void ring_config_set_audio_ringtone_device(int index);
std::vector<std::string> ring_config_get_audio_input_device_list(void);
std::vector<std::string> ring_config_get_current_audio_devices_index(void);
int ring_config_get_audio_input_device_index(const std::string& name);
int ring_config_get_audio_output_device_index(const std::string& name);
std::string ring_config_get_current_audio_output_plugin(void);
bool ring_config_get_noise_suppress_state(void);
void ring_config_set_noise_suppress_state(bool state);
bool ring_config_is_agc_enabled(void);
void ring_config_enable_agc(bool enabled);
void ring_config_mute_dtmf(bool mute);
bool ring_config_is_dtmf_muted(void);
bool ring_config_is_capture_muted(void);
void ring_config_mute_capture(bool mute);
bool ring_config_is_playback_muted(void);
void ring_config_mute_playback(int mute);
std::string ring_config_get_audio_manager(void);
bool ring_config_set_audio_manager(const std::string& api);
std::vector<std::string> ring_config_get_supported_audio_managers(void);
int ring_config_is_iax2_enabled(void);
std::string ring_config_get_record_path(void);
void ring_config_set_record_path(const std::string& path);
bool ring_config_is_always_recording(void);
void ring_config_set_always_recording(bool rec);
void ring_config_set_history_limit(int days);
int ring_config_get_history_limit(void);
void ring_config_clear_history(void);
void ring_config_set_accounts_order(const std::string& order);
std::map<std::string, std::string> ring_config_get_hook_settings(void);
void ring_config_set_hook_settings(const std::map<std::string, std::string>& settings);
std::vector<std::map<std::string, std::string>> ring_config_get_history(void);
std::map<std::string, std::string> ring_config_get_tls_settings();
std::map<std::string, std::string> ring_config_validate_certificate(const std::string& accountId,
    const std::string& certificate, const std::string& private_key);
std::map<std::string, std::string> ring_config_get_certificate_details(const std::string& certificate);
void ring_config_set_tls_settings(const std::map< std::string, std::string >& settings);
std::map<std::string, std::string> ring_config_get_ip2ip_details(void);
std::vector<std::map<std::string, std::string>> ring_config_get_credentials(const std::string& account_id);
void ring_config_set_credentials(const std::string& account_id, const std::vector<std::map<std::string, std::string>>& details);
std::string ring_config_get_addr_from_interface_name(const std::string& interface);
std::vector<std::string> ring_config_get_all_ip_interface(void);
std::vector<std::string> ring_config_get_all_ip_interface_by_name(void);
std::map<std::string, std::string> ring_config_get_shortcuts();
void ring_config_set_shortcuts(const std::map<std::string, std::string>& shortcuts);
void ring_config_set_volume(const std::string& device, double value);
double ring_config_get_volume(const std::string& device);

/* presence API */
void ring_pres_publish(const std::string& account_id, int status, const std::string& note);
void ring_pres_answer_server_request(const std::string& uri, int flag);
void ring_pres_subscribe_buddy(const std::string& account_id, const std::string& uri, int flag);
std::vector<std::map<std::string, std::string>> ring_pres_get_subscriptions(const std::string& account_id);
void ring_pres_set_subscriptions(const std::string& account_id, const std::vector<std::string>& uris);

/* video API */
#ifdef RING_VIDEO
std::vector<std::map<std::string, std::string>> ring_video_get_codecs(const std::string& account_id);
void ring_video_set_codecs(const std::string& account_id, const std::vector<std::map<std::string, std::string>>& details);
std::vector<std::string> ring_video_get_device_list(void);
std::map<std::string, std::map<std::string, std::vector<std::string>>> ring_video_get_capabilities(const std::string& name);
std::map<std::string, std::string> ring_video_get_settings(const std::string& name);
void ring_video_set_default_device(const std::string& dev);
std::string ring_video_get_default_device(void);
std::string ring_video_get_current_codec_name(const std::string& call_id);
void ring_video_start_camera(void);
void ring_video_stop_camera(void);
bool ring_video_switch_input(const std::string& resource);
bool ring_video_is_camera_started(void);
void ring_video_apply_settings(const std::string& name, const std::map<std::string, std::string>& settings);
#endif /* RING_VIDEO */

#endif /* RING_H */
