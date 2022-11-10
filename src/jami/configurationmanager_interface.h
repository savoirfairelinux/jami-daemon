/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include "def.h"

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <cstdint>

#include "jami.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace libjami {

[[deprecated("Replaced by registerSignalHandlers")]] LIBJAMI_PUBLIC void registerConfHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

struct LIBJAMI_PUBLIC Message
{
    std::string from;
    std::map<std::string, std::string> payloads;
    uint64_t received;
};

LIBJAMI_PUBLIC std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
LIBJAMI_PUBLIC std::map<std::string, std::string> getVolatileAccountDetails(
    const std::string& accountID);
LIBJAMI_PUBLIC void setAccountDetails(const std::string& accountID,
                                      const std::map<std::string, std::string>& details);
LIBJAMI_PUBLIC void setAccountActive(const std::string& accountID,
                                     bool active,
                                     bool shutdownConnections = false);
LIBJAMI_PUBLIC std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
LIBJAMI_PUBLIC std::string addAccount(const std::map<std::string, std::string>& details,
                                      const std::string& accountID = {});
LIBJAMI_PUBLIC void monitor(bool continuous);
LIBJAMI_PUBLIC bool exportOnRing(const std::string& accountID, const std::string& password);
LIBJAMI_PUBLIC bool exportToFile(const std::string& accountID,
                                 const std::string& destinationPath,
                                 const std::string& password = {});
LIBJAMI_PUBLIC bool revokeDevice(const std::string& accountID,
                                 const std::string& password,
                                 const std::string& deviceID);
LIBJAMI_PUBLIC std::map<std::string, std::string> getKnownRingDevices(const std::string& accountID);
LIBJAMI_PUBLIC bool changeAccountPassword(const std::string& accountID,
                                          const std::string& password_old,
                                          const std::string& password_new);
LIBJAMI_PUBLIC bool isPasswordValid(const std::string& accountID, const std::string& password);

LIBJAMI_PUBLIC bool lookupName(const std::string& account,
                               const std::string& nameserver,
                               const std::string& name);
LIBJAMI_PUBLIC bool lookupAddress(const std::string& account,
                                  const std::string& nameserver,
                                  const std::string& address);
LIBJAMI_PUBLIC bool registerName(const std::string& account,
                                 const std::string& password,
                                 const std::string& name);
LIBJAMI_PUBLIC bool searchUser(const std::string& account, const std::string& query);

LIBJAMI_PUBLIC void removeAccount(const std::string& accountID);
LIBJAMI_PUBLIC std::vector<std::string> getAccountList();
LIBJAMI_PUBLIC void sendRegister(const std::string& accountID, bool enable);
LIBJAMI_PUBLIC void registerAllAccounts(void);
LIBJAMI_PUBLIC uint64_t sendAccountTextMessage(const std::string& accountID,
                                               const std::string& to,
                                               const std::map<std::string, std::string>& payloads,
                                               int32_t flags);
LIBJAMI_PUBLIC bool cancelMessage(const std::string& accountID, uint64_t message);
LIBJAMI_PUBLIC std::vector<Message> getLastMessages(const std::string& accountID,
                                                    const uint64_t& base_timestamp);
LIBJAMI_PUBLIC std::map<std::string, std::string> getNearbyPeers(const std::string& accountID);
LIBJAMI_PUBLIC int getMessageStatus(uint64_t id);
LIBJAMI_PUBLIC int getMessageStatus(const std::string& accountID, uint64_t id);
LIBJAMI_PUBLIC void setIsComposing(const std::string& accountID,
                                   const std::string& conversationUri,
                                   bool isWriting);
LIBJAMI_PUBLIC bool setMessageDisplayed(const std::string& accountID,
                                        const std::string& conversationUri,
                                        const std::string& messageId,
                                        int status);

LIBJAMI_PUBLIC std::vector<unsigned> getCodecList();
LIBJAMI_PUBLIC std::vector<std::string> getSupportedTlsMethod();
LIBJAMI_PUBLIC std::vector<std::string> getSupportedCiphers(const std::string& accountID);
LIBJAMI_PUBLIC std::map<std::string, std::string> getCodecDetails(const std::string& accountID,
                                                                  const unsigned& codecId);
LIBJAMI_PUBLIC bool setCodecDetails(const std::string& accountID,
                                    const unsigned& codecId,
                                    const std::map<std::string, std::string>& details);
LIBJAMI_PUBLIC std::vector<unsigned> getActiveCodecList(const std::string& accountID);

LIBJAMI_PUBLIC void setActiveCodecList(const std::string& accountID,
                                       const std::vector<unsigned>& list);

LIBJAMI_PUBLIC std::vector<std::string> getAudioPluginList();
LIBJAMI_PUBLIC void setAudioPlugin(const std::string& audioPlugin);
LIBJAMI_PUBLIC std::vector<std::string> getAudioOutputDeviceList();
LIBJAMI_PUBLIC void setAudioOutputDevice(int32_t index);
LIBJAMI_PUBLIC void startAudio();
LIBJAMI_PUBLIC void setAudioInputDevice(int32_t index);
LIBJAMI_PUBLIC void setAudioRingtoneDevice(int32_t index);
LIBJAMI_PUBLIC std::vector<std::string> getAudioInputDeviceList();
LIBJAMI_PUBLIC std::vector<std::string> getCurrentAudioDevicesIndex();
LIBJAMI_PUBLIC int32_t getAudioInputDeviceIndex(const std::string& name);
LIBJAMI_PUBLIC int32_t getAudioOutputDeviceIndex(const std::string& name);
LIBJAMI_PUBLIC std::string getCurrentAudioOutputPlugin();
LIBJAMI_PUBLIC std::string getNoiseSuppressState();
LIBJAMI_PUBLIC void setNoiseSuppressState(const std::string& state);

LIBJAMI_PUBLIC bool isAgcEnabled();
LIBJAMI_PUBLIC void setAgcState(bool enabled);

LIBJAMI_PUBLIC void muteDtmf(bool mute);
LIBJAMI_PUBLIC bool isDtmfMuted();

LIBJAMI_PUBLIC bool isCaptureMuted();
LIBJAMI_PUBLIC void muteCapture(bool mute);
LIBJAMI_PUBLIC bool isPlaybackMuted();
LIBJAMI_PUBLIC void mutePlayback(bool mute);
LIBJAMI_PUBLIC bool isRingtoneMuted();
LIBJAMI_PUBLIC void muteRingtone(bool mute);

LIBJAMI_PUBLIC std::vector<std::string> getSupportedAudioManagers();
LIBJAMI_PUBLIC std::string getAudioManager();
LIBJAMI_PUBLIC bool setAudioManager(const std::string& api);

LIBJAMI_PUBLIC std::string getRecordPath();
LIBJAMI_PUBLIC void setRecordPath(const std::string& recPath);
LIBJAMI_PUBLIC bool getIsAlwaysRecording();
LIBJAMI_PUBLIC void setIsAlwaysRecording(bool rec);
LIBJAMI_PUBLIC bool getRecordPreview();
LIBJAMI_PUBLIC void setRecordPreview(bool rec);
LIBJAMI_PUBLIC int getRecordQuality();
LIBJAMI_PUBLIC void setRecordQuality(int quality);

LIBJAMI_PUBLIC void setHistoryLimit(int32_t days);
LIBJAMI_PUBLIC int32_t getHistoryLimit();

LIBJAMI_PUBLIC void setRingingTimeout(int32_t timeout);
LIBJAMI_PUBLIC int32_t getRingingTimeout();

LIBJAMI_PUBLIC void setAccountsOrder(const std::string& order);

LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getCredentials(
    const std::string& accountID);
LIBJAMI_PUBLIC void setCredentials(const std::string& accountID,
                                   const std::vector<std::map<std::string, std::string>>& details);

LIBJAMI_PUBLIC std::string getAddrFromInterfaceName(const std::string& iface);

LIBJAMI_PUBLIC std::vector<std::string> getAllIpInterface();
LIBJAMI_PUBLIC std::vector<std::string> getAllIpInterfaceByName();

LIBJAMI_PUBLIC void setVolume(const std::string& device, double value);
LIBJAMI_PUBLIC double getVolume(const std::string& device);

/*
 * Security
 */
LIBJAMI_PUBLIC std::map<std::string, std::string> validateCertificate(
    const std::string& accountId, const std::string& certificate);
LIBJAMI_PUBLIC std::map<std::string, std::string> validateCertificatePath(
    const std::string& accountId,
    const std::string& certificatePath,
    const std::string& privateKey,
    const std::string& privateKeyPassword,
    const std::string& caList);

LIBJAMI_PUBLIC std::map<std::string, std::string> getCertificateDetails(
    const std::string& certificate);
LIBJAMI_PUBLIC std::map<std::string, std::string> getCertificateDetailsPath(
    const std::string& certificatePath,
    const std::string& privateKey,
    const std::string& privateKeyPassword);

LIBJAMI_PUBLIC std::vector<std::string> getPinnedCertificates();

LIBJAMI_PUBLIC std::vector<std::string> pinCertificate(const std::vector<uint8_t>& certificate,
                                                       bool local);
LIBJAMI_PUBLIC bool unpinCertificate(const std::string& certId);

LIBJAMI_PUBLIC void pinCertificatePath(const std::string& path);
LIBJAMI_PUBLIC unsigned unpinCertificatePath(const std::string& path);

LIBJAMI_PUBLIC bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
LIBJAMI_PUBLIC bool setCertificateStatus(const std::string& account,
                                         const std::string& certId,
                                         const std::string& status);
LIBJAMI_PUBLIC std::vector<std::string> getCertificatesByStatus(const std::string& account,
                                                                const std::string& status);

/* contact requests */
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getTrustRequests(
    const std::string& accountId);
LIBJAMI_PUBLIC bool acceptTrustRequest(const std::string& accountId, const std::string& from);
LIBJAMI_PUBLIC bool discardTrustRequest(const std::string& accountId, const std::string& from);
LIBJAMI_PUBLIC void sendTrustRequest(const std::string& accountId,
                                     const std::string& to,
                                     const std::vector<uint8_t>& payload = {});

/* Contacts */

LIBJAMI_PUBLIC void addContact(const std::string& accountId, const std::string& uri);
LIBJAMI_PUBLIC void removeContact(const std::string& accountId, const std::string& uri, bool ban);
LIBJAMI_PUBLIC std::map<std::string, std::string> getContactDetails(const std::string& accountId,
                                                                    const std::string& uri);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getContacts(
    const std::string& accountId);

/*
 * Network connectivity
 */
LIBJAMI_PUBLIC void connectivityChanged();

/* Dht proxy */

/**
 * Set the device push notification token (for all accounts).
 * If set, proxy clients will use push notifications.
 * Set to empty to disable push notifications.
 */
LIBJAMI_PUBLIC void setPushNotificationToken(const std::string& pushDeviceToken);

/**
 * Set the topic for ios
 * bundle_id for ios 14.5 and higher
 * bundle_id.voip for ios prior 14.5
 */
LIBJAMI_PUBLIC void setPushNotificationTopic(const std::string& topic);

LIBJAMI_PUBLIC void setPushNotificationConfig(const std::map<std::string, std::string>& data);

/**
 * To be called by clients with relevant data when a push notification is received.
 */
LIBJAMI_PUBLIC void pushNotificationReceived(const std::string& from,
                                             const std::map<std::string, std::string>& data);

/**
 * Returns whether or not the audio meter is enabled for ring buffer @id.
 *
 * NOTE If @id is empty, returns true if at least 1 audio meter is enabled.
 */
LIBJAMI_PUBLIC bool isAudioMeterActive(const std::string& id);

/**
 * Enables/disables an audio meter for the specified @id.
 *
 * NOTE If @id is empty, applies to all ring buffers.
 */
LIBJAMI_PUBLIC void setAudioMeterState(const std::string& id, bool state);

/**
 * Add/remove default moderator for conferences
 */
LIBJAMI_PUBLIC void setDefaultModerator(const std::string& accountID,
                                        const std::string& peerURI,
                                        bool state);

/**
 * Get default moderators for an account
 */
LIBJAMI_PUBLIC std::vector<std::string> getDefaultModerators(const std::string& accountID);

/**
 * Enable/disable local moderators for conferences
 */
LIBJAMI_PUBLIC void enableLocalModerators(const std::string& accountID, bool isModEnabled);

/**
 * Get local moderators state
 */
LIBJAMI_PUBLIC bool isLocalModeratorsEnabled(const std::string& accountID);

/**
 * Enable/disable all moderators for conferences
 */
LIBJAMI_PUBLIC void setAllModerators(const std::string& accountID, bool allModerators);

/**
 * Get all moderators state
 */
LIBJAMI_PUBLIC bool isAllModerators(const std::string& accountID);

struct LIBJAMI_PUBLIC AudioSignal
{
    struct LIBJAMI_PUBLIC DeviceEvent
    {
        constexpr static const char* name = "audioDeviceEvent";
        using cb_type = void(void);
    };
    // Linear audio level (between 0 and 1). To get level in dB: dB=20*log10(level)
    struct LIBJAMI_PUBLIC AudioMeter
    {
        constexpr static const char* name = "AudioMeter";
        using cb_type = void(const std::string& id, float level);
    };
};

// Configuration signal type definitions
struct LIBJAMI_PUBLIC ConfigurationSignal
{
    struct LIBJAMI_PUBLIC VolumeChanged
    {
        constexpr static const char* name = "VolumeChanged";
        using cb_type = void(const std::string& /*device*/, double /*value*/);
    };
    struct LIBJAMI_PUBLIC AccountsChanged
    {
        constexpr static const char* name = "AccountsChanged";
        using cb_type = void(void);
    };
    struct LIBJAMI_PUBLIC Error
    {
        constexpr static const char* name = "Error";
        using cb_type = void(int /*alert*/);
    };

    // TODO: move those to AccountSignal in next API breakage
    struct LIBJAMI_PUBLIC AccountDetailsChanged
    {
        constexpr static const char* name = "AccountDetailsChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::map<std::string, std::string>& /* details */);
    };
    struct LIBJAMI_PUBLIC StunStatusFailed
    {
        constexpr static const char* name = "StunStatusFailed";
        using cb_type = void(const std::string& /*account_id*/);
    };
    struct LIBJAMI_PUBLIC RegistrationStateChanged
    {
        constexpr static const char* name = "RegistrationStateChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*state*/,
                             int /*detailsCode*/,
                             const std::string& /*detailsStr*/);
    };
    struct LIBJAMI_PUBLIC VolatileDetailsChanged
    {
        constexpr static const char* name = "VolatileDetailsChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::map<std::string, std::string>& /* details */);
    };
    struct LIBJAMI_PUBLIC IncomingAccountMessage
    {
        constexpr static const char* name = "IncomingAccountMessage";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*from*/,
                             const std::string& /*message_id*/,
                             const std::map<std::string, std::string>& /*payloads*/);
    };
    struct LIBJAMI_PUBLIC AccountMessageStatusChanged
    {
        constexpr static const char* name = "AccountMessageStatusChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*conversation_id*/,
                             const std::string& /*peer*/,
                             const std::string& /*message_id*/,
                             int /*state*/);
    };
    struct LIBJAMI_PUBLIC NeedsHost
    {
        constexpr static const char* name = "NeedsHost";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*conversation_id*/);
    };
    struct LIBJAMI_PUBLIC ActiveCallsChanged
    {
        constexpr static const char* name = "ActiveCallsChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*conversation_id*/,
                             const std::vector<std::map<std::string, std::string>>& /*activeCalls*/);
    };
    struct LIBJAMI_PUBLIC ProfileReceived
    {
        constexpr static const char* name = "ProfileReceived";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*from*/,
                             const std::string& /*vcard*/);
    };
    struct LIBJAMI_PUBLIC ComposingStatusChanged
    {
        constexpr static const char* name = "ComposingStatusChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*from*/,
                             int /*status*/);
    };
    struct LIBJAMI_PUBLIC IncomingTrustRequest
    {
        constexpr static const char* name = "IncomingTrustRequest";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*from*/,
                             const std::string& /*conversationId*/,
                             const std::vector<uint8_t>& payload,
                             time_t received);
    };
    struct LIBJAMI_PUBLIC ContactAdded
    {
        constexpr static const char* name = "ContactAdded";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*uri*/,
                             bool confirmed);
    };
    struct LIBJAMI_PUBLIC ContactRemoved
    {
        constexpr static const char* name = "ContactRemoved";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*uri*/,
                             bool banned);
    };
    struct LIBJAMI_PUBLIC ExportOnRingEnded
    {
        constexpr static const char* name = "ExportOnRingEnded";
        using cb_type = void(const std::string& /*account_id*/, int state, const std::string& pin);
    };
    struct LIBJAMI_PUBLIC NameRegistrationEnded
    {
        constexpr static const char* name = "NameRegistrationEnded";
        using cb_type = void(const std::string& /*account_id*/, int state, const std::string& name);
    };
    struct LIBJAMI_PUBLIC KnownDevicesChanged
    {
        constexpr static const char* name = "KnownDevicesChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::map<std::string, std::string>& devices);
    };
    struct LIBJAMI_PUBLIC RegisteredNameFound
    {
        constexpr static const char* name = "RegisteredNameFound";
        using cb_type = void(const std::string& /*account_id*/,
                             int state,
                             const std::string& /*address*/,
                             const std::string& /*name*/);
    };
    struct LIBJAMI_PUBLIC UserSearchEnded
    {
        constexpr static const char* name = "UserSearchEnded";
        using cb_type = void(const std::string& /*account_id*/,
                             int state,
                             const std::string& /*query*/,
                             const std::vector<std::map<std::string, std::string>>& /*results*/);
    };
    struct LIBJAMI_PUBLIC CertificatePinned
    {
        constexpr static const char* name = "CertificatePinned";
        using cb_type = void(const std::string& /*certId*/);
    };
    struct LIBJAMI_PUBLIC CertificatePathPinned
    {
        constexpr static const char* name = "CertificatePathPinned";
        using cb_type = void(const std::string& /*path*/,
                             const std::vector<std::string>& /*certId*/);
    };
    struct LIBJAMI_PUBLIC CertificateExpired
    {
        constexpr static const char* name = "CertificateExpired";
        using cb_type = void(const std::string& /*certId*/);
    };
    struct LIBJAMI_PUBLIC CertificateStateChanged
    {
        constexpr static const char* name = "CertificateStateChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*certId*/,
                             const std::string& /*state*/);
    };
    struct LIBJAMI_PUBLIC MediaParametersChanged
    {
        constexpr static const char* name = "MediaParametersChanged";
        using cb_type = void(const std::string& /*accountId*/);
    };
    struct LIBJAMI_PUBLIC MigrationEnded
    {
        constexpr static const char* name = "MigrationEnded";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /*state*/);
    };
    struct LIBJAMI_PUBLIC DeviceRevocationEnded
    {
        constexpr static const char* name = "DeviceRevocationEnded";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*device*/,
                             int /*status*/);
    };
    struct LIBJAMI_PUBLIC AccountProfileReceived
    {
        constexpr static const char* name = "AccountProfileReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& displayName,
                             const std::string& /*userPhoto*/);
    };
    /**
     * These are special getters for Android and UWP, so the daemon can retrieve
     * information only accessible through their respective platform APIs
     */
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    struct LIBJAMI_PUBLIC GetHardwareAudioFormat
    {
        constexpr static const char* name = "GetHardwareAudioFormat";
        using cb_type = void(std::vector<int32_t>* /* params_ret */);
    };
#endif
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    struct LIBJAMI_PUBLIC GetAppDataPath
    {
        constexpr static const char* name = "GetAppDataPath";
        using cb_type = void(const std::string& name, std::vector<std::string>* /* path_ret */);
    };
    struct LIBJAMI_PUBLIC GetDeviceName
    {
        constexpr static const char* name = "GetDeviceName";
        using cb_type = void(std::vector<std::string>* /* path_ret */);
    };
#endif
    struct LIBJAMI_PUBLIC HardwareDecodingChanged
    {
        constexpr static const char* name = "HardwareDecodingChanged";
        using cb_type = void(bool /* state */);
    };
    struct LIBJAMI_PUBLIC HardwareEncodingChanged
    {
        constexpr static const char* name = "HardwareEncodingChanged";
        using cb_type = void(bool /* state */);
    };
    struct LIBJAMI_PUBLIC MessageSend
    {
        constexpr static const char* name = "MessageSend";
        using cb_type = void(const std::string&);
    };
};

} // namespace libjami
