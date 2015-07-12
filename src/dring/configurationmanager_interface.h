/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#ifndef DRING_CONFIGURATIONMANAGERI_H
#define DRING_CONFIGURATIONMANAGERI_H

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <cstdint>

#include "dring.h"
#include "security_const.h"

namespace DRing {

void registerConfHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountID);
void setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details);
std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
std::string addAccount(const std::map<std::string, std::string>& details);
void removeAccount(const std::string& accountID);
std::vector<std::string> getAccountList();
void sendRegister(const std::string& accountID, bool enable);
void registerAllAccounts(void);
void sendAccountTextMessage(const std::string& accountID, const std::string& to, const std::string& message);

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

std::string getAudioManager();
bool setAudioManager(const std::string& api);

int32_t isIax2Enabled();
std::string getRecordPath();
void setRecordPath(const std::string& recPath);
bool getIsAlwaysRecording();
void setIsAlwaysRecording(bool rec);

void setHistoryLimit(int32_t days);
int32_t getHistoryLimit();

void setAccountsOrder(const std::string& order);

std::map<std::string, std::string> getHookSettings();
void setHookSettings(const std::map<std::string, std::string>& settings);

std::map<std::string, std::string> getIp2IpDetails();

std::vector<std::map<std::string, std::string>> getCredentials(const std::string& accountID);
void setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details);

std::string getAddrFromInterfaceName(const std::string& interface);

std::vector<std::string> getAllIpInterface();
std::vector<std::string> getAllIpInterfaceByName();

std::map<std::string, std::string> getShortcuts();
void setShortcuts(const std::map<std::string, std::string> &shortcutsMap);

void setVolume(const std::string& device, double value);
double getVolume(const std::string& device);

/*
 * Security
 */
std::map<std::string, std::string> validateCertificate(const std::string& accountId,
    const std::string& certificate/*, const std::vector<std::string>& caList*/);
std::map<std::string, std::string> validateCertificatePath(const std::string& accountId,
    const std::string& certificatePath, const std::string& privateKey, const std::string& caList);

std::map<std::string, std::string> getCertificateDetails(const std::string& certificate);
std::map<std::string, std::string> getCertificateDetailsPath(const std::string& certificatePath);

std::vector<std::string> getPinnedCertificates();

std::vector<std::string> pinCertificate(const std::vector<uint8_t>& certificate, bool local);
bool unpinCertificate(const std::string& certId);

void pinCertificatePath(const std::string& path);
unsigned unpinCertificatePath(const std::string& path);

bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
bool setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
std::vector<std::string> getCertificatesByStatus(const std::string& account, const std::string& status);

/* contact requests */
std::map<std::string, std::string> getTrustRequests(const std::string& accountId);
bool acceptTrustRequest(const std::string& accountId, const std::string& from);
bool discardTrustRequest(const std::string& accountId, const std::string& from);

void sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload = {});

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
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*from*/, const std::string& /*message*/);
        };
        struct IncomingTrustRequest {
                constexpr static const char* name = "IncomingTrustRequest";
                using cb_type = void(const std::string& /*account_id*/, const std::string& /*from*/, const std::vector<uint8_t>& payload, time_t received);
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
#ifdef __ANDROID__
        /**
         * These are special getters for Android so the daemon can retreive
         * some informations only accessible through Java APIs
         */
        struct GetHardwareAudioFormat {
                constexpr static const char* name = "GetHardwareAudioFormat";
                using cb_type = void(std::vector<int32_t>* /* params_ret */);
        };
        struct GetAppDataPath {
                constexpr static const char* name = "GetAppDataPath";
                using cb_type = void(const std::string& name, std::vector<std::string>* /* path_ret */);
        };
#endif
};

} // namespace DRing

#endif // DRING_CONFIGURATIONMANAGERI_H
