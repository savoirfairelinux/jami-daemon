/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

SIPAccount::SIPAccount (const std::string& accountID)
    : Account (accountID, "SIP")
    , routeSet_ ("")
    , pool_ (NULL)
    , regc_ (NULL)
    , bRegister_ (false)
    , registrationExpire_ ("")
    , interface_ ("default")
    , publishedSameasLocal_ (true)
    , publishedIpAddress_ ("")
    , localPort_ (DEFAULT_SIP_PORT)
    , publishedPort_ (DEFAULT_SIP_PORT)
    , serviceRoute_ ("")
    , tlsListenerPort_ (DEFAULT_SIP_TLS_PORT)
    , transportType_ (PJSIP_TRANSPORT_UNSPECIFIED)
    , transport_ (NULL)
    , resolveOnce_ (false)
    , cred_ (NULL)
    , tlsSetting_ (NULL)
    , dtmfType_ (OVERRTP)
    , tlsEnable_ ("false")
	, tlsPort_ (DEFAULT_SIP_TLS_PORT)
    , tlsCaListFile_ ("")
    , tlsCertificateFile_ ("")
    , tlsPrivateKeyFile_ ("")
    , tlsPassword_ ("")
    , tlsMethod_ ("TLSv1")
    , tlsCiphers_ ("")
    , tlsServerName_ ("")
    , tlsVerifyServer_ (true)
    , tlsVerifyClient_ (true)
    , tlsRequireClientCertificate_ (true)
    , tlsNegotiationTimeoutSec_ ("2")
    , tlsNegotiationTimeoutMsec_ ("0")
    , stunServer_ (DFT_STUN_SERVER)
    , stunEnabled_ (false)
    , srtpEnabled_ (false)
    , srtpKeyExchange_ ("sdes")
    , srtpFallback_ (false)
    , zrtpDisplaySas_ (true)
    , zrtpDisplaySasOnce_ (false)
    , zrtpHelloHash_ (true)
    , zrtpNotSuppWarning_ (true)
{
    stunServerName_.ptr = NULL;
    stunServerName_.slen = 0;
    stunPort_ = 0;
}

SIPAccount::~SIPAccount()
{
    /* One SIP account less connected to the sip voiplink */
    if (accountID_ != "default")
        dynamic_cast<SIPVoIPLink*> (link_)->decrementClients();

    /* Delete accounts-related information */
    regc_ = NULL;
    delete[] cred_;
    delete tlsSetting_;
}

void SIPAccount::serialize (Conf::YamlEmitter *emitter)
{
	if(emitter == NULL) {
		_error("SIPAccount: Error: emitter is NULL in serialize");
		return;
	}

    Conf::MappingNode accountmap (NULL);
    Conf::MappingNode srtpmap (NULL);
    Conf::MappingNode zrtpmap (NULL);
    Conf::MappingNode tlsmap (NULL);

    Conf::ScalarNode id (Account::accountID_);
    Conf::ScalarNode username (Account::username_);
    Conf::ScalarNode alias (Account::alias_);
    Conf::ScalarNode hostname (Account::hostname_);
    Conf::ScalarNode enable (enabled_);
    Conf::ScalarNode type (Account::type_);
    Conf::ScalarNode expire (registrationExpire_);
    Conf::ScalarNode interface (interface_);
    std::stringstream portstr;
    portstr << localPort_;
    Conf::ScalarNode port (portstr.str());
    Conf::ScalarNode serviceRoute (serviceRoute_);

    Conf::ScalarNode mailbox (mailBox_);
    Conf::ScalarNode publishAddr (publishedIpAddress_);
    std::stringstream publicportstr;
    publicportstr << publishedPort_;
    Conf::ScalarNode publishPort (publicportstr.str());
    Conf::ScalarNode sameasLocal (publishedSameasLocal_);
    Conf::ScalarNode resolveOnce (resolveOnce_);
    Conf::ScalarNode codecs (codecStr_);
    Conf::ScalarNode ringtonePath (ringtonePath_);
    Conf::ScalarNode ringtoneEnabled (ringtoneEnabled_);
    Conf::ScalarNode stunServer (stunServer_);
    Conf::ScalarNode stunEnabled (stunEnabled_);
    Conf::ScalarNode displayName (displayName_);
    Conf::ScalarNode dtmfType (dtmfType_==OVERRTP ? "overrtp" : "sipinfo");

    std::stringstream countstr;
    countstr << 0;
    Conf::ScalarNode count (countstr.str());

    Conf::ScalarNode srtpenabled (srtpEnabled_);
    Conf::ScalarNode keyExchange (srtpKeyExchange_);
    Conf::ScalarNode rtpFallback (srtpFallback_);

    Conf::ScalarNode displaySas (zrtpDisplaySas_);
    Conf::ScalarNode displaySasOnce (zrtpDisplaySasOnce_);
    Conf::ScalarNode helloHashEnabled (zrtpHelloHash_);
    Conf::ScalarNode notSuppWarning (zrtpNotSuppWarning_);

    portstr.str("");
    portstr << tlsPort_;
    Conf::ScalarNode tlsport (portstr.str());
    Conf::ScalarNode certificate (tlsCertificateFile_);
    Conf::ScalarNode calist (tlsCaListFile_);
    Conf::ScalarNode ciphers (tlsCiphers_);
    Conf::ScalarNode tlsenabled (tlsEnable_);
    Conf::ScalarNode tlsmethod (tlsMethod_);
    Conf::ScalarNode timeout (tlsNegotiationTimeoutSec_);
    Conf::ScalarNode tlspassword (tlsPassword_);
    Conf::ScalarNode privatekey (tlsPrivateKeyFile_);
    Conf::ScalarNode requirecertif (tlsRequireClientCertificate_);
    Conf::ScalarNode server (tlsServerName_);
    Conf::ScalarNode verifyclient (tlsVerifyServer_);
    Conf::ScalarNode verifyserver (tlsVerifyClient_);

    accountmap.setKeyValue (aliasKey, &alias);
    accountmap.setKeyValue (typeKey, &type);
    accountmap.setKeyValue (idKey, &id);
    accountmap.setKeyValue (usernameKey, &username);
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

    Conf::SequenceNode credentialseq (NULL);
    accountmap.setKeyValue (credKey, &credentialseq);

	std::vector<std::map<std::string, std::string> >::const_iterator it;
	for (it = credentials_.begin(); it != credentials_.end(); ++it) {
		std::map<std::string, std::string> cred = *it;
		Conf::MappingNode *map = new Conf::MappingNode(NULL);
		map->setKeyValue(USERNAME, new Conf::ScalarNode(cred[USERNAME]));
		map->setKeyValue(PASSWORD, new Conf::ScalarNode(cred[PASSWORD]));
		map->setKeyValue(REALM, new Conf::ScalarNode(cred[REALM]));
		credentialseq.addNode(map);
	}

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
    } catch (const Conf::YamlEmitterException &e) {
        _error ("ConfigTree: %s", e.what());
    }

    Conf::Sequence *seq = credentialseq.getSequence();
    Conf::Sequence::iterator seqit;
    for (seqit = seq->begin(); seqit != seq->end(); ++seqit) {
    	Conf::MappingNode *node = (Conf::MappingNode*)*seqit;
    	delete node->getValue(USERNAME);
		delete node->getValue(PASSWORD);
		delete node->getValue(REALM);
    	delete node;
    }
}



void SIPAccount::unserialize (Conf::MappingNode *map)
{
    Conf::MappingNode *srtpMap;
    Conf::MappingNode *tlsMap;
    Conf::MappingNode *zrtpMap;

    assert(map);

    map->getValue(aliasKey, &alias_);
    map->getValue(typeKey, &type_);
    map->getValue(usernameKey, &username_);
    map->getValue(hostnameKey, &hostname_);
    map->getValue(accountEnableKey, &enabled_);
    map->getValue(mailboxKey, &mailBox_);
    map->getValue(codecsKey, &codecStr_);
    // Update codec list which one is used for SDP offer
    setActiveCodecs (ManagerImpl::unserialize (codecStr_));

    map->getValue(ringtonePathKey, &ringtonePath_);
    map->getValue(ringtoneEnabledKey, &ringtoneEnabled_);
    map->getValue(expireKey, &registrationExpire_);
    map->getValue(interfaceKey, &interface_);
    int port;
    map->getValue(portKey, &port);
    localPort_ = port;
    map->getValue(publishAddrKey, &publishedIpAddress_);
    map->getValue(publishPortKey, &port);
    publishedPort_ = port;
    map->getValue(sameasLocalKey, &publishedSameasLocal_);
    map->getValue(resolveOnceKey, &resolveOnce_);

    std::string dtmfType;
    map->getValue(dtmfTypeKey, &dtmfType);
    dtmfType_ = (dtmfType == "overrtp") ? OVERRTP : SIPINFO;

    map->getValue(serviceRouteKey, &serviceRoute_);
    // stun enabled
    map->getValue(stunEnabledKey, &stunEnabled_);
    map->getValue(stunServerKey, &stunServer_);

    // Init stun server name with default server name
    stunServerName_ = pj_str ( (char*) stunServer_.data());

    map->getValue(displayNameKey, &displayName_);

	std::vector<std::map<std::string, std::string> > creds;

	Conf::YamlNode *credNode = map->getValue (credKey);

	/* We check if the credential key is a sequence
	 * because it was a mapping in a previous version of
	 * the configuration file.
	 */
	if (credNode && credNode->getType() == Conf::SEQUENCE) {
	    Conf::SequenceNode *credSeq = (Conf::SequenceNode *) credNode;
		Conf::Sequence::iterator it;
		Conf::Sequence *seq = credSeq->getSequence();
		for(it = seq->begin(); it != seq->end(); ++it) {
			Conf::MappingNode *cred = (Conf::MappingNode *) (*it);
			std::string user;
			std::string pass;
			std::string realm;
			cred->getValue(USERNAME, &user);
			cred->getValue(PASSWORD, &pass);
			cred->getValue(REALM, &realm);
			std::map<std::string, std::string> map;
			map[USERNAME] = user;
			map[PASSWORD] = pass;
			map[REALM] = realm;
			creds.push_back(map);
		}
	}
    if (creds.empty()) {
    	// migration from old file format
		std::map<std::string, std::string> credmap;
		std::string password;
	    map->getValue(passwordKey, &password);

		credmap[USERNAME] = username_;
		credmap[PASSWORD] = password;
		credmap[REALM] = "*";
		creds.push_back(credmap);
    }
    setCredentials (creds);

    // get srtp submap
    srtpMap = (Conf::MappingNode *) (map->getValue (srtpKey));
    if (srtpMap) {
        srtpMap->getValue(srtpEnableKey, &srtpEnabled_);
        srtpMap->getValue(keyExchangeKey, &srtpKeyExchange_);
        srtpMap->getValue(rtpFallbackKey, &srtpFallback_);
    }

    // get zrtp submap
    zrtpMap = (Conf::MappingNode *) (map->getValue (zrtpKey));
    if (zrtpMap) {
        zrtpMap->getValue(displaySasKey, &zrtpDisplaySas_);
        zrtpMap->getValue(displaySasOnceKey, &zrtpDisplaySasOnce_);
        zrtpMap->getValue(helloHashEnabledKey, &zrtpHelloHash_);
        zrtpMap->getValue(notSuppWarningKey, &zrtpNotSuppWarning_);
    }

    // get tls submap
    tlsMap = (Conf::MappingNode *) (map->getValue (tlsKey));
    if (tlsMap) {
        tlsMap->getValue(tlsEnableKey, &tlsEnable_);
        tlsMap->getValue(tlsPortKey, &tlsPort_);
        tlsMap->getValue(certificateKey, &tlsCertificateFile_);
        tlsMap->getValue(calistKey, &tlsCaListFile_);
        tlsMap->getValue(ciphersKey, &tlsCiphers_);
        tlsMap->getValue(methodKey, &tlsMethod_);
        tlsMap->getValue(tlsPasswordKey, &tlsPassword_);
        tlsMap->getValue(privateKeyKey, &tlsPrivateKeyFile_);
        tlsMap->getValue(requireCertifKey, &tlsRequireClientCertificate_);
        tlsMap->getValue(serverKey, &tlsServerName_);
        tlsMap->getValue(verifyClientKey, &tlsVerifyServer_);
        tlsMap->getValue(verifyServerKey, &tlsVerifyClient_);
        // FIXME
        tlsMap->getValue(timeoutKey, &tlsNegotiationTimeoutSec_);
        tlsMap->getValue(timeoutKey, &tlsNegotiationTimeoutMsec_);
    }
}


void SIPAccount::setAccountDetails (std::map<std::string, std::string> details)
{
    // Account setting common to SIP and IAX
    setAlias (details[CONFIG_ACCOUNT_ALIAS]);
    setType (details[CONFIG_ACCOUNT_TYPE]);
    setUsername (details[USERNAME]);
    setHostname (details[HOSTNAME]);
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
    if (accountID_ == IP2IP_PROFILE)
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

    if (getCredentialCount() == 0) { // credentials not set, construct 1 entry
        std::vector<std::map<std::string, std::string> > v;
        std::map<std::string, std::string> map;
        map[USERNAME] = getUsername();
        map[PASSWORD] = details[PASSWORD];
        map[REALM]    = "*";
        v.push_back(map);
        setCredentials(v);
    }
}

std::map<std::string, std::string> SIPAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[ACCOUNT_ID] = accountID_;
    // The IP profile does not allow to set an alias
    a[CONFIG_ACCOUNT_ALIAS] = (accountID_ == IP2IP_PROFILE) ? IP2IP_PROFILE : alias_;

    a[CONFIG_ACCOUNT_ENABLE] = enabled_ ? "true" : "false";
    a[CONFIG_ACCOUNT_TYPE] = type_;
    a[HOSTNAME] = hostname_;
    a[USERNAME] = username_;

    a[CONFIG_RINGTONE_PATH] = ringtonePath_;
    a[CONFIG_RINGTONE_ENABLED] = ringtoneEnabled_ ? "true" : "false";
    a[CONFIG_ACCOUNT_MAILBOX] = mailBox_;

    RegistrationState state = Unregistered;
    std::string registrationStateCode;
    std::string registrationStateDescription;

    if (accountID_ == IP2IP_PROFILE)
        registrationStateDescription = "Direct IP call";
    else {
        state = registrationState_;
        int code = registrationStateDetailed_.first;
        std::stringstream out;
        out << code;
        registrationStateCode = out.str();
        registrationStateDescription = registrationStateDetailed_.second;
    }

    a[REGISTRATION_STATUS] = (accountID_ == IP2IP_PROFILE) ? "READY": Manager::instance().mapStateNumberToString (state);
    a[REGISTRATION_STATE_CODE] = registrationStateCode;
    a[REGISTRATION_STATE_DESCRIPTION] = registrationStateDescription;

    // Add sip specific details
    a[ROUTESET] = getServiceRoute();
    a[CONFIG_ACCOUNT_RESOLVE_ONCE] = isResolveOnce() ? "true" : "false";
    a[USERAGENT] = getUserAgent();

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
    link_ = SIPVoIPLink::instance ();
    dynamic_cast<SIPVoIPLink*> (link_)->incrementClients();
}


int SIPAccount::registerVoIPLink()
{
    if (hostname_.length() >= PJ_MAX_HOSTNAME) {
        return 1;
    }

    // Init TLS settings if the user wants to use TLS
    if (tlsEnable_ == "true") {
        _debug ("SIPAccount: TLS is enabled for account %s", getAccountID().c_str());
        transportType_ = PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();
    }

    // Init STUN settings for this account if the user selected it
    if (stunEnabled_) {
        transportType_ = PJSIP_TRANSPORT_START_OTHER;
        initStunConfiguration ();
    } else {
        stunServerName_ = pj_str ( (char*) stunServer_.c_str());
    }

    try {
        // In our definition of the ip2ip profile (aka Direct IP Calls),
        // no registration should be performed
        if (accountID_ != IP2IP_PROFILE)
            link_->sendRegister (this);
    }
    catch (const VoipLinkException &e) {
        _error("SIPAccount: %s", e.what());
    }

    return 0;
}

int SIPAccount::unregisterVoIPLink()
{
    if (accountID_ == IP2IP_PROFILE) {
        return true;
    }

    try {
        link_->sendUnregister (this);
        setRegistrationInfo (NULL);
    }
    catch (const VoipLinkException &e) {
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
    setTlsListenerPort(tlsPort_);

    delete tlsSetting_;
    tlsSetting_ = new pjsip_tls_setting;

    assert (tlsSetting_);

    pjsip_tls_setting_default (tlsSetting_);

    pj_cstr (&tlsSetting_->ca_list_file, tlsCaListFile_.c_str());
    pj_cstr (&tlsSetting_->cert_file, tlsCertificateFile_.c_str());
    pj_cstr (&tlsSetting_->privkey_file, tlsPrivateKeyFile_.c_str());
    pj_cstr (&tlsSetting_->password, tlsPassword_.c_str());
    tlsSetting_->method = sslMethodStringToPjEnum (tlsMethod_);
    pj_cstr (&tlsSetting_->ciphers, tlsCiphers_.c_str());
    pj_cstr (&tlsSetting_->server_name, tlsServerName_.c_str());

    tlsSetting_->verify_server = tlsVerifyServer_ ? PJ_TRUE: PJ_FALSE;
    tlsSetting_->verify_client = tlsVerifyClient_ ? PJ_TRUE: PJ_FALSE;
    tlsSetting_->require_client_cert = tlsRequireClientCertificate_ ? PJ_TRUE: PJ_FALSE;

    tlsSetting_->timeout.sec = atol (tlsNegotiationTimeoutSec_.c_str());
    tlsSetting_->timeout.msec = atol (tlsNegotiationTimeoutMsec_.c_str());
}

void SIPAccount::initStunConfiguration (void)
{
    size_t pos;
    std::string stunServer, serverName, serverPort;

    stunServer = stunServer_;
    // Init STUN socket
    pos = stunServer.find (':');

    if (pos == std::string::npos) {
        stunServerName_ = pj_str ( (char*) stunServer.data());
        stunPort_ = PJ_STUN_PORT;
        //stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);
    } else {
        serverName = stunServer.substr (0, pos);
        serverPort = stunServer.substr (pos + 1);
        stunPort_ = atoi (serverPort.data());
        stunServerName_ = pj_str ( (char*) serverName.data());
        //stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) nPort);
    }
}

void SIPAccount::loadConfig()
{
    if (registrationExpire_.empty())
        registrationExpire_ = DFT_EXPIRE_VALUE;

    if (tlsEnable_ == "true") {
        initTlsConfiguration();
        transportType_ = PJSIP_TRANSPORT_TLS;
    } else {
        transportType_ = PJSIP_TRANSPORT_UDP;
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

std::string SIPAccount::getFromUri (void) const
{
    char uri[PJSIP_MAX_URL_SIZE];

    std::string scheme;
    std::string transport;
    std::string username = username_;
    std::string hostname = hostname_;

    // UDP does not require the transport specification

    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (transportType_));
    } else
        scheme = "sip:";

    // Get login name if username is not specified
    if (username_.empty())
        username = getLoginName();

    // Get machine hostname if not provided
    if (hostname_.empty())
        hostname = getMachineName();

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
    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (transportType_));
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
        hostname = hostname_;
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
    std::string hostname = hostname_;

    // UDP does not require the transport specification

    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (transportType_));
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

    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string (pjsip_transport_get_type_name (transportType_));
    } else
        scheme = "sip:";

    _debug ("Display Name: %s", displayName_.c_str());

    int len = pj_ansi_snprintf (contact, PJSIP_MAX_URL_SIZE,

            "%s%s<%s%s%s%s%s%s:%d%s>",
            displayName_.c_str(),
            (displayName_.empty() ? "" : " "),
            scheme.c_str(),
            username_.c_str(),
            (username_.empty() ? "":"@"),
            beginquote,
            address.c_str(),
            endquote,
            atoi (port.c_str()),
            transport.c_str());

    return std::string (contact, len);
}


namespace {
std::string computeMd5HashFromCredential (
    const std::string& username, const std::string& password,
    const std::string& realm)
{
#define MD5_APPEND(pms,buf,len) pj_md5_update(pms, (const pj_uint8_t*)buf, len)

    pj_md5_context pms;

    /* Compute md5 hash = MD5(username ":" realm ":" password) */
    pj_md5_init (&pms);
    MD5_APPEND (&pms, username.data(), username.length());
    MD5_APPEND (&pms, ":", 1);
    MD5_APPEND (&pms, realm.data(), realm.length());
    MD5_APPEND (&pms, ":", 1);
    MD5_APPEND (&pms, password.data(), password.length());

    unsigned char digest[16];
    pj_md5_final (&pms, digest);

    char hash[32];
    int i;
    for (i = 0; i < 16; ++i) {
        pj_val_to_hex_digit (digest[i], &hash[2*i]);
    }

    return std::string(hash, 32);
}


} // anon namespace

void SIPAccount::setCredentials (const std::vector<std::map<std::string, std::string> >& creds)
{
    bool md5HashingEnabled = Manager::instance().preferences.getMd5Hash();
    std::vector< std::map<std::string, std::string> >::iterator it;

	assert(creds.size() > 0); // we can not authenticate without credentials

	credentials_ = creds;

    /* md5 hashing */
    for(it = credentials_.begin(); it != credentials_.end(); ++it) {
        std::string username = (*it)[USERNAME];
        std::string realm = (*it)[REALM];
        std::string password = (*it)[PASSWORD];

        if (md5HashingEnabled) {
            // TODO: Fix this.
            // This is an extremly weak test in order to check
            // if the password is a hashed value. This is done
            // because deleteCredential() is called before this
            // method. Therefore, we cannot check if the value
            // is different from the one previously stored in
            // the configuration file. This is to avoid to
            // re-hash a hashed password.

            if (password.length() != 32) {
                (*it)[PASSWORD] = computeMd5HashFromCredential(username, password, realm);
            }
        }
    }

    std::string digest;
    size_t n = getCredentialCount();

    // Create the credential array
    delete[] cred_;
    cred_ = new pjsip_cred_info[n];

    unsigned i;
    for (i = 0; i < n; i++) {
    	const std::string &password = credentials_[i][PASSWORD];
    	int dataType = (md5HashingEnabled && password.length() == 32)
			? PJSIP_CRED_DATA_DIGEST
			: PJSIP_CRED_DATA_PLAIN_PASSWD;

        cred_[i].username = pj_str ((char*)credentials_[i][USERNAME].c_str());
        cred_[i].data = pj_str ((char*)password.c_str());
        cred_[i].realm = pj_str ((char*)credentials_[i][REALM].c_str());

        cred_[i].data_type = dataType;
        cred_[i].scheme = pj_str ( (char*) "digest");
    }
}

const std::vector<std::map<std::string, std::string> > &SIPAccount::getCredentials (void)
{
    return credentials_;
}
