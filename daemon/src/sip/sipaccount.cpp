/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sipvoiplink.h"
#include "sip_utils.h"

#ifdef SFL_PRESENCE
#include "sippresence.h"
#include "client/configurationmanager.h"
#endif

#include "account_schema.h"
#include "config/yamlnode.h"
#include "config/yamlemitter.h"
#include "logger.h"
#include "manager.h"

#ifdef SFL_VIDEO
#include "video/libav_utils.h"
#endif

#include <unistd.h>
#include <pwd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cstdlib>

const char * const SIPAccount::IP2IP_PROFILE = "IP2IP";
const char * const SIPAccount::OVERRTP_STR = "overrtp";
const char * const SIPAccount::SIPINFO_STR = "sipinfo";
const char * const SIPAccount::ACCOUNT_TYPE = "SIP";

static const int MIN_REGISTRATION_TIME = 60;
static const int DEFAULT_REGISTRATION_TIME = 3600;
static const char *const VALID_TLS_METHODS[] = {"Default", "TLSv1", "SSLv3", "SSLv23"};
static const char *const VALID_SRTP_KEY_EXCHANGES[] = {"", "sdes", "zrtp"};

// we force RTP ports to be even, so we only need HALF_MAX_PORT booleans
bool SIPAccount::portsInUse_[HALF_MAX_PORT];

SIPAccount::SIPAccount(const std::string& accountID, bool presenceEnabled)
    : Account(accountID)
    , auto_rereg_()
    , credentials_()
    , transport_(nullptr)
    , tlsListener_(nullptr)
    , regc_(nullptr)
    , bRegister_(false)
    , registrationExpire_(MIN_REGISTRATION_TIME)
    , interface_("default")
    , publishedSameasLocal_(true)
    , publishedIp_()
    , publishedIpAddress_()
    , localPort_(DEFAULT_SIP_PORT)
    , publishedPort_(DEFAULT_SIP_PORT)
    , serviceRoute_()
    , tlsListenerPort_(DEFAULT_SIP_TLS_PORT)
    , transportType_(PJSIP_TRANSPORT_UNSPECIFIED)
    , cred_()
    , tlsSetting_()
    , ciphers_(100)
    , stunServerName_()
    , stunPort_(PJ_STUN_PORT)
    , dtmfType_(OVERRTP_STR)
    , tlsEnable_(false)
    , tlsCaListFile_()
    , tlsCertificateFile_()
    , tlsPrivateKeyFile_()
    , tlsPassword_()
    , tlsMethod_("TLSv1")
    , tlsCiphers_()
    , tlsServerName_(0, 0)
    , tlsVerifyServer_(false)
    , tlsVerifyClient_(true)
    , tlsRequireClientCertificate_(true)
    , tlsNegotiationTimeoutSec_("2")
    , stunServer_("")
    , stunEnabled_(false)
    , srtpEnabled_(false)
    , srtpKeyExchange_("")
    , srtpFallback_(false)
    , zrtpDisplaySas_(true)
    , zrtpDisplaySasOnce_(false)
    , zrtpHelloHash_(true)
    , zrtpNotSuppWarning_(true)
    , registrationStateDetailed_()
    , keepAliveEnabled_(false)
    , keepAliveTimer_()
    , keepAliveTimerActive_(false)
    , link_(SIPVoIPLink::instance())
    , receivedParameter_("")
    , rPort_(-1)
    , via_addr_()
    , contactBuffer_()
    , contact_{contactBuffer_, 0}
    , contactRewriteMethod_(2)
    , allowViaRewrite_(true)
    , allowContactRewrite_(1)
    , contactOverwritten_(false)
    , via_tp_(nullptr)
    , audioPortRange_({16384, 32766})
#ifdef SFL_VIDEO
    , videoPortRange_({49152, (MAX_PORT) - 2})
#endif
#ifdef SFL_PRESENCE
    , presence_(presenceEnabled ? new SIPPresence(this) : nullptr)
#endif
{
    via_addr_.host.ptr = 0;
    via_addr_.host.slen = 0;
    via_addr_.port = 0;

    if (isIP2IP())
        alias_ = IP2IP_PROFILE;
}


SIPAccount::~SIPAccount()
{
    // ensure that no registration callbacks survive past this point
    destroyRegistrationInfo();
    setTransport();

#ifdef SFL_PRESENCE
    delete presence_;
#endif
}


static std::array<std::unique_ptr<Conf::ScalarNode>, 2>
serializeRange(Conf::MappingNode &accountMap, const char *minKey, const char *maxKey, const std::pair<uint16_t, uint16_t> &range)
{
    using namespace Conf;
    std::array<std::unique_ptr<ScalarNode>, 2> result;

    std::ostringstream os;
    os << range.first;
    result[0].reset(new ScalarNode(os.str()));
    os.str("");
    accountMap.setKeyValue(minKey, result[0].get());

    os << range.second;
    ScalarNode portMax(os.str());
    result[1].reset(new ScalarNode(os.str()));
    accountMap.setKeyValue(maxKey, result[1].get());
    return result;
}

static void
updateRange(int min, int max, std::pair<uint16_t, uint16_t> &range)
{
    if (min > 0 and (max > min) and max <= MAX_PORT - 2) {
        range.first = min;
        range.second = max;
    }
}

static void
unserializeRange(const Conf::YamlNode &mapNode, const char *minKey, const char *maxKey, std::pair<uint16_t, uint16_t> &range)
{
    int tmpMin = 0;
    int tmpMax = 0;
    mapNode.getValue(minKey, &tmpMin);
    mapNode.getValue(maxKey, &tmpMax);
    updateRange(tmpMin, tmpMax, range);
}

void SIPAccount::serialize(Conf::YamlEmitter &emitter)
{
    using namespace Conf;
    using std::vector;
    using std::string;
    using std::map;
    MappingNode accountmap(nullptr);
    MappingNode srtpmap(nullptr);
    MappingNode zrtpmap(nullptr);
    MappingNode tlsmap(nullptr);

    ScalarNode id(Account::accountID_);
    ScalarNode username(Account::username_);
    ScalarNode alias(Account::alias_);
    ScalarNode hostname(Account::hostname_);
    ScalarNode enable(enabled_);
    ScalarNode autoAnswer(autoAnswerEnabled_);
    ScalarNode type(ACCOUNT_TYPE);
    std::stringstream registrationExpireStr;
    registrationExpireStr << registrationExpire_;
    ScalarNode expire(registrationExpireStr.str());
    ScalarNode interface(interface_);
    std::stringstream portstr;
    portstr << localPort_;
    ScalarNode port(portstr.str());
    ScalarNode serviceRoute(serviceRoute_);
    ScalarNode keepAliveEnabled(keepAliveEnabled_);

#ifdef SFL_PRESENCE
    std::string pres(presence_ and presence_->isEnabled() ? Conf::TRUE_STR : Conf::FALSE_STR);
    ScalarNode presenceEnabled(pres);
    std::string presPub(presence_ and presence_->isSupported(PRESENCE_FUNCTION_PUBLISH) ? Conf::TRUE_STR : Conf::FALSE_STR);
    ScalarNode presencePublish(presPub);
    std::string presSub(presence_ and presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE) ? Conf::TRUE_STR : Conf::FALSE_STR);
    ScalarNode presenceSubscribe(presSub);
#endif

    ScalarNode mailbox(mailBox_);
    ScalarNode publishAddr(publishedIpAddress_);
    std::stringstream publicportstr;
    publicportstr << publishedPort_;

    ScalarNode publishPort(publicportstr.str());

    ScalarNode sameasLocal(publishedSameasLocal_);
    ScalarNode audioCodecs(audioCodecStr_);
#ifdef SFL_VIDEO
    SequenceNode videoCodecs(nullptr);
    accountmap.setKeyValue(VIDEO_CODECS_KEY, &videoCodecs);

    for (auto &codec : videoCodecList_) {
        MappingNode *mapNode = new MappingNode(nullptr);
        mapNode->setKeyValue(VIDEO_CODEC_NAME, new ScalarNode(codec[VIDEO_CODEC_NAME]));
        mapNode->setKeyValue(VIDEO_CODEC_BITRATE, new ScalarNode(codec[VIDEO_CODEC_BITRATE]));
        mapNode->setKeyValue(VIDEO_CODEC_ENABLED, new ScalarNode(codec[VIDEO_CODEC_ENABLED]));
        mapNode->setKeyValue(VIDEO_CODEC_PARAMETERS, new ScalarNode(codec[VIDEO_CODEC_PARAMETERS]));
        videoCodecs.addNode(mapNode);
    }

#endif

    ScalarNode ringtonePath(ringtonePath_);
    ScalarNode ringtoneEnabled(ringtoneEnabled_);
    ScalarNode videoEnabled(videoEnabled_);
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
    portstr << tlsListenerPort_;
    ScalarNode tlsport(portstr.str());
    ScalarNode certificate(tlsCertificateFile_);
    ScalarNode calist(tlsCaListFile_);
    ScalarNode ciphersNode(tlsCiphers_);
    ScalarNode tlsenabled(tlsEnable_);
    ScalarNode tlsmethod(tlsMethod_);
    ScalarNode timeout(tlsNegotiationTimeoutSec_);
    ScalarNode tlspassword(tlsPassword_);
    ScalarNode privatekey(tlsPrivateKeyFile_);
    ScalarNode requirecertif(tlsRequireClientCertificate_);
    ScalarNode server(tlsServerName_);
    ScalarNode verifyclient(tlsVerifyServer_);
    ScalarNode verifyserver(tlsVerifyClient_);

    accountmap.setKeyValue(ALIAS_KEY, &alias);
    accountmap.setKeyValue(TYPE_KEY, &type);
    accountmap.setKeyValue(ID_KEY, &id);
    accountmap.setKeyValue(USERNAME_KEY, &username);
    accountmap.setKeyValue(HOSTNAME_KEY, &hostname);
    accountmap.setKeyValue(ACCOUNT_ENABLE_KEY, &enable);
    accountmap.setKeyValue(ACCOUNT_AUTOANSWER_KEY, &autoAnswer);
    accountmap.setKeyValue(MAILBOX_KEY, &mailbox);
    accountmap.setKeyValue(Preferences::REGISTRATION_EXPIRE_KEY, &expire);
    accountmap.setKeyValue(INTERFACE_KEY, &interface);
    accountmap.setKeyValue(PORT_KEY, &port);
    accountmap.setKeyValue(STUN_SERVER_KEY, &stunServer);
    accountmap.setKeyValue(STUN_ENABLED_KEY, &stunEnabled);
    accountmap.setKeyValue(PUBLISH_ADDR_KEY, &publishAddr);
    accountmap.setKeyValue(PUBLISH_PORT_KEY, &publishPort);
    accountmap.setKeyValue(SAME_AS_LOCAL_KEY, &sameasLocal);
    accountmap.setKeyValue(SERVICE_ROUTE_KEY, &serviceRoute);
    accountmap.setKeyValue(DTMF_TYPE_KEY, &dtmfType);
    accountmap.setKeyValue(DISPLAY_NAME_KEY, &displayName);
    accountmap.setKeyValue(AUDIO_CODECS_KEY, &audioCodecs);
    accountmap.setKeyValue(RINGTONE_PATH_KEY, &ringtonePath);
    accountmap.setKeyValue(RINGTONE_ENABLED_KEY, &ringtoneEnabled);
    accountmap.setKeyValue(VIDEO_ENABLED_KEY, &videoEnabled);
    accountmap.setKeyValue(KEEP_ALIVE_ENABLED, &keepAliveEnabled);
#ifdef SFL_PRESENCE
    accountmap.setKeyValue(PRESENCE_ENABLED_KEY, &presenceEnabled);
    accountmap.setKeyValue(PRESENCE_PUBLISH_SUPPORTED_KEY, &presencePublish);
    accountmap.setKeyValue(PRESENCE_SUBSCRIBE_SUPPORTED_KEY, &presenceSubscribe);
#endif

    accountmap.setKeyValue(SRTP_KEY, &srtpmap);
    srtpmap.setKeyValue(SRTP_ENABLE_KEY, &srtpenabled);
    srtpmap.setKeyValue(KEY_EXCHANGE_KEY, &keyExchange);
    srtpmap.setKeyValue(RTP_FALLBACK_KEY, &rtpFallback);

    accountmap.setKeyValue(ZRTP_KEY, &zrtpmap);
    zrtpmap.setKeyValue(DISPLAY_SAS_KEY, &displaySas);
    zrtpmap.setKeyValue(DISPLAY_SAS_ONCE_KEY, &displaySasOnce);
    zrtpmap.setKeyValue(HELLO_HASH_ENABLED_KEY, &helloHashEnabled);
    zrtpmap.setKeyValue(NOT_SUPP_WARNING_KEY, &notSuppWarning);

    SequenceNode credentialseq(nullptr);
    accountmap.setKeyValue(CRED_KEY, &credentialseq);

    for (const auto &it : credentials_) {
        std::map<std::string, std::string> cred = it;
        MappingNode *map = new MappingNode(nullptr);
        map->setKeyValue(CONFIG_ACCOUNT_USERNAME, new ScalarNode(cred[CONFIG_ACCOUNT_USERNAME]));
        map->setKeyValue(CONFIG_ACCOUNT_PASSWORD, new ScalarNode(cred[CONFIG_ACCOUNT_PASSWORD]));
        map->setKeyValue(CONFIG_ACCOUNT_REALM, new ScalarNode(cred[CONFIG_ACCOUNT_REALM]));
        credentialseq.addNode(map);
    }

    accountmap.setKeyValue(TLS_KEY, &tlsmap);
    tlsmap.setKeyValue(TLS_PORT_KEY, &tlsport);
    tlsmap.setKeyValue(CERTIFICATE_KEY, &certificate);
    tlsmap.setKeyValue(CALIST_KEY, &calist);
    tlsmap.setKeyValue(CIPHERS_KEY, &ciphersNode);
    tlsmap.setKeyValue(TLS_ENABLE_KEY, &tlsenabled);
    tlsmap.setKeyValue(METHOD_KEY, &tlsmethod);
    tlsmap.setKeyValue(TIMEOUT_KEY, &timeout);
    tlsmap.setKeyValue(TLS_PASSWORD_KEY, &tlspassword);
    tlsmap.setKeyValue(PRIVATE_KEY_KEY, &privatekey);
    tlsmap.setKeyValue(REQUIRE_CERTIF_KEY, &requirecertif);
    tlsmap.setKeyValue(SERVER_KEY, &server);
    tlsmap.setKeyValue(VERIFY_CLIENT_KEY, &verifyclient);
    tlsmap.setKeyValue(VERIFY_SERVER_KEY, &verifyserver);

    ScalarNode userAgent(userAgent_);
    accountmap.setKeyValue(USER_AGENT_KEY, &userAgent);

    ScalarNode hasCustomUserAgent(hasCustomUserAgent_);
    accountmap.setKeyValue(HAS_CUSTOM_USER_AGENT_KEY, &hasCustomUserAgent);

    auto audioPortNodes(serializeRange(accountmap, AUDIO_PORT_MIN_KEY, AUDIO_PORT_MAX_KEY, audioPortRange_));
#ifdef SFL_VIDEO
    auto videoPortNodes(serializeRange(accountmap, VIDEO_PORT_MIN_KEY, VIDEO_PORT_MAX_KEY, videoPortRange_));
#endif

    try {
        emitter.serializeAccount(&accountmap);
    } catch (const YamlEmitterException &e) {
        ERROR("%s", e.what());
    }

    // Cleanup
    Sequence *credSeq = credentialseq.getSequence();

    for (const auto &seqit : *credSeq) {
        MappingNode *node = static_cast<MappingNode*>(seqit);
        delete node->getValue(CONFIG_ACCOUNT_USERNAME);
        delete node->getValue(CONFIG_ACCOUNT_PASSWORD);
        delete node->getValue(CONFIG_ACCOUNT_REALM);
        delete node;
    }

#ifdef SFL_VIDEO
    Sequence *videoCodecSeq = videoCodecs.getSequence();

    for (auto &i : *videoCodecSeq) {
        MappingNode *node = static_cast<MappingNode*>(i);
        delete node->getValue(VIDEO_CODEC_NAME);
        delete node->getValue(VIDEO_CODEC_BITRATE);
        delete node->getValue(VIDEO_CODEC_ENABLED);
        delete node->getValue(VIDEO_CODEC_PARAMETERS);
        delete node;
    }

#endif
}

void SIPAccount::usePublishedAddressPortInVIA()
{
    via_addr_.host.ptr = (char *) publishedIpAddress_.c_str();
    via_addr_.host.slen = publishedIpAddress_.size();
    via_addr_.port = publishedPort_;
}

template <typename T>
static void
validate(std::string &member, const std::string &param, const T& valid)
{
    const auto begin = std::begin(valid);
    const auto end = std::end(valid);
    if (find(begin, end, param) != end)
        member = param;
    else
        ERROR("Invalid parameter \"%s\"", param.c_str());
}

void SIPAccount::unserialize(const Conf::YamlNode &mapNode)
{
    using namespace Conf;
    using std::vector;
    using std::map;
    using std::string;

    mapNode.getValue(ALIAS_KEY, &alias_);
    mapNode.getValue(USERNAME_KEY, &username_);

    if (not isIP2IP()) mapNode.getValue(HOSTNAME_KEY, &hostname_);

    mapNode.getValue(ACCOUNT_ENABLE_KEY, &enabled_);
    mapNode.getValue(ACCOUNT_AUTOANSWER_KEY, &autoAnswerEnabled_);

    if (not isIP2IP()) mapNode.getValue(MAILBOX_KEY, &mailBox_);

    mapNode.getValue(AUDIO_CODECS_KEY, &audioCodecStr_);
    // Update codec list which one is used for SDP offer
    setActiveAudioCodecs(split_string(audioCodecStr_));
#ifdef SFL_VIDEO
    YamlNode *videoCodecsNode(mapNode.getValue(VIDEO_CODECS_KEY));

    if (videoCodecsNode and videoCodecsNode->getType() == SEQUENCE) {
        SequenceNode *videoCodecs = static_cast<SequenceNode *>(videoCodecsNode);
        Sequence *seq = videoCodecs->getSequence();

        if (seq->empty()) {
            // Video codecs are an empty list
            WARN("Loading default video codecs");
            videoCodecList_ = libav_utils::getDefaultCodecs();
        } else {
            vector<map<string, string> > videoCodecDetails;

            for (const auto &it : *seq) {
                MappingNode *codec = static_cast<MappingNode *>(it);
                map<string, string> codecMap;
                codec->getValue(VIDEO_CODEC_NAME, &codecMap[VIDEO_CODEC_NAME]);
                codec->getValue(VIDEO_CODEC_BITRATE, &codecMap[VIDEO_CODEC_BITRATE]);
                codec->getValue(VIDEO_CODEC_ENABLED, &codecMap[VIDEO_CODEC_ENABLED]);
                codec->getValue(VIDEO_CODEC_PARAMETERS, &codecMap[VIDEO_CODEC_PARAMETERS]);
                videoCodecDetails.push_back(codecMap);
            }

            // these must be validated
            setVideoCodecs(videoCodecDetails);
        }
    } else {
        // either this is an older config file which had videoCodecs as a scalar node,
        // or it had no video codecs at all
        WARN("Loading default video codecs");
        videoCodecList_ = libav_utils::getDefaultCodecs();
    }

#endif

    mapNode.getValue(RINGTONE_PATH_KEY, &ringtonePath_);
    mapNode.getValue(RINGTONE_ENABLED_KEY, &ringtoneEnabled_);
    mapNode.getValue(VIDEO_ENABLED_KEY, &videoEnabled_);

    if (not isIP2IP()) mapNode.getValue(Preferences::REGISTRATION_EXPIRE_KEY, &registrationExpire_);

    mapNode.getValue(INTERFACE_KEY, &interface_);
    int port = DEFAULT_SIP_PORT;
    mapNode.getValue(PORT_KEY, &port);
    localPort_ = port;
    mapNode.getValue(PUBLISH_ADDR_KEY, &publishedIpAddress_);
    mapNode.getValue(PUBLISH_PORT_KEY, &port);
    publishedPort_ = port;
    mapNode.getValue(SAME_AS_LOCAL_KEY, &publishedSameasLocal_);

    if (not publishedSameasLocal_)
        usePublishedAddressPortInVIA();

    if (not isIP2IP()) mapNode.getValue(KEEP_ALIVE_ENABLED, &keepAliveEnabled_);

#ifdef SFL_PRESENCE
    std::string pres;
    mapNode.getValue(PRESENCE_ENABLED_KEY, &pres);
    enablePresence(pres == Conf::TRUE_STR);
    mapNode.getValue(PRESENCE_PUBLISH_SUPPORTED_KEY, &pres);
    if (presence_)
        presence_->support(PRESENCE_FUNCTION_PUBLISH, pres == Conf::TRUE_STR);
    mapNode.getValue(PRESENCE_SUBSCRIBE_SUPPORTED_KEY, &pres);
    if (presence_)
        presence_->support(PRESENCE_FUNCTION_SUBSCRIBE, pres == Conf::TRUE_STR);
#endif

    std::string dtmfType;
    mapNode.getValue(DTMF_TYPE_KEY, &dtmfType);
    dtmfType_ = dtmfType;

    if (not isIP2IP()) mapNode.getValue(SERVICE_ROUTE_KEY, &serviceRoute_);

    // stun enabled
    if (not isIP2IP()) mapNode.getValue(STUN_ENABLED_KEY, &stunEnabled_);

    if (not isIP2IP()) mapNode.getValue(STUN_SERVER_KEY, &stunServer_);

    // Init stun server name with default server name
    stunServerName_ = pj_str((char*) stunServer_.data());

    mapNode.getValue(DISPLAY_NAME_KEY, &displayName_);

    std::vector<std::map<std::string, std::string> > creds;

    YamlNode *credNode = mapNode.getValue(CRED_KEY);

    /* We check if the credential key is a sequence
     * because it was a mapping in a previous version of
     * the configuration file.
     */
    if (credNode && credNode->getType() == SEQUENCE) {
        SequenceNode *credSeq = static_cast<SequenceNode *>(credNode);
        Sequence *seq = credSeq->getSequence();

        for (const auto &it : *seq) {
            MappingNode *cred = static_cast<MappingNode *>(it);
            std::string user;
            std::string pass;
            std::string realm;
            cred->getValue(CONFIG_ACCOUNT_USERNAME, &user);
            cred->getValue(CONFIG_ACCOUNT_PASSWORD, &pass);
            cred->getValue(CONFIG_ACCOUNT_REALM, &realm);
            std::map<std::string, std::string> credentialMap;
            credentialMap[CONFIG_ACCOUNT_USERNAME] = user;
            credentialMap[CONFIG_ACCOUNT_PASSWORD] = pass;
            credentialMap[CONFIG_ACCOUNT_REALM] = realm;
            creds.push_back(credentialMap);
        }
    }

    if (creds.empty()) {
        // migration from old file format
        std::map<std::string, std::string> credmap;
        std::string password;

        if (not isIP2IP()) mapNode.getValue(PASSWORD_KEY, &password);

        credmap[CONFIG_ACCOUNT_USERNAME] = username_;
        credmap[CONFIG_ACCOUNT_PASSWORD] = password;
        credmap[CONFIG_ACCOUNT_REALM] = "*";
        creds.push_back(credmap);
    }

    setCredentials(creds);

    // get srtp submap
    MappingNode *srtpMap = static_cast<MappingNode *>(mapNode.getValue(SRTP_KEY));

    if (srtpMap) {
        srtpMap->getValue(SRTP_ENABLE_KEY, &srtpEnabled_);
        std::string tmp;
        srtpMap->getValue(KEY_EXCHANGE_KEY, &tmp);
        validate(srtpKeyExchange_, tmp, VALID_SRTP_KEY_EXCHANGES);
        srtpMap->getValue(RTP_FALLBACK_KEY, &srtpFallback_);
    }

    // get zrtp submap
    MappingNode *zrtpMap = static_cast<MappingNode *>(mapNode.getValue(ZRTP_KEY));

    if (zrtpMap) {
        zrtpMap->getValue(DISPLAY_SAS_KEY, &zrtpDisplaySas_);
        zrtpMap->getValue(DISPLAY_SAS_ONCE_KEY, &zrtpDisplaySasOnce_);
        zrtpMap->getValue(HELLO_HASH_ENABLED_KEY, &zrtpHelloHash_);
        zrtpMap->getValue(NOT_SUPP_WARNING_KEY, &zrtpNotSuppWarning_);
    }

    // get tls submap
    MappingNode *tlsMap = static_cast<MappingNode *>(mapNode.getValue(TLS_KEY));

    if (tlsMap) {
        tlsMap->getValue(TLS_ENABLE_KEY, &tlsEnable_);
        std::string tlsPort;
        tlsMap->getValue(TLS_PORT_KEY, &tlsPort);
        tlsListenerPort_ = atoi(tlsPort.c_str());
        tlsMap->getValue(CERTIFICATE_KEY, &tlsCertificateFile_);
        tlsMap->getValue(CALIST_KEY, &tlsCaListFile_);
        tlsMap->getValue(CIPHERS_KEY, &tlsCiphers_);

        std::string tmp(tlsMethod_);
        tlsMap->getValue(METHOD_KEY, &tmp);
        validate(tlsMethod_, tmp, VALID_TLS_METHODS);

        tlsMap->getValue(TLS_PASSWORD_KEY, &tlsPassword_);
        tlsMap->getValue(PRIVATE_KEY_KEY, &tlsPrivateKeyFile_);
        tlsMap->getValue(REQUIRE_CERTIF_KEY, &tlsRequireClientCertificate_);
        tlsMap->getValue(SERVER_KEY, &tlsServerName_);
        tlsMap->getValue(VERIFY_CLIENT_KEY, &tlsVerifyClient_);
        tlsMap->getValue(VERIFY_SERVER_KEY, &tlsVerifyServer_);
        // FIXME
        tlsMap->getValue(TIMEOUT_KEY, &tlsNegotiationTimeoutSec_);
    }
    mapNode.getValue(USER_AGENT_KEY, &userAgent_);
    mapNode.getValue(HAS_CUSTOM_USER_AGENT_KEY, &userAgent_);

    unserializeRange(mapNode, AUDIO_PORT_MIN_KEY, AUDIO_PORT_MAX_KEY, audioPortRange_);
#ifdef SFL_VIDEO
    unserializeRange(mapNode, VIDEO_PORT_MIN_KEY, VIDEO_PORT_MAX_KEY, videoPortRange_);
#endif
}

template <typename T>
static void
parseInt(const std::map<std::string, std::string> &details, const char *key, T &i)
{
    const auto iter = details.find(key);
    if (iter == details.end()) {
        ERROR("Couldn't find key %s", key);
        return;
    }
    i = atoi(iter->second.c_str());
}

void SIPAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    // Account setting common to SIP and IAX
    parseString(details, CONFIG_ACCOUNT_ALIAS, alias_);
    parseString(details, CONFIG_ACCOUNT_USERNAME, username_);
    parseString(details, CONFIG_ACCOUNT_HOSTNAME, hostname_);
    parseBool(details, CONFIG_ACCOUNT_ENABLE, enabled_);
    parseBool(details, CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled_);
    parseString(details, CONFIG_RINGTONE_PATH, ringtonePath_);
    parseBool(details, CONFIG_RINGTONE_ENABLED, ringtoneEnabled_);
    parseBool(details, CONFIG_VIDEO_ENABLED, videoEnabled_);
    parseString(details, CONFIG_ACCOUNT_MAILBOX, mailBox_);

    // SIP specific account settings

    // general sip settings
    parseString(details, CONFIG_ACCOUNT_ROUTESET, serviceRoute_);
    parseString(details, CONFIG_LOCAL_INTERFACE, interface_);
    parseBool(details, CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_);
    parseString(details, CONFIG_PUBLISHED_ADDRESS, publishedIpAddress_);
    parseInt(details, CONFIG_LOCAL_PORT, localPort_);
    parseInt(details, CONFIG_PUBLISHED_PORT, publishedPort_);

    if (not publishedSameasLocal_)
        usePublishedAddressPortInVIA();

    parseString(details, CONFIG_STUN_SERVER, stunServer_);
    parseBool(details, CONFIG_STUN_ENABLE, stunEnabled_);
    parseString(details, CONFIG_ACCOUNT_DTMF_TYPE, dtmfType_);
    parseInt(details, CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire_);

    if (registrationExpire_ < MIN_REGISTRATION_TIME)
        registrationExpire_ = MIN_REGISTRATION_TIME;

    parseBool(details, CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT, hasCustomUserAgent_);
    if (hasCustomUserAgent_)
        parseString(details, CONFIG_ACCOUNT_USERAGENT, userAgent_);
    else
        userAgent_ = DEFAULT_USER_AGENT;

    parseBool(details, CONFIG_KEEP_ALIVE_ENABLED, keepAliveEnabled_);
#ifdef SFL_PRESENCE
    bool presenceEnabled = false;
    parseBool(details, CONFIG_PRESENCE_ENABLED, presenceEnabled);
    enablePresence(presenceEnabled);
#endif

    int tmpMin = -1;
    parseInt(details, CONFIG_ACCOUNT_AUDIO_PORT_MIN, tmpMin);
    int tmpMax = -1;
    parseInt(details, CONFIG_ACCOUNT_AUDIO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, audioPortRange_);
#ifdef SFL_VIDEO
    tmpMin = -1;
    parseInt(details, CONFIG_ACCOUNT_VIDEO_PORT_MIN, tmpMin);
    tmpMax = -1;
    parseInt(details, CONFIG_ACCOUNT_VIDEO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, videoPortRange_);
#endif

    // srtp settings
    parseBool(details, CONFIG_SRTP_ENABLE, srtpEnabled_);
    parseBool(details, CONFIG_SRTP_RTP_FALLBACK, srtpFallback_);
    parseBool(details, CONFIG_ZRTP_DISPLAY_SAS, zrtpDisplaySas_);
    parseBool(details, CONFIG_ZRTP_DISPLAY_SAS_ONCE, zrtpDisplaySasOnce_);
    parseBool(details, CONFIG_ZRTP_NOT_SUPP_WARNING, zrtpNotSuppWarning_);
    parseBool(details, CONFIG_ZRTP_HELLO_HASH, zrtpHelloHash_);
    auto iter = details.find(CONFIG_SRTP_KEY_EXCHANGE);
    if (iter != details.end())
        validate(srtpKeyExchange_, iter->second, VALID_SRTP_KEY_EXCHANGES);

    // TLS settings
    parseBool(details, CONFIG_TLS_ENABLE, tlsEnable_);
    parseInt(details, CONFIG_TLS_LISTENER_PORT, tlsListenerPort_);
    parseString(details, CONFIG_TLS_CA_LIST_FILE, tlsCaListFile_);
    parseString(details, CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_);

    parseString(details, CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_);
    parseString(details, CONFIG_TLS_PASSWORD, tlsPassword_);
    iter = details.find(CONFIG_TLS_METHOD);
    if (iter != details.end())
        validate(tlsMethod_, iter->second, VALID_TLS_METHODS);
    parseString(details, CONFIG_TLS_CIPHERS, tlsCiphers_);
    parseString(details, CONFIG_TLS_SERVER_NAME, tlsServerName_);
    parseBool(details, CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer_);
    parseBool(details, CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient_);
    parseBool(details, CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    parseString(details, CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);

    if (credentials_.empty()) { // credentials not set, construct 1 entry
        WARN("No credentials set, inferring them...");
        std::vector<std::map<std::string, std::string> > v;
        std::map<std::string, std::string> map;
        map[CONFIG_ACCOUNT_USERNAME] = username_;
        parseString(details, CONFIG_ACCOUNT_PASSWORD, map[CONFIG_ACCOUNT_PASSWORD]);
        map[CONFIG_ACCOUNT_REALM] = "*";
        v.push_back(map);
        setCredentials(v);
    }
}

static std::string retrievePassword(const std::map<std::string, std::string>& map, const std::string &username)
{
    std::map<std::string, std::string>::const_iterator map_iter_username;
    std::map<std::string, std::string>::const_iterator map_iter_password;
    map_iter_username = map.find(CONFIG_ACCOUNT_USERNAME);

    if (map_iter_username != map.end()) {
        if (map_iter_username->second == username) {
            map_iter_password = map.find(CONFIG_ACCOUNT_PASSWORD);

            if (map_iter_password != map.end()) {
                return map_iter_password->second;
            }
        }
    }

    return "";
}

void
addRangeToDetails(std::map<std::string, std::string> &a, const char *minKey, const char *maxKey, const std::pair<uint16_t, uint16_t> &range)
{
    std::ostringstream os;
    os << range.first;
    a[minKey] = os.str();
    os.str("");
    os << range.second;
    a[maxKey] = os.str();
}

std::map<std::string, std::string> SIPAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    // note: The IP2IP profile will always have IP2IP as an alias
    a[CONFIG_ACCOUNT_ALIAS] = alias_;

    a[CONFIG_ACCOUNT_ENABLE] = enabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_ACCOUNT_AUTOANSWER] = autoAnswerEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_ACCOUNT_TYPE] = ACCOUNT_TYPE;
    a[CONFIG_ACCOUNT_HOSTNAME] = hostname_;
    a[CONFIG_ACCOUNT_USERNAME] = username_;
    // get password for this username
    a[CONFIG_ACCOUNT_PASSWORD] = "";

    if (hasCredentials()) {

        for (const auto &vect_item : credentials_) {
            const std::string password = retrievePassword(vect_item, username_);

            if (not password.empty())
                a[CONFIG_ACCOUNT_PASSWORD] = password;
        }
    }

    a[CONFIG_RINGTONE_PATH] = ringtonePath_;
    a[CONFIG_RINGTONE_ENABLED] = ringtoneEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_VIDEO_ENABLED] = videoEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_ACCOUNT_MAILBOX] = mailBox_;
#ifdef SFL_PRESENCE
    a[CONFIG_PRESENCE_ENABLED] = presence_ and presence_->isEnabled()? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_PRESENCE_PUBLISH_SUPPORTED] = presence_ and presence_->isSupported(PRESENCE_FUNCTION_PUBLISH)? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED] = presence_ and presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE)? Conf::TRUE_STR : Conf::FALSE_STR;
    // initialize status values
    a[CONFIG_PRESENCE_STATUS] = presence_ and presence_->isOnline()? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_PRESENCE_NOTE] = presence_ ? presence_->getNote() : " ";
#endif

    RegistrationState state = RegistrationState::UNREGISTERED;
    std::string registrationStateCode;
    std::string registrationStateDescription;

    if (isIP2IP())
        registrationStateDescription = "Direct IP call";
    else {
        state = registrationState_;
        int code = registrationStateDetailed_.first;
        std::stringstream out;
        out << code;
        registrationStateCode = out.str();
        registrationStateDescription = registrationStateDetailed_.second;
    }

    a[CONFIG_ACCOUNT_REGISTRATION_STATUS] = isIP2IP() ? "READY" : mapStateNumberToString(state);
    a[CONFIG_ACCOUNT_REGISTRATION_STATE_CODE] = registrationStateCode;
    a[CONFIG_ACCOUNT_REGISTRATION_STATE_DESC] = registrationStateDescription;

    // Add sip specific details
    a[CONFIG_ACCOUNT_ROUTESET] = serviceRoute_;
    a[CONFIG_ACCOUNT_USERAGENT] = hasCustomUserAgent_ ? userAgent_ : DEFAULT_USER_AGENT;
    a[CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT] = hasCustomUserAgent_ ? Conf::TRUE_STR : Conf::FALSE_STR;

    addRangeToDetails(a, CONFIG_ACCOUNT_AUDIO_PORT_MIN, CONFIG_ACCOUNT_AUDIO_PORT_MAX, audioPortRange_);
#ifdef SFL_VIDEO
    addRangeToDetails(a, CONFIG_ACCOUNT_VIDEO_PORT_MIN, CONFIG_ACCOUNT_VIDEO_PORT_MAX, videoPortRange_);
#endif

    std::stringstream registrationExpireStr;
    registrationExpireStr << registrationExpire_;
    a[CONFIG_ACCOUNT_REGISTRATION_EXPIRE] = registrationExpireStr.str();
    a[CONFIG_LOCAL_INTERFACE] = interface_;
    a[CONFIG_PUBLISHED_SAMEAS_LOCAL] = publishedSameasLocal_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_PUBLISHED_ADDRESS] = publishedIpAddress_;

    std::stringstream localport;
    localport << localPort_;
    a[CONFIG_LOCAL_PORT] = localport.str();
    std::stringstream publishedport;
    publishedport << publishedPort_;
    a[CONFIG_PUBLISHED_PORT] = publishedport.str();
    a[CONFIG_STUN_ENABLE] = stunEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_STUN_SERVER] = stunServer_;
    a[CONFIG_ACCOUNT_DTMF_TYPE] = dtmfType_;
    a[CONFIG_KEEP_ALIVE_ENABLED] = keepAliveEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;

    a[CONFIG_SRTP_KEY_EXCHANGE] = srtpKeyExchange_;
    a[CONFIG_SRTP_ENABLE] = srtpEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_SRTP_RTP_FALLBACK] = srtpFallback_ ? Conf::TRUE_STR : Conf::FALSE_STR;

    a[CONFIG_ZRTP_DISPLAY_SAS] = zrtpDisplaySas_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_ZRTP_DISPLAY_SAS_ONCE] = zrtpDisplaySasOnce_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_ZRTP_HELLO_HASH] = zrtpHelloHash_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_ZRTP_NOT_SUPP_WARNING] = zrtpNotSuppWarning_ ? Conf::TRUE_STR : Conf::FALSE_STR;

    // TLS listener is unique and parameters are modified through IP2IP_PROFILE
    std::stringstream tlslistenerport;
    tlslistenerport << tlsListenerPort_;
    a[CONFIG_TLS_LISTENER_PORT] = tlslistenerport.str();
    a[CONFIG_TLS_ENABLE] = tlsEnable_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_TLS_CA_LIST_FILE] = tlsCaListFile_;
    a[CONFIG_TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    a[CONFIG_TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    a[CONFIG_TLS_PASSWORD] = tlsPassword_;
    a[CONFIG_TLS_METHOD] = tlsMethod_;
    a[CONFIG_TLS_CIPHERS] = tlsCiphers_;
    a[CONFIG_TLS_SERVER_NAME] = tlsServerName_;
    a[CONFIG_TLS_VERIFY_SERVER] = tlsVerifyServer_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_TLS_VERIFY_CLIENT] = tlsVerifyClient_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE] = tlsRequireClientCertificate_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    a[CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC] = tlsNegotiationTimeoutSec_;

    return a;
}

void SIPAccount::registerVoIPLink()
{
    if (hostname_.length() >= PJ_MAX_HOSTNAME)
        return;

    DEBUG("SIPAccount::registerVoIPLink %s ", hostname_.c_str());

    auto IPs = ip_utils::getAddrList(hostname_);
    for (const auto& ip : IPs)
        DEBUG("--- %s ", ip.toString().c_str());

#if HAVE_TLS
    // Init TLS settings if the user wants to use TLS
    if (tlsEnable_) {
        DEBUG("TLS is enabled for account %s", accountID_.c_str());

        // Dropping current calls already using the transport is currently required
        // with TLS.
        Manager::instance().freeAccount(accountID_);

        // PJSIP does not currently support TLS over IPv6
        transportType_ = PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();
    } else
#endif
    {
        bool IPv6 = false;
#if HAVE_IPV6
        if (isIP2IP()) {
            DEBUG("SIPAccount::registerVoIPLink isIP2IP.");
            IPv6 = ip_utils::getInterfaceAddr(interface_).isIpv6();
        } else if (!IPs.empty())
            IPv6 = IPs[0].isIpv6();
#endif
        transportType_ = IPv6 ? PJSIP_TRANSPORT_UDP6  : PJSIP_TRANSPORT_UDP;
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
    if (isIP2IP())
        return;

    try {
        link_.sendRegister(*this);
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }

#ifdef SFL_PRESENCE
    if (presence_ and presence_->isEnabled()) {
        presence_->subscribeClient(getFromUri(), true); // self presence subscription
        presence_->sendPresence(true, ""); // try to publish whatever the status is.
    }
#endif
}

void SIPAccount::unregisterVoIPLink(std::function<void(bool)> released_cb)
{
    if (isIP2IP()) {
        if (released_cb)
            released_cb(false);
        return;
    }

    try {
        link_.sendUnregister(*this, released_cb);
    } catch (const VoipLinkException &e) {
        ERROR("SIPAccount::unregisterVoIPLink %s", e.what());
        setTransport();
        if (released_cb)
            released_cb(false);
    }
}

void SIPAccount::startKeepAliveTimer()
{

    if (isTlsEnabled())
        return;

    if (isIP2IP())
        return;

    if (keepAliveTimerActive_)
        return;

    DEBUG("Start keep alive timer for account %s", getAccountID().c_str());

    // make sure here we have an entirely new timer
    memset(&keepAliveTimer_, 0, sizeof(pj_timer_entry));

    pj_time_val keepAliveDelay_;
    keepAliveTimer_.cb = &SIPAccount::keepAliveRegistrationCb;
    keepAliveTimer_.user_data = this;
    keepAliveTimer_.id = rand();

    // expiration may be undetermined during the first registration request
    if (registrationExpire_ == 0) {
        DEBUG("Registration Expire: 0, taking 60 instead");
        keepAliveDelay_.sec = 3600;
    } else {
        DEBUG("Registration Expire: %d", registrationExpire_);
        keepAliveDelay_.sec = registrationExpire_ + MIN_REGISTRATION_TIME;
    }

    keepAliveDelay_.msec = 0;

    keepAliveTimerActive_ = true;

    link_.registerKeepAliveTimer(keepAliveTimer_, keepAliveDelay_);
}

void SIPAccount::stopKeepAliveTimer()
{
    if (keepAliveTimerActive_) {
        DEBUG("Stop keep alive timer %d for account %s", keepAliveTimer_.id, getAccountID().c_str());
        keepAliveTimerActive_ = false;
        link_.cancelKeepAliveTimer(keepAliveTimer_);
    }
}

#if HAVE_TLS
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

void SIPAccount::trimCiphers()
{
    int sum = 0;
    int count = 0;

    // PJSIP aborts if our cipher list exceeds 1000 characters
    static const int MAX_CIPHERS_STRLEN = 1000;

    for (const auto &item : ciphers_) {
        sum += strlen(pj_ssl_cipher_name(item));

        if (sum > MAX_CIPHERS_STRLEN)
            break;

        ++count;
    }

    ciphers_.resize(count);
    DEBUG("Using %u ciphers", ciphers_.size());
}

void SIPAccount::initTlsConfiguration()
{
    unsigned cipherNum;

    // Determine the cipher list supported on this machine
    cipherNum = ciphers_.size();

    if (pj_ssl_cipher_get_availables(&ciphers_.front(), &cipherNum) != PJ_SUCCESS)
        ERROR("Could not determine cipher list on this system");

    ciphers_.resize(cipherNum);

    trimCiphers();

    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    pjsip_tls_setting_default(&tlsSetting_);

    pj_cstr(&tlsSetting_.ca_list_file, tlsCaListFile_.c_str());
    pj_cstr(&tlsSetting_.cert_file, tlsCertificateFile_.c_str());
    pj_cstr(&tlsSetting_.privkey_file, tlsPrivateKeyFile_.c_str());
    pj_cstr(&tlsSetting_.password, tlsPassword_.c_str());
    tlsSetting_.method = sslMethodStringToPjEnum(tlsMethod_);
    tlsSetting_.ciphers_num = ciphers_.size();
    tlsSetting_.ciphers = &ciphers_.front();

    tlsSetting_.verify_server = tlsVerifyServer_;
    tlsSetting_.verify_client = tlsVerifyClient_;
    tlsSetting_.require_client_cert = tlsRequireClientCertificate_;

    tlsSetting_.timeout.sec = atol(tlsNegotiationTimeoutSec_.c_str());

    tlsSetting_.qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    tlsSetting_.qos_ignore_error = PJ_TRUE;
}

#endif

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
        registrationExpire_ = DEFAULT_REGISTRATION_TIME; /** Default expire value for registration */

#if HAVE_TLS

    if (tlsEnable_) {
        initTlsConfiguration();
        transportType_ = PJSIP_TRANSPORT_TLS;
    } else
#endif
        transportType_ = PJSIP_TRANSPORT_UDP;
}

bool SIPAccount::fullMatch(const std::string& username, const std::string& hostname, pjsip_endpoint *endpt, pj_pool_t *pool) const
{
    return userMatch(username) and hostnameMatch(hostname, endpt, pool);
}

bool SIPAccount::userMatch(const std::string& username) const
{
    return !username.empty() and username == username_;
}

bool SIPAccount::hostnameMatch(const std::string& hostname, pjsip_endpoint * /*endpt*/, pj_pool_t * /*pool*/) const
{
    if (hostname == hostname_)
        return true;
    const auto a = ip_utils::getAddrList(hostname);
    const auto b = ip_utils::getAddrList(hostname_);
    return ip_utils::haveCommonAddr(a, b);
}

bool SIPAccount::proxyMatch(const std::string& hostname, pjsip_endpoint * /*endpt*/, pj_pool_t * /*pool*/) const
{
    if (hostname == serviceRoute_)
        return true;
    const auto a = ip_utils::getAddrList(hostname);
    const auto b = ip_utils::getAddrList(hostname_);
    return ip_utils::haveCommonAddr(a, b);
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
    if (transportType_ == PJSIP_TRANSPORT_TLS || transportType_ == PJSIP_TRANSPORT_TLS6) {
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

#if HAVE_IPV6
    if (IpAddr::isIpv6(hostname))
        hostname = IpAddr(hostname).toString(false, true);
#endif

    return "<" + scheme + username + "@" + hostname + transport + ">";
}

std::string SIPAccount::getToUri(const std::string& username) const
{
    std::string scheme;
    std::string transport;
    std::string hostname;

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS || transportType_ == PJSIP_TRANSPORT_TLS6) {
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

#if HAVE_IPV6
    if (not hostname.empty() and IpAddr::isIpv6(hostname))
        hostname = IpAddr(hostname).toString(false, true);
#endif

    return "<" + scheme + username + (hostname.empty() ? "" : "@") + hostname + transport + ">";
}

std::string SIPAccount::getServerUri() const
{
    std::string scheme;
    std::string transport;

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS || transportType_ == PJSIP_TRANSPORT_TLS6) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else {
        scheme = "sip:";
    }

    std::string host;
#if HAVE_IPV6
    if (IpAddr::isIpv6(hostname_))
        host = IpAddr(hostname_).toString(false, true);
    else
#endif
        host = hostname_;

    return "<" + scheme + host + transport + ">";
}


pj_str_t
SIPAccount::getContactHeader()
{
    if (transport_ == nullptr)
        ERROR("Transport not created yet");

    if (contact_.slen and contactOverwritten_)
        return contact_;

    // The transport type must be specified, in our case START_OTHER refers to stun transport
    pjsip_transport_type_e transportType = transportType_;

    if (transportType == PJSIP_TRANSPORT_START_OTHER)
        transportType = PJSIP_TRANSPORT_UDP;

    // Else we determine this infor based on transport information
    std::string address;
    pj_uint16_t port;

    link_.sipTransport->findLocalAddressFromTransport(transport_, transportType, hostname_, address, port);

    if (not publishedSameasLocal_) {
        address = publishedIpAddress_;
        port = publishedPort_;
        DEBUG("Using published address %s and port %d", address.c_str(), port);
    } else if (stunEnabled_) {
        link_.sipTransport->findLocalAddressFromSTUN(transport_, &stunServerName_, stunPort_, address, port);
        setPublishedAddress(address);
        publishedPort_ = port;
        usePublishedAddressPortInVIA();
    } else {
        if (!receivedParameter_.empty()) {
            address = receivedParameter_;
            DEBUG("Using received address %s", address.c_str());
        }

        if (rPort_ != -1 and rPort_ != 0) {
            port = rPort_;
            DEBUG("Using received port %d", port);
        }
    }

    // UDP does not require the transport specification
    std::string scheme;
    std::string transport;

#if HAVE_IPV6
    /* Enclose IPv6 address in square brackets */
    if (IpAddr::isIpv6(address)) {
        address = IpAddr(address).toString(false, true);
    }
#endif

    if (transportType != PJSIP_TRANSPORT_UDP and transportType != PJSIP_TRANSPORT_UDP6) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType));
    } else
        scheme = "sip:";

    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "%s%s<%s%s%s%s:%d%s>",
                                     displayName_.c_str(),
                                     (displayName_.empty() ? "" : " "),
                                     scheme.c_str(),
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.c_str(),
                                     port,
                                     transport.c_str());
    return contact_;
}

pjsip_host_port
SIPAccount::getHostPortFromSTUN(pj_pool_t *pool)
{
    std::string addr;
    pj_uint16_t port;
    link_.sipTransport->findLocalAddressFromSTUN(transport_, &stunServerName_, stunPort_, addr, port);
    pjsip_host_port result;
    pj_strdup2(pool, &result.host, addr.c_str());
    result.host.slen = addr.length();
    result.port = port;
    return result;
}

void SIPAccount::keepAliveRegistrationCb(UNUSED pj_timer_heap_t *th, pj_timer_entry *te)
{
    SIPAccount *sipAccount = static_cast<SIPAccount *>(te->user_data);

    if (sipAccount == nullptr) {
        ERROR("SIP account is nullptr while registering a new keep alive timer");
        return;
    }

    ERROR("Keep alive registration callback for account %s", sipAccount->getAccountID().c_str());

    // IP2IP default does not require keep-alive
    if (sipAccount->isIP2IP())
        return;

    // TLS is connection oriented and does not require keep-alive
    if (sipAccount->isTlsEnabled())
        return;

    sipAccount->stopKeepAliveTimer();

    if (sipAccount->isRegistered())
        sipAccount->registerVoIPLink();
}

static std::string
computeMd5HashFromCredential(const std::string& username,
                             const std::string& password,
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
#undef MD5_APPEND

    unsigned char digest[16];
    pj_md5_final(&pms, digest);

    char hash[32];

    for (int i = 0; i < 16; ++i)
        pj_val_to_hex_digit(digest[i], &hash[2 * i]);

    return std::string(hash, 32);
}

void
SIPAccount::setTransport(pjsip_transport* transport, pjsip_tpfactory* lis)
{
    // release old transport
    if (transport_ && transport_ != transport) {
        if (regc_)
            pjsip_regc_release_transport(regc_);
        pjsip_transport_dec_ref(transport_);
    }
    if (tlsListener_ && tlsListener_ != lis)
        tlsListener_->destroy(tlsListener_);
    // set new transport
    transport_ = transport;
    tlsListener_ = lis;
}

void SIPAccount::setCredentials(const std::vector<std::map<std::string, std::string> >& creds)
{
    // we can not authenticate without credentials
    if (creds.empty()) {
        ERROR("Cannot authenticate with empty credentials list");
        return;
    }

    using std::vector;
    using std::string;
    using std::map;

    bool md5HashingEnabled = Manager::instance().preferences.getMd5Hash();

    credentials_ = creds;

    /* md5 hashing */
    for (auto &it : credentials_) {
        map<string, string>::const_iterator val = it.find(CONFIG_ACCOUNT_USERNAME);
        const std::string username = val != it.end() ? val->second : "";
        val = it.find(CONFIG_ACCOUNT_REALM);
        const std::string realm(val != it.end() ? val->second : "");
        val = it.find(CONFIG_ACCOUNT_PASSWORD);
        const std::string password(val != it.end() ? val->second : "");

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
                it[CONFIG_ACCOUNT_PASSWORD] = computeMd5HashFromCredential(username, password, realm);
        }
    }

    // Create the credential array
    cred_.resize(credentials_.size());

    size_t i = 0;

    for (const auto &item : credentials_) {
        map<string, string>::const_iterator val = item.find(CONFIG_ACCOUNT_PASSWORD);
        const std::string password = val != item.end() ? val->second : "";
        int dataType = (md5HashingEnabled and password.length() == 32)
                       ? PJSIP_CRED_DATA_DIGEST
                       : PJSIP_CRED_DATA_PLAIN_PASSWD;

        val = item.find(CONFIG_ACCOUNT_USERNAME);

        if (val != item.end())
            cred_[i].username = pj_str((char*) val->second.c_str());

        cred_[i].data = pj_str((char*) password.c_str());

        val = item.find(CONFIG_ACCOUNT_REALM);

        if (val != item.end())
            cred_[i].realm = pj_str((char*) val->second.c_str());

        cred_[i].data_type = dataType;
        cred_[i].scheme = pj_str((char*) "digest");
        ++i;
    }
}

const std::vector<std::map<std::string, std::string> > &
SIPAccount::getCredentials() const
{
    return credentials_;
}

std::string SIPAccount::getUserAgentName() const
{
    if (not hasCustomUserAgent_ or userAgent_.empty())
        return DEFAULT_USER_AGENT;
    return userAgent_;
}

std::map<std::string, std::string> SIPAccount::getIp2IpDetails() const
{
    assert(isIP2IP());
    std::map<std::string, std::string> ip2ipAccountDetails;
    ip2ipAccountDetails[CONFIG_SRTP_KEY_EXCHANGE] = srtpKeyExchange_;
    ip2ipAccountDetails[CONFIG_SRTP_ENABLE] = srtpEnabled_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    ip2ipAccountDetails[CONFIG_SRTP_RTP_FALLBACK] = srtpFallback_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_DISPLAY_SAS] = zrtpDisplaySas_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_HELLO_HASH] = zrtpHelloHash_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_NOT_SUPP_WARNING] = zrtpNotSuppWarning_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_DISPLAY_SAS_ONCE] = zrtpDisplaySasOnce_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    ip2ipAccountDetails[CONFIG_LOCAL_INTERFACE] = interface_;
    std::stringstream portstr;
    portstr << localPort_;
    ip2ipAccountDetails[CONFIG_LOCAL_PORT] = portstr.str();

    std::map<std::string, std::string> tlsSettings(getTlsSettings());
    std::copy(tlsSettings.begin(), tlsSettings.end(), std::inserter(
                  ip2ipAccountDetails, ip2ipAccountDetails.end()));

    return ip2ipAccountDetails;
}

std::map<std::string, std::string> SIPAccount::getTlsSettings() const
{
    assert(isIP2IP());
    std::map<std::string, std::string> tlsSettings;

    std::stringstream portstr;
    portstr << tlsListenerPort_;
    tlsSettings[CONFIG_TLS_LISTENER_PORT] = portstr.str();
    tlsSettings[CONFIG_TLS_ENABLE] = tlsEnable_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    tlsSettings[CONFIG_TLS_CA_LIST_FILE] = tlsCaListFile_;
    tlsSettings[CONFIG_TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    tlsSettings[CONFIG_TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    tlsSettings[CONFIG_TLS_PASSWORD] = tlsPassword_;
    tlsSettings[CONFIG_TLS_METHOD] = tlsMethod_;
    tlsSettings[CONFIG_TLS_CIPHERS] = tlsCiphers_;
    tlsSettings[CONFIG_TLS_SERVER_NAME] = tlsServerName_;
    tlsSettings[CONFIG_TLS_VERIFY_SERVER] = tlsVerifyServer_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    tlsSettings[CONFIG_TLS_VERIFY_CLIENT] = tlsVerifyClient_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    tlsSettings[CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE] = tlsRequireClientCertificate_ ? Conf::TRUE_STR : Conf::FALSE_STR;
    tlsSettings[CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC] = tlsNegotiationTimeoutSec_;

    return tlsSettings;
}

static void
set_opt(const std::map<std::string, std::string> &details, const char *key, std::string &val)
{
    std::map<std::string, std::string>::const_iterator it = details.find(key);

    if (it != details.end())
        val = it->second;
}

static void
set_opt(const std::map<std::string, std::string> &details, const char *key, bool &val)
{
    std::map<std::string, std::string>::const_iterator it = details.find(key);

    if (it != details.end())
        val = it->second == Conf::TRUE_STR;
}

static void
set_opt(const std::map<std::string, std::string> &details, const char *key, pj_uint16_t &val)
{
    std::map<std::string, std::string>::const_iterator it = details.find(key);

    if (it != details.end())
        val = atoi(it->second.c_str());
}

void SIPAccount::setTlsSettings(const std::map<std::string, std::string>& details)
{
    assert(isIP2IP());
    set_opt(details, CONFIG_TLS_LISTENER_PORT, tlsListenerPort_);
    set_opt(details, CONFIG_TLS_ENABLE, tlsEnable_);
    set_opt(details, CONFIG_TLS_CA_LIST_FILE, tlsCaListFile_);
    set_opt(details, CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_);
    set_opt(details, CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_);
    set_opt(details, CONFIG_TLS_PASSWORD, tlsPassword_);
    set_opt(details, CONFIG_TLS_METHOD, tlsMethod_);
    set_opt(details, CONFIG_TLS_CIPHERS, tlsCiphers_);
    set_opt(details, CONFIG_TLS_SERVER_NAME, tlsServerName_);
    set_opt(details, CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient_);
    set_opt(details, CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    set_opt(details, CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);
}

VoIPLink* SIPAccount::getVoIPLink()
{
    return &link_;
}

bool SIPAccount::isIP2IP() const
{
    return accountID_ == IP2IP_PROFILE;
}

#ifdef SFL_PRESENCE
SIPPresence * SIPAccount::getPresence() const
{
    return presence_;
}

/**
 *  Enable the presence module
 */
void
SIPAccount::enablePresence(const bool& enabled)
{
    if (!presence_) {
        ERROR("Presence not initialized");
        return;
    }

    DEBUG("Presence enabled for %s : %s.",
          accountID_.c_str(),
          enabled? Conf::TRUE_STR : Conf::FALSE_STR);

    presence_->enable(enabled);
}

/**
 *  Set the presence (PUBLISH/SUBSCRIBE) support flags
 *  and process the change.
 */
void
SIPAccount::supportPresence(int function, bool enabled)
{
    if (!presence_) {
        ERROR("Presence not initialized");
        return;
    }

    if (presence_->isSupported(function) == enabled)
        return;

    DEBUG("Presence support for %s (%s: %s).", accountID_.c_str(),
          function == PRESENCE_FUNCTION_PUBLISH ? "publish" : "subscribe",
          enabled ? Conf::TRUE_STR : Conf::FALSE_STR);
    presence_->support(function, enabled);

    // force presence to disable when nothing is supported
    if (not presence_->isSupported(PRESENCE_FUNCTION_PUBLISH) and
        not presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE))
        enablePresence(false);

    Manager::instance().saveConfig();
    Manager::instance().getClient()->getConfigurationManager()->accountsChanged();
}
#endif

MatchRank
SIPAccount::matches(const std::string &userName, const std::string &server,
                    pjsip_endpoint *endpt, pj_pool_t *pool) const
{
    if (fullMatch(userName, server, endpt, pool)) {
        DEBUG("Matching account id in request is a fullmatch %s@%s", userName.c_str(), server.c_str());
        return MatchRank::FULL;
    } else if (hostnameMatch(server, endpt, pool)) {
        DEBUG("Matching account id in request with hostname %s", server.c_str());
        return MatchRank::PARTIAL;
    } else if (userMatch(userName)) {
        DEBUG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::PARTIAL;
    } else if (proxyMatch(server, endpt, pool)) {
        DEBUG("Matching account id in request with proxy %s", server.c_str());
        return MatchRank::PARTIAL;
    } else {
        return MatchRank::NONE;
    }
}

// returns even number in range [lower, upper]
uint16_t
SIPAccount::getRandomEvenNumber(const std::pair<uint16_t, uint16_t> &range)
{
    const uint16_t halfUpper = range.second * 0.5;
    const uint16_t halfLower = range.first * 0.5;
    uint16_t result;
    do {
        result = 2 * (halfLower + rand() % (halfUpper - halfLower + 1));
    } while (portsInUse_[result / 2]);

    portsInUse_[result / 2] = true;
    return result;
}

void
SIPAccount::releasePort(uint16_t port)
{
    portsInUse_[port / 2] = false;
}

uint16_t
SIPAccount::generateAudioPort() const
{
    return getRandomEvenNumber(audioPortRange_);
}

#ifdef SFL_VIDEO
uint16_t
SIPAccount::generateVideoPort() const
{
    return getRandomEvenNumber(videoPortRange_);
}
#endif

void
SIPAccount::destroyRegistrationInfo()
{
    if (!regc_) return;
    pjsip_regc_destroy(regc_);
    regc_ = nullptr;
}

void
SIPAccount::resetAutoRegistration()
{
    auto_rereg_.active = PJ_FALSE;
    auto_rereg_.attempt_cnt = 0;
}

/* Update NAT address from the REGISTER response */
bool
SIPAccount::checkNATAddress(pjsip_regc_cbparam *param, pj_pool_t *pool)
{
    pjsip_transport *tp = param->rdata->tp_info.transport;

    /* Get the received and rport info */
    pjsip_via_hdr *via = param->rdata->msg_info.via;
    int rport;
    if (via->rport_param < 1) {
        /* Remote doesn't support rport */
        rport = via->sent_by.port;
        if (rport == 0) {
            pjsip_transport_type_e tp_type;
            tp_type = (pjsip_transport_type_e) tp->key.type;
            rport = pjsip_transport_get_default_port_for_type(tp_type);
        }
    } else {
        rport = via->rport_param;
    }

    const pj_str_t *via_addr = via->recvd_param.slen != 0 ?
        &via->recvd_param : &via->sent_by.host;

    /* If allowViaRewrite_ is enabled, we save the Via "received" address
     * from the response.
     */
    if (allowViaRewrite_ and (via_addr_.host.slen == 0 or via_tp_ != tp)) {
        if (pj_strcmp(&via_addr_.host, via_addr))
            pj_strdup(pool, &via_addr_.host, via_addr);

        via_addr_.port = rport;
        via_tp_ = tp;
        pjsip_regc_set_via_sent_by(regc_, &via_addr_, via_tp_);
    }

    /* Only update if account is configured to auto-update */
    if (not allowContactRewrite_)
        return false;

    /* Compare received and rport with the URI in our registration */
    const pj_str_t STR_CONTACT = { (char*) "Contact", 7 };
    pjsip_contact_hdr *contact_hdr = (pjsip_contact_hdr*)
    pjsip_parse_hdr(pool, &STR_CONTACT, contact_.ptr, contact_.slen, nullptr);
    pj_assert(contact_hdr != nullptr);
    pjsip_sip_uri *uri = (pjsip_sip_uri*) contact_hdr->uri;
    pj_assert(uri != nullptr);
    uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);

    if (uri->port == 0) {
        pjsip_transport_type_e tp_type;
        tp_type = (pjsip_transport_type_e) tp->key.type;
        uri->port = pjsip_transport_get_default_port_for_type(tp_type);
    }

    /* Convert IP address strings into sockaddr for comparison.
     * (http://trac.pjsip.org/repos/ticket/863)
     */
    IpAddr contact_addr, recv_addr;
    pj_status_t status = pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &uri->host, contact_addr.pjPtr());
    if (status == PJ_SUCCESS)
        status = pj_sockaddr_parse(pj_AF_UNSPEC(), 0, via_addr, recv_addr.pjPtr());

    bool matched;
    if (status == PJ_SUCCESS) {
        // Compare the addresses as sockaddr according to the ticket above
        matched = (uri->port == rport and contact_addr == recv_addr);
    } else {
        // Compare the addresses as string, as before
        matched = (uri->port == rport and pj_stricmp(&uri->host, via_addr) == 0);
    }

    if (matched) {
        // Address doesn't change
        return false;
    }

    /* Get server IP */
    IpAddr srv_ip = {std::string(param->rdata->pkt_info.src_name)};

    /* At this point we've detected that the address as seen by registrar.
     * has changed.
     */

    /* Do not switch if both Contact and server's IP address are
     * public but response contains private IP. A NAT in the middle
     * might have messed up with the SIP packets. See:
     * http://trac.pjsip.org/repos/ticket/643
     *
     * This exception can be disabled by setting allow_contact_rewrite
     * to 2. In this case, the switch will always be done whenever there
     * is difference in the IP address in the response.
     */
    if (allowContactRewrite_ != 2 and
        not contact_addr.isPrivate() and
        not srv_ip.isPrivate() and
        recv_addr.isPrivate()) {
        /* Don't switch */
        return false;
    }

    /* Also don't switch if only the port number part is different, and
     * the Via received address is private.
     * See http://trac.pjsip.org/repos/ticket/864
     */
    if (allowContactRewrite_ != 2 and contact_addr == recv_addr and recv_addr.isPrivate()) {
        /* Don't switch */
        return false;
    }

    std::string via_addrstr(via_addr->ptr, via_addr->slen);
#if HAVE_IPV6
    /* Enclose IPv6 address in square brackets */
    if (IpAddr::isIpv6(via_addrstr))
        via_addrstr = IpAddr(via_addrstr).toString(false, true);
#endif

    WARN("IP address change detected for account %s "
         "(%.*s:%d --> %s:%d). Updating registration "
         "(using method %d)",
         accountID_.c_str(),
         (int) uri->host.slen,
         uri->host.ptr,
         uri->port,
         via_addrstr.c_str(),
         rport,
         contactRewriteMethod_);

    pj_assert(contactRewriteMethod_ == 1 or contactRewriteMethod_ == 2);

    if (contactRewriteMethod_ == 1) {
        /* Unregister current contact */
        link_.sendUnregister(*this);
        destroyRegistrationInfo();
    }

    /*
     * Build new Contact header
     */
    {
        char *tmp;
        char transport_param[32];
        int len;

        /* Don't add transport parameter if it's UDP */
        if (tp->key.type != PJSIP_TRANSPORT_UDP and
            tp->key.type != PJSIP_TRANSPORT_UDP6) {
            pj_ansi_snprintf(transport_param, sizeof(transport_param),
                 ";transport=%s",
                 pjsip_transport_get_type_name(
                     (pjsip_transport_type_e)tp->key.type));
        } else {
            transport_param[0] = '\0';
        }

        tmp = (char*) pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
        len = pj_ansi_snprintf(tmp, PJSIP_MAX_URL_SIZE,
                "<sip:%s%s%s:%d%s>",
                username_.c_str(),
                (not username_.empty() ?  "@" : ""),
                via_addrstr.c_str(),
                rport,
                transport_param);
        if (len < 1) {
            ERROR("URI too long");
            return false;
        }

        pj_str_t tmp_str = {tmp, len};
        pj_strncpy_with_null(&contact_, &tmp_str, PJSIP_MAX_URL_SIZE);
    }

    if (contactRewriteMethod_ == 2 && regc_ != nullptr) {
        contactOverwritten_ = true;

        /*  Unregister old contact */
        try {
            link_.sendUnregister(*this);
        } catch (const VoipLinkException &e) {
            ERROR("%s", e.what());
        }

        pjsip_regc_update_contact(regc_, 1, &contact_);

        /*  Perform new registration */
        try {
            link_.sendRegister(*this);
        } catch (const VoipLinkException &e) {
            ERROR("%s", e.what());
        }
    }

    return true;
}

/* Auto re-registration timeout callback */
void
SIPAccount::autoReregTimerCb(pj_timer_heap_t * /*th*/, pj_timer_entry *te)
{
    auto context = static_cast<std::pair<SIPAccount *, pjsip_endpoint *> *>(te->user_data);
    SIPAccount *acc = context->first;
    pjsip_endpoint *endpt = context->second;

    /* Check if the reregistration timer is still valid, e.g: while waiting
     * timeout timer application might have deleted the account or disabled
     * the auto-reregistration.
     */
    if (not acc->auto_rereg_.active) {
        delete context;
        return;
    }

    /* Start re-registration */
    acc->auto_rereg_.attempt_cnt++;
    try {
        acc->link_.sendRegister(*acc);
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        acc->scheduleReregistration(endpt);
    }
    delete context;
}

/* Schedule reregistration for specified account. Note that the first
 * re-registration after a registration failure will be done immediately.
 * Also note that this function should be called within PJSUA mutex.
 */
void
SIPAccount::scheduleReregistration(pjsip_endpoint *endpt)
{
    if (!isEnabled())
        return;

    /* Cancel any re-registration timer */
    if (auto_rereg_.timer.id) {
        auto_rereg_.timer.id = PJ_FALSE;
        pjsip_endpt_cancel_timer(endpt, &auto_rereg_.timer);
    }

    /* Update re-registration flag */
    auto_rereg_.active = PJ_TRUE;

    /* Set up timer for reregistration */
    auto_rereg_.timer.cb = &SIPAccount::autoReregTimerCb;
    auto_rereg_.timer.user_data = new std::pair<SIPAccount *, pjsip_endpoint *>(this, endpt);

    /* Reregistration attempt. The first attempt will be done immediately. */
    pj_time_val delay;
    const int FIRST_RETRY_INTERVAL = 60;
    const int RETRY_INTERVAL = 300;
    delay.sec = auto_rereg_.attempt_cnt ? RETRY_INTERVAL : FIRST_RETRY_INTERVAL;
    delay.msec = 0;

    /* Randomize interval by +/- 10 secs */
    if (delay.sec >= 10) {
        delay.msec = -10000 + (pj_rand() % 20000);
    } else {
        delay.sec = 0;
        delay.msec = (pj_rand() % 10000);
    }

    pj_time_val_normalize(&delay);

    WARN("Scheduling re-registration retry in %u seconds..", delay.sec);
    auto_rereg_.timer.id = PJ_TRUE;
    if (pjsip_endpt_schedule_timer(endpt, &auto_rereg_.timer, &delay) != PJ_SUCCESS)
        auto_rereg_.timer.id = PJ_FALSE;
}

void SIPAccount::updateDialogViaSentBy(pjsip_dialog *dlg)
{
    if (allowViaRewrite_ && via_addr_.host.slen > 0)
        pjsip_dlg_set_via_sent_by(dlg, &via_addr_, via_tp_);
}
