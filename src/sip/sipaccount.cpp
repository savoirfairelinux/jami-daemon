/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 */

#include "sipaccount.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler_intrinsics.h"

#include "sdp.h"
#include "sipvoiplink.h"
#include "sipcall.h"
#include "sip_utils.h"
#include "array_size.h"

#include "call_factory.h"

#include "sippresence.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include "account_schema.h"
#include "config/yamlparser.h"
#include "logger.h"
#include "manager.h"
#include "client/ring_signal.h"
#include "dring/account_const.h"

#ifdef RING_VIDEO
#include "libav_utils.h"
#endif

#include "system_codec_container.h"

#include "upnp/upnp_control.h"
#include "ip_utils.h"
#include "string_utils.h"

#include "im/instant_messaging.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
#include <lmcons.h>
#else
#include <pwd.h>
#endif

namespace ring {

using yaml_utils::parseValue;
using yaml_utils::parseVectorMap;
using sip_utils::CONST_PJ_STR;

static constexpr int MIN_REGISTRATION_TIME = 60;  // seconds
static constexpr unsigned DEFAULT_REGISTRATION_TIME = 3600;  // seconds
static constexpr unsigned REGISTRATION_FIRST_RETRY_INTERVAL = 60; // seconds
static constexpr unsigned REGISTRATION_RETRY_INTERVAL = 300; // seconds
static const char *const VALID_TLS_PROTOS[] = {"Default", "TLSv1.2", "TLSv1.1", "TLSv1"};

constexpr const char * const SIPAccount::ACCOUNT_TYPE;

static void
registration_cb(pjsip_regc_cbparam *param)
{
    if (!param) {
        RING_ERR("registration callback parameter is null");
        return;
    }

    auto account = static_cast<SIPAccount *>(param->token);
    if (!account) {
        RING_ERR("account doesn't exist in registration callback");
        return;
    }

    account->onRegister(param);
}

SIPAccount::SIPAccount(const std::string& accountID, bool presenceEnabled)
    : SIPAccountDB(accountID)
    , presence_(presenceEnabled ? new SIPPresence(this) : nullptr)
{
    via_addr_.host.ptr = 0;
    via_addr_.host.slen = 0;
    via_addr_.port = 0;
}

SIPAccount::~SIPAccount()
{
    // ensure that no registration callbacks survive past this point
    destroyRegistrationInfo();
    setTransport();

    delete presence_;
}

std::shared_ptr<SIPCall>
SIPAccount::newIncomingCall(const std::string& from UNUSED)
{
    auto& manager = Manager::instance();
    return manager.callFactory.newCall<SIPCall, SIPAccount>(*this, manager.getNewCallID(), Call::CallType::INCOMING);
}

template <>
std::shared_ptr<SIPCall>
SIPAccount::newOutgoingCall(const std::string& toUrl)
{
    std::string to;
    int family;

    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, SIPAccount>(*this, manager.getNewCallID(), Call::CallType::OUTGOING);
    call->setSecure(isTlsEnabled());

    if (isIP2IP()) {
        bool ipv6 = false;
#if HAVE_IPV6
        ipv6 = IpAddr::isIpv6(toUrl);
#endif
        to = ipv6 ? IpAddr(toUrl).toString(false, true) : toUrl;
        family = ipv6 ? pj_AF_INET6() : pj_AF_INET();

        // TODO: resolve remote host using SIPVoIPLink::resolveSrvName
        std::shared_ptr<SipTransport> t = isTlsEnabled() ?
            link_->sipTransportBroker->getTlsTransport(tlsListener_, IpAddr(sip_utils::getHostFromUri(to))) :
            transport_;
        setTransport(t);
        call->setTransport(t);

        RING_DBG("New %s IP to IP call to %s", ipv6?"IPv6":"IPv4", to.c_str());
    }
    else {
        to = toUrl;
        call->setTransport(transport_);
        // FIXME : for now, use the same address family as the SIP transport
        family = pjsip_transport_type_get_af(getTransportType());

        RING_DBG("UserAgent: New registered account call to %s", toUrl.c_str());
    }

    auto toUri = getToUri(to);
    call->initIceTransport(true);
    call->setIPToIP(isIP2IP());
    call->setPeerNumber(toUri);
    call->initRecFilename(to);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface(), family);
    call->setCallMediaLocal(localAddress);

    IpAddr addrSdp;
    if (getUPnPActive()) {
        /* use UPnP addr, or published addr if its set */
        addrSdp = getPublishedSameasLocal() ?
            getUPnPIpAddress() : getPublishedIpAddress();
    } else {
        addrSdp = isStunEnabled() or (not getPublishedSameasLocal()) ?
            getPublishedIpAddress() : localAddress;
    }

    /* fallback on local address */
    if (not addrSdp) addrSdp = localAddress;

    // Building the local SDP offer
    auto& sdp = call->getSDP();

    if (getPublishedSameasLocal())
        sdp.setPublishedIP(addrSdp);
    else
        sdp.setPublishedIP(getPublishedAddress());

    const bool created = sdp.createOffer(
        getActiveAccountCodecInfoList(MEDIA_AUDIO),
        getActiveAccountCodecInfoList(videoEnabled_ ? MEDIA_VIDEO : MEDIA_NONE),
        getSrtpKeyExchange()
    );

    if (created) {
        auto shared_this = std::static_pointer_cast<SIPAccount>(shared_from_this());
        std::weak_ptr<SIPCall> weak_call = call;
        manager.addTask([shared_this, weak_call] {
            auto call = weak_call.lock();

            if (not call or not shared_this->SIPStartCall(call)) {
                RING_ERR("Could not send outgoing INVITE request for new call");
                call->onFailure();
            }
            return false;
        });
    } else {
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
    }

    return call;
}

std::shared_ptr<Call>
SIPAccount::newOutgoingCall(const std::string& toUrl)
{
    return newOutgoingCall<SIPCall>(toUrl);
}

bool
SIPAccount::SIPStartCall(std::shared_ptr<SIPCall>& call)
{
    // Add Ice headers to local SDP if ice transport exist
    call->setupLocalSDPFromIce();

    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    auto transport = call->getTransport();
    if (!transport) {
        RING_ERR("Unable to start call without transport");
        return false;
    }

    pj_str_t pjContact = getContactHeader(transport->get());
    RING_DBG("contact header: %.*s / %s -> %s",
             (int)pjContact.slen, pjContact.ptr, from.c_str(), toUri.c_str());

    pjsip_dialog *dialog = NULL;

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog) != PJ_SUCCESS) {
        RING_ERR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    auto subj_hdr_name = CONST_PJ_STR("Subject");
    pjsip_hdr* subj_hdr = (pjsip_hdr*) pjsip_parse_hdr(dialog->pool, &subj_hdr_name, (char *) "Phone call", 10, NULL);

    pj_list_push_back(&dialog->inv_hdr, subj_hdr);

    pjsip_inv_session* inv = nullptr;
    if (pjsip_inv_create_uac(dialog, call->getSDP().getLocalSdpSession(), 0, &inv) != PJ_SUCCESS) {
        RING_ERR("Unable to create invite session for user agent client");
        return false;
    }

    if (!inv) {
        RING_ERR("Call invite is not initialized");
        return PJ_FALSE;
    }

    pjsip_dlg_inc_lock(inv->dlg);
    inv->mod_data[link_->getModId()] = call.get();
    call->inv.reset(inv);

    updateDialogViaSentBy(dialog);

    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));

    if (hasCredentials() and pjsip_auth_clt_set_credentials(&dialog->auth_sess, getCredentialCount(), getCredInfo()) != PJ_SUCCESS) {
        RING_ERR("Could not initialize credentials for invite session authentication");
        return false;
    }

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv.get(), &tdata) != PJ_SUCCESS) {
        RING_ERR("Could not initialize invite messager for this call");
        return false;
    }

    const pjsip_tpselector tp_sel = link_->getTransportSelector(transport->get());
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        RING_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        RING_ERR("Unable to send invite message for this call");
        return false;
    }

    call->setState(Call::CallState::ACTIVE, Call::ConnectionState::PROGRESSING);

    return true;
}

void SIPAccount::serialize(YAML::Emitter &out)
{
    out << YAML::BeginMap;
    SIPAccountDB::serialize(out);

    out << YAML::Key << Conf::PORT_KEY << YAML::Value << localPort_;

    out << YAML::Key << USERNAME_KEY << YAML::Value << username_;

    // each credential is a map, and we can have multiple credentials
    out << YAML::Key << Conf::CRED_KEY << YAML::Value << getCredentials();
    out << YAML::Key << Conf::KEEP_ALIVE_ENABLED << YAML::Value << keepAliveEnabled_;

    out << YAML::Key << PRESENCE_MODULE_ENABLED_KEY << YAML::Value << (presence_ and presence_->isEnabled());
    out << YAML::Key << Conf::PRESENCE_PUBLISH_SUPPORTED_KEY << YAML::Value << (presence_ and presence_->isSupported(PRESENCE_FUNCTION_PUBLISH));
    out << YAML::Key << Conf::PRESENCE_SUBSCRIBE_SUPPORTED_KEY << YAML::Value << (presence_ and presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE));

    out << YAML::Key << Preferences::REGISTRATION_EXPIRE_KEY << YAML::Value << registrationExpire_;
    out << YAML::Key << Conf::SERVICE_ROUTE_KEY << YAML::Value << serviceRoute_;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountDB::serializeTls(out);
    out << YAML::Key << Conf::TLS_ENABLE_KEY << YAML::Value << tlsEnable_;
    out << YAML::Key << Conf::TLS_PORT_KEY << YAML::Value << tlsListenerPort_;
    out << YAML::Key << Conf::VERIFY_CLIENT_KEY << YAML::Value << tlsVerifyClient_;
    out << YAML::Key << Conf::VERIFY_SERVER_KEY << YAML::Value << tlsVerifyServer_;
    out << YAML::Key << Conf::REQUIRE_CERTIF_KEY << YAML::Value << tlsRequireClientCertificate_;
    out << YAML::Key << Conf::TIMEOUT_KEY << YAML::Value << tlsNegotiationTimeoutSec_;
    out << YAML::Key << Conf::CIPHERS_KEY << YAML::Value << tlsCiphers_;
    out << YAML::Key << Conf::METHOD_KEY << YAML::Value << tlsMethod_;
    out << YAML::Key << Conf::SERVER_KEY << YAML::Value << tlsServerName_;
    out << YAML::EndMap;

    // srtp submap
    out << YAML::Key << Conf::SRTP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << Conf::KEY_EXCHANGE_KEY << YAML::Value << sip_utils::getKeyExchangeName(srtpKeyExchange_);
    out << YAML::Key << Conf::RTP_FALLBACK_KEY << YAML::Value << srtpFallback_;
    out << YAML::EndMap;

    out << YAML::EndMap;
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
        RING_ERR("Invalid parameter \"%s\"", param.c_str());
}

void SIPAccount::unserialize(const YAML::Node &node)
{
    SIPAccountBase::unserialize(node);
    parseValue(node, USERNAME_KEY, username_);

    if (not publishedSameasLocal_)
        usePublishedAddressPortInVIA();

    int port = sip_utils::DEFAULT_SIP_PORT;
    parseValue(node, Conf::PORT_KEY, port);
    localPort_ = port;

    if (not isIP2IP()) {
        parseValue(node, Preferences::REGISTRATION_EXPIRE_KEY, registrationExpire_);
        parseValue(node, Conf::KEEP_ALIVE_ENABLED, keepAliveEnabled_);
        parseValue(node, Conf::SERVICE_ROUTE_KEY, serviceRoute_);
        const auto& credsNode = node[Conf::CRED_KEY];
        setCredentials(parseVectorMap(credsNode, {
            Conf::CONFIG_ACCOUNT_REALM,
            Conf::CONFIG_ACCOUNT_USERNAME,
            Conf::CONFIG_ACCOUNT_PASSWORD
        }));
    }

    bool presEnabled = false;
    parseValue(node, PRESENCE_MODULE_ENABLED_KEY, presEnabled);
    enablePresence(presEnabled);
    bool publishSupported = false;
    parseValue(node, Conf::PRESENCE_PUBLISH_SUPPORTED_KEY, publishSupported);
    bool subscribeSupported = false;
    parseValue(node, Conf::PRESENCE_SUBSCRIBE_SUPPORTED_KEY, subscribeSupported);
    if (presence_) {
        presence_->support(PRESENCE_FUNCTION_PUBLISH, publishSupported);
        presence_->support(PRESENCE_FUNCTION_SUBSCRIBE, subscribeSupported);
    }

    // Init stun server name with default server name
    stunServerName_ = pj_str((char*) stunServer_.data());

    const auto &credsNode = node[Conf::CRED_KEY];
    setCredentials(parseVectorMap(credsNode, {
        Conf::CONFIG_ACCOUNT_REALM,
        Conf::CONFIG_ACCOUNT_USERNAME,
        Conf::CONFIG_ACCOUNT_PASSWORD
    }));

    // get tls submap
    const auto &tlsMap = node[Conf::TLS_KEY];

    parseValue(tlsMap, Conf::TLS_ENABLE_KEY, tlsEnable_);
    parseValue(tlsMap, Conf::TLS_PORT_KEY, tlsListenerPort_);
    parseValue(tlsMap, Conf::CIPHERS_KEY, tlsCiphers_);

    std::string tmpMethod(tlsMethod_);
    parseValue(tlsMap, Conf::METHOD_KEY, tmpMethod);
    validate(tlsMethod_, tmpMethod, VALID_TLS_PROTOS);

    parseValue(tlsMap, Conf::SERVER_KEY, tlsServerName_);
    parseValue(tlsMap, Conf::REQUIRE_CERTIF_KEY, tlsRequireClientCertificate_);
    parseValue(tlsMap, Conf::VERIFY_CLIENT_KEY, tlsVerifyClient_);
    parseValue(tlsMap, Conf::VERIFY_SERVER_KEY, tlsVerifyServer_);
    // FIXME
    parseValue(tlsMap, Conf::TIMEOUT_KEY, tlsNegotiationTimeoutSec_);

    // get srtp submap
    const auto &srtpMap = node[Conf::SRTP_KEY];
    std::string tmpKey;
    parseValue(srtpMap, Conf::KEY_EXCHANGE_KEY, tmpKey);
    srtpKeyExchange_ = sip_utils::getKeyExchangeProtocol(tmpKey.c_str());
    parseValue(srtpMap, Conf::RTP_FALLBACK_KEY, srtpFallback_);
}

void SIPAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    SIPAccountBase::setAccountDetails(details);
    parseString(details, Conf::CONFIG_ACCOUNT_USERNAME, username_);

    parseInt(details, Conf::CONFIG_LOCAL_PORT, localPort_);

    // SIP specific account settings
    parseString(details, Conf::CONFIG_ACCOUNT_ROUTESET, serviceRoute_);

    if (not publishedSameasLocal_)
        usePublishedAddressPortInVIA();

    parseInt(details, Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire_);

    if (registrationExpire_ < MIN_REGISTRATION_TIME)
        registrationExpire_ = MIN_REGISTRATION_TIME;

    parseBool(details, Conf::CONFIG_KEEP_ALIVE_ENABLED, keepAliveEnabled_);
    bool presenceEnabled = false;
    parseBool(details, Conf::CONFIG_PRESENCE_ENABLED, presenceEnabled);
    enablePresence(presenceEnabled);

    // TLS settings
    parseBool(details, Conf::CONFIG_TLS_ENABLE, tlsEnable_);
    parseInt(details, Conf::CONFIG_TLS_LISTENER_PORT, tlsListenerPort_);
    auto iter = details.find(Conf::CONFIG_TLS_METHOD);
    if (iter != details.end())
        validate(tlsMethod_, iter->second, VALID_TLS_PROTOS);
    parseString(details, Conf::CONFIG_TLS_CIPHERS, tlsCiphers_);
    parseString(details, Conf::CONFIG_TLS_SERVER_NAME, tlsServerName_);
    parseBool(details, Conf::CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer_);
    parseBool(details, Conf::CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient_);
    parseBool(details, Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    parseString(details, Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);
    parseBool(details, Conf::CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer_);
    parseBool(details, Conf::CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient_);
    parseBool(details, Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_);
    parseString(details, Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec_);

    // srtp settings
    parseBool(details, Conf::CONFIG_SRTP_RTP_FALLBACK, srtpFallback_);
    iter = details.find(Conf::CONFIG_SRTP_KEY_EXCHANGE);
    if (iter != details.end())
        srtpKeyExchange_ = sip_utils::getKeyExchangeProtocol(iter->second.c_str());

    if (credentials_.empty()) { // credentials not set, construct 1 entry
        RING_WARN("No credentials set, inferring them...");
        std::vector<std::map<std::string, std::string> > v;
        std::map<std::string, std::string> map;
        map[Conf::CONFIG_ACCOUNT_USERNAME] = username_;
        parseString(details, Conf::CONFIG_ACCOUNT_PASSWORD, map[Conf::CONFIG_ACCOUNT_PASSWORD]);
        map[Conf::CONFIG_ACCOUNT_REALM] = "*";
        v.push_back(map);
        setCredentials(v);
    }
}

std::map<std::string, std::string>
SIPAccount::getAccountDetails() const
{
    auto a = SIPAccountDB::getAccountDetails();

    std::string password {};
    if (hasCredentials()) {
        for (const auto &cred : credentials_)
            if (cred.username == username_) {
                password = cred.password;
                break;
            }
    }
    a.emplace(Conf::CONFIG_ACCOUNT_PASSWORD,                std::move(password));

    a.emplace(Conf::CONFIG_LOCAL_PORT,                      ring::to_string(localPort_));
    a.emplace(Conf::CONFIG_ACCOUNT_ROUTESET,                serviceRoute_);
    a.emplace(Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE,     ring::to_string(registrationExpire_));
    a.emplace(Conf::CONFIG_KEEP_ALIVE_ENABLED,              keepAliveEnabled_ ? TRUE_STR : FALSE_STR);

    a.emplace(Conf::CONFIG_PRESENCE_ENABLED,                presence_ and presence_->isEnabled()? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_PRESENCE_PUBLISH_SUPPORTED,      presence_ and presence_->isSupported(PRESENCE_FUNCTION_PUBLISH)? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED,    presence_ and presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE)? TRUE_STR : FALSE_STR);

    auto tlsSettings(getTlsSettings());
    a.insert(tlsSettings.begin(), tlsSettings.end());

    a.emplace(Conf::CONFIG_SRTP_KEY_EXCHANGE,               sip_utils::getKeyExchangeName(srtpKeyExchange_));
    a.emplace(Conf::CONFIG_SRTP_ENABLE,                     isSrtpEnabled() ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_SRTP_RTP_FALLBACK,               srtpFallback_ ? TRUE_STR : FALSE_STR);

    return a;
}

std::map<std::string, std::string>
SIPAccount::getVolatileAccountDetails() const
{
    auto a = SIPAccountDB::getVolatileAccountDetails();
    a.emplace(Conf::CONFIG_ACCOUNT_REGISTRATION_STATE_CODE, ring::to_string(registrationStateDetailed_.first));
    a.emplace(Conf::CONFIG_ACCOUNT_REGISTRATION_STATE_DESC, registrationStateDetailed_.second);
    a.emplace(DRing::Account::VolatileProperties::InstantMessaging::OFF_CALL, TRUE_STR);

    if (presence_) {
        a.emplace(Conf::CONFIG_PRESENCE_STATUS,     presence_->isOnline() ? TRUE_STR : FALSE_STR);
        a.emplace(Conf::CONFIG_PRESENCE_NOTE,       presence_->getNote());
    }

    if (transport_ and transport_->isSecure() and transport_->isConnected()) {
        const auto& tlsInfos = transport_->getTlsInfos();
        auto cipher = pj_ssl_cipher_name(tlsInfos.cipher);
        if (tlsInfos.cipher and not cipher)
            RING_WARN("Unknown cipher: %d", tlsInfos.cipher);
        a.emplace(DRing::TlsTransport::TLS_CIPHER,         cipher ? cipher : "");
        a.emplace(DRing::TlsTransport::TLS_PEER_CERT,      tlsInfos.peerCert->toString());
        auto ca = tlsInfos.peerCert->issuer;
        unsigned n = 0;
        while (ca) {
            std::ostringstream name_str;
            name_str << DRing::TlsTransport::TLS_PEER_CA_ << n++;
            a.emplace(name_str.str(),                      ca->toString());
            ca = ca->issuer;
        }
        a.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM,    ring::to_string(n));
    }

    return a;
}

void SIPAccount::doRegisterIPToIP()
{
    SIPAccountDB::doRegisterIPToIP();

    if (presence_ and presence_->isEnabled()) {
        presence_->subscribeClient(getFromUri(), true); // self presence subscription
        presence_->sendPresence(true, ""); // try to publish whatever the status is.
    }
}

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
        RING_ERR("Presence not initialized");
        return;
    }

    RING_DBG("Presence enabled for %s : %s.",
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
        RING_ERR("Presence not initialized");
        return;
    }

    if (presence_->isSupported(function) == enabled)
        return;

    RING_DBG("Presence support for %s (%s: %s).", accountID_.c_str(),
          function == PRESENCE_FUNCTION_PUBLISH ? "publish" : "subscribe",
          enabled ? TRUE_STR : FALSE_STR);
    presence_->support(function, enabled);

    // force presence to disable when nothing is supported
    if (not presence_->isSupported(PRESENCE_FUNCTION_PUBLISH) and
        not presence_->isSupported(PRESENCE_FUNCTION_SUBSCRIBE))
        enablePresence(false);

    Manager::instance().saveConfig();
    // FIXME: bad signal used here, we need a global config changed signal.
    emitSignal<DRing::ConfigurationSignal::AccountsChanged>();
}

void SIPAccount::updateDialogViaSentBy(pjsip_dialog *dlg)
{
    if (allowViaRewrite_ && via_addr_.host.slen > 0)
        pjsip_dlg_set_via_sent_by(dlg, &via_addr_, via_tp_);
}

#if 0
/**
 * Create Accept header for MESSAGE.
 */
static pjsip_accept_hdr* im_create_accept(pj_pool_t *pool)
{
    /* Create Accept header. */
    pjsip_accept_hdr *accept;

    accept = pjsip_accept_hdr_create(pool);
    accept->values[0] = CONST_PJ_STR("text/plain");
    accept->values[1] = CONST_PJ_STR("application/im-iscomposing+xml");
    accept->count = 2;

    return accept;
}
#endif

void
SIPAccount::sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads, uint64_t id)
{
    if (to.empty() or payloads.empty()) {
        RING_WARN("No sender or payload");
        messageEngine_.onMessageSent(id, false);
        return;
    }

    auto toUri = getToUri(to);

    const pjsip_method msg_method = {PJSIP_OTHER_METHOD, CONST_PJ_STR("MESSAGE")};
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());
    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    /* Create request. */
    pjsip_tx_data *tdata;
    pj_status_t status = pjsip_endpt_create_request(link_->getEndpoint(), &msg_method,
                                                    &pjTo, &pjFrom, &pjTo, nullptr, nullptr, -1,
                                                    nullptr, &tdata);
    if (status != PJ_SUCCESS) {
        RING_ERR("Unable to create request: %s", sip_utils::sip_strerror(status).c_str());
        messageEngine_.onMessageSent(id, false);
        return;
    }

    const pjsip_tpselector tp_sel = getTransportSelector();
    pjsip_tx_data_set_transport(tdata, &tp_sel);

    im::fillPJSIPMessageBody(*tdata, payloads);

    struct ctx {
        std::weak_ptr<SIPAccount> acc;
        uint64_t id;
    };
    ctx* t = new ctx;
    t->acc = std::static_pointer_cast<SIPAccount>(shared_from_this());
    t->id = id;

    status = pjsip_endpt_send_request(link_->getEndpoint(), tdata, -1, t, [](void *token, pjsip_event *e) {
        auto c = (ctx*) token;
        try {
            if (auto acc = c->acc.lock()) {
                acc->messageEngine_.onMessageSent(c->id, e
                                                      && e->body.tsx_state.tsx
                                                      && e->body.tsx_state.tsx->status_code == PJSIP_SC_OK);
            }
        } catch (const std::exception& e) {
            RING_ERR("Error calling message callback: %s", e.what());
        }
        delete c;
    });

    if (status != PJ_SUCCESS) {
        RING_ERR("Unable to send request: %s", sip_utils::sip_strerror(status).c_str());
        return;
    }
}

} // namespace ring
