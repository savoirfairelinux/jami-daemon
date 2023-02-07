/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
    -> decltype(libjami::getAccountDetails(accountID))
{
    return libjami::getAccountDetails(accountID);
}

auto
DBusConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
    -> decltype(libjami::getVolatileAccountDetails(accountID))
{
    return libjami::getVolatileAccountDetails(accountID);
}

void
DBusConfigurationManager::setAccountDetails(const std::string& accountID,
                                            const std::map<std::string, std::string>& details)
{
    libjami::setAccountDetails(accountID, details);
}

void
DBusConfigurationManager::setAccountActive(const std::string& accountID, const bool& active)
{
    libjami::setAccountActive(accountID, active);
}

auto
DBusConfigurationManager::getAccountTemplate(const std::string& accountType)
    -> decltype(libjami::getAccountTemplate(accountType))
{
    return libjami::getAccountTemplate(accountType);
}

auto
DBusConfigurationManager::addAccount(const std::map<std::string, std::string>& details)
    -> decltype(libjami::addAccount(details))
{
    return libjami::addAccount(details);
}

auto
DBusConfigurationManager::monitor(const bool& continuous) -> decltype(libjami::monitor(continuous))
{
    return libjami::monitor(continuous);
}

auto
DBusConfigurationManager::exportOnRing(const std::string& accountID, const std::string& password)
    -> decltype(libjami::exportOnRing(accountID, password))
{
    return libjami::exportOnRing(accountID, password);
}

auto
DBusConfigurationManager::exportToFile(const std::string& accountID,
                                       const std::string& destinationPath,
                                       const std::string& password)
    -> decltype(libjami::exportToFile(accountID, destinationPath, password))
{
    return libjami::exportToFile(accountID, destinationPath, password);
}

auto
DBusConfigurationManager::revokeDevice(const std::string& accountID,
                                       const std::string& password,
                                       const std::string& device)
    -> decltype(libjami::revokeDevice(accountID, password, device))
{
    return libjami::revokeDevice(accountID, password, device);
}

auto
DBusConfigurationManager::getKnownRingDevices(const std::string& accountID)
    -> decltype(libjami::getKnownRingDevices(accountID))
{
    return libjami::getKnownRingDevices(accountID);
}

auto
DBusConfigurationManager::changeAccountPassword(const std::string& accountID,
                                                const std::string& password_old,
                                                const std::string& password_new)
    -> decltype(libjami::changeAccountPassword(accountID, password_old, password_new))
{
    return libjami::changeAccountPassword(accountID, password_old, password_new);
}

auto
DBusConfigurationManager::lookupName(const std::string& account,
                                     const std::string& nameserver,
                                     const std::string& name)
    -> decltype(libjami::lookupName(account, nameserver, name))
{
    return libjami::lookupName(account, nameserver, name);
}

auto
DBusConfigurationManager::lookupAddress(const std::string& account,
                                        const std::string& nameserver,
                                        const std::string& address)
    -> decltype(libjami::lookupAddress(account, nameserver, address))
{
    return libjami::lookupAddress(account, nameserver, address);
}

auto
DBusConfigurationManager::registerName(const std::string& account,
                                       const std::string& password,
                                       const std::string& name)
    -> decltype(libjami::registerName(account, password, name))
{
    return libjami::registerName(account, password, name);
}

auto
DBusConfigurationManager::searchUser(const std::string& account, const std::string& query)
    -> decltype(libjami::searchUser(account, query))
{
    return libjami::searchUser(account, query);
}

void
DBusConfigurationManager::removeAccount(const std::string& accountID)
{
    libjami::removeAccount(accountID);
}

auto
DBusConfigurationManager::getAccountList() -> decltype(libjami::getAccountList())
{
    return libjami::getAccountList();
}

void
DBusConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
    libjami::sendRegister(accountID, enable);
}

void
DBusConfigurationManager::registerAllAccounts(void)
{
    libjami::registerAllAccounts();
}

auto
DBusConfigurationManager::sendTextMessage(const std::string& accountID,
                                          const std::string& to,
                                          const std::map<std::string, std::string>& payloads,
                                          const uint32_t& flags)
    -> decltype(libjami::sendAccountTextMessage(accountID, to, payloads, flags))
{
    return libjami::sendAccountTextMessage(accountID, to, payloads, flags);
}

std::vector<RingDBusMessage>
DBusConfigurationManager::getLastMessages(const std::string& accountID,
                                          const uint64_t& base_timestamp)
{
    auto messages = libjami::getLastMessages(accountID, base_timestamp);
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
    return libjami::getNearbyPeers(accountID);
}

auto
DBusConfigurationManager::getMessageStatus(const uint64_t& id)
    -> decltype(libjami::getMessageStatus(id))
{
    return libjami::getMessageStatus(id);
}

auto
DBusConfigurationManager::getMessageStatus(const std::string& accountID, const uint64_t& id)
    -> decltype(libjami::getMessageStatus(accountID, id))
{
    return libjami::getMessageStatus(accountID, id);
}

bool
DBusConfigurationManager::cancelMessage(const std::string& accountID, const uint64_t& id)
{
    return libjami::cancelMessage(accountID, id);
}

void
DBusConfigurationManager::setIsComposing(const std::string& accountID,
                                         const std::string& conversationUri,
                                         const bool& isWriting)
{
    libjami::setIsComposing(accountID, conversationUri, isWriting);
}

bool
DBusConfigurationManager::setMessageDisplayed(const std::string& accountID,
                                              const std::string& conversationUri,
                                              const std::string& messageId,
                                              const int32_t& status)
{
    return libjami::setMessageDisplayed(accountID, conversationUri, messageId, status);
}

auto
DBusConfigurationManager::getCodecList() -> decltype(libjami::getCodecList())
{
    return libjami::getCodecList();
}

auto
DBusConfigurationManager::getSupportedTlsMethod() -> decltype(libjami::getSupportedTlsMethod())
{
    return libjami::getSupportedTlsMethod();
}

auto
DBusConfigurationManager::getSupportedCiphers(const std::string& accountID)
    -> decltype(libjami::getSupportedCiphers(accountID))
{
    return libjami::getSupportedCiphers(accountID);
}

auto
DBusConfigurationManager::getCodecDetails(const std::string& accountID, const unsigned& codecId)
    -> decltype(libjami::getCodecDetails(accountID, codecId))
{
    return libjami::getCodecDetails(accountID, codecId);
}

auto
DBusConfigurationManager::setCodecDetails(const std::string& accountID,
                                          const unsigned& codecId,
                                          const std::map<std::string, std::string>& details)
    -> decltype(libjami::setCodecDetails(accountID, codecId, details))
{
    return libjami::setCodecDetails(accountID, codecId, details);
}

auto
DBusConfigurationManager::getActiveCodecList(const std::string& accountID)
    -> decltype(libjami::getActiveCodecList(accountID))
{
    return libjami::getActiveCodecList(accountID);
}

void
DBusConfigurationManager::setActiveCodecList(const std::string& accountID,
                                             const std::vector<unsigned>& list)
{
    libjami::setActiveCodecList(accountID, list);
}

auto
DBusConfigurationManager::getAudioPluginList() -> decltype(libjami::getAudioPluginList())
{
    return libjami::getAudioPluginList();
}

void
DBusConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    libjami::setAudioPlugin(audioPlugin);
}

auto
DBusConfigurationManager::getAudioOutputDeviceList()
    -> decltype(libjami::getAudioOutputDeviceList())
{
    return libjami::getAudioOutputDeviceList();
}

void
DBusConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    libjami::setAudioOutputDevice(index);
}

void
DBusConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    libjami::setAudioInputDevice(index);
}

void
DBusConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    libjami::setAudioRingtoneDevice(index);
}

auto
DBusConfigurationManager::getAudioInputDeviceList() -> decltype(libjami::getAudioInputDeviceList())
{
    return libjami::getAudioInputDeviceList();
}

auto
DBusConfigurationManager::getCurrentAudioDevicesIndex()
    -> decltype(libjami::getCurrentAudioDevicesIndex())
{
    return libjami::getCurrentAudioDevicesIndex();
}

auto
DBusConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
    -> decltype(libjami::getAudioInputDeviceIndex(name))
{
    return libjami::getAudioInputDeviceIndex(name);
}

auto
DBusConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
    -> decltype(libjami::getAudioOutputDeviceIndex(name))
{
    return libjami::getAudioOutputDeviceIndex(name);
}

auto
DBusConfigurationManager::getCurrentAudioOutputPlugin()
    -> decltype(libjami::getCurrentAudioOutputPlugin())
{
    return libjami::getCurrentAudioOutputPlugin();
}

auto
DBusConfigurationManager::getNoiseSuppressState() -> decltype(libjami::getNoiseSuppressState())
{
    return libjami::getNoiseSuppressState();
}

void
DBusConfigurationManager::setNoiseSuppressState(const std::string& state)
{
    libjami::setNoiseSuppressState(state);
}

auto
DBusConfigurationManager::isAgcEnabled() -> decltype(libjami::isAgcEnabled())
{
    return libjami::isAgcEnabled();
}

void
DBusConfigurationManager::setAgcState(const bool& enabled)
{
    libjami::setAgcState(enabled);
}

void
DBusConfigurationManager::muteDtmf(const bool& mute)
{
    libjami::muteDtmf(mute);
}

auto
DBusConfigurationManager::isDtmfMuted() -> decltype(libjami::isDtmfMuted())
{
    return libjami::isDtmfMuted();
}

auto
DBusConfigurationManager::isCaptureMuted() -> decltype(libjami::isCaptureMuted())
{
    return libjami::isCaptureMuted();
}

void
DBusConfigurationManager::muteCapture(const bool& mute)
{
    libjami::muteCapture(mute);
}

auto
DBusConfigurationManager::isPlaybackMuted() -> decltype(libjami::isPlaybackMuted())
{
    return libjami::isPlaybackMuted();
}

void
DBusConfigurationManager::mutePlayback(const bool& mute)
{
    libjami::mutePlayback(mute);
}

auto
DBusConfigurationManager::isRingtoneMuted() -> decltype(libjami::isRingtoneMuted())
{
    return libjami::isRingtoneMuted();
}

void
DBusConfigurationManager::muteRingtone(const bool& mute)
{
    libjami::muteRingtone(mute);
}

auto
DBusConfigurationManager::getAudioManager() -> decltype(libjami::getAudioManager())
{
    return libjami::getAudioManager();
}

auto
DBusConfigurationManager::setAudioManager(const std::string& api)
    -> decltype(libjami::setAudioManager(api))
{
    return libjami::setAudioManager(api);
}

auto
DBusConfigurationManager::getSupportedAudioManagers()
    -> decltype(libjami::getSupportedAudioManagers())
{
    return libjami::getSupportedAudioManagers();
}

auto
DBusConfigurationManager::getRecordPath() -> decltype(libjami::getRecordPath())
{
    return libjami::getRecordPath();
}

void
DBusConfigurationManager::setRecordPath(const std::string& recPath)
{
    libjami::setRecordPath(recPath);
}

auto
DBusConfigurationManager::getIsAlwaysRecording() -> decltype(libjami::getIsAlwaysRecording())
{
    return libjami::getIsAlwaysRecording();
}

void
DBusConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    libjami::setIsAlwaysRecording(rec);
}

auto
DBusConfigurationManager::getRecordPreview() -> decltype(libjami::getRecordPreview())
{
    return libjami::getRecordPreview();
}

void
DBusConfigurationManager::setRecordPreview(const bool& rec)
{
    libjami::setRecordPreview(rec);
}

auto
DBusConfigurationManager::getRecordQuality() -> decltype(libjami::getRecordQuality())
{
    return libjami::getRecordQuality();
}

void
DBusConfigurationManager::setRecordQuality(const int32_t& quality)
{
    libjami::setRecordQuality(quality);
}

void
DBusConfigurationManager::setHistoryLimit(const int32_t& days)
{
    libjami::setHistoryLimit(days);
}

auto
DBusConfigurationManager::getHistoryLimit() -> decltype(libjami::getHistoryLimit())
{
    return libjami::getHistoryLimit();
}

void
DBusConfigurationManager::setRingingTimeout(const int32_t& timeout)
{
    libjami::setRingingTimeout(timeout);
}

auto
DBusConfigurationManager::getRingingTimeout() -> decltype(libjami::getRingingTimeout())
{
    return libjami::getRingingTimeout();
}

void
DBusConfigurationManager::setAccountsOrder(const std::string& order)
{
    libjami::setAccountsOrder(order);
}

auto
DBusConfigurationManager::validateCertificate(const std::string& accountId,
                                              const std::string& certificate)
    -> decltype(libjami::validateCertificate(accountId, certificate))
{
    return libjami::validateCertificate(accountId, certificate);
}

auto
DBusConfigurationManager::validateCertificatePath(const std::string& accountId,
                                                  const std::string& certificate,
                                                  const std::string& privateKey,
                                                  const std::string& privateKeyPass,
                                                  const std::string& caList)
    -> decltype(libjami::validateCertificatePath(
        accountId, certificate, privateKey, privateKeyPass, caList))
{
    return libjami::validateCertificatePath(accountId,
                                            certificate,
                                            privateKey,
                                            privateKeyPass,
                                            caList);
}

auto
DBusConfigurationManager::getCertificateDetails(const std::string& certificate)
    -> decltype(libjami::getCertificateDetails(certificate))
{
    return libjami::getCertificateDetails(certificate);
}

auto
DBusConfigurationManager::getCertificateDetailsPath(const std::string& certificate,
                                                    const std::string& privateKey,
                                                    const std::string& privateKeyPass)
    -> decltype(libjami::getCertificateDetailsPath(certificate, privateKey, privateKeyPass))
{
    return libjami::getCertificateDetailsPath(certificate, privateKey, privateKeyPass);
}

auto
DBusConfigurationManager::getPinnedCertificates() -> decltype(libjami::getPinnedCertificates())
{
    return libjami::getPinnedCertificates();
}

auto
DBusConfigurationManager::pinCertificate(const std::vector<uint8_t>& certificate, const bool& local)
    -> decltype(libjami::pinCertificate(certificate, local))
{
    return libjami::pinCertificate(certificate, local);
}

void
DBusConfigurationManager::pinCertificatePath(const std::string& certPath)
{
    return libjami::pinCertificatePath(certPath);
}

auto
DBusConfigurationManager::unpinCertificate(const std::string& certId)
    -> decltype(libjami::unpinCertificate(certId))
{
    return libjami::unpinCertificate(certId);
}

auto
DBusConfigurationManager::unpinCertificatePath(const std::string& p)
    -> decltype(libjami::unpinCertificatePath(p))
{
    return libjami::unpinCertificatePath(p);
}

auto
DBusConfigurationManager::pinRemoteCertificate(const std::string& accountId,
                                               const std::string& certId)
    -> decltype(libjami::pinRemoteCertificate(accountId, certId))
{
    return libjami::pinRemoteCertificate(accountId, certId);
}

auto
DBusConfigurationManager::setCertificateStatus(const std::string& accountId,
                                               const std::string& certId,
                                               const std::string& status)
    -> decltype(libjami::setCertificateStatus(accountId, certId, status))
{
    return libjami::setCertificateStatus(accountId, certId, status);
}

auto
DBusConfigurationManager::getCertificatesByStatus(const std::string& accountId,
                                                  const std::string& status)
    -> decltype(libjami::getCertificatesByStatus(accountId, status))
{
    return libjami::getCertificatesByStatus(accountId, status);
}

auto
DBusConfigurationManager::getTrustRequests(const std::string& accountId)
    -> decltype(libjami::getTrustRequests(accountId))
{
    return libjami::getTrustRequests(accountId);
}

auto
DBusConfigurationManager::acceptTrustRequest(const std::string& accountId, const std::string& from)
    -> decltype(libjami::acceptTrustRequest(accountId, from))
{
    return libjami::acceptTrustRequest(accountId, from);
}

auto
DBusConfigurationManager::discardTrustRequest(const std::string& accountId, const std::string& from)
    -> decltype(libjami::discardTrustRequest(accountId, from))
{
    return libjami::discardTrustRequest(accountId, from);
}

void
DBusConfigurationManager::sendTrustRequest(const std::string& accountId,
                                           const std::string& to,
                                           const std::vector<uint8_t>& payload)
{
    libjami::sendTrustRequest(accountId, to, payload);
}

void
DBusConfigurationManager::addContact(const std::string& accountId, const std::string& uri)
{
    libjami::addContact(accountId, uri);
}

void
DBusConfigurationManager::removeContact(const std::string& accountId,
                                        const std::string& uri,
                                        const bool& ban)
{
    libjami::removeContact(accountId, uri, ban);
}

auto
DBusConfigurationManager::getContactDetails(const std::string& accountId, const std::string& uri)
    -> decltype(libjami::getContactDetails(accountId, uri))
{
    return libjami::getContactDetails(accountId, uri);
}

auto
DBusConfigurationManager::getContacts(const std::string& accountId)
    -> decltype(libjami::getContacts(accountId))
{
    return libjami::getContacts(accountId);
}

auto
DBusConfigurationManager::getCredentials(const std::string& accountID)
    -> decltype(libjami::getCredentials(accountID))
{
    return libjami::getCredentials(accountID);
}

void
DBusConfigurationManager::setCredentials(
    const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details)
{
    libjami::setCredentials(accountID, details);
}

auto
DBusConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
    -> decltype(libjami::getAddrFromInterfaceName(interface))
{
    return libjami::getAddrFromInterfaceName(interface);
}

auto
DBusConfigurationManager::getAllIpInterface() -> decltype(libjami::getAllIpInterface())
{
    return libjami::getAllIpInterface();
}

auto
DBusConfigurationManager::getAllIpInterfaceByName() -> decltype(libjami::getAllIpInterfaceByName())
{
    return libjami::getAllIpInterfaceByName();
}

void
DBusConfigurationManager::setVolume(const std::string& device, const double& value)
{
    libjami::setVolume(device, value);
}

auto
DBusConfigurationManager::getVolume(const std::string& device)
    -> decltype(libjami::getVolume(device))
{
    return libjami::getVolume(device);
}

void
DBusConfigurationManager::connectivityChanged()
{
    libjami::connectivityChanged();
}

void
DBusConfigurationManager::sendFile(const std::string& accountId,
                                   const std::string& conversationId,
                                   const std::string& path,
                                   const std::string& displayName,
                                   const std::string& replyTo)
{
    libjami::sendFile(accountId, conversationId, path, displayName, replyTo);
}

void
DBusConfigurationManager::fileTransferInfo(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& fileId,
                                           uint32_t& error,
                                           std::string& path,
                                           int64_t& total,
                                           int64_t& progress)
{
    error = uint32_t(
        libjami::fileTransferInfo(accountId, conversationId, fileId, path, total, progress));
}

bool
DBusConfigurationManager::downloadFile(const std::string& accountId,
                                       const std::string& conversationUri,
                                       const std::string& interactionId,
                                       const std::string& fileId,
                                       const std::string& path)
{
    return libjami::downloadFile(accountId, conversationUri, interactionId, fileId, path);
}

uint32_t
DBusConfigurationManager::cancelDataTransfer(const std::string& accountId,
                                             const std::string& conversationId,
                                             const std::string& fileId)
{
    return uint32_t(libjami::cancelDataTransfer(accountId, conversationId, fileId));
}

std::string
DBusConfigurationManager::startConversation(const std::string& accountId)
{
    return libjami::startConversation(accountId);
}

void
DBusConfigurationManager::acceptConversationRequest(const std::string& accountId,
                                                    const std::string& conversationId)
{
    libjami::acceptConversationRequest(accountId, conversationId);
}

void
DBusConfigurationManager::declineConversationRequest(const std::string& accountId,
                                                     const std::string& conversationId)
{
    libjami::declineConversationRequest(accountId, conversationId);
}

bool
DBusConfigurationManager::removeConversation(const std::string& accountId,
                                             const std::string& conversationId)
{
    return libjami::removeConversation(accountId, conversationId);
}

std::vector<std::string>
DBusConfigurationManager::getConversations(const std::string& accountId)
{
    return libjami::getConversations(accountId);
}

std::vector<std::map<std::string, std::string>>
DBusConfigurationManager::getActiveCalls(const std::string& accountId,
                                         const std::string& conversationId)
{
    return libjami::getActiveCalls(accountId, conversationId);
}

std::vector<std::map<std::string, std::string>>
DBusConfigurationManager::getConversationRequests(const std::string& accountId)
{
    return libjami::getConversationRequests(accountId);
}

void
DBusConfigurationManager::updateConversationInfos(const std::string& accountId,
                                                  const std::string& conversationId,
                                                  const std::map<std::string, std::string>& infos)
{
    libjami::updateConversationInfos(accountId, conversationId, infos);
}

std::map<std::string, std::string>
DBusConfigurationManager::conversationInfos(const std::string& accountId,
                                            const std::string& conversationId)
{
    return libjami::conversationInfos(accountId, conversationId);
}

void
DBusConfigurationManager::setConversationPreferences(const std::string& accountId,
                                                     const std::string& conversationId,
                                                     const std::map<std::string, std::string>& infos)
{
    libjami::setConversationPreferences(accountId, conversationId, infos);
}

std::map<std::string, std::string>
DBusConfigurationManager::getConversationPreferences(const std::string& accountId,
                                                     const std::string& conversationId)
{
    return libjami::getConversationPreferences(accountId, conversationId);
}

void
DBusConfigurationManager::addConversationMember(const std::string& accountId,
                                                const std::string& conversationId,
                                                const std::string& contactUri)
{
    libjami::addConversationMember(accountId, conversationId, contactUri);
}

void
DBusConfigurationManager::removeConversationMember(const std::string& accountId,
                                                   const std::string& conversationId,
                                                   const std::string& contactUri)
{
    libjami::removeConversationMember(accountId, conversationId, contactUri);
}

std::vector<std::map<std::string, std::string>>
DBusConfigurationManager::getConversationMembers(const std::string& accountId,
                                                 const std::string& conversationId)
{
    return libjami::getConversationMembers(accountId, conversationId);
}

void
DBusConfigurationManager::sendMessage(const std::string& accountId,
                                      const std::string& conversationId,
                                      const std::string& message,
                                      const std::string& replyTo,
                                      const int32_t& flag)
{
    libjami::sendMessage(accountId, conversationId, message, replyTo, flag);
}

uint32_t
DBusConfigurationManager::loadConversationMessages(const std::string& accountId,
                                                   const std::string& conversationId,
                                                   const std::string& fromMessage,
                                                   const uint32_t& n)
{
    return libjami::loadConversationMessages(accountId, conversationId, fromMessage, n);
}

uint32_t
DBusConfigurationManager::loadConversationUntil(const std::string& accountId,
                                                const std::string& conversationId,
                                                const std::string& fromMessage,
                                                const std::string& to)
{
    return libjami::loadConversationUntil(accountId, conversationId, fromMessage, to);
}

uint32_t
DBusConfigurationManager::countInteractions(const std::string& accountId,
                                            const std::string& conversationId,
                                            const std::string& toId,
                                            const std::string& fromId,
                                            const std::string& authorUri)
{
    return libjami::countInteractions(accountId, conversationId, toId, fromId, authorUri);
}

uint32_t
DBusConfigurationManager::searchConversation(const std::string& accountId,
                                             const std::string& conversationId,
                                             const std::string& author,
                                             const std::string& lastId,
                                             const std::string& regexSearch,
                                             const std::string& type,
                                             const int64_t& after,
                                             const int64_t& before,
                                             const uint32_t& maxResult,
                                             const int32_t& flag)
{
    return libjami::searchConversation(accountId,
                                       conversationId,
                                       author,
                                       lastId,
                                       regexSearch,
                                       type,
                                       after,
                                       before,
                                       maxResult,
                                       flag);
}

bool
DBusConfigurationManager::isAudioMeterActive(const std::string& id)
{
    return libjami::isAudioMeterActive(id);
}

void
DBusConfigurationManager::setAudioMeterState(const std::string& id, const bool& state)
{
    return libjami::setAudioMeterState(id, state);
}

void
DBusConfigurationManager::setDefaultModerator(const std::string& accountID,
                                              const std::string& peerURI,
                                              const bool& state)
{
    libjami::setDefaultModerator(accountID, peerURI, state);
}

auto
DBusConfigurationManager::getDefaultModerators(const std::string& accountID)
    -> decltype(libjami::getDefaultModerators(accountID))
{
    return libjami::getDefaultModerators(accountID);
}

void
DBusConfigurationManager::enableLocalModerators(const std::string& accountID,
                                                const bool& isModEnabled)
{
    return libjami::enableLocalModerators(accountID, isModEnabled);
}

bool
DBusConfigurationManager::isLocalModeratorsEnabled(const std::string& accountID)
{
    return libjami::isLocalModeratorsEnabled(accountID);
}

void
DBusConfigurationManager::setAllModerators(const std::string& accountID, const bool& allModerators)
{
    return libjami::setAllModerators(accountID, allModerators);
}

bool
DBusConfigurationManager::isAllModerators(const std::string& accountID)
{
    return libjami::isAllModerators(accountID);
}