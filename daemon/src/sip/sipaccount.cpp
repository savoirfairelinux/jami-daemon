/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "intrin.h"

#include "sdp.h"
#include "sipvoiplink.h"
#include "sipcall.h"
#include "sip_utils.h"
#include "array_size.h"

#include "call_factory.h"

#ifdef SFL_PRESENCE
#include "sippresence.h"
#include "client/configurationmanager.h"
#endif

#include <yaml-cpp/yaml.h>

#include "account_schema.h"
#include "config/yamlparser.h"
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

static const int MIN_REGISTRATION_TIME = 60;
static const int DEFAULT_REGISTRATION_TIME = 3600;
static const char *const VALID_TLS_METHODS[] = {"Default", "TLSv1", "SSLv3", "SSLv23"};
static const char *const VALID_SRTP_KEY_EXCHANGES[] = {"", "sdes", "zrtp"};

constexpr const char * const SIPAccount::ACCOUNT_TYPE;

#if HAVE_TLS

const CipherArray SIPAccount::TLSv1_DEFAULT_CIPHER_LIST = {
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA256,
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA256,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA,
    PJ_TLS_RSA_WITH_RC4_128_SHA,
    PJ_TLS_RSA_WITH_RC4_128_MD5
};

const CipherArray SIPAccount::SSLv3_DEFAULT_CIPHER_LIST = {
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA256,
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA256,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA,
    PJ_TLS_RSA_WITH_RC4_128_SHA,
    PJ_TLS_RSA_WITH_RC4_128_MD5
};

const CipherArray SIPAccount::SSLv23_DEFAULT_CIPHER_LIST = {
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA256,
    PJ_TLS_RSA_WITH_AES_256_CBC_SHA,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA256,
    PJ_TLS_RSA_WITH_AES_128_CBC_SHA,
    PJ_TLS_RSA_WITH_RC4_128_SHA,
    PJ_TLS_RSA_WITH_RC4_128_MD5,
    PJ_SSL_CK_DES_192_EDE3_CBC_WITH_MD5,
    PJ_SSL_CK_RC4_128_WITH_MD5,
    PJ_SSL_CK_IDEA_128_CBC_WITH_MD5,
    PJ_SSL_CK_RC2_128_CBC_WITH_MD5,
};

#endif

static void
registration_cb(pjsip_regc_cbparam *param)
{
    if (!param) {
        ERROR("registration callback parameter is null");
        return;
    }

    auto account = static_cast<SIPAccount *>(param->token);
    if (!account) {
        ERROR("account doesn't exist in registration callback");
        return;
    }

    account->onRegister(param);
}

SIPAccount::SIPAccount(const std::string& accountID, bool presenceEnabled)
    : SIPAccountBase(accountID)
    , auto_rereg_()
    , credentials_()
    , regc_(nullptr)
    , bRegister_(false)
    , registrationExpire_(MIN_REGISTRATION_TIME)
    , serviceRoute_()
    , cred_()
    , tlsSetting_()
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
    , zrtpDisplaySas_(true)
    , zrtpDisplaySasOnce_(false)
    , zrtpHelloHash_(true)
    , zrtpNotSuppWarning_(true)
    , registrationStateDetailed_()
    , keepAliveEnabled_(false)
    , keepAliveTimer_()
    , keepAliveTimerActive_(false)
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

std::shared_ptr<SIPCall>
SIPAccount::newIncomingCall(const std::string& id)
{
    return Manager::instance().callFactory.newCall<SIPCall, SIPAccount>(*this, id, Call::INCOMING);
}

template <>
std::shared_ptr<SIPCall>
SIPAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    std::string to;
    std::string toUri;
    int family;

    auto call = Manager::instance().callFactory.newCall<SIPCall, SIPAccount>(*this, id, Call::OUTGOING);

    if (isIP2IP()) {
        bool ipv6 = false;
#if HAVE_IPV6
        ipv6 = IpAddr::isIpv6(toUrl);
#endif
        to = ipv6 ? IpAddr(toUrl).toString(false, true) : toUrl;
        toUri = getToUri(to);
        family = ipv6 ? pj_AF_INET6() : pj_AF_INET();

        std::shared_ptr<SipTransport> t =
#if HAVE_TLS
            isTlsEnabled() ? link_->sipTransport->getTlsTransport(tlsListener_, toUri) :
#endif
            transport_;
        setTransport(t);
        call->setTransport(t);

        DEBUG("New %s IP to IP call to %s", ipv6?"IPv6":"IPv4", to.c_str());
    }
    else {
        to = toUrl;

        // If toUrl is not a well formatted sip URI, use account information to process it
        if (toUrl.find("sip:") != std::string::npos or
            toUrl.find("sips:") != std::string::npos)
            toUri = toUrl;
        else
            toUri = getToUri(to);

        call->setTransport(transport_);
        // FIXME : for now, use the same address family as the SIP transport
        family = pjsip_transport_type_get_af(getTransportType());

        DEBUG("UserAgent: New registered account call to %s", toUrl.c_str());
    }

    call->setIPToIP(isIP2IP());
    call->setPeerNumber(toUri);
    call->initRecFilename(to);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface(), family);
    call->setCallMediaLocal(localAddress);

    // May use the published address as well
    const auto addrSdp = isStunEnabled() or (not getPublishedSameasLocal()) ?
        getPublishedIpAddress() : localAddress;

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);
    if (!ac)
        throw VoipLinkException("Could not instantiate codec for early media");

    std::vector<sfl::AudioCodec *> audioCodecs;
    audioCodecs.push_back(ac);

    try {
        call->getAudioRtp().initConfig();
        call->getAudioRtp().initSession();

        if (isStunEnabled())
            call->updateSDPFromSTUN();

        call->getAudioRtp().initLocalCryptoInfo();
        call->getAudioRtp().start(audioCodecs);
    } catch (...) {
        throw VoipLinkException("Could not start rtp session for early media");
    }

    // Building the local SDP offer
    auto& localSDP = call->getLocalSDP();

    if (getPublishedSameasLocal())
        localSDP.setPublishedIP(addrSdp);
    else
        localSDP.setPublishedIP(getPublishedAddress());

    const bool created = localSDP.createOffer(getActiveAudioCodecs(), getActiveVideoCodecs());

    if (not created or not SIPStartCall(call))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");

    return call;
}

std::shared_ptr<Call>
SIPAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    return newOutgoingCall<SIPCall>(id, toUrl);
}

bool
SIPAccount::SIPStartCall(std::shared_ptr<SIPCall>& call)
{
    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    auto transport = call->getTransport();
    if (!transport) {
        ERROR("Unable to start call without transport");
        return false;
    }

    pj_str_t pjContact = getContactHeader(transport->get());
    const std::string debugContactHeader(pj_strbuf(&pjContact), pj_strlen(&pjContact));
    DEBUG("contact header: %s / %s -> %s",
          debugContactHeader.c_str(), from.c_str(), toUri.c_str());

    pjsip_dialog *dialog = NULL;

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog) != PJ_SUCCESS) {
        ERROR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    pj_str_t subj_hdr_name = CONST_PJ_STR("Subject");
    pjsip_hdr* subj_hdr = (pjsip_hdr*) pjsip_parse_hdr(dialog->pool, &subj_hdr_name, (char *) "Phone call", 10, NULL);

    pj_list_push_back(&dialog->inv_hdr, subj_hdr);

    pjsip_inv_session* inv = nullptr;
    if (pjsip_inv_create_uac(dialog, call->getLocalSDP().getLocalSdpSession(), 0, &inv) != PJ_SUCCESS) {
        ERROR("Unable to create invite session for user agent client");
        return false;
    }

    if (!inv) {
        ERROR("Call invite is not initialized");
        return PJ_FALSE;
    }

    call->inv.reset(inv);

    updateDialogViaSentBy(dialog);

    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));

    if (hasCredentials() and pjsip_auth_clt_set_credentials(&dialog->auth_sess, getCredentialCount(), getCredInfo()) != PJ_SUCCESS) {
        ERROR("Could not initialize credentials for invite session authentication");
        return false;
    }

    call->inv->mod_data[link_->getModId()] = call.get();

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv.get(), &tdata) != PJ_SUCCESS) {
        ERROR("Could not initialize invite messager for this call");
        return false;
    }

    const pjsip_tpselector tp_sel = SipTransportBroker::getTransportSelector(transport->get());
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        ERROR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        call->inv.reset();
        ERROR("Unable to send invite message for this call");
        return false;
    }

    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);

    return true;
}

void SIPAccount::serialize(YAML::Emitter &out)
{
    using namespace Conf;

    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);

    // each credential is a map, and we can have multiple credentials
    out << YAML::Key << CRED_KEY << YAML::Value << credentials_;
    out << YAML::Key << KEEP_ALIVE_ENABLED << YAML::Value << keepAliveEnabled_;

#ifdef SFL_PRESENCE
    out << YAML::Key << PRESENCE_MODULE_ENABLED_KEY << YAML::Value << (presence_ and presence_->isEnabled());
    out << YAML::Key << PRESENCE_PUBLISH_SUPPORTED_KEY << YAML::Value << (presence_ and presence_->isSupported(PRESENCE_FUNCTION_PUBLISH));
    out << YAML::Key << PRESENCE_SUBSCRIBE_SUPPORTED_KEY << YAML::Value << (presence_ and presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE));
#else
    out << YAML::Key << PRESENCE_MODULE_ENABLED_KEY << YAML::Value << false;
    out << YAML::Key << PRESENCE_PUBLISH_SUPPORTED_KEY << YAML::Value << false;
    out << YAML::Key << PRESENCE_SUBSCRIBE_SUPPORTED_KEY << YAML::Value << false;
#endif
    out << YAML::Key << Preferences::REGISTRATION_EXPIRE_KEY << YAML::Value << registrationExpire_;
    out << YAML::Key << SERVICE_ROUTE_KEY << YAML::Value << serviceRoute_;

    out << YAML::Key << STUN_ENABLED_KEY << YAML::Value << stunEnabled_;
    out << YAML::Key << STUN_SERVER_KEY << YAML::Value << stunServer_;

    // tls submap
    out << YAML::Key << TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::Key << TLS_ENABLE_KEY << YAML::Value << tlsEnable_;
    out << YAML::Key << VERIFY_CLIENT_KEY << YAML::Value << tlsVerifyClient_;
    out << YAML::Key << VERIFY_SERVER_KEY << YAML::Value << tlsVerifyServer_;
    out << YAML::Key << REQUIRE_CERTIF_KEY << YAML::Value << tlsRequireClientCertificate_;
    out << YAML::Key << TIMEOUT_KEY << YAML::Value << tlsNegotiationTimeoutSec_;
    out << YAML::Key << CALIST_KEY << YAML::Value << tlsCaListFile_;
    out << YAML::Key << CERTIFICATE_KEY << YAML::Value << tlsCertificateFile_;
    out << YAML::Key << CIPHERS_KEY << YAML::Value << tlsCiphers_;
    out << YAML::Key << METHOD_KEY << YAML::Value << tlsMethod_;
    out << YAML::Key << TLS_PASSWORD_KEY << YAML::Value << tlsPassword_;
    out << YAML::Key << PRIVATE_KEY_KEY << YAML::Value << tlsPrivateKeyFile_;
    out << YAML::Key << SERVER_KEY << YAML::Value << tlsServerName_;
    out << YAML::EndMap;

    // srtp submap
    out << YAML::Key << SRTP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << SRTP_ENABLE_KEY << YAML::Value << srtpEnabled_;
    out << YAML::Key << KEY_EXCHANGE_KEY << YAML::Value << srtpKeyExchange_;
    out << YAML::Key << RTP_FALLBACK_KEY << YAML::Value << srtpFallback_;
    out << YAML::EndMap;

    // zrtp submap
    out << YAML::Key << ZRTP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << DISPLAY_SAS_KEY << YAML::Value << zrtpDisplaySas_;
    out << YAML::Key << DISPLAY_SAS_ONCE_KEY << YAML::Value << zrtpDisplaySasOnce_;
    out << YAML::Key << HELLO_HASH_ENABLED_KEY << YAML::Value << zrtpHelloHash_;
    out << YAML::Key << NOT_SUPP_WARNING_KEY << YAML::Value << zrtpNotSuppWarning_;
    out << YAML::EndMap;

    out << YAML::EndMap;
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

void SIPAccount::unserialize(const YAML::Node &node)
{
    using namespace Conf;
    using namespace yaml_utils;

    SIPAccountBase::unserialize(node);
    if (not publishedSameasLocal_)
        usePublishedAddressPortInVIA();

    if (not isIP2IP()) parseValue(node, Preferences::REGISTRATION_EXPIRE_KEY, registrationExpire_);

    if (not isIP2IP()) parseValue(node, KEEP_ALIVE_ENABLED, keepAliveEnabled_);

    bool presEnabled = false;
    parseValue(node, PRESENCE_ENABLED_KEY, presEnabled);
    enablePresence(presEnabled);
    bool publishSupported = false;
    parseValue(node, PRESENCE_PUBLISH_SUPPORTED_KEY, publishSupported);
    bool subscribeSupported = false;
    parseValue(node, PRESENCE_SUBSCRIBE_SUPPORTED_KEY, subscribeSupported);
#ifdef SFL_PRESENCE
    if (presence_) {
        presence_->support(PRESENCE_FUNCTION_PUBLISH, publishSupported);
        presence_->support(PRESENCE_FUNCTION_SUBSCRIBE, subscribeSupported);
    }
#endif

    if (not isIP2IP()) parseValue(node, SERVICE_ROUTE_KEY, serviceRoute_);

    // stun enabled
    if (not isIP2IP()) parseValue(node, STUN_ENABLED_KEY, stunEnabled_);
    if (not isIP2IP()) parseValue(node, STUN_SERVER_KEY, stunServer_);

    // Init stun server name with default server name
    stunServerName_ = pj_str((char*) stunServer_.data());

    const auto &credsNode = node[CRED_KEY];
    const auto creds = parseVectorMap(credsNode, {CONFIG_ACCOUNT_PASSWORD,
            CONFIG_ACCOUNT_REALM, CONFIG_ACCOUNT_USERNAME});
    setCredentials(creds);

    // get zrtp submap
    const auto &zrtpMap = node[ZRTP_KEY];

    parseValue(zrtpMap, DISPLAY_SAS_KEY, zrtpDisplaySas_);
    parseValue(zrtpMap, DISPLAY_SAS_ONCE_KEY, zrtpDisplaySasOnce_);
    parseValue(zrtpMap, HELLO_HASH_ENABLED_KEY, zrtpHelloHash_);
    parseValue(zrtpMap, NOT_SUPP_WARNING_KEY, zrtpNotSuppWarning_);

    // get tls submap
    const auto &tlsMap = node[TLS_KEY];

    parseValue(tlsMap, TLS_ENABLE_KEY, tlsEnable_);
    parseValue(tlsMap, CERTIFICATE_KEY, tlsCertificateFile_);
    parseValue(tlsMap, CALIST_KEY, tlsCaListFile_);
    parseValue(tlsMap, CIPHERS_KEY, tlsCiphers_);

    std::string tmpMethod(tlsMethod_);
    parseValue(tlsMap, METHOD_KEY, tmpMethod);
    validate(tlsMethod_, tmpMethod, VALID_TLS_METHODS);

    parseValue(tlsMap, TLS_PASSWORD_KEY, tlsPassword_);
    parseValue(tlsMap, PRIVATE_KEY_KEY, tlsPrivateKeyFile_);
    parseValue(tlsMap, SERVER_KEY, tlsServerName_);
    parseValue(tlsMap, REQUIRE_CERTIF_KEY, tlsRequireClientCertificate_);
    parseValue(tlsMap, VERIFY_CLIENT_KEY, tlsVerifyClient_);
    parseValue(tlsMap, VERIFY_SERVER_KEY, tlsVerifyServer_);
    // FIXME
    parseValue(tlsMap, TIMEOUT_KEY, tlsNegotiationTimeoutSec_);

    // get srtp submap
    const auto &srtpMap = node[SRTP_KEY];
    parseValue(srtpMap, SRTP_ENABLE_KEY, srtpEnabled_);

    std::string tmpKey;
    parseValue(srtpMap, KEY_EXCHANGE_KEY, tmpKey);
    validate(srtpKeyExchange_, tmpKey, VALID_SRTP_KEY_EXCHANGES);
    parseValue(srtpMap, RTP_FALLBACK_KEY, srtpFallback_);
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
    SIPAccountBase::setAccountDetails(details);

    // SIP specific account settings
    parseString(details, CONFIG_ACCOUNT_ROUTESET, serviceRoute_);

    if (not publishedSameasLocal_)
        usePublishedAddressPortInVIA();

    parseString(details, CONFIG_STUN_SERVER, stunServer_);
    parseBool(details, CONFIG_STUN_ENABLE, stunEnabled_);
    parseInt(details, CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire_);

    if (registrationExpire_ < MIN_REGISTRATION_TIME)
        registrationExpire_ = MIN_REGISTRATION_TIME;

    parseBool(details, CONFIG_KEEP_ALIVE_ENABLED, keepAliveEnabled_);
#ifdef SFL_PRESENCE
    bool presenceEnabled = false;
    parseBool(details, CONFIG_PRESENCE_ENABLED, presenceEnabled);
    enablePresence(presenceEnabled);
#endif

    // srtp settings
    parseBool(details, CONFIG_ZRTP_DISPLAY_SAS, zrtpDisplaySas_);
    parseBool(details, CONFIG_ZRTP_DISPLAY_SAS_ONCE, zrtpDisplaySasOnce_);
    parseBool(details, CONFIG_ZRTP_NOT_SUPP_WARNING, zrtpNotSuppWarning_);
    parseBool(details, CONFIG_ZRTP_HELLO_HASH, zrtpHelloHash_);

    // TLS settings
    parseBool(details, CONFIG_TLS_ENABLE, tlsEnable_);
    parseInt(details, CONFIG_TLS_LISTENER_PORT, tlsListenerPort_);
    parseString(details, CONFIG_TLS_CA_LIST_FILE, tlsCaListFile_);
    parseString(details, CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_);

    parseString(details, CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_);
    parseString(details, CONFIG_TLS_PASSWORD, tlsPassword_);
    auto iter = details.find(CONFIG_TLS_METHOD);
    if (iter != details.end())
        validate(tlsMethod_, iter->second, VALID_TLS_METHODS);
    parseString(details, CONFIG_TLS_CIPHERS, tlsCiphers_);
    parseString(details, CONFIG_TLS_SERVER_NAME, tlsServerName_);
    parseBool(details, CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer_);
    parseBool(details, CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient_);
    parseBool(details, CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    parseString(details, CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);
    parseBool(details, CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer_);
    parseBool(details, CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient_);
    parseBool(details, CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    parseString(details, CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);

    // srtp settings
    parseBool(details, CONFIG_SRTP_ENABLE, srtpEnabled_);
    parseBool(details, CONFIG_SRTP_RTP_FALLBACK, srtpFallback_);
    iter = details.find(CONFIG_SRTP_KEY_EXCHANGE);
    if (iter != details.end())
        validate(srtpKeyExchange_, iter->second, VALID_SRTP_KEY_EXCHANGES);

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

std::map<std::string, std::string> SIPAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();

    a[CONFIG_ACCOUNT_PASSWORD] = "";
    if (hasCredentials()) {
        for (const auto &vect_item : credentials_) {
            const std::string password = retrievePassword(vect_item, username_);

            if (not password.empty())
                a[CONFIG_ACCOUNT_PASSWORD] = password;
        }
    }

    std::string registrationStateCode;
    std::string registrationStateDescription;
    if (isIP2IP())
        registrationStateDescription = "Direct IP call";
    else {
        int code = registrationStateDetailed_.first;
        std::stringstream out;
        out << code;
        registrationStateCode = out.str();
        registrationStateDescription = registrationStateDetailed_.second;
    }
    a[CONFIG_ACCOUNT_REGISTRATION_STATE_CODE] = registrationStateCode;
    a[CONFIG_ACCOUNT_REGISTRATION_STATE_DESC] = registrationStateDescription;

#ifdef SFL_PRESENCE
    a[CONFIG_PRESENCE_ENABLED] = presence_ and presence_->isEnabled()? TRUE_STR : FALSE_STR;
    a[CONFIG_PRESENCE_PUBLISH_SUPPORTED] = presence_ and presence_->isSupported(PRESENCE_FUNCTION_PUBLISH)? TRUE_STR : FALSE_STR;
    a[CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED] = presence_ and presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE)? TRUE_STR : FALSE_STR;
    // initialize status values
    a[CONFIG_PRESENCE_STATUS] = presence_ and presence_->isOnline()? TRUE_STR : FALSE_STR;
    a[CONFIG_PRESENCE_NOTE] = presence_ ? presence_->getNote() : " ";
#endif

    // Add sip specific details
    a[CONFIG_ACCOUNT_ROUTESET] = serviceRoute_;

    std::stringstream registrationExpireStr;
    registrationExpireStr << registrationExpire_;
    a[CONFIG_ACCOUNT_REGISTRATION_EXPIRE] = registrationExpireStr.str();

    a[CONFIG_STUN_ENABLE] = stunEnabled_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_STUN_SERVER] = stunServer_;
    a[CONFIG_KEEP_ALIVE_ENABLED] = keepAliveEnabled_ ? TRUE_STR : FALSE_STR;

    // TLS listener is unique and parameters are modified through IP2IP_PROFILE
    a[CONFIG_TLS_ENABLE] = tlsEnable_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_TLS_CA_LIST_FILE] = tlsCaListFile_;
    a[CONFIG_TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    a[CONFIG_TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    a[CONFIG_TLS_PASSWORD] = tlsPassword_;
    a[CONFIG_TLS_METHOD] = tlsMethod_;
    a[CONFIG_TLS_CIPHERS] = tlsCiphers_;
    a[CONFIG_TLS_SERVER_NAME] = tlsServerName_;
    a[CONFIG_TLS_VERIFY_SERVER] = tlsVerifyServer_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_TLS_VERIFY_CLIENT] = tlsVerifyClient_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE] = tlsRequireClientCertificate_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC] = tlsNegotiationTimeoutSec_;

    a[CONFIG_SRTP_KEY_EXCHANGE] = srtpKeyExchange_;
    a[CONFIG_SRTP_ENABLE] = srtpEnabled_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_SRTP_RTP_FALLBACK] = srtpFallback_ ? TRUE_STR : FALSE_STR;

    a[CONFIG_ZRTP_DISPLAY_SAS] = zrtpDisplaySas_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_ZRTP_DISPLAY_SAS_ONCE] = zrtpDisplaySasOnce_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_ZRTP_HELLO_HASH] = zrtpHelloHash_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_ZRTP_NOT_SUPP_WARNING] = zrtpNotSuppWarning_ ? TRUE_STR : FALSE_STR;

    return a;
}

std::map<std::string, std::string> SIPAccount::getVolatileAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getVolatileAccountDetails();
    std::stringstream codestream;
    codestream << registrationStateDetailed_.first;
    a[CONFIG_ACCOUNT_REGISTRATION_STATE_CODE] = codestream.str();
    a[CONFIG_ACCOUNT_REGISTRATION_STATE_DESC] = registrationStateDetailed_.second;

#ifdef SFL_PRESENCE
    if (presence_) {
        a[CONFIG_PRESENCE_STATUS] = presence_ and presence_->isOnline()? TRUE_STR : FALSE_STR;
        a[CONFIG_PRESENCE_NOTE] = presence_ ? presence_->getNote() : " ";
    }
#endif

#if HAVE_TLS
    //TODO
#endif

    return a;
}

void SIPAccount::doRegister()
{
    if (not isEnabled()) {
        WARN("Account must be enabled to register, ignoring");
        return;
    }

    if (hostname_.length() >= PJ_MAX_HOSTNAME)
        return;

    DEBUG("doRegister %s ", hostname_.c_str());

    auto IPs = ip_utils::getAddrList(hostname_);
    for (const auto& ip : IPs)
        DEBUG("--- %s ", ip.toString().c_str());

    bool IPv6 = false;
#if HAVE_IPV6
    if (isIP2IP()) {
        DEBUG("doRegister isIP2IP.");
        IPv6 = ip_utils::getInterfaceAddr(interface_).isIpv6();
    } else if (!IPs.empty())
        IPv6 = IPs[0].isIpv6();
#endif

#if HAVE_TLS
    // Init TLS settings if the user wants to use TLS
    if (tlsEnable_) {
        DEBUG("TLS is enabled for account %s", accountID_.c_str());

        // Dropping current calls already using the transport is currently required
        // with TLS.
        freeAccount();

        transportType_ = IPv6 ? PJSIP_TRANSPORT_TLS6 : PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();

        if (!tlsListener_) {
            tlsListener_ = link_->sipTransport->getTlsListener(
                SipTransportDescr {getTransportType(), getTlsListenerPort(), getLocalInterface()},
                getTlsSetting());
            if (!tlsListener_) {
                setRegistrationState(RegistrationState::ERROR_GENERIC);
                ERROR("Error creating TLS listener.");
                return;
            }
        }
    } else
#endif
    {
        tlsListener_.reset();
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
    if (isIP2IP()) {
        // If we use Tls for IP2IP, transports will be created on connection.
        if (!tlsEnable_)
            setTransport(link_->sipTransport->getUdpTransport(
                SipTransportDescr { getTransportType(), getLocalPort(), getLocalInterface() }
            ));
        return;
    }

    try {
        WARN("Creating transport");
        transport_.reset();
#if HAVE_TLS
        if (isTlsEnabled()) {
            setTransport(link_->sipTransport->getTlsTransport(tlsListener_, getServerUri()));
        } else
#endif
        {
            setTransport(link_->sipTransport->getUdpTransport(
                SipTransportDescr { getTransportType(), getLocalPort(), getLocalInterface() }
            ));
        }
        if (!transport_)
            throw VoipLinkException("Can't create transport");

        sendRegister();
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
        return;
    }

#ifdef SFL_PRESENCE
    if (presence_ and presence_->isEnabled()) {
        presence_->subscribeClient(getFromUri(), true); // self presence subscription
        presence_->sendPresence(true, ""); // try to publish whatever the status is.
    }
#endif
}

void SIPAccount::doUnregister(std::function<void(bool)> released_cb)
{
    tlsListener_.reset();
    if (isIP2IP()) {
        if (released_cb)
            released_cb(false);
        return;
    }

    try {
        sendUnregister();
    } catch (const VoipLinkException &e) {
        ERROR("doUnregister %s", e.what());
    }

    // remove the transport from the account
    if (transport_)
        setTransport();
    if (released_cb)
        released_cb(true);
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

    link_->registerKeepAliveTimer(keepAliveTimer_, keepAliveDelay_);
}

void SIPAccount::stopKeepAliveTimer()
{
    if (keepAliveTimerActive_) {
        DEBUG("Stop keep alive timer %d for account %s", keepAliveTimer_.id, getAccountID().c_str());
        keepAliveTimerActive_ = false;
        link_->cancelKeepAliveTimer(keepAliveTimer_);
    }
}

void
SIPAccount::sendRegister()
{
    if (not isEnabled()) {
        WARN("Account must be enabled to register, ignoring");
        return;
    }

    setRegister(true);
    setRegistrationState(RegistrationState::TRYING);

    pjsip_regc *regc = nullptr;
    if (pjsip_regc_create(link_->getEndpoint(), (void *) this, &registration_cb, &regc) != PJ_SUCCESS)
        throw VoipLinkException("UserAgent: Unable to create regc structure.");

    std::string srvUri(getServerUri());
    pj_str_t pjSrv = pj_str((char*) srvUri.c_str());

    // Generate the FROM header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    // Get the received header
    std::string received(getReceivedParameter());

    // Get the contact header
    const pj_str_t pjContact(getContactHeader());

    if (transport_) {
        if (not getPublishedSameasLocal() or (not received.empty() and received != getPublishedAddress())) {
            pjsip_host_port *via = getViaAddr();
            DEBUG("Setting VIA sent-by to %.*s:%d", via->host.slen, via->host.ptr, via->port);

            if (pjsip_regc_set_via_sent_by(regc, via, transport_->get()) != PJ_SUCCESS)
                throw VoipLinkException("Unable to set the \"sent-by\" field");
        } else if (isStunEnabled()) {
            if (pjsip_regc_set_via_sent_by(regc, getViaAddr(), transport_->get()) != PJ_SUCCESS)
                throw VoipLinkException("Unable to set the \"sent-by\" field");
        }
    }

    pj_status_t status;

    //DEBUG("pjsip_regc_init from:%s, srv:%s, contact:%s", from.c_str(), srvUri.c_str(), std::string(pj_strbuf(&pjContact), pj_strlen(&pjContact)).c_str());
    if ((status = pjsip_regc_init(regc, &pjSrv, &pjFrom, &pjFrom, 1, &pjContact, getRegistrationExpire())) != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        throw VoipLinkException("Unable to initialize account registration structure");
    }

    if (hasServiceRoute())
        pjsip_regc_set_route_set(regc, sip_utils::createRouteSet(getServiceRoute(), link_->getPool()));

    pjsip_regc_set_credentials(regc, getCredentialCount(), getCredInfo());

    pjsip_hdr hdr_list;
    pj_list_init(&hdr_list);
    std::string useragent(getUserAgentName());
    pj_str_t pJuseragent = pj_str((char*) useragent.c_str());
    const pj_str_t STR_USER_AGENT = CONST_PJ_STR("User-Agent");

    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create(link_->getPool(), &STR_USER_AGENT, &pJuseragent);
    pj_list_push_back(&hdr_list, (pjsip_hdr*) h);
    pjsip_regc_add_headers(regc, &hdr_list);
    pjsip_tx_data *tdata;

    if (pjsip_regc_register(regc, PJ_TRUE, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to initialize transaction data for account registration");

    const pjsip_tpselector tp_sel = getTransportSelector();
    if (pjsip_regc_set_transport(regc, &tp_sel) != PJ_SUCCESS)
        throw VoipLinkException("Unable to set transport");

    // pjsip_regc_send increment the transport ref count by one,
    if ((status = pjsip_regc_send(regc, tdata)) != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        throw VoipLinkException("Unable to send account registration request");
    }

    setRegistrationInfo(regc);
}

void
SIPAccount::onRegister(pjsip_regc_cbparam *param)
{
    if (param->regc != getRegistrationInfo())
        return;

    if (param->status != PJ_SUCCESS) {
        ERROR("SIP registration error %d", param->status);
        destroyRegistrationInfo();
        stopKeepAliveTimer();
    } else if (param->code < 0 || param->code >= 300) {
        ERROR("SIP registration failed, status=%d (%.*s)",
              param->code, (int)param->reason.slen, param->reason.ptr);
        destroyRegistrationInfo();
        stopKeepAliveTimer();
        switch (param->code) {
            case PJSIP_SC_FORBIDDEN:
                setRegistrationState(RegistrationState::ERROR_AUTH);
                break;
            case PJSIP_SC_NOT_FOUND:
                setRegistrationState(RegistrationState::ERROR_HOST);
                break;
            case PJSIP_SC_REQUEST_TIMEOUT:
                setRegistrationState(RegistrationState::ERROR_HOST);
                break;
            case PJSIP_SC_SERVICE_UNAVAILABLE:
                setRegistrationState(RegistrationState::ERROR_SERVICE_UNAVAILABLE);
                break;
            default:
                setRegistrationState(RegistrationState::ERROR_GENERIC);
        }
    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {

        // Update auto registration flag
        resetAutoRegistration();

        if (param->expiration < 1) {
            destroyRegistrationInfo();
            /* Stop keep-alive timer if any. */
            stopKeepAliveTimer();
            DEBUG("Unregistration success");
            setRegistrationState(RegistrationState::UNREGISTERED);
        } else {
            /* TODO Check and update SIP outbound status first, since the result
             * will determine if we should update re-registration
             */
            // update_rfc5626_status(acc, param->rdata);

            if (checkNATAddress(param, link_->getPool()))
                WARN("Contact overwritten");

            /* TODO Check and update Service-Route header */
            if (hasServiceRoute())
                pjsip_regc_set_route_set(param->regc, sip_utils::createRouteSet(getServiceRoute(), link_->getPool()));

            // start the periodic registration request based on Expire header
            // account determines itself if a keep alive is required
            if (isKeepAliveEnabled())
                startKeepAliveTimer();

            setRegistrationState(RegistrationState::REGISTERED);
        }
    }

    /* Check if we need to auto retry registration. Basically, registration
     * failure codes triggering auto-retry are those of temporal failures
     * considered to be recoverable in relatively short term.
     */
    switch (param->code) {
        case PJSIP_SC_REQUEST_TIMEOUT:
        case PJSIP_SC_INTERNAL_SERVER_ERROR:
        case PJSIP_SC_BAD_GATEWAY:
        case PJSIP_SC_SERVICE_UNAVAILABLE:
        case PJSIP_SC_SERVER_TIMEOUT:
            scheduleReregistration(link_->getEndpoint());
            break;

        default:
            /* Global failure */
            if (PJSIP_IS_STATUS_IN_CLASS(param->code, 600))
                scheduleReregistration(link_->getEndpoint());
    }

    const pj_str_t *description = pjsip_get_status_text(param->code);

    if (param->code && description) {
        std::string state(description->ptr, description->slen);

        Manager::instance().getClient()->getConfigurationManager()->sipRegistrationStateChanged(getAccountID(), state, param->code);
        Manager::instance().getClient()->getConfigurationManager()->volatileAccountDetailsChanged(accountID_, getVolatileAccountDetails());
        std::pair<int, std::string> details(param->code, state);
        // TODO: there id a race condition for this ressource when closing the application
        setRegistrationStateDetailed(details);
        setRegistrationExpire(param->expiration);
    }
}

void
SIPAccount::sendUnregister()
{
    // This may occurs if account failed to register and is in state INVALID
    if (!isRegistered()) {
        setRegistrationState(RegistrationState::UNREGISTERED);
        return;
    }

    // Make sure to cancel any ongoing timers before unregister
    stopKeepAliveTimer();

    pjsip_regc *regc = getRegistrationInfo();
    if (!regc)
        throw VoipLinkException("Registration structure is NULL");

    pjsip_tx_data *tdata = nullptr;
    if (pjsip_regc_unregister(regc, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to unregister sip account");

    pj_status_t status;
    if ((status = pjsip_regc_send(regc, tdata)) != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        throw VoipLinkException("Unable to send request to unregister sip account");
    }

    setRegister(false);
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

/**
 * PJSIP aborts if our cipher list exceeds 1000 characters
 */
void SIPAccount::trimCiphers()
{
    size_t sum = 0;
    unsigned count = 0;
    static const size_t MAX_CIPHERS_STRLEN = 1000;
    for (const auto &item : ciphers_) {
        sum += strlen(pj_ssl_cipher_name(item));
        if (sum > MAX_CIPHERS_STRLEN)
            break;
        ++count;
    }
    ciphers_.resize(count);
}

void SIPAccount::initTlsConfiguration()
{
    pjsip_tls_setting_default(&tlsSetting_);
    tlsSetting_.method = sslMethodStringToPjEnum(tlsMethod_);

    // Determine the cipher list supported on this machine
    CipherArray avail_ciphers(256);
    unsigned cipherNum = avail_ciphers.size();
    if (pj_ssl_cipher_get_availables(&avail_ciphers.front(), &cipherNum) != PJ_SUCCESS)
        ERROR("Could not determine cipher list on this system");
    avail_ciphers.resize(cipherNum);

    ciphers_.clear();
#if PJ_VERSION_NUM > (2 << 24 | 2 << 16)
    if (not tlsCiphers_.empty()) {
        std::stringstream ss(tlsCiphers_);
        std::string item;
        while (std::getline(ss, item, ' ')) {
            if (item.empty()) continue;
            auto item_cid = pj_ssl_cipher_id(item.c_str());
            if (item_cid != PJ_TLS_UNKNOWN_CIPHER) {
                WARN("Valid cipher: %s", item.c_str());
                ciphers_.push_back(item_cid);
            }
            else
                ERROR("Invalid cipher: %s", item.c_str());
        }
    }
#endif
    if (ciphers_.empty()) {
        ciphers_ = (tlsSetting_.method == PJSIP_TLSV1_METHOD) ? TLSv1_DEFAULT_CIPHER_LIST : (
                   (tlsSetting_.method == PJSIP_SSLV3_METHOD) ? SSLv3_DEFAULT_CIPHER_LIST : (
                   (tlsSetting_.method == PJSIP_SSLV23_METHOD) ? SSLv23_DEFAULT_CIPHER_LIST : CipherArray {} ));
    }
    ciphers_.erase(std::remove_if(ciphers_.begin(), ciphers_.end(), [&](pj_ssl_cipher c) {
        return std::find(avail_ciphers.cbegin(), avail_ciphers.cend(), c) == avail_ciphers.cend();
    }), ciphers_.end());

    trimCiphers();

    pj_cstr(&tlsSetting_.ca_list_file, tlsCaListFile_.c_str());
    pj_cstr(&tlsSetting_.cert_file, tlsCertificateFile_.c_str());
    pj_cstr(&tlsSetting_.privkey_file, tlsPrivateKeyFile_.c_str());
    pj_cstr(&tlsSetting_.password, tlsPassword_.c_str());

    DEBUG("Using %u ciphers", ciphers_.size());
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
SIPAccount::getContactHeader(pjsip_transport* t)
{
    if (!t && transport_)
        t = transport_->get();
    if (!t)
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

    link_->sipTransport->findLocalAddressFromTransport(
        t,
        transportType,
        hostname_,
        address, port);

    if (not publishedSameasLocal_) {
        address = publishedIpAddress_;
        port = publishedPort_;
        DEBUG("Using published address %s and port %d", address.c_str(), port);
    } else if (stunEnabled_) {
        link_->sipTransport->findLocalAddressFromSTUN(
            t,
            &stunServerName_,
            stunPort_,
            address, port);
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
    link_->sipTransport->findLocalAddressFromSTUN(
        transport_ ? transport_->get() : nullptr,
        &stunServerName_,
        stunPort_,
        addr, port);
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
        sipAccount->doRegister();
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
SIPAccount::setTransport(const std::shared_ptr<SipTransport>& t)
{
    if (transport_ == t)
        return;
    if (transport_ && regc_)
        pjsip_regc_release_transport(regc_);
    SIPAccountBase::setTransport(t);
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
    ip2ipAccountDetails[CONFIG_SRTP_ENABLE] = srtpEnabled_ ? TRUE_STR : FALSE_STR;
    ip2ipAccountDetails[CONFIG_SRTP_RTP_FALLBACK] = srtpFallback_ ? TRUE_STR : FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_DISPLAY_SAS] = zrtpDisplaySas_ ? TRUE_STR : FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_HELLO_HASH] = zrtpHelloHash_ ? TRUE_STR : FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_NOT_SUPP_WARNING] = zrtpNotSuppWarning_ ? TRUE_STR : FALSE_STR;
    ip2ipAccountDetails[CONFIG_ZRTP_DISPLAY_SAS_ONCE] = zrtpDisplaySasOnce_ ? TRUE_STR : FALSE_STR;
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
    tlsSettings[CONFIG_TLS_ENABLE] = tlsEnable_ ? TRUE_STR : FALSE_STR;
    tlsSettings[CONFIG_TLS_CA_LIST_FILE] = tlsCaListFile_;
    tlsSettings[CONFIG_TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    tlsSettings[CONFIG_TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    tlsSettings[CONFIG_TLS_PASSWORD] = tlsPassword_;
    tlsSettings[CONFIG_TLS_METHOD] = tlsMethod_;
    tlsSettings[CONFIG_TLS_CIPHERS] = tlsCiphers_;
    tlsSettings[CONFIG_TLS_SERVER_NAME] = tlsServerName_;
    tlsSettings[CONFIG_TLS_VERIFY_SERVER] = tlsVerifyServer_ ? TRUE_STR : FALSE_STR;
    tlsSettings[CONFIG_TLS_VERIFY_CLIENT] = tlsVerifyClient_ ? TRUE_STR : FALSE_STR;
    tlsSettings[CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE] = tlsRequireClientCertificate_ ? TRUE_STR : FALSE_STR;
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
        val = it->second == Account::TRUE_STR;
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
          enabled? TRUE_STR : FALSE_STR);

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
          enabled ? TRUE_STR : FALSE_STR);
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

    std::shared_ptr<SipTransport> tmp_tp {nullptr};
    if (contactRewriteMethod_ == 1) {
        /* Save transport in case we're gonna reuse it */
        tmp_tp = transport_;
        /* Unregister current contact */
        sendUnregister();
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
            tmp_tp = transport_;
            sendUnregister();
        } catch (const VoipLinkException &e) {
            ERROR("%s", e.what());
        }

        pjsip_regc_update_contact(regc_, 1, &contact_);

        /*  Perform new registration */
        try {
            sendRegister();
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
        acc->sendRegister();
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
