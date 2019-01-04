/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "dring.h"
#include "security_const.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace DRing {

[[deprecated("Replaced by registerSignalHandlers")]] DRING_PUBLIC
void registerConfHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

struct DRING_PUBLIC Message
{
    std::string from;
    std::map<std::string, std::string> payloads;
    uint64_t received;
};

DRING_PUBLIC std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
DRING_PUBLIC std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountID);
DRING_PUBLIC void setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details);
DRING_PUBLIC std::map<std::string, std::string> testAccountICEInitialization(const std::string& accountID);
DRING_PUBLIC void setAccountActive(const std::string& accountID, bool active);
DRING_PUBLIC std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
DRING_PUBLIC std::string addAccount(const std::map<std::string, std::string>& details);
DRING_PUBLIC bool exportOnRing(const std::string& accountID, const std::string& password);
DRING_PUBLIC bool exportToFile(const std::string& accountID, const std::string& destinationPath, const std::string& password = {});
DRING_PUBLIC bool revokeDevice(const std::string& accountID, const std::string& password, const std::string& deviceID);
DRING_PUBLIC std::map<std::string, std::string> getKnownRingDevices(const std::string& accountID);
DRING_PUBLIC bool changeAccountPassword(const std::string& accountID, const std::string& password_old, const std::string& password_new);

DRING_PUBLIC bool lookupName(const std::string& account, const std::string& nameserver, const std::string& name);
DRING_PUBLIC bool lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address);
DRING_PUBLIC bool registerName(const std::string& account, const std::string& password, const std::string& name);

DRING_PUBLIC void removeAccount(const std::string& accountID);
DRING_PUBLIC void setAccountEnabled(const std::string& accountID, bool enable);
DRING_PUBLIC std::vector<std::string> getAccountList();
DRING_PUBLIC void sendRegister(const std::string& accountID, bool enable);
DRING_PUBLIC void registerAllAccounts(void);
DRING_PUBLIC uint64_t sendAccountTextMessage(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& payloads);
DRING_PUBLIC std::vector<Message> getLastMessages(const std::string& accountID, const uint64_t& base_timestamp);
DRING_PUBLIC int getMessageStatus(uint64_t id);


DRING_PUBLIC std::map<std::string, std::string> getTlsDefaultSettings();

DRING_PUBLIC std::vector<unsigned> getCodecList();
DRING_PUBLIC std::vector<std::string> getSupportedTlsMethod();
DRING_PUBLIC std::vector<std::string> getSupportedCiphers(const std::string& accountID);
DRING_PUBLIC std::map<std::string, std::string> getCodecDetails(const std::string& accountID, const unsigned& codecId);
DRING_PUBLIC bool setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details);
DRING_PUBLIC std::vector<unsigned> getActiveCodecList(const std::string& accountID);

DRING_PUBLIC void setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list);

DRING_PUBLIC std::vector<std::string> getAudioPluginList();
DRING_PUBLIC void setAudioPlugin(const std::string& audioPlugin);
DRING_PUBLIC std::vector<std::string> getAudioOutputDeviceList();
DRING_PUBLIC void setAudioOutputDevice(int32_t index);
DRING_PUBLIC void setAudioInputDevice(int32_t index);
DRING_PUBLIC void setAudioRingtoneDevice(int32_t index);
DRING_PUBLIC std::vector<std::string> getAudioInputDeviceList();
DRING_PUBLIC std::vector<std::string> getCurrentAudioDevicesIndex();
DRING_PUBLIC int32_t getAudioInputDeviceIndex(const std::string& name);
DRING_PUBLIC int32_t getAudioOutputDeviceIndex(const std::string& name);
DRING_PUBLIC std::string getCurrentAudioOutputPlugin();
DRING_PUBLIC bool getNoiseSuppressState();
DRING_PUBLIC void setNoiseSuppressState(bool state);

DRING_PUBLIC bool isAgcEnabled();
DRING_PUBLIC void setAgcState(bool enabled);

DRING_PUBLIC void muteDtmf(bool mute);
DRING_PUBLIC bool isDtmfMuted();

DRING_PUBLIC bool isCaptureMuted();
DRING_PUBLIC void muteCapture(bool mute);
DRING_PUBLIC bool isPlaybackMuted();
DRING_PUBLIC void mutePlayback(bool mute);
DRING_PUBLIC bool isRingtoneMuted();
DRING_PUBLIC void muteRingtone(bool mute);

DRING_PUBLIC std::string getAudioManager();
DRING_PUBLIC bool setAudioManager(const std::string& api);

DRING_PUBLIC std::string getRecordPath();
DRING_PUBLIC void setRecordPath(const std::string& recPath);
DRING_PUBLIC bool getIsAlwaysRecording();
DRING_PUBLIC void setIsAlwaysRecording(bool rec);

DRING_PUBLIC void setHistoryLimit(int32_t days);
DRING_PUBLIC int32_t getHistoryLimit();

DRING_PUBLIC void setRingingTimeout(int32_t timeout);
DRING_PUBLIC int32_t getRingingTimeout();

DRING_PUBLIC void setAccountsOrder(const std::string& order);

DRING_PUBLIC std::map<std::string, std::string> getHookSettings();
DRING_PUBLIC void setHookSettings(const std::map<std::string, std::string>& settings);

DRING_PUBLIC std::vector<std::map<std::string, std::string>> getCredentials(const std::string& accountID);
DRING_PUBLIC void setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details);

DRING_PUBLIC std::string getAddrFromInterfaceName(const std::string& iface);

DRING_PUBLIC std::vector<std::string> getAllIpInterface();
DRING_PUBLIC std::vector<std::string> getAllIpInterfaceByName();

DRING_PUBLIC std::map<std::string, std::string> getShortcuts();
DRING_PUBLIC void setShortcuts(const std::map<std::string, std::string> &shortcutsMap);

DRING_PUBLIC void setVolume(const std::string& device, double value);
DRING_PUBLIC double getVolume(const std::string& device);

/*
 * Security
 */
DRING_PUBLIC std::map<std::string, std::string> validateCertificate(const std::string& accountId, const std::string& certificate);
DRING_PUBLIC std::map<std::string, std::string> validateCertificatePath(const std::string& accountId,
    const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPassword, const std::string& caList);

DRING_PUBLIC std::map<std::string, std::string> getCertificateDetails(const std::string& certificate);
DRING_PUBLIC std::map<std::string, std::string> getCertificateDetailsPath(const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPassword);

DRING_PUBLIC std::vector<std::string> getPinnedCertificates();

DRING_PUBLIC std::vector<std::string> pinCertificate(const std::vector<uint8_t>& certificate, bool local);
DRING_PUBLIC bool unpinCertificate(const std::string& certId);

DRING_PUBLIC void pinCertificatePath(const std::string& path);
DRING_PUBLIC unsigned unpinCertificatePath(const std::string& path);

DRING_PUBLIC bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
DRING_PUBLIC bool setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
DRING_PUBLIC std::vector<std::string> getCertificatesByStatus(const std::string& account, const std::string& status);

/* contact requests */
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getTrustRequests(const std::string& accountId);
DRING_PUBLIC bool acceptTrustRequest(const std::string& accountId, const std::string& from);
DRING_PUBLIC bool discardTrustRequest(const std::string& accountId, const std::string& from);
DRING_PUBLIC void sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload = {});

/* Contacts */

DRING_PUBLIC void addContact(const std::string& accountId, const std::string& uri);
DRING_PUBLIC void removeContact(const std::string& accountId, const std::string& uri, bool ban);
DRING_PUBLIC std::map<std::string, std::string> getContactDetails(const std::string& accountId, const std::string& uri);
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getContacts(const std::string& accountId);

/*
 * Import/Export accounts
 */
DRING_PUBLIC int exportAccounts(std::vector<std::string> accountIDs, std::string filepath, std::string password);
DRING_PUBLIC int importAccounts(std::string archivePath, std::string password);

/*
 * Network connectivity
 */
DRING_PUBLIC void connectivityChanged();

/* Dht proxy */

/**
 * Start or stop to use the proxy for account
 */
DRING_PUBLIC void enableProxyClient(const std::string& accountID, bool enable);

/**
 * Set the device push notification token (for all accounts).
 * If set, proxy clients will use push notifications.
 * Set to empty to disable push notifications.
 */
DRING_PUBLIC void setPushNotificationToken(const std::string& pushDeviceToken);

/**
 * To be called by clients with relevant data when a push notification is received.
 */
DRING_PUBLIC void pushNotificationReceived(const std::string& from, const std::map<std::string, std::string>& data);

struct DRING_PUBLIC AudioSignal {
        struct DRING_PUBLIC DeviceEvent {
                constexpr static const char* name = "audioDeviceEvent";
                using cb_type = void(void);
        };
};

// Configuration signal type definitions
struct DRING_PUBLIC ConfigurationSignal {
        struct DRING_PUBLIC VolumeChanged {
                constexpr static const char* name = "VolumeChanged";
                using cb_type = void(const std::string& /*device*/, double /*value*/);
        };
        struct DRING_PUBLIC AccountsChanged {
                constexpr static const char* name = "AccountsChanged";
                using cb_type = void(void);
        };
        struct DRING_PUBLIC Error {
                constexpr static const char* name = "Error";
                using cb_type = void(int /*alert*/);
        };

        // TODO: move those to AccountSignal in next API breakage
        struct DRING_PUBLIC AccountDetailsChanged {
                constexpr static const char* name = "AccountDetailsChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::map<std::string, std::string>& /* details */);
        };
        struct DRING_PUBLIC StunStatusFailed {
                constexpr static const char* name = "StunStatusFailed";
                using cb_type = void(const std::string& /*account_id*/);
        };
        struct DRING_PUBLIC RegistrationStateChanged {
                constexpr static const char* name = "RegistrationStateChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*state*/, int /*detailsCode*/, const std::string& /*detailsStr*/);
        };
        struct DRING_PUBLIC VolatileDetailsChanged {
                constexpr static const char* name = "VolatileDetailsChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::map<std::string, std::string>& /* details */);
        };
        struct DRING_PUBLIC IncomingAccountMessage {
                constexpr static const char* name = "IncomingAccountMessage";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*from*/, const std::map<std::string, std::string>& /*payloads*/);
        };
        struct DRING_PUBLIC AccountMessageStatusChanged {
                constexpr static const char* name = "AccountMessageStatusChanged";
                using cb_type = void(const std::string& /*account_id*/, uint64_t /*message_id*/, const std::string& /*to*/, int /*state*/);
        };
        struct DRING_PUBLIC IncomingTrustRequest {
                constexpr static const char* name = "IncomingTrustRequest";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*from*/, const std::vector<uint8_t>& payload, time_t received);
        };
        struct DRING_PUBLIC ContactAdded {
                constexpr static const char* name = "ContactAdded";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*uri*/, bool confirmed);
        };
        struct DRING_PUBLIC ContactRemoved {
                constexpr static const char* name = "ContactRemoved";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*uri*/, bool banned);
        };
        struct DRING_PUBLIC ExportOnRingEnded {
                constexpr static const char* name = "ExportOnRingEnded";
                using cb_type = void(const std::string& /*account_id*/, int state, const std::string& pin);
        };
        struct DRING_PUBLIC NameRegistrationEnded {
                constexpr static const char* name = "NameRegistrationEnded";
                using cb_type = void(const std::string& /*account_id*/, int state, const std::string& name);
        };
        struct DRING_PUBLIC KnownDevicesChanged {
                constexpr static const char* name = "KnownDevicesChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::map<std::string, std::string>& devices);
        };
        struct DRING_PUBLIC RegisteredNameFound {
                constexpr static const char* name = "RegisteredNameFound";
                using cb_type = void(const std::string& /*account_id*/, int state, const std::string& /*address*/, const std::string& /*name*/);
        };
        struct DRING_PUBLIC CertificatePinned {
                constexpr static const char* name = "CertificatePinned";
                using cb_type = void(const std::string& /*certId*/);
        };
        struct DRING_PUBLIC CertificatePathPinned {
                constexpr static const char* name = "CertificatePathPinned";
                using cb_type = void(const std::string& /*path*/, const std::vector<std::string>& /*certId*/);
        };
        struct DRING_PUBLIC CertificateExpired {
                constexpr static const char* name = "CertificateExpired";
                using cb_type = void(const std::string& /*certId*/);
        };
        struct DRING_PUBLIC CertificateStateChanged {
                constexpr static const char* name = "CertificateStateChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*certId*/, const std::string& /*state*/);
        };
        struct DRING_PUBLIC MediaParametersChanged {
                constexpr static const char* name = "MediaParametersChanged";
                using cb_type = void(const std::string& /*accountId*/);
        };
        struct DRING_PUBLIC MigrationEnded {
                constexpr static const char* name = "MigrationEnded";
                using cb_type = void(const std::string& /*accountId*/, const std::string& /*state*/);
        };
        struct DRING_PUBLIC DeviceRevocationEnded {
                constexpr static const char* name = "DeviceRevocationEnded";
                using cb_type = void(const std::string& /*accountId*/, const std::string& /*device*/, int /*status*/);
        };
        /**
         * These are special getters for Android and UWP, so the daemon can retrieve
         * information only accessible through their respective platform APIs
         */
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        struct DRING_PUBLIC GetHardwareAudioFormat {
                constexpr static const char* name = "GetHardwareAudioFormat";
                using cb_type = void(std::vector<int32_t>* /* params_ret */);
        };
#endif
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        struct DRING_PUBLIC GetAppDataPath {
                constexpr static const char* name = "GetAppDataPath";
                using cb_type = void(const std::string& name, std::vector<std::string>* /* path_ret */);
        };
        struct DRING_PUBLIC GetDeviceName {
            constexpr static const char* name = "GetDeviceName";
            using cb_type = void(std::vector<std::string>* /* path_ret */);
        };
#endif
};

// Can be used when a client's stdout is not available
struct DRING_PUBLIC DebugSignal {
    struct DRING_PUBLIC MessageSend {
        constexpr static const char* name = "MessageSend";
        using cb_type = void(const std::string&);
    };
};

} // namespace DRing
