/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "dbusconfigurationmanager.h"
#include "configurationmanager_interface.h"
#include "datatransfer_interface.h"
#include "conversation_interface.h"

DBusConfigurationManager::DBusConfigurationManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/ConfigurationManager")
{}

auto
DBusConfigurationManager::getAccountDetails(const std::string& accountID)
    -> decltype(DRing::getAccountDetails(accountID))
{
    return DRing::getAccountDetails(accountID);
}

auto
DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
    -> decltype(DRing::getVolatileAccountDetails(accountID))
{
    return DRing::getVolatileAccountDetails(accountID);
}

void
DBusConfigurationManager::setAccountDetails(const std::string& accountID,
                                            const std::map<std::string, std::string>& details)
{
    DRing::setAccountDetails(accountID, details);
}

void
DBusConfigurationManager::setAccountActive(const std::string& accountID, const bool& active)
{
    DRing::setAccountActive(accountID, active);
}

auto
DBusConfigurationManager::getAccountTemplate(const std::string& accountType)
    -> decltype(DRing::getAccountTemplate(accountType))
{
    return DRing::getAccountTemplate(accountType);
}

auto
DBusConfigurationManager::addAccount(const std::map<std::string, std::string>& details)
    -> decltype(DRing::addAccount(details))
{
    return DRing::addAccount(details);
}

auto
DBusConfigurationManager::monitor(const bool& continuous) -> decltype(DRing::monitor(continuous))
{
    return DRing::monitor(continuous);
}

auto
DBusConfigurationManager::exportOnRing(const std::string& accountID, const std::string& password)
    -> decltype(DRing::exportOnRing(accountID, password))
{
    return DRing::exportOnRing(accountID, password);
}

auto
DBusConfigurationManager::exportToFile(const std::string& accountID,
                                       const std::string& destinationPath,
                                       const std::string& password)
    -> decltype(DRing::exportToFile(accountID, destinationPath, password))
{
    return DRing::exportToFile(accountID, destinationPath, password);
}

auto
DBusConfigurationManager::revokeDevice(const std::string& accountID,
                                       const std::string& password,
                                       const std::string& device)
    -> decltype(DRing::revokeDevice(accountID, password, device))
{
    return DRing::revokeDevice(accountID, password, device);
}

auto
DBusConfigurationManager::getKnownRingDevices(const std::string& accountID)
    -> decltype(DRing::getKnownRingDevices(accountID))
{
    return DRing::getKnownRingDevices(accountID);
}

auto
DBusConfigurationManager::changeAccountPassword(const std::string& accountID,
                                                const std::string& password_old,
                                                const std::string& password_new)
    -> decltype(DRing::changeAccountPassword(accountID, password_old, password_new))
{
    return DRing::changeAccountPassword(accountID, password_old, password_new);
}

auto
DBusConfigurationManager::lookupName(const std::string& account,
                                     const std::string& nameserver,
                                     const std::string& name)
    -> decltype(DRing::lookupName(account, nameserver, name))
{
    return DRing::lookupName(account, nameserver, name);
}

auto
DBusConfigurationManager::lookupAddress(const std::string& account,
                                        const std::string& nameserver,
                                        const std::string& address)
    -> decltype(DRing::lookupAddress(account, nameserver, address))
{
    return DRing::lookupAddress(account, nameserver, address);
}

auto
DBusConfigurationManager::registerName(const std::string& account,
                                       const std::string& password,
                                       const std::string& name)
    -> decltype(DRing::registerName(account, password, name))
{
    return DRing::registerName(account, password, name);
}

auto
DBusConfigurationManager::searchUser(const std::string& account, const std::string& query)
    -> decltype(DRing::searchUser(account, query))
{
    return DRing::searchUser(account, query);
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
DBusConfigurationManager::sendTextMessage(const std::string& accountID,
                                          const std::string& to,
                                          const std::map<std::string, std::string>& payloads)
    -> decltype(DRing::sendAccountTextMessage(accountID, to, payloads))
{
    return DRing::sendAccountTextMessage(accountID, to, payloads);
}

std::vector<RingDBusMessage>
DBusConfigurationManager::getLastMessages(const std::string& accountID,
                                          const uint64_t& base_timestamp)
{
    auto messages = DRing::getLastMessages(accountID, base_timestamp);
    std::vector<RingDBusMessage> result;
    for (const auto& message : messages) {
        RingDBusMessage m;
        m._1 = message.from;
        m._2 = message.payloads;
        m._3 = message.received;
        result.emplace_back(m);
    }
    return result;
}

std::map<std::string, std::string>
DBusConfigurationManager::getNearbyPeers(const std::string& accountID)
{
    return DRing::getNearbyPeers(accountID);
}

auto
DBusConfigurationManager::getMessageStatus(const uint64_t& id)
    -> decltype(DRing::getMessageStatus(id))
{
    return DRing::getMessageStatus(id);
}

auto
DBusConfigurationManager::getMessageStatus(const std::string& accountID, const uint64_t& id)
    -> decltype(DRing::getMessageStatus(accountID, id))
{
    return DRing::getMessageStatus(accountID, id);
}

bool
DBusConfigurationManager::cancelMessage(const std::string& accountID, const uint64_t& id)
{
    return DRing::cancelMessage(accountID, id);
}

void
DBusConfigurationManager::setIsComposing(const std::string& accountID,
                                         const std::string& conversationUri,
                                         const bool& isWriting)
{
    DRing::setIsComposing(accountID, conversationUri, isWriting);
}

bool
DBusConfigurationManager::setMessageDisplayed(const std::string& accountID,
                                              const std::string& conversationUri,
                                              const std::string& messageId,
                                              const int32_t& status)
{
    return DRing::setMessageDisplayed(accountID, conversationUri, messageId, status);
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
DBusConfigurationManager::getSupportedCiphers(const std::string& accountID)
    -> decltype(DRing::getSupportedCiphers(accountID))
{
    return DRing::getSupportedCiphers(accountID);
}

auto
DBusConfigurationManager::getCodecDetails(const std::string& accountID, const unsigned& codecId)
    -> decltype(DRing::getCodecDetails(accountID, codecId))
{
    return DRing::getCodecDetails(accountID, codecId);
}

auto
DBusConfigurationManager::setCodecDetails(const std::string& accountID,
                                          const unsigned& codecId,
                                          const std::map<std::string, std::string>& details)
    -> decltype(DRing::setCodecDetails(accountID, codecId, details))
{
    return DRing::setCodecDetails(accountID, codecId, details);
}

auto
DBusConfigurationManager::getActiveCodecList(const std::string& accountID)
    -> decltype(DRing::getActiveCodecList(accountID))
{
    return DRing::getActiveCodecList(accountID);
}

void
DBusConfigurationManager::setActiveCodecList(const std::string& accountID,
                                             const std::vector<unsigned>& list)
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
DBusConfigurationManager::getCurrentAudioDevicesIndex()
    -> decltype(DRing::getCurrentAudioDevicesIndex())
{
    return DRing::getCurrentAudioDevicesIndex();
}

auto
DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
    -> decltype(DRing::getAudioInputDeviceIndex(name))
{
    return DRing::getAudioInputDeviceIndex(name);
}

auto
DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
    -> decltype(DRing::getAudioOutputDeviceIndex(name))
{
    return DRing::getAudioOutputDeviceIndex(name);
}

auto
DBusConfigurationManager::getCurrentAudioOutputPlugin()
    -> decltype(DRing::getCurrentAudioOutputPlugin())
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
DBusConfigurationManager::setAudioManager(const std::string& api)
    -> decltype(DRing::setAudioManager(api))
{
    return DRing::setAudioManager(api);
}

auto
DBusConfigurationManager::getSupportedAudioManagers()
    -> decltype(DRing::getSupportedAudioManagers())
{
    return DRing::getSupportedAudioManagers();
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

auto
DBusConfigurationManager::getRecordPreview() -> decltype(DRing::getRecordPreview())
{
    return DRing::getRecordPreview();
}

void
DBusConfigurationManager::setRecordPreview(const bool& rec)
{
    DRing::setRecordPreview(rec);
}

auto
DBusConfigurationManager::getRecordQuality() -> decltype(DRing::getRecordQuality())
{
    return DRing::getRecordQuality();
}

void
DBusConfigurationManager::setRecordQuality(const int32_t& quality)
{
    DRing::setRecordQuality(quality);
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
DBusConfigurationManager::setRingingTimeout(const int32_t& timeout)
{
    DRing::setRingingTimeout(timeout);
}

auto
DBusConfigurationManager::getRingingTimeout() -> decltype(DRing::getRingingTimeout())
{
    return DRing::getRingingTimeout();
}

void
DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    DRing::setAccountsOrder(order);
}

auto
DBusConfigurationManager::validateCertificate(const std::string& accountId,
                                              const std::string& certificate)
    -> decltype(DRing::validateCertificate(accountId, certificate))
{
    return DRing::validateCertificate(accountId, certificate);
}

auto
DBusConfigurationManager::validateCertificatePath(const std::string& accountId,
                                                  const std::string& certificate,
                                                  const std::string& privateKey,
                                                  const std::string& privateKeyPass,
                                                  const std::string& caList)
    -> decltype(
        DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList))
{
    return DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList);
}

auto
DBusConfigurationManager::getCertificateDetails(const std::string& certificate)
    -> decltype(DRing::getCertificateDetails(certificate))
{
    return DRing::getCertificateDetails(certificate);
}

auto
DBusConfigurationManager::getCertificateDetailsPath(const std::string& certificate,
                                                    const std::string& privateKey,
                                                    const std::string& privateKeyPass)
    -> decltype(DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass))
{
    return DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass);
}

auto
DBusConfigurationManager::getPinnedCertificates() -> decltype(DRing::getPinnedCertificates())
{
    return DRing::getPinnedCertificates();
}

auto
DBusConfigurationManager::pinCertificate(const std::vector<uint8_t>& certificate, const bool& local)
    -> decltype(DRing::pinCertificate(certificate, local))
{
    return DRing::pinCertificate(certificate, local);
}

void
DBusConfigurationManager::pinCertificatePath(const std::string& certPath)
{
    return DRing::pinCertificatePath(certPath);
}

auto
DBusConfigurationManager::unpinCertificate(const std::string& certId)
    -> decltype(DRing::unpinCertificate(certId))
{
    return DRing::unpinCertificate(certId);
}

auto
DBusConfigurationManager::unpinCertificatePath(const std::string& p)
    -> decltype(DRing::unpinCertificatePath(p))
{
    return DRing::unpinCertificatePath(p);
}

auto
DBusConfigurationManager::pinRemoteCertificate(const std::string& accountId,
                                               const std::string& certId)
    -> decltype(DRing::pinRemoteCertificate(accountId, certId))
{
    return DRing::pinRemoteCertificate(accountId, certId);
}

auto
DBusConfigurationManager::setCertificateStatus(const std::string& accountId,
                                               const std::string& certId,
                                               const std::string& status)
    -> decltype(DRing::setCertificateStatus(accountId, certId, status))
{
    return DRing::setCertificateStatus(accountId, certId, status);
}

auto
DBusConfigurationManager::getCertificatesByStatus(const std::string& accountId,
                                                  const std::string& status)
    -> decltype(DRing::getCertificatesByStatus(accountId, status))
{
    return DRing::getCertificatesByStatus(accountId, status);
}

auto
DBusConfigurationManager::getTrustRequests(const std::string& accountId)
    -> decltype(DRing::getTrustRequests(accountId))
{
    return DRing::getTrustRequests(accountId);
}

auto
DBusConfigurationManager::acceptTrustRequest(const std::string& accountId, const std::string& from)
    -> decltype(DRing::acceptTrustRequest(accountId, from))
{
    return DRing::acceptTrustRequest(accountId, from);
}

auto
DBusConfigurationManager::discardTrustRequest(const std::string& accountId, const std::string& from)
    -> decltype(DRing::discardTrustRequest(accountId, from))
{
    return DRing::discardTrustRequest(accountId, from);
}

void
DBusConfigurationManager::sendTrustRequest(const std::string& accountId,
                                           const std::string& to,
                                           const std::vector<uint8_t>& payload)
{
    DRing::sendTrustRequest(accountId, to, payload);
}

void
DBusConfigurationManager::addContact(const std::string& accountId, const std::string& uri)
{
    DRing::addContact(accountId, uri);
}

void
DBusConfigurationManager::removeContact(const std::string& accountId,
                                        const std::string& uri,
                                        const bool& ban)
{
    DRing::removeContact(accountId, uri, ban);
}

auto
DBusConfigurationManager::getContactDetails(const std::string& accountId, const std::string& uri)
    -> decltype(DRing::getContactDetails(accountId, uri))
{
    return DRing::getContactDetails(accountId, uri);
}

auto
DBusConfigurationManager::getContacts(const std::string& accountId)
    -> decltype(DRing::getContacts(accountId))
{
    return DRing::getContacts(accountId);
}

auto
DBusConfigurationManager::getCredentials(const std::string& accountID)
    -> decltype(DRing::getCredentials(accountID))
{
    return DRing::getCredentials(accountID);
}

void
DBusConfigurationManager::setCredentials(
    const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details)
{
    DRing::setCredentials(accountID, details);
}

auto
DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
    -> decltype(DRing::getAddrFromInterfaceName(interface))
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
DBusConfigurationManager::setShortcuts(const std::map<std::string, std::string>& shortcutsMap)
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

void
DBusConfigurationManager::connectivityChanged()
{
    DRing::connectivityChanged();
}

void
DBusConfigurationManager::sendFile(const RingDBusDataTransferInfo& in,
                                   uint32_t& error,
                                   DRing::DataTransferId& id)
{
    DRing::DataTransferInfo info;
    info.accountId = in._1;
    info.lastEvent = DRing::DataTransferEventCode(in._2);
    info.flags = in._3;
    info.totalSize = in._4;
    info.bytesProgress = in._5;
    info.author = in._6;
    info.peer = in._7;
    info.conversationId = in._8;
    info.displayName = in._9;
    info.path = in._10;
    info.mimetype = in._11;
    error = uint32_t(DRing::sendFile(info, id));
}

void
DBusConfigurationManager::dataTransferInfo(const std::string& accountId,
                                           const std::string& conversationId,
                                           const DRing::DataTransferId& id,
                                           uint32_t& error,
                                           RingDBusDataTransferInfo& out)
{
    DRing::DataTransferInfo info;
    auto res = DRing::dataTransferInfo(accountId, conversationId, id, info);
    if (res == DRing::DataTransferError::success) {
        out._1 = info.accountId;
        out._2 = uint32_t(info.lastEvent);
        out._3 = info.flags;
        out._4 = info.totalSize;
        out._5 = info.bytesProgress;
        out._6 = info.author;
        out._7 = info.peer;
        out._8 = info.conversationId;
        out._9 = info.displayName;
        out._10 = info.path;
        out._11 = info.mimetype;
    }
    error = uint32_t(res);
}

void
DBusConfigurationManager::dataTransferBytesProgress(const std::string& accountId,
                                                    const std::string& conversationId,
                                                    const uint64_t& id,
                                                    uint32_t& error,
                                                    int64_t& total,
                                                    int64_t& progress)
{
    error = uint32_t(
        DRing::dataTransferBytesProgress(accountId, conversationId, id, total, progress));
}

uint32_t
DBusConfigurationManager::acceptFileTransfer(const std::string& accountId,
                                             const std::string& conversationId,
                                             const uint64_t& id,
                                             const std::string& file_path,
                                             const int64_t& offset)
{
    return uint32_t(DRing::acceptFileTransfer(accountId, conversationId, id, file_path, offset));
}

uint64_t
DBusConfigurationManager::downloadFile(const std::string& accountId,
                                       const std::string& conversationUri,
                                       const std::string& interactionId,
                                       const std::string& path)
{
    return DRing::downloadFile(accountId, conversationUri, interactionId, path);
}

uint32_t
DBusConfigurationManager::cancelDataTransfer(const std::string& accountId,
                                             const std::string& conversationId,
                                             const uint64_t& id)
{
    return uint32_t(DRing::cancelDataTransfer(accountId, conversationId, id));
}

std::string
DBusConfigurationManager::startConversation(const std::string& accountId)
{
    return DRing::startConversation(accountId);
}

void
DBusConfigurationManager::acceptConversationRequest(const std::string& accountId,
                                                    const std::string& conversationId)
{
    DRing::acceptConversationRequest(accountId, conversationId);
}

void
DBusConfigurationManager::declineConversationRequest(const std::string& accountId,
                                                     const std::string& conversationId)
{
    DRing::declineConversationRequest(accountId, conversationId);
}

bool
DBusConfigurationManager::removeConversation(const std::string& accountId,
                                             const std::string& conversationId)
{
    return DRing::removeConversation(accountId, conversationId);
}

std::vector<std::string>
DBusConfigurationManager::getConversations(const std::string& accountId)
{
    return DRing::getConversations(accountId);
}

std::vector<std::map<std::string, std::string>>
DBusConfigurationManager::getConversationRequests(const std::string& accountId)
{
    return DRing::getConversationRequests(accountId);
}

void
DBusConfigurationManager::updateConversationInfos(const std::string& accountId,
                                                  const std::string& conversationId,
                                                  const std::map<std::string, std::string>& infos)
{
    DRing::updateConversationInfos(accountId, conversationId, infos);
}

std::map<std::string, std::string>
DBusConfigurationManager::conversationInfos(const std::string& accountId,
                                            const std::string& conversationId)
{
    return DRing::conversationInfos(accountId, conversationId);
}

void
DBusConfigurationManager::addConversationMember(const std::string& accountId,
                                                const std::string& conversationId,
                                                const std::string& contactUri)
{
    DRing::addConversationMember(accountId, conversationId, contactUri);
}

void
DBusConfigurationManager::removeConversationMember(const std::string& accountId,
                                                   const std::string& conversationId,
                                                   const std::string& contactUri)
{
    DRing::removeConversationMember(accountId, conversationId, contactUri);
}

std::vector<std::map<std::string, std::string>>
DBusConfigurationManager::getConversationMembers(const std::string& accountId,
                                                 const std::string& conversationId)
{
    return DRing::getConversationMembers(accountId, conversationId);
}

void
DBusConfigurationManager::sendMessage(const std::string& accountId,
                                      const std::string& conversationId,
                                      const std::string& message,
                                      const std::string& parent)
{
    DRing::sendMessage(accountId, conversationId, message, parent);
}

uint32_t
DBusConfigurationManager::loadConversationMessages(const std::string& accountId,
                                                   const std::string& conversationId,
                                                   const std::string& fromMessage,
                                                   const uint32_t& n)
{
    return DRing::loadConversationMessages(accountId, conversationId, fromMessage, n);
}

bool
DBusConfigurationManager::isAudioMeterActive(const std::string& id)
{
    return DRing::isAudioMeterActive(id);
}

void
DBusConfigurationManager::setAudioMeterState(const std::string& id, const bool& state)
{
    return DRing::setAudioMeterState(id, state);
}

void
DBusConfigurationManager::setDefaultModerator(const std::string& accountID,
                                              const std::string& peerURI,
                                              const bool& state)
{
    DRing::setDefaultModerator(accountID, peerURI, state);
}

auto
DBusConfigurationManager::getDefaultModerators(const std::string& accountID)
    -> decltype(DRing::getDefaultModerators(accountID))
{
    return DRing::getDefaultModerators(accountID);
}

void
DBusConfigurationManager::enableLocalModerators(const std::string& accountID,
                                                const bool& isModEnabled)
{
    return DRing::enableLocalModerators(accountID, isModEnabled);
}

bool
DBusConfigurationManager::isLocalModeratorsEnabled(const std::string& accountID)
{
    return DRing::isLocalModeratorsEnabled(accountID);
}

void
DBusConfigurationManager::setAllModerators(const std::string& accountID, const bool& allModerators)
{
    return DRing::setAllModerators(accountID, allModerators);
}

bool
DBusConfigurationManager::isAllModerators(const std::string& accountID)
{
    return DRing::isAllModerators(accountID);
}