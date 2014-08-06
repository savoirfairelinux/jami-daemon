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
#include <string>
#include <vector>
#include <map>
#include <cstdlib>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"
#include "managerimpl.h"
#include "logger.h"
#include "sflphone.h"
#include "client/callmanager.h"
#include "client/configurationmanager.h"

#ifdef SFL_PRESENCE
#include "client/presencemanager.h"
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
#include "client/videomanager.h"
#endif // SFL_VIDEO

namespace {
    // This manager pointer is only set after proper library initialization.
    ManagerImpl* _manager = nullptr;

    CallManager* _getCallManager()
    {
        return _manager->getClient()->getCallManager();
    }

    ConfigurationManager* _getConfigurationManager()
    {
        return _manager->getClient()->getConfigurationManager();
    }

#ifdef SFL_PRESENCE
    PresenceManager* _getPresenceManager()
    {
        return _manager->getClient()->getPresenceManager();
    }
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    VideoManager* _getVideoManager()
    {
        return _manager->getClient()->getVideoManager();
    }
#endif // SFL_VIDEO

    // User handlers of library events
    struct sflph_ev_handlers _evHandlers;
}

const char *
sflph_version()
{
    return PACKAGE_VERSION;
}

int sflph_init(struct sflph_ev_handlers* ev_handlers, enum sflph_init_flag flags)
{
    // Ignore initialization if already done
    if (_manager) {
        return 0;
    }

    // Copy user event handlers
    _evHandlers = *ev_handlers;

    // Handle flags
    setDebugMode((flags & SFLPH_FLAG_DEBUG) ? true : false);
    setConsoleLog((flags & SFLPH_FLAG_CONSOLE_LOG) ? true : false);

    // Create manager
    try {
        _manager = &(Manager::instance());
    } catch (...) {
        return -SFLPH_ERR_MANAGER_INIT;
    }

    // Register user event handlers
    _getCallManager()->registerEvHandlers(&_evHandlers.call_ev_handlers);
    _getConfigurationManager()->registerEvHandlers(&_evHandlers.config_ev_handlers);

#ifdef SFL_PRESENCE
    _getPresenceManager()->registerEvHandlers(&_evHandlers.pres_ev_handlers);
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    _getVideoManager()->registerEvHandlers(&_evHandlers.video_ev_handlers);
#endif // SFL_VIDEO

    // Initialize manager now
    try {
        _manager->init("");
    } catch (...) {
        _manager = nullptr;
        return -SFLPH_ERR_MANAGER_INIT;
    }

    return 0;
}

void sflph_fini(void)
{
    // Ignore if not yet initialized
    if (!_manager) {
        return;
    }

    // Finish manager
    _manager->finish();
    _manager = nullptr;
}

void sflph_poll_events()
{
    _manager->pollEvents();
}

bool sflph_call_place(const std::string& account_id, const std::string& call_id, const std::string& to)
{
    return _getCallManager()->placeCall(account_id, call_id, to);
}

bool sflph_call_refuse(const std::string& call_id)
{
    return _getCallManager()->refuse(call_id);
}

bool sflph_call_accept(const std::string& call_id)
{
    return _getCallManager()->accept(call_id);
}

bool sflph_call_hang_up(const std::string& call_id)
{
    return _getCallManager()->hangUp(call_id);
}

bool sflph_call_hold(const std::string& call_id)
{
    return _getCallManager()->hold(call_id);
}

bool sflph_call_unhold(const std::string& call_id)
{
    return _getCallManager()->unhold(call_id);
}

bool sflph_call_transfer(const std::string& call_id, const std::string& to)
{
    return _getCallManager()->transfer(call_id, to);
}

bool sflph_call_attended_transfer(const std::string& transfer_id, const std::string& target_id)
{
    return _getCallManager()->attendedTransfer(transfer_id, target_id);
}

std::map<std::string, std::string> sflph_call_get_call_details(const std::string& call_id)
{
    return _getCallManager()->getCallDetails(call_id);
}

std::vector<std::string> sflph_call_get_call_list(void)
{
    return _getCallManager()->getCallList();
}

void sflph_call_remove_conference(const std::string& conf_id)
{
    _getCallManager()->removeConference(conf_id);
}

bool sflph_call_join_participant(const std::string& sel_call_id, const std::string& drag_call_id)
{
    return _getCallManager()->joinParticipant(sel_call_id, drag_call_id);
}

void sflph_call_create_conf_from_participant_list(const std::vector<std::string>& participants)
{
    _getCallManager()->createConfFromParticipantList(participants);
}

bool sflph_call_is_conference_participant(const std::string& call_id)
{
    return _getCallManager()->isConferenceParticipant(call_id);
}

bool sflph_call_add_participant(const std::string& call_id, const std::string& conf_id)
{
    return _getCallManager()->addParticipant(call_id, conf_id);
}

bool sflph_call_add_main_participant(const std::string& conf_id)
{
    return _getCallManager()->addMainParticipant(conf_id);
}

bool sflph_call_detach_participant(const std::string& call_id)
{
    return _getCallManager()->detachParticipant(call_id);
}

bool sflph_call_join_conference(const std::string& sel_conf_id, const std::string& drag_conf_id)
{
    return _getCallManager()->joinConference(sel_conf_id, drag_conf_id);
}

bool sflph_call_hang_up_conference(const std::string& conf_id)
{
    return _getCallManager()->hangUpConference(conf_id);
}

bool sflph_call_hold_conference(const std::string& conf_id)
{
    return _getCallManager()->holdConference(conf_id);
}

bool sflph_call_unhold_conference(const std::string& conf_id)
{
    return _getCallManager()->unholdConference(conf_id);
}

std::vector<std::string> sflph_call_get_conference_list(void)
{
    return _getCallManager()->getConferenceList();
}

std::vector<std::string> sflph_call_get_participant_list(const std::string& conf_id)
{
    return _getCallManager()->getParticipantList(conf_id);
}

std::vector<std::string> sflph_call_get_display_names(const std::string& conf_id)
{
    return _getCallManager()->getDisplayNames(conf_id);
}

std::string sflph_call_get_conference_id(const std::string& call_id)
{
    return _getCallManager()->getConferenceId(call_id);
}

std::map<std::string, std::string> sflph_call_get_conference_details(const std::string& call_id)
{
    return _getCallManager()->getConferenceDetails(call_id);
}

bool sflph_call_play_recorded_file(const std::string& path)
{
    return _getCallManager()->startRecordedFilePlayback(path);
}

void sflph_call_stop_recorded_file(const std::string& path)
{
    _getCallManager()->stopRecordedFilePlayback(path);
}

bool sflph_call_toggle_recording(const std::string& call_id)
{
    return _getCallManager()->toggleRecording(call_id);
}

void sflph_call_set_recording(const std::string& call_id)
{
    _getCallManager()->setRecording(call_id);
}

void sflph_call_record_playback_seek(double pos)
{
    _getCallManager()->recordPlaybackSeek(pos);
}

bool sflph_call_is_recording(const std::string& call_id)
{
    return _getCallManager()->getIsRecording(call_id);
}

std::string sflph_call_get_current_audio_codec_name(const std::string& call_id)
{
    return _getCallManager()->getCurrentAudioCodecName(call_id);
}

void sflph_call_play_dtmf(const std::string& key)
{
    _getCallManager()->playDTMF(key);
}

void sflph_call_start_tone(int start, int type)
{
    _getCallManager()->startTone(start, type);
}

void sflph_call_set_sas_verified(const std::string& call_id)
{
    _getCallManager()->setSASVerified(call_id);
}

void sflph_call_reset_sas_verified(const std::string& call_id)
{
    _getCallManager()->resetSASVerified(call_id);
}

void sflph_call_set_confirm_go_clear(const std::string& call_id)
{
    _getCallManager()->setConfirmGoClear(call_id);
}

void sflph_call_request_go_clear(const std::string& call_id)
{
    _getCallManager()->requestGoClear(call_id);
}

void sflph_call_accept_enrollment(const std::string& call_id, bool accepted)
{
    _getCallManager()->acceptEnrollment(call_id, accepted);
}

void sflph_call_send_text_message(const std::string& call_id, const std::string& message)
{
    _getCallManager()->sendTextMessage(call_id, message);
}

std::map<std::string, std::string> sflph_config_get_account_details(const std::string& account_id)
{
    return _getConfigurationManager()->getAccountDetails(account_id);
}

void sflph_config_set_account_details(const std::string& account_id, const std::map<std::string, std::string>& details)
{
    _getConfigurationManager()->setAccountDetails(account_id, details);
}

std::map<std::string, std::string> sflph_config_get_account_template(void)
{
    return _getConfigurationManager()->getAccountTemplate();
}

std::string sflph_config_add_account(const std::map<std::string, std::string>& details)
{
    return _getConfigurationManager()->addAccount(details);
}

void sflph_config_remove_account(const std::string& account_id)
{
    _getConfigurationManager()->removeAccount(account_id);
}

std::vector<std::string> sflph_config_get_account_list(void)
{
    return _getConfigurationManager()->getAccountList();
}

void sflph_config_send_register(const std::string& account_id, bool enable)
{
    _getConfigurationManager()->sendRegister(account_id, enable);
}

void sflph_config_register_all_accounts(void)
{
    _getConfigurationManager()->registerAllAccounts();
}

std::map<std::string, std::string> sflph_config_get_tls_default_settings(void)
{
    return _getConfigurationManager()->getTlsSettingsDefault();
}

std::vector<int> sflph_config_get_audio_codec_list(void)
{
    return _getConfigurationManager()->getAudioCodecList();
}

std::vector<std::string> sflph_config_get_supported_tls_method(void)
{
    return _getConfigurationManager()->getSupportedTlsMethod();
}

std::vector<std::string> sflph_config_get_audio_codec_details(int payload)
{
    return _getConfigurationManager()->getAudioCodecDetails(payload);
}

std::vector<int> sflph_config_get_active_audio_codec_list(const std::string& account_id)
{
    return _getConfigurationManager()->getActiveAudioCodecList(account_id);
}

void sflph_config_set_active_audio_codec_list(const std::vector<std::string>& list, const std::string& account_id)
{
    _getConfigurationManager()->setActiveAudioCodecList(list, account_id);
}

std::vector<std::string> sflph_config_get_audio_plugin_list(void)
{
    return _getConfigurationManager()->getAudioPluginList();
}

void sflph_config_set_audio_plugin(const std::string& audio_plugin)
{
    _getConfigurationManager()->setAudioPlugin(audio_plugin);
}

std::vector<std::string> sflph_config_get_audio_output_device_list()
{
    return _getConfigurationManager()->getAudioOutputDeviceList();
}

void sflph_config_set_audio_output_device(int index)
{
    _getConfigurationManager()->setAudioOutputDevice(index);
}

void sflph_config_set_audio_input_device(int index)
{
    _getConfigurationManager()->setAudioInputDevice(index);
}

void sflph_config_set_audio_ringtone_device(int index)
{
    _getConfigurationManager()->setAudioRingtoneDevice(index);
}

std::vector<std::string> sflph_config_get_audio_input_device_list(void)
{
    return _getConfigurationManager()->getAudioInputDeviceList();
}

std::vector<std::string> sflph_config_get_current_audio_devices_index(void)
{
    return _getConfigurationManager()->getCurrentAudioDevicesIndex();
}

int sflph_config_get_audio_input_device_index(const std::string& name)
{
    return _getConfigurationManager()->getAudioInputDeviceIndex(name);
}

int sflph_config_get_audio_output_device_index(const std::string& name)
{
    return _getConfigurationManager()->getAudioOutputDeviceIndex(name);
}

std::string sflph_config_get_current_audio_output_plugin(void)
{
    return _getConfigurationManager()->getCurrentAudioOutputPlugin();
}

bool sflph_config_get_noise_suppress_state(void)
{
    return _getConfigurationManager()->getNoiseSuppressState();
}

void sflph_config_set_noise_suppress_state(bool state)
{
    _getConfigurationManager()->setNoiseSuppressState(state);
}

bool sflph_config_is_agc_enabled(void)
{
    return _getConfigurationManager()->isAgcEnabled();
}

void sflph_config_enable_agc(bool enabled)
{
    _getConfigurationManager()->setAgcState(enabled);
}

void sflph_config_mute_dtmf(bool mute)
{
    _getConfigurationManager()->muteDtmf(mute);
}

bool sflph_config_is_dtmf_muted(void)
{
    return _getConfigurationManager()->isDtmfMuted();
}

bool sflph_config_is_capture_muted(void)
{
    return _getConfigurationManager()->isCaptureMuted();
}

void sflph_config_mute_capture(bool mute)
{
    _getConfigurationManager()->muteCapture(mute);
}

bool sflph_config_is_playback_muted(void)
{
    return _getConfigurationManager()->isPlaybackMuted();
}

void sflph_config_mute_playback(int mute)
{
    _getConfigurationManager()->mutePlayback(mute);
}

std::map<std::string, std::string> sflph_config_get_ringtone_list(void)
{
    return _getConfigurationManager()->getRingtoneList();
}

std::string sflph_config_get_audio_manager(void)
{
    return _getConfigurationManager()->getAudioManager();
}

bool sflph_config_set_audio_manager(const std::string& api)
{
    return _getConfigurationManager()->setAudioManager(api);
}

std::vector<std::string> sflph_config_get_supported_audio_managers(void)
{
    return {
#if HAVE_ALSA
        ALSA_API_STR,
#endif
#if HAVE_PULSE
        PULSEAUDIO_API_STR,
#endif
#if HAVE_JACK
        JACK_API_STR,
#endif
    };
}

int sflph_config_is_iax2_enabled(void)
{
    return _getConfigurationManager()->isIax2Enabled();
}

std::string sflph_config_get_record_path(void)
{
    return _getConfigurationManager()->getRecordPath();
}

void sflph_config_set_record_path(const std::string& path)
{
    _getConfigurationManager()->setRecordPath(path);
}

bool sflph_config_is_always_recording(void)
{
    return _getConfigurationManager()->getIsAlwaysRecording();
}

void sflph_config_set_always_recording(bool rec)
{
    _getConfigurationManager()->setIsAlwaysRecording(rec);
}

void sflph_config_set_history_limit(int days)
{
    _getConfigurationManager()->setHistoryLimit(days);
}

int sflph_config_get_history_limit(void)
{
    return _getConfigurationManager()->getHistoryLimit();
}

void sflph_config_clear_history(void)
{
    _getConfigurationManager()->clearHistory();
}

void sflph_config_set_accounts_order(const std::string& order)
{
    _getConfigurationManager()->setAccountsOrder(order);
}

std::map<std::string, std::string> sflph_config_get_hook_settings(void)
{
    return _getConfigurationManager()->getHookSettings();
}

void sflph_config_set_hook_settings(const std::map<std::string, std::string>& settings)
{
    _getConfigurationManager()->setHookSettings(settings);
}

std::vector<std::map<std::string, std::string>> sflph_config_get_history(void)
{
    return _getConfigurationManager()->getHistory();
}

std::map<std::string, std::string> sflph_config_get_tls_settings()
{
    return _getConfigurationManager()->getTlsSettings();
}

void sflph_config_set_tls_settings(const std::map< std::string, std::string >& settings)
{
    _getConfigurationManager()->setTlsSettings(settings);
}

std::map<std::string, std::string> sflph_config_get_ip2ip_details(void)
{
    return _getConfigurationManager()->getIp2IpDetails();
}

std::vector<std::map<std::string, std::string>> sflph_config_get_credentials(const std::string& account_id)
{
    return _getConfigurationManager()->getCredentials(account_id);
}

void sflph_config_set_credentials(const std::string& account_id, const std::vector<std::map<std::string, std::string>>& details)
{
    _getConfigurationManager()->setCredentials(account_id, details);
}

std::string sflph_config_get_addr_from_interface_name(const std::string& interface)
{
    return _getConfigurationManager()->getAddrFromInterfaceName(interface);
}

std::vector<std::string> sflph_config_get_all_ip_interface(void)
{
    return _getConfigurationManager()->getAllIpInterface();
}

std::vector<std::string> sflph_config_get_all_ip_interface_by_name(void)
{
    return _getConfigurationManager()->getAllIpInterfaceByName();
}

std::map<std::string, std::string> sflph_config_get_shortcuts()
{
    return _getConfigurationManager()->getShortcuts();
}

void sflph_config_set_shortcuts(const std::map<std::string, std::string>& shortcuts)
{
    _getConfigurationManager()->setShortcuts(shortcuts);
}

void sflph_config_set_volume(const std::string& device, double value)
{
    _getConfigurationManager()->setVolume(device, value);
}

double sflph_config_get_volume(const std::string& device)
{
    return _getConfigurationManager()->getVolume(device);
}

bool sflph_config_check_for_private_key(const std::string& pem_path)
{
    return _getConfigurationManager()->checkForPrivateKey(pem_path);
}

bool sflph_config_check_certificate_validity(const std::string& ca_path, const std::string& pem_path)
{
    return _getConfigurationManager()->checkCertificateValidity(ca_path, pem_path);
}

bool sflph_config_check_hostname_certificate(const std::string& host, const std::string& port)
{
    return _getConfigurationManager()->checkHostnameCertificate(host, port);
}

#ifdef SFL_PRESENCE
void sflph_pres_publish(const std::string& account_id, int status, const std::string& note)
{
    _getPresenceManager()->publish(account_id, status, note);
}

void sflph_pres_answer_server_request(const std::string& uri, int flag)
{
    _getPresenceManager()->answerServerRequest(uri, flag);
}

void sflph_pres_subscribe_buddy(const std::string& account_id, const std::string& uri, int flag)
{
    _getPresenceManager()->subscribeBuddy(account_id, uri, flag);
}

std::vector<std::map<std::string, std::string>> sflph_pres_get_subscriptions(const std::string& account_id)
{
    return _getPresenceManager()->getSubscriptions(account_id);
}

void sflph_pres_set_subscriptions(const std::string& account_id, const std::vector<std::string>& uris)
{
    _getPresenceManager()->setSubscriptions(account_id, uris);
}
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
std::vector<std::map<std::string, std::string>> sflph_video_get_codecs(const std::string& account_id)
{
    return _getVideoManager()->getCodecs(account_id);
}

void sflph_video_set_codecs(const std::string& account_id, const std::vector<std::map<std::string, std::string>>& details)
{
    _getVideoManager()->setCodecs(account_id, details);
}

std::vector<std::string> sflph_video_get_device_list(void)
{
    return _getVideoManager()->getDeviceList();
}

std::map<std::string, std::string> sflph_video_get_settings(const std::string& name)
{
    return _getVideoManager()->getSettings(name);
}

void sflph_video_set_default_device(const std::string& dev)
{
    _getVideoManager()->setDefaultDevice(dev);
}

std::string sflph_video_get_default_device(void)
{
    return _getVideoManager()->getDefaultDevice();
}

std::string sflph_video_get_current_codec_name(const std::string& call_id)
{
    return _getVideoManager()->getCurrentCodecName(call_id);
}

void sflph_video_start_camera(void)
{
    _getVideoManager()->startCamera();
}

void sflph_video_stop_camera(void)
{
    _getVideoManager()->stopCamera();
}

bool sflph_video_switch_input(const std::string& resource)
{
    return _getVideoManager()->switchInput(resource);
}

bool sflph_video_is_camera_started(void)
{
    return _getVideoManager()->hasCameraStarted();
}

void sflph_video_apply_settings(const std::string& name, const std::map<std::string, std::string>& settings)
{
	_getVideoManager()->applySettings(name, settings);
}

std::map<std::string, std::map<std::string, std::vector<std::string>>> sflph_video_get_capabilities(const std::string& name)
{
	return _getVideoManager()->getCapabilities(name);
}

#endif // SFL_VIDEO
