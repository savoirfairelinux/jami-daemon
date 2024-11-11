/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

%header %{
#include "jami/jami.h"
#include "jami/configurationmanager_interface.h"

class ConfigurationCallback {
public:
    virtual ~ConfigurationCallback(){}
    virtual void volumeChanged(const std::string& device, int value){}
    virtual void accountsChanged(void){}
    virtual void historyChanged(void){}
    virtual void stunStatusFailure(const std::string& account_id){}
    virtual void accountDetailsChanged(const std::string& account_id, const std::map<std::string, std::string>& details){}
    virtual void registrationStateChanged(const std::string& account_id, const std::string& state, int code, const std::string& detail_str){}
    virtual void volatileAccountDetailsChanged(const std::string& account_id, const std::map<std::string, std::string>& details){}
    virtual void incomingAccountMessage(const std::string& /*account_id*/, const std::string& /*from*/, const std::string& /*message_id*/, const std::map<std::string, std::string>& /*payload*/){}
    virtual void accountMessageStatusChanged(const std::string& /*account_id*/, const std::string& /*conversationId*/, const std::string& /*peer*/, const std::string& /*message_id*/, int /*state*/){}
    virtual void needsHost(const std::string& /*account_id*/, const std::string& /*conversationId*/){}
    virtual void activeCallsChanged(const std::string& /*account_id*/, const std::string& /*conversationId*/, const std::vector<std::map<std::string, std::string>>& /*activeCalls*/ ){}
    virtual void profileReceived(const std::string& /*account_id*/, const std::string& /*from*/, const std::string& /*path*/){}
    virtual void composingStatusChanged(const std::string& /*account_id*/, const std::string& /*convId*/, const std::string& /*from*/, int /*state*/){}
    virtual void knownDevicesChanged(const std::string& /*account_id*/, const std::map<std::string, std::string>& /*devices*/){}
    virtual void exportOnRingEnded(const std::string& /*account_id*/, int /*state*/, const std::string& /*pin*/){}

    virtual void incomingTrustRequest(const std::string& /*account_id*/, const std::string& /*conversationId*/, const std::string& /*from*/, const std::vector<uint8_t>& /*payload*/, time_t received){}
    virtual void contactAdded(const std::string& /*account_id*/, const std::string& /*uri*/, bool confirmed){}
    virtual void contactRemoved(const std::string& /*account_id*/, const std::string& /*uri*/, bool banned){}

    virtual void certificatePinned(const std::string& /*certId*/){}
    virtual void certificatePathPinned(const std::string& /*path*/, const std::vector<std::string>& /*certId*/){}
    virtual void certificateExpired(const std::string& /*certId*/){}
    virtual void certificateStateChanged(const std::string& /*account_id*/, const std::string& /*certId*/, const std::string& /*state*/){}

    virtual void errorAlert(int alert){}

    virtual void nameRegistrationEnded(const std::string& /*account_id*/, int state, const std::string& /*name*/){}
    virtual void registeredNameFound(const std::string& /*account_id*/, int state, const std::string& /*address*/, const std::string& /*name*/){}
    virtual void userSearchEnded(const std::string& /*account_id*/, int state, const std::string& /*query*/, const std::vector<std::map<std::string, std::string>>& /*results*/){}

    virtual void migrationEnded(const std::string& /*accountId*/, const std::string& /*state*/){}
    virtual void deviceRevocationEnded(const std::string& /*accountId*/, const std::string& /*device*/, int /*status*/){}
    virtual void accountProfileReceived(const std::string& /*accountId*/, const std::string& /*displayName*/, const std::string& /*photo*/){}

    virtual void hardwareDecodingChanged(bool /*state*/){}
    virtual void hardwareEncodingChanged(bool /*state*/){}

    virtual void audioMeter(const std::string& /*id*/, float /*level*/){}
    virtual void messageSend(const std::string& /*message*/){}
};
%}

%feature("director") ConfigurationCallback;

namespace libjami {

struct Message
{
    std::string from;
    std::map<std::string, std::string> payloads;
    uint64_t received;
};

std::map<std::string, std::string> getAccountDetails(const std::string& accountId);
std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountId);
void setAccountDetails(const std::string& accountId, const std::map<std::string, std::string>& details);
void setAccountActive(const std::string& accountId, bool active);
std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
void monitor(bool continuous);
std::vector<std::map<std::string, std::string>> getConnectionList(const std::string& accountId, const std::string& conversationId);
std::vector<std::map<std::string, std::string>> getChannelList(const std::string& accountId, const std::string& connectionId);
std::string addAccount(const std::map<std::string, std::string>& details);
void removeAccount(const std::string& accountId);
std::vector<std::string> getAccountList();
void sendRegister(const std::string& accountId, bool enable);
void registerAllAccounts(void);
uint64_t sendAccountTextMessage(const std::string& accountId, const std::string& to, const std::map<std::string, std::string>& message, const int32_t& flag);
std::vector<libjami::Message> getLastMessages(const std::string& accountId, uint64_t base_timestamp);
int getMessageStatus(uint64_t id);
int getMessageStatus(const std::string& accountId, uint64_t id);
bool cancelMessage(const std::string& accountId, uint64_t id);
void setIsComposing(const std::string& accountId, const std::string& conversationUri, bool isWriting);
bool setMessageDisplayed(const std::string& accountId, const std::string& conversationUri, const std::string& messageId, int status);
bool changeAccountPassword(const std::string& accountId, const std::string& password_old, const std::string& password_new);
bool isPasswordValid(const std::string& accountId, const std::string& password);
std::vector<uint8_t> getPasswordKey(const std::string& accountId, const std::string& password);

bool lookupName(const std::string& account, const std::string& nameserver, const std::string& name);
bool lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address);
bool registerName(const std::string& account, const std::string& name, const std::string& scheme, const std::string& password);
bool searchUser(const std::string& account, const std::string& query);

std::vector<unsigned> getCodecList();
std::vector<std::string> getSupportedTlsMethod();
std::vector<std::string> getSupportedCiphers(const std::string& accountId);
std::map<std::string, std::string> getCodecDetails(const std::string& accountId, const unsigned& codecId);
bool setCodecDetails(const std::string& accountId, const unsigned& codecId, const std::map<std::string, std::string>& details);
std::vector<unsigned> getActiveCodecList(const std::string& accountId);
bool exportOnRing(const std::string& accountId, const std::string& password);
bool exportToFile(const std::string& accountId, const std::string& destinationPath, const std::string& scheme, const std::string& password);

std::map<std::string, std::string> getKnownRingDevices(const std::string& accountId);
bool revokeDevice(const std::string& accountId, const std::string& deviceId, const std::string& scheme, const std::string& password);

void setActiveCodecList(const std::string& accountId, const std::vector<unsigned>& list);
std::vector<std::map<std::string, std::string>> getActiveCalls(const std::string& accountId, const std::string& convId);

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
std::string getNoiseSuppressState();
void setNoiseSuppressState(const std::string& state);

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
bool getRecordPreview();
void setRecordPreview(bool rec);
int32_t getRecordQuality();
void setRecordQuality(int32_t rec);

void setHistoryLimit(int32_t days);
int32_t getHistoryLimit();

void setRingingTimeout(int32_t timeout);
int32_t getRingingTimeout();

void setAccountsOrder(const std::string& order);

std::vector<std::map<std::string, std::string> > getCredentials(const std::string& accountId);
void setCredentials(const std::string& accountId, const std::vector<std::map<std::string, std::string> >& details);

std::string getAddrFromInterfaceName(const std::string& interface);

std::vector<std::string> getAllIpInterface();
std::vector<std::string> getAllIpInterfaceByName();

void setVolume(const std::string& device, double value);
double getVolume(const std::string& device);

/*
 * Security
 */
std::map<std::string, std::string> validateCertificatePath(const std::string& accountId,
                                                       const std::string& certificate,
                                                       const std::string& privateKey,
                                                       const std::string& privateKeyPassword,
                                                       const std::string& caList);

std::map<std::string, std::string> validateCertificate(const std::string& accountId, const std::string& certificate);

std::map<std::string, std::string> getCertificateDetails(const std::string& accountId, const std::string& certificate);
std::map<std::string, std::string> getCertificateDetailsPath(const std::string& accountId, const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass);

std::vector<std::string> getPinnedCertificates(const std::string& accountId);

std::vector<std::string> pinCertificate(const std::string& accountId, const std::vector<uint8_t>& certificate, bool local);
bool unpinCertificate(const std::string& accountId, const std::string& certId);

void pinCertificatePath(const std::string& accountId, const std::string& path);
unsigned unpinCertificatePath(const std::string& accountId, const std::string& path);

bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
bool setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
std::vector<std::string> getCertificatesByStatus(const std::string& account, const std::string& status);

/* contact requests */
std::vector<std::map<std::string, std::string>> getTrustRequests(const std::string& accountId);
bool acceptTrustRequest(const std::string& accountId, const std::string& from);
bool discardTrustRequest(const std::string& accountId, const std::string& from);
void sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload);

/* Contacts */

void addContact(const std::string& accountId, const std::string& uri);
void removeContact(const std::string& accountId, const std::string& uri, const bool& ban);
std::vector<std::map<std::string, std::string>> getContacts(const std::string& accountId);
std::map<std::string, std::string> getContactDetails(const std::string& accountId, const std::string& uri);

void connectivityChanged();

bool isAudioMeterActive(const std::string& id);
void setAudioMeterState(const std::string& id, bool state);

void setDefaultModerator(const std::string& accountId, const std::string& peerURI, bool state);
std::vector<std::string> getDefaultModerators(const std::string& accountId);
void enableLocalModerators(const std::string& accountId, bool isModEnabled);
bool isLocalModeratorsEnabled(const std::string& accountId);
void setAllModerators(const std::string& accountId, bool allModerators);
bool isAllModerators(const std::string& accountId);

}

class ConfigurationCallback {
public:
    virtual ~ConfigurationCallback(){}
    virtual void volumeChanged(const std::string& device, int value){}
    virtual void accountsChanged(void){}
    virtual void historyChanged(void){}
    virtual void stunStatusFailure(const std::string& account_id){}
    virtual void accountDetailsChanged(const std::string& account_id, const std::map<std::string, std::string>& details){}
    virtual void profileReceived(const std::string& /*account_id*/, const std::string& /*from*/, const std::string& /*path*/) {}
    virtual void registrationStateChanged(const std::string& account_id, const std::string& state, int code, const std::string& detail_str){}
    virtual void volatileAccountDetailsChanged(const std::string& account_id, const std::map<std::string, std::string>& details){}
    virtual void incomingAccountMessage(const std::string& /*account_id*/, const std::string& /*from*/, const std::string& /*message_id*/, const std::map<std::string, std::string>& /*payload*/){}
    virtual void accountMessageStatusChanged(const std::string& /*account_id*/, const std::string& /*conversationId*/, const std::string& /*peer*/, const std::string& /*message_id*/, int /*state*/){}
    virtual void needsHost(const std::string& /*account_id*/, const std::string& /*conversationId*/){}
    virtual void activeCallsChanged(const std::string& /*account_id*/, const std::string& /*conversationId*/, const std::vector<std::map<std::string, std::string>>& /*activeCalls*/ ){}
    virtual void composingStatusChanged(const std::string& /*account_id*/, const std::string& /*convId*/, const std::string& /*from*/, int /*state*/){}
    virtual void knownDevicesChanged(const std::string& /*account_id*/, const std::map<std::string, std::string>& /*devices*/){}
    virtual void exportOnRingEnded(const std::string& /*account_id*/, int /*state*/, const std::string& /*pin*/){}

    virtual void incomingTrustRequest(const std::string& /*account_id*/, const std::string& /*conversationId*/, const std::string& /*from*/, const std::vector<uint8_t>& /*payload*/, time_t received){}
    virtual void contactAdded(const std::string& /*account_id*/, const std::string& /*uri*/, bool confirmed){}
    virtual void contactRemoved(const std::string& /*account_id*/, const std::string& /*uri*/, bool banned){}

    virtual void certificatePinned(const std::string& /*certId*/){}
    virtual void certificatePathPinned(const std::string& /*path*/, const std::vector<std::string>& /*certId*/){}
    virtual void certificateExpired(const std::string& /*certId*/){}
    virtual void certificateStateChanged(const std::string& /*account_id*/, const std::string& /*certId*/, const std::string& /*state*/){}

    virtual void errorAlert(int alert){}

    virtual void nameRegistrationEnded(const std::string& /*account_id*/, int state, const std::string& /*name*/){}
    virtual void registeredNameFound(const std::string& /*account_id*/, int state, const std::string& /*address*/, const std::string& /*name*/){}
    virtual void userSearchEnded(const std::string& /*account_id*/, int state, const std::string& /*query*/, const std::vector<std::map<std::string, std::string>>& /*results*/){}

    virtual void migrationEnded(const std::string& /*accountId*/, const std::string& /*state*/){}
    virtual void deviceRevocationEnded(const std::string& /*accountId*/, const std::string& /*device*/, int /*status*/){}
    virtual void accountProfileReceived(const std::string& /*accountId*/, const std::string& /*displayName*/, const std::string& /*photo*/){}

    virtual void hardwareDecodingChanged(bool /*state*/){}
    virtual void hardwareEncodingChanged(bool /*state*/){}

    virtual void audioMeter(const std::string& /*id*/, float /*level*/){}
    virtual void messageSend(const std::string& /*message*/){}
};
