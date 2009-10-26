/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 */

#include <global.h>
#include <configurationmanager.h>
#include <sstream>
#include "../manager.h"
#include "sip/sipvoiplink.h"

const char* ConfigurationManager::SERVER_PATH = "/org/sflphone/SFLphone/ConfigurationManager";



ConfigurationManager::ConfigurationManager (DBus::Connection& connection)
        : DBus::ObjectAdaptor (connection, SERVER_PATH)
{
}


std::map< std::string, std::string >
ConfigurationManager::getAccountDetails (const std::string& accountID)
{
    _debug ("ConfigurationManager::getAccountDetails\n");
    return Manager::instance().getAccountDetails (accountID);
}

std::map< std::string, std::string >
ConfigurationManager::getTlsSettingsDefault (void)
{
    _debug ("ConfigurationManager::getTlsDefaultSettings\n");

    std::map<std::string, std::string> tlsSettingsDefault;
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_CA_LIST_FILE, ""));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_CERTIFICATE_FILE, ""));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_PRIVATE_KEY_FILE, ""));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_PASSWORD, ""));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_METHOD, "TLSv1"));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_CIPHERS, ""));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_SERVER_NAME, ""));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_VERIFY_SERVER, "true"));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_VERIFY_CLIENT, "true"));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_REQUIRE_CLIENT_CERTIFICATE, "true"));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_SEC, "2"));
    tlsSettingsDefault.insert (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_MSEC, "0"));

    return tlsSettingsDefault;
}

std::map< std::string, std::string >
ConfigurationManager::getIp2IpDetails (void)
{

    std::map<std::string, std::string> ip2ipAccountDetails;

    ip2ipAccountDetails.insert (std::pair<std::string, std::string> (SRTP_KEY_EXCHANGE, Manager::instance().getConfigString (IP2IP_PROFILE, SRTP_KEY_EXCHANGE)));
    ip2ipAccountDetails.insert (std::pair<std::string, std::string> (SRTP_ENABLE, Manager::instance().getConfigString (IP2IP_PROFILE, SRTP_ENABLE)));
    ip2ipAccountDetails.insert (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS, Manager::instance().getConfigString (IP2IP_PROFILE, ZRTP_DISPLAY_SAS)));
    ip2ipAccountDetails.insert (std::pair<std::string, std::string> (ZRTP_HELLO_HASH, Manager::instance().getConfigString (IP2IP_PROFILE, ZRTP_HELLO_HASH)));
    ip2ipAccountDetails.insert (std::pair<std::string, std::string> (ZRTP_NOT_SUPP_WARNING, Manager::instance().getConfigString (IP2IP_PROFILE, ZRTP_NOT_SUPP_WARNING)));
    ip2ipAccountDetails.insert (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS_ONCE, Manager::instance().getConfigString (IP2IP_PROFILE, ZRTP_DISPLAY_SAS_ONCE)));

    std::map<std::string, std::string> tlsSettings;
    tlsSettings = getTlsSettings (IP2IP_PROFILE);
    std::copy (tlsSettings.begin(), tlsSettings.end(), std::inserter (ip2ipAccountDetails, ip2ipAccountDetails.end()));

    return ip2ipAccountDetails;

}

void
ConfigurationManager::setIp2IpDetails (const std::map< std::string, std::string >& details)
{
    std::map<std::string, std::string> map_cpy = details;
    std::map<std::string, std::string>::iterator it;

    it = map_cpy.find (SRTP_ENABLE);

    if (it != details.end()) {
        Manager::instance().setConfig (IP2IP_PROFILE, SRTP_ENABLE, it->second);
    }

    it = map_cpy.find (SRTP_KEY_EXCHANGE);

    if (it != details.end()) {
        Manager::instance().setConfig (IP2IP_PROFILE, SRTP_KEY_EXCHANGE, it->second);
    }

    it = map_cpy.find (ZRTP_DISPLAY_SAS);

    if (it != details.end()) {
        Manager::instance().setConfig (IP2IP_PROFILE, ZRTP_DISPLAY_SAS, it->second);
    }

    it = map_cpy.find (ZRTP_NOT_SUPP_WARNING);

    if (it != details.end()) {
        Manager::instance().setConfig (IP2IP_PROFILE, ZRTP_NOT_SUPP_WARNING, it->second);
    }

    it = map_cpy.find (ZRTP_HELLO_HASH);

    if (it != details.end()) {
        Manager::instance().setConfig (IP2IP_PROFILE, ZRTP_HELLO_HASH, it->second);
    }

    it = map_cpy.find (ZRTP_DISPLAY_SAS_ONCE);

    if (it != details.end()) {
        Manager::instance().setConfig (IP2IP_PROFILE, ZRTP_DISPLAY_SAS_ONCE, it->second);
    }

    setTlsSettings (IP2IP_PROFILE, details);

    Manager::instance().saveConfig();

    // Update account details to the client side
    accountsChanged();

}

std::map< std::string, std::string >
ConfigurationManager::getTlsSettings (const std::string& section)
{
    std::map<std::string, std::string> tlsSettings;
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_ENABLE, Manager::instance().getConfigString (section, TLS_ENABLE)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_CA_LIST_FILE, Manager::instance().getConfigString (section, TLS_CA_LIST_FILE)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_CERTIFICATE_FILE, Manager::instance().getConfigString (section, TLS_CERTIFICATE_FILE)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_PRIVATE_KEY_FILE, Manager::instance().getConfigString (section, TLS_PRIVATE_KEY_FILE)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_PASSWORD, Manager::instance().getConfigString (section, TLS_PASSWORD)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_METHOD, Manager::instance().getConfigString (section, TLS_METHOD)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_CIPHERS, Manager::instance().getConfigString (section, TLS_CIPHERS)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_SERVER_NAME, Manager::instance().getConfigString (section, TLS_SERVER_NAME)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_VERIFY_SERVER, Manager::instance().getConfigString (section, TLS_VERIFY_SERVER)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_VERIFY_CLIENT, Manager::instance().getConfigString (section, TLS_VERIFY_CLIENT)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_REQUIRE_CLIENT_CERTIFICATE, Manager::instance().getConfigString (section, TLS_REQUIRE_CLIENT_CERTIFICATE)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_NEGOTIATION_TIMEOUT_SEC, Manager::instance().getConfigString (section, TLS_NEGOTIATION_TIMEOUT_SEC)));
    tlsSettings.insert (std::pair<std::string, std::string>
                        (TLS_NEGOTIATION_TIMEOUT_MSEC, Manager::instance().getConfigString (section, TLS_NEGOTIATION_TIMEOUT_MSEC)));
    return tlsSettings;
}

void
ConfigurationManager::setTlsSettings (const std::string& section, const std::map< std::string, std::string >& details)
{
    std::map<std::string, std::string> map_cpy = details;
    std::map<std::string, std::string>::iterator it;

    it = map_cpy.find (TLS_ENABLE);

    if (it != details.end()) {
        Manager::instance().setConfig (section, TLS_ENABLE, it->second);
    }

    it = map_cpy.find (TLS_CA_LIST_FILE);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_CA_LIST_FILE, it->second);
    }

    it = map_cpy.find (TLS_CERTIFICATE_FILE);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_CERTIFICATE_FILE, it->second);
    }

    it = map_cpy.find (TLS_PRIVATE_KEY_FILE);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_PRIVATE_KEY_FILE, it->second);
    }

    it = map_cpy.find (TLS_PASSWORD);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_PASSWORD, it->second);
    }

    it = map_cpy.find (TLS_METHOD);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_METHOD, it->second);
    }

    it = map_cpy.find (TLS_CIPHERS);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_CIPHERS, it->second);
    }

    it = map_cpy.find (TLS_SERVER_NAME);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_SERVER_NAME, it->second);
    }

    it = map_cpy.find (TLS_VERIFY_CLIENT);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_VERIFY_CLIENT, it->second);
    }

    it = map_cpy.find (TLS_REQUIRE_CLIENT_CERTIFICATE);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_REQUIRE_CLIENT_CERTIFICATE, it->second);
    }

    it = map_cpy.find (TLS_NEGOTIATION_TIMEOUT_SEC);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_NEGOTIATION_TIMEOUT_SEC, it->second);
    }

    it = map_cpy.find (TLS_NEGOTIATION_TIMEOUT_MSEC);

    if (it != map_cpy.end()) {
        Manager::instance().setConfig (section, TLS_NEGOTIATION_TIMEOUT_MSEC, it->second);
    }

    Manager::instance().saveConfig();

    // Update account details to the client side
    accountsChanged();

}

std::map< std::string, std::string >
ConfigurationManager::getCredential (const std::string& accountID, const int32_t& index)
{
    _debug ("ConfigurationManager::getCredential number %i for accountID %s\n", index, accountID.c_str());

    std::string credentialIndex;
    std::stringstream streamOut;
    streamOut << index;
    credentialIndex = streamOut.str();

    std::string section = std::string ("Credential") + std::string (":") + accountID + std::string (":") + credentialIndex;

    std::map<std::string, std::string> credentialInformation;
    std::string username = Manager::instance().getConfigString (section, USERNAME);
    std::string password = Manager::instance().getConfigString (section, PASSWORD);
    std::string realm = Manager::instance().getConfigString (section, REALM);

    credentialInformation.insert (std::pair<std::string, std::string> (USERNAME, username));
    credentialInformation.insert (std::pair<std::string, std::string> (PASSWORD, password));
    credentialInformation.insert (std::pair<std::string, std::string> (REALM, realm));

    return credentialInformation;
}

int32_t
ConfigurationManager::getNumberOfCredential (const std::string& accountID)
{
    _debug ("ConfigurationManager::getNumberOfCredential\n");
    return Manager::instance().getConfigInt (accountID, CONFIG_CREDENTIAL_NUMBER);
}

void
ConfigurationManager::setNumberOfCredential (const std::string& accountID, const int32_t& number)
{
    if (accountID != AccountNULL || !accountID.empty()) {
        Manager::instance().setConfig (accountID, CONFIG_CREDENTIAL_NUMBER, number);
    }
}

void
ConfigurationManager::setCredential (const std::string& accountID, const int32_t& index,
                                     const std::map< std::string, std::string >& details)
{
    _debug ("ConfigurationManager::setCredential received\n");
    Manager::instance().setCredential (accountID, index, details);
}

void
ConfigurationManager::deleteAllCredential (const std::string& accountID)
{
    _debug ("ConfigurationManager::deleteAllCredential received\n");
    Manager::instance().deleteAllCredential (accountID);
}

void
ConfigurationManager::setAccountDetails (const std::string& accountID,
        const std::map< std::string, std::string >& details)
{
    _debug ("ConfigurationManager::setAccountDetails received\n");
    Manager::instance().setAccountDetails (accountID, details);
}

void
ConfigurationManager::sendRegister (const std::string& accountID, const int32_t& expire)
{
    _debug ("ConfigurationManager::sendRegister received\n");
    Manager::instance().sendRegister (accountID, expire);
}

std::string
ConfigurationManager::addAccount (const std::map< std::string, std::string >& details)
{
    _debug ("ConfigurationManager::addAccount received\n");
    return Manager::instance().addAccount (details);
}


void
ConfigurationManager::removeAccount (const std::string& accoundID)
{
    _debug ("ConfigurationManager::removeAccount received\n");
    return Manager::instance().removeAccount (accoundID);
}

std::vector< std::string >
ConfigurationManager::getAccountList()
{
    _debug ("ConfigurationManager::getAccountList received\n");
    return Manager::instance().getAccountList();
}

//TODO
std::vector< std::string >
ConfigurationManager::getToneLocaleList()
{
    std::vector< std::string > ret;
    _debug ("ConfigurationManager::getToneLocaleList received\n");
    return ret;
}

//TODO
std::string
ConfigurationManager::getVersion()
{
    std::string ret ("");
    _debug ("ConfigurationManager::getVersion received\n");
    return ret;
}

//TODO
std::vector< std::string >
ConfigurationManager::getRingtoneList()
{
    std::vector< std::string >  ret;
    _debug ("ConfigurationManager::getRingtoneList received\n");
    return ret;
}



std::vector< std::string  >
ConfigurationManager::getCodecList (void)
{
    _debug ("ConfigurationManager::getCodecList received\n");
    return Manager::instance().getCodecList();
}

std::vector<std::string>
ConfigurationManager::getSupportedTlsMethod (void)
{
    _debug ("ConfigurationManager::getSupportedTlsMethod received\n");
    std::vector<std::string> method;
    method.push_back ("Default");
    method.push_back ("TLSv1");
    method.push_back ("SSLv2");
    method.push_back ("SSLv3");
    method.push_back ("SSLv23");
    return method;
}

std::vector< std::string >
ConfigurationManager::getCodecDetails (const int32_t& payload)
{
    _debug ("ConfigurationManager::getCodecDetails received\n");
    return Manager::instance().getCodecDetails (payload);
}

std::vector< std::string >
ConfigurationManager::getActiveCodecList()
{
    _debug ("ConfigurationManager::getActiveCodecList received\n");
    return Manager::instance().getActiveCodecList();
}

void
ConfigurationManager::setActiveCodecList (const std::vector< std::string >& list)
{
    _debug ("ConfigurationManager::setActiveCodecList received\n");
    Manager::instance().setActiveCodecList (list);
}

// Audio devices related methods
std::vector< std::string >
ConfigurationManager::getInputAudioPluginList()
{
    _debug ("ConfigurationManager::getInputAudioPluginList received\n");
    return Manager::instance().getInputAudioPluginList();
}

std::vector< std::string >
ConfigurationManager::getOutputAudioPluginList()
{
    _debug ("ConfigurationManager::getOutputAudioPluginList received\n");
    return Manager::instance().getOutputAudioPluginList();
}

void
ConfigurationManager::setInputAudioPlugin (const std::string& audioPlugin)
{
    _debug ("ConfigurationManager::setInputAudioPlugin received\n");
    return Manager::instance().setInputAudioPlugin (audioPlugin);
}

void
ConfigurationManager::setOutputAudioPlugin (const std::string& audioPlugin)
{
    _debug ("ConfigurationManager::setOutputAudioPlugin received\n");
    return Manager::instance().setOutputAudioPlugin (audioPlugin);
}

std::vector< std::string >
ConfigurationManager::getAudioOutputDeviceList()
{
    _debug ("ConfigurationManager::getAudioOutputDeviceList received\n");
    return Manager::instance().getAudioOutputDeviceList();
}

void
ConfigurationManager::setAudioOutputDevice (const int32_t& index)
{
    _debug ("ConfigurationManager::setAudioOutputDevice received\n");
    return Manager::instance().setAudioOutputDevice (index);
}

std::vector< std::string >
ConfigurationManager::getAudioInputDeviceList()
{
    _debug ("ConfigurationManager::getAudioInputDeviceList received\n");
    return Manager::instance().getAudioInputDeviceList();
}

void
ConfigurationManager::setAudioInputDevice (const int32_t& index)
{
    _debug ("ConfigurationManager::setAudioInputDevice received\n");
    return Manager::instance().setAudioInputDevice (index);
}

std::vector< std::string >
ConfigurationManager::getCurrentAudioDevicesIndex()
{
    _debug ("ConfigurationManager::getCurrentAudioDeviceIndex received\n");
    return Manager::instance().getCurrentAudioDevicesIndex();
}

int32_t
ConfigurationManager::getAudioDeviceIndex (const std::string& name)
{
    _debug ("ConfigurationManager::getAudioDeviceIndex received\n");
    return Manager::instance().getAudioDeviceIndex (name);
}

std::string
ConfigurationManager::getCurrentAudioOutputPlugin (void)
{
    _debug ("ConfigurationManager::getCurrentAudioOutputPlugin received\n");
    return Manager::instance().getCurrentAudioOutputPlugin();
}


std::vector< std::string >
ConfigurationManager::getPlaybackDeviceList()
{
    std::vector< std::string >  ret;
    _debug ("ConfigurationManager::getPlaybackDeviceList received\n");
    return ret;
}

std::vector< std::string >
ConfigurationManager::getRecordDeviceList()
{
    std::vector< std::string >  ret;
    _debug ("ConfigurationManager::getRecordDeviceList received\n");
    return ret;

}

bool
ConfigurationManager::isMd5CredentialHashing (void)
{
    bool isEnabled = Manager::instance().getConfigBool (PREFERENCES, CONFIG_MD5HASH);
    return isEnabled;
}

void
ConfigurationManager::setMd5CredentialHashing (const bool& enabled)
{
    if (enabled) {
        Manager::instance().setConfig (PREFERENCES, CONFIG_MD5HASH, TRUE_STR);
    } else {
        Manager::instance().setConfig (PREFERENCES, CONFIG_MD5HASH, FALSE_STR);
    }
}

int32_t
ConfigurationManager::isIax2Enabled (void)
{
    return Manager::instance().isIax2Enabled();
}

void
ConfigurationManager::ringtoneEnabled (void)
{
    Manager::instance().ringtoneEnabled();
}

int32_t
ConfigurationManager::isRingtoneEnabled (void)
{
    return Manager::instance().isRingtoneEnabled();
}

std::string
ConfigurationManager::getRingtoneChoice (void)
{
    return Manager::instance().getRingtoneChoice();
}

void
ConfigurationManager::setRingtoneChoice (const std::string& tone)
{
    Manager::instance().setRingtoneChoice (tone);
}

std::string
ConfigurationManager::getRecordPath (void)
{
    return Manager::instance().getRecordPath();
}

void
ConfigurationManager::setRecordPath (const std::string& recPath)
{
    Manager::instance().setRecordPath (recPath);
}

int32_t
ConfigurationManager::getDialpad (void)
{
    return Manager::instance().getDialpad();
}

void
ConfigurationManager::setDialpad (void)
{
    Manager::instance().setDialpad();
}

int32_t
ConfigurationManager::getSearchbar (void)
{
    return Manager::instance().getSearchbar();
}

void
ConfigurationManager::setSearchbar (void)
{
    Manager::instance().setSearchbar();
}

int32_t
ConfigurationManager::getVolumeControls (void)
{
    return Manager::instance().getVolumeControls();
}

void
ConfigurationManager::setVolumeControls (void)
{
    Manager::instance().setVolumeControls();
}

int32_t
ConfigurationManager::getHistoryLimit (void)
{
    return Manager::instance().getHistoryLimit();
}

void
ConfigurationManager::setHistoryLimit (const int32_t& days)
{
    Manager::instance().setHistoryLimit (days);
}


void ConfigurationManager::setHistoryEnabled (void)
{
    Manager::instance ().setHistoryEnabled ();
}

std::string ConfigurationManager::getHistoryEnabled (void)
{
    return Manager::instance ().getHistoryEnabled ();
}

void
ConfigurationManager::startHidden (void)
{
    _debug ("Manager received startHidden\n");
    Manager::instance().startHidden();
}

int32_t
ConfigurationManager::isStartHidden (void)
{
    _debug ("Manager received isStartHidden\n");
    return Manager::instance().isStartHidden();
}

void
ConfigurationManager::switchPopupMode (void)
{
    _debug ("Manager received switchPopupMode\n");
    Manager::instance().switchPopupMode();
}

int32_t
ConfigurationManager::popupMode (void)
{
    _debug ("Manager received popupMode\n");
    return Manager::instance().popupMode();
}

void
ConfigurationManager::setNotify (void)
{
    _debug ("Manager received setNotify\n");
    Manager::instance().setNotify();
}

int32_t
ConfigurationManager::getNotify (void)
{
    _debug ("Manager received getNotify\n");
    return Manager::instance().getNotify();
}

void
ConfigurationManager::setAudioManager (const int32_t& api)
{
    _debug ("Manager received setAudioManager\n");
    Manager::instance().setAudioManager (api);
}

int32_t
ConfigurationManager::getAudioManager (void)
{
    _debug ("Manager received getAudioManager\n");
    return Manager::instance().getAudioManager();
}

void
ConfigurationManager::setMailNotify (void)
{
    _debug ("Manager received setMailNotify\n");
    Manager::instance().setMailNotify();
}

int32_t
ConfigurationManager::getMailNotify (void)
{
    _debug ("Manager received getMailNotify\n");
    return Manager::instance().getMailNotify();
}

std::string
ConfigurationManager::getPulseAppVolumeControl (void)
{
    return Manager::instance().getPulseAppVolumeControl();
}

void
ConfigurationManager::setPulseAppVolumeControl (void)
{
    Manager::instance().setPulseAppVolumeControl();
}

int32_t
ConfigurationManager::getSipPort (void)
{
    return Manager::instance().getSipPort();
}

void
ConfigurationManager::setSipPort (const int32_t& portNum)
{
    _debug ("Manager received setSipPort: %d\n", portNum);
    Manager::instance().setSipPort (portNum);
}

std::map<std::string, int32_t> ConfigurationManager::getAddressbookSettings (void)
{
    return Manager::instance().getAddressbookSettings ();
}

void ConfigurationManager::setAddressbookSettings (const std::map<std::string, int32_t>& settings)
{
    Manager::instance().setAddressbookSettings (settings);
}

std::vector< std::string > ConfigurationManager::getAddressbookList (void)
{
    return Manager::instance().getAddressbookList();
}

void ConfigurationManager::setAddressbookList (const std::vector< std::string >& list)
{
    _debug ("Manager received setAddressbookList\n") ;
    Manager::instance().setAddressbookList (list);
}

std::map<std::string,std::string> ConfigurationManager::getHookSettings (void)
{
    return Manager::instance().getHookSettings ();
}

void ConfigurationManager::setHookSettings (const std::map<std::string, std::string>& settings)
{
    Manager::instance().setHookSettings (settings);
}

void  ConfigurationManager::setAccountsOrder (const std::string& order)
{
    Manager::instance().setAccountsOrder (order);
}

std::map <std::string, std::string> ConfigurationManager::getHistory (void)
{
    return Manager::instance().send_history_to_client ();
}

void ConfigurationManager::setHistory (const std::map <std::string, std::string>& entries)
{
    Manager::instance().receive_history_from_client (entries);
}

std::vector<std::string> ConfigurationManager::getAllIpInterface (void)
{
    _debug ("ConfigurationManager::getAllIpInterface received\n");

    std::vector<std::string> vector;
    SIPVoIPLink * sipLink = NULL;
    sipLink = SIPVoIPLink::instance ("");

    if (sipLink != NULL) {
        vector = sipLink->getAllIpInterface();
    }

    return vector;
}
