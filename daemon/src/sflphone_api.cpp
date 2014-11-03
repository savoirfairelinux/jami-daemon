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

static CallManager* getCallManager()
{
    return Manager::instance().getClient()->getCallManager();
}

static ConfigurationManager* getConfigurationManager()
{
    return Manager::instance().getClient()->getConfigurationManager();
}

#ifdef SFL_PRESENCE
static PresenceManager* getPresenceManager()
{
    return Manager::instance().getClient()->getPresenceManager();
}
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
static VideoManager* getVideoManager()
{
    return Manager::instance().getClient()->getVideoManager();
}
#endif // SFL_VIDEO

const char *
sflph_version()
{
    return PACKAGE_VERSION;
}

int sflph_init(sflph_ev_handlers* ev_handlers, enum sflph_init_flag flags)
{
    // User handlers of library events
    // FIXME: static evil
    static sflph_ev_handlers evHandlers_;

    // Copy user event handlers
    evHandlers_ = *ev_handlers;

    // Handle flags
    setDebugMode(flags & SFLPH_FLAG_DEBUG);
    setConsoleLog(flags & SFLPH_FLAG_CONSOLE_LOG);

    // Create manager
    try {
        // FIXME: static evil
        static ManagerImpl *manager;
        // ensure that we haven't been in this function before
        assert(!manager);
        manager = &(Manager::instance());
    } catch (...) {
        return -SFLPH_ERR_MANAGER_INIT;
    }

    // Register user event handlers
    getCallManager()->registerEvHandlers(&evHandlers_.call_ev_handlers);
    getConfigurationManager()->registerEvHandlers(&evHandlers_.config_ev_handlers);

#ifdef SFL_PRESENCE
    getPresenceManager()->registerEvHandlers(&evHandlers_.pres_ev_handlers);
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    getVideoManager()->registerEvHandlers(&evHandlers_.video_ev_handlers);
#endif // SFL_VIDEO

    // Initialize manager now
    try {
        Manager::instance().init("");
    } catch (...) {
        return -SFLPH_ERR_MANAGER_INIT;
    }

    return 0;
}

void sflph_fini(void)
{
    // Finish manager
    Manager::instance().finish();
}

void sflph_poll_events()
{
    Manager::instance().pollEvents();
}

bool sflph_call_place(const std::string& account_id, const std::string& call_id, const std::string& to)
{
    return getCallManager()->placeCall(account_id, call_id, to);
}

bool sflph_call_refuse(const std::string& call_id)
{
    return getCallManager()->refuse(call_id);
}

bool sflph_call_accept(const std::string& call_id)
{
    return getCallManager()->accept(call_id);
}

bool sflph_call_hang_up(const std::string& call_id)
{
    return getCallManager()->hangUp(call_id);
}

bool sflph_call_hold(const std::string& call_id)
{
    return getCallManager()->hold(call_id);
}

bool sflph_call_unhold(const std::string& call_id)
{
    return getCallManager()->unhold(call_id);
}

bool sflph_call_transfer(const std::string& call_id, const std::string& to)
{
    return getCallManager()->transfer(call_id, to);
}

bool sflph_call_attended_transfer(const std::string& transfer_id, const std::string& target_id)
{
    return getCallManager()->attendedTransfer(transfer_id, target_id);
}

std::map<std::string, std::string> sflph_call_get_call_details(const std::string& call_id)
{
    return getCallManager()->getCallDetails(call_id);
}

std::vector<std::string> sflph_call_get_call_list(void)
{
    return getCallManager()->getCallList();
}

void sflph_call_remove_conference(const std::string& conf_id)
{
    getCallManager()->removeConference(conf_id);
}

bool sflph_call_join_participant(const std::string& sel_call_id, const std::string& drag_call_id)
{
    return getCallManager()->joinParticipant(sel_call_id, drag_call_id);
}

void sflph_call_create_conf_from_participant_list(const std::vector<std::string>& participants)
{
    getCallManager()->createConfFromParticipantList(participants);
}

bool sflph_call_is_conference_participant(const std::string& call_id)
{
    return getCallManager()->isConferenceParticipant(call_id);
}

bool sflph_call_add_participant(const std::string& call_id, const std::string& conf_id)
{
    return getCallManager()->addParticipant(call_id, conf_id);
}

bool sflph_call_add_main_participant(const std::string& conf_id)
{
    return getCallManager()->addMainParticipant(conf_id);
}

bool sflph_call_detach_participant(const std::string& call_id)
{
    return getCallManager()->detachParticipant(call_id);
}

bool sflph_call_join_conference(const std::string& sel_conf_id, const std::string& drag_conf_id)
{
    return getCallManager()->joinConference(sel_conf_id, drag_conf_id);
}

bool sflph_call_hang_up_conference(const std::string& conf_id)
{
    return getCallManager()->hangUpConference(conf_id);
}

bool sflph_call_hold_conference(const std::string& conf_id)
{
    return getCallManager()->holdConference(conf_id);
}

bool sflph_call_unhold_conference(const std::string& conf_id)
{
    return getCallManager()->unholdConference(conf_id);
}

std::vector<std::string> sflph_call_get_conference_list(void)
{
    return getCallManager()->getConferenceList();
}

std::vector<std::string> sflph_call_get_participant_list(const std::string& conf_id)
{
    return getCallManager()->getParticipantList(conf_id);
}

std::vector<std::string> sflph_call_get_display_names(const std::string& conf_id)
{
    return getCallManager()->getDisplayNames(conf_id);
}

std::string sflph_call_get_conference_id(const std::string& call_id)
{
    return getCallManager()->getConferenceId(call_id);
}

std::map<std::string, std::string> sflph_call_get_conference_details(const std::string& call_id)
{
    return getCallManager()->getConferenceDetails(call_id);
}

bool sflph_call_play_recorded_file(const std::string& path)
{
    return getCallManager()->startRecordedFilePlayback(path);
}

void sflph_call_stop_recorded_file(const std::string& path)
{
    getCallManager()->stopRecordedFilePlayback(path);
}

bool sflph_call_toggle_recording(const std::string& call_id)
{
    return getCallManager()->toggleRecording(call_id);
}

void sflph_call_set_recording(const std::string& call_id)
{
    getCallManager()->setRecording(call_id);
}

void sflph_call_record_playback_seek(double pos)
{
    getCallManager()->recordPlaybackSeek(pos);
}

bool sflph_call_is_recording(const std::string& call_id)
{
    return getCallManager()->getIsRecording(call_id);
}

std::string sflph_call_get_current_audio_codec_name(const std::string& call_id)
{
    return getCallManager()->getCurrentAudioCodecName(call_id);
}

void sflph_call_play_dtmf(const std::string& key)
{
    getCallManager()->playDTMF(key);
}

void sflph_call_start_tone(int start, int type)
{
    getCallManager()->startTone(start, type);
}

void sflph_call_set_sas_verified(const std::string& call_id)
{
    getCallManager()->setSASVerified(call_id);
}

void sflph_call_reset_sas_verified(const std::string& call_id)
{
    getCallManager()->resetSASVerified(call_id);
}

void sflph_call_set_confirm_go_clear(const std::string& call_id)
{
    getCallManager()->setConfirmGoClear(call_id);
}

void sflph_call_request_go_clear(const std::string& call_id)
{
    getCallManager()->requestGoClear(call_id);
}

void sflph_call_accept_enrollment(const std::string& call_id, bool accepted)
{
    getCallManager()->acceptEnrollment(call_id, accepted);
}

void sflph_call_send_text_message(const std::string& call_id, const std::string& message)
{
    getCallManager()->sendTextMessage(call_id, message);
}

std::map<std::string, std::string> sflph_config_get_account_details(const std::string& account_id)
{
    return getConfigurationManager()->getAccountDetails(account_id);
}

std::map<std::string, std::string> sflph_config_get_volatile_account_details(const std::string& account_id)
{
    return getConfigurationManager()->getVolatileAccountDetails(account_id);
}

void sflph_config_set_account_details(const std::string& account_id, const std::map<std::string, std::string>& details)
{
    getConfigurationManager()->setAccountDetails(account_id, details);
}

std::map<std::string, std::string> sflph_config_get_account_template(void)
{
    return getConfigurationManager()->getAccountTemplate();
}

std::string sflph_config_add_account(const std::map<std::string, std::string>& details)
{
    return getConfigurationManager()->addAccount(details);
}

void sflph_config_remove_account(const std::string& account_id)
{
    getConfigurationManager()->removeAccount(account_id);
}

std::vector<std::string> sflph_config_get_account_list(void)
{
    return getConfigurationManager()->getAccountList();
}

void sflph_config_send_register(const std::string& account_id, bool enable)
{
    getConfigurationManager()->sendRegister(account_id, enable);
}

void sflph_config_register_all_accounts(void)
{
    getConfigurationManager()->registerAllAccounts();
}

std::map<std::string, std::string> sflph_config_get_tls_default_settings(void)
{
    return getConfigurationManager()->getTlsSettingsDefault();
}

std::vector<int> sflph_config_get_audio_codec_list(void)
{
    return getConfigurationManager()->getAudioCodecList();
}

std::vector<std::string> sflph_config_get_supported_tls_method(void)
{
    return getConfigurationManager()->getSupportedTlsMethod();
}

std::vector<std::string> sflph_config_get_audio_codec_details(int payload)
{
    return getConfigurationManager()->getAudioCodecDetails(payload);
}

std::vector<int> sflph_config_get_active_audio_codec_list(const std::string& account_id)
{
    return getConfigurationManager()->getActiveAudioCodecList(account_id);
}

void sflph_config_set_active_audio_codec_list(const std::vector<std::string>& list, const std::string& account_id)
{
    getConfigurationManager()->setActiveAudioCodecList(list, account_id);
}

std::vector<std::string> sflph_config_get_audio_plugin_list(void)
{
    return getConfigurationManager()->getAudioPluginList();
}

void sflph_config_set_audio_plugin(const std::string& audio_plugin)
{
    getConfigurationManager()->setAudioPlugin(audio_plugin);
}

std::vector<std::string> sflph_config_get_audio_output_device_list()
{
    return getConfigurationManager()->getAudioOutputDeviceList();
}

void sflph_config_set_audio_output_device(int index)
{
    getConfigurationManager()->setAudioOutputDevice(index);
}

void sflph_config_set_audio_input_device(int index)
{
    getConfigurationManager()->setAudioInputDevice(index);
}

void sflph_config_set_audio_ringtone_device(int index)
{
    getConfigurationManager()->setAudioRingtoneDevice(index);
}

std::vector<std::string> sflph_config_get_audio_input_device_list(void)
{
    return getConfigurationManager()->getAudioInputDeviceList();
}

std::vector<std::string> sflph_config_get_current_audio_devices_index(void)
{
    return getConfigurationManager()->getCurrentAudioDevicesIndex();
}

int sflph_config_get_audio_input_device_index(const std::string& name)
{
    return getConfigurationManager()->getAudioInputDeviceIndex(name);
}

int sflph_config_get_audio_output_device_index(const std::string& name)
{
    return getConfigurationManager()->getAudioOutputDeviceIndex(name);
}

std::string sflph_config_get_current_audio_output_plugin(void)
{
    return getConfigurationManager()->getCurrentAudioOutputPlugin();
}

bool sflph_config_get_noise_suppress_state(void)
{
    return getConfigurationManager()->getNoiseSuppressState();
}

void sflph_config_set_noise_suppress_state(bool state)
{
    getConfigurationManager()->setNoiseSuppressState(state);
}

bool sflph_config_is_agc_enabled(void)
{
    return getConfigurationManager()->isAgcEnabled();
}

void sflph_config_enable_agc(bool enabled)
{
    getConfigurationManager()->setAgcState(enabled);
}

void sflph_config_mute_dtmf(bool mute)
{
    getConfigurationManager()->muteDtmf(mute);
}

bool sflph_config_is_dtmf_muted(void)
{
    return getConfigurationManager()->isDtmfMuted();
}

bool sflph_config_is_capture_muted(void)
{
    return getConfigurationManager()->isCaptureMuted();
}

void sflph_config_mute_capture(bool mute)
{
    getConfigurationManager()->muteCapture(mute);
}

bool sflph_config_is_playback_muted(void)
{
    return getConfigurationManager()->isPlaybackMuted();
}

void sflph_config_mute_playback(int mute)
{
    getConfigurationManager()->mutePlayback(mute);
}

std::map<std::string, std::string> sflph_config_get_ringtone_list(void)
{
    return getConfigurationManager()->getRingtoneList();
}

std::string sflph_config_get_audio_manager(void)
{
    return getConfigurationManager()->getAudioManager();
}

bool sflph_config_set_audio_manager(const std::string& api)
{
    return getConfigurationManager()->setAudioManager(api);
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
    return getConfigurationManager()->isIax2Enabled();
}

std::string sflph_config_get_record_path(void)
{
    return getConfigurationManager()->getRecordPath();
}

void sflph_config_set_record_path(const std::string& path)
{
    getConfigurationManager()->setRecordPath(path);
}

bool sflph_config_is_always_recording(void)
{
    return getConfigurationManager()->getIsAlwaysRecording();
}

void sflph_config_set_always_recording(bool rec)
{
    getConfigurationManager()->setIsAlwaysRecording(rec);
}

void sflph_config_set_history_limit(int days)
{
    getConfigurationManager()->setHistoryLimit(days);
}

int sflph_config_get_history_limit(void)
{
    return getConfigurationManager()->getHistoryLimit();
}

void sflph_config_clear_history(void)
{
    getConfigurationManager()->clearHistory();
}

void sflph_config_set_accounts_order(const std::string& order)
{
    getConfigurationManager()->setAccountsOrder(order);
}

std::map<std::string, std::string> sflph_config_get_hook_settings(void)
{
    return getConfigurationManager()->getHookSettings();
}

void sflph_config_set_hook_settings(const std::map<std::string, std::string>& settings)
{
    getConfigurationManager()->setHookSettings(settings);
}

std::vector<std::map<std::string, std::string>> sflph_config_get_history(void)
{
    return getConfigurationManager()->getHistory();
}

std::map<std::string, std::string> sflph_config_get_tls_settings()
{
    return getConfigurationManager()->getTlsSettings();
}

void sflph_config_set_tls_settings(const std::map< std::string, std::string >& settings)
{
    getConfigurationManager()->setTlsSettings(settings);
}

std::map<std::string, std::string> sflph_config_get_ip2ip_details(void)
{
    return getConfigurationManager()->getIp2IpDetails();
}

std::vector<std::map<std::string, std::string>> sflph_config_get_credentials(const std::string& account_id)
{
    return getConfigurationManager()->getCredentials(account_id);
}

void sflph_config_set_credentials(const std::string& account_id, const std::vector<std::map<std::string, std::string>>& details)
{
    getConfigurationManager()->setCredentials(account_id, details);
}

std::string sflph_config_get_addr_from_interface_name(const std::string& interface)
{
    return getConfigurationManager()->getAddrFromInterfaceName(interface);
}

std::vector<std::string> sflph_config_get_all_ip_interface(void)
{
    return getConfigurationManager()->getAllIpInterface();
}

std::vector<std::string> sflph_config_get_all_ip_interface_by_name(void)
{
    return getConfigurationManager()->getAllIpInterfaceByName();
}

std::map<std::string, std::string> sflph_config_get_shortcuts()
{
    return getConfigurationManager()->getShortcuts();
}

void sflph_config_set_shortcuts(const std::map<std::string, std::string>& shortcuts)
{
    getConfigurationManager()->setShortcuts(shortcuts);
}

void sflph_config_set_volume(const std::string& device, double value)
{
    getConfigurationManager()->setVolume(device, value);
}

double sflph_config_get_volume(const std::string& device)
{
    return getConfigurationManager()->getVolume(device);
}

bool sflph_config_check_for_private_key(const std::string& pem_path)
{
    return getConfigurationManager()->checkForPrivateKey(pem_path);
}

bool sflph_config_check_certificate_validity(const std::string& ca_path, const std::string& pem_path)
{
    return getConfigurationManager()->checkCertificateValidity(ca_path, pem_path);
}

bool sflph_config_check_hostname_certificate(const std::string& host, const std::string& port)
{
    return getConfigurationManager()->checkHostnameCertificate(host, port);
}

std::map<std::string, std::string> sflph_config_validate_certificate(const std::string& accountId, const std::string& certificate, const std::string& private_key)
{
    return getConfigurationManager()->validateCertificate(accountId,certificate,private_key);
}

std::map<std::string, std::string> sflph_config_get_certificate_details(const std::string& certificate, const std::string& privateKey);
{
    return getConfigurationManager()->getCertificateDetails(certificate, privateKey);
}

std::map<std::string, std::string> sflph_config_get_server_certificate_details(const std::string& accountId)
{
    return getConfigurationManager()->getServerCertificateDetails(accountId);
}

#ifdef SFL_PRESENCE
void sflph_pres_publish(const std::string& account_id, int status, const std::string& note)
{
    getPresenceManager()->publish(account_id, status, note);
}

void sflph_pres_answer_server_request(const std::string& uri, int flag)
{
    getPresenceManager()->answerServerRequest(uri, flag);
}

void sflph_pres_subscribe_buddy(const std::string& account_id, const std::string& uri, int flag)
{
    getPresenceManager()->subscribeBuddy(account_id, uri, flag);
}

std::vector<std::map<std::string, std::string>> sflph_pres_get_subscriptions(const std::string& account_id)
{
    return getPresenceManager()->getSubscriptions(account_id);
}

void sflph_pres_set_subscriptions(const std::string& account_id, const std::vector<std::string>& uris)
{
    getPresenceManager()->setSubscriptions(account_id, uris);
}
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
std::vector<std::map<std::string, std::string>> sflph_video_get_codecs(const std::string& account_id)
{
    return getVideoManager()->getCodecs(account_id);
}

void sflph_video_set_codecs(const std::string& account_id, const std::vector<std::map<std::string, std::string>>& details)
{
    getVideoManager()->setCodecs(account_id, details);
}

std::vector<std::string> sflph_video_get_device_list(void)
{
    return getVideoManager()->getDeviceList();
}

std::map<std::string, std::string> sflph_video_get_settings(const std::string& name)
{
    return getVideoManager()->getSettings(name);
}

void sflph_video_set_default_device(const std::string& dev)
{
    getVideoManager()->setDefaultDevice(dev);
}

std::string sflph_video_get_default_device(void)
{
    return getVideoManager()->getDefaultDevice();
}

std::string sflph_video_get_current_codec_name(const std::string& call_id)
{
    return getVideoManager()->getCurrentCodecName(call_id);
}

void sflph_video_start_camera(void)
{
    getVideoManager()->startCamera();
}

void sflph_video_stop_camera(void)
{
    getVideoManager()->stopCamera();
}

bool sflph_video_switch_input(const std::string& resource)
{
    return getVideoManager()->switchInput(resource);
}

bool sflph_video_is_camera_started(void)
{
    return getVideoManager()->hasCameraStarted();
}

void sflph_video_apply_settings(const std::string& name, const std::map<std::string, std::string>& settings)
{
    getVideoManager()->applySettings(name, settings);
}

std::map<std::string, std::map<std::string, std::vector<std::string>>> sflph_video_get_capabilities(const std::string& name)
{
    return getVideoManager()->getCapabilities(name);
}

#endif // SFL_VIDEO
