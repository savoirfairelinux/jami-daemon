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
    return DRing::getAccountDetails(accountID);
}

std::map<std::string, std::string> DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
{
    return DRing::getVolatileAccountDetails(accountID);
}

void DBusConfigurationManager::setAccountDetails(const std::string& accountID, const std::map< std::string, std::string >& details)
{
    DRing::setAccountDetails(accountID, details);
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountTemplate()
{
    return DRing::getAccountTemplate();
}

std::string DBusConfigurationManager::addAccount(const std::map< std::string, std::string >& details)
{
    return DRing::addAccount(details);
}

void DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    DRing::removeAccount(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAccountList()
{
    return DRing::getAccountList();
}

void DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    DRing::sendRegister(accountID, enable);
}

void DBusConfigurationManager::registerAllAccounts(void)
{
    DRing::registerAllAccounts();
}

std::map< std::string, std::string > DBusConfigurationManager::getTlsDefaultSettings()
{
    return DRing::getTlsDefaultSettings();
}

std::vector< int32_t > DBusConfigurationManager::getAudioCodecList()
{
    return DRing::getAudioCodecList();
}

std::vector< std::string > DBusConfigurationManager::getSupportedTlsMethod()
{
    return DRing::getSupportedTlsMethod();
}

std::vector< std::string > DBusConfigurationManager::getSupportedCiphers(const std::string& accountID)
{
    return DRing::getSupportedCiphers(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioCodecDetails(const int32_t& payload)
{
    return DRing::getAudioCodecDetails(payload);
}

std::vector< int32_t > DBusConfigurationManager::getActiveAudioCodecList(const std::string& accountID)
{
    return DRing::getActiveAudioCodecList(accountID);
}

void DBusConfigurationManager::setActiveAudioCodecList(const std::vector< std::string >& list, const std::string& accountID)
{
    DRing::setActiveAudioCodecList(list, accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioPluginList()
{
    return DRing::getAudioPluginList();
}

void DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    DRing::setAudioPlugin(audioPlugin);
}

std::vector< std::string > DBusConfigurationManager::getAudioOutputDeviceList()
{
    return DRing::getAudioOutputDeviceList();
}

void DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    DRing::setAudioOutputDevice(index);
}

void DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    DRing::setAudioInputDevice(index);
}

void DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    DRing::setAudioRingtoneDevice(index);
}

std::vector< std::string > DBusConfigurationManager::getAudioInputDeviceList()
{
    return DRing::getAudioInputDeviceList();
}

std::vector< std::string > DBusConfigurationManager::getCurrentAudioDevicesIndex()
{
    return DRing::getCurrentAudioDevicesIndex();
}

int32_t DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    return DRing::getAudioInputDeviceIndex(name);
}

int32_t DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    return DRing::getAudioOutputDeviceIndex(name);
}

std::string DBusConfigurationManager::getCurrentAudioOutputPlugin()
{
    return DRing::getCurrentAudioOutputPlugin();
}

bool DBusConfigurationManager::getNoiseSuppressState()
{
    return DRing::getNoiseSuppressState();
}

void DBusConfigurationManager::setNoiseSuppressState(const bool& state)
{
    DRing::setNoiseSuppressState(state);
}

bool DBusConfigurationManager::isAgcEnabled()
{
    return DRing::isAgcEnabled();
}

void DBusConfigurationManager::setAgcState(const bool& enabled)
{
    DRing::setAgcState(enabled);
}

void DBusConfigurationManager::muteDtmf(const bool& mute)
{
    DRing::muteDtmf(mute);
}

bool DBusConfigurationManager::isDtmfMuted()
{
    return DRing::isDtmfMuted();
}

bool DBusConfigurationManager::isCaptureMuted()
{
    return DRing::isCaptureMuted();
}

void DBusConfigurationManager::muteCapture(const bool& mute)
{
    DRing::muteCapture(mute);
}

bool DBusConfigurationManager::isPlaybackMuted()
{
    return DRing::isPlaybackMuted();
}

void DBusConfigurationManager::mutePlayback(const bool& mute)
{
    DRing::mutePlayback(mute);
}

std::string DBusConfigurationManager::getAudioManager()
{
    return DRing::getAudioManager();
}

bool DBusConfigurationManager::setAudioManager(const std::string& api)
{
    return DRing::setAudioManager(api);
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
    return DRing::isIax2Enabled();
}

std::string DBusConfigurationManager::getRecordPath()
{
    return DRing::getRecordPath();
}

void DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    DRing::setRecordPath(recPath);
}

bool DBusConfigurationManager::getIsAlwaysRecording()
{
    return DRing::getIsAlwaysRecording();
}

void DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    DRing::setIsAlwaysRecording(rec);
}

void DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    DRing::setHistoryLimit(days);
}

int32_t DBusConfigurationManager::getHistoryLimit()
{
    return DRing::getHistoryLimit();
}

void DBusConfigurationManager::clearHistory()
{
    DRing::clearHistory();
}

void DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    DRing::setAccountsOrder(order);
}

std::map<std::string, std::string> DBusConfigurationManager::getHookSettings()
{
    return DRing::getHookSettings();
}

void DBusConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    DRing::setHookSettings(settings);
}

std::vector<std::map<std::string, std::string> > DBusConfigurationManager::getHistory()
{
    return DRing::getHistory();
}

std::map<std::string, std::string> DBusConfigurationManager::getTlsSettings()
{
    return DRing::getTlsSettings();
}

std::map<std::string, std::string> DBusConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate, const std::string& privateKey)
{
   return DRing::validateCertificate(accountId, certificate, privateKey);
}

std::map<std::string, std::string> DBusConfigurationManager::getCertificateDetails(const std::string& certificate)
{
    return DRing::getCertificateDetails(certificate);
}

void DBusConfigurationManager::setTlsSettings(const std::map< std::string, std::string >& details)
{
    DRing::setTlsSettings(details);
}

std::map< std::string, std::string > DBusConfigurationManager::getIp2IpDetails()
{
    return DRing::getIp2IpDetails();
}

std::vector< std::map< std::string, std::string > > DBusConfigurationManager::getCredentials(const std::string& accountID)
{
    return DRing::getCredentials(accountID);
}

void DBusConfigurationManager::setCredentials(const std::string& accountID, const std::vector< std::map< std::string, std::string > >& details)
{
    DRing::setCredentials(accountID, details);
}

std::string DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    return DRing::getAddrFromInterfaceName(interface);
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterface()
{
    return DRing::getAllIpInterface();
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterfaceByName()
{
    return DRing::getAllIpInterfaceByName();
}

std::map<std::string, std::string> DBusConfigurationManager::getShortcuts()
{
    return DRing::getShortcuts();
}

void DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    DRing::setShortcuts(shortcutsMap);
}

void DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    DRing::setVolume(device, value);
}

double DBusConfigurationManager::getVolume(const std::string& device)
{
    return DRing::getVolume(device);
}
