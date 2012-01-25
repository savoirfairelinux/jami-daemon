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
#include "sipvoiplink.h"
#include "manager.h"
#include "config.h"
#include <pwd.h>
#include <sstream>
#include <cassert>

const char * const SIPAccount::OVERRTP_STR = "overrtp";
const char * const SIPAccount::SIPINFO_STR = "sipinfo";

SIPAccount::SIPAccount(const std::string& accountID)
    : Account(accountID, "SIP")
    , transport_(NULL)
    , credentials_()
    , regc_(NULL)
    , bRegister_(false)
    , registrationExpire_(600)
    , interface_("default")
    , publishedSameasLocal_(true)
    , publishedIpAddress_()
    , localPort_(DEFAULT_SIP_PORT)
    , publishedPort_(DEFAULT_SIP_PORT)
    , serviceRoute_()
    , tlsListenerPort_(DEFAULT_SIP_TLS_PORT)
    , transportType_(PJSIP_TRANSPORT_UNSPECIFIED)
    , cred_(NULL)
    , tlsSetting_()
    , contactHeader_()
    , stunServerName_()
    , stunPort_(0)
    , dtmfType_(OVERRTP_STR)
    , tlsEnable_("false")
    , tlsPort_(DEFAULT_SIP_TLS_PORT)
    , tlsCaListFile_()
    , tlsCertificateFile_()
    , tlsPrivateKeyFile_()
    , tlsPassword_()
    , tlsMethod_("TLSv1")
    , tlsCiphers_()
    , tlsServerName_(0, 0)
    , tlsVerifyServer_(true)
    , tlsVerifyClient_(true)
    , tlsRequireClientCertificate_(true)
    , tlsNegotiationTimeoutSec_("2")
    , tlsNegotiationTimeoutMsec_("0")
    , stunServer_("stun.sflphone.org")
    , stunEnabled_(false)
    , srtpEnabled_(false)
    , srtpKeyExchange_("sdes")
    , srtpFallback_(false)
    , zrtpDisplaySas_(true)
    , zrtpDisplaySasOnce_(false)
    , zrtpHelloHash_(true)
    , zrtpNotSuppWarning_(true)
    , registrationStateDetailed_()
    , keepAliveTimer_()
{
    link_ = SIPVoIPLink::instance();
}

SIPAccount::~SIPAccount()
{
    delete [] cred_;
}

void SIPAccount::serialize(Conf::YamlEmitter *emitter)
{
    using namespace Conf;
    using std::vector;
    using std::string;
    MappingNode accountmap(NULL);
    MappingNode srtpmap(NULL);
    MappingNode zrtpmap(NULL);
    MappingNode tlsmap(NULL);

    ScalarNode id(Account::accountID_);
    ScalarNode username(Account::username_);
    ScalarNode alias(Account::alias_);
    ScalarNode hostname(Account::hostname_);
    ScalarNode enable(enabled_);
    ScalarNode type(Account::type_);
    std::stringstream expirevalstr;
    expirevalstr << registrationExpire_;
    ScalarNode expire(expirevalstr);
    ScalarNode interface(interface_);
    std::stringstream portstr;
    portstr << localPort_;
    ScalarNode port(portstr.str());
    ScalarNode serviceRoute(serviceRoute_);

    ScalarNode mailbox(mailBox_);
    ScalarNode publishAddr(publishedIpAddress_);
    std::stringstream publicportstr;
    publicportstr << publishedPort_;

    ScalarNode publishPort(publicportstr.str());

    ScalarNode sameasLocal(publishedSameasLocal_);
    ScalarNode codecs(codecStr_);
#ifdef SFL_VIDEO
    for (vector<string>::const_iterator i = videoCodecList_.begin();
            i != videoCodecList_.end(); ++i)
        DEBUG("%s", i->c_str());
    DEBUG("%s", Manager::instance().serialize(videoCodecList_).c_str());

    ScalarNode vcodecs(Manager::instance().serialize(videoCodecList_));
#endif

    ScalarNode ringtonePath(ringtonePath_);
    ScalarNode ringtoneEnabled(ringtoneEnabled_);
    ScalarNode stunServer(stunServer_);
    ScalarNode stunEnabled(stunEnabled_);
    ScalarNode displayName(displayName_);
    ScalarNode dtmfType(dtmfType_);

    std::stringstream countstr;
    countstr << 0;
    ScalarNode count(countstr.str());

    ScalarNode srtpenabled(srtpEnabled_);
    ScalarNode keyExchange(srtpKeyExchange_);
    ScalarNode rtpFallback(srtpFallback_);

    ScalarNode displaySas(zrtpDisplaySas_);
    ScalarNode displaySasOnce(zrtpDisplaySasOnce_);
    ScalarNode helloHashEnabled(zrtpHelloHash_);
    ScalarNode notSuppWarning(zrtpNotSuppWarning_);

    portstr.str("");
    portstr << tlsPort_;
    
    ScalarNode tlsport(portstr.str());
    ScalarNode certificate(tlsCertificateFile_);
    ScalarNode calist(tlsCaListFile_);
    ScalarNode ciphers(tlsCiphers_);
    ScalarNode tlsenabled(tlsEnable_);
    ScalarNode tlsmethod(tlsMethod_);
    ScalarNode timeout(tlsNegotiationTimeoutSec_);
    ScalarNode tlspassword(tlsPassword_);
    ScalarNode privatekey(tlsPrivateKeyFile_);
    ScalarNode requirecertif(tlsRequireClientCertificate_);
    ScalarNode server(tlsServerName_);
    ScalarNode verifyclient(tlsVerifyServer_);
    ScalarNode verifyserver(tlsVerifyClient_);

    accountmap.setKeyValue(aliasKey, &alias);
    accountmap.setKeyValue(typeKey, &type);
    accountmap.setKeyValue(idKey, &id);
    accountmap.setKeyValue(usernameKey, &username);
    accountmap.setKeyValue(hostnameKey, &hostname);
    accountmap.setKeyValue(accountEnableKey, &enable);
    accountmap.setKeyValue(mailboxKey, &mailbox);
    accountmap.setKeyValue(expireKey, &expire);
    accountmap.setKeyValue(interfaceKey, &interface);
    accountmap.setKeyValue(portKey, &port);
    accountmap.setKeyValue(stunServerKey, &stunServer);
    accountmap.setKeyValue(stunEnabledKey, &stunEnabled);
    accountmap.setKeyValue(publishAddrKey, &publishAddr);
    accountmap.setKeyValue(publishPortKey, &publishPort);
    accountmap.setKeyValue(sameasLocalKey, &sameasLocal);
    accountmap.setKeyValue(serviceRouteKey, &serviceRoute);
    accountmap.setKeyValue(dtmfTypeKey, &dtmfType);
    accountmap.setKeyValue(displayNameKey, &displayName);
    accountmap.setKeyValue(codecsKey, &codecs);
#ifdef SFL_VIDEO
    accountmap.setKeyValue(videocodecsKey, &vcodecs);
#endif
    accountmap.setKeyValue(ringtonePathKey, &ringtonePath);
    accountmap.setKeyValue(ringtoneEnabledKey, &ringtoneEnabled);

    accountmap.setKeyValue(srtpKey, &srtpmap);
    srtpmap.setKeyValue(srtpEnableKey, &srtpenabled);
    srtpmap.setKeyValue(keyExchangeKey, &keyExchange);
    srtpmap.setKeyValue(rtpFallbackKey, &rtpFallback);

    accountmap.setKeyValue(zrtpKey, &zrtpmap);
    zrtpmap.setKeyValue(displaySasKey, &displaySas);
    zrtpmap.setKeyValue(displaySasOnceKey, &displaySasOnce);
    zrtpmap.setKeyValue(helloHashEnabledKey, &helloHashEnabled);
    zrtpmap.setKeyValue(notSuppWarningKey, &notSuppWarning);

    SequenceNode credentialseq(NULL);
    accountmap.setKeyValue(credKey, &credentialseq);

    std::vector<std::map<std::string, std::string> >::const_iterator it;

    for (it = credentials_.begin(); it != credentials_.end(); ++it) {
        std::map<std::string, std::string> cred = *it;
        MappingNode *map = new MappingNode(NULL);
        map->setKeyValue(USERNAME, new ScalarNode(cred[USERNAME]));
        map->setKeyValue(PASSWORD, new ScalarNode(cred[PASSWORD]));
        map->setKeyValue(REALM, new ScalarNode(cred[REALM]));
        credentialseq.addNode(map);
    }

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

    try {
        emitter->serializeAccount(&accountmap);
    } catch (const YamlEmitterException &e) {
        ERROR("ConfigTree: %s", e.what());
    }

    Sequence *seq = credentialseq.getSequence();
    Sequence::iterator seqit;

    for (seqit = seq->begin(); seqit != seq->end(); ++seqit) {
        MappingNode *node = (MappingNode*)*seqit;
        delete node->getValue(USERNAME);
        delete node->getValue(PASSWORD);
        delete node->getValue(REALM);
        delete node;
    }
}

void SIPAccount::unserialize(Conf::MappingNode *map)
{
    using namespace Conf;
    MappingNode *srtpMap;
    MappingNode *tlsMap;
    MappingNode *zrtpMap;

    assert(map);

    map->getValue(aliasKey, &alias_);
    map->getValue(typeKey, &type_);
    map->getValue(usernameKey, &username_);
    map->getValue(hostnameKey, &hostname_);
    map->getValue(accountEnableKey, &enabled_);
    map->getValue(mailboxKey, &mailBox_);
    map->getValue(codecsKey, &codecStr_);
#ifdef SFL_VIDEO
    std::string vcodecs;
    map->getValue(videocodecsKey, &vcodecs);
#endif

    // Update codec list which one is used for SDP offer
    setActiveCodecs(ManagerImpl::unserialize(codecStr_));

#ifdef SFL_VIDEO
    setActiveVideoCodecs(Manager::instance().unserialize(vcodecs));
#endif

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

    std::string dtmfType;
    map->getValue(dtmfTypeKey, &dtmfType);
    dtmfType_ = dtmfType;

    map->getValue(serviceRouteKey, &serviceRoute_);
    // stun enabled
    map->getValue(stunEnabledKey, &stunEnabled_);
    map->getValue(stunServerKey, &stunServer_);

    // Init stun server name with default server name
    stunServerName_ = pj_str((char*) stunServer_.data());

    map->getValue(displayNameKey, &displayName_);

    std::vector<std::map<std::string, std::string> > creds;

    YamlNode *credNode = map->getValue(credKey);

    /* We check if the credential key is a sequence
     * because it was a mapping in a previous version of
     * the configuration file.
     */
    if (credNode && credNode->getType() == SEQUENCE) {
        SequenceNode *credSeq = (SequenceNode *) credNode;
        Sequence::iterator it;
        Sequence *seq = credSeq->getSequence();

        for (it = seq->begin(); it != seq->end(); ++it) {
            MappingNode *cred = (MappingNode *)(*it);
            std::string user;
            std::string pass;
            std::string realm;
            cred->getValue(USERNAME, &user);
            cred->getValue(PASSWORD, &pass);
            cred->getValue(REALM, &realm);
            std::map<std::string, std::string> credentialMap;
            credentialMap[USERNAME] = user;
            credentialMap[PASSWORD] = pass;
            credentialMap[REALM] = realm;
            creds.push_back(credentialMap);
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

    setCredentials(creds);

    // get srtp submap
    srtpMap = (MappingNode *)(map->getValue(srtpKey));

    if (srtpMap) {
        srtpMap->getValue(srtpEnableKey, &srtpEnabled_);
        srtpMap->getValue(keyExchangeKey, &srtpKeyExchange_);
        srtpMap->getValue(rtpFallbackKey, &srtpFallback_);
    }

    // get zrtp submap
    zrtpMap = (MappingNode *)(map->getValue(zrtpKey));

    if (zrtpMap) {
        zrtpMap->getValue(displaySasKey, &zrtpDisplaySas_);
        zrtpMap->getValue(displaySasOnceKey, &zrtpDisplaySasOnce_);
        zrtpMap->getValue(helloHashEnabledKey, &zrtpHelloHash_);
        zrtpMap->getValue(notSuppWarningKey, &zrtpNotSuppWarning_);
    }

    // get tls submap
    tlsMap = (MappingNode *)(map->getValue(tlsKey));

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


void SIPAccount::setAccountDetails(std::map<std::string, std::string> details)
{
    // Account setting common to SIP and IAX
    alias_ = details[CONFIG_ACCOUNT_ALIAS];
    type_ = details[CONFIG_ACCOUNT_TYPE];
    username_ = details[USERNAME];
    hostname_ = details[HOSTNAME];
    enabled_ = details[CONFIG_ACCOUNT_ENABLE] == "true";
    ringtonePath_ = details[CONFIG_RINGTONE_PATH];
    ringtoneEnabled_ = details[CONFIG_RINGTONE_ENABLED] == "true";
    mailBox_ = details[CONFIG_ACCOUNT_MAILBOX];

    // SIP specific account settings

    // general sip settings
    displayName_ = details[DISPLAY_NAME];
    serviceRoute_ = details[ROUTESET];
    interface_ = details[LOCAL_INTERFACE];
    publishedSameasLocal_ = details[PUBLISHED_SAMEAS_LOCAL] == "true";
    publishedIpAddress_ = details[PUBLISHED_ADDRESS];
    localPort_ = atoi(details[LOCAL_PORT].c_str());
    publishedPort_ = atoi(details[PUBLISHED_PORT].c_str());
    stunServer_ = details[STUN_SERVER];
    stunEnabled_ = details[STUN_ENABLE] == "true";
    dtmfType_ = details[ACCOUNT_DTMF_TYPE];

    registrationExpire_ = atoi(details[CONFIG_ACCOUNT_REGISTRATION_EXPIRE].c_str());

    userAgent_ = details[USERAGENT];

    // srtp settings
    srtpEnabled_ = details[SRTP_ENABLE] == "true";
    srtpFallback_ = details[SRTP_RTP_FALLBACK] == "true";
    zrtpDisplaySas_ = details[ZRTP_DISPLAY_SAS] == "true";
    zrtpDisplaySasOnce_ = details[ZRTP_DISPLAY_SAS_ONCE] == "true";
    zrtpNotSuppWarning_ = details[ZRTP_NOT_SUPP_WARNING] == "true";
    zrtpHelloHash_ = details[ZRTP_HELLO_HASH] == "true";
    srtpKeyExchange_ = details[SRTP_KEY_EXCHANGE];

    // TLS settings
    // The TLS listener is unique and globally defined through IP2IP_PROFILE
    if (accountID_ == IP2IP_PROFILE)
        tlsListenerPort_ = atoi(details[TLS_LISTENER_PORT].c_str());

    tlsEnable_ = details[TLS_ENABLE];
    tlsCaListFile_ = details[TLS_CA_LIST_FILE];
    tlsCertificateFile_ = details[TLS_CERTIFICATE_FILE];
    tlsPrivateKeyFile_ = details[TLS_PRIVATE_KEY_FILE];
    tlsPassword_ = details[TLS_PASSWORD];
    tlsMethod_ = details[TLS_METHOD];
    tlsCiphers_ = details[TLS_CIPHERS];
    tlsServerName_ = details[TLS_SERVER_NAME];
    tlsVerifyServer_ = details[TLS_VERIFY_SERVER] == "true";
    tlsVerifyClient_ = details[TLS_VERIFY_CLIENT] == "true";
    tlsRequireClientCertificate_ = details[TLS_REQUIRE_CLIENT_CERTIFICATE] == "true";
    tlsNegotiationTimeoutSec_ = details[TLS_NEGOTIATION_TIMEOUT_SEC];
    tlsNegotiationTimeoutMsec_ = details[TLS_NEGOTIATION_TIMEOUT_MSEC];

    if (credentials_.empty()) { // credentials not set, construct 1 entry
        std::vector<std::map<std::string, std::string> > v;
        std::map<std::string, std::string> map;
        map[USERNAME] = username_;
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

    a[REGISTRATION_STATUS] = (accountID_ == IP2IP_PROFILE) ? "READY": mapStateNumberToString(state);
    a[REGISTRATION_STATE_CODE] = registrationStateCode;
    a[REGISTRATION_STATE_DESCRIPTION] = registrationStateDescription;

    // Add sip specific details
    a[ROUTESET] = serviceRoute_;
    a[USERAGENT] = userAgent_;

    std::stringstream expireval;
    expireval << registrationExpire_;
    a[CONFIG_ACCOUNT_REGISTRATION_EXPIRE] = expireval.str();
    a[LOCAL_INTERFACE] = interface_;
    a[PUBLISHED_SAMEAS_LOCAL] = publishedSameasLocal_ ? "true" : "false";
    a[PUBLISHED_ADDRESS] = publishedIpAddress_;

    std::stringstream localport;
    localport << localPort_;
    a[LOCAL_PORT] = localport.str();
    std::stringstream publishedport;
    publishedport << publishedPort_;
    a[PUBLISHED_PORT] = publishedport.str();
    a[STUN_ENABLE] = stunEnabled_ ? "true" : "false";
    a[STUN_SERVER] = stunServer_;
    a[ACCOUNT_DTMF_TYPE] = dtmfType_;

    a[SRTP_KEY_EXCHANGE] = srtpKeyExchange_;
    a[SRTP_ENABLE] = srtpEnabled_ ? "true" : "false";
    a[SRTP_RTP_FALLBACK] = srtpFallback_ ? "true" : "false";

    a[ZRTP_DISPLAY_SAS] = zrtpDisplaySas_ ? "true" : "false";
    a[ZRTP_DISPLAY_SAS_ONCE] = zrtpDisplaySasOnce_ ? "true" : "false";
    a[ZRTP_HELLO_HASH] = zrtpHelloHash_ ? "true" : "false";
    a[ZRTP_NOT_SUPP_WARNING] = zrtpNotSuppWarning_ ? "true" : "false";

    // TLS listener is unique and parameters are modified through IP2IP_PROFILE
    std::stringstream tlslistenerport;
    tlslistenerport << tlsListenerPort_;
    a[TLS_LISTENER_PORT] = tlslistenerport.str();
    a[TLS_ENABLE] = tlsEnable_;
    a[TLS_CA_LIST_FILE] = tlsCaListFile_;
    a[TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    a[TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    a[TLS_PASSWORD] = tlsPassword_;
    a[TLS_METHOD] = tlsMethod_;
    a[TLS_CIPHERS] = tlsCiphers_;
    a[TLS_SERVER_NAME] = tlsServerName_;
    a[TLS_VERIFY_SERVER] = tlsVerifyServer_ ? "true" : "false";
    a[TLS_VERIFY_CLIENT] = tlsVerifyClient_ ? "true" : "false";
    a[TLS_REQUIRE_CLIENT_CERTIFICATE] = tlsRequireClientCertificate_ ? "true" : "false";
    a[TLS_NEGOTIATION_TIMEOUT_SEC] = tlsNegotiationTimeoutSec_;
    a[TLS_NEGOTIATION_TIMEOUT_MSEC] = tlsNegotiationTimeoutMsec_;

    return a;
}

void SIPAccount::registerVoIPLink()
{
    if (hostname_.length() >= PJ_MAX_HOSTNAME)
        return;

    // Init TLS settings if the user wants to use TLS
    if (tlsEnable_ == "true") {
        DEBUG("SIPAccount: TLS is enabled for account %s", accountID_.c_str());
        transportType_ = PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();
    }

    // Init STUN settings for this account if the user selected it
    if (stunEnabled_) {
        transportType_ = PJSIP_TRANSPORT_START_OTHER;
        initStunConfiguration();
    } else {
        stunServerName_ = pj_str((char*) stunServer_.c_str());
    }

    // In our definition of the ip2ip profile (aka Direct IP Calls),
    // no registration should be performed
    if (accountID_ == IP2IP_PROFILE)
        return;

    try {
        link_->sendRegister(this);
    } catch (const VoipLinkException &e) {
        ERROR("SIPAccount: %s", e.what());
    }
}

void SIPAccount::unregisterVoIPLink()
{
    if (accountID_ == IP2IP_PROFILE)
        return;

    try {
        link_->sendUnregister(this);
    } catch (const VoipLinkException &e) {
        ERROR("SIPAccount: %s", e.what());
    }
}

void SIPAccount::startKeepAliveTimer() {
    pj_time_val keepAliveDelay_;

    if (isTlsEnabled())
        return;

    keepAliveTimer_.cb = &SIPAccount::keepAliveRegistrationCb;
    keepAliveTimer_.user_data = (void *)this; 

    // expiration may no be determined when during the first registration request
    if(registrationExpire_ == 0) {
        keepAliveDelay_.sec = 60;
    }
    else {
        keepAliveDelay_.sec = registrationExpire_;
    }
    keepAliveDelay_.msec = 0;
 
    reinterpret_cast<SIPVoIPLink *>(link_)->registerKeepAliveTimer(keepAliveTimer_, keepAliveDelay_); 
}

void SIPAccount::stopKeepAliveTimer() {
     reinterpret_cast<SIPVoIPLink *>(link_)->cancelKeepAliveTimer(keepAliveTimer_); 
}

pjsip_ssl_method SIPAccount::sslMethodStringToPjEnum(const std::string& method)
{
    if (method == "Default")
        return PJSIP_SSL_UNSPECIFIED_METHOD;

    if (method == "TLSv1")
        return PJSIP_TLSV1_METHOD;

    if (method == "SSLv3")
        return PJSIP_SSLV3_METHOD;

    if (method == "SSLv23")
        return PJSIP_SSLV23_METHOD;

    return PJSIP_SSL_UNSPECIFIED_METHOD;
}

void SIPAccount::initTlsConfiguration()
{
    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    tlsListenerPort_ = tlsPort_;

    pjsip_tls_setting_default(&tlsSetting_);

    pj_cstr(&tlsSetting_.ca_list_file, tlsCaListFile_.c_str());
    pj_cstr(&tlsSetting_.cert_file, tlsCertificateFile_.c_str());
    pj_cstr(&tlsSetting_.privkey_file, tlsPrivateKeyFile_.c_str());
    pj_cstr(&tlsSetting_.password, tlsPassword_.c_str());
    tlsSetting_.method = sslMethodStringToPjEnum(tlsMethod_);
    pj_cstr(&tlsSetting_.ciphers, tlsCiphers_.c_str());
    pj_cstr(&tlsSetting_.server_name, tlsServerName_.c_str());

    tlsSetting_.verify_server = tlsVerifyServer_ ? PJ_TRUE: PJ_FALSE;
    tlsSetting_.verify_client = tlsVerifyClient_ ? PJ_TRUE: PJ_FALSE;
    tlsSetting_.require_client_cert = tlsRequireClientCertificate_ ? PJ_TRUE: PJ_FALSE;

    tlsSetting_.timeout.sec = atol(tlsNegotiationTimeoutSec_.c_str());
    tlsSetting_.timeout.msec = atol(tlsNegotiationTimeoutMsec_.c_str());
}

void SIPAccount::initStunConfiguration()
{
    size_t pos;
    std::string stunServer, serverName, serverPort;

    stunServer = stunServer_;
    // Init STUN socket
    pos = stunServer.find(':');

    if (pos == std::string::npos) {
        stunServerName_ = pj_str((char*) stunServer.data());
        stunPort_ = PJ_STUN_PORT;
        //stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);
    } else {
        serverName = stunServer.substr(0, pos);
        serverPort = stunServer.substr(pos + 1);
        stunPort_ = atoi(serverPort.data());
        stunServerName_ = pj_str((char*) serverName.data());
        //stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) nPort);
    }
}

void SIPAccount::loadConfig()
{
    if (registrationExpire_ == 0)
        registrationExpire_ = 600; /** Default expire value for registration */

    if (tlsEnable_ == "true") {
        initTlsConfiguration();
        transportType_ = PJSIP_TRANSPORT_TLS;
    } else
        transportType_ = PJSIP_TRANSPORT_UDP;
}

bool SIPAccount::fullMatch(const std::string& username, const std::string& hostname) const
{
    return userMatch(username) and hostnameMatch(hostname);
}

bool SIPAccount::userMatch(const std::string& username) const
{
    return !username.empty() and username == username_;
}

bool SIPAccount::hostnameMatch(const std::string& hostname) const
{
    return hostname == hostname_;
}

std::string SIPAccount::getLoginName()
{
    struct passwd * user_info = getpwuid(getuid());
    return user_info ? user_info->pw_name : "";
}

std::string SIPAccount::getFromUri() const
{
    std::string scheme;
    std::string transport;
    std::string username(username_);
    std::string hostname(hostname_);

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else
        scheme = "sip:";

    // Get login name if username is not specified
    if (username_.empty())
        username = getLoginName();

    // Get machine hostname if not provided
    if (hostname_.empty())
        hostname = std::string(pj_gethostname()->ptr, pj_gethostname()->slen);

    return "<" + scheme + username + "@" + hostname + transport + ">";
}

std::string SIPAccount::getToUri(const std::string& username) const
{
    std::string scheme;
    std::string transport;
    std::string hostname;

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else
        scheme = "sip:";

    // Check if scheme is already specified
    if (username.find("sip") == 0)
        scheme = "";

    // Check if hostname is already specified
    if (username.find("@") == std::string::npos)
        hostname = hostname_;

    return "<" + scheme + username + (hostname.empty() ? "" : "@") + hostname + transport + ">";
}

std::string SIPAccount::getServerUri() const
{
    std::string scheme;
    std::string transport;

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else {
        scheme = "sip:";
        transport = "";
    }

    return "<" + scheme + hostname_ + transport + ">";
}

std::string SIPAccount::getContactHeader() const
{
    std::string scheme;
    std::string transport;

    // Use the CONTACT header provided by the registrar if any
    if(!contactHeader_.empty())
        return contactHeader_;

    // Else we determine this infor based on transport information
    std::string address, port;
    SIPVoIPLink *siplink = dynamic_cast<SIPVoIPLink *>(link_);
    siplink->findLocalAddressFromTransport(transport_, transportType_, address, port);

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else
        scheme = "sip:";

    return displayName_ + (displayName_.empty() ? "" : " ") + "<" +
           scheme + username_ + (username_.empty() ? "":"@") +
           address + ":" + port + transport + ">";
}

void SIPAccount::keepAliveRegistrationCb(UNUSED pj_timer_heap_t *th, pj_timer_entry *te) 
{
   SIPAccount *sipAccount = reinterpret_cast<SIPAccount *>(te->user_data);

   if (sipAccount->isTlsEnabled())
       return;

   if(sipAccount->isRegistered()) {

       // send a new register request
       sipAccount->registerVoIPLink();

       // make sure the current timer is deactivated   
       sipAccount->stopKeepAliveTimer(); 

       // register a new timer
       sipAccount->startKeepAliveTimer(); 
   }
}

namespace {
std::string computeMd5HashFromCredential(
    const std::string& username, const std::string& password,
    const std::string& realm)
{
#define MD5_APPEND(pms,buf,len) pj_md5_update(pms, (const pj_uint8_t*)buf, len)

    pj_md5_context pms;

    /* Compute md5 hash = MD5(username ":" realm ":" password) */
    pj_md5_init(&pms);
    MD5_APPEND(&pms, username.data(), username.length());
    MD5_APPEND(&pms, ":", 1);
    MD5_APPEND(&pms, realm.data(), realm.length());
    MD5_APPEND(&pms, ":", 1);
    MD5_APPEND(&pms, password.data(), password.length());

    unsigned char digest[16];
    pj_md5_final(&pms, digest);

    char hash[32];

    for (int i = 0; i < 16; ++i)
        pj_val_to_hex_digit(digest[i], &hash[2*i]);

    return std::string(hash, 32);
}


} // anon namespace

void SIPAccount::setCredentials(const std::vector<std::map<std::string, std::string> >& creds)
{
    using std::vector;
    using std::string;
    using std::map;

    bool md5HashingEnabled = Manager::instance().preferences.getMd5Hash();

    assert(creds.size() > 0); // we can not authenticate without credentials

    credentials_ = creds;

    /* md5 hashing */
    for (vector<map<string, string> >::iterator it = credentials_.begin(); it != credentials_.end(); ++it) {
        map<string, string>::const_iterator val = (*it).find(USERNAME);
        const std::string username = val != (*it).end() ? val->second : "";
        val = (*it).find(REALM);
        const std::string realm(val != (*it).end() ? val->second : "");
        val = (*it).find(PASSWORD);
        const std::string password(val != (*it).end() ? val->second : "");

        if (md5HashingEnabled) {
            // TODO: Fix this.
            // This is an extremly weak test in order to check
            // if the password is a hashed value. This is done
            // because deleteCredential() is called before this
            // method. Therefore, we cannot check if the value
            // is different from the one previously stored in
            // the configuration file. This is to avoid to
            // re-hash a hashed password.

            if (password.length() != 32)
                (*it)[PASSWORD] = computeMd5HashFromCredential(username, password, realm);
        }
    }

    // Create the credential array
    delete[] cred_;
    cred_ = new pjsip_cred_info[credentials_.size()];

    size_t i = 0;

    for (vector<map<string, string > >::const_iterator iter = credentials_.begin();
            iter != credentials_.end(); ++iter) {
        map<string, string>::const_iterator val = (*iter).find(PASSWORD);
        const std::string password = val != (*iter).end() ? val->second : "";
        int dataType = (md5HashingEnabled and password.length() == 32)
                       ? PJSIP_CRED_DATA_DIGEST
                       : PJSIP_CRED_DATA_PLAIN_PASSWD;

        val = (*iter).find(USERNAME);

        if (val != (*iter).end())
            cred_[i].username = pj_str((char*) val->second.c_str());

        cred_[i].data = pj_str((char*) password.c_str());

        val = (*iter).find(REALM);

        if (val != (*iter).end())
            cred_[i].realm = pj_str((char*) val->second.c_str());

        cred_[i].data_type = dataType;
        cred_[i].scheme = pj_str((char*) "digest");
        ++i;
    }
}

const std::vector<std::map<std::string, std::string> > &SIPAccount::getCredentials()
{
    return credentials_;
}

std::string SIPAccount::getUserAgentName() const
{
    std::string result(userAgent_);

    if (result == "sflphone" or result.empty())
        result += "/" PACKAGE_VERSION;

    return result;
}

std::map<std::string, std::string> SIPAccount::getIp2IpDetails() const
{
    assert(accountID_ == IP2IP_PROFILE);
    std::map<std::string, std::string> ip2ipAccountDetails;
    ip2ipAccountDetails[ACCOUNT_ID] = IP2IP_PROFILE;
    ip2ipAccountDetails[SRTP_KEY_EXCHANGE] = srtpKeyExchange_;
    ip2ipAccountDetails[SRTP_ENABLE] = srtpEnabled_ ? "true" : "false";
    ip2ipAccountDetails[SRTP_RTP_FALLBACK] = srtpFallback_ ? "true" : "false";
    ip2ipAccountDetails[ZRTP_DISPLAY_SAS] = zrtpDisplaySas_ ? "true" : "false";
    ip2ipAccountDetails[ZRTP_HELLO_HASH] = zrtpHelloHash_ ? "true" : "false";
    ip2ipAccountDetails[ZRTP_NOT_SUPP_WARNING] = zrtpNotSuppWarning_ ? "true" : "false";
    ip2ipAccountDetails[ZRTP_DISPLAY_SAS_ONCE] = zrtpDisplaySasOnce_ ? "true" : "false";
    ip2ipAccountDetails[LOCAL_INTERFACE] = interface_;
    std::stringstream portstr;
    portstr << localPort_;
    ip2ipAccountDetails[LOCAL_PORT] = portstr.str();

    std::map<std::string, std::string> tlsSettings;
    tlsSettings = getTlsSettings();
    std::copy(tlsSettings.begin(), tlsSettings.end(), std::inserter(
                  ip2ipAccountDetails, ip2ipAccountDetails.end()));

    return ip2ipAccountDetails;
}

std::map<std::string, std::string> SIPAccount::getTlsSettings() const
{
    std::map<std::string, std::string> tlsSettings;
    assert(accountID_ == IP2IP_PROFILE);

    std::stringstream portstr;
    portstr << tlsListenerPort_;
    tlsSettings[TLS_LISTENER_PORT] = portstr.str();
    tlsSettings[TLS_ENABLE] = tlsEnable_;
    tlsSettings[TLS_CA_LIST_FILE] = tlsCaListFile_;
    tlsSettings[TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    tlsSettings[TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    tlsSettings[TLS_PASSWORD] = tlsPassword_;
    tlsSettings[TLS_METHOD] = tlsMethod_;
    tlsSettings[TLS_CIPHERS] = tlsCiphers_;
    tlsSettings[TLS_SERVER_NAME] = tlsServerName_;
    tlsSettings[TLS_VERIFY_SERVER] = tlsVerifyServer_ ? "true" : "false";
    tlsSettings[TLS_VERIFY_CLIENT] = tlsVerifyClient_ ? "true" : "false";
    tlsSettings[TLS_REQUIRE_CLIENT_CERTIFICATE] = tlsRequireClientCertificate_ ? "true" : "false";
    tlsSettings[TLS_NEGOTIATION_TIMEOUT_SEC] = tlsNegotiationTimeoutSec_;
    tlsSettings[TLS_NEGOTIATION_TIMEOUT_MSEC] = tlsNegotiationTimeoutMsec_;

    return tlsSettings;
}

namespace {
void set_opt(const std::map<std::string, std::string> &details, const char *key, std::string &val)
{
    std::map<std::string, std::string>::const_iterator it = details.find(key);

    if (it != details.end())
        val = it->second;
}

void set_opt(const std::map<std::string, std::string> &details, const char *key, bool &val)
{
    std::map<std::string, std::string>::const_iterator it = details.find(key);

    if (it != details.end())
        val = it->second == "true";
}

void set_opt(const std::map<std::string, std::string> &details, const char *key, pj_uint16_t &val)
{
    std::map<std::string, std::string>::const_iterator it = details.find(key);

    if (it != details.end())
        val = atoi(it->second.c_str());
}
} //anon namespace

void SIPAccount::setTlsSettings(const std::map<std::string, std::string>& details)
{
    assert(accountID_ == IP2IP_PROFILE);
    set_opt(details, TLS_LISTENER_PORT, tlsListenerPort_);
    set_opt(details, TLS_ENABLE, tlsEnable_);
    set_opt(details, TLS_CA_LIST_FILE, tlsCaListFile_);
    set_opt(details, TLS_CERTIFICATE_FILE, tlsCertificateFile_);
    set_opt(details, TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_);
    set_opt(details, TLS_PASSWORD, tlsPassword_);
    set_opt(details, TLS_METHOD, tlsMethod_);
    set_opt(details, TLS_CIPHERS, tlsCiphers_);
    set_opt(details, TLS_SERVER_NAME, tlsServerName_);
    set_opt(details, TLS_VERIFY_CLIENT, tlsVerifyClient_);
    set_opt(details, TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    set_opt(details, TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);
    set_opt(details, TLS_NEGOTIATION_TIMEOUT_MSEC, tlsNegotiationTimeoutMsec_);
}
