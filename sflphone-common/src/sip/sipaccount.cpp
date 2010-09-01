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

Credentials::Credentials() : credentialCount (0) {}

Credentials::~Credentials() {}

void Credentials::setNewCredential (std::string username, std::string password, std::string realm)
{
    credentialArray[credentialCount].username = username;
    credentialArray[credentialCount].password = password;
    credentialArray[credentialCount].realm = realm;

}

CredentialItem *Credentials::getCredential (int index)
{
    if ( (index >= 0) && (index < credentialCount))
        return & (credentialArray[index]);
    else
        return NULL;
}

void Credentials::serialize (Conf::YamlEmitter *emitter UNUSED)
{

}

void Credentials::unserialize (Conf::MappingNode *map)
{

    Conf::ScalarNode *val = NULL;

    _debug ("SipAccount: Unserialize");

    val = (Conf::ScalarNode *) (map->getValue (credentialCountKey));

    if (val) {
        credentialCount = atoi (val->getValue().data());
        val = NULL;
    }
}



SIPAccount::SIPAccount (const AccountID& accountID)
        : Account (accountID, "SIP")
        , _routeSet ("")
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

    _debug ("Sip account constructor called");

    _stunServerName.ptr = NULL;
    _stunServerName.slen = 0;
    _stunPort = 0;

    // IP2IP settings must be loaded before singleton instanciation, cannot call it here...

    // _link = SIPVoIPLink::instance ("");

    /* Represents the number of SIP accounts connected the same link */
    // dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();

}

SIPAccount::~SIPAccount()
{
    /* One SIP account less connected to the sip voiplink */
    if (_accountID != "default")
        dynamic_cast<SIPVoIPLink*> (_link)->decrementClients();

    /* Delete accounts-related information */
    _regc = NULL;
    free (_cred);
    free (_tlsSetting);
}

void SIPAccount::serialize (Conf::YamlEmitter *emitter)
{

    _debug ("SipAccount: serialize %s", _accountID.c_str());


    Conf::MappingNode accountmap (NULL);
    Conf::MappingNode credentialmap (NULL);
    Conf::MappingNode srtpmap (NULL);
    Conf::MappingNode zrtpmap (NULL);
    Conf::MappingNode tlsmap (NULL);

    Conf::ScalarNode id (Account::_accountID);
    Conf::ScalarNode username (Account::_username);
    Conf::ScalarNode password (Account::_password);
    Conf::ScalarNode alias (Account::_alias);
    Conf::ScalarNode hostname (Account::_hostname);
    Conf::ScalarNode enable (_enabled ? "true" : "false");
    Conf::ScalarNode type (Account::_type);
    Conf::ScalarNode expire (_registrationExpire);
    Conf::ScalarNode interface (_interface);
    std::stringstream portstr;
    portstr << _localPort;
    Conf::ScalarNode port (portstr.str());
    Conf::ScalarNode serviceRoute (_serviceRoute);

    Conf::ScalarNode mailbox ("97");
    Conf::ScalarNode publishAddr (_publishedIpAddress);
    std::stringstream publicportstr;
    publicportstr << _publishedPort;
    Conf::ScalarNode publishPort (publicportstr.str());
    Conf::ScalarNode sameasLocal (_publishedSameasLocal ? "true" : "false");
    Conf::ScalarNode resolveOnce (_resolveOnce ? "true" : "false");
    Conf::ScalarNode codecs (_codecStr);
    Conf::ScalarNode ringtonePath (_ringtonePath);
    Conf::ScalarNode ringtoneEnabled (_ringtoneEnabled ? "true" : "false");
    Conf::ScalarNode stunServer (_stunServer);
    Conf::ScalarNode stunEnabled (_stunEnabled ? "true" : "false");
    Conf::ScalarNode displayName (_displayName);
    Conf::ScalarNode dtmfType (_dtmfType==0 ? "overrtp" : "sipinfo");

    std::stringstream countstr;
    countstr << 0;
    Conf::ScalarNode count (countstr.str());

    Conf::ScalarNode srtpenabled (_srtpEnabled ? "true" : "false");
    Conf::ScalarNode keyExchange (_srtpKeyExchange);
    Conf::ScalarNode rtpFallback (_srtpFallback ? "true" : "false");

    Conf::ScalarNode displaySas (_zrtpDisplaySas ? "true" : "false");
    Conf::ScalarNode displaySasOnce (_zrtpDisplaySasOnce ? "true" : "false");
    Conf::ScalarNode helloHashEnabled (_zrtpHelloHash ? "true" : "false");
    Conf::ScalarNode notSuppWarning (_zrtpNotSuppWarning ? "true" : "false");

    Conf::ScalarNode tlsport (_tlsPortStr);
    Conf::ScalarNode certificate (_tlsCertificateFile);
    Conf::ScalarNode calist (_tlsCaListFile);
    Conf::ScalarNode ciphers (_tlsCiphers);
    Conf::ScalarNode tlsenabled (_tlsEnable);
    Conf::ScalarNode tlsmethod (_tlsMethod);
    Conf::ScalarNode timeout (_tlsNegotiationTimeoutSec);
    Conf::ScalarNode tlspassword (_tlsPassword);
    Conf::ScalarNode privatekey (_tlsPrivateKeyFile);
    Conf::ScalarNode requirecertif (_tlsRequireClientCertificate ? "true" : "false");
    Conf::ScalarNode server (_tlsServerName);
    Conf::ScalarNode verifyclient (_tlsVerifyServer ? "true" : "false");
    Conf::ScalarNode verifyserver (_tlsVerifyClient ? "true" : "false");

    accountmap.setKeyValue (aliasKey, &alias);
    accountmap.setKeyValue (typeKey, &type);
    accountmap.setKeyValue (idKey, &id);
    accountmap.setKeyValue (usernameKey, &username);
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
    Conf::ScalarNode *val;
    Conf::MappingNode *srtpMap;
    Conf::MappingNode *tlsMap;
    Conf::MappingNode *zrtpMap;
    Conf::MappingNode *credMap;

    _debug ("SipAccount: Unserialize %s", _accountID.c_str());

    val = (Conf::ScalarNode *) (map->getValue (aliasKey));

    if (val) {
        _alias = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (typeKey));

    if (val) {
        _type = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (idKey));

    if (val) {
        _accountID = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (usernameKey));

    if (val) {
        _username = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (passwordKey));

    if (val) {
        _password = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (hostnameKey));

    if (val) {
        _hostname = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (accountEnableKey));

    if (val) {
        _enabled = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    //  val = (Conf::ScalarNode *)(map->getValue(mailboxKey));

    val = (Conf::ScalarNode *) (map->getValue (codecsKey));

    if (val) {
        _codecStr = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (ringtonePathKey));

    if (val) {
        _ringtonePath = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (ringtoneEnabledKey));

    if (val) {
        _ringtoneEnabled = (val->getValue() == "true") ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (expireKey));

    if (val) {
        _registrationExpire = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (interfaceKey));

    if (val) {
        _interface = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (portKey));

    if (val) {
        _localPort = atoi (val->getValue().data());
        val = NULL;
    }

    // val = (Conf::ScalarNode *)(map->getValue(mailboxKey));
    val = (Conf::ScalarNode *) (map->getValue (publishAddrKey));

    if (val) {
        _publishedIpAddress = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (publishPortKey));

    if (val) {
        _publishedPort = atoi (val->getValue().data());
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (sameasLocalKey));

    if (val) {
        _publishedSameasLocal = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (resolveOnceKey));

    if (val) {
        _resolveOnce = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (dtmfTypeKey));

    if (val) {
        val = NULL;
    }

    // _dtmfType = atoi(val->getValue();
    val = (Conf::ScalarNode *) (map->getValue (serviceRouteKey));

    if (val) {
        _serviceRoute = val->getValue();
    }

    // stun enabled
    val = (Conf::ScalarNode *) (map->getValue (stunEnabledKey));

    if (val) {
        _stunEnabled = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (map->getValue (stunServerKey));

    if (val) {
        _stunServer = val->getValue();
        val = NULL;
    }

    _stunServerName = pj_str ( (char*) _stunServer.data());

    credMap = (Conf::MappingNode *) (map->getValue (credKey));
    credentials.unserialize (credMap);

    val = (Conf::ScalarNode *) (map->getValue (displayNameKey));

    if (val) {
        _displayName = val->getValue();
        val = NULL;
    }

    // get srtp submap
    srtpMap = (Conf::MappingNode *) (map->getValue (srtpKey));

    if (!srtpMap)
        throw SipAccountException (" did not found srtp map");

    val = (Conf::ScalarNode *) (srtpMap->getValue (srtpEnableKey));

    if (val) {
        _srtpEnabled = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (srtpMap->getValue (keyExchangeKey));

    if (val) {
        _srtpKeyExchange = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (srtpMap->getValue (rtpFallbackKey));

    if (val) {
        _srtpFallback = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    // get zrtp submap
    zrtpMap = (Conf::MappingNode *) (map->getValue (zrtpKey));

    if (!zrtpMap)
        throw SipAccountException (" did not found zrtp map");

    val = (Conf::ScalarNode *) (zrtpMap->getValue (displaySasKey));

    if (val) {
        _zrtpDisplaySas = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (zrtpMap->getValue (displaySasOnceKey));

    if (val) {
        _zrtpDisplaySasOnce = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (zrtpMap->getValue (helloHashEnabledKey));

    if (val) {
        _zrtpHelloHash = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (zrtpMap->getValue (notSuppWarningKey));

    if (val) {
        _zrtpNotSuppWarning = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    // get tls submap
    tlsMap = (Conf::MappingNode *) (map->getValue (tlsKey));

    if (!tlsMap)
        throw SipAccountException (" did not found tls map");

    val = (Conf::ScalarNode *) (tlsMap->getValue (tlsEnableKey));

    if (val) {
        _tlsEnable = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (tlsPortKey));

    if (val) {
        _tlsPortStr = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (certificateKey));

    if (val) {
        _tlsCertificateFile = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (calistKey));

    if (val) {
        _tlsCaListFile = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (ciphersKey));

    if (val) {
        _tlsCiphers = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (methodKey));

    if (val) {
        _tlsMethod = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (timeoutKey));

    if (val) _tlsNegotiationTimeoutSec = val->getValue();

    if (val) {
        _tlsNegotiationTimeoutMsec = val->getValue();
        val=NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (tlsPasswordKey));

    if (val) {
        _tlsPassword = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (privateKeyKey));

    if (val) {
        _tlsPrivateKeyFile = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (requireCertifKey));

    if (val) {
        _tlsRequireClientCertificate = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (serverKey));

    if (val) {
        _tlsServerName = val->getValue();
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (verifyClientKey));

    if (val) {
        _tlsVerifyServer = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

    val = (Conf::ScalarNode *) (tlsMap->getValue (verifyServerKey));

    if (val) {
        _tlsVerifyClient = (val->getValue().compare ("true") == 0) ? true : false;
        val = NULL;
    }

}


void SIPAccount::setAccountDetails (const std::map<std::string, std::string>& details)
{

    std::map<std::string, std::string> map_cpy;
    std::map<std::string, std::string>::iterator iter;

    _debug ("SipAccount: set account details %s", _accountID.c_str());

    // Work on a copy
    map_cpy = details;

    std::string alias;
    std::string type;
    std::string hostname;
    std::string username;
    std::string password;
    std::string mailbox;
    std::string accountEnable;
    std::string ringtonePath;
    std::string ringtoneEnabled;

    // Account setting common to SIP and IAX
    find_in_map (CONFIG_ACCOUNT_ALIAS, alias)
    find_in_map (CONFIG_ACCOUNT_TYPE, type)
    find_in_map (HOSTNAME, hostname)
    find_in_map (USERNAME, username)
    find_in_map (PASSWORD, password)
    find_in_map (CONFIG_ACCOUNT_MAILBOX, mailbox);
    find_in_map (CONFIG_ACCOUNT_ENABLE, accountEnable);
    find_in_map (CONFIG_RINGTONE_PATH, ringtonePath);
    find_in_map (CONFIG_RINGTONE_ENABLED, ringtoneEnabled);

    setAlias (alias);
    setType (type);
    setUsername (username);
    setHostname (hostname);
    setPassword (password);
    setEnabled ( (accountEnable == "true"));
    setRingtonePath (ringtonePath);
    setRingtoneEnabled ( (ringtoneEnabled == "true"));

    // SIP specific account settings
    if (getType() == "SIP") {

        std::string ua_name;
        std::string realm;
        std::string routeset;
        std::string authenticationName;

        std::string resolveOnce;
        std::string registrationExpire;

        std::string displayName;
        std::string localInterface;
        std::string publishedSameasLocal;
        std::string localAddress;
        std::string publishedAddress;
        std::string localPort;
        std::string publishedPort;
        std::string stunEnable;
        std::string stunServer;
        std::string dtmfType;
        std::string srtpEnable;
        std::string srtpRtpFallback;
        std::string zrtpDisplaySas;
        std::string zrtpDisplaySasOnce;
        std::string zrtpNotSuppWarning;
        std::string zrtpHelloHash;
        std::string srtpKeyExchange;

        std::string tlsListenerPort;
        std::string tlsEnable;
        std::string tlsCaListFile;
        std::string tlsCertificateFile;
        std::string tlsPrivateKeyFile;
        std::string tlsPassword;
        std::string tlsMethod;
        std::string tlsCiphers;
        std::string tlsServerName;
        std::string tlsVerifyServer;
        std::string tlsVerifyClient;
        std::string tlsRequireClientCertificate;
        std::string tlsNegotiationTimeoutSec;
        std::string tlsNegotiationTimeoutMsec;

        // general sip settings
        find_in_map (DISPLAY_NAME, displayName)
        find_in_map (ROUTESET, routeset)
        find_in_map (LOCAL_INTERFACE, localInterface)
        find_in_map (PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal)
        find_in_map (PUBLISHED_ADDRESS, publishedAddress)
        find_in_map (LOCAL_PORT, localPort)
        find_in_map (PUBLISHED_PORT, publishedPort)
        find_in_map (STUN_ENABLE, stunEnable)
        find_in_map (STUN_SERVER, stunServer)
        find_in_map (ACCOUNT_DTMF_TYPE, dtmfType)
        find_in_map (CONFIG_ACCOUNT_RESOLVE_ONCE, resolveOnce)
        find_in_map (CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire)

        setDisplayName (displayName);
        setServiceRoute (routeset);
        setLocalInterface (localInterface);
        setPublishedSameasLocal ( (publishedSameasLocal.compare ("true") == 0) ? true : false);
        setPublishedAddress (publishedAddress);
        setLocalPort (atoi (localPort.data()));
        setPublishedPort (atoi (publishedPort.data()));
        setStunServer (stunServer);
        setStunEnabled ( (stunEnable == "true"));
        setResolveOnce ( (resolveOnce.compare ("true") ==0) ? true : false);
        setRegistrationExpire (registrationExpire);

        // sip credential
        find_in_map (REALM, realm)
        find_in_map (AUTHENTICATION_USERNAME, authenticationName)
        find_in_map (USERAGENT, ua_name)

        setUseragent (ua_name);

        // srtp settings
        find_in_map (SRTP_ENABLE, srtpEnable)
        find_in_map (SRTP_RTP_FALLBACK, srtpRtpFallback)
        find_in_map (ZRTP_DISPLAY_SAS, zrtpDisplaySas)
        find_in_map (ZRTP_DISPLAY_SAS_ONCE, zrtpDisplaySasOnce)
        find_in_map (ZRTP_NOT_SUPP_WARNING, zrtpNotSuppWarning)
        find_in_map (ZRTP_HELLO_HASH, zrtpHelloHash)
        find_in_map (SRTP_KEY_EXCHANGE, srtpKeyExchange)

        setSrtpEnable ( (srtpEnable.compare ("true") == 0) ? true : false);
        setSrtpFallback ( (srtpRtpFallback.compare ("true") == 0) ? true : false);
        setZrtpDisplaySas ( (zrtpDisplaySas.compare ("true") == 0) ? true : false);
        setZrtpDiaplaySasOnce ( (zrtpDisplaySasOnce.compare ("true") == 0) ? true : false);
        setZrtpNotSuppWarning ( (zrtpNotSuppWarning.compare ("true") == 0) ? true : false);
        setZrtpHelloHash ( (zrtpHelloHash.compare ("true") == 0) ? true : false);
        // sipaccount->setSrtpKeyExchange((srtpKeyExchange.compare("true") == 0) ? true : false);
        setSrtpKeyExchange (srtpKeyExchange);

        // TLS settings
        // The TLS listener is unique and globally defined through IP2IP_PROFILE
        if (_accountID == IP2IP_PROFILE) {
            find_in_map (TLS_LISTENER_PORT, tlsListenerPort)
        }

        find_in_map (TLS_ENABLE, tlsEnable)
        find_in_map (TLS_CA_LIST_FILE, tlsCaListFile)
        find_in_map (TLS_CERTIFICATE_FILE, tlsCertificateFile)
        find_in_map (TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile)
        find_in_map (TLS_PASSWORD, tlsPassword)
        find_in_map (TLS_METHOD, tlsMethod)
        find_in_map (TLS_CIPHERS, tlsCiphers)
        find_in_map (TLS_SERVER_NAME, tlsServerName)
        find_in_map (TLS_VERIFY_SERVER, tlsVerifyServer)
        find_in_map (TLS_VERIFY_CLIENT, tlsVerifyClient)
        find_in_map (TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate)
        find_in_map (TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec)
        find_in_map (TLS_NEGOTIATION_TIMEOUT_MSEC, tlsNegotiationTimeoutMsec)

        if (_accountID == IP2IP_PROFILE) {
            setTlsListenerPort (atoi (tlsListenerPort.data()));
        }

        setTlsEnable (tlsEnable);
        setTlsCaListFile (tlsCaListFile);
        setTlsCertificateFile (tlsCertificateFile);
        setTlsPrivateKeyFile (tlsPrivateKeyFile);
        setTlsPassword (tlsPassword);
        setTlsMethod (tlsMethod);
        setTlsCiphers (tlsCiphers);
        setTlsServerName (tlsServerName);
        setTlsVerifyServer (tlsVerifyServer.compare ("true") ? true : false);
        setTlsVerifyClient (tlsVerifyServer.compare ("true") ? true : false);
        setTlsRequireClientCertificate (tlsRequireClientCertificate.compare ("true") ? true : false);
        setTlsNegotiationTimeoutSec (tlsNegotiationTimeoutSec);
        setTlsNegotiationTimeoutMsec (tlsNegotiationTimeoutMsec);

        if (!Manager::instance().preferences.getMd5Hash()) {
            setPassword (password);
        } else {
            // Make sure not to re-hash the password field if
            // it is already saved as a MD5 Hash.
            // TODO: This test is weak. Fix this.
            if ( (password.compare (getPassword()) != 0)) {
                _debug ("SipAccount: Password sent and password from config are different. Re-hashing");
                std::string hash;

                if (authenticationName.empty()) {
                    hash = Manager::instance().computeMd5HashFromCredential (username, password, realm);
                } else {
                    hash = Manager::instance().computeMd5HashFromCredential (authenticationName, password, realm);
                }

                setPassword (hash);
            }
        }
    }
}

std::map<std::string, std::string> SIPAccount::getAccountDetails()
{
    _debug ("SipAccount: get account details %s", _accountID.c_str());

    std::map<std::string, std::string> a;

    a.insert (std::pair<std::string, std::string> (ACCOUNT_ID, _accountID));
    // The IP profile does not allow to set an alias
    (_accountID == IP2IP_PROFILE) ?
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, IP2IP_PROFILE)) :
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, getAlias()));

    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ENABLE, isEnabled() ? "true" : "false"));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_TYPE, getType()));
    a.insert (std::pair<std::string, std::string> (HOSTNAME, getHostname()));
    a.insert (std::pair<std::string, std::string> (USERNAME, getUsername()));
    a.insert (std::pair<std::string, std::string> (PASSWORD, getPassword()));

    a.insert (std::pair<std::string, std::string> (CONFIG_RINGTONE_PATH, getRingtonePath()));
    a.insert (std::pair<std::string, std::string> (CONFIG_RINGTONE_ENABLED, getRingtoneEnabled() ? "true" : "false"));

    RegistrationState state = Unregistered;
    std::string registrationStateCode;
    std::string registrationStateDescription;


    if (_accountID == IP2IP_PROFILE) {
        registrationStateCode = EMPTY_FIELD;
        registrationStateDescription = "Direct IP call";
    } else {
        state = getRegistrationState();
        int code = getRegistrationStateDetailed().first;
        std::stringstream out;
        out << code;
        registrationStateCode = out.str();
        registrationStateDescription = getRegistrationStateDetailed().second;
    }


    (_accountID == IP2IP_PROFILE) ?
    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATUS, "READY")) :
    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATUS, Manager::instance().mapStateNumberToString (state)));

    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATE_CODE, registrationStateCode));
    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATE_DESCRIPTION, registrationStateDescription));


    // Add sip specific details
    if (getType() == "SIP") {

        a.insert (std::pair<std::string, std::string> (ROUTESET, getServiceRoute()));
        a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_RESOLVE_ONCE, isResolveOnce() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (REALM, _realm));
        a.insert (std::pair<std::string, std::string> (USERAGENT, getUseragent()));

        a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_REGISTRATION_EXPIRE, getRegistrationExpire()));
        a.insert (std::pair<std::string, std::string> (LOCAL_INTERFACE, getLocalInterface()));
        a.insert (std::pair<std::string, std::string> (PUBLISHED_SAMEAS_LOCAL, getPublishedSameasLocal() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (PUBLISHED_ADDRESS, getPublishedAddress()));

        std::stringstream localport;
        localport << getLocalPort();
        a.insert (std::pair<std::string, std::string> (LOCAL_PORT, localport.str()));
        std::stringstream publishedport;
        publishedport << getPublishedPort();
        a.insert (std::pair<std::string, std::string> (PUBLISHED_PORT, publishedport.str()));
        a.insert (std::pair<std::string, std::string> (STUN_ENABLE, isStunEnabled() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (STUN_SERVER, getStunServer()));
        a.insert (std::pair<std::string, std::string> (ACCOUNT_DTMF_TYPE, (getDtmfType() == 0) ? "0" : "1"));

        a.insert (std::pair<std::string, std::string> (SRTP_KEY_EXCHANGE, getSrtpKeyExchange()));
        a.insert (std::pair<std::string, std::string> (SRTP_ENABLE, getSrtpEnable() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (SRTP_RTP_FALLBACK, getSrtpFallback() ? "true" : "false"));

        a.insert (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS, getZrtpDisplaySas() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS_ONCE, getZrtpDiaplaySasOnce() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (ZRTP_HELLO_HASH, getZrtpHelloHash() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (ZRTP_NOT_SUPP_WARNING, getZrtpNotSuppWarning() ? "true" : "false"));

        // TLS listener is unique and parameters are modified through IP2IP_PROFILE
        std::stringstream tlslistenerport;
        tlslistenerport << getTlsListenerPort();
        a.insert (std::pair<std::string, std::string> (TLS_LISTENER_PORT, tlslistenerport.str()));
        a.insert (std::pair<std::string, std::string> (TLS_ENABLE, getTlsEnable()));
        a.insert (std::pair<std::string, std::string> (TLS_CA_LIST_FILE, getTlsCaListFile()));
        a.insert (std::pair<std::string, std::string> (TLS_CERTIFICATE_FILE, getTlsCertificateFile()));
        a.insert (std::pair<std::string, std::string> (TLS_PRIVATE_KEY_FILE, getTlsPrivateKeyFile()));
        a.insert (std::pair<std::string, std::string> (TLS_PASSWORD, getTlsPassword()));
        a.insert (std::pair<std::string, std::string> (TLS_METHOD, getTlsMethod()));
        a.insert (std::pair<std::string, std::string> (TLS_CIPHERS, getTlsCiphers()));
        a.insert (std::pair<std::string, std::string> (TLS_SERVER_NAME, getTlsServerName()));
        a.insert (std::pair<std::string, std::string> (TLS_VERIFY_SERVER, getTlsVerifyServer() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (TLS_VERIFY_CLIENT, getTlsVerifyClient() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (TLS_REQUIRE_CLIENT_CERTIFICATE, getTlsRequireClientCertificate() ? "true" : "false"));
        a.insert (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_SEC, getTlsNegotiationTimeoutSec()));
        a.insert (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_MSEC, getTlsNegotiationTimeoutMsec()));

    }

    return a;

}


// void SIPAccount::setVoIPLink(VoIPLink *link) {
void SIPAccount::setVoIPLink()
{

    _link = SIPVoIPLink::instance ("");
    dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();

}


int SIPAccount::initCredential (void)
{
    _debug ("SipAccount: Init credential");

    bool md5HashingEnabled = false;
    int dataType = 0;
    md5HashingEnabled = Manager::instance().preferences.getMd5Hash();
    std::string digest;

    // Create the credential array
    pjsip_cred_info * cred_info = (pjsip_cred_info *) malloc (sizeof (pjsip_cred_info) * (getCredentialCount()));

    if (cred_info == NULL) {
        _error ("SipAccount: Error: Failed to set cred_info for account %s", _accountID.c_str());
        return !SUCCESS;
    }

    pj_bzero (cred_info, sizeof (pjsip_cred_info) * getCredentialCount());

    // Use authentication username if provided
    if (!_authenticationUsername.empty())
        cred_info[0].username = pj_str (strdup (_authenticationUsername.c_str()));
    else
        cred_info[0].username = pj_str (strdup (_username.c_str()));

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

    // Default credential already initialized, use credentials.getCredentialCount()
    for (i = 0; i < credentials.getCredentialCount(); i++) {

        std::string username = _username;
        std::string password = _password;
        std::string realm = _realm;

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
    if (_tlsEnable == "true") {
        _debug ("Account: TLS is ennabled for accounr %s", getAccountID().c_str());
        _transportType = PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();
    }

    // Init STUN settings for this account if the user selected it

    if (_stunEnabled) {
        _transportType = PJSIP_TRANSPORT_START_OTHER;
        initStunConfiguration ();
    } else {
        _stunServerName = pj_str ( (char*) _stunServer.data());
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
    _debug ("SipAccount: Init TLS configuration");

    /*
     * Initialize structure to zero
     */
    if (_tlsSetting) {
        free (_tlsSetting);
        _tlsSetting = NULL;
    }

    // TLS listener is unique and should be only modified through IP2IP_PROFILE

    // setTlsListenerPort(atoi(tlsPortStr.c_str()));
    setTlsListenerPort (atoi (_tlsPortStr.c_str()));

    _tlsSetting = (pjsip_tls_setting *) malloc (sizeof (pjsip_tls_setting));

    assert (_tlsSetting);

    pjsip_tls_setting_default (_tlsSetting);

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

std::string SIPAccount::getTransportMapKey (void)
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

