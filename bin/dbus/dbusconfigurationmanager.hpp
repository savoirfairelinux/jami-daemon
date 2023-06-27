/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#pragma once

#include "dbusconfigurationmanager.adaptor.h"
#include <configurationmanager_interface.h>
#include <datatransfer_interface.h>
#include <conversation_interface.h>

class DBusConfigurationManager : public sdbus::AdaptorInterfaces<cx::ring::Ring::ConfigurationManager_adaptor>
{
public:
    using DBusSwarmMessage = sdbus::Struct<std::string, std::string, std::string, std::map<std::string, std::string>, std::vector<std::map<std::string, std::string>>, std::vector<std::map<std::string, std::string>>>;
    DBusConfigurationManager(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, "/cx/ring/Ring/ConfigurationManager")
    {
        registerAdaptor();
        registerSignalHandlers();
    }

    ~DBusConfigurationManager()
    {
        unregisterAdaptor();
    }

    auto
    getAccountDetails(const std::string& accountID)
        -> decltype(libjami::getAccountDetails(accountID))
    {
        return libjami::getAccountDetails(accountID);
    }

    auto
    getVolatileAccountDetails(const std::string& accountID)
        -> decltype(libjami::getVolatileAccountDetails(accountID))
    {
        return libjami::getVolatileAccountDetails(accountID);
    }

    void
    setAccountDetails(const std::string& accountID,
                      const std::map<std::string, std::string>& details)
    {
        libjami::setAccountDetails(accountID, details);
    }

    void
    setAccountActive(const std::string& accountID, const bool& active)
    {
        libjami::setAccountActive(accountID, active);
    }

    auto
    getAccountTemplate(const std::string& accountType)
        -> decltype(libjami::getAccountTemplate(accountType))
    {
        return libjami::getAccountTemplate(accountType);
    }

    auto
    addAccount(const std::map<std::string, std::string>& details)
        -> decltype(libjami::addAccount(details))
    {
        return libjami::addAccount(details);
    }

    auto
    monitor(const bool& continuous) -> decltype(libjami::monitor(continuous))
    {
        return libjami::monitor(continuous);
    }

    auto
    exportOnRing(const std::string& accountID, const std::string& password)
        -> decltype(libjami::exportOnRing(accountID, password))
    {
        return libjami::exportOnRing(accountID, password);
    }

    auto
    exportToFile(const std::string& accountID,
                 const std::string& destinationPath,
                 const std::string& password)
        -> decltype(libjami::exportToFile(accountID, destinationPath, password))
    {
        return libjami::exportToFile(accountID, destinationPath, password);
    }

    auto
    revokeDevice(const std::string& accountID,
                 const std::string& password,
                 const std::string& device)
        -> decltype(libjami::revokeDevice(accountID, password, device))
    {
        return libjami::revokeDevice(accountID, password, device);
    }

    auto
    getKnownRingDevices(const std::string& accountID)
        -> decltype(libjami::getKnownRingDevices(accountID))
    {
        return libjami::getKnownRingDevices(accountID);
    }

    auto
    changeAccountPassword(const std::string& accountID,
                          const std::string& password_old,
                          const std::string& password_new)
        -> decltype(libjami::changeAccountPassword(accountID, password_old, password_new))
    {
        return libjami::changeAccountPassword(accountID, password_old, password_new);
    }

    auto
    lookupName(const std::string& account,
               const std::string& nameserver,
               const std::string& name)
        -> decltype(libjami::lookupName(account, nameserver, name))
    {
        return libjami::lookupName(account, nameserver, name);
    }

    auto
    lookupAddress(const std::string& account,
                  const std::string& nameserver,
                  const std::string& address)
        -> decltype(libjami::lookupAddress(account, nameserver, address))
    {
        return libjami::lookupAddress(account, nameserver, address);
    }

    auto
    registerName(const std::string& account,
                 const std::string& password,
                 const std::string& name)
        -> decltype(libjami::registerName(account, password, name))
    {
        return libjami::registerName(account, password, name);
    }

    auto
    searchUser(const std::string& account, const std::string& query)
        -> decltype(libjami::searchUser(account, query))
    {
        return libjami::searchUser(account, query);
    }

    void
    removeAccount(const std::string& accountID)
    {
        libjami::removeAccount(accountID);
    }

    auto
    getAccountList() -> decltype(libjami::getAccountList())
    {
        return libjami::getAccountList();
    }

    void
    sendRegister(const std::string& accountID, const bool& enable)
    {
        libjami::sendRegister(accountID, enable);
    }

    void
    registerAllAccounts(void)
    {
        libjami::registerAllAccounts();
    }

    auto
    sendTextMessage(const std::string& accountID,
                    const std::string& to,
                    const std::map<std::string, std::string>& payloads,
                    const int32_t& flags)
        -> decltype(libjami::sendAccountTextMessage(accountID, to, payloads, flags))
    {
        return libjami::sendAccountTextMessage(accountID, to, payloads, flags);
    }

    std::vector<sdbus::Struct<std::string, std::map<std::string, std::string>, uint64_t>>
    getLastMessages(const std::string& accountID,
                    const uint64_t& base_timestamp)
    {
        auto messages = libjami::getLastMessages(accountID, base_timestamp);
        std::vector<sdbus::Struct<std::string, std::map<std::string, std::string>, uint64_t>> result;
        for (const auto& message : messages) {
            sdbus::Struct<std::string, std::map<std::string, std::string>, uint64_t> m(
                message.from, message.payloads, message.received );
            result.emplace_back(m);
        }
        return result;
    }

    std::map<std::string, std::string>
    getNearbyPeers(const std::string& accountID)
    {
        return libjami::getNearbyPeers(accountID);
    }

    auto
    getMessageStatus(const uint64_t& id)
        -> decltype(libjami::getMessageStatus(id))
    {
        return libjami::getMessageStatus(id);
    }

    auto
    getMessageStatus(const std::string& accountID, const uint64_t& id)
        -> decltype(libjami::getMessageStatus(accountID, id))
    {
        return libjami::getMessageStatus(accountID, id);
    }

    bool
    cancelMessage(const std::string& accountID, const uint64_t& id)
    {
        return libjami::cancelMessage(accountID, id);
    }

    void
    setIsComposing(const std::string& accountID,
                   const std::string& conversationUri,
                   const bool& isWriting)
    {
        libjami::setIsComposing(accountID, conversationUri, isWriting);
    }

    bool
    setMessageDisplayed(const std::string& accountID,
                        const std::string& conversationUri,
                        const std::string& messageId,
                        const int32_t& status)
    {
        return libjami::setMessageDisplayed(accountID, conversationUri, messageId, status);
    }

    auto
    getCodecList() -> decltype(libjami::getCodecList())
    {
        return libjami::getCodecList();
    }

    auto
    getSupportedTlsMethod() -> decltype(libjami::getSupportedTlsMethod())
    {
        return libjami::getSupportedTlsMethod();
    }

    auto
    getSupportedCiphers(const std::string& accountID)
        -> decltype(libjami::getSupportedCiphers(accountID))
    {
        return libjami::getSupportedCiphers(accountID);
    }

    auto
    getCodecDetails(const std::string& accountID, const unsigned& codecId)
        -> decltype(libjami::getCodecDetails(accountID, codecId))
    {
        return libjami::getCodecDetails(accountID, codecId);
    }

    auto
    setCodecDetails(const std::string& accountID,
                    const unsigned& codecId,
                    const std::map<std::string, std::string>& details)
        -> decltype(libjami::setCodecDetails(accountID, codecId, details))
    {
        return libjami::setCodecDetails(accountID, codecId, details);
    }

    auto
    getActiveCodecList(const std::string& accountID)
        -> decltype(libjami::getActiveCodecList(accountID))
    {
        return libjami::getActiveCodecList(accountID);
    }

    void
    setActiveCodecList(const std::string& accountID,
                       const std::vector<unsigned>& list)
    {
        libjami::setActiveCodecList(accountID, list);
    }

    auto
    getAudioPluginList() -> decltype(libjami::getAudioPluginList())
    {
        return libjami::getAudioPluginList();
    }

    void
    setAudioPlugin(const std::string& audioPlugin)
    {
        libjami::setAudioPlugin(audioPlugin);
    }

    auto
    getAudioOutputDeviceList()
        -> decltype(libjami::getAudioOutputDeviceList())
    {
        return libjami::getAudioOutputDeviceList();
    }

    void
    setAudioOutputDevice(const int32_t& index)
    {
        libjami::setAudioOutputDevice(index);
    }

    void
    setAudioInputDevice(const int32_t& index)
    {
        libjami::setAudioInputDevice(index);
    }

    void
    setAudioRingtoneDevice(const int32_t& index)
    {
        libjami::setAudioRingtoneDevice(index);
    }

    auto
    getAudioInputDeviceList() -> decltype(libjami::getAudioInputDeviceList())
    {
        return libjami::getAudioInputDeviceList();
    }

    auto
    getCurrentAudioDevicesIndex()
        -> decltype(libjami::getCurrentAudioDevicesIndex())
    {
        return libjami::getCurrentAudioDevicesIndex();
    }

    auto
    getAudioInputDeviceIndex(const std::string& name)
        -> decltype(libjami::getAudioInputDeviceIndex(name))
    {
        return libjami::getAudioInputDeviceIndex(name);
    }

    auto
    getAudioOutputDeviceIndex(const std::string& name)
        -> decltype(libjami::getAudioOutputDeviceIndex(name))
    {
        return libjami::getAudioOutputDeviceIndex(name);
    }

    auto
    getCurrentAudioOutputPlugin()
        -> decltype(libjami::getCurrentAudioOutputPlugin())
    {
        return libjami::getCurrentAudioOutputPlugin();
    }

    auto
    getNoiseSuppressState() -> decltype(libjami::getNoiseSuppressState())
    {
        return libjami::getNoiseSuppressState();
    }

    void
    setNoiseSuppressState(const std::string& state)
    {
        libjami::setNoiseSuppressState(state);
    }

    auto
    isAgcEnabled() -> decltype(libjami::isAgcEnabled())
    {
        return libjami::isAgcEnabled();
    }

    void
    setAgcState(const bool& enabled)
    {
        libjami::setAgcState(enabled);
    }

    void
    muteDtmf(const bool& mute)
    {
        libjami::muteDtmf(mute);
    }

    auto
    isDtmfMuted() -> decltype(libjami::isDtmfMuted())
    {
        return libjami::isDtmfMuted();
    }

    auto
    isCaptureMuted() -> decltype(libjami::isCaptureMuted())
    {
        return libjami::isCaptureMuted();
    }

    void
    muteCapture(const bool& mute)
    {
        libjami::muteCapture(mute);
    }

    auto
    isPlaybackMuted() -> decltype(libjami::isPlaybackMuted())
    {
        return libjami::isPlaybackMuted();
    }

    void
    mutePlayback(const bool& mute)
    {
        libjami::mutePlayback(mute);
    }

    auto
    isRingtoneMuted() -> decltype(libjami::isRingtoneMuted())
    {
        return libjami::isRingtoneMuted();
    }

    void
    muteRingtone(const bool& mute)
    {
        libjami::muteRingtone(mute);
    }

    auto
    getAudioManager() -> decltype(libjami::getAudioManager())
    {
        return libjami::getAudioManager();
    }

    auto
    setAudioManager(const std::string& api)
        -> decltype(libjami::setAudioManager(api))
    {
        return libjami::setAudioManager(api);
    }

    auto
    getSupportedAudioManagers()
        -> decltype(libjami::getSupportedAudioManagers())
    {
        return libjami::getSupportedAudioManagers();
    }

    auto
    getRecordPath() -> decltype(libjami::getRecordPath())
    {
        return libjami::getRecordPath();
    }

    void
    setRecordPath(const std::string& recPath)
    {
        libjami::setRecordPath(recPath);
    }

    auto
    getIsAlwaysRecording() -> decltype(libjami::getIsAlwaysRecording())
    {
        return libjami::getIsAlwaysRecording();
    }

    void
    setIsAlwaysRecording(const bool& rec)
    {
        libjami::setIsAlwaysRecording(rec);
    }

    auto
    getRecordPreview() -> decltype(libjami::getRecordPreview())
    {
        return libjami::getRecordPreview();
    }

    void
    setRecordPreview(const bool& rec)
    {
        libjami::setRecordPreview(rec);
    }

    auto
    getRecordQuality() -> decltype(libjami::getRecordQuality())
    {
        return libjami::getRecordQuality();
    }

    void
    setRecordQuality(const int32_t& quality)
    {
        libjami::setRecordQuality(quality);
    }

    void
    setHistoryLimit(const int32_t& days)
    {
        libjami::setHistoryLimit(days);
    }

    auto
    getHistoryLimit() -> decltype(libjami::getHistoryLimit())
    {
        return libjami::getHistoryLimit();
    }

    void
    setRingingTimeout(const int32_t& timeout)
    {
        libjami::setRingingTimeout(timeout);
    }

    auto
    getRingingTimeout() -> decltype(libjami::getRingingTimeout())
    {
        return libjami::getRingingTimeout();
    }

    void
    setAccountsOrder(const std::string& order)
    {
        libjami::setAccountsOrder(order);
    }

    auto
    validateCertificate(const std::string& accountId,
                        const std::string& certificate)
        -> decltype(libjami::validateCertificate(accountId, certificate))
    {
        return libjami::validateCertificate(accountId, certificate);
    }

    auto
    validateCertificatePath(const std::string& accountId,
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
    getCertificateDetails(const std::string& accountId,
                          const std::string& certificate)
        -> decltype(libjami::getCertificateDetails(accountId, certificate))
    {
        return libjami::getCertificateDetails(accountId, certificate);
    }

    auto
    getCertificateDetailsPath(const std::string& accountId,
                              const std::string& certificate,
                              const std::string& privateKey,
                              const std::string& privateKeyPass)
        -> decltype(libjami::getCertificateDetailsPath(
            accountId, certificate, privateKey, privateKeyPass))
    {
        return libjami::getCertificateDetailsPath(accountId, certificate, privateKey, privateKeyPass);
    }

    auto
    getPinnedCertificates(const std::string& accountId)
        -> decltype(libjami::getPinnedCertificates(accountId))
    {
        return libjami::getPinnedCertificates(accountId);
    }

    auto
    pinCertificate(const std::string& accountId,
                   const std::vector<uint8_t>& certificate,
                   const bool& local)
        -> decltype(libjami::pinCertificate(accountId, certificate, local))
    {
        return libjami::pinCertificate(accountId, certificate, local);
    }

    void
    pinCertificatePath(const std::string& accountId, const std::string& certPath)
    {
        libjami::pinCertificatePath(accountId, certPath);
    }

    auto
    unpinCertificate(const std::string& accountId, const std::string& certId)
        -> decltype(libjami::unpinCertificate(accountId, certId))
    {
        return libjami::unpinCertificate(accountId, certId);
    }

    auto
    unpinCertificatePath(const std::string& accountId,
                         const std::string& p)
        -> decltype(libjami::unpinCertificatePath(accountId, p))
    {
        return libjami::unpinCertificatePath(accountId, p);
    }

    auto
    pinRemoteCertificate(const std::string& accountId,
                         const std::string& certId)
        -> decltype(libjami::pinRemoteCertificate(accountId, certId))
    {
        return libjami::pinRemoteCertificate(accountId, certId);
    }

    auto
    setCertificateStatus(const std::string& accountId,
                         const std::string& certId,
                         const std::string& status)
        -> decltype(libjami::setCertificateStatus(accountId, certId, status))
    {
        return libjami::setCertificateStatus(accountId, certId, status);
    }

    auto
    getCertificatesByStatus(const std::string& accountId,
                            const std::string& status)
        -> decltype(libjami::getCertificatesByStatus(accountId, status))
    {
        return libjami::getCertificatesByStatus(accountId, status);
    }

    auto
    getTrustRequests(const std::string& accountId)
        -> decltype(libjami::getTrustRequests(accountId))
    {
        return libjami::getTrustRequests(accountId);
    }

    auto
    acceptTrustRequest(const std::string& accountId, const std::string& from)
        -> decltype(libjami::acceptTrustRequest(accountId, from))
    {
        return libjami::acceptTrustRequest(accountId, from);
    }

    auto
    discardTrustRequest(const std::string& accountId, const std::string& from)
        -> decltype(libjami::discardTrustRequest(accountId, from))
    {
        return libjami::discardTrustRequest(accountId, from);
    }

    void
    sendTrustRequest(const std::string& accountId,
                     const std::string& to,
                     const std::vector<uint8_t>& payload)
    {
        libjami::sendTrustRequest(accountId, to, payload);
    }

    void
    addContact(const std::string& accountId, const std::string& uri)
    {
        libjami::addContact(accountId, uri);
    }

    void
    removeContact(const std::string& accountId,
                  const std::string& uri,
                  const bool& ban)
    {
        libjami::removeContact(accountId, uri, ban);
    }

    auto
    getContactDetails(const std::string& accountId, const std::string& uri)
        -> decltype(libjami::getContactDetails(accountId, uri))
    {
        return libjami::getContactDetails(accountId, uri);
    }

    auto
    getConnectionList(const std::string& accountId, const std::string& conversationId)
        -> decltype(libjami::getConnectionList(accountId, conversationId))
    {
        return libjami::getConnectionList(accountId,conversationId);
    }

    auto
    getChannelList(const std::string& accountId, const std::string& connectionId)
        -> decltype(libjami::getChannelList(accountId, connectionId))
    {
        return libjami::getChannelList(accountId,connectionId);
    }

    auto
    getContacts(const std::string& accountId)
        -> decltype(libjami::getContacts(accountId))
    {
        return libjami::getContacts(accountId);
    }

    auto
    getCredentials(const std::string& accountID)
        -> decltype(libjami::getCredentials(accountID))
    {
        return libjami::getCredentials(accountID);
    }

    void
    setCredentials(
        const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details)
    {
        libjami::setCredentials(accountID, details);
    }

    auto
    getAddrFromInterfaceName(const std::string& interface)
        -> decltype(libjami::getAddrFromInterfaceName(interface))
    {
        return libjami::getAddrFromInterfaceName(interface);
    }

    auto
    getAllIpInterface() -> decltype(libjami::getAllIpInterface())
    {
        return libjami::getAllIpInterface();
    }

    auto
    getAllIpInterfaceByName() -> decltype(libjami::getAllIpInterfaceByName())
    {
        return libjami::getAllIpInterfaceByName();
    }

    void
    setVolume(const std::string& device, const double& value)
    {
        libjami::setVolume(device, value);
    }

    auto
    getVolume(const std::string& device)
        -> decltype(libjami::getVolume(device))
    {
        return libjami::getVolume(device);
    }

    void
    connectivityChanged()
    {
        libjami::connectivityChanged();
    }

    void
    sendFile(const std::string& accountId,
             const std::string& conversationId,
             const std::string& path,
             const std::string& displayName,
             const std::string& replyTo)
    {
        libjami::sendFile(accountId, conversationId, path, displayName, replyTo);
    }

    std::tuple<uint32_t, std::string, int64_t, int64_t>
    fileTransferInfo(const std::string& accountId,
                     const std::string& to,
                     const std::string& fileId)
    {
        uint32_t error;
        std::string path;
        int64_t total;
        int64_t progress;
        error = (uint32_t) libjami::fileTransferInfo(accountId, to, fileId, path, total, progress);
        return {error, path, total, progress};
    }

    bool
    downloadFile(const std::string& accountId,
                 const std::string& conversationUri,
                 const std::string& interactionId,
                 const std::string& fileId,
                 const std::string& path)
    {
        return libjami::downloadFile(accountId, conversationUri, interactionId, fileId, path);
    }

    uint32_t
    cancelDataTransfer(const std::string& accountId,
                       const std::string& conversationId,
                       const std::string& fileId)
    {
        return uint32_t(libjami::cancelDataTransfer(accountId, conversationId, fileId));
    }

    std::string
    startConversation(const std::string& accountId)
    {
        return libjami::startConversation(accountId);
    }

    void
    acceptConversationRequest(const std::string& accountId,
                              const std::string& conversationId)
    {
        libjami::acceptConversationRequest(accountId, conversationId);
    }

    void
    declineConversationRequest(const std::string& accountId,
                               const std::string& conversationId)
    {
        libjami::declineConversationRequest(accountId, conversationId);
    }

    bool
    removeConversation(const std::string& accountId,
                       const std::string& conversationId)
    {
        return libjami::removeConversation(accountId, conversationId);
    }

    std::vector<std::string>
    getConversations(const std::string& accountId)
    {
        return libjami::getConversations(accountId);
    }

    std::vector<std::map<std::string, std::string>>
    getActiveCalls(const std::string& accountId,
                   const std::string& conversationId)
    {
        return libjami::getActiveCalls(accountId, conversationId);
    }

    std::vector<std::map<std::string, std::string>>
    getConversationRequests(const std::string& accountId)
    {
        return libjami::getConversationRequests(accountId);
    }

    void
    updateConversationInfos(const std::string& accountId,
                            const std::string& conversationId,
                            const std::map<std::string, std::string>& infos)
    {
        libjami::updateConversationInfos(accountId, conversationId, infos);
    }

    std::map<std::string, std::string>
    conversationInfos(const std::string& accountId,
                      const std::string& conversationId)
    {
        return libjami::conversationInfos(accountId, conversationId);
    }

    void
    setConversationPreferences(const std::string& accountId,
                               const std::string& conversationId,
                               const std::map<std::string, std::string>& infos)
    {
        libjami::setConversationPreferences(accountId, conversationId, infos);
    }

    std::map<std::string, std::string>
    getConversationPreferences(const std::string& accountId,
                               const std::string& conversationId)
    {
        return libjami::getConversationPreferences(accountId, conversationId);
    }

    void
    addConversationMember(const std::string& accountId,
                          const std::string& conversationId,
                          const std::string& contactUri)
    {
        libjami::addConversationMember(accountId, conversationId, contactUri);
    }

    void
    removeConversationMember(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& contactUri)
    {
        libjami::removeConversationMember(accountId, conversationId, contactUri);
    }

    std::vector<std::map<std::string, std::string>>
    getConversationMembers(const std::string& accountId,
                           const std::string& conversationId)
    {
        return libjami::getConversationMembers(accountId, conversationId);
    }

    void
    sendMessage(const std::string& accountId,
                const std::string& conversationId,
                const std::string& message,
                const std::string& replyTo,
                const int32_t& flag)
    {
        libjami::sendMessage(accountId, conversationId, message, replyTo, flag);
    }

    uint32_t
    loadConversationMessages(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& fromMessage,
                             const uint32_t& n)
    {
        return libjami::loadConversationMessages(accountId, conversationId, fromMessage, n);
    }

    uint32_t
    loadConversation(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& fromMessage,
                             const uint32_t& n)
    {
        return libjami::loadConversation(accountId, conversationId, fromMessage, n);
    }

    uint32_t
    loadConversationUntil(const std::string& accountId,
                          const std::string& conversationId,
                          const std::string& fromMessage,
                          const std::string& to)
    {
        return libjami::loadConversationUntil(accountId, conversationId, fromMessage, to);
    }

    uint32_t
    countInteractions(const std::string& accountId,
                      const std::string& conversationId,
                      const std::string& toId,
                      const std::string& fromId,
                      const std::string& authorUri)
    {
        return libjami::countInteractions(accountId, conversationId, toId, fromId, authorUri);
    }

    uint32_t
    searchConversation(const std::string& accountId,
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
    isAudioMeterActive(const std::string& id)
    {
        return libjami::isAudioMeterActive(id);
    }

    void
    setAudioMeterState(const std::string& id, const bool& state)
    {
        return libjami::setAudioMeterState(id, state);
    }

    void
    setDefaultModerator(const std::string& accountID,
                        const std::string& peerURI,
                        const bool& state)
    {
        libjami::setDefaultModerator(accountID, peerURI, state);
    }

    auto
    getDefaultModerators(const std::string& accountID)
        -> decltype(libjami::getDefaultModerators(accountID))
    {
        return libjami::getDefaultModerators(accountID);
    }

    void
    enableLocalModerators(const std::string& accountID,
                          const bool& isModEnabled)
    {
        return libjami::enableLocalModerators(accountID, isModEnabled);
    }

    bool
    isLocalModeratorsEnabled(const std::string& accountID)
    {
        return libjami::isLocalModeratorsEnabled(accountID);
    }

    void
    setAllModerators(const std::string& accountID, const bool& allModerators)
    {
        return libjami::setAllModerators(accountID, allModerators);
    }

    bool
    isAllModerators(const std::string& accountID)
    {
        return libjami::isAllModerators(accountID);
    }

private:

    void
    registerSignalHandlers()
    {
        using namespace std::placeholders;

        using libjami::exportable_serialized_callback;
        using libjami::ConfigurationSignal;
        using libjami::AudioSignal;
        using libjami::DataTransferSignal;
        using libjami::ConversationSignal;
        using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

        // Configuration event handlers
        const std::map<std::string, SharedCallback> configEvHandlers = {
            exportable_serialized_callback<ConfigurationSignal::VolumeChanged>(
                std::bind(&DBusConfigurationManager::emitVolumeChanged, this, _1, _2)),
            exportable_serialized_callback<ConfigurationSignal::AccountsChanged>(
                std::bind(&DBusConfigurationManager::emitAccountsChanged, this)),
            exportable_serialized_callback<ConfigurationSignal::AccountDetailsChanged>(
                std::bind(&DBusConfigurationManager::emitAccountDetailsChanged, this, _1, _2)),
            exportable_serialized_callback<ConfigurationSignal::StunStatusFailed>(
                std::bind(&DBusConfigurationManager::emitStunStatusFailure, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::RegistrationStateChanged>(
                std::bind(&DBusConfigurationManager::emitRegistrationStateChanged, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConfigurationSignal::VolatileDetailsChanged>(
                std::bind(&DBusConfigurationManager::emitVolatileAccountDetailsChanged, this, _1, _2)),
            exportable_serialized_callback<ConfigurationSignal::Error>(
                std::bind(&DBusConfigurationManager::emitErrorAlert, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::IncomingAccountMessage>(
                std::bind(&DBusConfigurationManager::emitIncomingAccountMessage, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConfigurationSignal::AccountMessageStatusChanged>(
                std::bind(&DBusConfigurationManager::emitAccountMessageStatusChanged, this, _1, _2, _3, _4, _5)),
            exportable_serialized_callback<ConfigurationSignal::ProfileReceived>(
                std::bind(&DBusConfigurationManager::emitProfileReceived, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::ActiveCallsChanged>(
                std::bind(&DBusConfigurationManager::emitActiveCallsChanged, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::ComposingStatusChanged>(
                std::bind(&DBusConfigurationManager::emitComposingStatusChanged, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConfigurationSignal::IncomingTrustRequest>(
                std::bind(&DBusConfigurationManager::emitIncomingTrustRequest, this, _1, _2, _3, _4, _5)),
            exportable_serialized_callback<ConfigurationSignal::ContactAdded>(
                std::bind(&DBusConfigurationManager::emitContactAdded, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::ContactRemoved>(
                std::bind(&DBusConfigurationManager::emitContactRemoved, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::ExportOnRingEnded>(
                std::bind(&DBusConfigurationManager::emitExportOnRingEnded, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::KnownDevicesChanged>(
                std::bind(&DBusConfigurationManager::emitKnownDevicesChanged, this, _1, _2)),
            exportable_serialized_callback<ConfigurationSignal::NameRegistrationEnded>(
                std::bind(&DBusConfigurationManager::emitNameRegistrationEnded, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::UserSearchEnded>(
                std::bind(&DBusConfigurationManager::emitUserSearchEnded, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConfigurationSignal::RegisteredNameFound>(
                std::bind(&DBusConfigurationManager::emitRegisteredNameFound, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConfigurationSignal::DeviceRevocationEnded>(
                std::bind(&DBusConfigurationManager::emitDeviceRevocationEnded, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::AccountProfileReceived>(
                std::bind(&DBusConfigurationManager::emitAccountProfileReceived, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::CertificatePinned>(
                std::bind(&DBusConfigurationManager::emitCertificatePinned, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::CertificatePathPinned>(
                std::bind(&DBusConfigurationManager::emitCertificatePathPinned, this, _1, _2)),
            exportable_serialized_callback<ConfigurationSignal::CertificateExpired>(
                std::bind(&DBusConfigurationManager::emitCertificateExpired, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::CertificateStateChanged>(
                std::bind(&DBusConfigurationManager::emitCertificateStateChanged, this, _1, _2, _3)),
            exportable_serialized_callback<ConfigurationSignal::MediaParametersChanged>(
                std::bind(&DBusConfigurationManager::emitMediaParametersChanged, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::MigrationEnded>(
                std::bind(&DBusConfigurationManager::emitMigrationEnded, this, _1, _2)),
            exportable_serialized_callback<ConfigurationSignal::HardwareDecodingChanged>(
                std::bind(&DBusConfigurationManager::emitHardwareDecodingChanged, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::HardwareEncodingChanged>(
                std::bind(&DBusConfigurationManager::emitHardwareEncodingChanged, this, _1)),
            exportable_serialized_callback<ConfigurationSignal::MessageSend>(
                std::bind(&DBusConfigurationManager::emitMessageSend, this, _1)),
        };

        // Audio event handlers
        const std::map<std::string, SharedCallback> audioEvHandlers = {
            exportable_serialized_callback<AudioSignal::DeviceEvent>(
                std::bind(&DBusConfigurationManager::emitAudioDeviceEvent, this)),
            exportable_serialized_callback<AudioSignal::AudioMeter>(
                std::bind(&DBusConfigurationManager::emitAudioMeter, this, _1, _2)),
        };

        const std::map<std::string, SharedCallback> dataXferEvHandlers = {
            exportable_serialized_callback<DataTransferSignal::DataTransferEvent>(
                std::bind(&DBusConfigurationManager::emitDataTransferEvent, this, _1, _2, _3, _4, _5)),
        };

        const std::map<std::string, SharedCallback> convEvHandlers = {
            exportable_serialized_callback<ConversationSignal::ConversationLoaded>(
                std::bind(&DBusConfigurationManager::emitConversationLoaded, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConversationSignal::SwarmLoaded>([this](const uint32_t& id, const std::string& account_id, const std::string& conversation_id, const std::vector<libjami::SwarmMessage>& messages) {
                std::vector<DBusSwarmMessage> msgList;
                for (const auto& message: messages) {
                    DBusSwarmMessage msg {message.id, message.type, message.linearizedParent, message.body, message.reactions, message.editions};
                    msgList.push_back(msg);
                }
                DBusConfigurationManager::emitSwarmLoaded(id, account_id, conversation_id, msgList);
            }),
            exportable_serialized_callback<ConversationSignal::MessagesFound>(
                std::bind(&DBusConfigurationManager::emitMessagesFound, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConversationSignal::MessageReceived>(
                std::bind(&DBusConfigurationManager::emitMessageReceived, this, _1, _2, _3)),
            exportable_serialized_callback<ConversationSignal::SwarmMessageReceived>([this](const std::string& account_id, const std::string& conversation_id, const libjami::SwarmMessage& message) {
                DBusSwarmMessage msg {message.id, message.type, message.linearizedParent, message.body, message.reactions, message.editions};
                DBusConfigurationManager::emitSwarmMessageReceived(account_id, conversation_id, msg);
            }),
            exportable_serialized_callback<ConversationSignal::SwarmMessageUpdated>([this](const std::string& account_id, const std::string& conversation_id, const libjami::SwarmMessage& message) {
                DBusSwarmMessage msg {message.id, message.type, message.linearizedParent, message.body, message.reactions, message.editions};
                DBusConfigurationManager::emitSwarmMessageUpdated(account_id, conversation_id, msg);
            }),
            exportable_serialized_callback<ConversationSignal::ReactionAdded>(
                std::bind(&DBusConfigurationManager::emitReactionAdded, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConversationSignal::ReactionRemoved>(
                std::bind(&DBusConfigurationManager::emitReactionRemoved, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConversationSignal::ConversationProfileUpdated>(
                std::bind(&DBusConfigurationManager::emitConversationProfileUpdated, this, _1, _2, _3)),
            exportable_serialized_callback<ConversationSignal::ConversationRequestReceived>(
                std::bind(&DBusConfigurationManager::emitConversationRequestReceived, this, _1, _2, _3)),
            exportable_serialized_callback<ConversationSignal::ConversationRequestDeclined>(
                std::bind(&DBusConfigurationManager::emitConversationRequestDeclined, this, _1, _2)),
            exportable_serialized_callback<ConversationSignal::ConversationReady>(
                std::bind(&DBusConfigurationManager::emitConversationReady, this, _1, _2)),
            exportable_serialized_callback<ConversationSignal::ConversationRemoved>(
                std::bind(&DBusConfigurationManager::emitConversationRemoved, this, _1, _2)),
            exportable_serialized_callback<ConversationSignal::ConversationMemberEvent>(
                std::bind(&DBusConfigurationManager::emitConversationMemberEvent, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConversationSignal::OnConversationError>(
                std::bind(&DBusConfigurationManager::emitOnConversationError, this, _1, _2, _3, _4)),
            exportable_serialized_callback<ConversationSignal::ConversationPreferencesUpdated>(
                std::bind(&DBusConfigurationManager::emitConversationPreferencesUpdated, this, _1, _2, _3)),
        };

        libjami::registerSignalHandlers(configEvHandlers);
        libjami::registerSignalHandlers(audioEvHandlers);
        libjami::registerSignalHandlers(dataXferEvHandlers);
        libjami::registerSignalHandlers(convEvHandlers);
    }
};
