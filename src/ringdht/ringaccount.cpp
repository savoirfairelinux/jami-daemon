/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Simon Désaulniers <simon.desaulniers@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ringaccount.h"

#include "logger.h"

#include "accountarchive.h"
#include "ringcontact.h"
#include "configkeys.h"

#include "thread_pool.h"

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "sip/sip_utils.h"

#include "sips_transport_ice.h"
#include "ice_transport.h"

#include "p2p.h"

#include "client/ring_signal.h"
#include "dring/call_const.h"
#include "dring/account_const.h"

#include "upnp/upnp_control.h"
#include "system_codec_container.h"

#include "account_schema.h"
#include "manager.h"
#include "utf8_utils.h"

#ifdef RING_VIDEO
#include "libav_utils.h"
#endif
#include "fileutils.h"
#include "string_utils.h"
#include "array_size.h"
#include "archiver.h"

#include "config/yamlparser.h"
#include "security/certstore.h"
#include "libdevcrypto/Common.h"
#include "base64.h"

#include <yaml-cpp/yaml.h>
#include <json/json.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cinttypes>
#include <cstdarg>
#include <initializer_list>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>

namespace ring {

using sip_utils::CONST_PJ_STR;

namespace Migration {

enum class State { // Contains all the Migration states
    SUCCESS,
    INVALID
};

std::string
mapStateNumberToString(const State migrationState)
{
#define CASE_STATE(X) case Migration::State::X: \
                           return #X

    switch (migrationState) {
        CASE_STATE(INVALID);
        CASE_STATE(SUCCESS);
    }
    return {};
}

void
setState (const std::string& accountID,
          const State migrationState)
{
    emitSignal<DRing::ConfigurationSignal::MigrationEnded>(accountID,
        mapStateNumberToString(migrationState));
}

} // namespace ring::Migration

struct RingAccount::BuddyInfo
{
    /* the buddy id */
    dht::InfoHash id;

    /* number of devices connected on the DHT */
    uint32_t devices_cnt {};

    /* The disposable object to update buddy info */
    std::future<size_t> listenToken;

    BuddyInfo(dht::InfoHash id) : id(id) {}
};

struct RingAccount::PendingCall
{
    std::chrono::steady_clock::time_point start;
    std::shared_ptr<IceTransport> ice_sp;
    std::weak_ptr<SIPCall> call;
    std::future<size_t> listen_key;
    dht::InfoHash call_key;
    dht::InfoHash from;
    dht::InfoHash from_account;
    std::shared_ptr<dht::crypto::Certificate> from_cert;
};

struct RingAccount::PendingMessage
{
    dht::InfoHash to;
    std::chrono::steady_clock::time_point received;
};

struct
RingAccount::TrustRequest {
    dht::InfoHash device;
    time_t received;
    std::vector<uint8_t> payload;
    MSGPACK_DEFINE_MAP(device, received, payload)
};

/**
 * Represents a known device attached to this account
 */
struct RingAccount::KnownDevice
{
    /** Device certificate */
    std::shared_ptr<dht::crypto::Certificate> certificate;

    /** Device name */
    std::string name {};

    /** Time of last received device sync */
    time_point last_sync {time_point::min()};

    KnownDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                const std::string& n = {},
                time_point sync = time_point::min())
        : certificate(cert), name(n), last_sync(sync) {}
};

/**
 * Device announcement stored on DHT.
 */
struct RingAccount::DeviceAnnouncement : public dht::SignedValue<DeviceAnnouncement>
{
private:
    using BaseClass = dht::SignedValue<DeviceAnnouncement>;
public:
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    dht::InfoHash dev;
    MSGPACK_DEFINE_MAP(dev);
};

struct RingAccount::DeviceSync : public dht::EncryptedValue<DeviceSync>
{
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    uint64_t date;
    std::string device_name;
    std::map<dht::InfoHash, std::string> devices_known;
    std::map<dht::InfoHash, Contact> peers;
    std::map<dht::InfoHash, TrustRequest> trust_requests;
    MSGPACK_DEFINE_MAP(date, device_name, devices_known, peers, trust_requests)
};

static constexpr int ICE_COMPONENTS {1};
static constexpr int ICE_COMP_SIP_TRANSPORT {0};
static constexpr auto ICE_NEGOTIATION_TIMEOUT = std::chrono::seconds(60);
static constexpr auto TLS_TIMEOUT = std::chrono::seconds(30);
const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);

static constexpr const char * const RING_URI_PREFIX = "ring:";
static constexpr const char * DEFAULT_TURN_SERVER = "turn.jami.net";
static constexpr const char * DEFAULT_TURN_USERNAME = "ring";
static constexpr const char * DEFAULT_TURN_PWD = "ring";
static constexpr const char * DEFAULT_TURN_REALM = "ring";
static const auto PROXY_REGEX = std::regex("(https?://)?([\\w\\.]+)(:(\\d+)|:\\[(.+)-(.+)\\])?");

constexpr const char* const RingAccount::ACCOUNT_TYPE;
/* constexpr */ const std::pair<uint16_t, uint16_t> RingAccount::DHT_PORT_RANGE {4000, 8888};

using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

static const std::string
stripPrefix(const std::string& toUrl)
{
    auto dhtf = toUrl.find(RING_URI_PREFIX);
    if (dhtf != std::string::npos) {
        dhtf = dhtf+5;
    } else {
        dhtf = toUrl.find("sips:");
        dhtf = (dhtf == std::string::npos) ? 0 : dhtf+5;
    }
    while (dhtf < toUrl.length() && toUrl[dhtf] == '/')
        dhtf++;
    return toUrl.substr(dhtf);
}

static const std::string
parseRingUri(const std::string& toUrl)
{
    auto sufix = stripPrefix(toUrl);
    if (sufix.length() < 40)
        throw std::invalid_argument("id must be a ring infohash");

    const std::string toUri = sufix.substr(0, 40);
    if (std::find_if_not(toUri.cbegin(), toUri.cend(), ::isxdigit) != toUri.cend())
        throw std::invalid_argument("id must be a ring infohash");
    return toUri;
}

static constexpr const char*
dhtStatusStr(dht::NodeStatus status) {
    return status == dht::NodeStatus::Connected  ? "connected"  : (
           status == dht::NodeStatus::Connecting ? "connecting" :
                                                   "disconnected");
}

/**
 * Local ICE Transport factory helper
 *
 * RingAccount must use this helper than direct IceTranportFactory API
 */
template <class... Args>
std::shared_ptr<IceTransport>
RingAccount::createIceTransport(const Args&... args)
{
    auto ice = Manager::instance().getIceTransportFactory().createTransport(args...);
    if (!ice)
        throw std::runtime_error("ICE transport creation failed");

    return ice;
}

RingAccount::RingAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID)
#if HAVE_RINGNS
    , nameDir_(NameDirectory::instance())
#endif
    , idPath_(fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID())
    , cachePath_(fileutils::get_cache_dir()+DIR_SEPARATOR_STR+getAccountID())
    , dataPath_(cachePath_ + DIR_SEPARATOR_STR "values")
    , proxyEnabled_(false)
    , proxyServer_("")
    , deviceKey_("")
    , dhtPeerConnector_ {new DhtPeerConnector {*this}}
{
    // Force the SFL turn server if none provided yet
    turnServer_ = DEFAULT_TURN_SERVER;
    turnServerUserName_ = DEFAULT_TURN_USERNAME;
    turnServerPwd_ = DEFAULT_TURN_PWD;
    turnServerRealm_ = DEFAULT_TURN_REALM;
    turnEnabled_ = true;

    std::ifstream proxyCache(cachePath_ + DIR_SEPARATOR_STR "dhtproxy");
    if (proxyCache)
      std::getline(proxyCache, proxyServerCached_);
}

RingAccount::~RingAccount()
{
    if (eventHandler) {
        eventHandler->cancel();
        eventHandler.reset();
    }
    dht_.join();
}

void
RingAccount::flush()
{
    // Class base method
    SIPAccountBase::flush();

    fileutils::removeAll(dataPath_);
    fileutils::removeAll(cachePath_);
    fileutils::removeAll(idPath_);
}

std::shared_ptr<SIPCall>
RingAccount::newIncomingCall(const std::string& from, const std::map<std::string, std::string>& details)
{
    std::lock_guard<std::mutex> lock(callsMutex_);
    auto call_it = pendingSipCalls_.begin();
    while (call_it != pendingSipCalls_.end()) {
        auto call = call_it->call.lock();
        if (not call) {
            RING_WARN("newIncomingCall: discarding deleted call");
            call_it = pendingSipCalls_.erase(call_it);
        } else if (call->getPeerNumber() == from || (call_it->from_cert and
                                                     call_it->from_cert->issuer and
                                                     call_it->from_cert->issuer->getId().toString() == from)) {
            RING_DBG("newIncomingCall: found matching call for %s", from.c_str());
            pendingSipCalls_.erase(call_it);
            call->updateDetails(details);
            return call;
        } else {
            ++call_it;
        }
    }
    RING_ERR("newIncomingCall: can't find matching call for %s", from.c_str());
    return nullptr;
}

template <>
std::shared_ptr<SIPCall>
RingAccount::newOutgoingCall(const std::string& toUrl,
                             const std::map<std::string, std::string>& volatileCallDetails)
{
    auto suffix = stripPrefix(toUrl);
    RING_DBG() << *this << "Calling DHT peer " << suffix;
    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, RingAccount>(*this, manager.getNewCallID(),
                                                                  Call::CallType::OUTGOING,
                                                                  volatileCallDetails);

    call->setIPToIP(true);
    call->setSecure(isTlsEnabled());

    try {
        const std::string toUri = parseRingUri(suffix);
        startOutgoingCall(call, toUri);
    } catch (...) {
#if HAVE_RINGNS
        NameDirectory::lookupUri(suffix, nameServer_, [wthis_=weak(),call](const std::string& result,
                                                                   NameDirectory::Response response) {
            // we may run inside an unknown thread, but following code must be called in main thread
            runOnMainThread([=, &result]() {
                if (response != NameDirectory::Response::found) {
                    call->onFailure(EINVAL);
                    return;
                }
                if (auto sthis = wthis_.lock()) {
                    try {
                        const std::string toUri = parseRingUri(result);
                        sthis->startOutgoingCall(call, toUri);
                    } catch (...) {
                        call->onFailure(ENOENT);
                    }
                } else {
                    call->onFailure();
                }
            });
        });
#else
        call->onFailure(ENOENT);
#endif
    }

    return call;
}

void
RingAccount::startOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string toUri)
{
    // TODO: for now, we automatically trust all explicitly called peers
    setCertificateStatus(toUri, tls::TrustStore::PermissionStatus::ALLOWED);

    call->setPeerNumber(toUri + "@ring.dht");
    call->setPeerUri(RING_URI_PREFIX + toUri);
    call->setState(Call::ConnectionState::TRYING);
    std::weak_ptr<SIPCall> wCall = call;

#if HAVE_RINGNS
    nameDir_.get().lookupAddress(toUri, [wCall](const std::string& result, const NameDirectory::Response& response){
        if (response == NameDirectory::Response::found)
            if (auto call = wCall.lock()) {
                call->setPeerRegistredName(result);
                call->setPeerUri(RING_URI_PREFIX + result);
            }
    });
#endif

    // Find listening devices for this account
    dht::InfoHash peer_account(toUri);
    forEachDevice(peer_account, [wCall, toUri, peer_account](const std::shared_ptr<RingAccount>& sthis, const dht::InfoHash& dev)
    {
        auto call = wCall.lock();
        if (not call) return;
        RING_DBG("[call %s] calling device %s", call->getCallId().c_str(), dev.toString().c_str());

        auto& manager = Manager::instance();
        auto dev_call = manager.callFactory.newCall<SIPCall, RingAccount>(*sthis, manager.getNewCallID(),
                                                                          Call::CallType::OUTGOING,
                                                                          call->getDetails());
        std::weak_ptr<SIPCall> weak_dev_call = dev_call;
        dev_call->setIPToIP(true);
        dev_call->setSecure(sthis->isTlsEnabled());
        auto ice = sthis->createIceTransport(("sip:" + dev_call->getCallId()).c_str(),
                                             ICE_COMPONENTS, true, sthis->getIceOptions());
        if (not ice) {
            RING_WARN("Can't create ICE");
            dev_call->removeCall();
            return;
        }

        call->addSubCall(*dev_call);

        manager.addTask([sthis, weak_dev_call, ice, dev, toUri, peer_account] {
            auto call = weak_dev_call.lock();

            // call aborted?
            if (not call)
                return false;

            if (ice->isFailed()) {
                RING_ERR("[call:%s] ice init failed", call->getCallId().c_str());
                call->onFailure(EIO);
                return false;
            }

            // Loop until ICE transport is initialized.
            // Note: we suppose that ICE init routine has a an internal timeout (bounded in time)
            // and we let upper layers decide when the call shall be aborded (our first check upper).
            if (not ice->isInitialized())
                return true;

            sthis->registerDhtAddress(*ice);

            // Next step: sent the ICE data to peer through DHT
            const dht::Value::Id callvid  = ValueIdDist()(sthis->rand);
            const auto callkey = dht::InfoHash::get("callto:" + dev.toString());
            dht::Value val { dht::IceCandidates(callvid, ice->packIceMsg()) };

            sthis->dht_.putEncrypted(
                callkey, dev,
                std::move(val),
                [=](bool ok) { // Put complete callback
                    if (!ok) {
                        RING_WARN("Can't put ICE descriptor on DHT");
                        if (auto call = weak_dev_call.lock())
                            call->onFailure();
                    } else
                        RING_DBG("Successfully put ICE descriptor on DHT");
                }
            );

            auto listenKey = sthis->dht_.listen<dht::IceCandidates>(
                callkey,
                [weak_dev_call, ice, callvid, dev] (dht::IceCandidates&& msg) {
                    if (msg.id != callvid or msg.from != dev)
                        return true;
                    // remove unprintable characters
                    auto iceData = std::string(msg.ice_data.cbegin(), msg.ice_data.cend());
                    iceData.erase(std::remove_if(iceData.begin(), iceData.end(),
                                                 [](unsigned char c){ return !std::isprint(c) && !std::isspace(c); }
                                                ), iceData.end());
                    RING_WARN("ICE request replied from DHT peer %s\nData: %s", dev.toString().c_str(), iceData.c_str());
                    if (auto call = weak_dev_call.lock()) {
                        call->setState(Call::ConnectionState::PROGRESSING);
                        if (!ice->start(msg.ice_data)) {
                            call->onFailure();
                            return true;
                        }
                    }
                    return false;
                }
            );

            std::lock_guard<std::mutex> lock(sthis->callsMutex_);
            sthis->pendingCalls_.emplace_back(PendingCall{
                std::chrono::steady_clock::now(),
                ice, weak_dev_call,
                std::move(listenKey),
                callkey,
                dev,
                peer_account,
                tls::CertificateStore::instance().getCertificate(toUri)
            });
            sthis->checkPendingCallsTask();
            return false;
        });
    }, [wCall](const std::shared_ptr<RingAccount>&, bool ok){
        if (not ok) {
            if (auto call = wCall.lock()) {
                RING_WARN("[call:%s] no devices found", call->getCallId().c_str());
                call->onFailure(static_cast<int>(std::errc::no_such_device_or_address));
            }
        }
    });
}

void
RingAccount::onConnectedOutgoingCall(SIPCall& call, const std::string& to_id, IpAddr target)
{
    RING_DBG("[call:%s] outgoing call connected to %s", call.getCallId().c_str(), to_id.c_str());

    call.initIceMediaTransport(true);
    call.setIPToIP(true);
    call.setPeerNumber(getToUri(to_id+"@"+target.toString(true).c_str()));

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface());

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

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    if (!getSystemCodecContainer()->searchCodecByName("PCMA", ring::MEDIA_AUDIO))
        throw VoipLinkException("Could not instantiate codec for early media");

    // Building the local SDP offer
    auto& sdp = call.getSDP();

    sdp.setPublishedIP(addrSdp);
    const bool created = sdp.createOffer(
                            getActiveAccountCodecInfoList(MEDIA_AUDIO),
                            getActiveAccountCodecInfoList(videoEnabled_ and not call.isAudioOnly() ? MEDIA_VIDEO
                                                                                                   : MEDIA_NONE),
                            getSrtpKeyExchange()
                         );

    if (not created or not SIPStartCall(call, target))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
}

std::shared_ptr<Call>
RingAccount::newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails)
{
    return newOutgoingCall<SIPCall>(toUrl, volatileCallDetails);
}

bool
RingAccount::SIPStartCall(SIPCall& call, IpAddr target)
{
    call.setupLocalSDPFromIce();
    std::string toUri(call.getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    std::string targetStr = getToUri(target.toString(true)/*+";transport=ICE"*/);
    pj_str_t pjTarget = pj_str((char*) targetStr.c_str());

    pj_str_t pjContact;
    {
        auto transport = call.getTransport();
        pjContact = getContactHeader(transport ? transport->get() : nullptr);
    }

    RING_DBG("contact header: %.*s / %s -> %s / %.*s",
             (int)pjContact.slen, pjContact.ptr, from.c_str(), toUri.c_str(),
             (int)pjTarget.slen, pjTarget.ptr);


    auto local_sdp = call.getSDP().getLocalSdpSession();
    pjsip_dialog* dialog {nullptr};
    pjsip_inv_session* inv {nullptr};
    if (!CreateClientDialogAndInvite(&pjFrom, &pjContact, &pjTo, &pjTarget, local_sdp, &dialog, &inv))
        return false;

    inv->mod_data[link_->getModId()] = &call;
    call.inv.reset(inv);

/*
    updateDialogViaSentBy(dialog);
    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));
*/

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call.inv.get(), &tdata) != PJ_SUCCESS) {
        RING_ERR("Could not initialize invite messager for this call");
        return false;
    }

    //const pjsip_tpselector tp_sel = getTransportSelector();
    const pjsip_tpselector tp_sel = {PJSIP_TPSELECTOR_TRANSPORT, {call.getTransport()->get()}};
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        RING_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    RING_DBG("[call:%s] Sending SIP invite", call.getCallId().c_str());
    if (pjsip_inv_send_msg(call.inv.get(), tdata) != PJ_SUCCESS) {
        RING_ERR("Unable to send invite message for this call");
        return false;
    }

    call.setState(Call::CallState::ACTIVE, Call::ConnectionState::PROGRESSING);

    return true;
}

void RingAccount::serialize(YAML::Emitter &out)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);

    if (registrationState_ == RegistrationState::INITIALIZING)
        return;

    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << Conf::DHT_PORT_KEY << YAML::Value << dhtPort_;
    out << YAML::Key << Conf::DHT_PUBLIC_IN_CALLS << YAML::Value << dhtPublicInCalls_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_HISTORY << YAML::Value << allowPeersFromHistory_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_CONTACT << YAML::Value << allowPeersFromContact_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_TRUSTED << YAML::Value << allowPeersFromTrusted_;

    out << YAML::Key << Conf::PROXY_ENABLED_KEY << YAML::Value << proxyEnabled_;
    out << YAML::Key << Conf::PROXY_SERVER_KEY << YAML::Value << proxyServer_;
    out << YAML::Key << Conf::PROXY_PUSH_TOKEN_KEY << YAML::Value << deviceKey_;

#if HAVE_RINGNS
    out << YAML::Key << DRing::Account::ConfProperties::RingNS::URI << YAML::Value <<  nameServer_;
#endif

    out << YAML::Key << DRing::Account::ConfProperties::ARCHIVE_PATH << YAML::Value << archivePath_;
    out << YAML::Key << DRing::Account::ConfProperties::ARCHIVE_HAS_PASSWORD << YAML::Value << archiveHasPassword_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT << YAML::Value << receipt_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT_SIG << YAML::Value << YAML::Binary(receiptSignature_.data(), receiptSignature_.size());
    out << YAML::Key << DRing::Account::ConfProperties::RING_DEVICE_NAME << YAML::Value << ringDeviceName_;
    if (not registeredName_.empty())
        out << YAML::Key << DRing::Account::VolatileProperties::REGISTERED_NAME << YAML::Value << registeredName_;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void RingAccount::unserialize(const YAML::Node &node)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);

    using yaml_utils::parseValue;
    using yaml_utils::parsePath;

    SIPAccountBase::unserialize(node);

    // get tls submap
    const auto &tlsMap = node[Conf::TLS_KEY];
    parsePath(tlsMap, Conf::CERTIFICATE_KEY, tlsCertificateFile_, idPath_);
    parsePath(tlsMap, Conf::CALIST_KEY, tlsCaListFile_, idPath_);
    parseValue(tlsMap, Conf::TLS_PASSWORD_KEY, tlsPassword_);
    parsePath(tlsMap, Conf::PRIVATE_KEY_KEY, tlsPrivateKeyFile_, idPath_);

    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_HISTORY, allowPeersFromHistory_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_CONTACT, allowPeersFromContact_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_TRUSTED, allowPeersFromTrusted_);

    parseValue(node, Conf::PROXY_ENABLED_KEY, proxyEnabled_);
    parseValue(node, Conf::PROXY_SERVER_KEY, proxyServer_);
    parseValue(node, Conf::PROXY_PUSH_TOKEN_KEY, deviceKey_);

    try {
        parseValue(node, DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);
    } catch (const std::exception& e) {
        RING_WARN("can't read device name: %s", e.what());
    }
    if (registeredName_.empty()) {
        try {
            parseValue(node, DRing::Account::VolatileProperties::REGISTERED_NAME, registeredName_);
        } catch (const std::exception& e) {
            RING_WARN("can't read registered name: %s", e.what());
        }
    }

    try {
        parsePath(node, DRing::Account::ConfProperties::ARCHIVE_PATH, archivePath_, idPath_);
        parseValue(node, DRing::Account::ConfProperties::ARCHIVE_HAS_PASSWORD, archiveHasPassword_);
    } catch (const std::exception& e) {
        RING_WARN("can't read archive path: %s", e.what());
        archiveHasPassword_ = true;
    }

    try {
        parseValue(node, Conf::RING_ACCOUNT_RECEIPT, receipt_);
        auto receipt_sig = node[Conf::RING_ACCOUNT_RECEIPT_SIG].as<YAML::Binary>();
        receiptSignature_ = {receipt_sig.data(), receipt_sig.data()+receipt_sig.size()};
    } catch (const std::exception& e) {
        RING_WARN("can't read receipt: %s", e.what());
    }

    if (not dhtPort_)
        dhtPort_ = getRandomEvenPort(DHT_PORT_RANGE);
    dhtPortUsed_ = dhtPort_;

#if HAVE_RINGNS
    try {
        parseValue(node, DRing::Account::ConfProperties::RingNS::URI, nameServer_);
    } catch (const std::exception& e) {
        RING_WARN("can't read name server: %s", e.what());
    }
    nameDir_ = NameDirectory::instance(nameServer_);
#endif

    parseValue(node, Conf::DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);

    loadAccount();
}

void
RingAccount::createRingDevice(const dht::crypto::Identity& id)
{
    if (not id.second->isCA()) {
        RING_ERR("[Account %s] trying to sign a certificate with a non-CA.", getAccountID().c_str());
    }
    auto dev_id = dht::crypto::generateIdentity("Ring device", id);
    if (!dev_id.first || !dev_id.second) {
        throw VoipLinkException("Can't generate identity for this account.");
    }
    idPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + getAccountID();
    fileutils::check_dir(idPath_.c_str(), 0700);

    // save the chain including CA
    std::tie(tlsPrivateKeyFile_, tlsCertificateFile_) = saveIdentity(dev_id, idPath_, "ring_device");
    tlsPassword_ = {};
    identity_ = dev_id;
    accountTrust_ = dht::crypto::TrustList{};
    accountTrust_.add(*id.second);
    auto deviceId = dev_id.first->getPublicKey().getId();
    ringDeviceId_ = deviceId.toString();
    ringDeviceName_ = ip_utils::getDeviceName();
    if (ringDeviceName_.empty())
        ringDeviceName_ = ringDeviceId_.substr(8);
    knownDevices_.emplace(deviceId, KnownDevice{dev_id.second, ringDeviceName_, clock::now()});

    receipt_ = makeReceipt(id);
    receiptSignature_ = id.first->sign({receipt_.begin(), receipt_.end()});
    RING_WARN("[Account %s] created new device: %s (%s)",
              getAccountID().c_str(), ringDeviceId_.c_str(), ringDeviceName_.c_str());
}

void
RingAccount::initRingDevice(const AccountArchive& a)
{
    RING_WARN("[Account %s] creating new device from archive", getAccountID().c_str());
    SIPAccountBase::setAccountDetails(a.config);
    parseInt(a.config, Conf::CONFIG_DHT_PORT, dhtPort_);
    parseBool(a.config, Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);
    parseBool(a.config, DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY, allowPeersFromHistory_);
    parseBool(a.config, DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT, allowPeersFromContact_);
    parseBool(a.config, DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED, allowPeersFromTrusted_);
    ringAccountId_ = a.id.second->getId().toString();
    username_ = RING_URI_PREFIX+ringAccountId_;
    ethAccount_ = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();
    contacts_ = a.contacts;
    createRingDevice(a.id);
    saveContacts();
}

std::string
RingAccount::makeReceipt(const dht::crypto::Identity& id)
{
    RING_DBG("[Account %s] signing device receipt", getAccountID().c_str());
    DeviceAnnouncement announcement;
    announcement.dev = identity_.second->getId();
    dht::Value ann_val {announcement};
    ann_val.sign(*id.first);

    std::ostringstream is;
    is << "{\"id\":\"" << id.second->getId()
       << "\",\"dev\":\"" << identity_.second->getId()
       << "\",\"eth\":\"" << ethAccount_
       << "\",\"announce\":\"" << base64::encode(ann_val.getPacked()) << "\"}";

    announce_ = std::make_shared<dht::Value>(std::move(ann_val));
    return is.str();
}

bool
RingAccount::useIdentity(const dht::crypto::Identity& identity)
{
    if (receipt_.empty() or receiptSignature_.empty())
        return false;

    if (not identity.first or not identity.second) {
        RING_ERR("[Account %s] no identity provided", getAccountID().c_str());
        return false;
    }

    auto accountCertificate = identity.second->issuer;
    if (not accountCertificate) {
        RING_ERR("[Account %s] device certificate must be issued by the account certificate", getAccountID().c_str());
        return false;
    }

    // match certificate chain
    dht::crypto::TrustList account_trust;
    account_trust.add(*accountCertificate);
    if (not account_trust.verify(*identity.second)) {
        RING_ERR("[Account %s] can't use identity: device certificate chain can't be verified", getAccountID().c_str());
        return false;
    }

    auto pk = accountCertificate->getPublicKey();
    RING_DBG("[Account %s] checking device receipt for %s", getAccountID().c_str(), pk.getId().toString().c_str());
    if (!pk.checkSignature({receipt_.begin(), receipt_.end()}, receiptSignature_)) {
        RING_ERR("[Account %s] device receipt signature check failed", getAccountID().c_str());
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(&receipt_[0], &receipt_[receipt_.size()], &root, nullptr)) {
        RING_ERR() << this << " device receipt parsing error";
        return false;
    }

    auto dev_id = root["dev"].asString();
    if (dev_id != identity.second->getId().toString()) {
        RING_ERR("[Account %s] device ID mismatch between receipt and certificate", getAccountID().c_str());
        return false;
    }
    auto id = root["id"].asString();
    if (id != pk.getId().toString()) {
        RING_ERR("[Account %s] account ID mismatch between receipt and certificate", getAccountID().c_str());
        return false;
    }

    dht::Value announce_val;
    try {
        auto announce = base64::decode(root["announce"].asString());
        msgpack::object_handle announce_msg = msgpack::unpack((const char*)announce.data(), announce.size());
        //dht::Value announce_val (announce_msg.get());
        announce_val.msgpack_unpack(announce_msg.get());
        if (not announce_val.checkSignature()) {
            RING_ERR("[Account %s] announce signature check failed", getAccountID().c_str());
            return false;
        }
        DeviceAnnouncement da;
        da.unpackValue(announce_val);
        if (da.from.toString() != id or da.dev.toString() != dev_id) {
            RING_ERR("[Account %s] device ID mismatch in announce", getAccountID().c_str());
            return false;
        }
    } catch (const std::exception& e) {
        RING_ERR("[Account %s] can't read announce: %s", getAccountID().c_str(), e.what());
        return false;
    }

    // success, make use of this identity (certificate chain and private key)
    identity_ = identity;
    accountTrust_ = std::move(account_trust);
    ringAccountId_ = id;
    ringDeviceId_ = identity.first->getPublicKey().getId().toString();
    username_ = RING_URI_PREFIX + id;
    announce_ = std::make_shared<dht::Value>(std::move(announce_val));
    ethAccount_ = root["eth"].asString();

    RING_DBG("[Account %s] ring:%s device %s receipt checked successfully", getAccountID().c_str(), id.c_str(), ringDeviceId_.c_str());
    return true;
}

dht::crypto::Identity
RingAccount::loadIdentity(const std::string& crt_path, const std::string& key_path, const std::string& key_pwd) const
{
    RING_DBG("[Account %s] loading identity: %s %s", getAccountID().c_str(), crt_path.c_str(), key_path.c_str());
    dht::crypto::Identity id;
    try {
        dht::crypto::Certificate dht_cert(fileutils::loadFile(crt_path, idPath_));
        dht::crypto::PrivateKey  dht_key(fileutils::loadFile(key_path, idPath_), key_pwd);
        auto crt_id = dht_cert.getId();
        if (crt_id != dht_key.getPublicKey().getId())
            return {};

        if (not dht_cert.issuer) {
            RING_ERR("[Account %s] device certificate %s has no issuer", getAccountID().c_str(), dht_cert.getId().toString().c_str());
            return {};
        }
        // load revocation lists for device authority (account certificate).
        tls::CertificateStore::instance().loadRevocations(*dht_cert.issuer);

        id = {
            std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)),
            std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))
        };
    }
    catch (const std::exception& e) {
        RING_ERR("Error loading identity: %s", e.what());
    }

    return id;
}

AccountArchive
RingAccount::readArchive(const std::string& pwd) const
{
    RING_DBG("[Account %s] reading account archive", getAccountID().c_str());
    return AccountArchive(fileutils::getFullPath(idPath_, archivePath_), pwd);
}


void
RingAccount::updateArchive(AccountArchive& archive) const
{
    using namespace DRing::Account::ConfProperties;

    // Keys not exported to archive
    static const auto filtered_keys = { Ringtone::PATH,
                                        ARCHIVE_PATH,
                                        RING_DEVICE_ID,
                                        RING_DEVICE_NAME,
                                        Conf::CONFIG_DHT_PORT };

    // Keys with meaning of file path where the contents has to be exported in base64
    static const auto encoded_keys = { TLS::CA_LIST_FILE,
                                       TLS::CERTIFICATE_FILE,
                                       TLS::PRIVATE_KEY_FILE };

    RING_DBG("[Account %s] building account archive", getAccountID().c_str());
    for (const auto& it : getAccountDetails()) {
        // filter-out?
        if (std::any_of(std::begin(filtered_keys), std::end(filtered_keys),
                        [&](const auto& key){ return key == it.first; }))
            continue;

        // file contents?
        if (std::any_of(std::begin(encoded_keys), std::end(encoded_keys),
                        [&](const auto& key){ return key == it.first; })) {
            try {
                archive.config.emplace(it.first, base64::encode(fileutils::loadFile(it.second)));
            } catch (...) {}
        } else
            archive.config.insert(it);
    }
    archive.contacts = contacts_;
}

void
RingAccount::saveArchive(AccountArchive& archive, const std::string& pwd)
{
    try {
        updateArchive(archive);
        if (archivePath_.empty())
            archivePath_ = "export.gz";
        archive.save(fileutils::getFullPath(idPath_, archivePath_), pwd);
        archiveHasPassword_ = not pwd.empty();
    } catch (const std::runtime_error& ex) {
        RING_ERR("[Account %s] Can't export archive: %s", getAccountID().c_str(), ex.what());
        return;
    }
}

bool
RingAccount::changeArchivePassword(const std::string& password_old, const std::string& password_new)
{
    auto path = fileutils::getFullPath(idPath_, archivePath_);
    try {
        AccountArchive(path, password_old).save(path, password_new);
        archiveHasPassword_ = not password_new.empty();
    } catch (const std::exception& ex) {
        RING_ERR("[Account %s] Can't change archive password: %s", getAccountID().c_str(), ex.what());
        return false;
    }
    return true;
}

std::pair<std::vector<uint8_t>, dht::InfoHash>
RingAccount::computeKeys(const std::string& password, const std::string& pin, bool previous)
{
    // Compute time seed
    auto now = std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch());
    auto tseed = now.count() / std::chrono::duration_cast<std::chrono::seconds>(EXPORT_KEY_RENEWAL_TIME).count();
    if (previous)
        tseed--;
    std::stringstream ss;
    ss << std::hex << tseed;
    auto tseed_str = ss.str();

    // Generate key for archive encryption, using PIN as the salt
    std::vector<uint8_t> salt_key;
    salt_key.reserve(pin.size() + tseed_str.size());
    salt_key.insert(salt_key.end(), pin.begin(), pin.end());
    salt_key.insert(salt_key.end(), tseed_str.begin(), tseed_str.end());
    auto key = dht::crypto::stretchKey(password, salt_key, 256/8);

    // Generate public storage location as SHA1(key).
    auto loc = dht::InfoHash::get(key);

    return {key, loc};
}

std::string
generatePIN(size_t length = 8)
{
    static constexpr const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    dht::crypto::random_device rd;
    std::uniform_int_distribution<size_t> dis(0, sizeof(alphabet)-2);
    std::string ret;
    ret.reserve(length);
    for (size_t i=0; i<length; i++)
        ret.push_back(alphabet[dis(rd)]);
    return ret;
}

void
RingAccount::addDevice(const std::string& password)
{
    auto this_ = std::static_pointer_cast<RingAccount>(shared_from_this());
    ThreadPool::instance().run([this_,password]() {
        std::vector<uint8_t> key;
        dht::InfoHash loc;
        std::string pin_str;
        AccountArchive a;
        try {
            RING_DBG("[Account %s] exporting account", this_->getAccountID().c_str());

            a = this_->readArchive(password);

            // Generate random PIN
            pin_str = generatePIN();

            std::tie(key, loc) = computeKeys(password, pin_str);
        } catch (const std::exception& e) {
            RING_ERR("[Account %s] can't export account: %s", this_->getAccountID().c_str(), e.what());
            emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(this_->getAccountID(), 1, "");
            return;
        }
        // now that key and loc are computed, display to user in lowercase
        std::transform(pin_str.begin(), pin_str.end(), pin_str.begin(), ::tolower);
        try {
            this_->updateArchive(a);
            auto encrypted = dht::crypto::aesEncrypt(archiver::compress(a.serialize()), key);
            if (not this_->dht_.isRunning())
                throw std::runtime_error("DHT is not running..");
            this_->dht_.put(loc, encrypted, [this_,pin_str](bool ok) {
                RING_DBG("[Account %s] account archive published: %s", this_->getAccountID().c_str(), ok ? "success" : "failure");
                if (ok)
                    emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(this_->getAccountID(), 0, pin_str);
                else
                    emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(this_->getAccountID(), 2, "");
            });
            RING_WARN("[Account %s] exporting account with PIN: %s at %s (size %zu)", this_->getAccountID().c_str(), pin_str.c_str(), loc.toString().c_str(), encrypted.size());
        } catch (const std::exception& e) {
            RING_ERR("[Account %s] can't export account: %s", this_->getAccountID().c_str(), e.what());
            emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(this_->getAccountID(), 2, "");
            return;
        }
    });
}

bool
RingAccount::exportArchive(const std::string& destinationPath, const std::string& password)
{
    try {
        // Save contacts if possible before exporting
        AccountArchive archive;
        if (!archiveHasPassword_ || !password.empty()) {
            archive = readArchive(password);
            updateArchive(archive);
            archive.save(fileutils::getFullPath(idPath_, archivePath_), password);
        }
        // Export the file
        auto sourcePath = fileutils::getFullPath(idPath_, archivePath_);
        std::ifstream src(sourcePath, std::ios::in | std::ios::binary);
        if (!src) return false;
        std::ofstream dst(destinationPath, std::ios::out | std::ios::binary);
        dst << src.rdbuf();
    } catch (const std::runtime_error& ex) {
        RING_ERR("[Account %s] Can't export archive: %s", getAccountID().c_str(), ex.what());
        return false;
    } catch (...) {
        RING_ERR("[Account %s] Can't export archive: can't read archive", getAccountID().c_str());
        return false;
    }
    return true;
}

bool
RingAccount::revokeDevice(const std::string& password, const std::string& device)
{
    // shared_ptr of future
    auto fa = ThreadPool::instance().getShared<AccountArchive>(
        [this, password] { return readArchive(password); });
    findCertificate(dht::InfoHash(device),
                    [this,fa=std::move(fa),password,device](const std::shared_ptr<dht::crypto::Certificate>& crt) mutable
    {
        if (not crt) {
            emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(), device, 2);
            return;
        }
        foundAccountDevice(crt);
        AccountArchive a;
        try {
            a = fa->get();
        } catch (...) {
            emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(), device, 1);
            return;
        }
        // Add revoked device to the revocation list and resign it
        if (not a.revoked)
            a.revoked = std::make_shared<decltype(a.revoked)::element_type>();
        a.revoked->revoke(*crt);
        a.revoked->sign(a.id);
        // add to CRL cache
        tls::CertificateStore::instance().pinRevocationList(a.id.second->getId().toString(), a.revoked);
        tls::CertificateStore::instance().loadRevocations(*identity_.second->issuer);
        saveArchive(a, password);
        knownDevices_.erase(crt->getId());
        saveKnownDevices();
        emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(), device, 0);
        emitSignal<DRing::ConfigurationSignal::KnownDevicesChanged>(getAccountID(), getKnownDevices());
        syncDevices();
    });
    return true;
}

std::pair<std::string, std::string>
RingAccount::saveIdentity(const dht::crypto::Identity id, const std::string& path, const std::string& name)
{
    auto names = std::make_pair(name + ".key", name + ".crt");
    if (id.first)
        fileutils::saveFile(path + DIR_SEPARATOR_STR + names.first, id.first->serialize(), 0600);
    if (id.second)
        fileutils::saveFile(path + DIR_SEPARATOR_STR + names.second, id.second->getPacked(), 0600);
    return names;
}

void
RingAccount::loadAccountFromArchive(AccountArchive&& archive, const std::string& archive_password)
{
    initRingDevice(archive);
    saveArchive(archive, archive_password);
    registrationState_ = RegistrationState::UNREGISTERED;
    Manager::instance().saveConfig();
    doRegister();
}

void
RingAccount::loadAccountFromFile(const std::string& archive_path, const std::string& archive_password)
{
    setRegistrationState(RegistrationState::INITIALIZING);
    auto accountId = getAccountID();
    ThreadPool::instance().run([w=weak(), archive_password, archive_path, accountId]{
        AccountArchive archive;
        try {
            archive = AccountArchive(archive_path, archive_password);
        } catch (const std::exception& ex) {
            RING_WARN("[Account %s] can't read file: %s", accountId.c_str(), ex.what());
            runOnMainThread([w, accountId]() {
                if (auto this_ = w.lock())
                    this_->setRegistrationState(RegistrationState::ERROR_GENERIC);
                Manager::instance().removeAccount(accountId);
            });
            return;
        }
        runOnMainThread([w, archive, archive_password]() mutable {
            if (auto this_ = w.lock())
                this_->loadAccountFromArchive(std::move(archive), archive_password);
        });
    });
}

void
RingAccount::loadAccountFromDHT(const std::string& archive_password, const std::string& archive_pin)
{
    setRegistrationState(RegistrationState::INITIALIZING);

    // launch dedicated dht instance
    if (dht_.isRunning()) {
        RING_ERR("DHT already running (stopping it first).");
        dht_.join();
    }
    dht_.setOnStatusChanged([](dht::NodeStatus s4, dht::NodeStatus s6) {
        RING_WARN("Dht status : IPv4 %s; IPv6 %s", dhtStatusStr(s4), dhtStatusStr(s6));
    });
    dht_.run((in_port_t)dhtPortUsed_, {}, true);
    dht_.bootstrap(loadNodes());
    auto bootstrap = loadBootstrap();
    if (not bootstrap.empty())
        dht_.bootstrap(bootstrap);

    auto w = weak();
    auto state_old = std::make_shared<std::pair<bool, bool>>(false, true);
    auto state_new = std::make_shared<std::pair<bool, bool>>(false, true);
    auto found = std::make_shared<bool>(false);

    auto searchEnded = [w,found,state_old,state_new](){
        if (*found)
            return;
        if (state_old->first && state_new->first) {
            bool network_error = !state_old->second && !state_new->second;
            if (auto this_ = w.lock()) {
                RING_WARN("[Account %s] failure looking for archive on DHT: %s", this_->getAccountID().c_str(), network_error ? "network error" : "not found");
                this_->setRegistrationState(network_error ? RegistrationState::ERROR_NETWORK : RegistrationState::ERROR_GENERIC);
                runOnMainThread([=]() {
                    Manager::instance().removeAccount(this_->getAccountID());
                });
            }
        }
    };

    auto search = [w,found,archive_password,archive_pin,searchEnded](bool previous, std::shared_ptr<std::pair<bool, bool>>& state) {
        std::vector<uint8_t> key;
        dht::InfoHash loc;

        // compute archive location and decryption keys
        try {
            std::tie(key, loc) = computeKeys(archive_password, archive_pin, previous);
            if (auto this_ = w.lock()) {
                RING_DBG("[Account %s] trying to load account from DHT with %s at %s", this_->getAccountID().c_str(), archive_pin.c_str(), loc.toString().c_str());
                this_->dht_.get(loc, [w,key,found,archive_password](const std::shared_ptr<dht::Value>& val) {
                    std::vector<uint8_t> decrypted;
                    try {
                        decrypted = archiver::decompress(dht::crypto::aesDecrypt(val->data, key));
                    } catch (const std::exception& ex) {
                        return true;
                    }
                    RING_DBG("Found archive on the DHT");
                    runOnMainThread([=]() {
                        if (auto this_ = w.lock()) {
                            try {
                                *found =  true;
                                this_->loadAccountFromArchive(AccountArchive(decrypted), archive_password);
                            } catch (const std::exception& e) {
                                RING_WARN("[Account %s] error reading archive: %s", this_->getAccountID().c_str(), e.what());
                                this_->setRegistrationState(RegistrationState::ERROR_GENERIC);
                                Manager::instance().removeAccount(this_->getAccountID());
                            }
                        }
                    });
                    return not *found;
                }, [=](bool ok) {
                    RING_DBG("[Account %s] DHT archive search ended at %s", this_->getAccountID().c_str(), loc.toString().c_str());
                    state->first = true;
                    state->second = ok;
                    searchEnded();
                });
            }
        } catch (const std::exception& e) {
            RING_ERR("Error computing keys: %s", e.what());
            state->first = true;
            state->second = true;
            searchEnded();
            return;
        }
    };

    ThreadPool::instance().run(std::bind(search, true, state_old));
    ThreadPool::instance().run(std::bind(search, false, state_new));
}

void
RingAccount::createAccount(const std::string& archive_password, dht::crypto::Identity&& migrate)
{
    RING_WARN("[Account %s] creating new account", getAccountID().c_str());
    setRegistrationState(RegistrationState::INITIALIZING);
    auto sthis = std::static_pointer_cast<RingAccount>(shared_from_this());
    ThreadPool::instance().run([sthis,archive_password,migrate]() mutable {
        AccountArchive a;
        auto& this_ = *sthis;

        auto future_keypair = ThreadPool::instance().get<dev::KeyPair>(std::bind(&dev::KeyPair::create));
        try {
            if (migrate.first and migrate.second) {
                RING_WARN("[Account %s] converting certificate from old ring account %s",
                          this_.getAccountID().c_str(), migrate.first->getPublicKey().getId().toString().c_str());
                a.id = std::move(migrate);
                try {
                    a.ca_key = std::make_shared<dht::crypto::PrivateKey>(fileutils::loadFile("ca.key", this_.idPath_));
                } catch (...) {}
                updateCertificates(a, migrate);
            } else {
                auto ca = dht::crypto::generateIdentity("Ring CA");
                if (!ca.first || !ca.second) {
                    throw VoipLinkException("Can't generate CA for this account.");
                }
                a.id = dht::crypto::generateIdentity("Ring", ca, 4096, true);
                if (!a.id.first || !a.id.second) {
                    throw VoipLinkException("Can't generate identity for this account.");
                }
                RING_WARN("[Account %s] new account: CA: %s, RingID: %s",
                          this_.getAccountID().c_str(), ca.second->getId().toString().c_str(),
                          a.id.second->getId().toString().c_str());
                a.ca_key = ca.first;
            }
            this_.ringAccountId_ = a.id.second->getId().toString();
            this_.username_ = RING_URI_PREFIX+this_.ringAccountId_;
            auto keypair = future_keypair.get();
            this_.ethAccount_ = keypair.address().hex();
            a.eth_key = keypair.secret().makeInsecure().asBytes();

            this_.createRingDevice(a.id);
            this_.saveArchive(a, archive_password);
        } catch (...) {
            this_.setRegistrationState(RegistrationState::ERROR_GENERIC);
            runOnMainThread([sthis]() {
                Manager::instance().removeAccount(sthis->getAccountID());
            });
        }
        RING_DBG("[Account %s] Account creation ended, saving configuration", this_.getAccountID().c_str());
        this_.setRegistrationState(RegistrationState::UNREGISTERED);
        Manager::instance().saveConfig();
        this_.doRegister();
    });
}

bool
RingAccount::needsMigration(const dht::crypto::Identity& id)
{
    if (not id.second)
        return false;
    auto cert = id.second->issuer;
    while (cert) {
        if (not cert->isCA()){
            RING_WARN("certificate %s is not a CA, needs update.", cert->getId().toString().c_str());
            return true;
        }
        if (cert->getExpiration() < clock::now()) {
            RING_WARN("certificate %s is expired, needs update.", cert->getId().toString().c_str());
            return true;
        }
        cert = cert->issuer;
    }
    return false;
}

bool
RingAccount::updateCertificates(AccountArchive& archive, dht::crypto::Identity& device)
{
    RING_WARN("Updating certificates");
    using Certificate = dht::crypto::Certificate;

    // We need the CA key to resign certificates
    if (not archive.id.first or
        not *archive.id.first or
        not archive.id.second or
        not archive.ca_key or
        not *archive.ca_key)
        return false;

    // Currently set the CA flag and update expiration dates
    bool updated = false;

    auto& cert = archive.id.second;
    auto ca = cert->issuer;
    // Update CA if possible and relevant
    if (not ca or (not ca->issuer and (not ca->isCA() or ca->getExpiration() < clock::now()))) {
        ca = std::make_shared<Certificate>(Certificate::generate(*archive.ca_key, "Ring CA", {}, true));
        updated = true;
        RING_DBG("CA CRT re-generated");
    }

    // Update certificate
    if (updated or not cert->isCA() or cert->getExpiration() < clock::now()) {
        cert = std::make_shared<Certificate>(Certificate::generate(*archive.id.first, "Ring", dht::crypto::Identity{archive.ca_key, ca}, true));
        updated = true;
        RING_DBG("ring CRT re-generated");
    }

    if (updated and device.first and *device.first) {
        // update device certificate
        device.second = std::make_shared<Certificate>(Certificate::generate(*device.first, "Ring device", archive.id));
        RING_DBG("device CRT re-generated");
    }

    return updated;
}

void
RingAccount::migrateAccount(const std::string& pwd, dht::crypto::Identity& device)
{
    AccountArchive archive;
    try {
        archive = readArchive(pwd);
    } catch (...) {
        RING_DBG("[Account %s] Can't load archive", getAccountID().c_str());
        Migration::setState(accountID_, Migration::State::INVALID);
        setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
        return;
    }

    if (updateCertificates(archive, device)) {
        std::tie(tlsPrivateKeyFile_, tlsCertificateFile_) = saveIdentity(device, idPath_, "ring_device");
        saveArchive(archive, pwd);
        setRegistrationState(RegistrationState::UNREGISTERED);
        Migration::setState(accountID_, Migration::State::SUCCESS);
    } else
        Migration::setState(accountID_, Migration::State::INVALID);
}

void
RingAccount::loadAccount(const std::string& archive_password, const std::string& archive_pin, const std::string& archive_path)
{
    if (registrationState_ == RegistrationState::INITIALIZING)
        return;

    RING_DBG("[Account %s] loading account", getAccountID().c_str());
    try {
        auto id = loadIdentity(tlsCertificateFile_, tlsPrivateKeyFile_, tlsPassword_);
        bool hasArchive = not archivePath_.empty()
            and fileutils::isFile(fileutils::getFullPath(idPath_, archivePath_));
        if (useIdentity(id)) {
            // normal loading path
            loadKnownDevices();
            loadContacts();
            loadTrustRequests();
            if (not hasArchive)
                RING_WARN("[Account %s] account archive not found, won't be able to add new devices", getAccountID().c_str());
            if (not isEnabled()) {
                setRegistrationState(RegistrationState::UNREGISTERED);
            }
        }
        else if (isEnabled()) {
            if (hasArchive) {
                if (needsMigration(id)) {
                    RING_WARN("[Account %s] account certificate needs update", getAccountID().c_str());
                    migrateAccount(archive_password, id);
                }
                else {
                    RING_WARN("[Account %s] archive present but no valid receipt: creating new device", getAccountID().c_str());
                    try {
                        initRingDevice(readArchive(archive_password));
                    }
                    catch (...) {
                        Migration::setState(accountID_, Migration::State::INVALID);
                        setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
                        return;
                    }
                    Migration::setState(accountID_, Migration::State::SUCCESS);
                    setRegistrationState(RegistrationState::UNREGISTERED);
                }
                Manager::instance().saveConfig();
                loadAccount(archive_password);
            }
            else {
                // no receipt or archive, creating new account
                if (not archive_path.empty()) {
                    // import account from file
                    loadAccountFromFile(archive_path, archive_password);
                }
                else if (not archive_pin.empty()) {
                    // import account from DHT
                    loadAccountFromDHT(archive_password, archive_pin);
                }
                else {
                    // create new account
                    createAccount(archive_password, std::move(id));
                }
            }
        }
    }
    catch (const std::exception& e) {
        RING_WARN("[Account %s] error loading account: %s", getAccountID().c_str(), e.what());
        identity_ = dht::crypto::Identity{};
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
RingAccount::setAccountDetails(const std::map<std::string, std::string>& details)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    SIPAccountBase::setAccountDetails(details);

    // TLS
    parsePath(details, Conf::CONFIG_TLS_CA_LIST_FILE, tlsCaListFile_, idPath_);
    parsePath(details, Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_, idPath_);
    parsePath(details, Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_, idPath_);
    parseString(details, Conf::CONFIG_TLS_PASSWORD, tlsPassword_);

    if (hostname_.empty())
        hostname_ = DHT_DEFAULT_BOOTSTRAP;
    parseInt(details, Conf::CONFIG_DHT_PORT, dhtPort_);
    parseBool(details, Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);
    parseBool(details, DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY, allowPeersFromHistory_);
    parseBool(details, DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT, allowPeersFromContact_);
    parseBool(details, DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED, allowPeersFromTrusted_);
    if (not dhtPort_)
        dhtPort_ = getRandomEvenPort(DHT_PORT_RANGE);
    dhtPortUsed_ = dhtPort_;

    std::string archive_password;
    std::string archive_pin;
    std::string archive_path;
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PASSWORD, archive_password);
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PIN,      archive_pin);
    std::transform(archive_pin.begin(), archive_pin.end(), archive_pin.begin(), ::toupper);
    parsePath(details, DRing::Account::ConfProperties::ARCHIVE_PATH,     archive_path, idPath_);
    parseString(details, DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);

    parseBool(details, DRing::Account::ConfProperties::PROXY_ENABLED, proxyEnabled_);
    auto oldProxyServer = proxyServer_;
    parseString(details, DRing::Account::ConfProperties::PROXY_SERVER, proxyServer_);
    parseString(details, DRing::Account::ConfProperties::PROXY_PUSH_TOKEN, deviceKey_);
    // Migrate from old versions
    if (proxyServer_.empty()
    || ((proxyServer_ == "dhtproxy.jami.net"
    ||  proxyServer_ == "dhtproxy.ring.cx")
    &&  proxyServerCached_.empty()))
        proxyServer_ = DHT_DEFAULT_PROXY;
    if (proxyServer_ != oldProxyServer) {
        RING_DBG("DHT Proxy configuration changed, resetting cache");
        proxyServerCached_ = {};
        auto proxyCachePath = cachePath_ + DIR_SEPARATOR_STR "dhtproxy";
        std::remove(proxyCachePath.c_str());
    }

#if HAVE_RINGNS
    parseString(details, DRing::Account::ConfProperties::RingNS::URI,     nameServer_);
    nameDir_ = NameDirectory::instance(nameServer_);
#endif

    loadAccount(archive_password, archive_pin, archive_path);

    // update device name if necessary
    auto dev = knownDevices_.find(dht::InfoHash(ringDeviceId_));
    if (dev != knownDevices_.end()) {
        if (dev->second.name != ringDeviceName_) {
            dev->second.name = ringDeviceName_;
            saveKnownDevices();
        }
    }
}

std::map<std::string, std::string>
RingAccount::getAccountDetails() const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    a.emplace(Conf::CONFIG_DHT_PORT, ring::to_string(dhtPort_));
    a.emplace(Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::RING_DEVICE_ID, ringDeviceId_);
    a.emplace(DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);
    a.emplace(DRing::Account::ConfProperties::Presence::SUPPORT_SUBSCRIBE, TRUE_STR);
    if (not archivePath_.empty())
        a.emplace(DRing::Account::ConfProperties::ARCHIVE_HAS_PASSWORD, archiveHasPassword_ ? TRUE_STR : FALSE_STR);

    /* these settings cannot be changed (read only), but clients should still be
     * able to read what they are */
    a.emplace(Conf::CONFIG_SRTP_KEY_EXCHANGE, sip_utils::getKeyExchangeName(getSrtpKeyExchange()));
    a.emplace(Conf::CONFIG_SRTP_ENABLE,       isSrtpEnabled() ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_SRTP_RTP_FALLBACK, getSrtpFallback() ? TRUE_STR : FALSE_STR);

    a.emplace(Conf::CONFIG_TLS_CA_LIST_FILE,        fileutils::getFullPath(idPath_, tlsCaListFile_));
    a.emplace(Conf::CONFIG_TLS_CERTIFICATE_FILE,    fileutils::getFullPath(idPath_, tlsCertificateFile_));
    a.emplace(Conf::CONFIG_TLS_PRIVATE_KEY_FILE,    fileutils::getFullPath(idPath_, tlsPrivateKeyFile_));
    a.emplace(Conf::CONFIG_TLS_PASSWORD,            tlsPassword_);
    a.emplace(Conf::CONFIG_TLS_METHOD,                     "Automatic");
    a.emplace(Conf::CONFIG_TLS_CIPHERS,                    "");
    a.emplace(Conf::CONFIG_TLS_SERVER_NAME,                "");
    a.emplace(Conf::CONFIG_TLS_VERIFY_SERVER,              TRUE_STR);
    a.emplace(Conf::CONFIG_TLS_VERIFY_CLIENT,              TRUE_STR);
    a.emplace(Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, TRUE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY, allowPeersFromHistory_?TRUE_STR:FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT, allowPeersFromContact_?TRUE_STR:FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED, allowPeersFromTrusted_?TRUE_STR:FALSE_STR);
    /* GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT is defined as -1 */
    a.emplace(Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC,    "-1");
    a.emplace(DRing::Account::ConfProperties::PROXY_ENABLED,    proxyEnabled_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::PROXY_SERVER,     proxyServer_);
    a.emplace(DRing::Account::ConfProperties::PROXY_PUSH_TOKEN, deviceKey_);

    //a.emplace(DRing::Account::ConfProperties::ETH::KEY_FILE,               ethPath_);
    a.emplace(DRing::Account::ConfProperties::RingNS::ACCOUNT,               ethAccount_);
#if HAVE_RINGNS
    a.emplace(DRing::Account::ConfProperties::RingNS::URI,                   nameDir_.get().getServer());
#endif

    return a;
}

std::map<std::string, std::string>
RingAccount::getVolatileAccountDetails() const
{
    auto a = SIPAccountBase::getVolatileAccountDetails();
    a.emplace(DRing::Account::VolatileProperties::InstantMessaging::OFF_CALL, TRUE_STR);
#if HAVE_RINGNS
    if (not registeredName_.empty())
        a.emplace(DRing::Account::VolatileProperties::REGISTERED_NAME, registeredName_);
#endif
    return a;
}

#if HAVE_RINGNS
void
RingAccount::lookupName(const std::string& name)
{
    auto acc = getAccountID();
    NameDirectory::lookupUri(name, nameServer_, [acc,name](const std::string& result, NameDirectory::Response response) {
        emitSignal<DRing::ConfigurationSignal::RegisteredNameFound>(acc, (int)response, result, name);
    });
}

void
RingAccount::lookupAddress(const std::string& addr)
{
    auto acc = getAccountID();
    nameDir_.get().lookupAddress(addr, [acc,addr](const std::string& result, NameDirectory::Response response) {
        emitSignal<DRing::ConfigurationSignal::RegisteredNameFound>(acc, (int)response, addr, result);
    });
}

void
RingAccount::registerName(const std::string& /*password*/, const std::string& name)
{
    auto acc = getAccountID();
    nameDir_.get().registerName(ringAccountId_, name, ethAccount_, [acc,name,w=weak()](NameDirectory::RegistrationResponse response){
        int res = (response == NameDirectory::RegistrationResponse::success)      ? 0 : (
                  (response == NameDirectory::RegistrationResponse::invalidName)  ? 2 : (
                  (response == NameDirectory::RegistrationResponse::alreadyTaken) ? 3 : 4));
        if (response == NameDirectory::RegistrationResponse::success) {
            if (auto this_ = w.lock())
                this_->registeredName_ = name;
        }
        emitSignal<DRing::ConfigurationSignal::NameRegistrationEnded>(acc, res, name);
    });
}
#endif

bool
RingAccount::handlePendingCallList()
{
    // Process pending call into a local list to not block threads depending on this list,
    // as incoming call handlers.
    decltype(pendingCalls_) pending_calls;
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pending_calls = std::move(pendingCalls_);
        pendingCalls_.clear();
    }

    auto pc_iter = std::begin(pending_calls);
    while (pc_iter != std::end(pending_calls)) {
        bool incoming = !pc_iter->call_key; // do it now, handlePendingCall may invalidate pc data
        bool handled;

        try {
            handled = handlePendingCall(*pc_iter, incoming);
        } catch (const std::exception& e) {
            RING_ERR("[DHT] exception during pending call handling: %s", e.what());
            handled = true; // drop from pending list
        }

        if (handled) {
            // Cancel pending listen (outgoing call)
            if (not incoming)
                dht_.cancelListen(pc_iter->call_key, pc_iter->listen_key.share());
            pc_iter = pending_calls.erase(pc_iter);
        } else
            ++pc_iter;
    }

    // Re-integrate non-handled and valid pending calls
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.splice(std::end(pendingCalls_), pending_calls);
        return not pendingCalls_.empty();
    }
}

pj_status_t
RingAccount::checkPeerTlsCertificate(dht::InfoHash from,
                        dht::InfoHash from_account,
                        unsigned status,
                        const gnutls_datum_t* cert_list,
                        unsigned cert_num,
                        std::shared_ptr<dht::crypto::Certificate>& cert_out)
{
    if (cert_num == 0) {
        RING_ERR("[peer:%s] No certificate", from.toString().c_str());
        return PJ_SSL_CERT_EUNKNOWN;
    }
    if (status & GNUTLS_CERT_EXPIRED or status & GNUTLS_CERT_NOT_ACTIVATED) {
        RING_ERR("[peer:%s] Expired certificate", from.toString().c_str());
        return PJ_SSL_CERT_EVALIDITY_PERIOD;
    }

    // Unserialize certificate chain
    std::vector<std::pair<uint8_t*, uint8_t*>> crt_data;
    crt_data.reserve(cert_num);
    for (unsigned i=0; i<cert_num; i++)
        crt_data.emplace_back(cert_list[i].data, cert_list[i].data + cert_list[i].size);
    auto crt = std::make_shared<dht::crypto::Certificate>(crt_data);

    // Check expected peer identity
    dht::InfoHash tls_account_id;
    if (not foundPeerDevice(crt, tls_account_id)) {
        RING_ERR("[peer:%s] Discarding message from invalid peer certificate.", from.toString().c_str());
        return PJ_SSL_CERT_EUNKNOWN;
    }
    if (from_account != tls_account_id) {
        RING_ERR("[peer:%s] Discarding message from wrong peer account %s.", from.toString().c_str(), tls_account_id.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    const auto tls_id = crt->getId();
    if (crt->getUID() != tls_id.toString()) {
        RING_ERR("[peer:%s] Certificate UID must be the public key ID", from.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }
    if (tls_id != from) {
        RING_ERR("[peer:%s] Certificate public key ID doesn't match (%s)",
                 from.toString().c_str(), tls_id.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    RING_DBG("[peer:%s] Certificate verified", from.toString().c_str());
    cert_out = std::move(crt);
    return PJ_SUCCESS;
}

bool
RingAccount::handlePendingCall(PendingCall& pc, bool incoming)
{
    auto call = pc.call.lock();
    if (not call)
        return true;

    auto ice = pc.ice_sp.get();
    if (not ice or ice->isFailed()) {
        RING_ERR("[call:%s] Null or failed ICE transport", call->getCallId().c_str());
        call->onFailure();
        return true;
    }

    // Return to pending list if not negotiated yet and not in timeout
    if (not ice->isRunning()) {
        if ((std::chrono::steady_clock::now() - pc.start) >= ICE_NEGOTIATION_TIMEOUT) {
            RING_WARN("[call:%s] Timeout on ICE negotiation", call->getCallId().c_str());
            call->onFailure();
            return true;
        }
        // Cleanup pending call if call is over (cancelled by user or any other reason)
        return call->getState() == Call::CallState::OVER;
    }

    // Securize a SIP transport with TLS (on top of ICE tranport) and assign the call with it
    auto remote_device = pc.from;
    auto remote_account = pc.from_account;
    if (not identity_.first or not identity_.second)
        throw std::runtime_error("No identity configured for this account.");

    std::weak_ptr<RingAccount> waccount = weak();
    std::weak_ptr<SIPCall> wcall = call;
    tls::TlsParams tlsParams {
        /*.ca_list = */"",
        /*.ca = */pc.from_cert,
        /*.cert = */identity_.second,
        /*.cert_key = */identity_.first,
        /*.dh_params = */dhParams_,
        /*.timeout = */std::chrono::duration_cast<decltype(tls::TlsParams::timeout)>(TLS_TIMEOUT),
        /*.cert_check = */[waccount,wcall,remote_device,remote_account](unsigned status,
                                                             const gnutls_datum_t* cert_list,
                                                             unsigned cert_num) -> pj_status_t {
            try {
                if (auto call = wcall.lock()) {
                    if (auto sthis = waccount.lock()) {
                        auto& this_ = *sthis;
                        std::shared_ptr<dht::crypto::Certificate> peer_cert;
                        auto ret = checkPeerTlsCertificate(remote_device, remote_account, status, cert_list, cert_num, peer_cert);
                        if (ret == PJ_SUCCESS and peer_cert) {
                            std::lock_guard<std::mutex> lock(this_.callsMutex_);
                            for (auto& pscall : this_.pendingSipCalls_) {
                                if (auto pcall = pscall.call.lock()) {
                                    if (pcall == call and not pscall.from_cert) {
                                        RING_DBG("[call:%s] got peer certificate from TLS negotiation", call->getCallId().c_str());
                                        tls::CertificateStore::instance().pinCertificate(peer_cert);
                                        pscall.from_cert = peer_cert;
                                        break;
                                    }
                                }
                            }
                        }
                        return ret;
                    }
                }
                return PJ_SSL_CERT_EUNTRUSTED;
            } catch (const std::exception& e) {
                RING_ERR("[peer:%s] TLS certificate check exception: %s",
                         remote_device.toString().c_str(), e.what());
                return PJ_SSL_CERT_EUNKNOWN;
            }
        }
    };

    // Following can create a transport that need to be negotiated (TLS).
    // This is a asynchronous task. So we're going to process the SIP after this negotiation.
    auto transport = link_->sipTransportBroker->getTlsIceTransport(pc.ice_sp,
                                                                   ICE_COMP_SIP_TRANSPORT,
                                                                   tlsParams);
    if (!transport)
        throw std::runtime_error("transport creation failed");

    call->setTransport(transport);

    if (incoming) {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingSipCalls_.emplace_back(std::move(pc)); // copy of pc
    } else {
        // Be acknowledged on transport connection/disconnection
        auto lid = reinterpret_cast<uintptr_t>(this);
        auto remote_id = remote_device.toString();
        auto remote_addr = ice->getRemoteAddress(ICE_COMP_SIP_TRANSPORT);
        auto& tr_self = *transport;
        transport->addStateListener(lid,
            [&tr_self, lid, wcall, waccount, remote_id, remote_addr](pjsip_transport_state state,
                                                                     UNUSED const pjsip_transport_state_info* info) {
                if (state == PJSIP_TP_STATE_CONNECTED) {
                    if (auto call = wcall.lock()) {
                        if (auto account = waccount.lock()) {
                            // Start SIP layer when TLS negotiation is successful
                            account->onConnectedOutgoingCall(*call, remote_id, remote_addr);
                            return;
                        }
                    }
                } else if (state == PJSIP_TP_STATE_DISCONNECTED) {
                    tr_self.removeStateListener(lid);
                }
            });
    }

    // Notify of fully available connection between peers
    call->setState(Call::ConnectionState::PROGRESSING);

    return true;
}

bool
RingAccount::mapPortUPnP()
{
    // return true if not using UPnP
    bool added = true;

    if (getUPnPActive()) {
        /* create port mapping from published port to local port to the local IP
         * note that since different RING accounts can use the same port,
         * it may already be open, thats OK
         *
         * if the desired port is taken by another client, then it will try to map
         * a different port, if succesfull, then we have to use that port for DHT
         */
        uint16_t port_used;
        std::lock_guard<std::mutex> lock(upnp_mtx);
        upnp_->removeMappings();
        added = upnp_->addAnyMapping(dhtPort_, ring::upnp::PortType::UDP, false, &port_used);
        if (added) {
            if (port_used != dhtPort_)
                RING_DBG("UPnP could not map port %u for DHT, using %u instead", dhtPort_, port_used);
            dhtPortUsed_ = port_used;
        }
    }

    upnp_->setIGDListener([w=weak()] {
        if (auto shared = w.lock())
            shared->igdChanged();
    });
    return added;
}

void
RingAccount::doRegister()
{
    std::unique_lock<std::mutex> lock(configurationMutex_);
    if (not isUsable()) {
        RING_WARN("Account must be enabled and active to register, ignoring");
        return;
    }

    // invalid state transitions:
    // INITIALIZING: generating/loading certificates, can't register
    // NEED_MIGRATION: old account detected, user needs to migrate
    if (registrationState_ == RegistrationState::INITIALIZING
     || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION)
        return;

    if (not dhParams_.valid()) {
        generateDhParams();
    }

    /* if UPnP is enabled, then wait for IGD to complete registration */
    if (upnp_) {
        RING_DBG("UPnP: waiting for IGD to register RING account");
        lock.unlock();
        setRegistrationState(RegistrationState::TRYING);
        std::thread{ [w=weak()] {
            if (auto acc = w.lock()) {
                if (not acc->mapPortUPnP())
                    RING_WARN("UPnP: Could not successfully map DHT port with UPnP, continuing with account registration anyways.");
                acc->doRegister_();
            }
        }}.detach();
    } else {
        lock.unlock();
        doRegister_();
    }
}


std::vector<dht::SockAddr>
RingAccount::loadBootstrap() const
{
    std::vector<dht::SockAddr> bootstrap;
    if (!hostname_.empty()) {
        std::stringstream ss(hostname_);
        std::string node_addr;
        while (std::getline(ss, node_addr, ';')) {
            auto ips = dht::SockAddr::resolve(node_addr);
            if (ips.empty()) {
                IpAddr resolved(node_addr);
                if (resolved) {
                    if (resolved.getPort() == 0)
                        resolved.setPort(DHT_DEFAULT_PORT);
                    bootstrap.emplace_back(static_cast<const sockaddr*>(resolved), resolved.getLength());
                }
            } else {
                bootstrap.reserve(bootstrap.size() + ips.size());
                for (auto& ip : ips) {
                    if (ip.getPort() == 0)
                        ip.setPort(DHT_DEFAULT_PORT);
                    bootstrap.emplace_back(std::move(ip));
                }
            }
        }
        for (const auto& ip : bootstrap)
            RING_DBG("Bootstrap node: %s", ip.toString().c_str());
    }
    return bootstrap;
}

void
RingAccount::trackBuddyPresence(const std::string& buddy_id, bool track)
{
    std::string buddyUri;

    try {
        buddyUri = parseRingUri(buddy_id);
    }
    catch (...) {
        RING_ERR("[Account %s] Failed to track a buddy due to an invalid URI %s", getAccountID().c_str(), buddy_id.c_str());
        return;
    }
    auto h = dht::InfoHash(buddyUri);
    std::lock_guard<std::mutex> lock(buddyInfoMtx);
    if (track) {
        auto buddy = trackedBuddies_.emplace(h, BuddyInfo {h});
        if (buddy.second) {
            trackPresence(buddy.first->first, buddy.first->second);
        }
    } else {
        auto buddy = trackedBuddies_.find(h);
        if (buddy != trackedBuddies_.end()) {
            if (dht_.isRunning())
                dht_.cancelListen(h, std::move(buddy->second.listenToken));
            trackedBuddies_.erase(buddy);
        }
    }
}

void
RingAccount::trackPresence(const dht::InfoHash& h, BuddyInfo& buddy)
{
    if (not dht_.isRunning()) {
        return;
    }
    buddy.listenToken = dht_.listen<DeviceAnnouncement>(h, [this, h](DeviceAnnouncement&&, bool expired){
        bool wasConnected, isConnected;
        {
            std::lock_guard<std::mutex> lock(buddyInfoMtx);
            auto buddy = trackedBuddies_.find(h);
            if (buddy == trackedBuddies_.end())
                return true;
            wasConnected = buddy->second.devices_cnt > 0;
            if (expired)
                --buddy->second.devices_cnt;
            else
                ++buddy->second.devices_cnt;
            isConnected = buddy->second.devices_cnt > 0;
        }
        if (isConnected and not wasConnected) {
            onTrackedBuddyOnline(h);
        } else if (not isConnected and wasConnected) {
            onTrackedBuddyOffline(h);
        }
        return true;
    });
    RING_DBG("[Account %s] tracking buddy %s", getAccountID().c_str(), h.to_c_str());
}

std::map<std::string, bool>
RingAccount::getTrackedBuddyPresence()
{
    std::lock_guard<std::mutex> lock(buddyInfoMtx);
    std::map<std::string, bool> presence_info;
    for (const auto& buddy_info_p : trackedBuddies_)
        presence_info.emplace(buddy_info_p.first.toString(), buddy_info_p.second.devices_cnt > 0);
    return presence_info;
}

void
RingAccount::onTrackedBuddyOnline(const dht::InfoHash& contactId)
{
    RING_DBG("Buddy %s online", contactId.toString().c_str());
    std::string id(contactId.toString());
    emitSignal<DRing::PresenceSignal::NewBuddyNotification>(getAccountID(), id, 1,  "");
    messageEngine_.onPeerOnline(id);
}

void
RingAccount::onTrackedBuddyOffline(const dht::InfoHash& contactId)
{
    RING_DBG("Buddy %s offline", contactId.toString().c_str());
    emitSignal<DRing::PresenceSignal::NewBuddyNotification>(getAccountID(), contactId.toString(), 0,  "");
}

void
RingAccount::doRegister_()
{
    std::lock_guard<std::mutex> lock(configurationMutex_);

    try {
        if (not identity_.first or not identity_.second)
            throw std::runtime_error("No identity configured for this account.");

        loadTreatedCalls();
        loadTreatedMessages();
        if (dht_.isRunning()) {
            RING_ERR("[Account %s] DHT already running (stopping it first).", getAccountID().c_str());
            dht_.join();
        }

#if HAVE_RINGNS
        // Look for registered name on the blockchain
        nameDir_.get().lookupAddress(ringAccountId_, [w=weak()](const std::string& result, const NameDirectory::Response& response) {
            if (auto this_ = w.lock()) {
                if (response == NameDirectory::Response::found) {
                    if (this_->registeredName_ != result) {
                        this_->registeredName_ = result;
                        emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(this_->accountID_, this_->getVolatileAccountDetails());
                    }
                } else if (response == NameDirectory::Response::notFound) {
                    if (not this_->registeredName_.empty()) {
                        this_->registeredName_.clear();
                        emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(this_->accountID_, this_->getVolatileAccountDetails());
                    }
                }
            }
        });
#endif

        auto currentDhtStatus = std::make_shared<dht::NodeStatus>(dht::NodeStatus::Disconnected);
        dht_.setOnStatusChanged([this, currentDhtStatus](dht::NodeStatus s4, dht::NodeStatus s6) {
            RING_DBG("[Account %s] Dht status : IPv4 %s; IPv6 %s", getAccountID().c_str(), dhtStatusStr(s4), dhtStatusStr(s6));
            RegistrationState state;
            auto newStatus = std::max(s4, s6);
            if (newStatus == *currentDhtStatus)
                return;
            switch (newStatus) {
                case dht::NodeStatus::Connecting:
                    RING_WARN("[Account %s] connecting to the DHT network...", getAccountID().c_str());
                    state = RegistrationState::TRYING;
                    break;
                case dht::NodeStatus::Connected:
                    RING_WARN("[Account %s] connected to the DHT network", getAccountID().c_str());
                    state = RegistrationState::REGISTERED;
                    break;
                case dht::NodeStatus::Disconnected:
                    RING_WARN("[Account %s] disconnected from the DHT network", getAccountID().c_str());
                    state = RegistrationState::UNREGISTERED;
                    break;
                default:
                    state = RegistrationState::ERROR_GENERIC;
                    break;
            }
            *currentDhtStatus = newStatus;
            setRegistrationState(state);
        });

        dht::DhtRunner::Config config {};
        config.dht_config.node_config.network = 0;
        config.dht_config.node_config.maintain_storage = false;
        config.dht_config.id = identity_;
        config.proxy_server = getDhtProxyServer();
        config.push_node_id = getAccountID();
        config.threaded = true;
        if (not config.proxy_server.empty())
            RING_WARN("[Account %s] using proxy server %s", getAccountID().c_str(), config.proxy_server.c_str());

        if (not deviceKey_.empty()) {
            RING_WARN("[Account %s] using push notifications", getAccountID().c_str());
            dht_.setPushNotificationToken(deviceKey_);
        }

        dht_.run((in_port_t)dhtPortUsed_, config);

        dht_.setLocalCertificateStore([](const dht::InfoHash& pk_id) {
            std::vector<std::shared_ptr<dht::crypto::Certificate>> ret;
            if (auto cert = tls::CertificateStore::instance().getCertificate(pk_id.toString()))
                ret.emplace_back(std::move(cert));
            RING_DBG("Query for local certificate store: %s: %zu found.", pk_id.toString().c_str(), ret.size());
            return ret;
        });

        auto dht_log_level = Manager::instance().dhtLogLevel.load();
        if (dht_log_level > 0) {
            static auto silent = [](char const* /*m*/, va_list /*args*/) {};
            static auto log_error = [](char const* m, va_list args) { Logger::vlog(LOG_ERR, nullptr, 0, true, m, args); };
            static auto log_warn = [](char const* m, va_list args) { Logger::vlog(LOG_WARNING, nullptr, 0, true, m, args); };
            static auto log_debug = [](char const* m, va_list args) { Logger::vlog(LOG_DEBUG, nullptr, 0, true, m, args); };
#ifndef _MSC_VER
            dht_.setLoggers(
                log_error,
                (dht_log_level > 1) ? log_warn : silent,
                (dht_log_level > 2) ? log_debug : silent);
#elif RING_UWP
            static auto log_all = [](char const* m, va_list args) {
                char tmp[2048];
                vsprintf(tmp, m, args);
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                ring::emitSignal<DRing::DebugSignal::MessageSend>(std::to_string(now) + " " + std::string(tmp));
            };
            dht_.setLoggers(log_all, log_all, silent);
#else
            if (dht_log_level > 2) {
                dht_.setLoggers(log_error, log_warn, log_debug);
            } else if (dht_log_level > 1) {
                dht_.setLoggers(log_error, log_warn, silent);
            } else {
                dht_.setLoggers(log_error, silent, silent);
            }
#endif
        }

        dht_.importValues(loadValues());

        setRegistrationState(RegistrationState::TRYING);

        dht_.bootstrap(loadNodes());
        auto bootstrap = loadBootstrap();
        if (not bootstrap.empty())
            dht_.bootstrap(bootstrap);

        // Put device annoucement
        if (announce_) {
            auto h = dht::InfoHash(ringAccountId_);
            RING_DBG("[Account %s] announcing device at %s", getAccountID().c_str(), h.toString().c_str());
            dht_.put(h, announce_, dht::DoneCallback{}, {}, true);
            for (const auto& crl : identity_.second->issuer->getRevocationLists())
                dht_.put(h, crl, dht::DoneCallback{}, {}, true);
            dht_.listen<DeviceAnnouncement>(h, [this](DeviceAnnouncement&& dev) {
                findCertificate(dev.dev, [this](const std::shared_ptr<dht::crypto::Certificate>& crt) {
                    foundAccountDevice(crt);
                });
                return true;
            });
            dht_.listen<dht::crypto::RevocationList>(h, [this](dht::crypto::RevocationList&& crl) {
                if (crl.isSignedBy(*identity_.second->issuer)) {
                    RING_DBG("[Account %s] found CRL for account.", getAccountID().c_str());
                    tls::CertificateStore::instance().pinRevocationList(
                        ringAccountId_,
                        std::make_shared<dht::crypto::RevocationList>(std::move(crl)));
                }
                return true;
            });
            syncDevices();
        } else {
            RING_WARN("[Account %s] can't announce device: no annoucement...", getAccountID().c_str());
        }

        // Listen for incoming calls
        callKey_ = dht::InfoHash::get("callto:"+ringDeviceId_);
        RING_DBG("[Account %s] Listening on callto:%s : %s", getAccountID().c_str(), ringDeviceId_.c_str(), callKey_.toString().c_str());
        dht_.listen<dht::IceCandidates>(
            callKey_,
            [this] (dht::IceCandidates&& msg) {
                // callback for incoming call
                auto from = msg.from;
                if (from == dht_.getId())
                    return true;

                auto res = treatedCalls_.insert(msg.id);
                saveTreatedCalls();
                if (!res.second)
                    return true;

                RING_WARN("[Account %s] ICE candidate from %s.", getAccountID().c_str(), from.toString().c_str());

                onPeerMessage(from, [this, msg=std::move(msg)](const std::shared_ptr<dht::crypto::Certificate>& cert,
                                                               const dht::InfoHash& account) mutable
                {
                    incomingCall(std::move(msg), cert, account);
                });
                return true;
            }
        );

        auto inboxKey = dht::InfoHash::get("inbox:"+ringDeviceId_);
        dht_.listen<dht::TrustRequest>(
            inboxKey,
            [this](dht::TrustRequest&& v) {
                if (v.service != DHT_TYPE_NS)
                    return true;

                findCertificate(v.from, [this, v](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                    // check peer certificate
                    dht::InfoHash peer_account;
                    if (not foundPeerDevice(cert, peer_account)) {
                        return;
                    }

                    RING_WARN("Got trust request from: %s / %s", peer_account.toString().c_str(), v.from.toString().c_str());
                    onTrustRequest(peer_account, v.from, time(nullptr), v.confirm, std::move(v.payload));
                });
                return true;
            }
        );

        auto syncDeviceKey = dht::InfoHash::get("inbox:"+ringDeviceId_);
        dht_.listen<DeviceSync>(
            syncDeviceKey,
            [this](DeviceSync&& sync) {
                // Received device sync data.
                // check device certificate
                findCertificate(sync.from, [this,sync](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                    if (!cert or cert->getId() != sync.from) {
                        RING_WARN("Can't find certificate for device %s", sync.from.toString().c_str());
                        return;
                    }
                    if (not foundAccountDevice(cert))
                        return;
                    onReceiveDeviceSync(std::move(sync));
                });

                return true;
            }
        );

        auto inboxDeviceKey = dht::InfoHash::get("inbox:"+ringDeviceId_);
        dht_.listen<dht::ImMessage>(
            inboxDeviceKey,
            [this,inboxDeviceKey](dht::ImMessage&& v) {
                {
                    std::lock_guard<std::mutex> lock(messageMutex_);
                    auto res = treatedMessages_.insert(v.id);
                    if (!res.second)
                        return true;
                }
                saveTreatedMessages();
                onPeerMessage(v.from, [this, v, inboxDeviceKey](const std::shared_ptr<dht::crypto::Certificate>&,
                                                                        const dht::InfoHash& peer_account)
                {
                    auto now = clock::to_time_t(clock::now());
                    std::string datatype = utf8_make_valid(v.datatype);
                    if (datatype.empty()) {
                        datatype = "text/plain";
                    }
                    std::map<std::string, std::string> payloads = {{datatype,
                                                                   utf8_make_valid(v.msg)}};
                    onTextMessage(peer_account.toString(), payloads);
                    RING_DBG() << "Sending message confirmation " << v.id;
                    dht_.putEncrypted(inboxDeviceKey,
                              v.from,
                              dht::ImMessage(v.id, std::string(), now));
                });
                return true;
            }
        );

        dhtPeerConnector_->onDhtConnected(ringDeviceId_);

        for (auto& buddy : trackedBuddies_) {
            buddy.second.devices_cnt = 0;
            trackPresence(buddy.first, buddy.second);
        }
    }
    catch (const std::exception& e) {
        RING_ERR("Error registering DHT account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
RingAccount::onTrustRequest(const dht::InfoHash& peer_account, const dht::InfoHash& peer_device, time_t received, bool confirm, std::vector<uint8_t>&& payload)
{
     // Check existing contact
    auto contact = contacts_.find(peer_account);
    if (contact != contacts_.end()) {
        // Banned contact: discard request
        if (contact->second.isBanned())
            return;
        // Send confirmation
        if (not confirm)
            sendTrustRequestConfirm(peer_account);
        // Contact exists, update confirmation status
        if (not contact->second.confirmed) {
            contact->second.confirmed = true;
            emitSignal<DRing::ConfigurationSignal::ContactAdded>(getAccountID(), peer_account.toString(), true);
            saveContacts();
            syncDevices();
        }
    } else {
        auto req = trustRequests_.find(peer_account);
        if (req == trustRequests_.end()) {
            // Add trust request
            req = trustRequests_.emplace(peer_account, TrustRequest{
                peer_device, received, std::move(payload)
            }).first;
        } else {
            // Update trust request
            if (received < req->second.received) {
                req->second.device = peer_device;
                req->second.received = received;
                req->second.payload = std::move(payload);
            } else {
                RING_DBG("[Account %s] Ignoring outdated trust request from %s", getAccountID().c_str(), peer_account.toString().c_str());
            }
        }
        saveTrustRequests();
        emitSignal<DRing::ConfigurationSignal::IncomingTrustRequest>(
            getAccountID(),
            req->first.toString(),
            req->second.payload,
            received
        );
    }
}

void
RingAccount::onPeerMessage(const dht::InfoHash& peer_device, std::function<void(const std::shared_ptr<dht::crypto::Certificate>& crt, const dht::InfoHash& peer_account)> cb)
{
    // quick check in case we already explicilty banned this device
    auto trustStatus = trust_.getCertificateStatus(peer_device.toString());
    if (trustStatus == tls::TrustStore::PermissionStatus::BANNED) {
        RING_WARN("[Account %s] Discarding message from banned device %s", getAccountID().c_str(), peer_device.toString().c_str());
        return;
    }

    findCertificate(peer_device,
        [this, peer_device, cb](const std::shared_ptr<dht::crypto::Certificate>& cert) {
        dht::InfoHash peer_account_id;
        if (not foundPeerDevice(cert, peer_account_id)) {
            RING_WARN("[Account %s] Discarding message from invalid peer certificate %s.", getAccountID().c_str(), peer_device.toString().c_str());
            return;
        }

        if (not trust_.isAllowed(*cert, dhtPublicInCalls_)) {
            RING_WARN("[Account %s] Discarding message from unauthorized peer %s.", getAccountID().c_str(), peer_device.toString().c_str());
            return;
        }

        cb(cert, peer_account_id);
    });
}

void
RingAccount::incomingCall(dht::IceCandidates&& msg, const std::shared_ptr<dht::crypto::Certificate>& from_cert, const dht::InfoHash& from)
{
    auto call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(*this, Manager::instance().getNewCallID(), Call::CallType::INCOMING);
    auto ice = createIceTransport(("sip:"+call->getCallId()).c_str(), ICE_COMPONENTS, false, getIceOptions());

    std::weak_ptr<SIPCall> wcall = call;
    Manager::instance().addTask([account=shared(), wcall, ice, msg, from_cert, from] {
        auto call = wcall.lock();

        // call aborted?
        if (not call)
            return false;

        if (ice->isFailed()) {
            RING_ERR("[call:%s] ice init failed", call->getCallId().c_str());
            call->onFailure(EIO);
            return false;
        }

        // Loop until ICE transport is initialized.
        // Note: we suppose that ICE init routine has a an internal timeout (bounded in time)
        // and we let upper layers decide when the call shall be aborted (our first check upper).
        if (not ice->isInitialized())
            return true;

        account->replyToIncomingIceMsg(call, ice, msg, from_cert, from);
        return false;
    });
}

bool
RingAccount::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name, const time_point& updated)
{
    if (not crt)
        return false;

    // match certificate chain
    if (not accountTrust_.verify(*crt)) {
        RING_WARN("[Account %s] Found invalid account device: %s", getAccountID().c_str(), crt->getId().toString().c_str());
        return false;
    }

    // insert device
    auto it = knownDevices_.emplace(crt->getId(), KnownDevice{crt, name, updated});
    if (it.second) {
        RING_DBG("[Account %s] Found account device: %s %s", getAccountID().c_str(),
                                                              name.c_str(),
                                                              crt->getId().toString().c_str());
        tls::CertificateStore::instance().pinCertificate(crt);
        saveKnownDevices();
        emitSignal<DRing::ConfigurationSignal::KnownDevicesChanged>(getAccountID(), getKnownDevices());
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            RING_DBG("[Account %s] updating device name: %s %s", getAccountID().c_str(),
                                                                  name.c_str(),
                                                                  crt->getId().toString().c_str());
            it.first->second.name = name;
            saveKnownDevices();
            emitSignal<DRing::ConfigurationSignal::KnownDevicesChanged>(getAccountID(), getKnownDevices());
        }
    }
    return true;
}

bool
RingAccount::foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, dht::InfoHash& account_id)
{
    if (not crt)
        return false;

    auto top_issuer = crt;
    while (top_issuer->issuer)
        top_issuer = top_issuer->issuer;

    // Device certificate can't be self-signed
    if (top_issuer == crt) {
        RING_WARN("Found invalid peer device: %s", crt->getId().toString().c_str());
        return false;
    }

    // Check peer certificate chain
    // Trust store with top issuer as the only CA
    dht::crypto::TrustList peer_trust;
    peer_trust.add(*top_issuer);
    if (not peer_trust.verify(*crt)) {
        RING_WARN("Found invalid peer device: %s", crt->getId().toString().c_str());
        return false;
    }

    account_id = crt->issuer->getId();
    RING_WARN("Found peer device: %s account:%s CA:%s", crt->getId().toString().c_str(), account_id.toString().c_str(), top_issuer->getId().toString().c_str());
    return true;
}

void
RingAccount::replyToIncomingIceMsg(const std::shared_ptr<SIPCall>& call,
                                   const std::shared_ptr<IceTransport>& ice,
                                   const dht::IceCandidates& peer_ice_msg,
                                   const std::shared_ptr<dht::crypto::Certificate>& from_cert,
                                   const dht::InfoHash& from_id)
{
    auto from = from_id.toString();
    call->setPeerUri(RING_URI_PREFIX + from);
    std::weak_ptr<SIPCall> wcall = call;
#if HAVE_RINGNS
    nameDir_.get().lookupAddress(from, [wcall](const std::string& result, const NameDirectory::Response& response){
        if (response == NameDirectory::Response::found)
            if (auto call = wcall.lock()) {
                call->setPeerRegistredName(result);
                call->setPeerUri(RING_URI_PREFIX + result);
            }
    });
#endif

    registerDhtAddress(*ice);
    // Asynchronous DHT put of our local ICE data
    auto shared_this = std::static_pointer_cast<RingAccount>(shared_from_this());
    dht_.putEncrypted(
        callKey_,
        peer_ice_msg.from,
        dht::Value {dht::IceCandidates(peer_ice_msg.id, ice->packIceMsg())},
        [wcall](bool ok) {
            if (!ok) {
                RING_WARN("Can't put ICE descriptor reply on DHT");
                if (auto call = wcall.lock())
                    call->onFailure();
            } else
                RING_DBG("Successfully put ICE descriptor reply on DHT");
        });

    auto started_time = std::chrono::steady_clock::now();

    // During the ICE reply we can start the ICE negotiation
    if (!ice->start(peer_ice_msg.ice_data)) {
        call->onFailure(EIO);
        return;
    }

    call->setPeerNumber(from);

    // Let the call handled by the PendingCall handler loop
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.emplace_back(PendingCall {
                /*.start = */started_time,
                /*.ice_sp = */ice,
                /*.call = */wcall,
                /*.listen_key = */{},
                /*.call_key = */{},
                /*.from = */peer_ice_msg.from,
                /*.from_account = */from_id,
                /*.from_cert = */from_cert });
        checkPendingCallsTask();
    }
}

void
RingAccount::doUnregister(std::function<void(bool)> released_cb)
{
    std::unique_lock<std::mutex> lock(configurationMutex_);

    if (registrationState_ == RegistrationState::INITIALIZING
     || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION) {
        lock.unlock();
        if (released_cb) released_cb(false);
        return;
    }

    RING_WARN("[Account %s] unregistering account %p", getAccountID().c_str(), this);
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.clear();
        pendingSipCalls_.clear();
        checkPendingCallsTask();
    }

    if (upnp_) {
        upnp_->setIGDListener();
        upnp_->removeMappings();
    }

    saveNodes(dht_.exportNodes());
    saveValues(dht_.exportValues());
    dht_.join();

    lock.unlock();
    setRegistrationState(RegistrationState::UNREGISTERED);

    if (released_cb)
        released_cb(false);
}

void
RingAccount::connectivityChanged()
{
    RING_WARN("connectivityChanged");
    if (not isUsable()) {
        // nothing to do
        return;
    }

    auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
    dht_.connectivityChanged();
}

bool
RingAccount::findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (auto cert = tls::CertificateStore::instance().getCertificate(h.toString())) {
        if (cb)
            cb(cert);
    } else {
        dht_.findCertificate(h, [cb](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (crt)
                tls::CertificateStore::instance().pinCertificate(crt);
            if (cb)
                cb(crt);
        });
    }
    return true;
}

bool
RingAccount::findCertificate(const std::string& crt_id)
{
    findCertificate(dht::InfoHash(crt_id));
    return true;
}

bool
RingAccount::setCertificateStatus(const std::string& cert_id, tls::TrustStore::PermissionStatus status)
{
    if (contacts_.find(dht::InfoHash(cert_id)) != contacts_.end()) {
        RING_DBG("Can't set certificate status for existing contacts %s", cert_id.c_str());
        return false;
    }
    findCertificate(cert_id);
    bool done = trust_.setCertificateStatus(cert_id, status);
    if (done)
        emitSignal<DRing::ConfigurationSignal::CertificateStateChanged>(getAccountID(), cert_id, tls::TrustStore::statusToStr(status));
    return done;
}

std::vector<std::string>
RingAccount::getCertificatesByStatus(tls::TrustStore::PermissionStatus status)
{
    return trust_.getCertificatesByStatus(status);
}

template<typename ID=dht::Value::Id>
std::set<ID>
loadIdList(const std::string& path)
{
    std::set<ID> ids;
    std::ifstream file(path);
    if (!file.is_open()) {
        RING_DBG("Could not load %s", path.c_str());
        return ids;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        ID vid;
        if (!(iss >> std::hex >> vid)) { break; }
        ids.insert(vid);
    }
    return ids;
}

template<typename ID=dht::Value::Id>
void
saveIdList(const std::string& path, const std::set<ID>& ids)
{
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        RING_ERR("Could not save to %s", path.c_str());
        return;
    }
    for (auto& c : ids)
        file << std::hex << c << "\n";
}

void
RingAccount::loadTreatedCalls()
{
    treatedCalls_ = loadIdList(cachePath_+DIR_SEPARATOR_STR "treatedCalls");
}

void
RingAccount::saveTreatedCalls() const
{
    fileutils::check_dir(cachePath_.c_str());
    saveIdList(cachePath_+DIR_SEPARATOR_STR "treatedCalls", treatedCalls_);
}

void
RingAccount::loadTreatedMessages()
{
    std::lock_guard<std::mutex> lock(messageMutex_);
    treatedMessages_ = loadIdList(cachePath_+DIR_SEPARATOR_STR "treatedMessages");
}

void
RingAccount::saveTreatedMessages() const
{
    ThreadPool::instance().run([w = weak()](){
        if (auto sthis = w.lock()) {
            auto& this_ = *sthis;
            std::lock_guard<std::mutex> lock(this_.messageMutex_);
            fileutils::check_dir(this_.cachePath_.c_str());
            saveIdList(this_.cachePath_+DIR_SEPARATOR_STR "treatedMessages", this_.treatedMessages_);
        }
    });
}

bool
RingAccount::isMessageTreated(unsigned int id)
{
    std::lock_guard<std::mutex> lock(messageMutex_);
    auto res = treatedMessages_.insert(id);
    if (res.second) {
        saveTreatedMessages();
        return false;
    }
    return true;
}

void
RingAccount::loadKnownDevices()
{
    std::map<dht::InfoHash, std::pair<std::string, uint64_t>> knownDevices;
    try {
        // read file
        auto file = fileutils::loadFile("knownDevicesNames", idPath_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(knownDevices);
    } catch (const std::exception& e) {
        RING_WARN("[Account %s] error loading devices: %s", getAccountID().c_str(), e.what());
        return;
    }

    for (const auto& d : knownDevices) {
        RING_DBG("[Account %s] loading known account device %s %s", getAccountID().c_str(),
                                                                    d.second.first.c_str(),
                                                                    d.first.toString().c_str());
        if (auto crt = tls::CertificateStore::instance().getCertificate(d.first.toString())) {
            if (not foundAccountDevice(crt, d.second.first, clock::from_time_t(d.second.second)))
                RING_WARN("[Account %s] can't add device %s", getAccountID().c_str(), d.first.toString().c_str());
        }
        else {
            RING_WARN("[Account %s] can't find certificate for device %s", getAccountID().c_str(), d.first.toString().c_str());
        }
    }
}

void
RingAccount::saveKnownDevices() const
{
    std::ofstream file(idPath_+DIR_SEPARATOR_STR "knownDevicesNames", std::ios::trunc | std::ios::binary);

    std::map<dht::InfoHash, std::pair<std::string, uint64_t>> devices;
    for (const auto& id : knownDevices_)
        devices.emplace(id.first, std::make_pair(id.second.name, clock::to_time_t(id.second.last_sync)));

    msgpack::pack(file, devices);
}

std::map<std::string, std::string>
RingAccount::getKnownDevices() const
{
    std::map<std::string, std::string> ids;
    for (auto& d : knownDevices_) {
        auto id = d.first.toString();
        auto label = d.second.name.empty() ? id.substr(0, 8) : d.second.name;
        ids.emplace(std::move(id), std::move(label));
    }
    return ids;
}

void
RingAccount::saveNodes(const std::vector<dht::NodeExport>& nodes) const
{
    if (nodes.empty())
        return;
    fileutils::check_dir(cachePath_.c_str());
    std::string nodesPath = cachePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::lock_guard<std::mutex> lock(fileutils::getFileLock(nodesPath));
        std::ofstream file(nodesPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            RING_ERR("Could not save nodes to %s", nodesPath.c_str());
            return;
        }
        for (auto& n : nodes)
            file << n.id << " " << IpAddr(n.ss).toString(true) << "\n";
    }
}

void
RingAccount::saveValues(const std::vector<dht::ValuesExport>& values) const
{
    std::lock_guard<std::mutex> lock(dhtValuesMtx_);
    fileutils::check_dir(dataPath_.c_str());
    for (const auto& v : values) {
        const std::string fname = dataPath_ + DIR_SEPARATOR_STR + v.first.toString();
        std::ofstream file(fname, std::ios::trunc | std::ios::out | std::ios::binary);
        file.write((const char*)v.second.data(), v.second.size());
    }
}

std::vector<dht::NodeExport>
RingAccount::loadNodes() const
{
    std::vector<dht::NodeExport> nodes;
    std::string nodesPath = cachePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::lock_guard<std::mutex> lock(fileutils::getFileLock(nodesPath));
        std::ifstream file(nodesPath);
        if (!file.is_open()) {
            RING_DBG("Could not load nodes from %s", nodesPath.c_str());
            return nodes;
        }
        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string id, ipstr;
            if (!(iss >> id >> ipstr)) { break; }
            IpAddr ip {ipstr};
            dht::NodeExport e {dht::InfoHash(id), ip, ip.getLength()};
            nodes.push_back(e);
        }
    }
    return nodes;
}

std::vector<dht::ValuesExport>
RingAccount::loadValues() const
{
    std::lock_guard<std::mutex> lock(dhtValuesMtx_);
    std::vector<dht::ValuesExport> values;
    const auto dircontent(fileutils::readDirectory(dataPath_));
    for (const auto& fname : dircontent) {
        const auto file = dataPath_+DIR_SEPARATOR_STR+fname;
        try {
            std::ifstream ifs(file, std::ifstream::in | std::ifstream::binary);
            std::istreambuf_iterator<char> begin(ifs), end;
            values.emplace_back(dht::ValuesExport{dht::InfoHash(fname), std::vector<uint8_t>{begin, end}});
        } catch (const std::exception& e) {
            RING_ERR("[Account %s] error reading value from cache : %s", getAccountID().c_str(), e.what());
        }
        fileutils::remove(file);
    }
    RING_DBG("[Account %s] loaded %zu values", getAccountID().c_str(), values.size());
    return values;
}

tls::DhParams
RingAccount::loadDhParams(const std::string path)
{
    try {
        // writeTime throw exception if file doesn't exist
        auto duration = clock::now() - fileutils::writeTime(path);
        if (duration >= std::chrono::hours(24 * 3)) // file is valid only 3 days
            throw std::runtime_error("file too old");

        RING_DBG("Loading DhParams from file '%s'", path.c_str());
        return {fileutils::loadFile(path)};
    } catch (const std::exception& e) {
        RING_DBG("Failed to load DhParams file '%s': %s", path.c_str(), e.what());
        if (auto params = tls::DhParams::generate()) {
            try {
                fileutils::saveFile(path, params.serialize(), 0600);
                RING_DBG("Saved DhParams to file '%s'", path.c_str());
            } catch (const std::exception& ex) {
                RING_WARN("Failed to save DhParams in file '%s': %s", path.c_str(), ex.what());
            }
            return params;
        }
        RING_ERR("Can't generate DH params.");
        return {};
    }
}

std::string
RingAccount::getDhtProxyServer()
{
    if (!proxyEnabled_) return {};
    if (proxyServerCached_.empty()) {
        std::vector<std::string> proxys;
        // Split the list of servers
        std::sregex_iterator begin = {proxyServer_.begin(), proxyServer_.end(), PROXY_REGEX}, end;
        for (auto it = begin; it != end; ++it) {
            auto &match = *it;
            if (match[5].matched and match[6].matched) {
                try {
                    auto start = std::stoi(match[5]), end = std::stoi(match[6]);
                    for (auto p = start; p <= end; p++)
                      proxys.emplace_back(match[1].str() + match[2].str() + ":" +
                                          std::to_string(p));
                } catch (...) {
                    RING_WARN("Malformed proxy, ignore it");
                    continue;
                }
            } else {
                proxys.emplace_back(match[0].str());
            }
        }
        if (proxys.empty())
          return {};
        // Select one of the list as the current proxy.
        auto randIt = proxys.begin();
        std::advance(randIt, std::rand() % proxys.size());
        proxyServerCached_ = *randIt;
        // Cache it!
        fileutils::check_dir(cachePath_.c_str(), 0700);
        std::string proxyCachePath = cachePath_ + DIR_SEPARATOR_STR "dhtproxy";
        std::ofstream file(proxyCachePath);
        RING_DBG("Cache DHT proxy server: %s", proxyServerCached_.c_str());
        if (file.is_open())
            file << proxyServerCached_;
        else
            RING_WARN("Cannot write into %s", proxyCachePath.c_str());
        return proxyServerCached_;
    }
    return proxyServerCached_;
}

void
RingAccount::generateDhParams()
{
    //make sure cachePath_ is writable
    fileutils::check_dir(cachePath_.c_str(), 0700);
    dhParams_ = ThreadPool::instance().get<tls::DhParams>(std::bind(loadDhParams, cachePath_ + DIR_SEPARATOR_STR "dhParams"));
}

MatchRank
RingAccount::matches(const std::string &userName, const std::string &server) const
{
    if (userName == ringAccountId_ || server == ringAccountId_ || userName == ringDeviceId_) {
        RING_DBG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::FULL;
    } else {
        return MatchRank::NONE;
    }
}

std::string
RingAccount::getFromUri() const
{
    const std::string uri = "<sip:" + ringAccountId_ + "@ring.dht>";
    if (not displayName_.empty())
        return "\"" + displayName_ + "\" " + uri;
    RING_DBG("getFromUri %s", uri.c_str());
    return uri;
}

std::string
RingAccount::getToUri(const std::string& to) const
{
    RING_DBG("getToUri %s", to.c_str());
    return "<sips:" + to + ";transport=dtls>";
}

pj_str_t
RingAccount::getContactHeader(pjsip_transport* t)
{
    std::string quotedDisplayName = "\"" + displayName_ + "\" " + (displayName_.empty() ? "" : " ");
    if (t) {
        // FIXME: be sure that given transport is from SipIceTransport
        auto tlsTr = reinterpret_cast<tls::SipsIceTransport::TransportData*>(t)->self;
        auto address = tlsTr->getLocalAddress().toString(true);
        contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                         "%s<sips:%s%s%s;transport=dtls>",
                                         quotedDisplayName.c_str(),
                                         identity_.second->getId().toString().c_str(),
                                         (address.empty() ? "" : "@"),
                                         address.c_str());
    } else {
        RING_ERR("getContactHeader: no SIP transport provided");
        contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                         "%s<sips:%s@ring.dht>",
                                         quotedDisplayName.c_str(),
                                         identity_.second->getId().toString().c_str());
    }
    return contact_;
}

/* contacts */

void
RingAccount::addContact(const std::string& uri, bool confirmed)
{
    dht::InfoHash h (uri);
    if (not h) {
        RING_ERR("[Account %s] addContact: invalid contact URI", getAccountID().c_str());
        return;
    }
    addContact(h, confirmed);
}

void
RingAccount::addContact(const dht::InfoHash& h, bool confirmed)
{
    RING_WARN("[Account %s] addContact: %s", getAccountID().c_str(), h.to_c_str());
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact{}).first;
    else if (c->second.isActive() and c->second.confirmed == confirmed)
        return;
    c->second.added = std::time(nullptr);
    c->second.confirmed = confirmed or c->second.confirmed;
    auto hStr = h.toString();
    trust_.setCertificateStatus(hStr, tls::TrustStore::PermissionStatus::ALLOWED);
    saveContacts();
    emitSignal<DRing::ConfigurationSignal::ContactAdded>(getAccountID(), hStr, c->second.confirmed);
    syncDevices();
}

void
RingAccount::removeContact(const std::string& uri, bool ban)
{
    RING_WARN("[Account %s] removeContact: %s", getAccountID().c_str(), uri.c_str());
    dht::InfoHash h (uri);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact{}).first;
    else if (not c->second.isActive() and c->second.banned == ban)
        return;
    c->second.removed = std::time(nullptr);
    c->second.banned = ban;
    trust_.setCertificateStatus(uri, ban ? tls::TrustStore::PermissionStatus::BANNED
                                         : tls::TrustStore::PermissionStatus::UNDEFINED);
    if (ban and trustRequests_.erase(h) > 0)
        saveTrustRequests();
    saveContacts();
    emitSignal<DRing::ConfigurationSignal::ContactRemoved>(getAccountID(), uri, ban);
    syncDevices();
}

std::map<std::string, std::string>
RingAccount::getContactDetails(const std::string& uri) const
{
    dht::InfoHash h (uri);

    const auto c = contacts_.find(h);
    if (c == std::end(contacts_)) {
        RING_WARN("[dht] contact '%s' not found", uri.c_str());
        return {};
    }

    auto details = c->second.toMap();
    if (not details.empty())
        details["id"] = c->first.toString();

    return details;
}

std::vector<std::map<std::string, std::string>>
RingAccount::getContacts() const
{
    std::vector<std::map<std::string, std::string>> ret;
    ret.reserve(contacts_.size());

    for (const auto& c : contacts_) {
        auto details = c.second.toMap();
        if (not details.empty()) {
            details["id"] = c.first.toString();
            ret.emplace_back(std::move(details));
        }
    }
    return ret;
}

void
RingAccount::updateContact(const dht::InfoHash& id, const Contact& contact)
{
    if (not id) {
        RING_ERR("[Account %s] updateContact: invalid contact ID", getAccountID().c_str());
        return;
    }
    bool stateChanged {false};
    auto c = contacts_.find(id);
    if (c == contacts_.end()) {
        RING_DBG("[Account %s] new contact: %s", getAccountID().c_str(), id.toString().c_str());
        c = contacts_.emplace(id, contact).first;
        stateChanged = c->second.isActive() or c->second.isBanned();
    } else {
        RING_DBG("[Account %s] updated contact: %s", getAccountID().c_str(), id.toString().c_str());
        stateChanged = c->second.update(contact);
    }
    if (stateChanged) {
        if (c->second.isActive()) {
            trust_.setCertificateStatus(id.toString(), tls::TrustStore::PermissionStatus::ALLOWED);
            emitSignal<DRing::ConfigurationSignal::ContactAdded>(getAccountID(), id.toString(), c->second.confirmed);
        } else {
            if (c->second.banned)
                trust_.setCertificateStatus(id.toString(), tls::TrustStore::PermissionStatus::BANNED);
            emitSignal<DRing::ConfigurationSignal::ContactRemoved>(getAccountID(), id.toString(), c->second.banned);
        }
    }
}

void
RingAccount::loadContacts()
{
    decltype(contacts_) contacts;
    try {
        // read file
        auto file = fileutils::loadFile("contacts", idPath_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(contacts);
    } catch (const std::exception& e) {
        RING_WARN("[Account %s] error loading contacts: %s", getAccountID().c_str(), e.what());
        return;
    }

    for (auto& peer : contacts)
        updateContact(peer.first, peer.second);
}

void
RingAccount::saveContacts() const
{
    std::ofstream file(idPath_+DIR_SEPARATOR_STR "contacts", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, contacts_);
}

/* trust requests */

std::vector<std::map<std::string, std::string>>
RingAccount::getTrustRequests() const
{
    using Map = std::map<std::string, std::string>;
    std::vector<Map> ret;
    ret.reserve(trustRequests_.size());
    for (const auto& r : trustRequests_) {
        ret.emplace_back(Map {
            {DRing::Account::TrustRequest::FROM, r.first.toString()},
            {DRing::Account::TrustRequest::RECEIVED, std::to_string(r.second.received)},
            {DRing::Account::TrustRequest::PAYLOAD, std::string(r.second.payload.begin(), r.second.payload.end())}
        });
    }
    return ret;
}

bool
RingAccount::acceptTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    if (not f)
        return false;

    // The contact sent us a TR so we are in its contact list
    addContact(f, true);

    auto i = trustRequests_.find(f);
    if (i == trustRequests_.end())
        return false;

    // Clear trust request
    auto treq = std::move(i->second);
    trustRequests_.erase(i);
    saveTrustRequests();

    // Send confirmation
    sendTrustRequestConfirm(f);
    return true;
}

bool
RingAccount::discardTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    if (trustRequests_.erase(f) > 0) {
        saveTrustRequests();
        return true;
    }
    return false;
}

void
RingAccount::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    auto toH = dht::InfoHash(to);
    if (not toH) {
        RING_ERR("[Account %s] can't send trust request to invalid hash: %s", getAccountID().c_str(), to.c_str());
        return;
    }
    addContact(toH);
    forEachDevice(toH, [toH,payload](const std::shared_ptr<RingAccount>& shared, const dht::InfoHash& dev)
    {
        RING_WARN("[Account %s] sending trust request to: %s / %s", shared->getAccountID().c_str(), toH.toString().c_str(), dev.toString().c_str());
        shared->dht_.putEncrypted(dht::InfoHash::get("inbox:"+dev.toString()),
                          dev,
                          dht::TrustRequest(DHT_TYPE_NS, payload));
    });
}

void
RingAccount::sendTrustRequestConfirm(const dht::InfoHash& to)
{
    dht::TrustRequest answer {DHT_TYPE_NS};
    answer.confirm = true;
    forEachDevice(to, [to,answer](const std::shared_ptr<RingAccount>& shared, const dht::InfoHash& dev)
    {
        RING_WARN("[Account %s] sending trust request reply: %s / %s", shared->getAccountID().c_str(), to.toString().c_str(), dev.toString().c_str());
        shared->dht_.putEncrypted(dht::InfoHash::get("inbox:"+dev.toString()), dev, answer);
    });
}

void
RingAccount::saveTrustRequests() const
{
    std::ofstream file(idPath_+DIR_SEPARATOR_STR "incomingTrustRequests", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, trustRequests_);
}

void
RingAccount::loadTrustRequests()
{
    std::map<dht::InfoHash, TrustRequest> requests;
    try {
        // read file
        auto file = fileutils::loadFile("incomingTrustRequests", idPath_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(requests);
    } catch (const std::exception& e) {
        RING_WARN("[Account %s] error loading trust requests: %s", getAccountID().c_str(), e.what());
        return;
    }

    for (auto& tr : requests)
        onTrustRequest(tr.first, tr.second.device, tr.second.received, false, std::move(tr.second.payload));
}

/* sync */

void
RingAccount::syncDevices()
{
    RING_DBG("[Account %s] building device sync from %s %s", getAccountID().c_str(), ringDeviceName_.c_str(), ringDeviceId_.c_str());
    DeviceSync sync_data;
    sync_data.date = clock::now().time_since_epoch().count();
    sync_data.device_name = ringDeviceName_;
    sync_data.peers = contacts_;

    static const size_t MAX_TRUST_REQUESTS = 20;
    if (trustRequests_.size() <= MAX_TRUST_REQUESTS)
        for (const auto& req : trustRequests_)
            sync_data.trust_requests.emplace(req.first, TrustRequest{req.second.device, req.second.received, {}});
    else {
        size_t inserted = 0;
        auto req = trustRequests_.lower_bound(dht::InfoHash::getRandom());
        while (inserted++ < MAX_TRUST_REQUESTS) {
            if (req == trustRequests_.end())
                req = trustRequests_.begin();
            sync_data.trust_requests.emplace(req->first, TrustRequest{req->second.device, req->second.received, {}});
            ++req;
        }
    }

    for (const auto& dev : knownDevices_) {
        if (dev.first.toString() == ringDeviceId_)
            sync_data.devices_known.emplace(dev.first, ringDeviceName_);
        else
            sync_data.devices_known.emplace(dev.first, dev.second.name);
    }
    for (const auto& dev : knownDevices_) {
        // don't send sync data to ourself
        if (dev.first.toString() == ringDeviceId_)
            continue;
        RING_DBG("[Account %s] sending device sync to %s %s", getAccountID().c_str(), dev.second.name.c_str(), dev.first.toString().c_str());
        auto syncDeviceKey = dht::InfoHash::get("inbox:"+dev.first.toString());
        dht_.putEncrypted(syncDeviceKey, dev.first, sync_data);
    }
}

void
RingAccount::onReceiveDeviceSync(DeviceSync&& sync)
{
    auto it = knownDevices_.find(sync.from);
    if (it == knownDevices_.end()) {
        RING_WARN("[Account %s] dropping sync data from unknown device", getAccountID().c_str());
        return;
    }
    auto sync_date = clock::time_point(clock::duration(sync.date));
    if (it->second.last_sync >= sync_date) {
        RING_DBG("[Account %s] dropping outdated sync data", getAccountID().c_str());
        return;
    }

    // Sync known devices
    RING_DBG("[Account %s] received device sync data (%lu devices, %lu contacts)", getAccountID().c_str(), sync.devices_known.size(), sync.peers.size());
    for (const auto& d : sync.devices_known) {
        findCertificate(d.first, [this,d](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (not crt)
                return;
            foundAccountDevice(crt, d.second);
        });
    }
    saveKnownDevices();

    // Sync contacts
    for (const auto& peer : sync.peers)
        updateContact(peer.first, peer.second);
    saveContacts();

    // Sync trust requests
    for (const auto& tr : sync.trust_requests)
        onTrustRequest(tr.first, tr.second.device, tr.second.received, false, {});

    it->second.last_sync = sync_date;
}

void
RingAccount::igdChanged()
{
    if (not dht_.isRunning())
        return;
    if (upnp_) {
        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        std::thread{[shared] {
            auto& this_ = *shared.get();
            auto oldPort = static_cast<in_port_t>(this_.dhtPortUsed_);
            if (not this_.mapPortUPnP())
                RING_WARN("UPnP: Could not map DHT port");
            auto newPort = static_cast<in_port_t>(this_.dhtPortUsed_);
            if (oldPort != newPort) {
                RING_WARN("DHT port changed: restarting network");
                this_.doRegister_();
            } else
                this_.dht_.connectivityChanged();
        }}.detach();
    } else
        dht_.connectivityChanged();
}

void
RingAccount::forEachDevice(const dht::InfoHash& to,
                           std::function<void(const std::shared_ptr<RingAccount>&,
                                              const dht::InfoHash&)> op,
                           std::function<void(const std::shared_ptr<RingAccount>&, bool)> end)
{
    auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
    auto treatedDevices = std::make_shared<std::set<dht::InfoHash>>();
    dht_.get<dht::crypto::RevocationList>(to, [to](dht::crypto::RevocationList&& crl){
        tls::CertificateStore::instance().pinRevocationList(to.toString(), std::move(crl));
        return true;
    });
    dht_.get<DeviceAnnouncement>(to, [shared,to,treatedDevices,op](DeviceAnnouncement&& dev) {
        if (dev.from != to)
            return true;
        if (treatedDevices->emplace(dev.dev).second)
            op(shared, dev.dev);
        return true;
    }, [=](bool /*ok*/){
        RING_DBG("[Account %s] found %lu devices for %s",
                 getAccountID().c_str(), treatedDevices->size(), to.to_c_str());
        if (end) end(shared, not treatedDevices->empty());
    });
}

void
RingAccount::sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads, uint64_t token)
{
    if (to.empty() or payloads.empty()) {
        messageEngine_.onMessageSent(to, token, false);
        return;
    }
    if (payloads.size() != 1) {
        // Multi-part message
        // TODO: not supported yet
        RING_ERR("Multi-part im is not supported yet by RingAccount");
        messageEngine_.onMessageSent(to, token, false);
        return;
    }

    std::string toUri;

    try {
        toUri = parseRingUri(to);
    }
    catch (...) {
        RING_ERR("Failed to send a text message due to an invalid URI %s", to.c_str());
        messageEngine_.onMessageSent(to, token, false);
        return;
    }

    auto toH = dht::InfoHash(toUri);
    auto now = clock::to_time_t(clock::now());

    struct PendingConfirmation {
        bool replied {false};
        std::map<dht::InfoHash, std::future<size_t>> listenTokens {};
    };
    auto confirm = std::make_shared<PendingConfirmation>();

    // Find listening devices for this account
    forEachDevice(toH, [confirm,to,token,payloads,now](const std::shared_ptr<RingAccount>& this_, const dht::InfoHash& dev)
    {
        {
            std::lock_guard<std::mutex> lock(this_->messageMutex_);
            auto e = this_->sentMessages_.emplace(token, PendingMessage {});
            e.first->second.to = dev;
        }

        auto h = dht::InfoHash::get("inbox:"+dev.toString());
        std::weak_ptr<RingAccount> w = this_;
        auto list_token = this_->dht_.listen<dht::ImMessage>(h, [w,token,confirm](dht::ImMessage&& msg) {
            if (auto sthis = w.lock()) {
                auto& this_ = *sthis;
                // check expected message confirmation
                if (msg.id != token)
                    return true;

                {
                    std::lock_guard<std::mutex> lock(this_.messageMutex_);
                    auto e = this_.sentMessages_.find(msg.id);
                    if (e == this_.sentMessages_.end() or e->second.to != msg.from) {
                        RING_DBG() << "[Account " << this_.getAccountID() << "] [message " << token << "] Message not found";
                        return true;
                    }
                    this_.sentMessages_.erase(e);
                    RING_DBG() << "[Account " << this_.getAccountID() << "] [message " << token << "] Received text message reply";

                    // add treated message
                    auto res = this_.treatedMessages_.insert(msg.id);
                    if (!res.second)
                        return true;
                }
                this_.saveTreatedMessages();

                // report message as confirmed received
                for (auto& t : confirm->listenTokens)
                    this_.dht_.cancelListen(t.first, t.second.get());
                confirm->listenTokens.clear();
                confirm->replied = true;
                this_.messageEngine_.onMessageSent(msg.from.toString(), token, true);
            }
            return false;
        });
        confirm->listenTokens.emplace(h, std::move(list_token));
        this_->dht_.putEncrypted(h, dev,
            dht::ImMessage(token, std::string(payloads.begin()->first), std::string(payloads.begin()->second), now),
            [w,to,token,confirm,h](bool ok) {
                if (auto this_ = w.lock()) {
                    RING_DBG() << "[Account " << this_->getAccountID() << "] [message " << token << "] Put encrypted " << (ok ? "ok" : "failed");
                    if (not ok) {
                        auto lt = confirm->listenTokens.find(h);
                        if (lt != confirm->listenTokens.end()) {
                            this_->dht_.cancelListen(h, lt->second.get());
                            confirm->listenTokens.erase(lt);
                        }
                        if (confirm->listenTokens.empty() and not confirm->replied)
                            this_->messageEngine_.onMessageSent(to, token, false);
                    }
                }
            });

        RING_DBG() << "[Account " << this_->getAccountID() << "] [message " << token << "] Sending message for device " << dev.toString();
    }, [to, token](const std::shared_ptr<RingAccount>& shared, bool ok) {
        if (not ok) {
            shared->messageEngine_.onMessageSent(to, token, false);
        }
    });

    // Timeout cleanup
    Manager::instance().scheduleTask([w=weak(), confirm, to, token]() {
        if (not confirm->replied) {
            if (auto this_ = w.lock()) {
                RING_DBG() << "[Account " << this_->getAccountID() << "] [message " << token << "] Timeout";
                for (auto& t : confirm->listenTokens)
                    this_->dht_.cancelListen(t.first, t.second.get());
                confirm->listenTokens.clear();
                confirm->replied = true;
                this_->messageEngine_.onMessageSent(to, token, false);
            }
        }
    }, std::chrono::steady_clock::now() + std::chrono::minutes(1));
}

void
RingAccount::registerDhtAddress(IceTransport& ice)
{
    const auto reg_addr = [&](IceTransport& ice, const IpAddr& ip) {
            RING_DBG("[Account %s] using public IP: %s", getAccountID().c_str(), ip.toString().c_str());
            for (unsigned compId = 1; compId <= ice.getComponentCount(); ++compId)
                ice.registerPublicIP(compId, ip);
            return ip;
        };

    auto ip = getPublishedAddress();
    if (ip.empty()) {
        // We need a public address in case of NAT'ed network
        // Trying to use one discovered by DHT service

        // IPv6 (sdp support only one IP, put IPv6 before IPv4 as this last has the priority over IPv6 less NAT'able)
        const auto& addr6 = dht_.getPublicAddress(AF_INET6);
        if (addr6.size())
            setPublishedAddress(reg_addr(ice, *addr6[0].get()));

        // IPv4
        const auto& addr4 = dht_.getPublicAddress(AF_INET);
        if (addr4.size())
            setPublishedAddress(reg_addr(ice, *addr4[0].get()));
    } else {
        reg_addr(ice, ip);
    }
}

std::vector<std::string>
RingAccount::publicAddresses()
{
    std::vector<std::string> addresses;
    for (auto& addr : dht_.getPublicAddress(AF_INET)) {
        addresses.emplace_back(addr.toString());
    }
    for (auto& addr : dht_.getPublicAddress(AF_INET6)) {
        addresses.emplace_back(addr.toString());
    }
    return addresses;
}

void
RingAccount::requestPeerConnection(const std::string& peer_id, const DRing::DataTransferId& tid,
                                   std::function<void(PeerConnection*)> connect_cb)
{
    dhtPeerConnector_->requestConnection(peer_id, tid, connect_cb);
}

void
RingAccount::closePeerConnection(const std::string& peer, const DRing::DataTransferId& tid)
{
    dhtPeerConnector_->closeConnection(peer, tid);
}

void
RingAccount::enableProxyClient(bool enable)
{
    RING_WARN("[Account %s] DHT proxy client: %s", getAccountID().c_str(), enable ? "enable" : "disable");
    dht_.enableProxy(enable);
}

void RingAccount::setPushNotificationToken(const std::string& token)
{
    RING_WARN("[Account %s] setPushNotificationToken: %s", getAccountID().c_str(), token.c_str());
    deviceKey_ = token;
    dht_.setPushNotificationToken(deviceKey_);
}

/**
 * To be called by clients with relevant data when a push notification is received.
 */
void RingAccount::pushNotificationReceived(const std::string& from, const std::map<std::string, std::string>& data)
{
    RING_WARN("[Account %s] pushNotificationReceived: %s", getAccountID().c_str(), from.c_str());
    dht_.pushNotificationReceived(data);
}


std::string
RingAccount::getUserUri() const
{
#ifdef HAVE_RINGNS
    if (not registeredName_.empty())
        return RING_URI_PREFIX + registeredName_;
#endif
    return username_;
}


std::vector<DRing::Message>
RingAccount::getLastMessages(const uint64_t& base_timestamp)
{
    return SIPAccountBase::getLastMessages(base_timestamp);
}

void
RingAccount::checkPendingCallsTask()
{
    bool hasHandler = eventHandler and not eventHandler->isCancelled();
    if (not pendingCalls_.empty() and not hasHandler) {
        eventHandler = Manager::instance().scheduler().scheduleAtFixedRate([w = weak()] {
            if (auto this_ = w.lock())
                return this_->handlePendingCallList();
            return false;
        }, std::chrono::milliseconds(10));
    } else if (pendingCalls_.empty() and hasHandler) {
        eventHandler->cancel();
        eventHandler.reset();
    }
}

} // namespace ring
