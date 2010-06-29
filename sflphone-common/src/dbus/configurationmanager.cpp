/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include <global.h>
#include <configurationmanager.h>
#include <sstream>
#include "../manager.h"
#include "sip/sipvoiplink.h"

const char* ConfigurationManager::SERVER_PATH =
		"/org/sflphone/SFLphone/ConfigurationManager";

ConfigurationManager::ConfigurationManager(DBus::Connection& connection) :
	DBus::ObjectAdaptor(connection, SERVER_PATH) {
	shortcutsKeys.push_back("pick_up");
	shortcutsKeys.push_back("hang_up");
	shortcutsKeys.push_back("popup_window");
	shortcutsKeys.push_back("toggle_pick_up_hang_up");
	shortcutsKeys.push_back("toggle_hold");
}

std::map<std::string, std::string> ConfigurationManager::getAccountDetails(
		const std::string& accountID) {
	return Manager::instance().getAccountDetails(accountID);
}

std::map<std::string, std::string> ConfigurationManager::getTlsSettingsDefault(
		void) {

	std::map<std::string, std::string> tlsSettingsDefault;
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_LISTENER_PORT, DEFAULT_SIP_TLS_PORT));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_CA_LIST_FILE, ""));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_CERTIFICATE_FILE, ""));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_PRIVATE_KEY_FILE, ""));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(TLS_PASSWORD,
			""));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(TLS_METHOD,
			"TLSv1"));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(TLS_CIPHERS,
			""));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_SERVER_NAME, ""));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_VERIFY_SERVER, "true"));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_VERIFY_CLIENT, "true"));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_REQUIRE_CLIENT_CERTIFICATE, "true"));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_NEGOTIATION_TIMEOUT_SEC, "2"));
	tlsSettingsDefault.insert(std::pair<std::string, std::string>(
			TLS_NEGOTIATION_TIMEOUT_MSEC, "0"));

	return tlsSettingsDefault;
}

std::map<std::string, std::string> ConfigurationManager::getIp2IpDetails(void) {

	std::map<std::string, std::string> ip2ipAccountDetails;

	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(ACCOUNT_ID,
			IP2IP_PROFILE));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			SRTP_KEY_EXCHANGE, Manager::instance().getConfigString(
					IP2IP_PROFILE, SRTP_KEY_EXCHANGE)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(SRTP_ENABLE,
			Manager::instance().getConfigString(IP2IP_PROFILE, SRTP_ENABLE)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			SRTP_RTP_FALLBACK, Manager::instance().getConfigString(
					IP2IP_PROFILE, SRTP_RTP_FALLBACK)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			ZRTP_DISPLAY_SAS, Manager::instance().getConfigString(
					IP2IP_PROFILE, ZRTP_DISPLAY_SAS)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			ZRTP_HELLO_HASH, Manager::instance().getConfigString(IP2IP_PROFILE,
					ZRTP_HELLO_HASH)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			ZRTP_NOT_SUPP_WARNING, Manager::instance().getConfigString(
					IP2IP_PROFILE, ZRTP_NOT_SUPP_WARNING)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			ZRTP_DISPLAY_SAS_ONCE, Manager::instance().getConfigString(
					IP2IP_PROFILE, ZRTP_DISPLAY_SAS_ONCE)));

	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(
			LOCAL_INTERFACE, Manager::instance().getConfigString(IP2IP_PROFILE,
					LOCAL_INTERFACE)));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(LOCAL_PORT,
			Manager::instance().getConfigString(IP2IP_PROFILE, LOCAL_PORT)));

	std::map<std::string, std::string> tlsSettings;
	tlsSettings = getTlsSettings(IP2IP_PROFILE);
	std::copy(tlsSettings.begin(), tlsSettings.end(), std::inserter(
			ip2ipAccountDetails, ip2ipAccountDetails.end()));

	return ip2ipAccountDetails;

}

void ConfigurationManager::setIp2IpDetails(const std::map<std::string,
		std::string>& details) {
	std::map<std::string, std::string> map_cpy = details;
	std::map<std::string, std::string>::iterator it;

	it = map_cpy.find(LOCAL_INTERFACE);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, LOCAL_INTERFACE,
				it->second);
	}

	it = map_cpy.find(LOCAL_PORT);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, LOCAL_PORT, it->second);
	}

	it = map_cpy.find(SRTP_ENABLE);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, SRTP_ENABLE, it->second);
	}

	it = map_cpy.find(SRTP_RTP_FALLBACK);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, SRTP_RTP_FALLBACK,
				it->second);
	}

	it = map_cpy.find(SRTP_KEY_EXCHANGE);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, SRTP_KEY_EXCHANGE,
				it->second);
	}

	it = map_cpy.find(ZRTP_DISPLAY_SAS);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, ZRTP_DISPLAY_SAS,
				it->second);
	}

	it = map_cpy.find(ZRTP_NOT_SUPP_WARNING);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, ZRTP_NOT_SUPP_WARNING,
				it->second);
	}

	it = map_cpy.find(ZRTP_HELLO_HASH);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, ZRTP_HELLO_HASH,
				it->second);
	}

	it = map_cpy.find(ZRTP_DISPLAY_SAS_ONCE);

	if (it != details.end()) {
		Manager::instance().setConfig(IP2IP_PROFILE, ZRTP_DISPLAY_SAS_ONCE,
				it->second);
	}

	setTlsSettings(IP2IP_PROFILE, details);

	Manager::instance().saveConfig();

	// Update account details to the client side
	accountsChanged();

	// Reload account settings from config
	Manager::instance().getAccount(IP2IP_PROFILE)->loadConfig();

}

std::map<std::string, std::string> ConfigurationManager::getTlsSettings(
		const std::string& section) {
	std::map<std::string, std::string> tlsSettings;

	tlsSettings.insert(std::pair<std::string, std::string>(TLS_LISTENER_PORT,
			Manager::instance().getConfigString(section, TLS_LISTENER_PORT)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_ENABLE,
			Manager::instance().getConfigString(section, TLS_ENABLE)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_CA_LIST_FILE,
			Manager::instance().getConfigString(section, TLS_CA_LIST_FILE)));
	tlsSettings.insert(std::pair<std::string, std::string>(
			TLS_CERTIFICATE_FILE, Manager::instance().getConfigString(section,
					TLS_CERTIFICATE_FILE)));
	tlsSettings.insert(std::pair<std::string, std::string>(
			TLS_PRIVATE_KEY_FILE, Manager::instance().getConfigString(section,
					TLS_PRIVATE_KEY_FILE)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_PASSWORD,
			Manager::instance().getConfigString(section, TLS_PASSWORD)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_METHOD,
			Manager::instance().getConfigString(section, TLS_METHOD)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_CIPHERS,
			Manager::instance().getConfigString(section, TLS_CIPHERS)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_SERVER_NAME,
			Manager::instance().getConfigString(section, TLS_SERVER_NAME)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_VERIFY_SERVER,
			Manager::instance().getConfigString(section, TLS_VERIFY_SERVER)));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_VERIFY_CLIENT,
			Manager::instance().getConfigString(section, TLS_VERIFY_CLIENT)));
	tlsSettings.insert(std::pair<std::string, std::string>(
			TLS_REQUIRE_CLIENT_CERTIFICATE,
			Manager::instance().getConfigString(section,
					TLS_REQUIRE_CLIENT_CERTIFICATE)));
	tlsSettings.insert(std::pair<std::string, std::string>(
			TLS_NEGOTIATION_TIMEOUT_SEC, Manager::instance().getConfigString(
					section, TLS_NEGOTIATION_TIMEOUT_SEC)));
	tlsSettings.insert(std::pair<std::string, std::string>(
			TLS_NEGOTIATION_TIMEOUT_MSEC, Manager::instance().getConfigString(
					section, TLS_NEGOTIATION_TIMEOUT_MSEC)));
	return tlsSettings;
}

void ConfigurationManager::setTlsSettings(const std::string& section,
		const std::map<std::string, std::string>& details) {
	std::map<std::string, std::string> map_cpy = details;
	std::map<std::string, std::string>::iterator it;

	it = map_cpy.find(TLS_LISTENER_PORT);
	if (it != details.end()) {
		Manager::instance().setConfig(section, TLS_LISTENER_PORT, it->second);
	}

	it = map_cpy.find(TLS_ENABLE);

	if (it != details.end()) {
		Manager::instance().setConfig(section, TLS_ENABLE, it->second);
	}

	it = map_cpy.find(TLS_CA_LIST_FILE);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_CA_LIST_FILE, it->second);
	}

	it = map_cpy.find(TLS_CERTIFICATE_FILE);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_CERTIFICATE_FILE, it->second);
	}

	it = map_cpy.find(TLS_PRIVATE_KEY_FILE);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_PRIVATE_KEY_FILE, it->second);
	}

	it = map_cpy.find(TLS_PASSWORD);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_PASSWORD, it->second);
	}

	it = map_cpy.find(TLS_METHOD);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_METHOD, it->second);
	}

	it = map_cpy.find(TLS_CIPHERS);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_CIPHERS, it->second);
	}

	it = map_cpy.find(TLS_SERVER_NAME);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_SERVER_NAME, it->second);
	}

	it = map_cpy.find(TLS_VERIFY_CLIENT);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_VERIFY_CLIENT, it->second);
	}

	it = map_cpy.find(TLS_REQUIRE_CLIENT_CERTIFICATE);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_REQUIRE_CLIENT_CERTIFICATE,
				it->second);
	}

	it = map_cpy.find(TLS_NEGOTIATION_TIMEOUT_SEC);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_NEGOTIATION_TIMEOUT_SEC,
				it->second);
	}

	it = map_cpy.find(TLS_NEGOTIATION_TIMEOUT_MSEC);

	if (it != map_cpy.end()) {
		Manager::instance().setConfig(section, TLS_NEGOTIATION_TIMEOUT_MSEC,
				it->second);
	}

	Manager::instance().saveConfig();

	// Update account details to the client side
	accountsChanged();

}

std::map<std::string, std::string> ConfigurationManager::getCredential(
		const std::string& accountID, const int32_t& index) {

	std::string credentialIndex;
	std::stringstream streamOut;
	streamOut << index;
	credentialIndex = streamOut.str();

	std::string section = std::string("Credential") + std::string(":")
			+ accountID + std::string(":") + credentialIndex;

	std::map<std::string, std::string> credentialInformation;
	std::string username = Manager::instance().getConfigString(section,
			USERNAME);
	std::string password = Manager::instance().getConfigString(section,
			PASSWORD);
	std::string realm = Manager::instance().getConfigString(section, REALM);

	credentialInformation.insert(std::pair<std::string, std::string>(USERNAME,
			username));
	credentialInformation.insert(std::pair<std::string, std::string>(PASSWORD,
			password));
	credentialInformation.insert(std::pair<std::string, std::string>(REALM,
			realm));

	return credentialInformation;
}

int32_t ConfigurationManager::getNumberOfCredential(
		const std::string& accountID) {
	return Manager::instance().getConfigInt(accountID, CONFIG_CREDENTIAL_NUMBER);
}

void ConfigurationManager::setNumberOfCredential(const std::string& accountID,
		const int32_t& number) {
	if (accountID != AccountNULL || !accountID.empty()) {
		Manager::instance().setConfig(accountID, CONFIG_CREDENTIAL_NUMBER,
				number);
	}
}

void ConfigurationManager::setCredential(const std::string& accountID,
		const int32_t& index, const std::map<std::string, std::string>& details) {
	Manager::instance().setCredential(accountID, index, details);
}

void ConfigurationManager::deleteAllCredential(const std::string& accountID) {
	Manager::instance().deleteAllCredential(accountID);
}

void ConfigurationManager::setAccountDetails(const std::string& accountID,
		const std::map<std::string, std::string>& details) {
	Manager::instance().setAccountDetails(accountID, details);
}

void ConfigurationManager::sendRegister(const std::string& accountID,
		const int32_t& expire) {
	Manager::instance().sendRegister(accountID, expire);
}

std::string ConfigurationManager::addAccount(const std::map<std::string,
		std::string>& details) {
	return Manager::instance().addAccount(details);
}

void ConfigurationManager::removeAccount(const std::string& accoundID) {
	return Manager::instance().removeAccount(accoundID);
}

std::vector<std::string> ConfigurationManager::getAccountList() {
	return Manager::instance().getAccountList();
}

//TODO
std::vector<std::string> ConfigurationManager::getToneLocaleList() {
	std::vector<std::string> ret;
	return ret;
}

//TODO
std::string ConfigurationManager::getVersion() {
	std::string ret("");
	return ret;
}

//TODO
std::vector<std::string> ConfigurationManager::getRingtoneList() {
	std::vector<std::string> ret;
	return ret;
}

/**
 * Send the list of all codecs loaded to the client through DBus.
 * Can stay global, as only the active codecs will be set per accounts
 */
std::vector<std::string> ConfigurationManager::getCodecList(void) {

	std::vector<std::string> list;

	CodecsMap codecs =
			Manager::instance().getCodecDescriptorMap().getCodecsMap();
	CodecsMap::iterator iter = codecs.begin();

	while (iter != codecs.end()) {
		std::stringstream ss;

		if (iter->second != NULL) {
			ss << iter->first;
			list.push_back((ss.str()).data());
		}

		iter++;
	}

	return list;
}

std::vector<std::string> ConfigurationManager::getSupportedTlsMethod(void) {
	std::vector<std::string> method;
	method.push_back("Default");
	method.push_back("TLSv1");
	method.push_back("SSLv2");
	method.push_back("SSLv3");
	method.push_back("SSLv23");
	return method;
}

std::vector<std::string> ConfigurationManager::getCodecDetails(
		const int32_t& payload) {

	return Manager::instance().getCodecDescriptorMap().getCodecSpecifications(
			payload);
}

std::vector<std::string> ConfigurationManager::getActiveCodecList(
		const std::string& accountID) {

	_debug("Send active codec list for account %s", accountID.c_str ());

	std::vector<std::string> v;
	Account *acc;
	CodecOrder active;
	unsigned int i = 0;
	size_t size;

	acc = Manager::instance().getAccount(accountID);
	if (acc != NULL) {
		active = acc->getActiveCodecs();
		size = active.size();
		while (i < size) {
			std::stringstream ss;
			ss << active[i];
			v.push_back((ss.str()).data());
			i++;
		}
	}

	return v;

}

void ConfigurationManager::setActiveCodecList(
		const std::vector<std::string>& list, const std::string& accountID) {

	_debug ("ConfigurationManager: Active codec list received");

	Account *acc;

	// Save the codecs list per account
	acc = Manager::instance().getAccount(accountID);
	if (acc != NULL) {
		acc->setActiveCodecs(list);
	}
}


std::vector<std::string> ConfigurationManager::getAudioPluginList() {

	std::vector<std::string> v;

	v.push_back(PCM_DEFAULT);
	// v.push_back(PCM_DMIX);
	v.push_back(PCM_DMIX_DSNOOP);

	return v;
}


void ConfigurationManager::setInputAudioPlugin(const std::string& audioPlugin) {
	return Manager::instance().setInputAudioPlugin(audioPlugin);
}

void ConfigurationManager::setOutputAudioPlugin(const std::string& audioPlugin) {
	return Manager::instance().setOutputAudioPlugin(audioPlugin);
}

std::vector<std::string> ConfigurationManager::getAudioOutputDeviceList() {
	return Manager::instance().getAudioOutputDeviceList();
}

std::vector<std::string> ConfigurationManager::getAudioInputDeviceList() {
	return Manager::instance().getAudioInputDeviceList();
}

void ConfigurationManager::setAudioOutputDevice(const int32_t& index) {
        return Manager::instance().setAudioDevice(index, SFL_PCM_PLAYBACK);
}

void ConfigurationManager::setAudioInputDevice(const int32_t& index) {
        return Manager::instance().setAudioDevice(index, SFL_PCM_CAPTURE);
}

void ConfigurationManager::setAudioRingtoneDevice(const int32_t& index) {
        return Manager::instance().setAudioDevice(index, SFL_PCM_RINGTONE);
}

std::vector<std::string> ConfigurationManager::getCurrentAudioDevicesIndex() {
	return Manager::instance().getCurrentAudioDevicesIndex();
}

int32_t ConfigurationManager::getAudioDeviceIndex(const std::string& name) {
	return Manager::instance().getAudioDeviceIndex(name);
}

std::string ConfigurationManager::getCurrentAudioOutputPlugin(void) {
	return Manager::instance().getCurrentAudioOutputPlugin();
}

std::string ConfigurationManager::getEchoCancelState(void) {
        return Manager::instance().getEchoCancelState();
}

void ConfigurationManager::setEchoCancelState(const std::string& state) {
        Manager::instance().setEchoCancelState(state);
}

std::string ConfigurationManager::getNoiseSuppressState(void) {
  return Manager::instance().getNoiseSuppressState();
}

void ConfigurationManager::setNoiseSuppressState(const std::string& state) {
  Manager::instance().setNoiseSuppressState(state);
}

std::vector<std::string> ConfigurationManager::getPlaybackDeviceList() {
	std::vector<std::string> ret;
	return ret;
}

std::vector<std::string> ConfigurationManager::getRecordDeviceList() {
	std::vector<std::string> ret;
	return ret;

}

bool ConfigurationManager::isMd5CredentialHashing(void) {
	bool isEnabled = Manager::instance().getConfigBool(PREFERENCES,
			CONFIG_MD5HASH);
	return isEnabled;
}

void ConfigurationManager::setMd5CredentialHashing(const bool& enabled) {
	if (enabled) {
		Manager::instance().setConfig(PREFERENCES, CONFIG_MD5HASH, TRUE_STR);
	} else {
		Manager::instance().setConfig(PREFERENCES, CONFIG_MD5HASH, FALSE_STR);
	}
}

int32_t ConfigurationManager::isIax2Enabled(void) {
	return Manager::instance().isIax2Enabled();
}

void ConfigurationManager::ringtoneEnabled(void) {
	Manager::instance().ringtoneEnabled();
}

int32_t ConfigurationManager::isRingtoneEnabled(void) {
	return Manager::instance().isRingtoneEnabled();
}

std::string ConfigurationManager::getRingtoneChoice(void) {
	return Manager::instance().getRingtoneChoice();
}

void ConfigurationManager::setRingtoneChoice(const std::string& tone) {
	Manager::instance().setRingtoneChoice(tone);
}

std::string ConfigurationManager::getRecordPath(void) {
	return Manager::instance().getRecordPath();
}

void ConfigurationManager::setRecordPath(const std::string& recPath) {
	Manager::instance().setRecordPath(recPath);
}

int32_t ConfigurationManager::getDialpad(void) {
	return Manager::instance().getDialpad();
}

void ConfigurationManager::setDialpad(const bool& display) {
	Manager::instance().setDialpad(display);
}

int32_t ConfigurationManager::getSearchbar(void) {
	return Manager::instance().getSearchbar();
}

void ConfigurationManager::setSearchbar(void) {
	Manager::instance().setSearchbar();
}

int32_t ConfigurationManager::getVolumeControls(void) {
	return Manager::instance().getVolumeControls();
}

void ConfigurationManager::setVolumeControls(const bool& display) {
	Manager::instance().setVolumeControls(display);
}

int32_t ConfigurationManager::getHistoryLimit(void) {
	return Manager::instance().getHistoryLimit();
}

void ConfigurationManager::setHistoryLimit(const int32_t& days) {
	Manager::instance().setHistoryLimit(days);
}

void ConfigurationManager::setHistoryEnabled(void) {
	Manager::instance().setHistoryEnabled();
}

std::string ConfigurationManager::getHistoryEnabled(void) {
	return Manager::instance().getHistoryEnabled();
}

void ConfigurationManager::startHidden(void) {
	Manager::instance().startHidden();
}

int32_t ConfigurationManager::isStartHidden(void) {
	return Manager::instance().isStartHidden();
}

void ConfigurationManager::switchPopupMode(void) {
	Manager::instance().switchPopupMode();
}

int32_t ConfigurationManager::popupMode(void) {
	return Manager::instance().popupMode();
}

void ConfigurationManager::setNotify(void) {
	Manager::instance().setNotify();
}

int32_t ConfigurationManager::getNotify(void) {
	return Manager::instance().getNotify();
}

void ConfigurationManager::setAudioManager(const int32_t& api) {
	Manager::instance().setAudioManager(api);
}

int32_t ConfigurationManager::getAudioManager(void) {
	return Manager::instance().getAudioManager();
}

void ConfigurationManager::setMailNotify(void) {
	Manager::instance().setMailNotify();
}

int32_t ConfigurationManager::getMailNotify(void) {
	return Manager::instance().getMailNotify();
}

std::map<std::string, int32_t> ConfigurationManager::getAddressbookSettings(
		void) {
	return Manager::instance().getAddressbookSettings();
}

void ConfigurationManager::setAddressbookSettings(const std::map<std::string,
		int32_t>& settings) {
	Manager::instance().setAddressbookSettings(settings);
}

std::vector<std::string> ConfigurationManager::getAddressbookList(void) {
	return Manager::instance().getAddressbookList();
}

void ConfigurationManager::setAddressbookList(
		const std::vector<std::string>& list) {
	Manager::instance().setAddressbookList(list);
}

std::map<std::string, std::string> ConfigurationManager::getHookSettings(void) {
	return Manager::instance().getHookSettings();
}

void ConfigurationManager::setHookSettings(const std::map<std::string,
		std::string>& settings) {
	Manager::instance().setHookSettings(settings);
}

void ConfigurationManager::setAccountsOrder(const std::string& order) {
	Manager::instance().setAccountsOrder(order);
}

std::map<std::string, std::string> ConfigurationManager::getHistory(void) {
	return Manager::instance().send_history_to_client();
}

void ConfigurationManager::setHistory(
		const std::map<std::string, std::string>& entries) {
	Manager::instance().receive_history_from_client(entries);
}

std::string ConfigurationManager::getAddrFromInterfaceName(
		const std::string& interface) {

	std::string address = SIPVoIPLink::instance("")->getInterfaceAddrFromName(
			interface);

	return address;
}

std::vector<std::string> ConfigurationManager::getAllIpInterface(void) {

	std::vector<std::string> vector;
	SIPVoIPLink * sipLink = NULL;
	sipLink = SIPVoIPLink::instance("");

	if (sipLink != NULL) {
		vector = sipLink->getAllIpInterface();
	}

	return vector;
}

std::vector<std::string> ConfigurationManager::getAllIpInterfaceByName(void) {
	std::vector<std::string> vector;
	SIPVoIPLink * sipLink = NULL;
	sipLink = SIPVoIPLink::instance("");

	if (sipLink != NULL) {
		vector = sipLink->getAllIpInterfaceByName();
	}

	return vector;
}

int32_t ConfigurationManager::getWindowWidth(void) {

	return Manager::instance().getConfigInt(PREFERENCES, WINDOW_WIDTH);
}

int32_t ConfigurationManager::getWindowHeight(void) {

	return Manager::instance().getConfigInt(PREFERENCES, WINDOW_HEIGHT);
}

void ConfigurationManager::setWindowWidth(const int32_t& width) {

	Manager::instance().setConfig(PREFERENCES, WINDOW_WIDTH, width);
}

void ConfigurationManager::setWindowHeight(const int32_t& height) {

	Manager::instance().setConfig(PREFERENCES, WINDOW_HEIGHT, height);
}

int32_t ConfigurationManager::getWindowPositionX(void) {

	return Manager::instance().getConfigInt(PREFERENCES, WINDOW_POSITION_X);
}

int32_t ConfigurationManager::getWindowPositionY(void) {

	return Manager::instance().getConfigInt(PREFERENCES, WINDOW_POSITION_Y);
}

void ConfigurationManager::setWindowPositionX(const int32_t& posX) {

	Manager::instance().setConfig(PREFERENCES, WINDOW_POSITION_X, posX);
}

void ConfigurationManager::setWindowPositionY(const int32_t& posY) {

	Manager::instance().setConfig(PREFERENCES, WINDOW_POSITION_Y, posY);
}

std::map<std::string, int32_t> ConfigurationManager::getShortcuts() {

	std::map<std::string, int> shortcutsMap;
	int shortcut;

	for (int i = 0; i < (int)shortcutsKeys.size(); i++) {
		std::string key = shortcutsKeys.at(i);
		shortcut = Manager::instance().getConfigInt("Shortcuts", key);
		shortcutsMap.insert(std::pair<std::string, int>(key, shortcut));
	}

	return shortcutsMap;
}

void ConfigurationManager::setShortcuts(
		const std::map<std::string, int32_t>& shortcutsMap) {

	std::map<std::string, int> map_cpy = shortcutsMap;
	std::map<std::string, int>::iterator it;

	for (int i = 0; i < (int)shortcutsKeys.size(); i++) {
		std::string key = shortcutsKeys.at(i);
		it = map_cpy.find(key);
		if (it != shortcutsMap.end()) {
			Manager::instance().setConfig("Shortcuts", key, it->second);
		}
	}

	Manager::instance().saveConfig();
}

void ConfigurationManager::enableStatusIcon (const std::string& value) {

	Manager::instance ().setConfig (PREFERENCES, SHOW_STATUSICON, value);
}

std::string ConfigurationManager::isStatusIconEnabled (void) {

	return Manager::instance ().getConfigString (PREFERENCES, SHOW_STATUSICON);
}
