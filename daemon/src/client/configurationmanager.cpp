/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configurationmanager.h"
#include "account_schema.h"
#include "manager.h"
#include "logger.h"
#include "fileutils.h"
#include "ip_utils.h"
#include "sip/sipaccount.h"
#include "audio/audiolayer.h"

#include <dirent.h>

#include <cerrno>
#include <cstring>
#include <sstream>

#define CODECS_NOT_LOADED           0x1000  /** Codecs not found */

void ConfigurationManager::registerEvHandlers(struct sflph_config_ev_handlers* evHandlers)
{
    evHandlers_ = *evHandlers;
}

std::map<std::string, std::string> ConfigurationManager::getIp2IpDetails()
{
    const auto account = Manager::instance().getIP2IPAccount();
    const auto sipaccount = static_cast<SIPAccount *>(account.get());

    if (!sipaccount) {
        ERROR("Could not find IP2IP account");
        return std::map<std::string, std::string>();
    } else
        return sipaccount->getIp2IpDetails();
}


std::map<std::string, std::string> ConfigurationManager::getAccountDetails(
    const std::string& accountID)
{
    return Manager::instance().getAccountDetails(accountID);
}


std::map<std::string, std::string> ConfigurationManager::getVolatileAccountDetails(
    const std::string& accountID)
{
    return Manager::instance().getVolatileAccountDetails(accountID);
}

std::map<std::string, std::string>
ConfigurationManager::getTlsSettingsDefault()
{
    std::stringstream portstr;
    portstr << DEFAULT_SIP_TLS_PORT;

    std::map<std::string, std::string> tlsSettingsDefault;
    tlsSettingsDefault[CONFIG_TLS_LISTENER_PORT] = portstr.str();
    tlsSettingsDefault[CONFIG_TLS_CA_LIST_FILE] = "";
    tlsSettingsDefault[CONFIG_TLS_CERTIFICATE_FILE] = "";
    tlsSettingsDefault[CONFIG_TLS_PRIVATE_KEY_FILE] = "";
    tlsSettingsDefault[CONFIG_TLS_PASSWORD] = "";
    tlsSettingsDefault[CONFIG_TLS_METHOD] = "TLSv1";
    tlsSettingsDefault[CONFIG_TLS_CIPHERS] = "";
    tlsSettingsDefault[CONFIG_TLS_SERVER_NAME] = "";
    tlsSettingsDefault[CONFIG_TLS_VERIFY_SERVER] = "true";
    tlsSettingsDefault[CONFIG_TLS_VERIFY_CLIENT] = "true";
    tlsSettingsDefault[CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE] = "true";
    tlsSettingsDefault[CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC] = "2";

    return tlsSettingsDefault;
}

std::map<std::string, std::string> ConfigurationManager::getTlsSettings()
{
    std::map<std::string, std::string> tlsSettings;

    const auto account = Manager::instance().getIP2IPAccount();
    const auto sipaccount = static_cast<SIPAccount *>(account.get());

    if (!sipaccount)
        return tlsSettings;

    return sipaccount->getTlsSettings();
}

void ConfigurationManager::setTlsSettings(const std::map<std::string, std::string>& details)
{
    const auto account = Manager::instance().getIP2IPAccount();
    const auto sipaccount = static_cast<SIPAccount *>(account.get());

    if (!sipaccount) {
        DEBUG("No valid account in set TLS settings");
        return;
    }

    sipaccount->setTlsSettings(details);

    Manager::instance().saveConfig();

    // Update account details to the client side
    accountsChanged();
}


void ConfigurationManager::setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    Manager::instance().setAccountDetails(accountID, details);
}

void ConfigurationManager::sendRegister(const std::string& accountID, bool enable)
{
    Manager::instance().sendRegister(accountID, enable);
}

void ConfigurationManager::registerAllAccounts()
{
    Manager::instance().registerAccounts();
}

///This function is used as a base for new accounts for clients that support it
std::map<std::string, std::string> ConfigurationManager::getAccountTemplate()
{
    SIPAccount dummy("dummy", false);
    return dummy.getAccountDetails();
}

std::string ConfigurationManager::addAccount(const std::map<std::string, std::string>& details)
{
    return Manager::instance().addAccount(details);
}

void ConfigurationManager::removeAccount(const std::string& accoundID)
{
    return Manager::instance().removeAccount(accoundID);
}

std::vector<std::string> ConfigurationManager::getAccountList()
{
    return Manager::instance().getAccountList();
}

/**
 * Send the list of all codecs loaded to the client through DBus.
 * Can stay global, as only the active codecs will be set per accounts
 */
std::vector<int32_t> ConfigurationManager::getAudioCodecList()
{
    std::vector<int32_t> list(Manager::instance().audioCodecFactory.getCodecList());

    if (list.empty())
        errorAlert(CODECS_NOT_LOADED);

    return list;
}

std::vector<std::string> ConfigurationManager::getSupportedTlsMethod()
{
    std::vector<std::string> method;
    method.push_back("Default");
    method.push_back("TLSv1");
    method.push_back("SSLv3");
    method.push_back("SSLv23");
    return method;
}

std::vector<std::string> ConfigurationManager::getAudioCodecDetails(int32_t payload)
{
    std::vector<std::string> result(Manager::instance().audioCodecFactory.getCodecSpecifications(payload));

    if (result.empty())
        errorAlert(CODECS_NOT_LOADED);

    return result;
}

std::vector<int32_t> ConfigurationManager::getActiveAudioCodecList(const std::string& accountID)
{
    if (const auto acc = Manager::instance().getAccount(accountID))
        return acc->getActiveAudioCodecs();
    else {
        ERROR("Could not find account %s, returning default", accountID.c_str());
        return Account::getDefaultAudioCodecs();
    }
}

void ConfigurationManager::setActiveAudioCodecList(const std::vector<std::string>& list, const std::string& accountID)
{
    if (auto acc = Manager::instance().getAccount(accountID)) {
        acc->setActiveAudioCodecs(list);
        Manager::instance().saveConfig();
    } else {
        ERROR("Could not find account %s", accountID.c_str());
    }
}

std::vector<std::string> ConfigurationManager::getAudioPluginList()
{
    std::vector<std::string> v;

    v.push_back(PCM_DEFAULT);
    v.push_back(PCM_DMIX_DSNOOP);

    return v;
}

void ConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    return Manager::instance().setAudioPlugin(audioPlugin);
}

std::vector<std::string> ConfigurationManager::getAudioOutputDeviceList()
{
    return Manager::instance().getAudioOutputDeviceList();
}

std::vector<std::string> ConfigurationManager::getAudioInputDeviceList()
{
    return Manager::instance().getAudioInputDeviceList();
}

void ConfigurationManager::setAudioOutputDevice(int32_t index)
{
    return Manager::instance().setAudioDevice(index, sfl::DeviceType::PLAYBACK);
}

void ConfigurationManager::setAudioInputDevice(int32_t index)
{
    return Manager::instance().setAudioDevice(index, sfl::DeviceType::CAPTURE);
}

void ConfigurationManager::setAudioRingtoneDevice(int32_t index)
{
    return Manager::instance().setAudioDevice(index, sfl::DeviceType::RINGTONE);
}

std::vector<std::string> ConfigurationManager::getCurrentAudioDevicesIndex()
{
    return Manager::instance().getCurrentAudioDevicesIndex();
}

int32_t ConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    return Manager::instance().getAudioInputDeviceIndex(name);
}

int32_t ConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    return Manager::instance().getAudioOutputDeviceIndex(name);
}

std::string ConfigurationManager::getCurrentAudioOutputPlugin()
{
    DEBUG("Get audio plugin %s", Manager::instance().getCurrentAudioOutputPlugin().c_str());

    return Manager::instance().getCurrentAudioOutputPlugin();
}

bool ConfigurationManager::getNoiseSuppressState()
{
    return Manager::instance().getNoiseSuppressState();
}

void ConfigurationManager::setNoiseSuppressState(bool state)
{
    Manager::instance().setNoiseSuppressState(state);
}

bool ConfigurationManager::isAgcEnabled()
{
    return Manager::instance().isAGCEnabled();
}

void ConfigurationManager::setAgcState(bool enabled)
{
    Manager::instance().setAGCState(enabled);
}

std::map<std::string, std::string> ConfigurationManager::getRingtoneList()
{
    std::map<std::string, std::string> ringToneList;
    std::string r_path(fileutils::get_ringtone_dir());
    struct dirent **namelist;
    int n = scandir(r_path.c_str(), &namelist, 0, alphasort);
    if (n == -1) {
        ERROR("%s", strerror(errno));
        return ringToneList;
    }

    while (n--) {
        if (strcmp(namelist[n]->d_name, ".") and strcmp(namelist[n]->d_name, "..")) {
            std::string file(namelist[n]->d_name);

            if (file.find(".wav") != std::string::npos)
                file.replace(file.find(".wav"), 4, "");
            else
                file.replace(file.size() - 3, 3, "");
            if (file[0] <= 0x7A and file[0] >= 0x61) file[0] = file[0] - 32;
            ringToneList[r_path + namelist[n]->d_name] = file;
        }
        free(namelist[n]);
    }
    free(namelist);
    return ringToneList;
}

int32_t ConfigurationManager::isIax2Enabled()
{
    return HAVE_IAX;
}

std::string ConfigurationManager::getRecordPath()
{
    return Manager::instance().audioPreference.getRecordPath();
}

void ConfigurationManager::setRecordPath(const std::string& recPath)
{
    Manager::instance().audioPreference.setRecordPath(recPath);
}

bool ConfigurationManager::getIsAlwaysRecording()
{
    return Manager::instance().getIsAlwaysRecording();
}

void ConfigurationManager::setIsAlwaysRecording(bool rec)
{
    Manager::instance().setIsAlwaysRecording(rec);
}

int32_t ConfigurationManager::getHistoryLimit()
{
    return Manager::instance().getHistoryLimit();
}

void ConfigurationManager::clearHistory()
{
    return Manager::instance().clearHistory();
}

void ConfigurationManager::setHistoryLimit(int32_t days)
{
    Manager::instance().setHistoryLimit(days);
}

bool ConfigurationManager::setAudioManager(const std::string& api)
{
    return Manager::instance().setAudioManager(api);
}

std::string ConfigurationManager::getAudioManager()
{
    return Manager::instance().getAudioManager();
}

void ConfigurationManager::setVolume(const std::string& device, double value)
{
    auto audiolayer = Manager::instance().getAudioDriver();

    if (!audiolayer) {
        ERROR("Audio layer not valid while updating volume");
        return;
    }

    DEBUG("set volume for %s: %f", device.c_str(), value);

    if (device == "speaker") {
        audiolayer->setPlaybackGain(value);
    } else if (device == "mic") {
        audiolayer->setCaptureGain(value);
    }

    volumeChanged(device, value);
}

double
ConfigurationManager::getVolume(const std::string& device)
{
    auto audiolayer = Manager::instance().getAudioDriver();

    if (!audiolayer) {
        ERROR("Audio layer not valid while updating volume");
        return 0.0;
    }

    if (device == "speaker")
        return audiolayer->getPlaybackGain();
    else if (device == "mic")
        return audiolayer->getCaptureGain();

    return 0;
}

// FIXME: we should store "muteDtmf" instead of "playDtmf"
// in config and avoid negating like this
bool ConfigurationManager::isDtmfMuted()
{
    return not Manager::instance().voipPreferences.getPlayDtmf();
}

void ConfigurationManager::muteDtmf(bool mute)
{
    Manager::instance().voipPreferences.setPlayDtmf(not mute);
}

bool ConfigurationManager::isCaptureMuted()
{
    auto audiolayer = Manager::instance().getAudioDriver();

    if (!audiolayer) {
        ERROR("Audio layer not valid");
        return false;
    }

    return audiolayer->isCaptureMuted();
}

void ConfigurationManager::muteCapture(bool mute)
{
    auto audiolayer = Manager::instance().getAudioDriver();

    if (!audiolayer) {
        ERROR("Audio layer not valid");
        return;
    }

    return audiolayer->muteCapture(mute);
}

bool ConfigurationManager::isPlaybackMuted()
{
    auto audiolayer = Manager::instance().getAudioDriver();

    if (!audiolayer) {
        ERROR("Audio layer not valid");
        return false;
    }

    return audiolayer->isPlaybackMuted();
}

void ConfigurationManager::mutePlayback(bool mute)
{
    auto audiolayer = Manager::instance().getAudioDriver();

    if (!audiolayer) {
        ERROR("Audio layer not valid");
        return;
    }

    return audiolayer->mutePlayback(mute);
}

std::map<std::string, std::string> ConfigurationManager::getHookSettings()
{
    return Manager::instance().hookPreference.toMap();
}

void ConfigurationManager::setHookSettings(const std::map<std::string,
        std::string>& settings)
{
    Manager::instance().hookPreference = HookPreference(settings);
}

void ConfigurationManager::setAccountsOrder(const std::string& order)
{
    Manager::instance().setAccountsOrder(order);
}

std::vector<std::map<std::string, std::string> > ConfigurationManager::getHistory()
{
    return Manager::instance().getHistory();
}

std::string
ConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    return ip_utils::getInterfaceAddr(interface);
}

std::vector<std::string> ConfigurationManager::getAllIpInterface()
{
    return ip_utils::getAllIpInterface();
}

std::vector<std::string> ConfigurationManager::getAllIpInterfaceByName()
{
    return ip_utils::getAllIpInterfaceByName();
}

std::map<std::string, std::string> ConfigurationManager::getShortcuts()
{
    return Manager::instance().shortcutPreferences.getShortcuts();
}

void ConfigurationManager::setShortcuts(
    const std::map<std::string, std::string>& shortcutsMap)
{
    Manager::instance().shortcutPreferences.setShortcuts(shortcutsMap);
    Manager::instance().saveConfig();
}

std::vector<std::map<std::string, std::string> > ConfigurationManager::getCredentials(
    const std::string& accountID)
{
    const auto sipaccount = Manager::instance().getAccount<SIPAccount>(accountID);

    std::vector<std::map<std::string, std::string> > credentialInformation;

    if (!sipaccount)
        return credentialInformation;
    else
        return sipaccount->getCredentials();
}

void ConfigurationManager::setCredentials(const std::string& accountID,
        const std::vector<std::map<std::string, std::string> >& details)
{
    const auto sipaccount = Manager::instance().getAccount<SIPAccount>(accountID);

    if (sipaccount)
        sipaccount->setCredentials(details);
}

void ConfigurationManager::volumeChanged(const std::string& device, int value)
{
    if (evHandlers_.on_volume_change) {
        evHandlers_.on_volume_change(device, value);
    }
}

void ConfigurationManager::accountsChanged()
{
    if (evHandlers_.on_accounts_change) {
        evHandlers_.on_accounts_change();
    }
}

void ConfigurationManager::historyChanged()
{
    if (evHandlers_.on_history_change) {
        evHandlers_.on_history_change();
    }
}

void ConfigurationManager::stunStatusFailure(const std::string& accountID)
{
    if (evHandlers_.on_stun_status_fail) {
        evHandlers_.on_stun_status_fail(accountID);
    }
}

void ConfigurationManager::registrationStateChanged(const std::string& accountID, int state)
{
    if (evHandlers_.on_registration_state_change) {
        evHandlers_.on_registration_state_change(accountID, state);
    }
}

void ConfigurationManager::sipRegistrationStateChanged(const std::string& accountID, const std::string& state, int32_t code)
{
    if (evHandlers_.on_sip_registration_state_change) {
        evHandlers_.on_sip_registration_state_change(accountID, state, code);
    }
}


void ConfigurationManager::volatileAccountDetailsChanged(const std::string& accountID, const std::map<std::string, std::string> &details)
{
    if (evHandlers_.on_volatile_details_change) {
        evHandlers_.on_volatile_details_change(accountID, details);
    }
}

void ConfigurationManager::errorAlert(int alert)
{
    if (evHandlers_.on_error) {
        evHandlers_.on_error(alert);
    }
}

std::vector< int32_t > ConfigurationManager::getHardwareAudioFormat()
{
    return std::vector<int32_t> {44100, 64};
}
