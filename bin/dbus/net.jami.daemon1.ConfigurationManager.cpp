/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

auto
getAccountDetails(const std::string& accountID) -> decltype(DRing::getAccountDetails(accountID))
{
    return DRing::getAccountDetails(accountID);
}

auto
getVolatileAccountDetails(const std::string& accountID) -> decltype(DRing::getVolatileAccountDetails(accountID))
{
    return DRing::getVolatileAccountDetails(accountID);
}

void
setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    DRing::setAccountDetails(accountID, details);
}

auto
testAccountICEInitialization(const std::string& accountID) -> decltype(DRing::testAccountICEInitialization(accountID))
{
    return DRing::testAccountICEInitialization(accountID);
}


void
setAccountActive(const std::string& accountID, const bool& active)
{
    DRing::setAccountActive(accountID, active);
}

auto
getAccountTemplate(const std::string& accountType) -> decltype(DRing::getAccountTemplate(accountType))
{
    return DRing::getAccountTemplate(accountType);
}

auto
addAccount(const std::map<std::string, std::string>& details) -> decltype(DRing::addAccount(details))
{
    return DRing::addAccount(details);
}

auto
exportOnRing(const std::string& accountID, const std::string& password) -> decltype(DRing::exportOnRing(accountID, password))
{
    return DRing::exportOnRing(accountID, password);
}

auto
exportToFile(const std::string& accountID, const std::string& destinationPath, const std::string& password) -> decltype(DRing::exportToFile(accountID, destinationPath, password))
{
    return DRing::exportToFile(accountID, destinationPath, password);
}

auto
revokeDevice(const std::string& accountID, const std::string& password, const std::string& device) -> decltype(DRing::revokeDevice(accountID, password, device))
{
    return DRing::revokeDevice(accountID, password, device);
}

auto
getKnownRingDevices(const std::string& accountID) -> decltype(DRing::getKnownRingDevices(accountID))
{
    return DRing::getKnownRingDevices(accountID);
}

auto
changeAccountPassword(const std::string& accountID, const std::string& password_old, const std::string& password_new) -> decltype(DRing::changeAccountPassword(accountID, password_old, password_new))
{
    return DRing::changeAccountPassword(accountID, password_old, password_new);
}

auto
lookupName(const std::string& account, const std::string& nameserver, const std::string& name) -> decltype(DRing::lookupName(account, nameserver, name))
{
    return DRing::lookupName(account, nameserver, name);
}

auto
lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address) -> decltype(DRing::lookupAddress(account, nameserver, address))
{
    return DRing::lookupAddress(account, nameserver, address);
}

auto
registerName(const std::string& account, const std::string& password, const std::string& name) -> decltype(DRing::registerName(account, password, name))
{
    return DRing::registerName(account, password, name);
}

void
removeAccount(const std::string& accountID)
{
    DRing::removeAccount(accountID);
}

auto
getAccountList() -> decltype(DRing::getAccountList())
{
    return DRing::getAccountList();
}

void
sendRegister(const std::string& accountID, const bool& enable)
{
    DRing::sendRegister(accountID, enable);
}

void
registerAllAccounts(void)
{
    DRing::registerAllAccounts();
}

auto
sendTextMessage(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& payloads) -> decltype(DRing::sendAccountTextMessage(accountID, to, payloads))
{
    return DRing::sendAccountTextMessage(accountID, to, payloads);
}

std::vector<sdbus::Struct<std::string, std::map<std::string, std::string>, uint64_t>>
getLastMessages(const std::string& accountID, const uint64_t& base_timestamp)
{
    auto messages = DRing::getLastMessages(accountID, base_timestamp);
    std::vector<sdbus::Struct<std::string, std::map<std::string, std::string>, uint64_t>> result;
    for (const auto& message : messages) {
        result.emplace_back(sdbus::make_struct(message.from, message.payloads, message.received));
    }
    return result;
}

std::map<std::string, std::string>
getNearbyPeers(const std::string& accountID)
{
    return DRing::getNearbyPeers(accountID);
}

auto
getMessageStatus(const uint64_t& id) -> decltype(DRing::getMessageStatus(id))
{
    return DRing::getMessageStatus(id);
}

auto
getMessageStatus(const std::string& accountID, const uint64_t& id) -> decltype(DRing::getMessageStatus(accountID, id))
{
    return DRing::getMessageStatus(accountID, id);
}

bool
cancelMessage(const std::string& accountID, const uint64_t& id)
{
    return DRing::cancelMessage(accountID, id);
}

auto
getTlsDefaultSettings() -> decltype(DRing::getTlsDefaultSettings())
{
    return DRing::getTlsDefaultSettings();
}

auto
getCodecList() -> decltype(DRing::getCodecList())
{
    return DRing::getCodecList();
}

auto
getSupportedTlsMethod() -> decltype(DRing::getSupportedTlsMethod())
{
    return DRing::getSupportedTlsMethod();
}

auto
getSupportedCiphers(const std::string& accountID) -> decltype(DRing::getSupportedCiphers(accountID))
{
    return DRing::getSupportedCiphers(accountID);
}

auto
getCodecDetails(const std::string& accountID, const unsigned& codecId) -> decltype(DRing::getCodecDetails(accountID, codecId))
{
    return DRing::getCodecDetails(accountID, codecId);
}

auto
setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details)
-> decltype(DRing::setCodecDetails(accountID, codecId, details))
{
    return DRing::setCodecDetails(accountID, codecId, details);
}

auto
getActiveCodecList(const std::string& accountID) -> decltype(DRing::getActiveCodecList(accountID))
{
    return DRing::getActiveCodecList(accountID);
}

void
setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list)
{
    DRing::setActiveCodecList(accountID, list);
}

auto
getAudioPluginList() -> decltype(DRing::getAudioPluginList())
{
    return DRing::getAudioPluginList();
}

void
setAudioPlugin(const std::string& audioPlugin)
{
    DRing::setAudioPlugin(audioPlugin);
}

auto
getAudioOutputDeviceList() -> decltype(DRing::getAudioOutputDeviceList())
{
    return DRing::getAudioOutputDeviceList();
}

void
setAudioOutputDevice(const int32_t& index)
{
    DRing::setAudioOutputDevice(index);
}

void
setAudioInputDevice(const int32_t& index)
{
    DRing::setAudioInputDevice(index);
}

void
setAudioRingtoneDevice(const int32_t& index)
{
    DRing::setAudioRingtoneDevice(index);
}

auto
getAudioInputDeviceList() -> decltype(DRing::getAudioInputDeviceList())
{
    return DRing::getAudioInputDeviceList();
}

auto
getCurrentAudioDevicesIndex() -> decltype(DRing::getCurrentAudioDevicesIndex())
{
    return DRing::getCurrentAudioDevicesIndex();
}

auto
getAudioInputDeviceIndex(const std::string& name) -> decltype(DRing::getAudioInputDeviceIndex(name))
{
    return DRing::getAudioInputDeviceIndex(name);
}

auto
getAudioOutputDeviceIndex(const std::string& name) -> decltype(DRing::getAudioOutputDeviceIndex(name))
{
    return DRing::getAudioOutputDeviceIndex(name);
}

auto
getCurrentAudioOutputPlugin() -> decltype(DRing::getCurrentAudioOutputPlugin())
{
    return DRing::getCurrentAudioOutputPlugin();
}

auto
getNoiseSuppressState() -> decltype(DRing::getNoiseSuppressState())
{
    return DRing::getNoiseSuppressState();
}

void
setNoiseSuppressState(const bool& state)
{
    DRing::setNoiseSuppressState(state);
}

auto
isAgcEnabled() -> decltype(DRing::isAgcEnabled())
{
    return DRing::isAgcEnabled();
}

void
setAgcState(const bool& enabled)
{
    DRing::setAgcState(enabled);
}

void
muteDtmf(const bool& mute)
{
    DRing::muteDtmf(mute);
}

auto
isDtmfMuted() -> decltype(DRing::isDtmfMuted())
{
    return DRing::isDtmfMuted();
}

auto
isCaptureMuted() -> decltype(DRing::isCaptureMuted())
{
    return DRing::isCaptureMuted();
}

void
muteCapture(const bool& mute)
{
    DRing::muteCapture(mute);
}

auto
isPlaybackMuted() -> decltype(DRing::isPlaybackMuted())
{
    return DRing::isPlaybackMuted();
}

void
mutePlayback(const bool& mute)
{
    DRing::mutePlayback(mute);
}

auto
isRingtoneMuted() -> decltype(DRing::isRingtoneMuted())
{
    return DRing::isRingtoneMuted();
}

void
muteRingtone(const bool& mute)
{
    DRing::muteRingtone(mute);
}

auto
getAudioManager() -> decltype(DRing::getAudioManager())
{
    return DRing::getAudioManager();
}

auto
setAudioManager(const std::string& api) -> decltype(DRing::setAudioManager(api))
{
    return DRing::setAudioManager(api);
}

std::vector<std::string>
getSupportedAudioManagers()
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
getRecordPath() -> decltype(DRing::getRecordPath())
{
    return DRing::getRecordPath();
}

void
setRecordPath(const std::string& recPath)
{
    DRing::setRecordPath(recPath);
}

auto
getIsAlwaysRecording() -> decltype(DRing::getIsAlwaysRecording())
{
    return DRing::getIsAlwaysRecording();
}

void
setIsAlwaysRecording(const bool& rec)
{
    DRing::setIsAlwaysRecording(rec);
}

void
setHistoryLimit(const int32_t& days)
{
    DRing::setHistoryLimit(days);
}

auto
getHistoryLimit() -> decltype(DRing::getHistoryLimit())
{
    return DRing::getHistoryLimit();
}

void
setRingingTimeout(const int32_t& timeout)
{
    DRing::setRingingTimeout(timeout);
}

auto
getRingingTimeout() -> decltype(DRing::getRingingTimeout())
{
    return DRing::getRingingTimeout();
}

void
setAccountsOrder(const std::string& order)
{
    DRing::setAccountsOrder(order);
}

auto
getHookSettings() -> decltype(DRing::getHookSettings())
{
    return DRing::getHookSettings();
}

void
setHookSettings(const std::map<std::string, std::string>& settings)
{
    DRing::setHookSettings(settings);
}

auto
validateCertificate(const std::string& accountId, const std::string& certificate) -> decltype(DRing::validateCertificate(accountId, certificate))
{
   return DRing::validateCertificate(accountId, certificate);
}

auto
validateCertificatePath(const std::string& accountId, const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass, const std::string& caList) -> decltype(DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList))
{
   return DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList);
}

auto
getCertificateDetails(const std::string& certificate) -> decltype(DRing::getCertificateDetails(certificate))
{
    return DRing::getCertificateDetails(certificate);
}

auto
getCertificateDetailsPath(const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass) -> decltype(DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass))
{
    return DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass);
}

auto
getPinnedCertificates() -> decltype(DRing::getPinnedCertificates())
{
    return DRing::getPinnedCertificates();
}

auto
pinCertificate(const std::vector<uint8_t>& certificate, const bool& local) -> decltype(DRing::pinCertificate(certificate, local))
{
    return DRing::pinCertificate(certificate, local);
}

void
pinCertificatePath(const std::string& certPath)
{
    return DRing::pinCertificatePath(certPath);
}

auto
unpinCertificate(const std::string& certId) -> decltype(DRing::unpinCertificate(certId))
{
    return DRing::unpinCertificate(certId);
}

auto
unpinCertificatePath(const std::string& p) -> decltype(DRing::unpinCertificatePath(p))
{
    return DRing::unpinCertificatePath(p);
}

auto
pinRemoteCertificate(const std::string& accountId, const std::string& certId) -> decltype(DRing::pinRemoteCertificate(accountId, certId))
{
    return DRing::pinRemoteCertificate(accountId, certId);
}

auto
setCertificateStatus(const std::string& accountId, const std::string& certId, const std::string& status) -> decltype(DRing::setCertificateStatus(accountId, certId, status))
{
    return DRing::setCertificateStatus(accountId, certId, status);
}

auto
getCertificatesByStatus(const std::string& accountId, const std::string& status) -> decltype(DRing::getCertificatesByStatus(accountId, status))
{
    return DRing::getCertificatesByStatus(accountId, status);
}

auto
getTrustRequests(const std::string& accountId) -> decltype(DRing::getTrustRequests(accountId))
{
    return DRing::getTrustRequests(accountId);
}

auto
acceptTrustRequest(const std::string& accountId, const std::string& from) -> decltype(DRing::acceptTrustRequest(accountId, from))
{
    return DRing::acceptTrustRequest(accountId, from);
}

auto
discardTrustRequest(const std::string& accountId, const std::string& from) -> decltype(DRing::discardTrustRequest(accountId, from))
{
    return DRing::discardTrustRequest(accountId, from);
}

void
sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload)
{
    DRing::sendTrustRequest(accountId, to, payload);
}

void
addContact(const std::string& accountId, const std::string& uri)
{
    DRing::addContact(accountId, uri);
}

void
removeContact(const std::string& accountId, const std::string& uri, const bool& ban)
{
    DRing::removeContact(accountId, uri, ban);
}

auto
getContactDetails(const std::string& accountId, const std::string& uri) -> decltype(DRing::getContactDetails(accountId, uri))
{
    return DRing::getContactDetails(accountId, uri);
}

auto
getContacts(const std::string& accountId) -> decltype(DRing::getContacts(accountId))
{
    return DRing::getContacts(accountId);
}

auto
getCredentials(const std::string& accountID) -> decltype(DRing::getCredentials(accountID))
{
    return DRing::getCredentials(accountID);
}

void
setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details)
{
    DRing::setCredentials(accountID, details);
}

auto
getAddrFromInterfaceName(const std::string& interface) -> decltype(DRing::getAddrFromInterfaceName(interface))
{
    return DRing::getAddrFromInterfaceName(interface);
}

auto
getAllIpInterface() -> decltype(DRing::getAllIpInterface())
{
    return DRing::getAllIpInterface();
}

auto
getAllIpInterfaceByName() -> decltype(DRing::getAllIpInterfaceByName())
{
    return DRing::getAllIpInterfaceByName();
}

auto
getShortcuts() -> decltype(DRing::getShortcuts())
{
    return DRing::getShortcuts();
}

void
setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    DRing::setShortcuts(shortcutsMap);
}

void
setVolume(const std::string& device, const double& value)
{
    DRing::setVolume(device, value);
}

auto
getVolume(const std::string& device) -> decltype(DRing::getVolume(device))
{
    return DRing::getVolume(device);
}

auto
exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password) -> decltype(DRing::exportAccounts(accountIDs, filepath, password))
{
    return DRing::exportAccounts(accountIDs, filepath, password);
}

auto
importAccounts(const std::string& archivePath, const std::string& password) -> decltype(DRing::importAccounts(archivePath, password))
{
    return DRing::importAccounts(archivePath, password);
}

void
connectivityChanged()
{
    DRing::connectivityChanged();
}

bool
isAudioMeterActive(const std::string& id)
{
    return DRing::isAudioMeterActive(id);
}

void
setAudioMeterState(const std::string& id, const bool& state)
{
    return DRing::setAudioMeterState(id, state);
}
