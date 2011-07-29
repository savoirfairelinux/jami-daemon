/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <pwd.h>
#include <sstream>
#include <cassert>

namespace {
    const char * const DFT_STUN_SERVER = "stun.sflphone.org"; /** Default STUN server address */
    const char *DFT_EXPIRE_VALUE = "600"; /** Default expire value for registration */
} // end anonymous namespace

Credentials::Credentials() : credentialCount (0) {}

Credentials::~Credentials() {}

void Credentials::setNewCredential (const std::string &username,
                                    const std::string &password,
                                    const std::string &realm)
{
    credentialArray[credentialCount].username = username;
    credentialArray[credentialCount].password = password;
    credentialArray[credentialCount].realm = realm;
}

const CredentialItem *Credentials::getCredential (unsigned index) const
{
    if (index >= credentialCount)
		return NULL;
    return &credentialArray[index];
}

void Credentials::serialize (Conf::YamlEmitter *emitter UNUSED)
{
}

void Credentials::unserialize (Conf::MappingNode *map)
{
    int count;
    map->getValue(credentialCountKey, &count);
    credentialCount = count;
}

namespace {
static void free_cred(pjsip_cred_info *cred)
{
    if (!cred)
        return;

    unsigned i;
    unsigned max = 1; /* getCredentialCount() see #6408 */ 
    for (i = 0 ; i < max ; i++) {
        free(cred[i].username.ptr);
        free(cred[i].data.ptr);
        free(cred[i].realm.ptr);
    }
    free (cred);
}
} // end anonymous namespace

SIPAccount::SIPAccount (const std::string& accountID)
    : Account (accountID, "SIP")
    , _routeSet ("")
    , _pool (NULL)
    , _regc (NULL)
    , _bRegister (false)
    , _registrationExpire ("")
    , _interface ("default")
    , _publishedSameasLocal (true)
    , _publishedIpAddress ("")
    , _localPort (atoi (DEFAULT_SIP_PORT))
    , _publishedPort (atoi (DEFAULT_SIP_PORT))
    , _serviceRoute ("")
    , _tlsListenerPort (atoi (DEFAULT_SIP_TLS_PORT))
    , _transportType (PJSIP_TRANSPORT_UNSPECIFIED)
    , _transport (NULL)
    , _resolveOnce (false)
    , _cred (NULL)
    , _realm (DEFAULT_REALM)
    , _authenticationUsername ("")
    , _tlsSetting (NULL)
    , _dtmfType (OVERRTP)
    , _tlsEnable ("false")
    , _tlsPortStr (DEFAULT_SIP_TLS_PORT)
    , _tlsCaListFile ("")
    , _tlsCertificateFile ("")
    , _tlsPrivateKeyFile ("")
    , _tlsPassword ("")
    , _tlsMethod ("TLSv1")
    , _tlsCiphers ("")
    , _tlsServerName ("")
    , _tlsVerifyServer (true)
    , _tlsVerifyClient (true)
    , _tlsRequireClientCertificate (true)
    , _tlsNegotiationTimeoutSec ("2")
    , _tlsNegotiationTimeoutMsec ("0")
    , _stunServer (DFT_STUN_SERVER)
    , _stunEnabled (false)
    , _srtpEnabled (false)
    , _srtpKeyExchange ("sdes")
    , _srtpFallback (false)
    , _zrtpDisplaySas (true)
    , _zrtpDisplaySasOnce (false)
    , _zrtpHelloHash (true)
    , _zrtpNotSuppWarning (true)
{
    _stunServerName.ptr = NULL;
    _stunServerName.slen = 0;
    _stunPort = 0;
}

SIPAccount::~SIPAccount()
{
    /* One SIP account less connected to the sip voiplink */
    if (_accountID != "default")
        dynamic_cast<SIPVoIPLink*> (_link)->decrementClients();

    /* Delete accounts-related information */
    _regc = NULL;
    free_cred(_cred);
    delete _tlsSetting;
}

void SIPAccount::serialize (Conf::YamlEmitter *emitter)
{
	if(emitter == NULL) {
		_error("SIPAccount: Error: emitter is NULL in serialize");
		return;
	}

    Conf::MappingNode accountmap (NULL);
    Conf::MappingNode credentialmap (NULL);
    Conf::MappingNode srtpmap (NULL);
    Conf::MappingNode zrtpmap (NULL);
    Conf::MappingNode tlsmap (NULL);

    Conf::ScalarNode id (Account::_accountID);
    Conf::ScalarNode username (Account::_username);
    Conf::ScalarNode authenticationUsername (_authenticationUsername);
    Conf::ScalarNode password (Account::_password);
    Conf::ScalarNode alias (Account::_alias);
    Conf::ScalarNode hostname (Account::_hostname);
    Conf::ScalarNode enable (_enabled);
    Conf::ScalarNode type (Account::_type);
    Conf::ScalarNode expire (_registrationExpire);
    Conf::ScalarNode interface (_interface);
    std::stringstream portstr;
    portstr << _localPort;
    Conf::ScalarNode port (portstr.str());
    Conf::ScalarNode serviceRoute (_serviceRoute);

    Conf::ScalarNode mailbox (_mailBox);
    Conf::ScalarNode publishAddr (_publishedIpAddress);
    std::stringstream publicportstr;
    publicportstr << _publishedPort;
    Conf::ScalarNode publishPort (publicportstr.str());
    Conf::ScalarNode sameasLocal (_publishedSameasLocal);
    Conf::ScalarNode resolveOnce (_resolveOnce);
    Conf::ScalarNode codecs (_codecStr);
    Conf::ScalarNode ringtonePath (_ringtonePath);
    Conf::ScalarNode ringtoneEnabled (_ringtoneEnabled);
    Conf::ScalarNode stunServer (_stunServer);
    Conf::ScalarNode stunEnabled (_stunEnabled);
    Conf::ScalarNode displayName (_displayName);
    Conf::ScalarNode dtmfType (_dtmfType==OVERRTP ? "overrtp" : "sipinfo");

    std::stringstream countstr;
    countstr << 0;
    Conf::ScalarNode count (countstr.str());

    Conf::ScalarNode srtpenabled (_srtpEnabled);
    Conf::ScalarNode keyExchange (_srtpKeyExchange);
    Conf::ScalarNode rtpFallback (_srtpFallback);

    Conf::ScalarNode displaySas (_zrtpDisplaySas);
    Conf::ScalarNode displaySasOnce (_zrtpDisplaySasOnce);
    Conf::ScalarNode helloHashEnabled (_zrtpHelloHash);
    Conf::ScalarNode notSuppWarning (_zrtpNotSuppWarning);

    Conf::ScalarNode tlsport (_tlsPortStr);
    Conf::ScalarNode certificate (_tlsCertificateFile);
    Conf::ScalarNode calist (_tlsCaListFile);
    Conf::ScalarNode ciphers (_tlsCiphers);
    Conf::ScalarNode tlsenabled (_tlsEnable);
    Conf::ScalarNode tlsmethod (_tlsMethod);
    Conf::ScalarNode timeout (_tlsNegotiationTimeoutSec);
    Conf::ScalarNode tlspassword (_tlsPassword);
    Conf::ScalarNode privatekey (_tlsPrivateKeyFile);
    Conf::ScalarNode requirecertif (_tlsRequireClientCertificate);
    Conf::ScalarNode server (_tlsServerName);
    Conf::ScalarNode verifyclient (_tlsVerifyServer);
    Conf::ScalarNode verifyserver (_tlsVerifyClient);

    accountmap.setKeyValue (aliasKey, &alias);
    accountmap.setKeyValue (typeKey, &type);
    accountmap.setKeyValue (idKey, &id);
    accountmap.setKeyValue (usernameKey, &username);
    accountmap.setKeyValue (authenticationUsernameKey, &authenticationUsername);
    accountmap.setKeyValue (passwordKey, &password);
    accountmap.setKeyValue (hostnameKey, &hostname);
    accountmap.setKeyValue (accountEnableKey, &enable);
    accountmap.setKeyValue (mailboxKey, &mailbox);
    accountmap.setKeyValue (expireKey, &expire);
    accountmap.setKeyValue (interfaceKey, &interface);
    accountmap.setKeyValue (portKey, &port);
    accountmap.setKeyValue (stunServerKey, &stunServer);
    accountmap.setKeyValue (stunEnabledKey, &stunEnabled);
    accountmap.setKeyValue (publishAddrKey, &publishAddr);
    accountmap.setKeyValue (publishPortKey, &publishPort);
    accountmap.setKeyValue (sameasLocalKey, &sameasLocal);
    accountmap.setKeyValue (resolveOnceKey, &resolveOnce);
    accountmap.setKeyValue (serviceRouteKey, &serviceRoute);
    accountmap.setKeyValue (dtmfTypeKey, &dtmfType);
    accountmap.setKeyValue (displayNameKey, &displayName);
    accountmap.setKeyValue (codecsKey, &codecs);
    accountmap.setKeyValue (ringtonePathKey, &ringtonePath);
    accountmap.setKeyValue (ringtoneEnabledKey, &ringtoneEnabled);

    accountmap.setKeyValue (srtpKey, &srtpmap);
    srtpmap.setKeyValue (srtpEnableKey, &srtpenabled);
    srtpmap.setKeyValue (keyExchangeKey, &keyExchange);
    srtpmap.setKeyValue (rtpFallbackKey, &rtpFallback);

    accountmap.setKeyValue (zrtpKey, &zrtpmap);
    zrtpmap.setKeyValue (displaySasKey, &displaySas);
    zrtpmap.setKeyValue (displaySasOnceKey, &displaySasOnce);
    zrtpmap.setKeyValue (helloHashEnabledKey, &helloHashEnabled);
    zrtpmap.setKeyValue (notSuppWarningKey, &notSuppWarning);

    accountmap.setKeyValue (credKey, &credentialmap);
    credentialmap.setKeyValue (credentialCountKey, &count);

    accountmap.setKeyValue (tlsKey, &tlsmap);
    tlsmap.setKeyValue (tlsPortKey, &tlsport);
    tlsmap.setKeyValue (certificateKey, &certificate);
    tlsmap.setKeyValue (calistKey, &calist);
    tlsmap.setKeyValue (ciphersKey, &ciphers);
    tlsmap.setKeyValue (tlsEnableKey, &tlsenabled);
    tlsmap.setKeyValue (methodKey, &tlsmethod);
    tlsmap.setKeyValue (timeoutKey, &timeout);
    tlsmap.setKeyValue (tlsPasswordKey, &tlspassword);
    tlsmap.setKeyValue (privateKeyKey, &privatekey);
    tlsmap.setKeyValue (requireCertifKey, &requirecertif);
    tlsmap.setKeyValue (serverKey, &server);
    tlsmap.setKeyValue (verifyClientKey, &verifyclient);
    tlsmap.setKeyValue (verifyServerKey, &verifyserver);

    try {
        emitter->serializeAccount (&accountmap);
    } catch (Conf::YamlEmitterException &e) {
        _error ("ConfigTree: %s", e.what());
    }
}



void SIPAccount::unserialize (Conf::MappingNode *map)
{
    Conf::MappingNode *srtpMap;
    Conf::MappingNode *tlsMap;
    Conf::MappingNode *zrtpMap;
    Conf::MappingNode *credMap;

    assert(map);

    map->getValue(aliasKey, &_alias);
    map->getValue(typeKey, &_type);
    map->getValue(usernameKey, &_username);
    map->getValue(authenticationUsernameKey, &_authenticationUsername);
    map->getValue(passwordKey, &_password);
    map->getValue(hostnameKey, &_hostname);
    map->getValue(accountEnableKey, &_enabled);
    map->getValue(mailboxKey, &_mailBox);
    map->getValue(codecsKey, &_codecStr);
    // Update codec list which one is used for SDP offer
    setActiveCodecs (Manager::instance ().unserialize (_codecStr));

    map->getValue(ringtonePathKey, &_ringtonePath);
    map->getValue(ringtoneEnabledKey, &_ringtoneEnabled);
    map->getValue(expireKey, &_registrationExpire);
    map->getValue(interfaceKey, &_interface);
    int port;
    map->getValue(portKey, &port);
    _localPort = port;
    map->getValue(publishAddrKey, &_publishedIpAddress);
    map->getValue(publishPortKey, &port);
    _publishedPort = port;
    map->getValue(sameasLocalKey, &_publishedSameasLocal);
    map->getValue(resolveOnceKey, &_resolveOnce);

    std::string dtmfType;
    map->getValue(dtmfTypeKey, &dtmfType);
    _dtmfType = (dtmfType == "overrtp") ? OVERRTP : SIPINFO;

    map->getValue(serviceRouteKey, &_serviceRoute);
    // stun enabled
    map->getValue(stunEnabledKey, &_stunEnabled);
    map->getValue(stunServerKey, &_stunServer);

    // Init stun server name with default server name
    _stunServerName = pj_str ( (char*) _stunServer.data());

    map->getValue(displayNameKey, &_displayName);


    credMap = (Conf::MappingNode *) (map->getValue (credKey));
    if (credMap)
        credentials.unserialize (credMap);


    // get srtp submap
    srtpMap = (Conf::MappingNode *) (map->getValue (srtpKey));
    if (srtpMap) {
        srtpMap->getValue(srtpEnableKey, &_srtpEnabled);
        srtpMap->getValue(keyExchangeKey, &_srtpKeyExchange);
        srtpMap->getValue(rtpFallbackKey, &_srtpFallback);
    }

    // get zrtp submap
    zrtpMap = (Conf::MappingNode *) (map->getValue (zrtpKey));
    if (zrtpMap) {
        zrtpMap->getValue(displaySasKey, &_zrtpDisplaySas);
        zrtpMap->getValue(displaySasOnceKey, &_zrtpDisplaySasOnce);
        zrtpMap->getValue(helloHashEnabledKey, &_zrtpHelloHash);
        zrtpMap->getValue(notSuppWarningKey, &_zrtpNotSuppWarning);
    }

    // get tls submap
    tlsMap = (Conf::MappingNode *) (map->getValue (tlsKey));
    if (tlsMap) {
        tlsMap->getValue(tlsEnableKey, &_tlsEnable);
        tlsMap->getValue(tlsPortKey, &_tlsPortStr);
        tlsMap->getValue(certificateKey, &_tlsCertificateFile);
        tlsMap->getValue(calistKey, &_tlsCaListFile);
        tlsMap->getValue(ciphersKey, &_tlsCiphers);
        tlsMap->getValue(methodKey, &_tlsMethod);
        tlsMap->getValue(tlsPasswordKey, &_tlsPassword);
        tlsMap->getValue(privateKeyKey, &_tlsPrivateKeyFile);
        tlsMap->getValue(requireCertifKey, &_tlsRequireClientCertificate);
        tlsMap->getValue(serverKey, &_tlsServerName);
        tlsMap->getValue(verifyClientKey, &_tlsVerifyServer);
        tlsMap->getValue(verifyServerKey, &_tlsVerifyClient);
        // FIXME
        tlsMap->getValue(timeoutKey, &_tlsNegotiationTimeoutSec);
        tlsMap->getValue(timeoutKey, &_tlsNegotiationTimeoutMsec);
    }
}


void SIPAccount::setAccountDetails (std::map<std::string, std::string> details)
{
	std::string password, username;

    // Account setting common to SIP and IAX
    setAlias (details[CONFIG_ACCOUNT_ALIAS]);
    setType (details[CONFIG_ACCOUNT_TYPE]);
    username = details[USERNAME];
    setUsername (username);
    setHostname (details[HOSTNAME]);
    password = details[PASSWORD];
    setPassword (password);
    setEnabled ( (details[CONFIG_ACCOUNT_ENABLE] == "true"));
    setRingtonePath (details[CONFIG_RINGTONE_PATH]);
    setRingtoneEnabled ( (details[CONFIG_RINGTONE_ENABLED] == "true"));
    setMailBox (details[CONFIG_ACCOUNT_MAILBOX]);

    // SIP specific account settings

    // general sip settings
    setDisplayName (details[DISPLAY_NAME]);
    setServiceRoute (details[ROUTESET]);
    setLocalInterface (details[LOCAL_INTERFACE]);
    setPublishedSameasLocal (details[PUBLISHED_SAMEAS_LOCAL] == "true");
    setPublishedAddress (details[PUBLISHED_ADDRESS]);
    setLocalPort (atoi (details[LOCAL_PORT].c_str()));
    setPublishedPort (atoi (details[PUBLISHED_PORT].c_str()));
    setStunServer (details[STUN_SERVER]);
    setStunEnabled (details[STUN_ENABLE] == "true");
    setDtmfType ( (details[ACCOUNT_DTMF_TYPE] == "overrtp") ? OVERRTP : SIPINFO);

    setResolveOnce (details[CONFIG_ACCOUNT_RESOLVE_ONCE] == "true");
    setRegistrationExpire (details[CONFIG_ACCOUNT_REGISTRATION_EXPIRE]);

    // sip credential
    _realm = details[REALM];
    _authenticationUsername = details[AUTHENTICATION_USERNAME];

    setUseragent (details[USERAGENT]);

    // srtp settings
    setSrtpEnable (details[SRTP_ENABLE] == "true");
    setSrtpFallback (details[SRTP_RTP_FALLBACK] == "true");
    setZrtpDisplaySas (details[ZRTP_DISPLAY_SAS] == "true");
    setZrtpDiaplaySasOnce (details[ZRTP_DISPLAY_SAS_ONCE] == "true");
    setZrtpNotSuppWarning (details[ZRTP_NOT_SUPP_WARNING] == "true");
    setZrtpHelloHash (details[ZRTP_HELLO_HASH] == "true");
    setSrtpKeyExchange (details[SRTP_KEY_EXCHANGE]);

    // TLS settings
    // The TLS listener is unique and globally defined through IP2IP_PROFILE
    if (_accountID == IP2IP_PROFILE)
    	setTlsListenerPort (atoi (details[TLS_LISTENER_PORT].c_str()));

    setTlsEnable (details[TLS_ENABLE]);
    setTlsCaListFile (details[TLS_CA_LIST_FILE]);
    setTlsCertificateFile (details[TLS_CERTIFICATE_FILE]);
    setTlsPrivateKeyFile (details[TLS_PRIVATE_KEY_FILE]);
    setTlsPassword (details[TLS_PASSWORD]);
    setTlsMethod (details[TLS_METHOD]);
    setTlsCiphers (details[TLS_CIPHERS]);
    setTlsServerName (details[TLS_SERVER_NAME]);
    setTlsVerifyServer (details[TLS_VERIFY_SERVER] == "true");
    setTlsVerifyClient (details[TLS_VERIFY_CLIENT] == "true");
    setTlsRequireClientCertificate (details[TLS_REQUIRE_CLIENT_CERTIFICATE] == "true");
    setTlsNegotiationTimeoutSec (details[TLS_NEGOTIATION_TIMEOUT_SEC]);
    setTlsNegotiationTimeoutMsec (details[TLS_NEGOTIATION_TIMEOUT_MSEC]);

    if (!Manager::instance().preferences.getMd5Hash()) {
        setPassword (password);
    } else {
        // Make sure not to re-hash the password field if
        // it is already saved as a MD5 Hash.
        // TODO: This test is weak. Fix this.
        if ( (password.compare (getPassword()) != 0)) {
            _debug ("SipAccount: Password sent and password from config are different. Re-hashing");
			std::string &authenticationUsername = _authenticationUsername.empty() ? username : _authenticationUsername;
            setPassword (Manager::instance().computeMd5HashFromCredential (authenticationUsername, password, _realm));
        }
    }
}

std::map<std::string, std::string> SIPAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[ACCOUNT_ID] = _accountID;
    // The IP profile does not allow to set an alias
    a[CONFIG_ACCOUNT_ALIAS] = (_accountID == IP2IP_PROFILE) ? IP2IP_PROFILE : getAlias();

    a[CONFIG_ACCOUNT_ENABLE] = isEnabled() ? "true" : "false";
    a[CONFIG_ACCOUNT_TYPE] = getType();
    a[HOSTNAME] = getHostname();
    a[USERNAME] = getUsername();
    a[PASSWORD] = getPassword();

    a[CONFIG_RINGTONE_PATH] = getRingtonePath();
    a[CONFIG_RINGTONE_ENABLED] = getRingtoneEnabled() ? "true" : "false";
    a[CONFIG_ACCOUNT_MAILBOX] = getMailBox();

    RegistrationState state = Unregistered;
    std::string registrationStateCode;
    std::string registrationStateDescription;

    if (_accountID == IP2IP_PROFILE) {
        registrationStateCode = ""; // emtpy field
        registrationStateDescription = "Direct IP call";
    } else {
        state = getRegistrationState();
        int code = getRegistrationStateDetailed().first;
        std::stringstream out;
        out << code;
        registrationStateCode = out.str();
        registrationStateDescription = getRegistrationStateDetailed().second;
    }

    a[REGISTRATION_STATUS] = (_accountID == IP2IP_PROFILE) ? "READY": Manager::instance().mapStateNumberToString (state);
    a[REGISTRATION_STATE_CODE] = registrationStateCode;
    a[REGISTRATION_STATE_DESCRIPTION] = registrationStateDescription;

    // Add sip specific details
    a[ROUTESET] = getServiceRoute();
    a[CONFIG_ACCOUNT_RESOLVE_ONCE] = isResolveOnce() ? "true" : "false";
    a[REALM] = _realm;
    a[USERAGENT] = getUseragent();
    a[AUTHENTICATION_USERNAME] = _authenticationUsername;

    a[CONFIG_ACCOUNT_REGISTRATION_EXPIRE] = getRegistrationExpire();
    a[LOCAL_INTERFACE] = getLocalInterface();
    a[PUBLISHED_SAMEAS_LOCAL] = getPublishedSameasLocal() ? "true" : "false";
    a[PUBLISHED_ADDRESS] = getPublishedAddress();

    std::stringstream localport;
    localport << getLocalPort();
    a[LOCAL_PORT] = localport.str();
    std::stringstream publishedport;
    publishedport << getPublishedPort();
    a[PUBLISHED_PORT] = publishedport.str();
    a[STUN_ENABLE] = isStunEnabled() ? "true" : "false";
    a[STUN_SERVER] = getStunServer();
    a[ACCOUNT_DTMF_TYPE] = (getDtmfType() == OVERRTP) ? "overrtp" : "sipinfo";

    a[SRTP_KEY_EXCHANGE] = getSrtpKeyExchange();
    a[SRTP_ENABLE] = getSrtpEnable() ? "true" : "false";
    a[SRTP_RTP_FALLBACK] = getSrtpFallback() ? "true" : "false";

    a[ZRTP_DISPLAY_SAS] = getZrtpDisplaySas() ? "true" : "false";
    a[ZRTP_DISPLAY_SAS_ONCE] = getZrtpDiaplaySasOnce() ? "true" : "false";
    a[ZRTP_HELLO_HASH] = getZrtpHelloHash() ? "true" : "false";
    a[ZRTP_NOT_SUPP_WARNING] = getZrtpNotSuppWarning() ? "true" : "false";

    // TLS listener is unique and parameters are modified through IP2IP_PROFILE
    std::stringstream tlslistenerport;
    tlslistenerport << getTlsListenerPort();
    a[TLS_LISTENER_PORT] = tlslistenerport.str();
    a[TLS_ENABLE] = getTlsEnable();
    a[TLS_CA_LIST_FILE] = getTlsCaListFile();
    a[TLS_CERTIFICATE_FILE] = getTlsCertificateFile();
    a[TLS_PRIVATE_KEY_FILE] = getTlsPrivateKeyFile();
    a[TLS_PASSWORD] = getTlsPassword();
    a[TLS_METHOD] = getTlsMethod();
    a[TLS_CIPHERS] = getTlsCiphers();
    a[TLS_SERVER_NAME] = getTlsServerName();
    a[TLS_VERIFY_SERVER] = getTlsVerifyServer() ? "true" : "false";
    a[TLS_VERIFY_CLIENT] = getTlsVerifyClient() ? "true" : "false";
    a[TLS_REQUIRE_CLIENT_CERTIFICATE] = getTlsRequireClientCertificate() ? "true" : "false";
    a[TLS_NEGOTIATION_TIMEOUT_SEC] = getTlsNegotiationTimeoutSec();
    a[TLS_NEGOTIATION_TIMEOUT_MSEC] = getTlsNegotiationTimeoutMsec();

    return a;
}


void SIPAccount::setVoIPLink()
{
    _link = SIPVoIPLink::instance ();
    dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();
}


void SIPAccount::initCredential (void)
{
    // We want to make sure that the password is really
    // 32 characters long. Otherwise, pjsip will fail
    // on an assertion.
    bool md5HashingEnabled = Manager::instance().preferences.getMd5Hash()
                             && _password.length() == 32;
    int dataType = md5HashingEnabled ? PJSIP_CRED_DATA_DIGEST 
                                     : PJSIP_CRED_DATA_PLAIN_PASSWD;
    std::string digest;

    // Create the credential array
    free_cred(_cred);
    _cred = (pjsip_cred_info *) calloc(getCredentialCount(), sizeof (pjsip_cred_info));

    if (!_cred) {
        _error ("SipAccount: Error: Failed to set _cred for account %s", _accountID.c_str());
        return;
    }

    if (md5HashingEnabled )
        _debug ("Setting digest ");

    std::string &authenticationUsername = _authenticationUsername.empty() ? _username : _authenticationUsername;
    // Use authentication username if provided
    _cred[0].username = pj_str (strdup (authenticationUsername.c_str()));

    // Set password
    _cred[0].data =  pj_str (strdup (_password.c_str()));

    // Set realm for that credential. * by default.
    _cred[0].realm = pj_str (strdup (_realm.c_str()));

    _cred[0].data_type = dataType;
    _cred[0].scheme = pj_str ( (char*) "digest");

#if 0 // FIXME, unused. see https://projects.savoirfairelinux.com/issues/6408
    unsigned i;

    // Default credential already initialized, use credentials.getCredentialCount()
    for (i = 0; i < credentials.getCredentialCount(); i++) {

        _cred[i].username = pj_str (strdup (_username.c_str()));
        _cred[i].data = pj_str (strdup (_password.c_str()));
        _cred[i].realm = pj_str (strdup (_realm.c_str()));

        _cred[i].data_type = dataType;

        _cred[i].scheme = pj_str ( (char*) "digest");

        _debug ("Setting credential %u realm = %s passwd = %s username = %s data_type = %d", i, _realm.c_str(), _password.c_str(), _username.c_str(), _cred[i].data_type);
    }
#endif
}


int SIPAccount::registerVoIPLink()
{
    if (_hostname.length() >= PJ_MAX_HOSTNAME) {
        return 1;
    }

    // Init set of additional credentials, if supplied by the user
    initCredential();

    // Init TLS settings if the user wants to use TLS
    if (_tlsEnable == "true") {
        _debug ("SIPAccount: TLS is enabled for account %s", getAccountID().c_str());
        _transportType = PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();
    }

    // Init STUN settings for this account if the user selected it
    if (_stunEnabled) {
        _transportType = PJSIP_TRANSPORT_START_OTHER;
        initStunConfiguration ();
    } else {
        _stunServerName = pj_str ( (char*) _stunServer.c_str());
    }

    try {
        // In our definition of the ip2ip profile (aka Direct IP Calls),
        // no registration should be performed
        if (_accountID != IP2IP_PROFILE) {
            _link->sendRegister (_accountID);
        }
    }
    catch(VoipLinkException &e) {
        _error("SIPAccount: %s", e.what());
    }

    return 0;
}

int SIPAccount::unregisterVoIPLink()
{
    if (_accountID == IP2IP_PROFILE) {
        return true;
    }

    try {
        _link->sendUnregister (_accountID);
        setRegistrationInfo (NULL);
    }
    catch(VoipLinkException &e) {
        _error("SIPAccount: %s", e.what());
        return false;
    }

    return true;
}

pjsip_ssl_method SIPAccount::sslMethodStringToPjEnum (const std::string& method)
{
    if (method == "Default")
        return PJSIP_SSL_UNSPECIFIED_METHOD;

    if (method == "TLSv1")
        return PJSIP_TLSV1_METHOD;

    if (method == "SSLv2")
        return PJSIP_SSLV2_METHOD;

    if (method == "SSLv3")
        return PJSIP_SSLV3_METHOD;

    if (method == "SSLv23")
        return PJSIP_SSLV23_METHOD;

    return PJSIP_SSL_UNSPECIFIED_METHOD;
}

void SIPAccount::initTlsConfiguration (void)
{
    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    setTlsListenerPort (atoi (_tlsPortStr.c_str()));

    delete _tlsSetting;
    _tlsSetting = new pjsip_tls_setting;

    assert (_tlsSetting);

    pjsip_tls_setting_default (_tlsSetting);

    pj_cstr (&_tlsSetting->ca_list_file, _tlsCaListFile.c_str());
    pj_cstr (&_tlsSetting->cert_file, _tlsCertificateFile.c_str());
    pj_cstr (&_tlsSetting->privkey_file, _tlsPrivateKeyFile.c_str());
    pj_cstr (&_tlsSetting->password, _tlsPassword.c_str());
    _tlsSetting->method = sslMethodStringToPjEnum (_tlsMethod);
    pj_cstr (&_tlsSetting->ciphers, _tlsCiphers.c_str());
    pj_cstr (&_tlsSetting->server_name, _tlsServerName.c_str());

    _tlsSetting->verify_server = _tlsVerifyServer ? PJ_TRUE: PJ_FALSE;
    _tlsSetting->verify_client = _tlsVerifyClient ? PJ_TRUE: PJ_FALSE;
    _tlsSetting->require_client_cert = _tlsRequireClientCertificate ? PJ_TRUE: PJ_FALSE;

    _tlsSetting->timeout.sec = atol (_tlsNegotiationTimeoutSec.c_str());
    _tlsSetting->timeout.msec = atol (_tlsNegotiationTimeoutMsec.c_str());
}

void SIPAccount::initStunConfiguration (void)
{
    size_t pos;
    std::string stunServer, serverName, serverPort;

    stunServer = _stunServer;
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
    if (_registrationExpire.empty())
        _registrationExpire = DFT_EXPIRE_VALUE;

    if (_tlsEnable == "true") {
        initTlsConfiguration();
        _transportType = PJSIP_TRANSPORT_TLS;
    } else {
        _transportType = PJSIP_TRANSPORT_UDP;
    }
}

bool SIPAccount::fullMatch (const std::string& username, const std::string& hostname) const
{
    return userMatch (username) && hostnameMatch (hostname);
}

bool SIPAccount::userMatch (const std::string& username) const
{
    return !username.empty() && username == getUsername();
}

bool SIPAccount::hostnameMatch (const std::string& hostname) const
{
    return hostname == getHostname();
}

std::string SIPAccount::getMachineName (void) const
{
    return std::string (pj_gethostname()->ptr, pj_gethostname()->slen);
}

std::string SIPAccount::getLoginName (void) const
{
    std::string username;

    struct passwd * user_info = getpwuid (getuid());
    if (user_info)
        username = user_info->pw_name;

    return username;
}

std::string SIPAccount::getTransportMapKey (void) const
{

    std::stringstream out;
    out << getLocalPort();
    std::string localPort = out.str();

    return localPort;
}


std::string SIPAccount::getFromUri (void) const
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

std::string SIPAccount::getToUri (const std::string& username) const
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

std::string SIPAccount::getServerUri (void) const
{
    char uri[PJSIP_MAX_URL_SIZE];

    std::string scheme;
    std::string transport;
    std::string hostname = _hostname;

    // UDP does not require the transport specification

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

std::string SIPAccount::getContactHeader (const std::string& address, const std::string& port) const
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
