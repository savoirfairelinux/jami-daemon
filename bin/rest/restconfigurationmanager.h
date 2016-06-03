#pragma once

#include <vector>
#include <map>
#include <string>
#include <restbed>
#include <iostream> //TODO : remove

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef RING_VIDEO
#include "dring/videomanager_interface.h"
#endif
#include "logger.h"
#include "im/message_engine.h"

#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class RestConfigurationManager
{
	public:
		RestConfigurationManager();

		std::vector<std::shared_ptr<restbed::Resource>> getResources();

	private:
		// Attributes
		std::vector<std::shared_ptr<restbed::Resource>> resources_;

		// Methods
		std::map<std::string, std::string> parsePost(const std::string& post);
		void populateResources();
		void defaultRoute(const std::shared_ptr<restbed::Session> session);

        void getAccountDetails(const std::shared_ptr<restbed::Session> session); ///
        void getVolatileAccountDetails(const std::shared_ptr<restbed::Session> session); ///
        void setAccountDetails(const std::shared_ptr<restbed::Session> session); ///
        void setAccountActive(const std::shared_ptr<restbed::Session> session); ///
        void getAccountTemplate(const std::shared_ptr<restbed::Session> session); ///
        void addAccount(const std::shared_ptr<restbed::Session> session); ///
        void removeAccount(const std::shared_ptr<restbed::Session> session); ///
        void getAccountList(const std::shared_ptr<restbed::Session> session); ///
        void sendRegister(const std::shared_ptr<restbed::Session> session); ///
        void registerAllAccounts(const std::shared_ptr<restbed::Session> session); ///
        void sendTextMessage(const std::shared_ptr<restbed::Session> session); ///
        void getMessageStatus(const std::shared_ptr<restbed::Session> session); ///
        void getTlsDefaultSettings(const std::shared_ptr<restbed::Session> session); ///
        void getCodecList(const std::shared_ptr<restbed::Session> session); ///
        void getSupportedTlsMethod(const std::shared_ptr<restbed::Session> session); ///
        void getSupportedCiphers(const std::shared_ptr<restbed::Session> session); ///
        void getCodecDetails(const std::shared_ptr<restbed::Session> session); ///
        void setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details);
        void getActiveCodecList(const std::shared_ptr<restbed::Session> session);
        void setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list);
        void getAudioPluginList();
        void setAudioPlugin(const std::string& audioPlugin);
        void getAudioOutputDeviceList();
        void setAudioOutputDevice(const int32_t& index);
        void setAudioInputDevice(const int32_t& index);
        void setAudioRingtoneDevice(const int32_t& index);
        void getAudioInputDeviceList();
        void getCurrentAudioDevicesIndex();
        void getAudioInputDeviceIndex(const std::string& name);
        void getAudioOutputDeviceIndex(const std::string& name);
        void getCurrentAudioOutputPlugin();
        void getNoiseSuppressState();
        void setNoiseSuppressState(const bool& state);
        void isAgcEnabled();
        void setAgcState(const bool& enabled);
        void muteDtmf(const bool& mute);
        void isDtmfMuted();
        void isCaptureMuted();
        void muteCapture(const bool& mute);
        void isPlaybackMuted();
        void mutePlayback(const bool& mute);
        void isRingtoneMuted();
        void muteRingtone(const bool& mute);
        void getAudioManager();
        void setAudioManager(const std::string& api);
        void getSupportedAudioManagers();
        void isIax2Enabled();
        void getRecordPath();
        void setRecordPath(const std::string& recPath);
        void getIsAlwaysRecording();
        void setIsAlwaysRecording(const bool& rec);
        void setHistoryLimit(const int32_t& days);
        void getHistoryLimit();
        void setAccountsOrder(const std::string& order);
        void getHookSettings();
        void setHookSettings(const std::map<std::string, std::string>& settings);
        void getCredentials(const std::shared_ptr<restbed::Session> session);
        void setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details);
        void getAddrFromInterfaceName(const std::string& interface);
        void getAllIpInterface();
        void getAllIpInterfaceByName();
        void getShortcuts();
        void setShortcuts(const std::map<std::string, std::string> &shortcutsMap);
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
