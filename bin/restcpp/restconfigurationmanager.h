/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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
#include <string>
#include <regex>
#include <restbed>
#include <mutex>

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef ENABLE_VIDEO
#include "dring/videomanager_interface.h"
#endif
#include "logger.h"
#include "im/message_engine.h"

#pragma GCC diagnostic warning "-Wignored-qualifiers"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class RestConfigurationManager
{
    public:
        RestConfigurationManager();

        std::vector<std::shared_ptr<restbed::Resource>> getResources();

        std::set<std::shared_ptr<restbed::Session>> getPendingNameResolutions(const std::string& name);


    private:
        // Attributes
        std::vector<std::shared_ptr<restbed::Resource>> resources_;
        std::multimap<std::string, std::shared_ptr<restbed::Session>> pendingNameResolutions;
        std::mutex pendingNameResolutionMtx;
        // Methods
        std::map<std::string, std::string> parsePost(const std::string& post);
        void populateResources();
        void addPendingNameResolutions(const std::string& name, const std::shared_ptr<restbed::Session> session);
        void defaultRoute(const std::shared_ptr<restbed::Session> session);


        void getAccountDetails(const std::shared_ptr<restbed::Session> session);
        void getVolatileAccountDetails(const std::shared_ptr<restbed::Session> session);
        void setAccountDetails(const std::shared_ptr<restbed::Session> session);
        void registerName(const std::shared_ptr<restbed::Session> session);
        void lookupName(const std::shared_ptr<restbed::Session> session);
        void setAccountActive(const std::shared_ptr<restbed::Session> session);
        void getAccountTemplate(const std::shared_ptr<restbed::Session> session);
        void addAccount(const std::shared_ptr<restbed::Session> session);
        void removeAccount(const std::shared_ptr<restbed::Session> session);
        void getAccountList(const std::shared_ptr<restbed::Session> session);
        void sendRegister(const std::shared_ptr<restbed::Session> session);
        void registerAllAccounts(const std::shared_ptr<restbed::Session> session);
        void sendTextMessage(const std::shared_ptr<restbed::Session> session);
        void getMessageStatus(const std::shared_ptr<restbed::Session> session);
        void getTlsDefaultSettings(const std::shared_ptr<restbed::Session> session);
        void getCodecList(const std::shared_ptr<restbed::Session> session);
        void getSupportedTlsMethod(const std::shared_ptr<restbed::Session> session);
        void getSupportedCiphers(const std::shared_ptr<restbed::Session> session);
        void getCodecDetails(const std::shared_ptr<restbed::Session> session);
        void setCodecDetails(const std::shared_ptr<restbed::Session> session);
        void getActiveCodecList(const std::shared_ptr<restbed::Session> session);
        void setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list); // /!\ not implemented
        void getAudioPluginList(const std::shared_ptr<restbed::Session> session);
        void setAudioPlugin(const std::shared_ptr<restbed::Session> session);
        void getAudioOutputDeviceList(const std::shared_ptr<restbed::Session> session);
        void setAudioOutputDevice(const std::shared_ptr<restbed::Session> session);
        void setAudioInputDevice(const std::shared_ptr<restbed::Session> session);
        void setAudioRingtoneDevice(const std::shared_ptr<restbed::Session> session);
        void getAudioInputDeviceList(const std::shared_ptr<restbed::Session> session);
        void getCurrentAudioDevicesIndex(const std::shared_ptr<restbed::Session> session);
        void getAudioInputDeviceIndex(const std::shared_ptr<restbed::Session> session);
        void getAudioOutputDeviceIndex(const std::shared_ptr<restbed::Session> session);
        void getCurrentAudioOutputPlugin(const std::shared_ptr<restbed::Session> session);
        void getNoiseSuppressState(const std::shared_ptr<restbed::Session> session);
        void setNoiseSuppressState(const std::shared_ptr<restbed::Session> session);
        void isAgcEnabled(const std::shared_ptr<restbed::Session> session);
        void setAgcState(const std::shared_ptr<restbed::Session> session);
        void muteDtmf(const std::shared_ptr<restbed::Session> session);
        void isDtmfMuted(const std::shared_ptr<restbed::Session> session);
        void isCaptureMuted(const std::shared_ptr<restbed::Session> session);
        void muteCapture(const std::shared_ptr<restbed::Session> session);
        void isPlaybackMuted(const std::shared_ptr<restbed::Session> session);
        void mutePlayback(const std::shared_ptr<restbed::Session> session);
        void isRingtoneMuted(const std::shared_ptr<restbed::Session> session);
        void muteRingtone(const std::shared_ptr<restbed::Session> session);
        void getAudioManager(const std::shared_ptr<restbed::Session> session);
        void setAudioManager(const std::shared_ptr<restbed::Session> session);
        void getSupportedAudioManagers(const std::shared_ptr<restbed::Session> session);
        void getRecordPath(const std::shared_ptr<restbed::Session> session);
        void setRecordPath(const std::shared_ptr<restbed::Session> session);
        void getIsAlwaysRecording(const std::shared_ptr<restbed::Session> session);
        void setIsAlwaysRecording(const std::shared_ptr<restbed::Session> session);
        void setHistoryLimit(const std::shared_ptr<restbed::Session> session);
        void getHistoryLimit(const std::shared_ptr<restbed::Session> session);
        void setAccountsOrder(const std::shared_ptr<restbed::Session> session);
        void getHookSettings(const std::shared_ptr<restbed::Session> session);
        void setHookSettings(const std::shared_ptr<restbed::Session> session);
        void getCredentials(const std::shared_ptr<restbed::Session> session);
        void setCredentials(const std::shared_ptr<restbed::Session> session);
        void getAddrFromInterfaceName(const std::shared_ptr<restbed::Session> session);
        void getAllIpInterface(const std::shared_ptr<restbed::Session> session);
        void getAllIpInterfaceByName(const std::shared_ptr<restbed::Session> session);
        void getShortcuts(const std::shared_ptr<restbed::Session> session);
        void setShortcuts(const std::shared_ptr<restbed::Session> session);
        void setVolume(const std::string& device, const double& value);
        void getVolume(const std::string& device);
        void validateCertificate(const std::string& accountId, const std::string& certificate);
        void validateCertificatePath(const std::string& accountId, const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPass, const std::string& caList);
        void getCertificateDetails(const std::string& certificate);
        void getCertificateDetailsPath(const std::string& certificatePath, const std::string& privateKey, const std::string& privateKeyPass);
        void getPinnedCertificates();
        void pinCertificate(const std::vector<uint8_t>& certificate, const bool& local);
        void unpinCertificate(const std::string& certId);
        void pinCertificatePath(const std::string& path);
        void unpinCertificatePath(const std::string& path);
        void pinRemoteCertificate(const std::string& accountId, const std::string& certId);
        void setCertificateStatus(const std::string& account, const std::string& certId, const std::string& status);
        void getCertificatesByStatus(const std::string& account, const std::string& status);
        void getTrustRequests(const std::shared_ptr<restbed::Session> session);
        void acceptTrustRequest(const std::string& accountId, const std::string& from);
        void discardTrustRequest(const std::string& accountId, const std::string& from);
        void sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload);
        void exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password);
        void importAccounts(const std::string& archivePath, const std::string& password);
};
