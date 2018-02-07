/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
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

void registerConfHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountID);
void setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details);
std::map<std::string, std::string> testAccountICEInitialization(const std::string& accountID);
void setAccountActive(const std::string& accountID, bool active);
std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
std::string addAccount(const std::map<std::string, std::string>& details);
bool exportOnRing(const std::string& accountID, const std::string& password);
bool revokeDevice(const std::string& accountID, const std::string& password, const std::string& deviceID);
std::map<std::string, std::string> getKnownRingDevices(const std::string& accountID);
bool changeAccountPassword(const std::string& accountID, const std::string& password_old, const std::string& password_new);

bool lookupName(const std::string& account, const std::string& nameserver, const std::string& name);
bool lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address);
bool registerName(const std::string& account, const std::string& password, const std::string& name);

void removeAccount(const std::string& accountID);
void setAccountEnabled(const std::string& accountID, bool enable);
std::vector<std::string> getAccountList();
void sendRegister(const std::string& accountID, bool enable);
void registerAllAccounts(void);
uint64_t sendAccountTextMessage(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& payloads);
int getMessageStatus(uint64_t id);


std::map<std::string, std::string> getTlsDefaultSettings();

std::vector<unsigned> getCodecList();
std::vector<std::string> getSupportedTlsMethod();
std::vector<std::string> getSupportedCiphers(const std::string& accountID);
std::map<std::string, std::string> getCodecDetails(const std::string& accountID, const unsigned& codecId);
bool setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details);
std::vector<unsigned> getActiveCodecList(const std::string& accountID);

void setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list);

std::vector<std::string> getAudioPluginList();
void setAudioPlugin(const std::string& audioPlugin);
std::vector<std::string> getAudioOutputDeviceList();
void setAudioOutputDevice(int32_t index);
void setAudioInputDevice(int32_t index);
void setAudioRingtoneDevice(int32_t index);
std::vector<std::string> getAudioInputDeviceList();
std::vector<std::string> getCurrentAudioDevicesIndex();
int32_t getAudioInputDeviceIndex(const std::string& name);
int32_t getAudioOutputDeviceIndex(const std::string& name);
std::string getCurrentAudioOutputPlugin();
bool getNoiseSuppressState();
void setNoiseSuppressState(bool state);

bool isAgcEnabled();
void setAgcState(bool enabled);

void muteDtmf(bool mute);
bool isDtmfMuted();

bool isCaptureMuted();
void muteCapture(bool mute);
bool isPlaybackMuted();
void mutePlayback(bool mute);
bool isRingtoneMuted();
void muteRingtone(bool mute);

std::string getAudioManager();
bool setAudioManager(const std::string& api);

std::string getRecordPath();
void setRecordPath(const std::string& recPath);
bool getIsAlwaysRecording();
void setIsAlwaysRecording(bool rec);

void setHistoryLimit(int32_t days);
int32_t getHistoryLimit();

void setAccountsOrder(const std::string& order);

std::map<std::string, std::string> getHookSettings();
void setHookSettings(const std::map<std::string, std::string>& settings);

std::vector<std::map<std::string, std::string>> getCredentials(const std::string& accountID);
void setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details);

std::string getAddrFromInterfaceName(const std::string& iface);

std::vector<std::string> getAllIpInterface();
std::vector<std::string> getAllIpInterfaceByName();

std::map<std::string, std::string> getShortcuts();
void setShortcuts(const std::map<std::string, std::string> &shortcutsMap);

void setVolume(const std::string& device, double value);
double getVolume(const std::string& device);

/*
 * Security
 */
std::map<std::string, std::string> validateCertificate(const std::string& accountId, const std::string& certificate);
std::map<std::string, std::string> validateCertificatePath(const std::string& accountId,
    const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPassword, const std::string& caList);

std::map<std::string, std::string> getCertificateDetails(const std::string& certificate);
std::map<std::string, std::string> getCertificateDetailsPath(const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPassword);

std::vector<std::string> getPinnedCertificates();

std::vector<std::string> pinCertificate(const std::vector<uint8_t>& certificate, bool local);
bool unpinCertificate(const std::string& certId);

void pinCertificatePath(const std::string& path);
unsigned unpinCertificatePath(const std::string& path);

bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
bool setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
std::vector<std::string> getCertificatesByStatus(const std::string& account, const std::string& status);

/* contact requests */
std::vector<std::map<std::string, std::string>> getTrustRequests(const std::string& accountId);
bool acceptTrustRequest(const std::string& accountId, const std::string& from);
bool discardTrustRequest(const std::string& accountId, const std::string& from);
void sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload = {});

/* Contacts */

void addContact(const std::string& accountId, const std::string& uri);
void removeContact(const std::string& accountId, const std::string& uri, bool ban);
std::map<std::string, std::string> getContactDetails(const std::string& accountId, const std::string& uri);
std::vector<std::map<std::string, std::string>> getContacts(const std::string& accountId);

/*
 * Import/Export accounts
 */
int exportAccounts(std::vector<std::string> accountIDs, std::string filepath, std::string password);
int importAccounts(std::string archivePath, std::string password);

/*
 * Network connectivity
 */
void connectivityChanged();

/* Dht proxy */

/**
 * Start or stop to use the proxy for account
 */
void enableProxyClient(const std::string& accountID, bool enable);

/**
 * Set the device push notification token (for all accounts).
 * If set, proxy clients will use push notifications.
 * Set to empty to disable push notifications.
 */
void setPushNotificationToken(const std::string& pushDeviceToken);

/**
 * To be called by clients with relevent data when a push notification is received.
 */
void pushNotificationReceived(const std::string& from, const std::map<std::string, std::string>& data);

struct AudioSignal {
        struct DeviceEvent {
                constexpr static const char* name = "audioDeviceEvent";
                using cb_type = void(void);
        };
};

// Configuration signal type definitions
struct ConfigurationSignal {
        struct VolumeChanged {
                constexpr static const char* name = "VolumeChanged";
                using cb_type = void(const std::string& /*device*/, double /*value*/);
        };
        struct AccountsChanged {
                constexpr static const char* name = "AccountsChanged";
                using cb_type = void(void);
        };
        struct Error {
                constexpr static const char* name = "Error";
                using cb_type = void(int /*alert*/);
        };

        // TODO: move those to AccountSignal in next API breakage
        struct AccountDetailsChanged {
                constexpr static const char* name = "AccountDetailsChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::map<std::string, std::string>& /* details */);
        };
        struct StunStatusFailed {
                constexpr static const char* name = "StunStatusFailed";
                using cb_type = void(const std::string& /*account_id*/);
        };
        struct RegistrationStateChanged {
                constexpr static const char* name = "RegistrationStateChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*state*/, int /*detailsCode*/, const std::string& /*detailsStr*/);
        };
        struct VolatileDetailsChanged {
                constexpr static const char* name = "VolatileDetailsChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::map<std::string, std::string>& /* details */);
        };
        struct IncomingAccountMessage {
                constexpr static const char* name = "IncomingAccountMessage";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*from*/, const std::map<std::string, std::string>& /*payloads*/);
        };
        struct AccountMessageStatusChanged {
                constexpr static const char* name = "AccountMessageStatusChanged";
                using cb_type = void(const std::string& /*account_id*/, uint64_t /*message_id*/, const std::string& /*to*/, int /*state*/);
        };
        struct IncomingTrustRequest {
                constexpr static const char* name = "IncomingTrustRequest";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*from*/, const std::vector<uint8_t>& payload, time_t received);
        };
        struct ContactAdded {
                constexpr static const char* name = "ContactAdded";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*uri*/, bool confirmed);
        };
        struct ContactRemoved {
                constexpr static const char* name = "ContactRemoved";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*uri*/, bool banned);
        };
        struct ExportOnRingEnded {
                constexpr static const char* name = "ExportOnRingEnded";
                using cb_type = void(const std::string& /*account_id*/, int state, const std::string& pin);
        };
        struct NameRegistrationEnded {
                constexpr static const char* name = "NameRegistrationEnded";
                using cb_type = void(const std::string& /*account_id*/, int state, const std::string& name);
        };
        struct KnownDevicesChanged {
                constexpr static const char* name = "KnownDevicesChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::map<std::string, std::string>& devices);
        };
        struct RegisteredNameFound {
                constexpr static const char* name = "RegisteredNameFound";
                using cb_type = void(const std::string& /*account_id*/, int state, const std::string& /*address*/, const std::string& /*name*/);
        };
        struct CertificatePinned {
                constexpr static const char* name = "CertificatePinned";
                using cb_type = void(const std::string& /*certId*/);
        };
        struct CertificatePathPinned {
                constexpr static const char* name = "CertificatePathPinned";
                using cb_type = void(const std::string& /*path*/, const std::vector<std::string>& /*certId*/);
        };
        struct CertificateExpired {
                constexpr static const char* name = "CertificateExpired";
                using cb_type = void(const std::string& /*certId*/);
        };
        struct CertificateStateChanged {
                constexpr static const char* name = "CertificateStateChanged";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*certId*/, const std::string& /*state*/);
        };
        struct MediaParametersChanged {
                constexpr static const char* name = "MediaParametersChanged";
                using cb_type = void(const std::string& /*accountId*/);
        };
        struct MigrationEnded {
                constexpr static const char* name = "MigrationEnded";
                using cb_type = void(const std::string& /*accountId*/, const std::string& /*state*/);
        };
        struct DeviceRevocationEnded {
                constexpr static const char* name = "DeviceRevocationEnded";
                using cb_type = void(const std::string& /*accountId*/, const std::string& /*device*/, int /*status*/);
        };
        /**
         * These are special getters for Android and UWP, so the daemon can retreive
         * information only accessible through their respective platform APIs
         */
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        struct GetHardwareAudioFormat {
                constexpr static const char* name = "GetHardwareAudioFormat";
                using cb_type = void(std::vector<int32_t>* /* params_ret */);
        };
#endif
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        struct GetAppDataPath {
                constexpr static const char* name = "GetAppDataPath";
                using cb_type = void(const std::string& name, std::vector<std::string>* /* path_ret */);
        };
        struct GetDeviceName {
            constexpr static const char* name = "GetDeviceName";
            using cb_type = void(std::vector<std::string>* /* path_ret */);
        };
#endif
};

// Can be used when a client's stdout is not available
struct DebugSignal {
    struct MessageSend {
        constexpr static const char* name = "MessageSend";
        using cb_type = void(const std::string&);
    };
};

} // namespace DRing
