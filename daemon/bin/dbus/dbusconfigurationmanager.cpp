/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include <iostream>
#include "sflphone.h"

#include "dbusconfigurationmanager.h"

DBusConfigurationManager::DBusConfigurationManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/org/sflphone/SFLphone/ConfigurationManager")
{
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountDetails(const std::string& accountID)
{
    return sflph_config_get_account_details(accountID);
}

std::map<std::string, std::string> DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
{
    return sflph_config_get_volatile_account_details(accountID);
}

void DBusConfigurationManager::setAccountDetails(const std::string& accountID, const std::map< std::string, std::string >& details)
{
    sflph_config_set_account_details(accountID, details);
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountTemplate()
{
    return sflph_config_get_account_template();
}

std::string DBusConfigurationManager::addAccount(const std::map< std::string, std::string >& details)
{
    return sflph_config_add_account(details);
}

void DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    sflph_config_remove_account(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAccountList()
{
    return sflph_config_get_account_list();
}

void DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    sflph_config_send_register(accountID, enable);
}

void DBusConfigurationManager::registerAllAccounts(void)
{
    sflph_config_register_all_accounts();
}

std::map< std::string, std::string > DBusConfigurationManager::getTlsSettingsDefault()
{
    return sflph_config_get_tls_default_settings();
}

std::vector< int32_t > DBusConfigurationManager::getAudioCodecList()
{
    return sflph_config_get_audio_codec_list();
}

std::vector< std::string > DBusConfigurationManager::getSupportedTlsMethod()
{
    return sflph_config_get_supported_tls_method();
}

std::vector< std::string > DBusConfigurationManager::getAudioCodecDetails(const int32_t& payload)
{
    return sflph_config_get_audio_codec_details(payload);
}

std::vector< int32_t > DBusConfigurationManager::getActiveAudioCodecList(const std::string& accountID)
{
    return sflph_config_get_active_audio_codec_list(accountID);
}

void DBusConfigurationManager::setActiveAudioCodecList(const std::vector< std::string >& list, const std::string& accountID)
{
    sflph_config_set_active_audio_codec_list(list, accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioPluginList()
{
    return sflph_config_get_audio_plugin_list();
}

void DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    sflph_config_set_audio_plugin(audioPlugin);
}

std::vector< std::string > DBusConfigurationManager::getAudioOutputDeviceList()
{
    return sflph_config_get_audio_output_device_list();
}

void DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    sflph_config_set_audio_output_device(index);
}

void DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    sflph_config_set_audio_input_device(index);
}

void DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    sflph_config_set_audio_ringtone_device(index);
}

std::vector< std::string > DBusConfigurationManager::getAudioInputDeviceList()
{
    return sflph_config_get_audio_input_device_list();
}

std::vector< std::string > DBusConfigurationManager::getCurrentAudioDevicesIndex()
{
    return sflph_config_get_current_audio_devices_index();
}

int32_t DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    return sflph_config_get_audio_input_device_index(name);
}

int32_t DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    return sflph_config_get_audio_output_device_index(name);
}

std::string DBusConfigurationManager::getCurrentAudioOutputPlugin()
{
    return sflph_config_get_current_audio_output_plugin();
}

bool DBusConfigurationManager::getNoiseSuppressState()
{
    return sflph_config_get_noise_suppress_state();
}

void DBusConfigurationManager::setNoiseSuppressState(const bool& state)
{
    sflph_config_set_noise_suppress_state(state);
}

bool DBusConfigurationManager::isAgcEnabled()
{
    return sflph_config_is_agc_enabled();
}

void DBusConfigurationManager::setAgcState(const bool& enabled)
{
    sflph_config_enable_agc(enabled);
}

void DBusConfigurationManager::muteDtmf(const bool& mute)
{
    sflph_config_mute_dtmf(mute);
}

bool DBusConfigurationManager::isDtmfMuted()
{
    return sflph_config_is_dtmf_muted();
}

bool DBusConfigurationManager::isCaptureMuted()
{
    return sflph_config_is_capture_muted();
}

void DBusConfigurationManager::muteCapture(const bool& mute)
{
    sflph_config_mute_capture(mute);
}

bool DBusConfigurationManager::isPlaybackMuted()
{
    return sflph_config_is_playback_muted();
}

void DBusConfigurationManager::mutePlayback(const bool& mute)
{
    sflph_config_mute_playback(mute);
}

std::map<std::string, std::string> DBusConfigurationManager::getRingtoneList()
{
    return sflph_config_get_ringtone_list();
}

std::string DBusConfigurationManager::getAudioManager()
{
    return sflph_config_get_audio_manager();
}

bool DBusConfigurationManager::setAudioManager(const std::string& api)
{
    return sflph_config_set_audio_manager(api);
}

std::vector<std::string> DBusConfigurationManager::getSupportedAudioManagers()
{
    return sflph_config_get_supported_audio_managers();
}

int32_t DBusConfigurationManager::isIax2Enabled()
{
    return sflph_config_is_iax2_enabled();
}

std::string DBusConfigurationManager::getRecordPath()
{
    return sflph_config_get_record_path();
}

void DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    sflph_config_set_record_path(recPath);
}

bool DBusConfigurationManager::getIsAlwaysRecording()
{
    return sflph_config_is_always_recording();
}

void DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    sflph_config_set_always_recording(rec);
}

void DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    sflph_config_set_history_limit(days);
}

int32_t DBusConfigurationManager::getHistoryLimit()
{
    return sflph_config_get_history_limit();
}

void DBusConfigurationManager::clearHistory()
{
    sflph_config_clear_history();
}

void DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    sflph_config_set_accounts_order(order);
}

std::map<std::string, std::string> DBusConfigurationManager::getHookSettings()
{
    return sflph_config_get_hook_settings();
}

void DBusConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    sflph_config_set_hook_settings(settings);
}

std::vector<std::map<std::string, std::string> > DBusConfigurationManager::getHistory()
{
    return sflph_config_get_history();
}

std::map<std::string, std::string> DBusConfigurationManager::getTlsSettings()
{
    return sflph_config_get_tls_settings();
}

void DBusConfigurationManager::setTlsSettings(const std::map< std::string, std::string >& details)
{
    sflph_config_set_tls_settings(details);
}

std::map< std::string, std::string > DBusConfigurationManager::getIp2IpDetails()
{
    return sflph_config_get_ip2ip_details();
}

std::vector< std::map< std::string, std::string > > DBusConfigurationManager::getCredentials(const std::string& accountID)
{
    return sflph_config_get_credentials(accountID);
}

void DBusConfigurationManager::setCredentials(const std::string& accountID, const std::vector< std::map< std::string, std::string > >& details)
{
    sflph_config_set_credentials(accountID, details);
}

std::string DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    return sflph_config_get_addr_from_interface_name(interface);
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterface()
{
    return sflph_config_get_all_ip_interface();
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterfaceByName()
{
    return sflph_config_get_all_ip_interface_by_name();
}

std::map<std::string, std::string> DBusConfigurationManager::getShortcuts()
{
    return sflph_config_get_shortcuts();
}

void DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    sflph_config_set_shortcuts(shortcutsMap);
}

void DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    sflph_config_set_volume(device, value);
}

double DBusConfigurationManager::getVolume(const std::string& device)
{
    return sflph_config_get_volume(device);
}

bool DBusConfigurationManager::checkForPrivateKey(const std::string& pemPath)
{
    return sflph_config_check_for_private_key(pemPath);
}

bool DBusConfigurationManager::checkCertificateValidity(const std::string& caPath, const std::string& pemPath)
{
    return sflph_config_check_certificate_validity(caPath, pemPath);
}

bool DBusConfigurationManager::checkHostnameCertificate(const  std::string& host, const std::string& port)
{
    return sflph_config_check_hostname_certificate(host, port);
}
