/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#ifndef __RING_DBUSCONFIGURATIONMANAGER_H__
#define __RING_DBUSCONFIGURATIONMANAGER_H__

#include <vector>
#include <map>
#include <string>

#include "dring/def.h"
#include "dbus_cpp.h"

#include "dring/datatransfer_interface.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbusconfigurationmanager.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

using RingDBusMessage = DBus::Struct<std::string, std::map<std::string, std::string>, uint64_t>;

class DRING_PUBLIC DBusConfigurationManager :
    public cx::ring::Ring::ConfigurationManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
{
    public:
        using RingDBusDataTransferInfo = DBus::Struct<std::string, uint32_t, uint32_t, int64_t, int64_t, std::string, std::string, std::string, std::string>;

        DBusConfigurationManager(DBus::Connection& connection);

        // Methods
        std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
        std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountID);
        void setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details);
        std::map<std::string, std::string> testAccountICEInitialization(const std::string& accountID);
        void setAccountActive(const std::string& accountID, const bool& active);
        std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
        std::string addAccount(const std::map<std::string, std::string>& details);
        bool exportOnRing(const std::string& accountID, const std::string& password);
        bool exportToFile(const std::string& accountID, const std::string& destinationPath, const std::string& password = {});
        bool revokeDevice(const std::string& accountID, const std::string& password, const std::string& device);
        std::map<std::string, std::string> getKnownRingDevices(const std::string& accountID);
        bool changeAccountPassword(const std::string& accountID, const std::string& password_old, const std::string& password_new);
        bool lookupName(const std::string& account, const std::string& nameserver, const std::string& name);
        bool lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address);
        bool registerName(const std::string& account, const std::string& password, const std::string& name);
        void removeAccount(const std::string& accoundID);
        std::vector<std::string> getAccountList();
        void sendRegister(const std::string& accoundID, const bool& enable);
        void registerAllAccounts(void);
        uint64_t sendTextMessage(const std::string& accoundID, const std::string& to, const std::map<std::string, std::string>& payloads);
        std::vector<RingDBusMessage> getLastMessages(const std::string& accountID, const uint64_t& base_timestamp);
        int getMessageStatus(const uint64_t& id);
        int getMessageStatus(const std::string& accountID, const uint64_t& id);
        bool cancelMessage(const std::string& accountID, const uint64_t& messageId);
        std::map<std::string, std::string> getTlsDefaultSettings();
        std::vector<std::string> getSupportedCiphers(const std::string& accountID);
        std::vector<unsigned> getCodecList();
        std::vector<std::string> getSupportedTlsMethod();
        std::map<std::string, std::string> getCodecDetails(const std::string& accountID, const unsigned& codecId);
        bool setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details);
        std::vector<unsigned> getActiveCodecList(const std::string& accountID);
        void setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list);
        std::vector<std::string> getAudioPluginList();
        void setAudioPlugin(const std::string& audioPlugin);
        std::vector<std::string> getAudioOutputDeviceList();
        void setAudioOutputDevice(const int32_t& index);
        void setAudioInputDevice(const int32_t& index);
        void setAudioRingtoneDevice(const int32_t& index);
        std::vector<std::string> getAudioInputDeviceList();
        std::vector<std::string> getCurrentAudioDevicesIndex();
        int32_t getAudioInputDeviceIndex(const std::string& name);
        int32_t getAudioOutputDeviceIndex(const std::string& name);
        std::string getCurrentAudioOutputPlugin();
        bool getNoiseSuppressState();
        void setNoiseSuppressState(const bool& state);
        bool isAgcEnabled();
        void setAgcState(const bool& enabled);
        void muteDtmf(const bool& mute);
        bool isDtmfMuted();
        bool isCaptureMuted();
        void muteCapture(const bool& mute);
        bool isPlaybackMuted();
        void mutePlayback(const bool& mute);
        bool isRingtoneMuted();
        void muteRingtone(const bool& mute);
        std::string getAudioManager();
        bool setAudioManager(const std::string& api);
        std::vector<std::string> getSupportedAudioManagers();
        std::string getRecordPath();
        void setRecordPath(const std::string& recPath);
        bool getIsAlwaysRecording();
        void setIsAlwaysRecording(const bool& rec);
        void setHistoryLimit(const int32_t& days);
        int32_t getHistoryLimit();
        void setRingingTimeout(const int32_t& timeout);
        int32_t getRingingTimeout();
        void setAccountsOrder(const std::string& order);
        std::map<std::string, std::string> getHookSettings();
        void setHookSettings(const std::map<std::string, std::string>& settings);
        std::vector<std::map<std::string, std::string>> getCredentials(const std::string& accountID);
        void setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details);
        std::string getAddrFromInterfaceName(const std::string& interface);
        std::vector<std::string> getAllIpInterface();
        std::vector<std::string> getAllIpInterfaceByName();
        std::map<std::string, std::string> getShortcuts();
        void setShortcuts(const std::map<std::string, std::string> &shortcutsMap);
        void setVolume(const std::string& device, const double& value);
        double getVolume(const std::string& device);
        std::map<std::string, std::string> validateCertificate(const std::string& accountId, const std::string& certificate);
        std::map<std::string, std::string> validateCertificatePath(const std::string& accountId, const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPass, const std::string& caList);
        std::map<std::string, std::string> getCertificateDetails(const std::string& certificate);
        std::map<std::string, std::string> getCertificateDetailsPath(const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPass);
        std::vector<std::string> getPinnedCertificates();
        std::vector<std::string> pinCertificate(const std::vector<uint8_t>& certificate, const bool& local);
        bool unpinCertificate(const std::string& certId);
        void pinCertificatePath(const std::string& path);
        unsigned unpinCertificatePath(const std::string& path);
        bool pinRemoteCertificate(const std::string& accountId, const std::string& certId);
        bool setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
        std::vector<std::string> getCertificatesByStatus(const std::string& account, const std::string& status);
        std::vector<std::map<std::string, std::string>> getTrustRequests(const std::string& accountId);
        bool acceptTrustRequest(const std::string& accountId, const std::string& from);
        bool discardTrustRequest(const std::string& accountId, const std::string& from);
        void sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload);
        void addContact(const std::string& accountId, const std::string& uri);
        void removeContact(const std::string& accountId, const std::string& uri, const bool& ban);
        std::map<std::string, std::string> getContactDetails(const std::string& accountId, const std::string& uri);
        std::vector<std::map<std::string, std::string>> getContacts(const std::string& accountId);
        int exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password);
        int importAccounts(const std::string& archivePath, const std::string& password);
        void connectivityChanged();
        std::vector<uint64_t> dataTransferList();
        void sendFile(const RingDBusDataTransferInfo& info, uint32_t& error, DRing::DataTransferId& id);
        void dataTransferInfo(const DRing::DataTransferId& id, uint32_t& error, RingDBusDataTransferInfo& info);
        void dataTransferBytesProgress(const uint64_t& id, uint32_t& error, int64_t& total, int64_t& progress);
        uint32_t acceptFileTransfer(const uint64_t& id, const std::string& file_path, const int64_t& offset);
        uint32_t cancelDataTransfer(const uint64_t& id);

        bool isAudioMeterActive(const std::string& id);
        void setAudioMeterState(const std::string& id, const bool& state);
};

#endif // __RING_DBUSCONFIGURATIONMANAGER_H__
