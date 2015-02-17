/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configurationmanager_interface.h"
#include "account_schema.h"
#include "manager.h"
#if HAVE_TLS && HAVE_DHT
#include "sip/tlsvalidator.h"
#endif
#include "logger.h"
#include "fileutils.h"
#include "ip_utils.h"
#include "sip/sipaccount.h"
#include "security_const.h"
#include "audio/audiolayer.h"
#include "client/signal.h"

#include <dirent.h>

#include <cerrno>
#include <cstring>
#include <sstream>

namespace DRing {

constexpr unsigned CODECS_NOT_LOADED = 0x1000; /** Codecs not found */

using ring::SIPAccount;
using ring::TlsValidator;
using ring::Account;
using ring::DeviceType;
using ring::HookPreference;

void
registerConfHandlers(const std::map<std::string,
                     std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}

std::map<std::string, std::string>
getIp2IpDetails()
{
    auto account = ring::Manager::instance().getIP2IPAccount();
    if (auto sipaccount = static_cast<SIPAccount*>(account.get()))
        return sipaccount->getIp2IpDetails();
    RING_ERR("Could not find IP2IP account");
    return std::map<std::string, std::string>();
}


std::map<std::string, std::string>
getAccountDetails(const std::string& accountID)
{
    return ring::Manager::instance().getAccountDetails(accountID);
}


std::map<std::string, std::string>
getVolatileAccountDetails(const std::string& accountID)
{
    return ring::Manager::instance().getVolatileAccountDetails(accountID);
}

std::map<std::string, std::string>
getTlsDefaultSettings()
{
    std::stringstream portstr;
    portstr << DEFAULT_SIP_TLS_PORT;

    return {
        {ring::Conf::CONFIG_TLS_LISTENER_PORT, portstr.str()},
        {ring::Conf::CONFIG_TLS_CA_LIST_FILE, ""},
        {ring::Conf::CONFIG_TLS_CERTIFICATE_FILE, ""},
        {ring::Conf::CONFIG_TLS_PRIVATE_KEY_FILE, ""},
        {ring::Conf::CONFIG_TLS_PASSWORD, ""},
        {ring::Conf::CONFIG_TLS_METHOD, "TLSv1"},
        {ring::Conf::CONFIG_TLS_CIPHERS, ""},
        {ring::Conf::CONFIG_TLS_SERVER_NAME, ""},
        {ring::Conf::CONFIG_TLS_VERIFY_SERVER, "true"},
        {ring::Conf::CONFIG_TLS_VERIFY_CLIENT, "true"},
        {ring::Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, "true"},
        {ring::Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, "2"}
    };
}

std::map<std::string, std::string>
getTlsSettings()
{
    auto account = ring::Manager::instance().getIP2IPAccount();
    if (auto sipaccount = static_cast<SIPAccount*>(account.get()))
        return sipaccount->getTlsSettings();
    RING_ERR("Could not find IP2IP account");
    return std::map<std::string, std::string>();
}

void
setTlsSettings(const std::map<std::string, std::string>& details)
{
    auto account = ring::Manager::instance().getIP2IPAccount();
    if (auto sipaccount = static_cast<SIPAccount*>(account.get())) {
        sipaccount->setTlsSettings(details);
        ring::Manager::instance().saveConfig();
        ring::emitSignal<ConfigurationSignal::AccountsChanged>();
    }

    RING_DBG("No valid account in set TLS settings");
    return;
}

std::map<std::string, std::string>
validateCertificate(const std::string&,
                    const std::string& certificate,
                    const std::string& privateKey)
{
#if HAVE_TLS && HAVE_DHT
    try {
        return TlsValidator{certificate, privateKey}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed");
        return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
    }
#else
    RING_WARN("TLS not supported");
    return {};
#endif
}

std::map<std::string, std::string>
getCertificateDetails(const std::string& certificate)
{
#if HAVE_TLS && HAVE_DHT
    try {
        return TlsValidator{certificate,""}.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed");
    }
#else
    RING_WARN("TLS not supported");
#endif
    return std::map<std::string, std::string>();
}

void
setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    ring::Manager::instance().setAccountDetails(accountID, details);
}

void
sendRegister(const std::string& accountID, bool enable)
{
    ring::Manager::instance().sendRegister(accountID, enable);
}

void
registerAllAccounts()
{
    ring::Manager::instance().registerAccounts();
}

///This function is used as a base for new accounts for clients that support it
std::map<std::string, std::string>
getAccountTemplate()
{
    return SIPAccount{"dummy", false}.getAccountDetails();
}

std::string
addAccount(const std::map<std::string, std::string>& details)
{
    return ring::Manager::instance().addAccount(details);
}

void
removeAccount(const std::string& accountID)
{
    return ring::Manager::instance().removeAccount(accountID);
}

std::vector<std::string>
getAccountList()
{
    return ring::Manager::instance().getAccountList();
}

/**
 * Send the list of all codecs loaded to the client through DBus.
 * Can stay global, as only the active codecs will be set per accounts
 */
std::vector<int32_t>
getAudioCodecList()
{
    std::vector<int32_t> list {ring::Manager::instance().audioCodecFactory.getCodecList()};
    if (list.empty())
        ring::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
    return list;
}

std::vector<std::string>
getSupportedTlsMethod()
{
    return {"Default", "TLSv1", "SSLv3", "SSLv23"};
}

std::vector<std::string>
getSupportedCiphers(const std::string& accountID)
{
#if HAVE_TLS
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID))
        return sipaccount->getSupportedCiphers();
    RING_ERR("SIP account %s doesn't exist", accountID.c_str());
#endif
    return {};
}

std::vector<std::string>
getAudioCodecDetails(int32_t payload)
{
    std::vector<std::string> result {ring::Manager::instance().audioCodecFactory.getCodecSpecifications(payload)};
    if (result.empty())
        ring::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
    return result;
}

std::vector<int32_t>
getActiveAudioCodecList(const std::string& accountID)
{
    if (auto acc = ring::Manager::instance().getAccount(accountID))
        return acc->getActiveAudioCodecs();
    RING_ERR("Could not find account %s, returning default", accountID.c_str());
    return Account::getDefaultAudioCodecs();
}

void
setActiveAudioCodecList(const std::vector<std::string>& list,
                        const std::string& accountID)
{
    if (auto acc = ring::Manager::instance().getAccount(accountID)) {
        acc->setActiveAudioCodecs(list);
        ring::Manager::instance().saveConfig();
    } else {
        RING_ERR("Could not find account %s", accountID.c_str());
    }
}

std::vector<std::string>
getAudioPluginList()
{
    return {PCM_DEFAULT, PCM_DMIX_DSNOOP};
}

void
setAudioPlugin(const std::string& audioPlugin)
{
    return ring::Manager::instance().setAudioPlugin(audioPlugin);
}

std::vector<std::string>
getAudioOutputDeviceList()
{
    return ring::Manager::instance().getAudioOutputDeviceList();
}

std::vector<std::string>
getAudioInputDeviceList()
{
    return ring::Manager::instance().getAudioInputDeviceList();
}

void
setAudioOutputDevice(int32_t index)
{
    return ring::Manager::instance().setAudioDevice(index, DeviceType::PLAYBACK);
}

void
setAudioInputDevice(int32_t index)
{
    return ring::Manager::instance().setAudioDevice(index, DeviceType::CAPTURE);
}

void
setAudioRingtoneDevice(int32_t index)
{
    return ring::Manager::instance().setAudioDevice(index, DeviceType::RINGTONE);
}

std::vector<std::string>
getCurrentAudioDevicesIndex()
{
    return ring::Manager::instance().getCurrentAudioDevicesIndex();
}

int32_t
getAudioInputDeviceIndex(const std::string& name)
{
    return ring::Manager::instance().getAudioInputDeviceIndex(name);
}

int32_t
getAudioOutputDeviceIndex(const std::string& name)
{
    return ring::Manager::instance().getAudioOutputDeviceIndex(name);
}

std::string
getCurrentAudioOutputPlugin()
{
    auto plugin = ring::Manager::instance().getCurrentAudioOutputPlugin();
    RING_DBG("Get audio plugin %s", plugin.c_str());
    return plugin;
}

bool
getNoiseSuppressState()
{
    return ring::Manager::instance().getNoiseSuppressState();
}

void
setNoiseSuppressState(bool state)
{
    ring::Manager::instance().setNoiseSuppressState(state);
}

bool
isAgcEnabled()
{
    return ring::Manager::instance().isAGCEnabled();
}

void
setAgcState(bool enabled)
{
    ring::Manager::instance().setAGCState(enabled);
}

int32_t
isIax2Enabled()
{
    return HAVE_IAX;
}

std::string
getRecordPath()
{
    return ring::Manager::instance().audioPreference.getRecordPath();
}

void
setRecordPath(const std::string& recPath)
{
    ring::Manager::instance().audioPreference.setRecordPath(recPath);
}

bool
getIsAlwaysRecording()
{
    return ring::Manager::instance().getIsAlwaysRecording();
}

void
setIsAlwaysRecording(bool rec)
{
    ring::Manager::instance().setIsAlwaysRecording(rec);
}

int32_t
getHistoryLimit()
{
    return ring::Manager::instance().getHistoryLimit();
}

void
clearHistory()
{
    return ring::Manager::instance().clearHistory();
}

void
setHistoryLimit(int32_t days)
{
    ring::Manager::instance().setHistoryLimit(days);
}

bool
setAudioManager(const std::string& api)
{
    return ring::Manager::instance().setAudioManager(api);
}

std::string
getAudioManager()
{
    return ring::Manager::instance().getAudioManager();
}

void
setVolume(const std::string& device, double value)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver()) {
        RING_DBG("set volume for %s: %f", device.c_str(), value);

        if (device == "speaker")
            audiolayer->setPlaybackGain(value);
        else if (device == "mic")
            audiolayer->setCaptureGain(value);

        ring::emitSignal<ConfigurationSignal::VolumeChanged>(device, value);
    } else {
        RING_ERR("Audio layer not valid while updating volume");
    }
}

double
getVolume(const std::string& device)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver()) {
        if (device == "speaker")
            return audiolayer->getPlaybackGain();
        if (device == "mic")
            return audiolayer->getCaptureGain();
    }

    RING_ERR("Audio layer not valid while updating volume");
    return 0.0;
}

// FIXME: we should store "muteDtmf" instead of "playDtmf"
// in config and avoid negating like this
bool
isDtmfMuted()
{
    return not ring::Manager::instance().voipPreferences.getPlayDtmf();
}

void
muteDtmf(bool mute)
{
    ring::Manager::instance().voipPreferences.setPlayDtmf(not mute);
}

bool
isCaptureMuted()
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->isCaptureMuted();

    RING_ERR("Audio layer not valid");
    return false;
}

void
muteCapture(bool mute)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->muteCapture(mute);

    RING_ERR("Audio layer not valid");
    return;
}

bool
isPlaybackMuted()
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->isPlaybackMuted();

    RING_ERR("Audio layer not valid");
    return false;
}

void
mutePlayback(bool mute)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->mutePlayback(mute);

    RING_ERR("Audio layer not valid");
    return;
}

std::map<std::string, std::string>
getHookSettings()
{
    return ring::Manager::instance().hookPreference.toMap();
}

void
setHookSettings(const std::map<std::string,
                std::string>& settings)
{
    ring::Manager::instance().hookPreference = HookPreference(settings);
}

void setAccountsOrder(const std::string& order)
{
    ring::Manager::instance().setAccountsOrder(order);
}

std::vector<std::map<std::string, std::string>>
getHistory()
{
    return ring::Manager::instance().getHistory();
}

std::string
getAddrFromInterfaceName(const std::string& interface)
{
    return ring::ip_utils::getInterfaceAddr(interface);
}

std::vector<std::string>
getAllIpInterface()
{
    return ring::ip_utils::getAllIpInterface();
}

std::vector<std::string>
getAllIpInterfaceByName()
{
    return ring::ip_utils::getAllIpInterfaceByName();
}

std::map<std::string, std::string>
getShortcuts()
{
    return ring::Manager::instance().shortcutPreferences.getShortcuts();
}

void
setShortcuts(const std::map<std::string, std::string>& shortcutsMap)
{
    ring::Manager::instance().shortcutPreferences.setShortcuts(shortcutsMap);
    ring::Manager::instance().saveConfig();
}

std::vector<std::map<std::string, std::string>>
getCredentials(const std::string& accountID)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID))
        return sipaccount->getCredentials();
    return {};
}

void
setCredentials(const std::string& accountID,
               const std::vector<std::map<std::string, std::string>>& details)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID))
        sipaccount->setCredentials(details);
}

} // namespace DRing
