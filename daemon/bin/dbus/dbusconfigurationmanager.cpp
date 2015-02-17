/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#include "dbusconfigurationmanager.h"
#include "configurationmanager_interface.h"

#include "media/audio/audiolayer.h"

DBusConfigurationManager::DBusConfigurationManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/ConfigurationManager")
{}

auto
DBusConfigurationManager::getAccountDetails(const std::string& accountID) -> decltype(DRing::getAccountDetails(accountID))
{
    return DRing::getAccountDetails(accountID);
}

auto
DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID) -> decltype(DRing::getVolatileAccountDetails(accountID))
{
    return DRing::getVolatileAccountDetails(accountID);
}

void
DBusConfigurationManager::setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    DRing::setAccountDetails(accountID, details);
}

auto
DBusConfigurationManager::getAccountTemplate() -> decltype(DRing::getAccountTemplate())
{
    return DRing::getAccountTemplate();
}

auto
DBusConfigurationManager::addAccount(const std::map<std::string, std::string>& details) -> decltype(DRing::addAccount(details))
{
    return DRing::addAccount(details);
}

void
DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    DRing::removeAccount(accountID);
}

auto
DBusConfigurationManager::getAccountList() -> decltype(DRing::getAccountList())
{
    return DRing::getAccountList();
}

void
DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    DRing::sendRegister(accountID, enable);
}

void
DBusConfigurationManager::registerAllAccounts(void)
{
    DRing::registerAllAccounts();
}

auto
DBusConfigurationManager::getTlsDefaultSettings() -> decltype(DRing::getTlsDefaultSettings())
{
    return DRing::getTlsDefaultSettings();
}

auto
DBusConfigurationManager::getAudioCodecList() -> decltype(DRing::getAudioCodecList())
{
    return DRing::getAudioCodecList();
}

auto
DBusConfigurationManager::getSupportedTlsMethod() -> decltype(DRing::getSupportedTlsMethod())
{
    return DRing::getSupportedTlsMethod();
}

auto
DBusConfigurationManager::getSupportedCiphers(const std::string& accountID) -> decltype(DRing::getSupportedCiphers(accountID))
{
    return DRing::getSupportedCiphers(accountID);
}

auto
DBusConfigurationManager::getAudioCodecDetails(const int32_t& payload) -> decltype(DRing::getAudioCodecDetails(payload))
{
    return DRing::getAudioCodecDetails(payload);
}

auto
DBusConfigurationManager::getActiveAudioCodecList(const std::string& accountID) -> decltype(DRing::getActiveAudioCodecList(accountID))
{
    return DRing::getActiveAudioCodecList(accountID);
}

void
DBusConfigurationManager::setActiveAudioCodecList(const std::vector<std::string>& list, const std::string& accountID)
{
    DRing::setActiveAudioCodecList(list, accountID);
}

auto
DBusConfigurationManager::getAudioPluginList() -> decltype(DRing::getAudioPluginList())
{
    return DRing::getAudioPluginList();
}

void
DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    DRing::setAudioPlugin(audioPlugin);
}

auto
DBusConfigurationManager::getAudioOutputDeviceList() -> decltype(DRing::getAudioOutputDeviceList())
{
    return DRing::getAudioOutputDeviceList();
}

void
DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    DRing::setAudioOutputDevice(index);
}

void
DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    DRing::setAudioInputDevice(index);
}

void
DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    DRing::setAudioRingtoneDevice(index);
}

auto
DBusConfigurationManager::getAudioInputDeviceList() -> decltype(DRing::getAudioInputDeviceList())
{
    return DRing::getAudioInputDeviceList();
}

auto
DBusConfigurationManager::getCurrentAudioDevicesIndex() -> decltype(DRing::getCurrentAudioDevicesIndex())
{
    return DRing::getCurrentAudioDevicesIndex();
}

auto
DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name) -> decltype(DRing::getAudioInputDeviceIndex(name))
{
    return DRing::getAudioInputDeviceIndex(name);
}

auto
DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name) -> decltype(DRing::getAudioOutputDeviceIndex(name))
{
    return DRing::getAudioOutputDeviceIndex(name);
}

auto
DBusConfigurationManager::getCurrentAudioOutputPlugin() -> decltype(DRing::getCurrentAudioOutputPlugin())
{
    return DRing::getCurrentAudioOutputPlugin();
}

auto
DBusConfigurationManager::getNoiseSuppressState() -> decltype(DRing::getNoiseSuppressState())
{
    return DRing::getNoiseSuppressState();
}

void
DBusConfigurationManager::setNoiseSuppressState(const bool& state)
{
    DRing::setNoiseSuppressState(state);
}

auto
DBusConfigurationManager::isAgcEnabled() -> decltype(DRing::isAgcEnabled())
{
    return DRing::isAgcEnabled();
}

void
DBusConfigurationManager::setAgcState(const bool& enabled)
{
    DRing::setAgcState(enabled);
}

void
DBusConfigurationManager::muteDtmf(const bool& mute)
{
    DRing::muteDtmf(mute);
}

auto
DBusConfigurationManager::isDtmfMuted() -> decltype(DRing::isDtmfMuted())
{
    return DRing::isDtmfMuted();
}

auto
DBusConfigurationManager::isCaptureMuted() -> decltype(DRing::isCaptureMuted())
{
    return DRing::isCaptureMuted();
}

void
DBusConfigurationManager::muteCapture(const bool& mute)
{
    DRing::muteCapture(mute);
}

auto
DBusConfigurationManager::isPlaybackMuted() -> decltype(DRing::isPlaybackMuted())
{
    return DRing::isPlaybackMuted();
}

void
DBusConfigurationManager::mutePlayback(const bool& mute)
{
    DRing::mutePlayback(mute);
}

auto
DBusConfigurationManager::getAudioManager() -> decltype(DRing::getAudioManager())
{
    return DRing::getAudioManager();
}

auto
DBusConfigurationManager::setAudioManager(const std::string& api) -> decltype(DRing::setAudioManager(api))
{
    return DRing::setAudioManager(api);
}

//FIXME
std::vector<std::string>
DBusConfigurationManager::getSupportedAudioManagers()
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

auto
DBusConfigurationManager::isIax2Enabled() -> decltype(DRing::isIax2Enabled())
{
    return DRing::isIax2Enabled();
}

auto
DBusConfigurationManager::getRecordPath() -> decltype(DRing::getRecordPath())
{
    return DRing::getRecordPath();
}

void
DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    DRing::setRecordPath(recPath);
}

auto
DBusConfigurationManager::getIsAlwaysRecording() -> decltype(DRing::getIsAlwaysRecording())
{
    return DRing::getIsAlwaysRecording();
}

void
DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    DRing::setIsAlwaysRecording(rec);
}

void
DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    DRing::setHistoryLimit(days);
}

auto
DBusConfigurationManager::getHistoryLimit() -> decltype(DRing::getHistoryLimit())
{
    return DRing::getHistoryLimit();
}

void
DBusConfigurationManager::clearHistory()
{
    DRing::clearHistory();
}

void
DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    DRing::setAccountsOrder(order);
}

auto
DBusConfigurationManager::getHookSettings() -> decltype(DRing::getHookSettings())
{
    return DRing::getHookSettings();
}

void
DBusConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    DRing::setHookSettings(settings);
}

auto
DBusConfigurationManager::getHistory() -> decltype(DRing::getHistory())
{
    return DRing::getHistory();
}

auto
DBusConfigurationManager::getTlsSettings() -> decltype(DRing::getTlsSettings())
{
    return DRing::getTlsSettings();
}

auto
DBusConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate, const std::string& privateKey) -> decltype(DRing::validateCertificate(accountId, certificate, privateKey))
{
   return DRing::validateCertificate(accountId, certificate, privateKey);
}

auto
DBusConfigurationManager::getCertificateDetails(const std::string& certificate) -> decltype(DRing::getCertificateDetails(certificate))
{
    return DRing::getCertificateDetails(certificate);
}

void
DBusConfigurationManager::setTlsSettings(const std::map<std::string, std::string>& details)
{
    DRing::setTlsSettings(details);
}

auto
DBusConfigurationManager::getIp2IpDetails() -> decltype(DRing::getIp2IpDetails())
{
    return DRing::getIp2IpDetails();
}

auto
DBusConfigurationManager::getCredentials(const std::string& accountID) -> decltype(DRing::getCredentials(accountID))
{
    return DRing::getCredentials(accountID);
}

void
DBusConfigurationManager::setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details)
{
    DRing::setCredentials(accountID, details);
}

auto
DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface) -> decltype(DRing::getAddrFromInterfaceName(interface))
{
    return DRing::getAddrFromInterfaceName(interface);
}

auto
DBusConfigurationManager::getAllIpInterface() -> decltype(DRing::getAllIpInterface())
{
    return DRing::getAllIpInterface();
}

auto
DBusConfigurationManager::getAllIpInterfaceByName() -> decltype(DRing::getAllIpInterfaceByName())
{
    return DRing::getAllIpInterfaceByName();
}

auto
DBusConfigurationManager::getShortcuts() -> decltype(DRing::getShortcuts())
{
    return DRing::getShortcuts();
}

void
DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    DRing::setShortcuts(shortcutsMap);
}

void
DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    DRing::setVolume(device, value);
}

auto
DBusConfigurationManager::getVolume(const std::string& device) -> decltype(DRing::getVolume(device))
{
    return DRing::getVolume(device);
}
