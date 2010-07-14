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
#include "sip/sipaccount.h"

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

  _debug("ConfigurationManager: get account details %s", accountID.c_str());
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

	SIPAccount *sipaccount = (SIPAccount *)Manager::instance().getAccount(IP2IP_PROFILE);

	if(!sipaccount) {
	  _error("ConfigurationManager: could not find account");
	  return ip2ipAccountDetails;
	}

	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(ACCOUNT_ID, IP2IP_PROFILE));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(SRTP_KEY_EXCHANGE, sipaccount->getSrtpKeyExchange())); 
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(SRTP_ENABLE, sipaccount->getSrtpEnable() ? "true" : "false"));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(SRTP_RTP_FALLBACK, sipaccount->getSrtpFallback() ? "true" : "false"));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(ZRTP_DISPLAY_SAS, sipaccount->getZrtpDisplaySas() ? "true" : "false"));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(ZRTP_HELLO_HASH, sipaccount->getZrtpHelloHash() ? "true" : "false"));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(ZRTP_NOT_SUPP_WARNING, sipaccount->getZrtpNotSuppWarning() ? "true" : "false"));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(ZRTP_DISPLAY_SAS_ONCE, sipaccount->getZrtpDiaplaySasOnce() ? "true" : "false"));
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(LOCAL_INTERFACE, sipaccount->getLocalInterface()));
	std::stringstream portstr; portstr << sipaccount->getLocalPort();
	ip2ipAccountDetails.insert(std::pair<std::string, std::string>(LOCAL_PORT, portstr.str()));

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

	SIPAccount *sipaccount = (SIPAccount *)Manager::instance().getAccount(IP2IP_PROFILE);

	if(!sipaccount) {
	  _error("ConfigurationManager: could not find account");
	}
	  

	it = map_cpy.find(LOCAL_INTERFACE);
	if (it != details.end()) sipaccount->setLocalInterface(it->second);

	it = map_cpy.find(LOCAL_PORT);
	if (it != details.end()) sipaccount->setLocalPort(atoi(it->second.data()));

	it = map_cpy.find(SRTP_ENABLE);
	if (it != details.end()) sipaccount->setSrtpEnable((it->second == "true"));

	it = map_cpy.find(SRTP_RTP_FALLBACK);
	if (it != details.end()) sipaccount->setSrtpFallback((it->second == "true"));

	it = map_cpy.find(SRTP_KEY_EXCHANGE);
	if (it != details.end()) sipaccount->setSrtpKeyExchange(it->second);

	it = map_cpy.find(ZRTP_DISPLAY_SAS);
	if (it != details.end()) sipaccount->setZrtpDisplaySas((it->second == "true"));

	it = map_cpy.find(ZRTP_NOT_SUPP_WARNING);
	if (it != details.end()) sipaccount->setZrtpNotSuppWarning((it->second == "true"));

	it = map_cpy.find(ZRTP_HELLO_HASH);
	if (it != details.end()) sipaccount->setZrtpHelloHash((it->second == "true"));

	it = map_cpy.find(ZRTP_DISPLAY_SAS_ONCE);
	if (it != details.end()) sipaccount->setZrtpDiaplaySasOnce((it->second == "true"));

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

	SIPAccount *sipaccount = (SIPAccount *)Manager::instance().getAccount(IP2IP_PROFILE); 

	if(!sipaccount)
	  return tlsSettings;

	std::stringstream portstr; portstr << sipaccount->getTlsListenerPort();
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_LISTENER_PORT, portstr.str()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_ENABLE, sipaccount->getTlsEnable()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_CA_LIST_FILE, sipaccount->getTlsCaListFile()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_CERTIFICATE_FILE, sipaccount->getTlsCertificateFile()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_PRIVATE_KEY_FILE, sipaccount->getTlsPrivateKeyFile()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_PASSWORD, sipaccount->getTlsPassword()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_METHOD, sipaccount->getTlsMethod()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_CIPHERS, sipaccount->getTlsCiphers()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_SERVER_NAME, sipaccount->getTlsServerName()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_VERIFY_SERVER, sipaccount->getTlsVerifyServer() ? "true" : "false"));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_VERIFY_CLIENT, sipaccount->getTlsVerifyClient() ? "true" : "false"));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_REQUIRE_CLIENT_CERTIFICATE, sipaccount->getTlsRequireClientCertificate() ? "true" : "false"));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_NEGOTIATION_TIMEOUT_SEC, sipaccount->getTlsNegotiationTimeoutSec()));
	tlsSettings.insert(std::pair<std::string, std::string>(TLS_NEGOTIATION_TIMEOUT_MSEC, sipaccount->getTlsNegotiationTimeoutMsec()));

	return tlsSettings;
}

void ConfigurationManager::setTlsSettings(const std::string& section,
		const std::map<std::string, std::string>& details) {

	std::map<std::string, std::string> map_cpy = details;
	std::map<std::string, std::string>::iterator it;

	SIPAccount * sipaccount = (SIPAccount *)Manager::instance().getAccount(IP2IP_PROFILE);

	if(!sipaccount) {
	  _debug("ConfigurationManager: Error: No valid account in set TLS settings");
	  return;
	}

	it = map_cpy.find(TLS_LISTENER_PORT);
	if (it != details.end()) sipaccount->setTlsListenerPort(atoi(it->second.data()));

	it = map_cpy.find(TLS_ENABLE);
	if (it != details.end()) sipaccount->setTlsEnable(it->second);

	it = map_cpy.find(TLS_CA_LIST_FILE);
	if (it != map_cpy.end()) sipaccount->setTlsCaListFile(it->second);

	it = map_cpy.find(TLS_CERTIFICATE_FILE);
	if (it != map_cpy.end()) sipaccount->setTlsCertificateFile(it->second);

	it = map_cpy.find(TLS_PRIVATE_KEY_FILE);
	if (it != map_cpy.end()) sipaccount->setTlsPrivateKeyFile(it->second);

	it = map_cpy.find(TLS_PASSWORD);
	if (it != map_cpy.end()) sipaccount->setTlsPassword(it->second);

	it = map_cpy.find(TLS_METHOD);
	if (it != map_cpy.end()) sipaccount->setTlsMethod(it->second);

	it = map_cpy.find(TLS_CIPHERS);
	if (it != map_cpy.end()) sipaccount->setTlsCiphers(it->second);

	it = map_cpy.find(TLS_SERVER_NAME);
	if (it != map_cpy.end()) sipaccount->setTlsServerName(it->second);

	it = map_cpy.find(TLS_VERIFY_CLIENT);
	if (it != map_cpy.end()) sipaccount->setTlsVerifyClient((it->second == "true") ? true : false);

	it = map_cpy.find(TLS_REQUIRE_CLIENT_CERTIFICATE);
	if (it != map_cpy.end()) sipaccount->setTlsRequireClientCertificate((it->second == "true") ? true : false);

	it = map_cpy.find(TLS_NEGOTIATION_TIMEOUT_SEC);
	if (it != map_cpy.end()) sipaccount->setTlsNegotiationTimeoutSec(it->second);

	it = map_cpy.find(TLS_NEGOTIATION_TIMEOUT_MSEC);
	if (it != map_cpy.end()) sipaccount->setTlsNegotiationTimeoutMsec(it->second);

	Manager::instance().saveConfig();

	// Update account details to the client side
	accountsChanged();

}

std::map<std::string, std::string> ConfigurationManager::getCredential(
		const std::string& accountID, const int32_t& index) {

        Account *account = Manager::instance().getAccount(accountID);

	std::map<std::string, std::string> credentialInformation;

	if(account->getType() != "SIP")
	  return credentialInformation;

	SIPAccount *sipaccount = (SIPAccount *)account;
 

	if(index == 0) {
	std::string username = sipaccount->getUsername();
	std::string password = sipaccount->getPassword();
        std::string realm = sipaccount->getRealm();

	credentialInformation.insert(std::pair<std::string, std::string>(USERNAME, username));
	credentialInformation.insert(std::pair<std::string, std::string>(PASSWORD, password));
	credentialInformation.insert(std::pair<std::string, std::string>(REALM, realm));
	}
	else {

	  // TODO: implement for extra credentials
	  std::string username = sipaccount->getUsername();
	  std::string password = sipaccount->getPassword();
	  std::string realm = sipaccount->getRealm();

	  credentialInformation.insert(std::pair<std::string, std::string>(USERNAME, username));
	  credentialInformation.insert(std::pair<std::string, std::string>(PASSWORD, password));
	  credentialInformation.insert(std::pair<std::string, std::string>(REALM, realm));
	}

	return credentialInformation;
}

int32_t ConfigurationManager::getNumberOfCredential(
		const std::string& accountID) {

  SIPAccount *sipaccount = (SIPAccount *)Manager::instance().getAccount(accountID);
  return sipaccount->getCredentialCount();
}

void ConfigurationManager::setNumberOfCredential(const std::string& accountID,
		const int32_t& number) {
  /*
  if (accountID != AccountNULL || !accountID.empty()) {
    SIPAccount *sipaccount = (SIPAccount *)Manager::instance().getAccount(accountID);
    sipaccount->setCredentialCount(number);
  }
  */
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

	CodecsMap codecs = Manager::instance().getCodecDescriptorMap().getCodecsMap();
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
	return Manager::instance().preferences.getMd5Hash();
}

void ConfigurationManager::setMd5CredentialHashing(const bool& enabled) {
        Manager::instance().preferences.setMd5Hash(enabled);
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

/*
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
*/

int32_t ConfigurationManager::getHistoryLimit(void) {
	return Manager::instance().getHistoryLimit();
}

void ConfigurationManager::setHistoryLimit(const int32_t& days) {
	Manager::instance().setHistoryLimit(days);
}

/*
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
*/

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

