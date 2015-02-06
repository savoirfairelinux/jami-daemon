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
{
    configurationManager_ = ring::Manager::instance().getConfigurationManager();
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountDetails(const std::string& accountID)
{
    return configurationManager_->getAccountDetails(accountID);
}

std::map<std::string, std::string> DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
{
    return configurationManager_->getVolatileAccountDetails(accountID);
}

void DBusConfigurationManager::setAccountDetails(const std::string& accountID, const std::map< std::string, std::string >& details)
{
    configurationManager_->setAccountDetails(accountID, details);
}

std::map<std::string, std::string> DBusConfigurationManager::getAccountTemplate()
{
    return configurationManager_->getAccountTemplate();
}

std::string DBusConfigurationManager::addAccount(const std::map< std::string, std::string >& details)
{
    return configurationManager_->addAccount(details);
}

void DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    configurationManager_->removeAccount(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAccountList()
{
    return configurationManager_->getAccountList();
}

void DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    configurationManager_->sendRegister(accountID, enable);
}

void DBusConfigurationManager::registerAllAccounts(void)
{
    configurationManager_->registerAllAccounts();
}

std::map< std::string, std::string > DBusConfigurationManager::getTlsDefaultSettings()
{
    return configurationManager_->getTlsDefaultSettings();
}

std::vector< int32_t > DBusConfigurationManager::getAudioCodecList()
{
    return configurationManager_->getAudioCodecList();
}

std::vector< std::string > DBusConfigurationManager::getSupportedTlsMethod()
{
    return configurationManager_->getSupportedTlsMethod();
}

std::vector< std::string > DBusConfigurationManager::getSupportedCiphers(const std::string& accountID)
{
    return configurationManager_->getSupportedCiphers(accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioCodecDetails(const int32_t& payload)
{
    return configurationManager_->getAudioCodecDetails(payload);
}

std::vector< int32_t > DBusConfigurationManager::getActiveAudioCodecList(const std::string& accountID)
{
    return configurationManager_->getActiveAudioCodecList(accountID);
}

void DBusConfigurationManager::setActiveAudioCodecList(const std::vector< std::string >& list, const std::string& accountID)
{
    configurationManager_->setActiveAudioCodecList(list, accountID);
}

std::vector< std::string > DBusConfigurationManager::getAudioPluginList()
{
    return configurationManager_->getAudioPluginList();
}

void DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    configurationManager_->setAudioPlugin(audioPlugin);
}

std::vector< std::string > DBusConfigurationManager::getAudioOutputDeviceList()
{
    return configurationManager_->getAudioOutputDeviceList();
}

void DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    configurationManager_->setAudioOutputDevice(index);
}

void DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    configurationManager_->setAudioInputDevice(index);
}

void DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    configurationManager_->setAudioRingtoneDevice(index);
}

std::vector< std::string > DBusConfigurationManager::getAudioInputDeviceList()
{
    return configurationManager_->getAudioInputDeviceList();
}

std::vector< std::string > DBusConfigurationManager::getCurrentAudioDevicesIndex()
{
    return configurationManager_->getCurrentAudioDevicesIndex();
}

int32_t DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    return configurationManager_->getAudioInputDeviceIndex(name);
}

int32_t DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    return configurationManager_->getAudioOutputDeviceIndex(name);
}

std::string DBusConfigurationManager::getCurrentAudioOutputPlugin()
{
    return configurationManager_->getCurrentAudioOutputPlugin();
}

bool DBusConfigurationManager::getNoiseSuppressState()
{
    return configurationManager_->getNoiseSuppressState();
}

void DBusConfigurationManager::setNoiseSuppressState(const bool& state)
{
    configurationManager_->setNoiseSuppressState(state);
}

bool DBusConfigurationManager::isAgcEnabled()
{
    return configurationManager_->isAgcEnabled();
}

void DBusConfigurationManager::setAgcState(const bool& enabled)
{
    configurationManager_->setAgcState(enabled);
}

void DBusConfigurationManager::muteDtmf(const bool& mute)
{
    configurationManager_->muteDtmf(mute);
}

bool DBusConfigurationManager::isDtmfMuted()
{
    return configurationManager_->isDtmfMuted();
}

bool DBusConfigurationManager::isCaptureMuted()
{
    return configurationManager_->isCaptureMuted();
}

void DBusConfigurationManager::muteCapture(const bool& mute)
{
    configurationManager_->muteCapture(mute);
}

bool DBusConfigurationManager::isPlaybackMuted()
{
    return configurationManager_->isPlaybackMuted();
}

void DBusConfigurationManager::mutePlayback(const bool& mute)
{
    configurationManager_->mutePlayback(mute);
}

std::map<std::string, std::string> DBusConfigurationManager::getRingtoneList()
{
    return configurationManager_->getRingtoneList();
}

std::string DBusConfigurationManager::getAudioManager()
{
    return configurationManager_->getAudioManager();
}

bool DBusConfigurationManager::setAudioManager(const std::string& api)
{
    return configurationManager_->setAudioManager(api);
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
    return configurationManager_->isIax2Enabled();
}

std::string DBusConfigurationManager::getRecordPath()
{
    return configurationManager_->getRecordPath();
}

void DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    configurationManager_->setRecordPath(recPath);
}

bool DBusConfigurationManager::getIsAlwaysRecording()
{
    return configurationManager_->getIsAlwaysRecording();
}

void DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    configurationManager_->setIsAlwaysRecording(rec);
}

void DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    configurationManager_->setHistoryLimit(days);
}

int32_t DBusConfigurationManager::getHistoryLimit()
{
    return configurationManager_->getHistoryLimit();
}

void DBusConfigurationManager::clearHistory()
{
    configurationManager_->clearHistory();
}

void DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    configurationManager_->setAccountsOrder(order);
}

std::map<std::string, std::string> DBusConfigurationManager::getHookSettings()
{
    return configurationManager_->getHookSettings();
}

void DBusConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    configurationManager_->setHookSettings(settings);
}

std::vector<std::map<std::string, std::string> > DBusConfigurationManager::getHistory()
{
    return configurationManager_->getHistory();
}

std::map<std::string, std::string> DBusConfigurationManager::getTlsSettings()
{
    return configurationManager_->getTlsSettings();
}

std::map<std::string, std::string> DBusConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate, const std::string& privateKey)
{
   return configurationManager_->validateCertificate(accountId, certificate, privateKey);
}

std::map<std::string, std::string> DBusConfigurationManager::getCertificateDetails(const std::string& certificate)
{
    return configurationManager_->getCertificateDetails(certificate);
}

void DBusConfigurationManager::setTlsSettings(const std::map< std::string, std::string >& details)
{
    configurationManager_->setTlsSettings(details);
}

std::map< std::string, std::string > DBusConfigurationManager::getIp2IpDetails()
{
    return configurationManager_->getIp2IpDetails();
}

std::vector< std::map< std::string, std::string > > DBusConfigurationManager::getCredentials(const std::string& accountID)
{
    return configurationManager_->getCredentials(accountID);
}

void DBusConfigurationManager::setCredentials(const std::string& accountID, const std::vector< std::map< std::string, std::string > >& details)
{
    configurationManager_->setCredentials(accountID, details);
}

std::string DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    return configurationManager_->getAddrFromInterfaceName(interface);
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterface()
{
    return configurationManager_->getAllIpInterface();
}

std::vector<std::string> DBusConfigurationManager::getAllIpInterfaceByName()
{
    return configurationManager_->getAllIpInterfaceByName();
}

std::map<std::string, std::string> DBusConfigurationManager::getShortcuts()
{
    return configurationManager_->getShortcuts();
}

void DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    configurationManager_->setShortcuts(shortcutsMap);
}

void DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    configurationManager_->setVolume(device, value);
}

double DBusConfigurationManager::getVolume(const std::string& device)
{
    return configurationManager_->getVolume(device);
}
