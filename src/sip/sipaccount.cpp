/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#ifdef ENABLE_VIDEO
#include "libav_utils.h"
#endif

#include "system_codec_container.h"

#include "upnp/upnp_control.h"
#include "ip_utils.h"
#include "string_utils.h"

#include "im/instant_messaging.h"

#include <opendht/crypto.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <lmcons.h>
#else
#include <pwd.h>
#endif

namespace jami {

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
        JAMI_ERR("registration callback parameter is null");
        return;
    }

    auto account = static_cast<SIPAccount *>(param->token);
    if (!account) {
        JAMI_ERR("account doesn't exist in registration callback");
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
    , ciphers_(100)
    , tlsMethod_("TLSv1")
    , tlsCiphers_()
    , tlsServerName_("")
    , tlsVerifyServer_(false)
    , tlsVerifyClient_(true)
    , tlsRequireClientCertificate_(true)
    , tlsNegotiationTimeoutSec_("2")
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
SIPAccount::newIncomingCall(const std::string& from UNUSED, const std::map<std::string, std::string>& details)
{
    auto& manager = Manager::instance();
    return manager.callFactory.newCall<SIPCall, SIPAccount>(*this,
                                                            manager.getNewCallID(),
                                                            Call::CallType::INCOMING,
                                                            details);
}

template <>
std::shared_ptr<SIPCall>
SIPAccount::newOutgoingCall(const std::string& toUrl,
                            const std::map<std::string, std::string>& volatileCallDetails)
{
    std::string to;
    int family;

    JAMI_DBG() << *this << "Calling SIP peer " << toUrl;

    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, SIPAccount>(*this, manager.getNewCallID(),
                                                                 Call::CallType::OUTGOING,
                                                                 volatileCallDetails);
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

        JAMI_DBG("New %s IP to IP call to %s", ipv6?"IPv6":"IPv4", to.c_str());
    }
    else {
        to = toUrl;
        call->setTransport(transport_);
        // FIXME : for now, use the same address family as the SIP transport
        family = pjsip_transport_type_get_af(getTransportType());

        JAMI_DBG("UserAgent: New registered account call to %s", toUrl.c_str());
    }

    auto toUri = getToUri(to);
    call->initIceMediaTransport(true);
    call->setIPToIP(isIP2IP());
    call->setPeerNumber(toUri);
    call->setPeerUri(toUri);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface(), family);

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
        getActiveAccountCodecInfoList(videoEnabled_ and not call->isAudioOnly() ? MEDIA_VIDEO : MEDIA_NONE),
        getSrtpKeyExchange()
    );

    if (created) {
        std::weak_ptr<SIPCall> weak_call = call;
        manager.scheduler().run([this, weak_call] {
            if (auto call = weak_call.lock()) {
                if (not SIPStartCall(call)) {
                    JAMI_ERR("Could not send outgoing INVITE request for new call");
                    call->onFailure();
                }
            }
            return false;
        });
    } else {
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
    }

    return call;
}

void
SIPAccount::onTransportStateChanged(pjsip_transport_state state, const pjsip_transport_state_info *info)
{
    pj_status_t currentStatus = transportStatus_;
    JAMI_DBG("Transport state changed to %s for account %s !", SipTransport::stateToStr(state), accountID_.c_str());
    if (!SipTransport::isAlive(transport_, state)) {
        if (info) {
            transportStatus_ = info->status;
            transportError_  = sip_utils::sip_strerror(info->status);
            JAMI_ERR("Transport disconnected: %s", transportError_.c_str());
        }
        else {
            // This is already the generic error used by pjsip.
            transportStatus_ = PJSIP_SC_SERVICE_UNAVAILABLE;
            transportError_  = "";
        }
        setRegistrationState(RegistrationState::ERROR_GENERIC, PJSIP_SC_TSX_TRANSPORT_ERROR);
        setTransport();
    }
    else {
        // The status can be '0', this is the same as OK
        transportStatus_ = info && info->status ? info->status : PJSIP_SC_OK;
        transportError_  = "";
    }

    // Notify the client of the new transport state
    if (currentStatus != transportStatus_)
        emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(accountID_, getVolatileAccountDetails());
}

void
SIPAccount::setTransport(const std::shared_ptr<SipTransport>& t)
{
    if (t == transport_)
        return;
    if (transport_) {
        JAMI_DBG("Removing transport from account");
        if (regc_)
            pjsip_regc_release_transport(regc_);
        transport_->removeStateListener(reinterpret_cast<uintptr_t>(this));
    }

    transport_ = t;

    if (transport_)
        transport_->addStateListener(reinterpret_cast<uintptr_t>(this), std::bind(&SIPAccount::onTransportStateChanged, this, std::placeholders::_1, std::placeholders::_2));
}

pjsip_tpselector
SIPAccount::getTransportSelector() {
    if (!transport_)
        return SIPVoIPLink::getTransportSelector(nullptr);
    return SIPVoIPLink::getTransportSelector(transport_->get());
}

std::shared_ptr<Call>
SIPAccount::newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails)
{
    return newOutgoingCall<SIPCall>(toUrl, volatileCallDetails);
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
        JAMI_ERR("Unable to start call without transport");
        return false;
    }

    pj_str_t pjContact = getContactHeader(transport->get());
    JAMI_DBG("contact header: %.*s / %s -> %s",
             (int)pjContact.slen, pjContact.ptr, from.c_str(), toUri.c_str());

    auto local_sdp = call->getSDP().getLocalSdpSession();
    pjsip_dialog* dialog {nullptr};
    pjsip_inv_session* inv {nullptr};
    if (!CreateClientDialogAndInvite(&pjFrom, &pjContact, &pjTo, nullptr, local_sdp, &dialog, &inv))
        return false;

    inv->mod_data[link_->getModId()] = call.get();
    call->inv.reset(inv);

    updateDialogViaSentBy(dialog);

    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));

    if (hasCredentials() and pjsip_auth_clt_set_credentials(&dialog->auth_sess, getCredentialCount(), getCredInfo()) != PJ_SUCCESS) {
        JAMI_ERR("Could not initialize credentials for invite session authentication");
        return false;
    }

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv.get(), &tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not initialize invite messager for this call");
        return false;
    }

    const pjsip_tpselector tp_sel = link_->getTransportSelector(transport->get());
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        JAMI_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("Unable to send invite message for this call");
        return false;
    }

    call->setState(Call::CallState::ACTIVE, Call::ConnectionState::PROGRESSING);

    return true;
}

void SIPAccount::serialize(YAML::Emitter &out) const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);

    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);

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
    SIPAccountBase::serializeTls(out);
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

void SIPAccount::usePublishedAddressPortInVIA()
{
    via_addr_.host.ptr = (char *) publishedIpAddress_.c_str();
    via_addr_.host.slen = publishedIpAddress_.size();
    via_addr_.port = publishedPort_;
}

void SIPAccount::useUPnPAddressPortInVIA()
{
    upnpIpAddr_ = getUPnPIpAddress().toString();
    via_addr_.host.ptr = (char *) upnpIpAddr_.c_str();
    via_addr_.host.slen = upnpIpAddr_.size();
    via_addr_.port = publishedPortUsed_;
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
        JAMI_ERR("Invalid parameter \"%s\"", param.c_str());
}

void SIPAccount::unserialize(const YAML::Node &node)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);

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
    parseValue(tlsMap, Conf::CERTIFICATE_KEY, tlsCertificateFile_);
    parseValue(tlsMap, Conf::CALIST_KEY, tlsCaListFile_);
    parseValue(tlsMap, Conf::TLS_PASSWORD_KEY, tlsPassword_);
    parseValue(tlsMap, Conf::PRIVATE_KEY_KEY, tlsPrivateKeyFile_);
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
    std::lock_guard<std::mutex> lock(configurationMutex_);

    SIPAccountBase::setAccountDetails(details);
    parseString(details, Conf::CONFIG_ACCOUNT_USERNAME, username_);

    parseInt(details, Conf::CONFIG_LOCAL_PORT, localPort_);

    // TLS
    parseString(details, Conf::CONFIG_TLS_CA_LIST_FILE, tlsCaListFile_);
    parseString(details, Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_);
    parseString(details, Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_);
    parseString(details, Conf::CONFIG_TLS_PASSWORD, tlsPassword_);

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
        JAMI_WARN("No credentials set, inferring them...");
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
    std::lock_guard<std::mutex> lock(configurationMutex_);

    auto a = SIPAccountBase::getAccountDetails();

    std::string password {};
    if (hasCredentials()) {
        for (const auto &cred : credentials_)
            if (cred.username == username_) {
                password = cred.password;
                break;
            }
    }
    a.emplace(Conf::CONFIG_ACCOUNT_PASSWORD,                std::move(password));

    a.emplace(Conf::CONFIG_LOCAL_PORT,                      std::to_string(localPort_));
    a.emplace(Conf::CONFIG_ACCOUNT_ROUTESET,                serviceRoute_);
    a.emplace(Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE,     std::to_string(registrationExpire_));
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
    auto a = SIPAccountBase::getVolatileAccountDetails();
    a.emplace(Conf::CONFIG_ACCOUNT_REGISTRATION_STATE_CODE, std::to_string(registrationStateDetailed_.first));
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
            JAMI_WARN("Unknown cipher: %d", tlsInfos.cipher);
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
        a.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM,    std::to_string(n));
    }

    return a;
}

bool SIPAccount::mapPortUPnP()
{
    // return true if not using UPnP
    bool added = true;

    if (getUPnPActive()) {
        /* create port mapping from published port to local port to the local IP
         * note that since different accounts can use the same port,
         * it may already be open, thats OK
         *
         * if the desired port is taken by another client, then it will try to map
         * a different port, if succesfull, then we have to use that port for SIP
         */
        uint16_t port_used;
        bool added = upnp_->addAnyMapping(publishedPort_, localPort_, jami::upnp::PortType::UDP, false, false, &port_used);
        if (added) {
            if (port_used != publishedPort_)
                JAMI_DBG("UPnP could not map published port %u for SIP, using %u instead", publishedPort_, port_used);
            publishedPortUsed_ = port_used;
        }
    }

    upnp_->setIGDListener([w=weak()] {
        if (auto acc = w.lock())
            acc->doRegister();
    });

    return added;
}

void SIPAccount::doRegister()
{
    std::unique_lock<std::mutex> lock(configurationMutex_);
    if (not isUsable()) {
        JAMI_WARN("Account must be enabled and active to register, ignoring");
        return;
    }

    JAMI_DBG("doRegister %s", hostname_.c_str());

    /* if UPnP is enabled, then wait for IGD to complete registration */
    if (upnp_) {
        JAMI_DBG("UPnP: waiting for IGD to register SIP account");
        lock.unlock();
        setRegistrationState(RegistrationState::TRYING);
        std::thread{ [w=weak()] {
            if (auto acc = w.lock()) {
                sip_utils::register_thread();
                if (not acc->mapPortUPnP())
                    JAMI_WARN("UPnP: Could not successfully map SIP port with UPnP, continuing with account registration anyways.");
                acc->doRegister1_();
            }
        }}.detach();
    } else {
        lock.unlock();
        doRegister1_();
    }
}

void SIPAccount::doRegister1_()
{
    {
        std::lock_guard<std::mutex> lock(configurationMutex_);
        if (isIP2IP()) {
            doRegister2_();
            return;
        }
    }

    link_->resolveSrvName(
        hostname_,
        tlsEnable_ ? PJSIP_TRANSPORT_TLS : PJSIP_TRANSPORT_UDP,
        [w = weak()](std::vector<IpAddr> host_ips) {
            if (auto acc = w.lock()) {
                std::lock_guard<std::mutex> lock(acc->configurationMutex_);
                if (host_ips.empty()) {
                    JAMI_ERR("Can't resolve hostname for registration.");
                    acc->setRegistrationState(RegistrationState::ERROR_GENERIC, PJSIP_SC_NOT_FOUND);
                    return;
                }
                acc->hostIp_ = host_ips[0];
                acc->doRegister2_();
            }
        }
    );
}

void SIPAccount::doRegister2_()
{
    bool ipv6 = false;
    if (isIP2IP()) {
        JAMI_DBG("doRegister isIP2IP.");
#if HAVE_IPV6
        ipv6 = ip_utils::getInterfaceAddr(interface_).isIpv6();
#endif
    } else if (!hostIp_) {
        setRegistrationState(RegistrationState::ERROR_GENERIC, PJSIP_SC_NOT_FOUND);
        JAMI_ERR("Hostname not resolved.");
        return;
    }
#if HAVE_IPV6
    else
        ipv6 = hostIp_.isIpv6();
#endif

    // Init TLS settings if the user wants to use TLS
    if (tlsEnable_) {
        JAMI_DBG("TLS is enabled for account %s", accountID_.c_str());

        // Dropping current calls already using the transport is currently required
        // with TLS.
        freeAccount();

        transportType_ = ipv6 ? PJSIP_TRANSPORT_TLS6 : PJSIP_TRANSPORT_TLS;
        initTlsConfiguration();

        if (!tlsListener_) {
            tlsListener_ = link_->sipTransportBroker->getTlsListener(
                SipTransportDescr {getTransportType(), getTlsListenerPort(), getLocalInterface()},
                getTlsSetting());
            if (!tlsListener_) {
                setRegistrationState(RegistrationState::ERROR_GENERIC);
                JAMI_ERR("Error creating TLS listener.");
                return;
            }
        }
    } else {
        tlsListener_.reset();
        transportType_ = ipv6 ? PJSIP_TRANSPORT_UDP6 : PJSIP_TRANSPORT_UDP;
    }

    // Init STUN settings for this account if the user selected it
    if (stunEnabled_)
        initStunConfiguration();
    else
        stunServerName_ = pj_str((char*) stunServer_.c_str());

    // In our definition of the ip2ip profile (aka Direct IP Calls),
    // no registration should be performed
    if (isIP2IP()) {
        // If we use Tls for IP2IP, transports will be created on connection.
        if (!tlsEnable_)
            setTransport(link_->sipTransportBroker->getUdpTransport(
                SipTransportDescr { getTransportType(), getLocalPort(), getLocalInterface() }
            ));
        setRegistrationState(RegistrationState::REGISTERED);
        return;
    }

    try {
        JAMI_WARN("Creating transport");
        transport_.reset();
        if (isTlsEnabled()) {
            setTransport(link_->sipTransportBroker->getTlsTransport(tlsListener_, hostIp_, tlsServerName_.empty() ? hostname_ : tlsServerName_));
        } else {
            setTransport(link_->sipTransportBroker->getUdpTransport(
                SipTransportDescr { getTransportType(), getLocalPort(), getLocalInterface() }
            ));
        }
        if (!transport_)
            throw VoipLinkException("Can't create transport");

        sendRegister();
    } catch (const VoipLinkException &e) {
        JAMI_ERR("%s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
        return;
    }

    if (presence_ and presence_->isEnabled()) {
        presence_->subscribeClient(getFromUri(), true); // self presence subscription
        presence_->sendPresence(true, ""); // try to publish whatever the status is.
    }
}

void SIPAccount::doUnregister(std::function<void(bool)> released_cb)
{
    std::unique_lock<std::mutex> lock(configurationMutex_);

    tlsListener_.reset();
    if (transport_)
        setTransport();

    if (!isIP2IP()) {
        try {
            sendUnregister();
        } catch (const VoipLinkException& e) {
            JAMI_ERR("doUnregister %s", e.what());
        }
    }

    lock.unlock();
    if (released_cb)
        released_cb(not isIP2IP());

    if (upnp_) {
        upnp_->setIGDListener();
        upnp_->removeMappings();
    }
}

void
SIPAccount::connectivityChanged()
{
    if (not isUsable()) {
        // nothing to do
        return;
    }

    doUnregister([acc=shared()](bool /* transport_free */) {
        if (acc->isUsable())
            acc->doRegister();
    });
}

void SIPAccount::startKeepAliveTimer()
{
    if (isTlsEnabled())
        return;

    if (isIP2IP())
        return;

    if (keepAliveTimerActive_)
        return;

    JAMI_DBG("Start keep alive timer for account %s", getAccountID().c_str());

    // make sure here we have an entirely new timer
    memset(&keepAliveTimer_, 0, sizeof(pj_timer_entry));

    pj_time_val keepAliveDelay_;
    keepAliveTimer_.cb = &SIPAccount::keepAliveRegistrationCb;
    keepAliveTimer_.user_data = this;
    keepAliveTimer_.id = timerIdDist_(rand);

    // expiration may be undetermined during the first registration request
    if (registrationExpire_ == 0) {
        JAMI_DBG("Registration Expire: 0, taking 60 instead");
        keepAliveDelay_.sec = 3600;
    } else {
        JAMI_DBG("Registration Expire: %d", registrationExpire_);
        keepAliveDelay_.sec = registrationExpire_ + MIN_REGISTRATION_TIME;
    }

    keepAliveDelay_.msec = 0;

    keepAliveTimerActive_ = true;

    link_->registerKeepAliveTimer(keepAliveTimer_, keepAliveDelay_);
}

void SIPAccount::stopKeepAliveTimer()
{
    if (keepAliveTimerActive_) {
        JAMI_DBG("Stop keep alive timer %d for account %s", keepAliveTimer_.id, getAccountID().c_str());
        keepAliveTimerActive_ = false;
        link_->cancelKeepAliveTimer(keepAliveTimer_);
    }
}

void
SIPAccount::sendRegister()
{
    if (not isUsable()) {
        JAMI_WARN("Account must be enabled and active to register, ignoring");
        return;
    }

    setRegister(true);
    setRegistrationState(RegistrationState::TRYING);

    pjsip_regc *regc = nullptr;
    if (pjsip_regc_create(link_->getEndpoint(), (void *) this, &registration_cb, &regc) != PJ_SUCCESS)
        throw VoipLinkException("UserAgent: Unable to create regc structure.");

    std::string srvUri(getServerUri());
    pj_str_t pjSrv {(char*) srvUri.data(), (pj_ssize_t) srvUri.size()};

    // Generate the FROM header
    std::string from(getFromUri());
    pj_str_t pjFrom {(char*) from.data(), (pj_ssize_t) from.size()};

    // Get the received header
    std::string received(getReceivedParameter());

    // Get the contact header
    const pj_str_t pjContact(getContactHeader());

    if (transport_) {
        if (getUPnPActive() or not getPublishedSameasLocal() or (not received.empty() and received != getPublishedAddress())) {
            pjsip_host_port *via = getViaAddr();
            JAMI_DBG("Setting VIA sent-by to %.*s:%d", (int)via->host.slen, via->host.ptr, via->port);

            if (pjsip_regc_set_via_sent_by(regc, via, transport_->get()) != PJ_SUCCESS)
                throw VoipLinkException("Unable to set the \"sent-by\" field");
        } else if (isStunEnabled()) {
            if (pjsip_regc_set_via_sent_by(regc, getViaAddr(), transport_->get()) != PJ_SUCCESS)
                throw VoipLinkException("Unable to set the \"sent-by\" field");
        }
    }

    pj_status_t status;

    //JAMI_DBG("pjsip_regc_init from:%s, srv:%s, contact:%s", from.c_str(), srvUri.c_str(), std::string(pj_strbuf(&pjContact), pj_strlen(&pjContact)).c_str());
    if ((status = pjsip_regc_init(regc, &pjSrv, &pjFrom, &pjFrom, 1, &pjContact, getRegistrationExpire())) != PJ_SUCCESS) {
        JAMI_ERR("pjsip_regc_init failed with error %d: %s", status,
                 sip_utils::sip_strerror(status).c_str());
        throw VoipLinkException("Unable to initialize account registration structure");
    }

    if (hasServiceRoute())
        pjsip_regc_set_route_set(regc, sip_utils::createRouteSet(getServiceRoute(), link_->getPool()));

    pjsip_regc_set_credentials(regc, getCredentialCount(), getCredInfo());

    pjsip_hdr hdr_list;
    pj_list_init(&hdr_list);
    std::string useragent(getUserAgentName());
    pj_str_t pJuseragent {(char*) useragent.data(), (pj_ssize_t) useragent.size()};
    constexpr pj_str_t STR_USER_AGENT = CONST_PJ_STR("User-Agent");

    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create(link_->getPool(), &STR_USER_AGENT, &pJuseragent);
    pj_list_push_back(&hdr_list, (pjsip_hdr*) h);
    pjsip_regc_add_headers(regc, &hdr_list);

    pjsip_tx_data *tdata;
    if (pjsip_regc_register(regc, PJ_TRUE, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to initialize transaction data for account registration");

    const pjsip_tpselector tp_sel = getTransportSelector();
    if (pjsip_regc_set_transport(regc, &tp_sel) != PJ_SUCCESS)
        throw VoipLinkException("Unable to set transport");

    if (hostIp_) {
        auto ai = &tdata->dest_info;
        ai->name = pj_strdup3(tdata->pool, hostname_.c_str());
        ai->addr.count = 1;
        ai->addr.entry[0].type = (pjsip_transport_type_e)tp_sel.u.transport->key.type;
        pj_memcpy(&ai->addr.entry[0].addr, hostIp_.pjPtr(), sizeof(pj_sockaddr));
        ai->addr.entry[0].addr_len = hostIp_.getLength();
        ai->cur_addr = 0;
    }

    // pjsip_regc_send increment the transport ref count by one,
    if ((status = pjsip_regc_send(regc, tdata)) != PJ_SUCCESS) {
        JAMI_ERR("pjsip_regc_init failed with error %d: %s", status,
                 sip_utils::sip_strerror(status).c_str());
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
        JAMI_ERR("SIP registration error %d", param->status);
        destroyRegistrationInfo();
        stopKeepAliveTimer();
        setRegistrationState(RegistrationState::ERROR_GENERIC, param->code);
    } else if (param->code < 0 || param->code >= 300) {
        JAMI_ERR("SIP registration failed, status=%d (%.*s)",
              param->code, (int)param->reason.slen, param->reason.ptr);
        destroyRegistrationInfo();
        stopKeepAliveTimer();
        switch (param->code) {
            case PJSIP_SC_FORBIDDEN:
                setRegistrationState(RegistrationState::ERROR_AUTH, param->code);
                break;
            case PJSIP_SC_NOT_FOUND:
                setRegistrationState(RegistrationState::ERROR_HOST, param->code);
                break;
            case PJSIP_SC_REQUEST_TIMEOUT:
                setRegistrationState(RegistrationState::ERROR_HOST, param->code);
                break;
            case PJSIP_SC_SERVICE_UNAVAILABLE:
                setRegistrationState(RegistrationState::ERROR_SERVICE_UNAVAILABLE, param->code);
                break;
            default:
                setRegistrationState(RegistrationState::ERROR_GENERIC, param->code);
        }
    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {

        // Update auto registration flag
        resetAutoRegistration();

        if (param->expiration < 1) {
            destroyRegistrationInfo();
            /* Stop keep-alive timer if any. */
            stopKeepAliveTimer();
            JAMI_DBG("Unregistration success");
            setRegistrationState(RegistrationState::UNREGISTERED, param->code);
        } else {
            /* TODO Check and update SIP outbound status first, since the result
             * will determine if we should update re-registration
             */
            // update_rfc5626_status(acc, param->rdata);

            if (checkNATAddress(param, link_->getPool()))
                JAMI_WARN("Contact overwritten");

            /* TODO Check and update Service-Route header */
            if (hasServiceRoute())
                pjsip_regc_set_route_set(param->regc, sip_utils::createRouteSet(getServiceRoute(), link_->getPool()));

            // start the periodic registration request based on Expire header
            // account determines itself if a keep alive is required
            if (isKeepAliveEnabled())
                startKeepAliveTimer();

            setRegistrationState(RegistrationState::REGISTERED, param->code);
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
            scheduleReregistration();
            break;

        default:
            /* Global failure */
            if (PJSIP_IS_STATUS_IN_CLASS(param->code, 600))
                scheduleReregistration();
    }
    setRegistrationExpire(param->expiration);
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
    setRegister(false);

    pjsip_regc *regc = getRegistrationInfo();
    if (!regc)
        throw VoipLinkException("Registration structure is NULL");

    pjsip_tx_data *tdata = nullptr;
    if (pjsip_regc_unregister(regc, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to unregister sip account");

    pj_status_t status;
    if ((status = pjsip_regc_send(regc, tdata)) != PJ_SUCCESS) {
        JAMI_ERR("pjsip_regc_send failed with error %d: %s", status,
                 sip_utils::sip_strerror(status).c_str());
        throw VoipLinkException("Unable to send request to unregister sip account");
    }
}

pj_uint32_t
SIPAccount::tlsProtocolFromString(const std::string& method)
{
    if (method == "Default")
        return PJSIP_SSL_DEFAULT_PROTO;
    if (method == "TLSv1.2")
        return PJ_SSL_SOCK_PROTO_TLS1_2;
    if (method == "TLSv1.1")
        return PJ_SSL_SOCK_PROTO_TLS1_2 | PJ_SSL_SOCK_PROTO_TLS1_1;
    if (method == "TLSv1")
        return PJ_SSL_SOCK_PROTO_TLS1_2 | PJ_SSL_SOCK_PROTO_TLS1_1 | PJ_SSL_SOCK_PROTO_TLS1;
    return PJSIP_SSL_DEFAULT_PROTO;
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
    tlsSetting_.proto = tlsProtocolFromString(tlsMethod_);

    // Determine the cipher list supported on this machine
    CipherArray avail_ciphers(256);
    unsigned cipherNum = avail_ciphers.size();
    if (pj_ssl_cipher_get_availables(&avail_ciphers.front(), &cipherNum) != PJ_SUCCESS)
        JAMI_ERR("Could not determine cipher list on this system");
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
                JAMI_WARN("Valid cipher: %s", item.c_str());
                ciphers_.push_back(item_cid);
            }
            else
                JAMI_ERR("Invalid cipher: %s", item.c_str());
        }
    }
#endif
    ciphers_.erase(std::remove_if(ciphers_.begin(), ciphers_.end(), [&](pj_ssl_cipher c) {
        return std::find(avail_ciphers.cbegin(), avail_ciphers.cend(), c) == avail_ciphers.cend();
    }), ciphers_.end());

    trimCiphers();

    pj_strset(&tlsSetting_.ca_list_file, (char*)tlsCaListFile_.c_str(), tlsCaListFile_.size());
    pj_strset(&tlsSetting_.cert_file, (char*)tlsCertificateFile_.c_str(), tlsCertificateFile_.size());
    pj_strset(&tlsSetting_.privkey_file, (char*)tlsPrivateKeyFile_.c_str(), tlsPrivateKeyFile_.size());
    pj_strset(&tlsSetting_.password, (char*)tlsPassword_.c_str(), tlsPassword_.size());

    JAMI_DBG("Using %zu ciphers", ciphers_.size());
    tlsSetting_.ciphers_num = ciphers_.size();
    tlsSetting_.ciphers = &ciphers_.front();

    tlsSetting_.verify_server = tlsVerifyServer_;
    tlsSetting_.verify_client = tlsVerifyClient_;
    tlsSetting_.require_client_cert = tlsRequireClientCertificate_;

    tlsSetting_.timeout.sec = atol(tlsNegotiationTimeoutSec_.c_str());

    tlsSetting_.qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    tlsSetting_.qos_ignore_error = PJ_TRUE;
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
        registrationExpire_ = DEFAULT_REGISTRATION_TIME; /** Default expire value for registration */

    if (tlsEnable_) {
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
    if (hostname == hostname_)
        return true;
    const auto a = ip_utils::getAddrList(hostname);
    const auto b = ip_utils::getAddrList(hostname_);
    return ip_utils::haveCommonAddr(a, b);
}

bool SIPAccount::proxyMatch(const std::string& hostname) const
{
    if (hostname == serviceRoute_)
        return true;
    const auto a = ip_utils::getAddrList(hostname);
    const auto b = ip_utils::getAddrList(hostname_);
    return ip_utils::haveCommonAddr(a, b);
}

std::string SIPAccount::getLoginName()
{
#ifndef _WIN32
    struct passwd * user_info = getpwuid(getuid());
    return user_info ? user_info->pw_name : "";
#elif defined (RING_UWP)
    return "Unknown";
#else
    TCHAR username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    std::string uname;
    if (GetUserName((TCHAR*)username, &size)) {
#ifdef _MSC_VER
        wchar_t* tmpstr = new wchar_t[UNLEN + 1];
        mbstowcs(tmpstr, username, UNLEN + 1);
        std::wstring wStr = tmpstr;
#else
        std::wstring wStr = username;
#endif
        uname = std::string(wStr.begin(), wStr.end());
    }
    return uname;
#endif
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

    const std::string uri = "<" + scheme + username + "@" + hostname + transport + ">";
    if (not displayName_.empty())
        return "\"" + displayName_ + "\" " + uri;
    return uri;
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
    if (username.find('@') == std::string::npos)
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
    if (contact_.slen and contactOverwritten_)
        return contact_;

    if (!t && transport_)
        t = transport_->get();
    if (!t) {
        JAMI_ERR("Transport not created yet");
        return {nullptr, 0};
    }

    // The transport type must be specified, in our case START_OTHER refers to stun transport
    pjsip_transport_type_e transportType = transportType_;

    if (transportType == PJSIP_TRANSPORT_START_OTHER)
        transportType = PJSIP_TRANSPORT_UDP;

    // Else we determine this infor based on transport information
    std::string address;
    pj_uint16_t port;

    link_->findLocalAddressFromTransport(
        t,
        transportType,
        hostname_,
        address, port);

    if (getUPnPActive() and getUPnPIpAddress()) {
        address = getUPnPIpAddress().toString();
        port = publishedPortUsed_;
        useUPnPAddressPortInVIA();
        JAMI_DBG("Using UPnP address %s and port %d", address.c_str(), port);
    } else if (not publishedSameasLocal_) {
        address = publishedIpAddress_;
        port = publishedPort_;
        JAMI_DBG("Using published address %s and port %d", address.c_str(), port);
    } else if (stunEnabled_) {
        auto success = link_->findLocalAddressFromSTUN(t, &stunServerName_,
                                                       stunPort_, address, port);
        if (not success)
            emitSignal<DRing::ConfigurationSignal::StunStatusFailed>(getAccountID());
        setPublishedAddress(address);
        publishedPort_ = port;
        usePublishedAddressPortInVIA();
    } else {
        if (!receivedParameter_.empty()) {
            address = receivedParameter_;
            JAMI_DBG("Using received address %s", address.c_str());
        }

        if (rPort_ != -1 and rPort_ != 0) {
            port = rPort_;
            JAMI_DBG("Using received port %d", port);
        }
    }

#if HAVE_IPV6
    /* Enclose IPv6 address in square brackets */
    if (IpAddr::isIpv6(address)) {
        address = IpAddr(address).toString(false, true);
    }
#endif

    const char* scheme = "sip";
    const char* transport = "";
    if (PJSIP_TRANSPORT_IS_SECURE(t)) {
        scheme = "sips";
        transport = ";transport=tls";
    }

    std::string quotedDisplayName = "\"" + displayName_ + "\" " + (displayName_.empty() ? "" : " ");
    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "%s<%s:%s%s%s:%d%s>",
                                     quotedDisplayName.c_str(),
                                     scheme,
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.c_str(),
                                     port,
                                     transport);
    return contact_;
}

pjsip_host_port
SIPAccount::getHostPortFromSTUN(pj_pool_t *pool)
{
    std::string addr;
    pj_uint16_t port;
    auto success = link_->findLocalAddressFromSTUN(
        transport_ ? transport_->get() : nullptr, &stunServerName_, stunPort_,
        addr, port);
    if (not success)
        emitSignal<DRing::ConfigurationSignal::StunStatusFailed>(getAccountID());
    pjsip_host_port result;
    pj_strdup2(pool, &result.host, addr.c_str());
    result.host.slen = addr.length();
    result.port = port;
    return result;
}

const std::vector<std::string>&
SIPAccount::getSupportedTlsCiphers()
{
    //Currently, both OpenSSL and GNUTLS implementations are static
    //reloading this for each account is unnecessary
    static std::vector<std::string> availCiphers {};

    // LIMITATION Assume the size might change, if there aren't any ciphers,
    // this will cause the cache to be repopulated at each call for nothing.
    if (availCiphers.empty()) {
        unsigned cipherNum = 256;
        CipherArray avail_ciphers(cipherNum);
        if (pj_ssl_cipher_get_availables(&avail_ciphers.front(), &cipherNum) != PJ_SUCCESS)
            JAMI_ERR("Could not determine cipher list on this system");
        avail_ciphers.resize(cipherNum);
        availCiphers.reserve(cipherNum);
        for (const auto &item : avail_ciphers) {
            if (item > 0) // 0 doesn't have a name
                availCiphers.push_back(pj_ssl_cipher_name(item));
        }
    }
    return availCiphers;
}

const std::vector<std::string>&
SIPAccount::getSupportedTlsProtocols()
{
    static std::vector<std::string> availProtos {VALID_TLS_PROTOS, VALID_TLS_PROTOS + arraySize(VALID_TLS_PROTOS)};
    return availProtos;
}

void SIPAccount::keepAliveRegistrationCb(UNUSED pj_timer_heap_t *th, pj_timer_entry *te)
{
    SIPAccount *sipAccount = static_cast<SIPAccount *>(te->user_data);

    if (sipAccount == nullptr) {
        JAMI_ERR("SIP account is nullptr while registering a new keep alive timer");
        return;
    }

    JAMI_ERR("Keep alive registration callback for account %s", sipAccount->getAccountID().c_str());

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

void
SIPAccount::Credentials::computePasswordHash()
{
    pj_md5_context pms;

    /* Compute md5 hash = MD5(username ":" realm ":" password) */
    pj_md5_init(&pms);
    pj_md5_update(&pms, (const uint8_t*)username.data(), username.length());
    pj_md5_update(&pms, (const uint8_t*)":", 1);
    pj_md5_update(&pms, (const uint8_t*)realm.data(), realm.length());
    pj_md5_update(&pms, (const uint8_t*)":", 1);
    pj_md5_update(&pms, (const uint8_t*)password.data(), password.length());

    unsigned char digest[16];
    pj_md5_final(&pms, digest);

    char hash[32];

    for (int i = 0; i < 16; ++i)
        pj_val_to_hex_digit(digest[i], &hash[2 * i]);

    password_h = {hash, 32};
}

void SIPAccount::setCredentials(const std::vector<std::map<std::string, std::string> >& creds)
{
    if (creds.empty()) {
        JAMI_ERR("Cannot authenticate with empty credentials list");
        return;
    }
    credentials_.clear();
    cred_.clear();

    bool md5HashingEnabled = Manager::instance().preferences.getMd5Hash();

    credentials_.reserve(creds.size());
    cred_.reserve(creds.size());
    for (const auto& cred : creds) {
        auto realm = cred.find(Conf::CONFIG_ACCOUNT_REALM);
        auto user = cred.find(Conf::CONFIG_ACCOUNT_USERNAME);
        auto passw = cred.find(Conf::CONFIG_ACCOUNT_PASSWORD);
        credentials_.emplace_back(realm != cred.end() ? realm->second : "",
                                  user  != cred.end() ? user->second  : "",
                                  passw != cred.end() ? passw->second : "");
        auto& c = credentials_.back();
        if (md5HashingEnabled)
            c.computePasswordHash();

        cred_.emplace_back(pjsip_cred_info {
            /*.realm     = */pj_str((char*) c.realm.c_str()),
            /*.scheme    = */pj_str((char*) "digest"),
            /*.username  = */pj_str((char*) c.username.c_str()),
            /*.data_type = */(c.password_h.empty()
                           ? PJSIP_CRED_DATA_PLAIN_PASSWD
                           : PJSIP_CRED_DATA_DIGEST),
            /*.data      = */pj_str((char*) (c.password_h.empty()
                           ? c.password.c_str()
                           : c.password_h.c_str())),
            /*.ext       = */{}
        });
    }
}

std::vector<std::map<std::string, std::string>>
SIPAccount::getCredentials() const
{
    std::vector<std::map<std::string, std::string>> ret;
    ret.reserve(credentials_.size());
    for (const auto& c : credentials_) {
        ret.emplace_back(std::map<std::string, std::string>{
            {Conf::CONFIG_ACCOUNT_REALM,    c.realm},
            {Conf::CONFIG_ACCOUNT_USERNAME, c.username},
            {Conf::CONFIG_ACCOUNT_PASSWORD, c.password}
        });
    }
    return ret;
}

void
SIPAccount::setRegistrationState(RegistrationState state, unsigned details_code,
                                 const std::string& /*detail_str*/)
{
    std::string details_str;
    const pj_str_t *description = pjsip_get_status_text(details_code);
    if (description)
        details_str = {description->ptr, (size_t)description->slen};
    setRegistrationStateDetailed({details_code, details_str});
    SIPAccountBase::setRegistrationState(state, details_code, details_str);
}

std::string SIPAccount::getUserAgentName() const
{
    if (not hasCustomUserAgent_ or userAgent_.empty())
        return DEFAULT_USER_AGENT;
    return userAgent_;
}

std::map<std::string, std::string>
SIPAccount::getTlsSettings() const
{
    return {
        {Conf::CONFIG_TLS_ENABLE,           tlsEnable_ ? TRUE_STR : FALSE_STR},
        {Conf::CONFIG_TLS_LISTENER_PORT,    std::to_string(tlsListenerPort_)},
        {Conf::CONFIG_TLS_CA_LIST_FILE,     tlsCaListFile_},
        {Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_},
        {Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_},
        {Conf::CONFIG_TLS_PASSWORD,         tlsPassword_},
        {Conf::CONFIG_TLS_METHOD,           tlsMethod_},
        {Conf::CONFIG_TLS_CIPHERS,          tlsCiphers_},
        {Conf::CONFIG_TLS_SERVER_NAME,      tlsServerName_},
        {Conf::CONFIG_TLS_VERIFY_SERVER,    tlsVerifyServer_ ? TRUE_STR : FALSE_STR},
        {Conf::CONFIG_TLS_VERIFY_CLIENT,    tlsVerifyClient_ ? TRUE_STR : FALSE_STR},
        {Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate_ ? TRUE_STR : FALSE_STR},
        {Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC,    tlsNegotiationTimeoutSec_}
    };
}

bool SIPAccount::isIP2IP() const
{
    return hostname_.empty();
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
        JAMI_ERR("Presence not initialized");
        return;
    }

    JAMI_DBG("Presence enabled for %s : %s.",
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
        JAMI_ERR("Presence not initialized");
        return;
    }

    if (presence_->isSupported(function) == enabled)
        return;

    JAMI_DBG("Presence support for %s (%s: %s).", accountID_.c_str(),
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

MatchRank
SIPAccount::matches(const std::string &userName, const std::string &server) const
{
    if (fullMatch(userName, server)) {
        JAMI_DBG("Matching account id in request is a fullmatch %s@%s", userName.c_str(), server.c_str());
        return MatchRank::FULL;
    } else if (hostnameMatch(server)) {
        JAMI_DBG("Matching account id in request with hostname %s", server.c_str());
        return MatchRank::PARTIAL;
    } else if (userMatch(userName)) {
        JAMI_DBG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::PARTIAL;
    } else if (proxyMatch(server)) {
        JAMI_DBG("Matching account id in request with proxy %s", server.c_str());
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

    JAMI_WARN("IP address change detected for account %s "
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
        const char* scheme = "sip";
        const char* transport_param = "";
        if (PJSIP_TRANSPORT_IS_SECURE(tp)) {
            scheme = "sips";
            transport_param = ";transport=tls";
        }

        char* tmp = (char*) pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
        int len = pj_ansi_snprintf(tmp, PJSIP_MAX_URL_SIZE,
                "<%s:%s%s%s:%d%s>",
                scheme,
                username_.c_str(),
                (not username_.empty() ?  "@" : ""),
                via_addrstr.c_str(),
                rport,
                transport_param);
        if (len < 1) {
            JAMI_ERR("URI too long");
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
            JAMI_ERR("%s", e.what());
        }

        // sendUnregister may failed and cause regc_ to be reset to nullptr
        // in this case re-registration has been scheduled, so just leave
        if (!regc_)
            return true;
        pjsip_regc_update_contact(regc_, 1, &contact_);

        /*  Perform new registration */
        try {
            sendRegister();
        } catch (const VoipLinkException &e) {
            JAMI_ERR("%s", e.what());
        }
    }

    return true;
}

/* Auto re-registration timeout callback */
void
SIPAccount::autoReregTimerCb()
{
    /* Check if the re-registration timer is still valid, e.g: while waiting
     * timeout timer application might have deleted the account or disabled
     * the auto-reregistration.
     */
    if (not auto_rereg_.active)
        return;

    /* Start re-registration */
    ++auto_rereg_.attempt_cnt;
    try {
        sendRegister();
    } catch (const VoipLinkException& e) {
        JAMI_ERR("Exception during SIP registration: %s", e.what());
        scheduleReregistration();
    }
}

/* Schedule reregistration for specified account. Note that the first
 * re-registration after a registration failure will be done immediately.
 * Also note that this function should be called within PJSUA mutex.
 */
void
SIPAccount::scheduleReregistration()
{
    if (!isUsable())
        return;

    /* Cancel any re-registration timer */
    if (auto_rereg_.timer.id) {
        auto_rereg_.timer.id = PJ_FALSE;
        pjsip_endpt_cancel_timer(link_->getEndpoint(), &auto_rereg_.timer);
    }

    /* Update re-registration flag */
    auto_rereg_.active = PJ_TRUE;

    /* Set up timer for reregistration */
    auto_rereg_.timer.cb = [](pj_timer_heap_t* /*th*/, pj_timer_entry* te) {
        static_cast<SIPAccount*>(te->user_data)->autoReregTimerCb();
    };
    auto_rereg_.timer.user_data = this;

    /* Reregistration attempt. The first attempt will be done sooner */
    pj_time_val delay;
    delay.sec = auto_rereg_.attempt_cnt ? REGISTRATION_RETRY_INTERVAL : REGISTRATION_FIRST_RETRY_INTERVAL;
    delay.msec = 0;

    /* Randomize interval by +/- 10 secs */
    if (delay.sec >= 10) {
        delay.msec = delay10ZeroDist_(rand);
    } else {
        delay.sec = 0;
        delay.msec = delay10PosDist_(rand);
    }

    pj_time_val_normalize(&delay);

    JAMI_WARN("Scheduling re-registration retry in %ld seconds..", delay.sec);
    auto_rereg_.timer.id = PJ_TRUE;
    if (pjsip_endpt_schedule_timer(link_->getEndpoint(), &auto_rereg_.timer, &delay) != PJ_SUCCESS)
        auto_rereg_.timer.id = PJ_FALSE;
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
        JAMI_WARN("No sender or payload");
        if (messageEngine_) messageEngine_->onMessageSent(to, id, false);
        return;
    }

    auto toUri = getToUri(to);

    constexpr pjsip_method msg_method = {PJSIP_OTHER_METHOD, CONST_PJ_STR("MESSAGE")};
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());
    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    /* Create request. */
    pjsip_tx_data *tdata;
    pj_status_t status = pjsip_endpt_create_request(link_->getEndpoint(), &msg_method,
                                                    &pjTo, &pjFrom, &pjTo, nullptr, nullptr, -1,
                                                    nullptr, &tdata);
    if (status != PJ_SUCCESS) {
        JAMI_ERR("Unable to create request: %s", sip_utils::sip_strerror(status).c_str());
        if (messageEngine_) messageEngine_->onMessageSent(to, id, false);
        return;
    }

    const pjsip_tpselector tp_sel = getTransportSelector();
    pjsip_tx_data_set_transport(tdata, &tp_sel);

    im::fillPJSIPMessageBody(*tdata, payloads);

    struct ctx {
        std::weak_ptr<SIPAccount> acc;
        std::string to;
        uint64_t id;
    };
    ctx* t = new ctx;
    t->acc = shared();
    t->to = to;
    t->id = id;

    status = pjsip_endpt_send_request(link_->getEndpoint(), tdata, -1, t, [](void *token, pjsip_event *e) {
        auto c = (ctx*) token;
        try {
            if (auto acc = c->acc.lock()) {
                if (acc->messageEngine_) acc->messageEngine_->onMessageSent(c->to, c->id, e
                                                      && e->body.tsx_state.tsx
                                                      && e->body.tsx_state.tsx->status_code == PJSIP_SC_OK);
            }
        } catch (const std::exception& e) {
            JAMI_ERR("Error calling message callback: %s", e.what());
        }
        delete c;
    });

    if (status != PJ_SUCCESS) {
        JAMI_ERR("Unable to send request: %s", sip_utils::sip_strerror(status).c_str());
        return;
    }
}

std::string
SIPAccount::getUserUri() const
{
    return getFromUri();
}

} // namespace jami
