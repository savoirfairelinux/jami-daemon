/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
DBusConfigurationManager::testAccountICEInitialization(const std::string& accountID) -> decltype(DRing::testAccountICEInitialization(accountID))
{
    return DRing::testAccountICEInitialization(accountID);
}


void
DBusConfigurationManager::setAccountActive(const std::string& accountID, const bool& active)
{
    DRing::setAccountActive(accountID, active);
}

auto
DBusConfigurationManager::getAccountTemplate(const std::string& accountType) -> decltype(DRing::getAccountTemplate(accountType))
{
    return DRing::getAccountTemplate(accountType);
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
DBusConfigurationManager::sendTextMessage(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& payloads) -> decltype(DRing::sendAccountTextMessage(accountID, to, payloads))
{
    return DRing::sendAccountTextMessage(accountID, to, payloads);
}

auto
DBusConfigurationManager::getMessageStatus(const uint64_t& id) -> decltype(DRing::getMessageStatus(id))
{
    return DRing::getMessageStatus(id);
}

auto
DBusConfigurationManager::getTlsDefaultSettings() -> decltype(DRing::getTlsDefaultSettings())
{
    return DRing::getTlsDefaultSettings();
}

auto
DBusConfigurationManager::getCodecList() -> decltype(DRing::getCodecList())
{
    return DRing::getCodecList();
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
DBusConfigurationManager::getCodecDetails(const std::string& accountID, const unsigned& codecId) -> decltype(DRing::getCodecDetails(accountID, codecId))
{
    return DRing::getCodecDetails(accountID, codecId);
}

auto
DBusConfigurationManager::setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details)
-> decltype(DRing::setCodecDetails(accountID, codecId, details))
{
    return DRing::setCodecDetails(accountID, codecId, details);
}

auto
DBusConfigurationManager::getActiveCodecList(const std::string& accountID) -> decltype(DRing::getActiveCodecList(accountID))
{
    return DRing::getActiveCodecList(accountID);
}

void
DBusConfigurationManager::setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list)
{
    DRing::setActiveCodecList(accountID, list);
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
DBusConfigurationManager::isRingtoneMuted() -> decltype(DRing::isRingtoneMuted())
{
    return DRing::isRingtoneMuted();
}

void
DBusConfigurationManager::muteRingtone(const bool& mute)
{
    DRing::muteRingtone(mute);
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
DBusConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate) -> decltype(DRing::validateCertificate(accountId, certificate))
{
   return DRing::validateCertificate(accountId, certificate);
}

auto
DBusConfigurationManager::validateCertificatePath(const std::string& accountId, const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass, const std::string& caList) -> decltype(DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList))
{
   return DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList);
}

auto
DBusConfigurationManager::getCertificateDetails(const std::string& certificate) -> decltype(DRing::getCertificateDetails(certificate))
{
    return DRing::getCertificateDetails(certificate);
}

auto
DBusConfigurationManager::getCertificateDetailsPath(const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass) -> decltype(DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass))
{
    return DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass);
}

auto
DBusConfigurationManager::getPinnedCertificates() -> decltype(DRing::getPinnedCertificates())
{
    return DRing::getPinnedCertificates();
}

auto
DBusConfigurationManager::pinCertificate(const std::vector<uint8_t>& certificate, const bool& local) -> decltype(DRing::pinCertificate(certificate, local))
{
    return DRing::pinCertificate(certificate, local);
}

void
DBusConfigurationManager::pinCertificatePath(const std::string& certPath)
{
    return DRing::pinCertificatePath(certPath);
}

auto
DBusConfigurationManager::unpinCertificate(const std::string& certId) -> decltype(DRing::unpinCertificate(certId))
{
    return DRing::unpinCertificate(certId);
}

auto
DBusConfigurationManager::unpinCertificatePath(const std::string& p) -> decltype(DRing::unpinCertificatePath(p))
{
    return DRing::unpinCertificatePath(p);
}

auto
DBusConfigurationManager::pinRemoteCertificate(const std::string& accountId, const std::string& certId) -> decltype(DRing::pinRemoteCertificate(accountId, certId))
{
    return DRing::pinRemoteCertificate(accountId, certId);
}

auto
DBusConfigurationManager::setCertificateStatus(const std::string& accountId, const std::string& certId, const std::string& status) -> decltype(DRing::setCertificateStatus(accountId, certId, status))
{
    return DRing::setCertificateStatus(accountId, certId, status);
}

auto
DBusConfigurationManager::getCertificatesByStatus(const std::string& accountId, const std::string& status) -> decltype(DRing::getCertificatesByStatus(accountId, status))
{
    return DRing::getCertificatesByStatus(accountId, status);
}

auto
DBusConfigurationManager::getTrustRequests(const std::string& accountId) -> decltype(DRing::getTrustRequests(accountId))
{
    return DRing::getTrustRequests(accountId);
}

auto
DBusConfigurationManager::acceptTrustRequest(const std::string& accountId, const std::string& from) -> decltype(DRing::acceptTrustRequest(accountId, from))
{
    return DRing::acceptTrustRequest(accountId, from);
}

auto
DBusConfigurationManager::discardTrustRequest(const std::string& accountId, const std::string& from) -> decltype(DRing::discardTrustRequest(accountId, from))
{
    return DRing::discardTrustRequest(accountId, from);
}

void
DBusConfigurationManager::sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload)
{
    DRing::sendTrustRequest(accountId, to, payload);
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

auto
DBusConfigurationManager::exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password) -> decltype(DRing::exportAccounts(accountIDs, filepath, password))
{
    return DRing::exportAccounts(accountIDs, filepath, password);
}

auto
DBusConfigurationManager::importAccounts(const std::string& archivePath, const std::string& password) -> decltype(DRing::importAccounts(archivePath, password))
{
    return DRing::importAccounts(archivePath, password);
}

void
DBusConfigurationManager::connectivityChanged()
{
    DRing::connectivityChanged();
}
