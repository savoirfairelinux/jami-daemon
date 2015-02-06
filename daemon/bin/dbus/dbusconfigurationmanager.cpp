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
#include <iostream>
#include "dring.h"

#include "dbusconfigurationmanager.h"
#include "managerimpl.h"
#include "manager.h"
#include "client/configurationmanager.h"

DBusConfigurationManager::DBusConfigurationManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/ConfigurationManager")
{}

std::map<std::string, std::string> DBusConfigurationManager::getAccountDetails(const std::string& accountID)
{
    return getAccountDetails(accountID);
}

std::map<std::string, std::string> DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
{
    return getVolatileAccountDetails(accountID);
}

void DBusConfigurationManager::setAccountDetails(const std::string& accountID, const std::map< std::string, std::string >& details)
{
    setAccountDetails(accountID, details);
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountTemplate()
{
    return getAccountTemplate();
}

std::string DBusConfigurationManager::addAccount(const std::map< std::string, std::string >& details)
{
    return addAccount(details);
}

void DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    removeAccount(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAccountList()
{
    return getAccountList();
}

void DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    sendRegister(accountID, enable);
}

void DBusConfigurationManager::registerAllAccounts(void)
{
    registerAllAccounts();
}

std::map< std::string, std::string > DBusConfigurationManager::getTlsDefaultSettings()
{
    return getTlsDefaultSettings();
}

std::vector< int32_t > DBusConfigurationManager::getAudioCodecList()
{
    return getAudioCodecList();
}

std::vector< std::string > DBusConfigurationManager::getSupportedTlsMethod()
{
    return getSupportedTlsMethod();
}

std::vector< std::string > DBusConfigurationManager::getSupportedCiphers(const std::string& accountID)
{
    return getSupportedCiphers(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioCodecDetails(const int32_t& payload)
{
    return getAudioCodecDetails(payload);
}

std::vector< int32_t > DBusConfigurationManager::getActiveAudioCodecList(const std::string& accountID)
{
    return getActiveAudioCodecList(accountID);
}

void DBusConfigurationManager::setActiveAudioCodecList(const std::vector< std::string >& list, const std::string& accountID)
{
    setActiveAudioCodecList(list, accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioPluginList()
{
    return getAudioPluginList();
}

void DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    setAudioPlugin(audioPlugin);
}

std::vector< std::string > DBusConfigurationManager::getAudioOutputDeviceList()
{
    return getAudioOutputDeviceList();
}

void DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    setAudioOutputDevice(index);
}

void DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    setAudioInputDevice(index);
}

void DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    setAudioRingtoneDevice(index);
}

std::vector< std::string > DBusConfigurationManager::getAudioInputDeviceList()
{
    return getAudioInputDeviceList();
}

std::vector< std::string > DBusConfigurationManager::getCurrentAudioDevicesIndex()
{
    return getCurrentAudioDevicesIndex();
}

int32_t DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    return getAudioInputDeviceIndex(name);
}

int32_t DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    return getAudioOutputDeviceIndex(name);
}

std::string DBusConfigurationManager::getCurrentAudioOutputPlugin()
{
    return getCurrentAudioOutputPlugin();
}

bool DBusConfigurationManager::getNoiseSuppressState()
{
    return getNoiseSuppressState();
}

void DBusConfigurationManager::setNoiseSuppressState(const bool& state)
{
    setNoiseSuppressState(state);
}

bool DBusConfigurationManager::isAgcEnabled()
{
    return isAgcEnabled();
}

void DBusConfigurationManager::setAgcState(const bool& enabled)
{
    setAgcState(enabled);
}

void DBusConfigurationManager::muteDtmf(const bool& mute)
{
    muteDtmf(mute);
}

bool DBusConfigurationManager::isDtmfMuted()
{
    return isDtmfMuted();
}

bool DBusConfigurationManager::isCaptureMuted()
{
    return isCaptureMuted();
}

void DBusConfigurationManager::muteCapture(const bool& mute)
{
    muteCapture(mute);
}

bool DBusConfigurationManager::isPlaybackMuted()
{
    return isPlaybackMuted();
}

void DBusConfigurationManager::mutePlayback(const bool& mute)
{
    mutePlayback(mute);
}

std::string DBusConfigurationManager::getAudioManager()
{
    return getAudioManager();
}

bool DBusConfigurationManager::setAudioManager(const std::string& api)
{
    return setAudioManager(api);
}

//FIXME
std::vector<std::string> DBusConfigurationManager::getSupportedAudioManagers()
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

int32_t DBusConfigurationManager::isIax2Enabled()
{
    return isIax2Enabled();
}

std::string DBusConfigurationManager::getRecordPath()
{
    return getRecordPath();
}

void DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    setRecordPath(recPath);
}

bool DBusConfigurationManager::getIsAlwaysRecording()
{
    return getIsAlwaysRecording();
}

void DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    setIsAlwaysRecording(rec);
}

void DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    setHistoryLimit(days);
}

int32_t DBusConfigurationManager::getHistoryLimit()
{
    return getHistoryLimit();
}

void DBusConfigurationManager::clearHistory()
{
    clearHistory();
}

void DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    setAccountsOrder(order);
}

std::map<std::string, std::string> DBusConfigurationManager::getHookSettings()
{
    return getHookSettings();
}

void DBusConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    setHookSettings(settings);
}

std::vector<std::map<std::string, std::string> > DBusConfigurationManager::getHistory()
{
    return getHistory();
}

std::map<std::string, std::string> DBusConfigurationManager::getTlsSettings()
{
    return getTlsSettings();
}

std::map<std::string, std::string> DBusConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate, const std::string& privateKey)
{
   return validateCertificate(accountId, certificate, privateKey);
}

std::map<std::string, std::string> DBusConfigurationManager::getCertificateDetails(const std::string& certificate)
{
    return getCertificateDetails(certificate);
}

void DBusConfigurationManager::setTlsSettings(const std::map< std::string, std::string >& details)
{
    setTlsSettings(details);
}

std::map< std::string, std::string > DBusConfigurationManager::getIp2IpDetails()
{
    return getIp2IpDetails();
}

std::vector< std::map< std::string, std::string > > DBusConfigurationManager::getCredentials(const std::string& accountID)
{
    return getCredentials(accountID);
}

void DBusConfigurationManager::setCredentials(const std::string& accountID, const std::vector< std::map< std::string, std::string > >& details)
{
    setCredentials(accountID, details);
}

std::string DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    return getAddrFromInterfaceName(interface);
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterface()
{
    return getAllIpInterface();
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterfaceByName()
{
    return getAllIpInterfaceByName();
}

std::map<std::string, std::string> DBusConfigurationManager::getShortcuts()
{
    return getShortcuts();
}

void DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    setShortcuts(shortcutsMap);
}

void DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    setVolume(device, value);
}

double DBusConfigurationManager::getVolume(const std::string& device)
{
    return getVolume(device);
}
