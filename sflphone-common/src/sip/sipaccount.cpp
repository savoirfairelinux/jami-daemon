/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
*
*  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
*  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include "sipaccount.h"
#include "manager.h"
#include "user_cfg.h"
#include <pwd.h>
#include <sstream>

Credentials::Credentials() : credentialCount(0) {}

Credentials::~Credentials() {}

void Credentials::serialize(Conf::YamlEmitter *emitter)
{
  
}

void Credentials::unserialize(Conf::MappingNode *map)
{

  Conf::ScalarNode *val = NULL;

  _debug("SipAccount: Unserialize");

  val = (Conf::ScalarNode *)(map->getValue(credentialCountKey));
  if(val) { credentialCount = atoi(val->getValue().data()); val = NULL; }
}



SIPAccount::SIPAccount (const AccountID& accountID)
        : Account (accountID, "sip")
	, _routeSet("")
        , _regc (NULL)
        , _bRegister (false)
        , _registrationExpire ("")
        , _publishedSameasLocal (true)
        , _publishedIpAddress ("")
        , _localPort (atoi (DEFAULT_SIP_PORT))
        , _publishedPort (atoi (DEFAULT_SIP_PORT))
	, _tlsListenerPort (atoi (DEFAULT_SIP_TLS_PORT))
        , _transportType (PJSIP_TRANSPORT_UNSPECIFIED)
        , _transport (NULL)
        , _resolveOnce (false)
        , _credentialCount (0)
        , _cred (NULL)
        , _realm (DEFAULT_REALM)
        , _authenticationUsername ("")
        , _tlsSetting (NULL)
	, _dtmfType(OVERRTP)
        , _displayName ("")
        , _tlsEnable("")
	, _tlsPortStr("")
	, _tlsCaListFile("")
	, _tlsCertificateFile("")
	, _tlsPrivateKeyFile("")
	, _tlsPassword("")
        , _tlsMethod("")
	, _tlsCiphers("")
	, _tlsServerName("")
	, _tlsVerifyServer(false)
	, _tlsVerifyClient(false)
	, _tlsRequireClientCertificate(false)
	, _tlsNegotiationTimeoutSec("")
	, _tlsNegotiationTimeoutMsec("")
	, _stunServer("")
	, _tlsEnabled(false)
	, _stunEnabled(false)
	  // , _routeSet("")
	  // , _realm("")
	, _authenticationUsename("")
	  // , _tlsListenerPort("5061")
	, _srtpEnabled(false)
	, _srtpKeyExchange("")
	, _srtpFallback(false)
	, _zrtpDisplaySas(false)
	, _zrtpDisplaySasOnce(false)
	, _zrtpHelloHash(false)
	, _zrtpNotSuppWarning(false)
	, _useragent("SFLphone")
{
    
    _debug("Sip account constructor called");
  
    // IP2IP settings must be loaded before singleton instanciation, cannot call it here... 

    // _link = SIPVoIPLink::instance ("");

    /* Represents the number of SIP accounts connected the same link */
    // dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();

}

SIPAccount::~SIPAccount()
{
    /* One SIP account less connected to the sip voiplink */
    dynamic_cast<SIPVoIPLink*> (_link)->decrementClients();

    /* Delete accounts-related information */
    _regc = NULL;
    free (_cred);
    free (_tlsSetting);
}

void SIPAccount::serialize(Conf::YamlEmitter *emitter) {

  _debug("SipAccount: serialize %s", _accountID.c_str());


  Conf::MappingNode accountmap(NULL);
  Conf::MappingNode credentialmap(NULL);
  Conf::MappingNode srtpmap(NULL);
  Conf::MappingNode zrtpmap(NULL);
  Conf::MappingNode tlsmap(NULL);

  Conf::ScalarNode id(Account::_accountID);
  Conf::ScalarNode username(Account::_username);
  Conf::ScalarNode password(Account::_password);
  Conf::ScalarNode alias(Account::_alias);
  Conf::ScalarNode hostname(Account::_hostname);
  Conf::ScalarNode enable(_enabled ? "true" : "false");
  Conf::ScalarNode type(Account::_type);
  Conf::ScalarNode expire(_registrationExpire);
  Conf::ScalarNode interface(_interface);
  std::stringstream portstr; portstr << _localPort;
  Conf::ScalarNode port(portstr.str());

  Conf::ScalarNode mailbox("97");
  Conf::ScalarNode publishAddr(_publishedIpAddress);
  std::stringstream publicportstr; publicportstr << _publishedPort;
  Conf::ScalarNode publishPort(publicportstr.str());
  Conf::ScalarNode sameasLocal(_publishedSameasLocal ? "true" : "false");
  Conf::ScalarNode resolveOnce(_resolveOnce ? "true" : "false");      
  Conf::ScalarNode codecs("");
  Conf::ScalarNode stunServer(std::string(_stunServerName.ptr, _stunServerName.slen));
  Conf::ScalarNode stunEnabled(_stunEnabled ? "true" : "false");
  Conf::ScalarNode displayName(_displayName);
  Conf::ScalarNode dtmfType(_dtmfType==0 ? "overrtp" : "sipinfo");

  std::stringstream countstr; countstr << _credentialCount;
  Conf::ScalarNode count(countstr.str());

  Conf::ScalarNode srtpenabled(_srtpEnabled ? "true" : "false");
  Conf::ScalarNode keyExchange(_srtpKeyExchange);
  Conf::ScalarNode rtpFallback(_srtpFallback ? "true" : "false");

  Conf::ScalarNode displaySas(_zrtpDisplaySas ? "true" : "false");
  Conf::ScalarNode displaySasOnce(_zrtpDisplaySasOnce ? "true" : "false");
  Conf::ScalarNode helloHashEnabled(_zrtpHelloHash ? "true" : "false");
  Conf::ScalarNode notSuppWarning(_zrtpNotSuppWarning ? "true" : "false");

  Conf::ScalarNode tlsport(_tlsPortStr);
  Conf::ScalarNode certificate(_tlsCertificateFile);
  Conf::ScalarNode calist(_tlsCaListFile);
  Conf::ScalarNode ciphers(_tlsCiphers);
  Conf::ScalarNode tlsenabled(_tlsEnable);
  Conf::ScalarNode tlsmethod(_tlsMethod);
  Conf::ScalarNode timeout(_tlsNegotiationTimeoutSec);
  Conf::ScalarNode tlspassword(_tlsPassword);
  Conf::ScalarNode privatekey(_tlsPrivateKeyFile);
  Conf::ScalarNode requirecertif(_tlsRequireClientCertificate ? "true" : "false");
  Conf::ScalarNode server(_tlsServerName);
  Conf::ScalarNode verifyclient(_tlsVerifyServer ? "true" : "false");
  Conf::ScalarNode verifyserver(_tlsVerifyClient ? "true" : "false");

  accountmap.setKeyValue(aliasKey, &alias);
  accountmap.setKeyValue(typeKey, &type);
  accountmap.setKeyValue(idKey, &id);
  accountmap.setKeyValue(usernameKey, &username);
  accountmap.setKeyValue(passwordKey, &password);
  accountmap.setKeyValue(hostnameKey, &hostname);
  accountmap.setKeyValue(accountEnableKey, &enable);
  accountmap.setKeyValue(mailboxKey, &mailbox);
  accountmap.setKeyValue(expireKey, &expire);
  accountmap.setKeyValue(interfaceKey, &interface);
  accountmap.setKeyValue(portKey, &port);
  accountmap.setKeyValue(publishAddrKey, &publishAddr);
  accountmap.setKeyValue(publishPortKey, &publishPort);
  accountmap.setKeyValue(sameasLocalKey, &sameasLocal);
  accountmap.setKeyValue(resolveOnceKey, &resolveOnce);
  accountmap.setKeyValue(dtmfTypeKey, &dtmfType);
  accountmap.setKeyValue(displayNameKey, &displayName);

  accountmap.setKeyValue(srtpKey, &srtpmap);
  srtpmap.setKeyValue(srtpEnableKey, &srtpenabled);
  srtpmap.setKeyValue(keyExchangeKey, &keyExchange);
  srtpmap.setKeyValue(rtpFallbackKey, &rtpFallback);
  
  accountmap.setKeyValue(zrtpKey, &zrtpmap);
  zrtpmap.setKeyValue(displaySasKey, &displaySas);
  zrtpmap.setKeyValue(displaySasOnceKey, &displaySasOnce);
  zrtpmap.setKeyValue(helloHashEnabledKey, &helloHashEnabled);
  zrtpmap.setKeyValue(notSuppWarningKey, &notSuppWarning);

  accountmap.setKeyValue(credKey, &credentialmap);
  credentialmap.setKeyValue(credentialCountKey, &count);

  accountmap.setKeyValue(tlsKey, &tlsmap);
  tlsmap.setKeyValue(tlsPortKey, &tlsport);
  tlsmap.setKeyValue(certificateKey, &certificate);
  tlsmap.setKeyValue(calistKey, &calist);
  tlsmap.setKeyValue(ciphersKey, &ciphers);
  tlsmap.setKeyValue(tlsEnableKey, &tlsenabled);
  tlsmap.setKeyValue(methodKey, &tlsmethod);
  tlsmap.setKeyValue(timeoutKey, &timeout);
  tlsmap.setKeyValue(tlsPasswordKey, &tlspassword);
  tlsmap.setKeyValue(privateKeyKey, &privatekey);
  tlsmap.setKeyValue(requireCertifKey, &requirecertif);
  tlsmap.setKeyValue(serverKey, &server);
  tlsmap.setKeyValue(verifyClientKey, &verifyclient);
  tlsmap.setKeyValue(verifyServerKey, &verifyserver);

  try{
    emitter->serializeAccount(&accountmap);
  }
  catch (Conf::YamlEmitterException &e) {
    _error("ConfigTree: %s", e.what());
  }
}


void SIPAccount::unserialize(Conf::MappingNode *map) 
{
  Conf::ScalarNode *val;
  Conf::MappingNode *srtpMap;
  Conf::MappingNode *tlsMap;
  Conf::MappingNode *zrtpMap;
  Conf::MappingNode *credMap;

  _debug("SipAccount: Unserialize");

  val = (Conf::ScalarNode *)(map->getValue(aliasKey));
  if(val) { _alias = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(typeKey));
  if(val) { _type = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(idKey));
  if(val) { _accountID = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(usernameKey));
  if(val) { _username = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(passwordKey));
  if(val) { _password = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(hostnameKey));
  if(val) { _hostname = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(accountEnableKey));
  if(val) { _enabled = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  //  val = (Conf::ScalarNode *)(map->getValue(mailboxKey));

  val = (Conf::ScalarNode *)(map->getValue(codecsKey));
  if(val) { val = NULL; }
  // _codecOrder = val->getValue();
  
  val = (Conf::ScalarNode *)(map->getValue(expireKey));
  if(val) { _registrationExpire = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(interfaceKey));
  if(val) { _interface = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(portKey));
  if(val) { _localPort = atoi(val->getValue().data()); val = NULL; }
  // val = (Conf::ScalarNode *)(map->getValue(mailboxKey));
  val = (Conf::ScalarNode *)(map->getValue(publishAddrKey));
  if(val) { _publishedIpAddress = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(publishPortKey));
  if(val) { _publishedPort = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(sameasLocalKey));
  if(val) { _publishedSameasLocal = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(resolveOnceKey));
  if(val) { _resolveOnce = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(dtmfTypeKey));
  if(val) { val = NULL; }
  // _dtmfType = atoi(val->getValue();

  // stun enabled
  val = (Conf::ScalarNode *)(map->getValue(stunEnabledKey));
  if(val) { _stunEnabled = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(stunServerKey));
  if(val) { _stunServer = val->getValue(); val = NULL; }
  _stunServerName = pj_str ( (char*) _stunServer.data());

  credMap = (Conf::MappingNode *)(map->getValue(credKey));
  credentials.unserialize(credMap);

  val = (Conf::ScalarNode *)(map->getValue(displayNameKey));
  if(val) { _displayName = val->getValue(); val = NULL; }

  // get srtp submap
  srtpMap = (Conf::MappingNode *)(map->getValue(srtpKey));
  if(!srtpMap)
    throw SipAccountException(" did not found srtp map");

  val = (Conf::ScalarNode *)(srtpMap->getValue(srtpEnableKey));
  if(val) { _srtpEnabled = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(srtpMap->getValue(keyExchangeKey));
  if(val) { _srtpKeyExchange = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(srtpMap->getValue(rtpFallbackKey));
  if(val) { _srtpFallback = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }

  // get zrtp submap
  zrtpMap = (Conf::MappingNode *)(map->getValue(zrtpKey));
  if(!zrtpMap)
    throw SipAccountException(" did not found zrtp map");

  val = (Conf::ScalarNode *)(zrtpMap->getValue(displaySasKey));
  if(val) { _zrtpDisplaySas = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(zrtpMap->getValue(displaySasOnceKey));
  if(val) { _zrtpDisplaySasOnce = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(zrtpMap->getValue(helloHashEnabledKey));
  if(val) { _zrtpHelloHash = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(zrtpMap->getValue(notSuppWarningKey));
  if(val) { _zrtpNotSuppWarning = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }

  // get tls submap
  tlsMap = (Conf::MappingNode *)(map->getValue(tlsKey));
  if(!tlsMap)
    throw SipAccountException(" did not found tls map");

  val = (Conf::ScalarNode *)(tlsMap->getValue(tlsEnableKey));
  if(val) { _tlsEnable = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(tlsPortKey));
  if(val) { _tlsPortStr = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(certificateKey));
  if(val) { _tlsCertificateFile = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(calistKey));
  if(val) { _tlsCaListFile = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(ciphersKey));
  if(val) { _tlsCiphers = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(methodKey));
  if(val) { _tlsMethod = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(timeoutKey));
  if(val) _tlsNegotiationTimeoutSec = val->getValue();
  if(val) { _tlsNegotiationTimeoutMsec = val->getValue(); val=NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(tlsPasswordKey));
  if(val) { _tlsPassword = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(privateKeyKey));
  if(val) { _tlsPrivateKeyFile = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(requireCertifKey));
  if(val) { _tlsRequireClientCertificate = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(serverKey));
  if(val) { _tlsServerName = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(verifyClientKey));
  if(val) { _tlsVerifyServer = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(tlsMap->getValue(verifyServerKey));
  if(val) { _tlsVerifyClient = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }

}


// void SIPAccount::setVoIPLink(VoIPLink *link) {
void SIPAccount::setVoIPLink() {

    _link = SIPVoIPLink::instance ("");
    dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();

}


int SIPAccount::initCredential (void)
{
    int credentialCount = 0;
    credentialCount = credentials.getCredentialCount();// Manager::instance().getConfigInt (_accountID, CONFIG_CREDENTIAL_NUMBER);
    credentialCount += 1;

    bool md5HashingEnabled = false;
    int dataType = 0;
    md5HashingEnabled = Manager::instance().preferences.getMd5Hash(); // Manager::instance().getConfigBool (PREFERENCES, CONFIG_MD5HASH);
    std::string digest;

    // Create the credential array
    pjsip_cred_info * cred_info = (pjsip_cred_info *) malloc (sizeof (pjsip_cred_info) * (credentialCount));

    if (cred_info == NULL) {
        _error ("SipAccount: Error: Failed to set cred_info for account %s", _accountID.c_str());
        return !SUCCESS;
    }

    pj_bzero (cred_info, sizeof (pjsip_cred_info) *credentialCount);

    // Use authentication username if provided
    if (!_authenticationUsername.empty()) {
        cred_info[0].username = pj_str (strdup (_authenticationUsername.c_str()));
    } else {
        cred_info[0].username = pj_str (strdup (_username.c_str()));
    }

    // Set password
    cred_info[0].data =  pj_str (strdup (_password.c_str()));

    // Set realm for that credential. * by default.
    cred_info[0].realm = pj_str (strdup (_realm.c_str()));

    // We want to make sure that the password is really
    // 32 characters long. Otherwise, pjsip will fail
    // on an assertion.
    if (md5HashingEnabled && _password.length() == 32) {
        dataType = PJSIP_CRED_DATA_DIGEST;
        _debug ("Setting digest ");
    } else {
        dataType = PJSIP_CRED_DATA_PLAIN_PASSWD;
    }

    // Set the datatype
    cred_info[0].data_type = dataType;
    
    // Set the secheme
    cred_info[0].scheme = pj_str ( (char*) "digest");

    int i;

    for (i = 1; i < credentialCount; i++) {
        std::string credentialIndex;
        std::stringstream streamOut;
        streamOut << i - 1;
        credentialIndex = streamOut.str();

        std::string section = std::string ("Credential") + std::string (":") + _accountID + std::string (":") + credentialIndex;

        std::string username = Manager::instance().getConfigString (section, USERNAME);
        std::string password = Manager::instance().getConfigString (section, PASSWORD);
        std::string realm = Manager::instance().getConfigString (section, REALM);

        cred_info[i].username = pj_str (strdup (username.c_str()));
        cred_info[i].data = pj_str (strdup (password.c_str()));
        cred_info[i].realm = pj_str (strdup (realm.c_str()));

        // We want to make sure that the password is really
        // 32 characters long. Otherwise, pjsip will fail
        // on an assertion.

        if (md5HashingEnabled && _password.length() == 32) {
            dataType = PJSIP_CRED_DATA_DIGEST;
            _debug ("Setting digest ");
        } else {
            dataType = PJSIP_CRED_DATA_PLAIN_PASSWD;
        }

        cred_info[i].data_type = dataType;

        cred_info[i].scheme = pj_str ( (char*) "digest");

        _debug ("Setting credential %d realm = %s passwd = %s username = %s data_type = %d", i, realm.c_str(), password.c_str(), username.c_str(), cred_info[i].data_type);
    }

    _credentialCount = credentialCount;

    _cred = cred_info;

    return SUCCESS;
}


int SIPAccount::registerVoIPLink()
{
    _debug ("Account: Register account %s", getAccountID().c_str());

    // Init general settings
    // loadConfig();

    if (_hostname.length() >= PJ_MAX_HOSTNAME) {
        return !SUCCESS;
    }

    // Init set of additional credentials, if supplied by the user
    initCredential();

    // Init TLS settings if the user wants to use TLS
    bool tlsEnabled = false;//  Manager::instance().getConfigBool (_accountID, TLS_ENABLE);

    if (tlsEnabled) {
        _transportType = PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();
    }

    // Init STUN settings for this account if the user selected it
    bool stunEnabled = _stunEnabled; // Manager::instance().getConfigBool (_accountID, STUN_ENABLE);

    if (stunEnabled) {
        _transportType = PJSIP_TRANSPORT_START_OTHER;
        initStunConfiguration ();
    }
    else {
      _stunServerName = pj_str ((char*) _stunServer.data());
    }

    // In our definition of the
    // ip2ip profile (aka Direct IP Calls),
    // no registration should be performed
    if (_accountID != IP2IP_PROFILE) {
        int status = _link->sendRegister (_accountID);
        ASSERT (status , SUCCESS);
    }

    return SUCCESS;
}

int SIPAccount::unregisterVoIPLink()
{
    _debug ("Unregister account %s" , getAccountID().c_str());

    if (_accountID == IP2IP_PROFILE) {
        return true;
    }

    if (_link->sendUnregister (_accountID)) {
        setRegistrationInfo (NULL);
        return true;
    } else
        return false;

}

pjsip_ssl_method SIPAccount::sslMethodStringToPjEnum (const std::string& method)
{
    if (method == "Default") {
        return PJSIP_SSL_UNSPECIFIED_METHOD;
    }

    if (method == "TLSv1") {
        return PJSIP_TLSV1_METHOD;
    }

    if (method == "SSLv2") {
        return PJSIP_SSLV2_METHOD;
    }

    if (method == "SSLv3") {
        return PJSIP_SSLV3_METHOD;
    }

    if (method == "SSLv23") {
        return PJSIP_SSLV23_METHOD;
    }

    return PJSIP_SSL_UNSPECIFIED_METHOD;
}

void SIPAccount::initTlsConfiguration (void)
{
    /*
     * Initialize structure to zero
     */
    if (_tlsSetting) {
        free (_tlsSetting);
        _tlsSetting = NULL;
    }

    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    // std::string tlsPortStr = Manager::instance().getConfigString(_accountID, TLS_LISTENER_PORT);
    // setTlsListenerPort(atoi(tlsPortStr.c_str()));
    setTlsListenerPort(atoi(_tlsPortStr.c_str()));
    
    _tlsSetting = (pjsip_tls_setting *) malloc (sizeof (pjsip_tls_setting));

    assert (_tlsSetting);

    pjsip_tls_setting_default (_tlsSetting);

    // std::string tlsCaListFile = Manager::instance().getConfigString (_accountID, TLS_CA_LIST_FILE);
    // std::string tlsCertificateFile = Manager::instance().getConfigString (_accountID, TLS_CERTIFICATE_FILE);
    // std::string tlsPrivateKeyFile = Manager::instance().getConfigString (_accountID, TLS_PRIVATE_KEY_FILE);
    // std::string tlsPassword = Manager::instance().getConfigString (_accountID, TLS_PASSWORD);
    // std::string tlsMethod = Manager::instance().getConfigString (_accountID, TLS_METHOD);
    // std::string tlsCiphers = Manager::instance().getConfigString (_accountID, TLS_CIPHERS);
    // std::string tlsServerName = Manager::instance().getConfigString (_accountID, TLS_SERVER_NAME);
    // bool tlsVerifyServer = Manager::instance().getConfigBool (_accountID, TLS_VERIFY_SERVER);
    // bool tlsVerifyClient = Manager::instance().getConfigBool (_accountID, TLS_VERIFY_CLIENT);
    // bool tlsRequireClientCertificate = Manager::instance().getConfigBool (_accountID, TLS_REQUIRE_CLIENT_CERTIFICATE);
    // std::string tlsNegotiationTimeoutSec = Manager::instance().getConfigString (_accountID, TLS_NEGOTIATION_TIMEOUT_SEC);
    // std::string tlsNegotiationTimeoutMsec = Manager::instance().getConfigString (_accountID, TLS_NEGOTIATION_TIMEOUT_MSEC);

    pj_cstr (&_tlsSetting->ca_list_file, _tlsCaListFile.c_str());
    pj_cstr (&_tlsSetting->cert_file, _tlsCertificateFile.c_str());
    pj_cstr (&_tlsSetting->privkey_file, _tlsPrivateKeyFile.c_str());
    pj_cstr (&_tlsSetting->password, _tlsPassword.c_str());
    _tlsSetting->method = sslMethodStringToPjEnum (_tlsMethod);
    pj_cstr (&_tlsSetting->ciphers, _tlsCiphers.c_str());
    pj_cstr (&_tlsSetting->server_name, _tlsServerName.c_str());

    _tlsSetting->verify_server = (_tlsVerifyServer == true) ? PJ_TRUE: PJ_FALSE;
    _tlsSetting->verify_client = (_tlsVerifyClient == true) ? PJ_TRUE: PJ_FALSE;
    _tlsSetting->require_client_cert = (_tlsRequireClientCertificate == true) ? PJ_TRUE: PJ_FALSE;

    _tlsSetting->timeout.sec = atol (_tlsNegotiationTimeoutSec.c_str());
    _tlsSetting->timeout.msec = atol (_tlsNegotiationTimeoutMsec.c_str());

}

void SIPAccount::initStunConfiguration (void)
{
    size_t pos;
    std::string stunServer, serverName, serverPort;

    stunServer = _stunServer; // Manager::instance().getConfigString (_accountID, STUN_SERVER);

    // Init STUN socket
    pos = stunServer.find (':');

    if (pos == std::string::npos) {
        _stunServerName = pj_str ( (char*) stunServer.data());
        _stunPort = PJ_STUN_PORT;
        //stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);
    } else {
        serverName = stunServer.substr (0, pos);
        serverPort = stunServer.substr (pos + 1);
        _stunPort = atoi (serverPort.data());
        _stunServerName = pj_str ( (char*) serverName.data());
        //stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) nPort);
    }
}

void SIPAccount::loadConfig()
{
    // Load primary credential
    setUsername (Manager::instance().getConfigString (_accountID, USERNAME));
    setRouteSet(Manager::instance().getConfigString(_accountID, ROUTESET));
    setPassword (Manager::instance().getConfigString (_accountID, PASSWORD));
    _authenticationUsername = Manager::instance().getConfigString (_accountID, AUTHENTICATION_USERNAME);
    _realm = Manager::instance().getConfigString (_accountID, REALM);
    _resolveOnce = Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_RESOLVE_ONCE) == "1" ? true : false;

    // Load general account settings
    setHostname (Manager::instance().getConfigString (_accountID, HOSTNAME));

    if (Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE).empty()) {
        _registrationExpire = DFT_EXPIRE_VALUE;
    } else {
        _registrationExpire = Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE);
    }

    // Load network settings
    // Local parameters

    // Load local interface
    setLocalInterface(Manager::instance().getConfigString (_accountID, LOCAL_INTERFACE));

    std::string localPort = Manager::instance().getConfigString (_accountID, LOCAL_PORT);
    setLocalPort (atoi (localPort.c_str()));


    // Published parameters
    setPublishedSameasLocal (Manager::instance().getConfigString (_accountID, PUBLISHED_SAMEAS_LOCAL) == TRUE_STR ? true : false);

    std::string publishedPort = Manager::instance().getConfigString (_accountID, PUBLISHED_PORT);

    setPublishedPort (atoi (publishedPort.c_str()));

    setPublishedAddress (Manager::instance().getConfigString (_accountID, PUBLISHED_ADDRESS));

    if(Manager::instance().getConfigString (_accountID, ACCOUNT_DTMF_TYPE) == OVERRTPSTR)
    	_dtmfType = OVERRTP;
	else
		_dtmfType = SIPINFO;

    // Init TLS settings if the user wants to use TLS
    bool tlsEnabled = Manager::instance().getConfigBool (_accountID, TLS_ENABLE);

    if (tlsEnabled) {
        initTlsConfiguration();
        _transportType = PJSIP_TRANSPORT_TLS;
    } else {
        _transportType = PJSIP_TRANSPORT_UDP;
    }

    // Account generic
    Account::loadConfig();
}

bool SIPAccount::fullMatch (const std::string& username, const std::string& hostname)
{
    return (userMatch (username) && hostnameMatch (hostname));
}

bool SIPAccount::userMatch (const std::string& username)
{
    if (username.empty()) {
        return false;
    }

    return (username == getUsername());
}

bool SIPAccount::hostnameMatch (const std::string& hostname)
{
    return (hostname == getHostname());
}

std::string SIPAccount::getMachineName (void)
{
    std::string hostname;
    hostname = std::string (pj_gethostname()->ptr, pj_gethostname()->slen);
    return hostname;
}

std::string SIPAccount::getLoginName (void)
{
    std::string username;

    uid_t uid = getuid();

    struct passwd * user_info = NULL;
    user_info = getpwuid (uid);

    if (user_info != NULL) {
        username = user_info->pw_name;
    }

    return username;
}

std::string SIPAccount::getTransportMapKey(void)
{
    
    std::stringstream out;
    out << getLocalPort();
    std::string localPort = out.str();

    return localPort;
}


std::string SIPAccount::getFromUri (void)
{
    char uri[PJSIP_MAX_URL_SIZE];

    std::string scheme;
    std::string transport;
    std::string username = _username;
    std::string hostname = _hostname;

    // UDP does not require the transport specification

    if (_transportType == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (_transportType));
    } else {
        scheme = "sip:";
        transport = "";
    }

    // Get login name if username is not specified
    if (_username.empty()) {
        username = getLoginName();
    }


    // Get machine hostname if not provided
    if (_hostname.empty()) {
      hostname = getMachineName();
    }


    int len = pj_ansi_snprintf (uri, PJSIP_MAX_URL_SIZE,

                                "<%s%s@%s%s>",
                                scheme.c_str(),
                                username.c_str(),
                                hostname.c_str(),
                                transport.c_str());

    return std::string (uri, len);
}

std::string SIPAccount::getToUri (const std::string& username)
{
    char uri[PJSIP_MAX_URL_SIZE];

    std::string scheme;
    std::string transport;
    std::string hostname = "";

    // UDP does not require the transport specification

    if (_transportType == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (_transportType));
    } else {
        scheme = "sip:";
        transport = "";
    }

    // Check if scheme is already specified
    if (username.find ("sip") == 0) {
        scheme = "";
    }

    // Check if hostname is already specified
    if (username.find ("@") == std::string::npos) {
        // hostname not specified
	hostname = _hostname;
    }

    int len = pj_ansi_snprintf (uri, PJSIP_MAX_URL_SIZE,

                                "<%s%s%s%s%s>",
                                scheme.c_str(),
                                username.c_str(),
                                (hostname.empty()) ? "" : "@",
                                hostname.c_str(),
                                transport.c_str());

    return std::string (uri, len);
}

std::string SIPAccount::getServerUri (void)
{
    char uri[PJSIP_MAX_URL_SIZE];

    std::string scheme;
    std::string transport;
    std::string hostname = _hostname;

    // UDP does not require the transport specification

    _debug("---------------------------- _hostname %s", _hostname.c_str());

    if (_transportType == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (_transportType));
    } else {
        scheme = "sip:";
        transport = "";
    }

    int len = pj_ansi_snprintf (uri, PJSIP_MAX_URL_SIZE,
                                "<%s%s%s>",
                                scheme.c_str(),
                                hostname.c_str(),
                                transport.c_str());

    return std::string (uri, len);
}

std::string SIPAccount::getContactHeader (const std::string& address, const std::string& port)
{
    char contact[PJSIP_MAX_URL_SIZE];
    const char * beginquote, * endquote;

    std::string scheme;
    std::string transport;

    // if IPV6, should be set to []
    beginquote = endquote = "";

    // UDP does not require the transport specification

    if (_transportType == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (_transportType));
    } else {
        scheme = "sip:";
        transport = "";
    }

    // _displayName = Manager::instance().getConfigString (_accountID, DISPLAY_NAME);

    _debug ("Display Name: %s", _displayName.c_str());

    int len = pj_ansi_snprintf (contact, PJSIP_MAX_URL_SIZE,

                                "%s%s<%s%s%s%s%s%s:%d%s>",
                                _displayName.c_str(),
                                (_displayName.empty() ? "" : " "),
                                scheme.c_str(),
                                _username.c_str(),
                                (_username.empty() ? "":"@"),
                                beginquote,
                                address.c_str(),
                                endquote,
                                atoi (port.c_str()),
                                transport.c_str());

    return std::string (contact, len);
}

