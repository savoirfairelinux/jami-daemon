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
#ifndef DRING_H
#define DRING_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <functional>
#include <string>
#include <map>
#include <memory>
#include <type_traits>

namespace DRing {

enum class EventHandlerKey { CALL, CONFIG, PRESENCE, VIDEO };

/* error codes returned by functions of this API */
enum class InitResult {
    SUCCESS=0,
    ERR_MANAGER_INIT,
};

/* flags for initialization */
enum InitFlag {
    DRING_FLAG_DEBUG=1,
    DRING_FLAG_CONSOLE_LOG=2,
};

/* External Callback Dynamic Utilities
 *
 * The library provides to users a way to be acknowledged
 * when daemon's objects have a state change.
 * The user is aware of this changement wen the deamon calls
 * a user-given callback.
 * Daemon handles many of these callbacks, one per event type.
 * The user registers his callback when he calls daemon DRing:init().
 * Each callback has its own function signature.
 * To keep ABI compatibility we don't let user directly provides
 * his callbacks as it or through a structure.
 * This way bring ABI violation if we need to change the order
 * and/or the existance of any callback type.
 * Thus the user have to pass them using following template classes
 * and functions, that wraps user-callback in a generic and ABI-compatible way.
 */

/* Generic class to transit user callbacks to daemon library.
 * Used conjointly with std::shared_ptr to hide the concrete class.
 * See CallbackWrapper template for details.
 */
class CallbackWrapperBase {};

/* Concrete class of CallbackWrapperBase.
 * This class wraps callbacks of a specific signature.
 * Also used to obtain the user callback from a CallbackWrapperBase shared ptr.
 *
 * This class is CopyConstructible, CopyAssignable, MoveConstructible
 * and MoveAssignable.
 */
template <typename TProto>
class CallbackWrapper : public CallbackWrapperBase {
    private:
        using TFunc = std::function<TProto>;
        TFunc cb_; // The user-callback

    public:
        // Empty wrapper: no callback associated.
        // Used to initialize internal callback arrays.
        CallbackWrapper() noexcept {}

        // Create and initialize a wrapper to given callback.
        CallbackWrapper(TFunc&& func) noexcept {
            cb_ = std::forward<TFunc>(func);
        }

        // Create and initialize a wrapper from a generic CallbackWrapperBase
        // shared pointer.
        // Note: the given callback is copied into internal storage.
        CallbackWrapper(std::shared_ptr<CallbackWrapperBase> p) noexcept {
            if (p)
                cb_ = ((CallbackWrapper<TProto>*)p.get())->cb_;
        }

        // Return user-callback reference.
        // The returned std::function can be null-initialized if no callback
        // has been set.
        const constexpr TFunc& operator *() const noexcept {
            return cb_;
        }

        // Return boolean true value if a non-null callback has been set
        explicit constexpr operator bool() const noexcept {
            return static_cast<bool>(cb_);
        }
};

// Return an exportable callback object.
// This object is a std::pair of a string and a CallbackWrapperBase shared_ptr.
// This last wraps given callback in a ABI-compatible way.
// Note: this version accepts callback as a rvalue.
template <typename Ts>
std::pair<std::string, std::shared_ptr<CallbackWrapperBase>>
exportable_callback(std::function<typename Ts::cb_type>&& func) {
    return std::make_pair((const std::string&)Ts::name, std::make_shared<CallbackWrapper<typename Ts::cb_type>>(std::forward<std::function<typename Ts::cb_type>>(func)));
}

/* Return the library version */
const char* version() noexcept;

/**
 * Initializes libring.
 *
 * @param ev_handlers Event handlers
 * @param flags       Flags to customize this initialization
 * @returns           0 if successful or a negative error code
 */
InitResult
init(const std::map<EventHandlerKey,
     std::map<std::string, std::shared_ptr<CallbackWrapperBase>>>& ev_handlers,
     enum InitFlag flags);

/**
 * Finalizes libring, freeing any resource allocated by the library.
 */
void fini(void) noexcept;

/**
 * Poll for Daemon events
 */
void poll_events(void);

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
std::map<std::string, std::string> ring_config_get_ringtone_list(void);
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
void ring_config_set_accounts_order(const std::string& order);
std::map<std::string, std::string> ring_config_get_hook_settings(void);
void ring_config_set_hook_settings(const std::map<std::string, std::string>& settings);
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

} // namespace DRing

#endif /* DRING_H */
