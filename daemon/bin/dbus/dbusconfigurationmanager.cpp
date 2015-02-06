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

static ring::ConfigurationManager* getConfigurationManager()
{
    return ring::Manager::instance().getConfigurationManager();
}

DBusConfigurationManager::DBusConfigurationManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/ConfigurationManager")
{
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountDetails(const std::string& accountID)
{
    return getConfigurationManager()->getAccountDetails(accountID);
}

std::map<std::string, std::string> DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
{
    return getConfigurationManager()->getVolatileAccountDetails(accountID);
}

void DBusConfigurationManager::setAccountDetails(const std::string& accountID, const std::map< std::string, std::string >& details)
{
    getConfigurationManager()->setAccountDetails(accountID, details);
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountTemplate()
{
    return getConfigurationManager()->getAccountTemplate();
}

std::string DBusConfigurationManager::addAccount(const std::map< std::string, std::string >& details)
{
    return getConfigurationManager()->addAccount(details);
}

void DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    getConfigurationManager()->removeAccount(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAccountList()
{
    return getConfigurationManager()->getAccountList();
}

void DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    getConfigurationManager()->sendRegister(accountID, enable);
}

void DBusConfigurationManager::registerAllAccounts(void)
{
    getConfigurationManager()->registerAllAccounts();
}

std::map< std::string, std::string > DBusConfigurationManager::getTlsDefaultSettings()
{
    return getConfigurationManager()->getTlsDefaultSettings();
}

std::vector< int32_t > DBusConfigurationManager::getAudioCodecList()
{
    return getConfigurationManager()->getAudioCodecList();
}

std::vector< std::string > DBusConfigurationManager::getSupportedTlsMethod()
{
    return getConfigurationManager()->getSupportedTlsMethod();
}

std::vector< std::string > DBusConfigurationManager::getSupportedCiphers(const std::string& accountID)
{
    return getConfigurationManager()->getSupportedCiphers(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioCodecDetails(const int32_t& payload)
{
    return getConfigurationManager()->getAudioCodecDetails(payload);
}

std::vector< int32_t > DBusConfigurationManager::getActiveAudioCodecList(const std::string& accountID)
{
    return getConfigurationManager()->getActiveAudioCodecList(accountID);
}

void DBusConfigurationManager::setActiveAudioCodecList(const std::vector< std::string >& list, const std::string& accountID)
{
    getConfigurationManager()->setActiveAudioCodecList(list, accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioPluginList()
{
    return getConfigurationManager()->getAudioPluginList();
}

void DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    getConfigurationManager()->setAudioPlugin(audioPlugin);
}

std::vector< std::string > DBusConfigurationManager::getAudioOutputDeviceList()
{
    return getConfigurationManager()->getAudioOutputDeviceList();
}

void DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    getConfigurationManager()->setAudioOutputDevice(index);
}

void DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    getConfigurationManager()->setAudioInputDevice(index);
}

void DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    getConfigurationManager()->setAudioRingtoneDevice(index);
}

std::vector< std::string > DBusConfigurationManager::getAudioInputDeviceList()
{
    return getConfigurationManager()->getAudioInputDeviceList();
}

std::vector< std::string > DBusConfigurationManager::getCurrentAudioDevicesIndex()
{
    return getConfigurationManager()->getCurrentAudioDevicesIndex();
}

int32_t DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    return getConfigurationManager()->getAudioInputDeviceIndex(name);
}

int32_t DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    return getConfigurationManager()->getAudioOutputDeviceIndex(name);
}

std::string DBusConfigurationManager::getCurrentAudioOutputPlugin()
{
    return getConfigurationManager()->getCurrentAudioOutputPlugin();
}

bool DBusConfigurationManager::getNoiseSuppressState()
{
    return getConfigurationManager()->getNoiseSuppressState();
}

void DBusConfigurationManager::setNoiseSuppressState(const bool& state)
{
    getConfigurationManager()->setNoiseSuppressState(state);
}

bool DBusConfigurationManager::isAgcEnabled()
{
    return getConfigurationManager()->isAgcEnabled();
}

void DBusConfigurationManager::setAgcState(const bool& enabled)
{
    getConfigurationManager()->setAgcState(enabled);
}

void DBusConfigurationManager::muteDtmf(const bool& mute)
{
    getConfigurationManager()->muteDtmf(mute);
}

bool DBusConfigurationManager::isDtmfMuted()
{
    return getConfigurationManager()->isDtmfMuted();
}

bool DBusConfigurationManager::isCaptureMuted()
{
    return getConfigurationManager()->isCaptureMuted();
}

void DBusConfigurationManager::muteCapture(const bool& mute)
{
    getConfigurationManager()->muteCapture(mute);
}

bool DBusConfigurationManager::isPlaybackMuted()
{
    return getConfigurationManager()->isPlaybackMuted();
}

void DBusConfigurationManager::mutePlayback(const bool& mute)
{
    getConfigurationManager()->mutePlayback(mute);
}

std::map<std::string, std::string> DBusConfigurationManager::getRingtoneList()
{
    return getConfigurationManager()->getRingtoneList();
}

std::string DBusConfigurationManager::getAudioManager()
{
    return getConfigurationManager()->getAudioManager();
}

bool DBusConfigurationManager::setAudioManager(const std::string& api)
{
    return getConfigurationManager()->setAudioManager(api);
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
    return getConfigurationManager()->isIax2Enabled();
}

std::string DBusConfigurationManager::getRecordPath()
{
    return getConfigurationManager()->getRecordPath();
}

void DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    getConfigurationManager()->setRecordPath(recPath);
}

bool DBusConfigurationManager::getIsAlwaysRecording()
{
    return getConfigurationManager()->getIsAlwaysRecording();
}

void DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    getConfigurationManager()->setIsAlwaysRecording(rec);
}

void DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    getConfigurationManager()->setHistoryLimit(days);
}

int32_t DBusConfigurationManager::getHistoryLimit()
{
    return getConfigurationManager()->getHistoryLimit();
}

void DBusConfigurationManager::clearHistory()
{
    getConfigurationManager()->clearHistory();
}

void DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    getConfigurationManager()->setAccountsOrder(order);
}

std::map<std::string, std::string> DBusConfigurationManager::getHookSettings()
{
    return getConfigurationManager()->getHookSettings();
}

void DBusConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    getConfigurationManager()->setHookSettings(settings);
}

std::vector<std::map<std::string, std::string> > DBusConfigurationManager::getHistory()
{
    return getConfigurationManager()->getHistory();
}

std::map<std::string, std::string> DBusConfigurationManager::getTlsSettings()
{
    return getConfigurationManager()->getTlsSettings();
}

std::map<std::string, std::string> DBusConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate, const std::string& privateKey)
{
   return getConfigurationManager()->validateCertificate(accountId, certificate, privateKey);
}

std::map<std::string, std::string> DBusConfigurationManager::getCertificateDetails(const std::string& certificate)
{
    return getConfigurationManager()->getCertificateDetails(certificate);
}

void DBusConfigurationManager::setTlsSettings(const std::map< std::string, std::string >& details)
{
    getConfigurationManager()->setTlsSettings(details);
}

std::map< std::string, std::string > DBusConfigurationManager::getIp2IpDetails()
{
    return getConfigurationManager()->getIp2IpDetails();
}

std::vector< std::map< std::string, std::string > > DBusConfigurationManager::getCredentials(const std::string& accountID)
{
    return getConfigurationManager()->getCredentials(accountID);
}

void DBusConfigurationManager::setCredentials(const std::string& accountID, const std::vector< std::map< std::string, std::string > >& details)
{
    getConfigurationManager()->setCredentials(accountID, details);
}

std::string DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    return getConfigurationManager()->getAddrFromInterfaceName(interface);
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterface()
{
    return getConfigurationManager()->getAllIpInterface();
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterfaceByName()
{
    return getConfigurationManager()->getAllIpInterfaceByName();
}

std::map<std::string, std::string> DBusConfigurationManager::getShortcuts()
{
    return getConfigurationManager()->getShortcuts();
}

void DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    getConfigurationManager()->setShortcuts(shortcutsMap);
}

void DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    getConfigurationManager()->setVolume(device, value);
}

double DBusConfigurationManager::getVolume(const std::string& device)
{
    return getConfigurationManager()->getVolume(device);
}

bool DBusConfigurationManager::checkForPrivateKey(const std::string& pemPath)
{
    return getConfigurationManager()->checkForPrivateKey(pemPath);
}

bool DBusConfigurationManager::checkCertificateValidity(const std::string& caPath, const std::string& pemPath)
{
    return getConfigurationManager()->checkCertificateValidity(caPath, pemPath);
}

bool DBusConfigurationManager::checkHostnameCertificate(const  std::string& host, const std::string& port)
{
    return getConfigurationManager()->checkHostnameCertificate(host, port);
}
