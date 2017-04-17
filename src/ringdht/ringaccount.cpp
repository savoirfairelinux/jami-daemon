/*
 *  Copyright (C) 2014-2017 Savoir-faire Linux Inc.
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

#include "ringaccount.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "thread_pool.h"

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "sip/sip_utils.h"

#include "sips_transport_ice.h"
#include "ice_transport.h"
#include "contactsmanager.h"

#include "client/ring_signal.h"
#include "dring/call_const.h"
#include "dring/account_const.h"

#include "upnp/upnp_control.h"
#include "system_codec_container.h"

#include "account_schema.h"
#include "logger.h"
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
#include <memory>
#include <sstream>
#include <cctype>
#include <cstdarg>
#include <string>

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
}

void
setState (const std::string& accountID,
          const State migrationState)
{
    emitSignal<DRing::ConfigurationSignal::MigrationEnded>(accountID,
        mapStateNumberToString(migrationState));
}
}

struct RingAccount::BuddyInfo
{
    /* the buddy id */
    dht::InfoHash id;

    /* the presence timestamps */
    std::map<dht::InfoHash, std::chrono::steady_clock::time_point> devicesTimestamps;

    /* The callable object to update buddy info */
    std::function<void()> updateInfo {};

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
 * Represents a known device attached to this Ring account
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
 * Crypto material contained in the archive,
 * not persisted in the account configuration
 */
struct RingAccount::ArchiveContent
{
    /** Account main private key and certificate chain */
    dht::crypto::Identity id;

    /** Generated CA key (for self-signed certificates) */
    std::shared_ptr<dht::crypto::PrivateKey> ca_key;

    /** Revoked devices */
    std::shared_ptr<dht::crypto::RevocationList> revoked;

    /** Ethereum private key */
    std::vector<uint8_t> eth_key;

    /** Contacts */
    std::map<dht::InfoHash, Contact> contacts;

    /** Account configuration */
    std::map<std::string, std::string> config;
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
static constexpr const char * DEFAULT_TURN_SERVER = "turn.ring.cx";
static constexpr const char * DEFAULT_TURN_USERNAME = "ring";
static constexpr const char * DEFAULT_TURN_PWD = "ring";
static constexpr const char * DEFAULT_TURN_REALM = "ring";

constexpr const char* const RingAccount::ACCOUNT_TYPE;
/* constexpr */ const std::pair<uint16_t, uint16_t> RingAccount::DHT_PORT_RANGE {4000, 8888};

static std::uniform_int_distribution<dht::Value::Id> udist;

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
{
    // Force the SFL turn server if none provided yet
    turnServer_ = DEFAULT_TURN_SERVER;
    turnServerUserName_ = DEFAULT_TURN_USERNAME;
    turnServerPwd_ = DEFAULT_TURN_PWD;
    turnServerRealm_ = DEFAULT_TURN_REALM;
    turnEnabled_ = true;
    contactsManager_ = new ContactsManager(accountID);
}

RingAccount::~RingAccount()
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
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
RingAccount::newIncomingCall(const std::string& from)
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
RingAccount::newOutgoingCall(const std::string& toUrl)
{
    auto sufix = stripPrefix(toUrl);
    RING_DBG("Calling DHT peer %s", sufix.c_str());
    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, RingAccount>(*this, manager.getNewCallID(),
                                                                  Call::CallType::OUTGOING);

    call->setIPToIP(true);
    call->setSecure(isTlsEnabled());
    call->initRecFilename(toUrl);

    try {
        const std::string toUri = parseRingUri(sufix);
        startOutgoingCall(call, toUri);
    } catch (...) {
#if HAVE_RINGNS
        std::weak_ptr<RingAccount> wthis_ = std::static_pointer_cast<RingAccount>(shared_from_this());
        NameDirectory::lookupUri(sufix, nameServer_, [wthis_,call](const std::string& result,
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
    call->setState(Call::ConnectionState::TRYING);
    std::weak_ptr<SIPCall> wCall = call;

    // Find listening Ring devices for this account
    forEachDevice(dht::InfoHash(toUri), [wCall](const std::shared_ptr<RingAccount>& sthis, const dht::InfoHash& dev)
    {
        auto call = wCall.lock();
        if (not call) return;
        RING_WARN("[call %s] Found device %s", call->getCallId().c_str(), dev.toString().c_str());

        auto& manager = Manager::instance();
        auto dev_call = manager.callFactory.newCall<SIPCall, RingAccount>(*sthis, manager.getNewCallID(),
                                                                          Call::CallType::OUTGOING);
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

        manager.addTask([sthis, weak_dev_call, ice, dev] {
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
            const dht::Value::Id callvid  = udist(sthis->rand_);
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
                    RING_WARN("ICE request replied from DHT peer %s\n%s", dev.toString().c_str(),
                              std::string(msg.ice_data.cbegin(), msg.ice_data.cend()).c_str());
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

            sthis->pendingCalls_.emplace_back(PendingCall{
                std::chrono::steady_clock::now(),
                ice, weak_dev_call,
                std::move(listenKey),
                callkey, dev,
                nullptr
            });
            return false;
        });
    }, [=](bool ok){
        if (not ok) {
            if (auto call = wCall.lock())
                call->onFailure();
        }
    });
}

void
RingAccount::createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to_id, IpAddr target)
{
    RING_WARN("RingAccount::createOutgoingCall to: %s target: %s",
              to_id.c_str(), target.toString(true).c_str());
    call->initIceMediaTransport(true);
    call->setIPToIP(true);
    call->setPeerNumber(getToUri(to_id+"@"+target.toString(true).c_str()));
    call->initRecFilename(to_id);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface());
    call->setCallMediaLocal(call->getIceMediaTransport()->getDefaultLocalAddress());

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
    auto& sdp = call->getSDP();

    sdp.setPublishedIP(addrSdp);
    const bool created = sdp.createOffer(
                            getActiveAccountCodecInfoList(MEDIA_AUDIO),
                            getActiveAccountCodecInfoList(videoEnabled_ ? MEDIA_VIDEO : MEDIA_NONE),
                            getSrtpKeyExchange()
                         );

    if (not created or not SIPStartCall(call, target))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
}

std::shared_ptr<Call>
RingAccount::newOutgoingCall(const std::string& toUrl)
{
    return newOutgoingCall<SIPCall>(toUrl);
}

bool
RingAccount::SIPStartCall(const std::shared_ptr<SIPCall>& call, IpAddr target)
{
    call->setupLocalSDPFromIce();
    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    std::string targetStr = getToUri(target.toString(true)/*+";transport=ICE"*/);
    pj_str_t pjTarget = pj_str((char*) targetStr.c_str());

    pj_str_t pjContact;
    {
        auto transport = call->getTransport();
        pjContact = getContactHeader(transport ? transport->get() : nullptr);
    }

    RING_DBG("contact header: %.*s / %s -> %s / %.*s",
             (int)pjContact.slen, pjContact.ptr, from.c_str(), toUri.c_str(),
             (int)pjTarget.slen, pjTarget.ptr);


    auto local_sdp = call->getSDP().getLocalSdpSession();
    pjsip_dialog* dialog {nullptr};
    pjsip_inv_session* inv {nullptr};
    if (!CreateClientDialogAndInvite(&pjFrom, &pjContact, &pjTo, &pjTarget, local_sdp, &dialog, &inv))
        return false;

    inv->mod_data[link_->getModId()] = call.get();
    call->inv.reset(inv);

/*
    updateDialogViaSentBy(dialog);
    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));
*/

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv.get(), &tdata) != PJ_SUCCESS) {
        RING_ERR("Could not initialize invite messager for this call");
        return false;
    }

    //const pjsip_tpselector tp_sel = getTransportSelector();
    const pjsip_tpselector tp_sel = {PJSIP_TPSELECTOR_TRANSPORT, {call->getTransport()->get()}};
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        RING_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    RING_ERR("Sending SIP invite");
    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        RING_ERR("Unable to send invite message for this call");
        return false;
    }

    call->setState(Call::CallState::ACTIVE, Call::ConnectionState::PROGRESSING);

    return true;
}

void RingAccount::serialize(YAML::Emitter &out)
{
    if (registrationState_ == RegistrationState::INITIALIZING)
        return;

    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << Conf::DHT_PORT_KEY << YAML::Value << dhtPort_;
    out << YAML::Key << Conf::DHT_PUBLIC_IN_CALLS << YAML::Value << dhtPublicInCalls_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_HISTORY << YAML::Value << allowPeersFromHistory_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_CONTACT << YAML::Value << allowPeersFromContact_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_TRUSTED << YAML::Value << allowPeersFromTrusted_;

#if HAVE_RINGNS
    out << YAML::Key << DRing::Account::ConfProperties::RingNS::URI << YAML::Value <<  nameServer_;
#endif

    out << YAML::Key << DRing::Account::ConfProperties::ARCHIVE_PATH << YAML::Value << archivePath_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT << YAML::Value << receipt_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT_SIG << YAML::Value << YAML::Binary(receiptSignature_.data(), receiptSignature_.size());
    out << YAML::Key << DRing::Account::ConfProperties::RING_DEVICE_NAME << YAML::Value << ringDeviceName_;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void RingAccount::unserialize(const YAML::Node &node)
{
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
    try {
        parseValue(node, DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);
    } catch (const std::exception& e) {
        RING_WARN("can't read device name: %s", e.what());
    }

    try {
        parsePath(node, DRing::Account::ConfProperties::ARCHIVE_PATH, archivePath_, idPath_);
    } catch (const std::exception& e) {
        RING_WARN("can't read archive path: %s", e.what());
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
    RING_WARN("createRingDevice");
    if (not id.second->isCA()) {
        RING_ERR("Trying to sign a certificate with a non-CA.");
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
    accountTrust_ = tls::TrustStore{};
    accountTrust_.setCertificateStatus(id.second, tls::TrustStore::PermissionStatus::ALLOWED, false);
    ringDeviceId_ = dev_id.first->getPublicKey().getId().toString();
    ringDeviceName_ = ip_utils::getHostname();
    if (ringDeviceName_.empty())
        ringDeviceName_ = ringDeviceId_.substr(8);

    receipt_ = makeReceipt(id);
    RING_WARN("createRingDevice with %s", id.first->getPublicKey().getId().toString().c_str());
    receiptSignature_ = id.first->sign({receipt_.begin(), receipt_.end()});
}

void
RingAccount::initRingDevice(const ArchiveContent& a)
{
    RING_WARN("initRingDevice");
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
    RING_WARN("making receipt");
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
    tls::TrustStore account_trust;
    account_trust.setCertificateStatus(accountCertificate, tls::TrustStore::PermissionStatus::ALLOWED, false);
    if (not account_trust.isAllowed(*identity.second)) {
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
    Json::Reader reader;
    if (!reader.parse(receipt_, root))
        return false;

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

RingAccount::ArchiveContent
RingAccount::readArchive(const std::string& pwd) const
{
    RING_DBG("[Account %s] reading account archive", getAccountID().c_str());

    std::vector<uint8_t> data;

    // Read file
    try {
        data = fileutils::loadFile(archivePath_, idPath_);
    } catch (const std::exception& e) {
        RING_ERR("[Account %s] archive loading error: %s", getAccountID().c_str(), e.what());
        throw;
    }

    // Decrypt
    try {
        data = dht::crypto::aesDecrypt(data, pwd);
    } catch (const std::exception& e) {
        RING_ERR("[Account %s] archive decrypt error: %s", getAccountID().c_str(), e.what());
        throw;
    }

    // Unserialize data
    return loadArchive(data);
}

RingAccount::ArchiveContent
RingAccount::loadArchive(const std::vector<uint8_t>& dat)
{
    ArchiveContent c;
    RING_DBG("Loading account archive (%lu bytes)", dat.size());

    std::vector<uint8_t> file;

    // Decompress
    try {
        file = archiver::decompress(dat);
    } catch (const std::exception& ex) {
        RING_ERR("Archive decompression error: %s", ex.what());
        throw std::runtime_error("failed to read file");
    }

    // Decode string
    std::string decoded {file.begin(), file.end()};
    Json::Value value;
    Json::Reader reader;
    if (!reader.parse(decoded.c_str(),value)) {
        RING_ERR("Archive JSON parsing error: %s", reader.getFormattedErrorMessages().c_str());
        throw std::runtime_error("failed to parse JSON");
    }

    // Import content
    try {
        c.config = DRing::getAccountTemplate(ACCOUNT_TYPE);
        for (Json::ValueIterator itr = value.begin() ; itr != value.end() ; itr++) {
            try {
                const auto key = itr.key().asString();
                if (key.empty())
                    continue;
                if (key.compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0) {
                } else if (key.compare(DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) == 0) {
                } else if (key.compare(DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE) == 0) {
                } else if (key.compare(Conf::RING_CA_KEY) == 0) {
                    c.ca_key = std::make_shared<dht::crypto::PrivateKey>(base64::decode(itr->asString()));
                } else if (key.compare(Conf::RING_ACCOUNT_KEY) == 0) {
                    c.id.first = std::make_shared<dht::crypto::PrivateKey>(base64::decode(itr->asString()));
                } else if (key.compare(Conf::RING_ACCOUNT_CERT) == 0) {
                    c.id.second = std::make_shared<dht::crypto::Certificate>(base64::decode(itr->asString()));
                } else if (key.compare(Conf::RING_ACCOUNT_CONTACTS) == 0) {
                    for (Json::ValueIterator citr = itr->begin() ; citr != itr->end() ; citr++) {
                        dht::InfoHash h {citr.key().asString()};
                        //if (h != dht::InfoHash{})
                          //  c.contacts.emplace(h, Contact{*citr});
                    }
                } else if (key.compare(Conf::ETH_KEY) == 0) {
                    c.eth_key = base64::decode(itr->asString());
                } else if (key.compare(Conf::RING_ACCOUNT_CRL) == 0) {
                    c.revoked = std::make_shared<dht::crypto::RevocationList>(base64::decode(itr->asString()));
                } else
                    c.config[key] = itr->asString();
            } catch (const std::exception& ex) {
                RING_ERR("Can't parse JSON entry with value of type %d: %s", (unsigned)itr->type(), ex.what());
            }
        }
    } catch (const std::exception& ex) {
        RING_ERR("Can't parse JSON: %s", ex.what());
    }

    return c;
}


std::vector<uint8_t>
RingAccount::makeArchive(const ArchiveContent& archive) const
{
    RING_DBG("[Account %s] building account archive", getAccountID().c_str());

    Json::Value root;

    auto details = getAccountDetails();
    for (auto it : details) {
        if (it.first.compare(DRing::Account::ConfProperties::Ringtone::PATH) == 0) {
            // Ringtone path is not exportable
        } else if (it.first.compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0 ||
                it.first.compare(DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE) == 0 ||
                it.first.compare(DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) == 0) {
            // replace paths by the files content
            if (not it.second.empty()) {
                try {
                    root[it.first] = base64::encode(fileutils::loadFile(it.second));
                } catch (...) {}
            }
        } else
            root[it.first] = it.second;
    }

    if (archive.ca_key and *archive.ca_key)
        root[Conf::RING_CA_KEY] = base64::encode(archive.ca_key->serialize());
    root[Conf::RING_ACCOUNT_KEY] = base64::encode(archive.id.first->serialize());
    root[Conf::RING_ACCOUNT_CERT] = base64::encode(archive.id.second->getPacked());
    root[Conf::ETH_KEY] = base64::encode(archive.eth_key);

    if (archive.revoked)
        root[Conf::RING_ACCOUNT_CRL] = base64::encode(archive.revoked->getPacked());

    if (not contacts_.empty()) {
        Json::Value& contacts = root[Conf::RING_ACCOUNT_CONTACTS];
       /* for (const auto& c : contacts_)
            contacts[c.first.toString()] = c.second.toJson();*/
    }

    Json::FastWriter fastWriter;
    std::string output = fastWriter.write(root);

    // Compress
    return archiver::compress(output);
}

void
RingAccount::saveArchive(const ArchiveContent& archive_content, const std::string& pwd)
{
    std::vector<uint8_t> archive;
    try {
        archive = makeArchive(archive_content);
    } catch (const std::runtime_error& ex) {
        RING_ERR("[Account %s] Can't export archive: %s", getAccountID().c_str(), ex.what());
        return;
    }

    // Encrypt using provided password
    auto encrypted = dht::crypto::aesEncrypt(archive, pwd);

    // Write
    try {
        if (archivePath_.empty())
            archivePath_ = "export.gz";
        fileutils::saveFile(fileutils::getFullPath(idPath_, archivePath_), encrypted);
    } catch (const std::runtime_error& ex) {
        RING_ERR("Export failed: %s", ex.what());
        return;
    }
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

void
RingAccount::addDevice(const std::string& password)
{
    auto this_ = std::static_pointer_cast<RingAccount>(shared_from_this());
    ThreadPool::instance().run([this_,password]() {
        std::vector<uint8_t> key;
        dht::InfoHash loc;
        std::string pin_str;
        ArchiveContent a;
        try {
            RING_DBG("[Account %s] exporting Ring account", this_->getAccountID().c_str());

            a = this_->readArchive(password);

            // Generate random 32bits PIN
            std::uniform_int_distribution<uint32_t> dis;
            auto pin = dis(this_->rand_);
            // Manipulate PIN as hex
            std::stringstream ss;
            ss << std::hex << pin;
            pin_str = ss.str();
            std::transform(pin_str.begin(), pin_str.end(), pin_str.begin(), ::toupper);

            std::tie(key, loc) = computeKeys(password, pin_str);
        } catch (const std::exception& e) {
            RING_ERR("[Account %s] can't export account: %s", this_->getAccountID().c_str(), e.what());
            emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(this_->getAccountID(), 1, "");
            return;
        }
        try {
            auto archive = this_->makeArchive(a);
            auto encrypted = dht::crypto::aesEncrypt(archive, key);
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
RingAccount::revokeDevice(const std::string& password, const std::string& device)
{
    // shared_ptr of future
    auto fa = ThreadPool::instance().getShared<ArchiveContent>(
        [this, password] { return readArchive(password); });
    auto sthis = shared();
    findCertificate(dht::InfoHash(device),
                    [fa,sthis,password,device](const std::shared_ptr<dht::crypto::Certificate>& crt) mutable
    {
        auto& this_ = *sthis;
        if (not crt) {
            emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(this_.getAccountID(), device, 2);
            return;
        }
        this_.foundAccountDevice(crt);
        ArchiveContent a;
        try {
            a = fa->get();
        } catch (...) {
            emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(this_.getAccountID(), device, 1);
            return;
        }
        // Add revoked device to the revocation list and resign it
        if (not a.revoked)
            a.revoked = std::make_shared<decltype(a.revoked)::element_type>();
        a.revoked->revoke(*crt);
        a.revoked->sign(a.id);
        // add to CRL cache
        tls::CertificateStore::instance().pinRevocationList(a.id.second->getId().toString(), a.revoked);
        tls::CertificateStore::instance().loadRevocations(*this_.identity_.second->issuer);
        this_.saveArchive(a, password);
        this_.knownDevices_.erase(crt->getId());
        this_.saveKnownDevices();
        emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(this_.getAccountID(), device, 0);
        emitSignal<DRing::ConfigurationSignal::KnownDevicesChanged>(this_.getAccountID(), this_.getKnownDevices());
        this_.syncDevices();
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
RingAccount::loadAccountFromDHT(const std::string& archive_password, const std::string& archive_pin)
{
    setRegistrationState(RegistrationState::INITIALIZING);

    // launch dedicated dht instance
    if (dht_.isRunning()) {
        RING_ERR("DHT already running (stopping it first).");
        dht_.join();
    }
    dht_.setOnStatusChanged([this](dht::NodeStatus s4, dht::NodeStatus s6) {
        RING_WARN("Dht status : IPv4 %s; IPv6 %s", dhtStatusStr(s4), dhtStatusStr(s6));
    });
    dht_.run((in_port_t)dhtPortUsed_, {}, true);
    dht_.bootstrap(loadNodes());
    auto bootstrap = loadBootstrap();
    if (not bootstrap.empty())
        dht_.bootstrap(bootstrap);

    std::weak_ptr<RingAccount> w = std::static_pointer_cast<RingAccount>(shared_from_this());
    auto state_old = std::make_shared<std::pair<bool, bool>>(false, true);
    auto state_new = std::make_shared<std::pair<bool, bool>>(false, true);
    auto found = std::make_shared<bool>(false);

    auto archiveFound = [w,found,archive_password](const ArchiveContent& a) {
        *found =  true;
        if (auto this_ = w.lock()) {
            this_->initRingDevice(a);
            this_->saveArchive(a, archive_password);
            this_->registrationState_ = RegistrationState::UNREGISTERED;
            Manager::instance().saveConfig();
            this_->doRegister();
        }
    };
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

    auto search = [w,found,archive_password,archive_pin,archiveFound,searchEnded](bool previous, std::shared_ptr<std::pair<bool, bool>>& state) {
        std::vector<uint8_t> key;
        dht::InfoHash loc;

        // compute archive location and decryption keys
        try {
            std::tie(key, loc) = computeKeys(archive_password, archive_pin, previous);
            if (auto this_ = w.lock()) {
                RING_DBG("[Account %s] trying to load account from DHT with %s at %s", this_->getAccountID().c_str(), archive_pin.c_str(), loc.toString().c_str());
                this_->dht_.get(loc, [w,key,found,archive_password,archiveFound](const std::shared_ptr<dht::Value>& val) {
                    std::vector<uint8_t> decrypted;
                    try {
                        decrypted = dht::crypto::aesDecrypt(val->data, key);
                    } catch (const std::exception& ex) {
                        return true;
                    }
                    RING_DBG("Found archive on the DHT");
                    runOnMainThread([=]() {
                        try {
                            archiveFound(loadArchive(decrypted));
                        } catch (const std::exception& e) {
                            if (auto this_ = w.lock()) {
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
    RING_WARN("Creating new Ring account");
    setRegistrationState(RegistrationState::INITIALIZING);
    auto sthis = std::static_pointer_cast<RingAccount>(shared_from_this());
    ThreadPool::instance().run([sthis,archive_password,migrate]() mutable {
        ArchiveContent a;
        auto& this_ = *sthis;

        RING_WARN("Generating ETH key");
        auto future_keypair = ThreadPool::instance().get<dev::KeyPair>(std::bind(&dev::KeyPair::create));

        try {
            if (migrate.first and migrate.second) {
                RING_WARN("Converting certificate from old ring account");
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
                RING_WARN("New account: CA: %s, RingID: %s", ca.second->getId().toString().c_str(), a.id.second->getId().toString().c_str());
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
        RING_DBG("Account generation ended, saving...");
        this_.setRegistrationState(RegistrationState::UNREGISTERED);
        Manager::instance().saveConfig();
        this_.doRegister();
    });
}

bool
RingAccount::needsMigration(const dht::crypto::Identity& id)
{
    if (not id.second)
        return true;
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
RingAccount::updateCertificates(ArchiveContent& archive, dht::crypto::Identity& device)
{
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
    ArchiveContent archive;
    try {
        archive = readArchive(pwd);
    } catch (...) {
        Migration::setState(accountID_, Migration::State::INVALID);
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
RingAccount::loadAccount(const std::string& archive_password, const std::string& archive_pin)
{
    if (registrationState_ == RegistrationState::INITIALIZING)
        return;

    RING_DBG("[Account %s] loading Ring account", getAccountID().c_str());
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
        } else if (hasArchive) {
            if (archive_password.empty()) {
                RING_WARN("[Account %s] password needed to read archive", getAccountID().c_str());
                setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
            } else {
                if (needsMigration(id)) {
                    RING_WARN("[Account %s] account certificate needs update", getAccountID().c_str());
                    migrateAccount(archive_password, id);
                } else {
                    RING_WARN("[Account %s] archive present but no valid receipt: creating new device", getAccountID().c_str());
                    try {
                        initRingDevice(readArchive(archive_password));
                    } catch (...) {
                        Migration::setState(accountID_, Migration::State::INVALID);
                        return;
                    }
                    Migration::setState(accountID_, Migration::State::SUCCESS);
                    setRegistrationState(RegistrationState::UNREGISTERED);
                }
                Manager::instance().saveConfig();
                loadAccount();
            }
        } else {
            // no receipt or archive, creating new account
            if (archive_password.empty()) {
                RING_WARN("[Account %s] password needed to create archive", getAccountID().c_str());
                if (id.first) {
                    ringAccountId_ = id.first->getPublicKey().getId().toString();
                    username_ = RING_URI_PREFIX+ringAccountId_;
                }
                setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
            } else {
                if (archive_pin.empty()) {
                    createAccount(archive_password, std::move(id));
                } else {
                    loadAccountFromDHT(archive_password, archive_pin);
                }
            }
        }
    } catch (const std::exception& e) {
        RING_WARN("[Account %s] error loading account: %s", getAccountID().c_str(), e.what());
        identity_ = dht::crypto::Identity{};
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
RingAccount::setAccountDetails(const std::map<std::string, std::string>& details)
{
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
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PASSWORD, archive_password);
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PIN,      archive_pin);
    std::transform(archive_pin.begin(), archive_pin.end(), archive_pin.begin(), ::toupper);
    parsePath(details, DRing::Account::ConfProperties::ARCHIVE_PATH,     archivePath_, idPath_);
    parseString(details, DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);

#if HAVE_RINGNS
    parseString(details, DRing::Account::ConfProperties::RingNS::URI,     nameServer_);
    nameDir_ = NameDirectory::instance(nameServer_);
#endif

    loadAccount(archive_password, archive_pin);

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
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    a.emplace(Conf::CONFIG_DHT_PORT, ring::to_string(dhtPort_));
    a.emplace(Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::RING_DEVICE_ID, ringDeviceId_);
    a.emplace(DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);
    a.emplace(DRing::Account::ConfProperties::Presence::SUPPORT_SUBSCRIBE, TRUE_STR);

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
    std::weak_ptr<RingAccount> w = std::static_pointer_cast<RingAccount>(shared_from_this());
    nameDir_.get().registerName(ringAccountId_, name, ethAccount_, [acc,name,w](NameDirectory::RegistrationResponse response){
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

void
RingAccount::handleEvents()
{
    // Process DHT events
    dht_.loop();

    // Call msg in "callto:"
    handlePendingCallList();
}

void
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

    static const dht::InfoHash invalid_hash; // Invariant

    auto pc_iter = std::begin(pending_calls);
    while (pc_iter != std::end(pending_calls)) {
        bool incoming = pc_iter->call_key == invalid_hash; // do it now, handlePendingCall may invalidate pc data
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
    }
}

pj_status_t
check_peer_certificate(dht::InfoHash from, unsigned status, const gnutls_datum_t* cert_list,
                       unsigned cert_num, std::shared_ptr<dht::crypto::Certificate>& cert_out)
{
    if (cert_num == 0) {
        RING_ERR("[peer:%s] No certificate", from.toString().c_str());
        return PJ_SSL_CERT_EUNKNOWN;
    }

    if (status & GNUTLS_CERT_EXPIRED or status & GNUTLS_CERT_NOT_ACTIVATED) {
        RING_ERR("[peer:%s] Expired certificate", from.toString().c_str());
        return PJ_SSL_CERT_EVALIDITY_PERIOD;
    }

    if (status & GNUTLS_CERT_INSECURE_ALGORITHM) {
        RING_ERR("[peer:%s] Untrusted certificate", from.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    // Assumes the chain has already been checked by GnuTLS.
    std::vector<std::pair<uint8_t*, uint8_t*>> crt_data;
    crt_data.reserve(cert_num);
    for (unsigned i=0; i<cert_num; i++)
        crt_data.emplace_back(cert_list[i].data, cert_list[i].data + cert_list[i].size);
    dht::crypto::Certificate crt(crt_data);

    const auto tls_id = crt.getId();
    if (crt.getUID() != tls_id.toString()) {
        RING_ERR("[peer:%s] Certificate UID must be the public key ID", from.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    if (tls_id != from) {
        RING_ERR("[peer:%s] Certificate public key ID doesn't match (%s)",
                 from.toString().c_str(), tls_id.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    RING_DBG("[peer:%s] Certificate verified", from.toString().c_str());

    cert_out = std::make_shared<dht::crypto::Certificate>(std::move(crt));

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
    auto remote_h = pc.from;
    if (not identity_.first or not identity_.second)
        throw std::runtime_error("No identity configured for this account.");

    std::weak_ptr<RingAccount> w = std::static_pointer_cast<RingAccount>(shared_from_this());
    std::weak_ptr<SIPCall> wcall = call;
    tls::TlsParams tlsParams {
        /*.ca_list = */"",
        /*.cert = */identity_.second,
        /*.cert_key = */identity_.first,
        /*.dh_params = */dhParams_,
        /*.timeout = */std::chrono::duration_cast<decltype(tls::TlsParams::timeout)>(TLS_TIMEOUT),
        /*.cert_check = */[w,wcall,remote_h,incoming](unsigned status, const gnutls_datum_t* cert_list, unsigned cert_num) -> pj_status_t {
            try {
                if (auto call = wcall.lock()) {
                    if (auto sthis = w.lock()) {
                        auto& this_ = *sthis;
                        std::shared_ptr<dht::crypto::Certificate> peer_cert;
                        auto ret = check_peer_certificate(remote_h, status, cert_list, cert_num, peer_cert);
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
                         remote_h.toString().c_str(), e.what());
                return PJ_SSL_CERT_EUNKNOWN;
            }
        }
    };
    call->setTransport(link_->sipTransportBroker->getTlsIceTransport(pc.ice_sp, ICE_COMP_SIP_TRANSPORT,
                                                            tlsParams));

    // Notify of fully available connection between peers
    RING_DBG("[call:%s] SIP communication established", call->getCallId().c_str());
    call->setState(Call::ConnectionState::PROGRESSING);

    // Incoming call?
    if (incoming) {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingSipCalls_.emplace_back(std::move(pc)); // copy of pc
    } else
        createOutgoingCall(call, remote_h.toString(), ice->getRemoteAddress(ICE_COMP_SIP_TRANSPORT));

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

    std::weak_ptr<RingAccount> w = std::static_pointer_cast<RingAccount>(shared_from_this());
    upnp_->setIGDListener([w] {
        if (auto shared = w.lock())
            shared->igdChanged();
    });
    return added;
}

void
RingAccount::doRegister()
{
    if (not isUsable()) {
        RING_WARN("Account must be enabled and active to register, ignoring");
        return;
    }

    // invalid state transitions:
    // INITIALIZING: generating/loading certificates, can't register
    // NEED_MIGRATION: old Ring account detected, user needs to migrate
    if (registrationState_ == RegistrationState::INITIALIZING
     || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION)
        return;

    if (not dhParams_.valid()) {
        generateDhParams();
    }

    /* if UPnP is enabled, then wait for IGD to complete registration */
    if ( upnpEnabled_ ) {
        auto shared = shared_from_this();
        RING_DBG("UPnP: waiting for IGD to register RING account");
        setRegistrationState(RegistrationState::TRYING);
        std::thread{ [shared] {
            auto this_ = std::static_pointer_cast<RingAccount>(shared).get();
            if ( not this_->mapPortUPnP())
                RING_WARN("UPnP: Could not successfully map DHT port with UPnP, continuing with account registration anyways.");
            this_->doRegister_();
        }}.detach();
    } else
        doRegister_();

}


std::vector<std::pair<sockaddr_storage, socklen_t>>
RingAccount::loadBootstrap() const
{
    std::vector<std::pair<sockaddr_storage, socklen_t>> bootstrap;
    if (!hostname_.empty()) {
        std::stringstream ss(hostname_);
        std::string node_addr;
        while (std::getline(ss, node_addr, ';')) {
            auto ips = ip_utils::getAddrList(node_addr);
            if (ips.empty()) {
                IpAddr resolved(node_addr);
                if (resolved) {
                    if (resolved.getPort() == 0)
                        resolved.setPort(DHT_DEFAULT_PORT);
                    bootstrap.emplace_back(resolved, resolved.getLength());
                }
            } else {
                for (auto& ip : ips) {
                    if (ip.getPort() == 0)
                        ip.setPort(DHT_DEFAULT_PORT);
                    bootstrap.emplace_back(ip, ip.getLength());
                }
            }
        }
        for (auto ip : bootstrap)
            RING_DBG("Bootstrap node: %s", IpAddr(ip.first).toString(true).c_str());
    }
    return bootstrap;
}

void
RingAccount::trackBuddyPresence(const std::string& buddy_id)
{
    if (not dht_.isRunning()) {
        RING_ERR("DHT node not running. Cannot track buddy %s", buddy_id.c_str());
        return;
    }
    std::weak_ptr<RingAccount> weak_this = std::static_pointer_cast<RingAccount>(shared_from_this());
    auto h = dht::InfoHash(parseRingUri(buddy_id));
    auto buddy_infop = trackedBuddies_.emplace(h, decltype(trackedBuddies_)::mapped_type {h});
    if (buddy_infop.second) {
        auto& buddy_info = buddy_infop.first->second;
        buddy_info.updateInfo = Manager::instance().scheduleTask([h,weak_this]() {
            if (auto shared_this = weak_this.lock()) {
                /* ::forEachDevice call will update buddy info accordingly. */
                shared_this->forEachDevice(h, {}, [h, weak_this] (bool /* ok */) {
                    if (auto shared_this = weak_this.lock()) {
                        std::lock_guard<std::recursive_mutex> lock(shared_this->buddyInfoMtx);
                        auto buddy_info_it = shared_this->trackedBuddies_.find(h);
                        if (buddy_info_it == shared_this->trackedBuddies_.end()) return;

                        auto& buddy_info = buddy_info_it->second;
                        if (buddy_info.updateInfo) {
                            auto cb = buddy_info.updateInfo;
                            Manager::instance().scheduleTask(
                                std::move(cb),
                                std::chrono::steady_clock::now() + DeviceAnnouncement::TYPE.expiration
                            );
                        }
                    }
                });
            }
        }, std::chrono::steady_clock::now())->cb;
        RING_DBG("Now tracking buddy %s", h.toString().c_str());
    } else
        RING_WARN("Buddy %s is already being tracked.", h.toString().c_str());
}

std::map<std::string, bool>
RingAccount::getTrackedBuddyPresence()
{
    std::lock_guard<std::recursive_mutex> lock(buddyInfoMtx);
    std::map<std::string, bool> presence_info;
    const auto shared_this = std::static_pointer_cast<const RingAccount>(shared_from_this());
    for (const auto& buddy_info_p : shared_this->trackedBuddies_) {
        const auto& devices_ts = buddy_info_p.second.devicesTimestamps;
        const auto& last_seen_device_id = std::max_element(devices_ts.cbegin(), devices_ts.cend(),
            [](decltype(buddy_info_p.second.devicesTimestamps)::value_type ld,
               decltype(buddy_info_p.second.devicesTimestamps)::value_type rd)
            {
                return ld.second < rd.second;
            }
        );
        presence_info.emplace(buddy_info_p.first.toString(), last_seen_device_id != devices_ts.cend()
            ? last_seen_device_id->second > std::chrono::steady_clock::now() - DeviceAnnouncement::TYPE.expiration
            : false);
    }
    return presence_info;
}

void
RingAccount::onTrackedBuddyOnline(std::map<dht::InfoHash, BuddyInfo>::iterator& buddy_info_it, const dht::InfoHash& device_id)
{
    std::lock_guard<std::recursive_mutex> lock(buddyInfoMtx);
    RING_DBG("Buddy %s online: (device: %s)", buddy_info_it->second.id.toString().c_str(), device_id.toString().c_str());
    buddy_info_it->second.devicesTimestamps[device_id] = std::chrono::steady_clock::now();
    emitSignal<DRing::PresenceSignal::NewBuddyNotification>(getAccountID(), buddy_info_it->second.id.toString(), 1,  "");
}

void
RingAccount::onTrackedBuddyOffline(std::map<dht::InfoHash, BuddyInfo>::iterator& buddy_info_it)
{
    std::lock_guard<std::recursive_mutex> lock(buddyInfoMtx);
    RING_DBG("Buddy %s offline", buddy_info_it->first.toString().c_str());
    emitSignal<DRing::PresenceSignal::NewBuddyNotification>(getAccountID(), buddy_info_it->first.toString(), 0,  "");
    buddy_info_it->second.devicesTimestamps.clear();
}

void
RingAccount::doRegister_()
{
    try {
        if (not identity_.first or not identity_.second)
            throw std::runtime_error("No identity configured for this account.");

        loadTreatedCalls();
        loadTreatedMessages();
        if (dht_.isRunning()) {
            RING_ERR("DHT already running (stopping it first).");
            dht_.join();
        }

        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        std::weak_ptr<RingAccount> w {shared};

#if HAVE_RINGNS
        // Look for registered name on the blockchain
        nameDir_.get().lookupAddress(ringAccountId_, [w](const std::string& result, const NameDirectory::Response& response) {
            if (response == NameDirectory::Response::found)
                if (auto this_ = w.lock()) {
                    if (this_->registeredName_ != result) {
                        this_->registeredName_ = result;
                        emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(this_->accountID_, this_->getVolatileAccountDetails());
                    }
                }
        });
#endif

        dht_.setOnStatusChanged([this](dht::NodeStatus s4, dht::NodeStatus s6) {
            RING_DBG("[Account %s] Dht status : IPv4 %s; IPv6 %s", getAccountID().c_str(), dhtStatusStr(s4), dhtStatusStr(s6));
            RegistrationState state;
            switch (std::max(s4, s6)) {
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
            setRegistrationState(state);
        });

        dht_.run((in_port_t)dhtPortUsed_, identity_, false);

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
#ifndef RING_UWP
            static auto log_error = [](char const* m, va_list args) { vlogger(LOG_ERR, m, args); };
            static auto log_warn = [](char const* m, va_list args) { vlogger(LOG_WARNING, m, args); };
            static auto log_debug = [](char const* m, va_list args) { vlogger(LOG_DEBUG, m, args); };
            dht_.setLoggers(
                log_error,
                (dht_log_level > 1) ? log_warn : silent,
                (dht_log_level > 2) ? log_debug : silent);
#else
            static auto log_all = [](char const* m, va_list args) {
                char tmp[2048];
                vsprintf(tmp, m, args);
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                ring::emitSignal<DRing::DebugSignal::MessageSend>(std::to_string(now) + " " + std::string(tmp));
            };
            dht_.setLoggers(log_all, log_all, silent);
#endif
        }

        dht_.importValues(loadValues());

        Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
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
            dht_.listen<DeviceAnnouncement>(h, [shared](DeviceAnnouncement&& dev) {
                shared->findCertificate(dev.dev, [shared](const std::shared_ptr<dht::crypto::Certificate>& crt) {
                    shared->foundAccountDevice(crt);
                });
                return true;
            });
            dht_.listen<dht::crypto::RevocationList>(h, [shared](dht::crypto::RevocationList&& crl) {
                if (crl.isSignedBy(*shared->identity_.second->issuer)) {
                    RING_DBG("[Account %s] found CRL for account.", shared->getAccountID().c_str());
                    tls::CertificateStore::instance().pinRevocationList(
                        shared->ringAccountId_,
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
            [shared] (dht::IceCandidates&& msg) {
                // callback for incoming call
                auto& this_ = *shared;
                if (msg.from == this_.dht_.getId())
                    return true;

                auto res = this_.treatedCalls_.insert(msg.id);
                this_.saveTreatedCalls();
                if (!res.second)
                    return true;

                RING_WARN("[Account %s] ICE candidate from %s.", this_.getAccountID().c_str(), msg.from.toString().c_str());

                this_.onPeerMessage(msg.from, [shared, msg](const std::shared_ptr<dht::crypto::Certificate>& cert,
                                                            const dht::InfoHash& /*account*/) mutable
                {
                    shared->incomingCall(std::move(msg), cert);
                });
                return true;
            }
        );

        auto inboxKey = dht::InfoHash::get("inbox:"+ringDeviceId_);
        dht_.listen<dht::TrustRequest>(
            inboxKey,
            [shared](dht::TrustRequest&& v) {
                if (v.service != DHT_TYPE_NS)
                    return true;

                shared->findCertificate(v.from, [shared, v](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                    auto& this_ = *shared.get();

                    // check peer certificate
                    dht::InfoHash peer_account;
                    if (not this_.foundPeerDevice(cert, peer_account)) {
                        return;
                    }

                    RING_WARN("Got trust request from: %s / %s", peer_account.toString().c_str(), v.from.toString().c_str());
                    this_.onTrustRequest(peer_account, v.from, time(nullptr), v.confirm, std::move(v.payload));
                });
                return true;
            }
        );

        auto syncDeviceKey = dht::InfoHash::get("inbox:"+ringDeviceId_);
        dht_.listen<DeviceSync>(
            syncDeviceKey,
            [shared](DeviceSync&& sync) {
                // Received device sync data.
                // check device certificate
                shared->findCertificate(sync.from, [shared,sync](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                    if (!cert or cert->getId() != sync.from) {
                        RING_WARN("Can't find certificate for device %s", sync.from.toString().c_str());
                        return;
                    }
                    if (not shared->foundAccountDevice(cert))
                        return;
                    shared->onReceiveDeviceSync(std::move(sync));
                });

                return true;
            }
        );

        auto inboxDeviceKey = dht::InfoHash::get("inbox:"+ringDeviceId_);
        dht_.listen<dht::ImMessage>(
            inboxDeviceKey,
            [shared, inboxDeviceKey](dht::ImMessage&& v) {
                auto& this_ = *shared.get();
                auto res = this_.treatedMessages_.insert(v.id);
                if (!res.second)
                    return true;
                this_.saveTreatedMessages();
                this_.onPeerMessage(v.from, [shared, v, inboxDeviceKey](const std::shared_ptr<dht::crypto::Certificate>&,
                                                                        const dht::InfoHash& peer_account)
                {
                    auto now = clock::to_time_t(clock::now());
                    std::map<std::string, std::string> payloads = {{"text/plain",
                                                                    utf8_make_valid(v.msg)}};
                    shared->onTextMessage(peer_account.toString(), payloads);
                    RING_DBG("Sending message confirmation %" PRIx64, v.id);
                    shared->dht_.putEncrypted(inboxDeviceKey,
                              v.from,
                              dht::ImMessage(v.id, std::string(), now));
                });
                return true;
            }
        );
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
        // Send confirmation
        if (not confirm)
            sendTrustRequestConfirm(peer_account);
        // Contact exists, update confirmation status
        if (not contact->second.confirmed) {
            contact->second.confirmed = true;
            emitSignal<DRing::ConfigurationSignal::ContactAdded>(getAccountID(), peer_account.toString(), true);
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
    // quick check in case we already explicilty banned this public key
    auto trustStatus = trust_.getCertificateStatus(peer_device.toString());
    if (trustStatus == tls::TrustStore::PermissionStatus::BANNED) {
        RING_WARN("[Account %s] Discarding message from banned peer %s", getAccountID().c_str(), peer_device.toString().c_str());
        return;
    }

    auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
    findCertificate(peer_device,
        [shared, peer_device, trustStatus, cb](const std::shared_ptr<dht::crypto::Certificate>& cert) {
        auto& this_ = *shared;

        dht::InfoHash peer_account_id;
        if (not this_.foundPeerDevice(cert, peer_account_id)) {
            RING_WARN("[Account %s] Discarding message from invalid peer certificate %s.", this_.getAccountID().c_str(), peer_device.toString().c_str());
            return;
        }

        if (not this_.dhtPublicInCalls_ and trustStatus != tls::TrustStore::PermissionStatus::ALLOWED) {
            if (!cert or cert->getId() != peer_device) {
                RING_WARN("[Account %s] Can't find certificate of %s for incoming message.", this_.getAccountID().c_str(), peer_device.toString().c_str());
                return;
            }

            auto& this_ = *shared;
            if (!this_.trust_.isAllowed(*cert)) {
                RING_WARN("[Account %s] Discarding message from untrusted peer %s.", this_.getAccountID().c_str(), peer_device.toString().c_str());
                return;
            }
        } else if (not this_.dhtPublicInCalls_ or trustStatus == tls::TrustStore::PermissionStatus::BANNED) {
            RING_WARN("[Account %s] Discarding message from untrusted or banned peer %s.", this_.getAccountID().c_str(), peer_device.toString().c_str());
            return;
        }

        cb(cert, peer_account_id);
    });
}

void
RingAccount::incomingCall(dht::IceCandidates&& msg, const std::shared_ptr<dht::crypto::Certificate>& from_cert)
{
    RING_WARN("ICE incoming from DHT peer %s", msg.from.toString().c_str());
    auto call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(*this, Manager::instance().getNewCallID(), Call::CallType::INCOMING);
    auto ice = createIceTransport(("sip:"+call->getCallId()).c_str(), ICE_COMPONENTS, false, getIceOptions());

    std::weak_ptr<SIPCall> wcall = call;
    auto account = std::static_pointer_cast<RingAccount>(shared_from_this());
    Manager::instance().addTask([account, wcall, ice, msg, from_cert] {
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

        account->replyToIncomingIceMsg(call, ice, msg, from_cert);
        return false;
    });
}

bool
RingAccount::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name, const time_point& updated)
{
    if (not crt)
        return false;

    // match certificate chain
    if (not accountTrust_.isAllowed(*crt)) {
        RING_WARN("[Account %s] Found invalid account device: %s", getAccountID().c_str(), crt->getId().toString().c_str());
        return false;
    }

    // insert device
    auto it = knownDevices_.emplace(crt->getId(), KnownDevice{crt, name, updated});
    if (it.second) {
        RING_WARN("[Account %s] Found account device: %s %s", getAccountID().c_str(),
                                                              name.c_str(),
                                                              crt->getId().toString().c_str());
        tls::CertificateStore::instance().pinCertificate(crt);
        saveKnownDevices();
        emitSignal<DRing::ConfigurationSignal::KnownDevicesChanged>(getAccountID(), getKnownDevices());
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            RING_WARN("[Account %s] updating device name: %s %s", getAccountID().c_str(),
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
        RING_WARN("[Account %s] Found invalid peer device: %s", getAccountID().c_str(), crt->getId().toString().c_str());
        return false;
    }

    RING_WARN("foundPeerDevice dev:%s CA:%s", crt->getId().toString().c_str(), top_issuer->getId().toString().c_str());

    // Check peer certificate chain
    // Trust store with top issuer as the only CA
    tls::TrustStore peer_trust;
    peer_trust.setCertificateStatus(top_issuer, tls::TrustStore::PermissionStatus::ALLOWED, false);

    if (not peer_trust.isAllowed(*crt)) {
        RING_WARN("[Account %s] Found invalid peer device: %s", getAccountID().c_str(), crt->getId().toString().c_str());
        return false;
    }

    account_id = crt->issuer->getId();
    return true;
}

void
RingAccount::replyToIncomingIceMsg(const std::shared_ptr<SIPCall>& call,
                                   const std::shared_ptr<IceTransport>& ice,
                                   const dht::IceCandidates& peer_ice_msg,
                                   const std::shared_ptr<dht::crypto::Certificate>& peer_cert)
{
    registerDhtAddress(*ice);

    dht::Value val { dht::IceCandidates(peer_ice_msg.id, ice->packIceMsg()) };

    auto from = (peer_cert ? (peer_cert->issuer ? peer_cert->issuer->getId() : peer_cert->getId()) : peer_ice_msg.from).toString();

    std::weak_ptr<SIPCall> wcall = call;
#if HAVE_RINGNS
    nameDir_.get().lookupAddress(from, [wcall](const std::string& result, const NameDirectory::Response& response){
        if (response == NameDirectory::Response::found)
            if (auto call = wcall.lock())
                call->setPeerRegistredName(result);
    });
#endif

    // Asynchronous DHT put of our local ICE data
    auto shared_this = std::static_pointer_cast<RingAccount>(shared_from_this());
    dht_.putEncrypted(
        callKey_,
        peer_ice_msg.from,
        std::move(val),
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
    call->initRecFilename(from);

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
                /*.from_cert = */peer_cert });
    }
}

void
RingAccount::doUnregister(std::function<void(bool)> released_cb)
{
    if (registrationState_ == RegistrationState::INITIALIZING
     || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION) {
        if (released_cb) released_cb(false);
        return;
    }

    RING_WARN("doUnregister");
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.clear();
        pendingSipCalls_.clear();
    }

    /* RING_DBG("UPnP: removing port mapping for DHT account."); */
    upnp_->setIGDListener();
    upnp_->removeMappings();

    Manager::instance().unregisterEventHandler((uintptr_t)this);
    saveNodes(dht_.exportNodes());
    saveValues(dht_.exportValues());
    dht_.join();
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
    findCertificate(cert_id);
    bool done = trust_.setCertificateStatus(cert_id, status);
    if (done)
        emitSignal<DRing::ConfigurationSignal::CertificateStateChanged>(getAccountID(), cert_id, tls::TrustStore::statusToStr(status));
    return done;
}

bool
RingAccount::setCertificateStatus(const std::string& cert_id, tls::TrustStatus status)
{
    findCertificate(cert_id);
    bool done = trust_.setCertificateStatus(cert_id, status);
    if (done)
        emitSignal<DRing::ConfigurationSignal::CertificateStateChanged>(getAccountID(), cert_id, tls::statusToStr(status));
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
    treatedMessages_ = loadIdList(cachePath_+DIR_SEPARATOR_STR "treatedMessages");
}

void
RingAccount::saveTreatedMessages() const
{
    fileutils::check_dir(cachePath_.c_str());
    saveIdList(cachePath_+DIR_SEPARATOR_STR "treatedMessages", treatedMessages_);
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
    if (t) {
        // FIXME: be sure that given transport is from SipIceTransport
        auto tlsTr = reinterpret_cast<tls::SipsIceTransport::TransportData*>(t)->self;
        auto address = tlsTr->getLocalAddress().toString(true);
        contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                         "%s%s<sips:%s%s%s;transport=dtls>",
                                         displayName_.c_str(),
                                         (displayName_.empty() ? "" : " "),
                                         identity_.second->getId().toString().c_str(),
                                         (address.empty() ? "" : "@"),
                                         address.c_str());
    } else {
        RING_ERR("getContactHeader: no SIP transport provided");
        contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                         "%s%s<sips:%s@ring.dht>",
                                         displayName_.c_str(),
                                         (displayName_.empty() ? "" : " "),
                                         identity_.second->getId().toString().c_str());
    }
    return contact_;
}

/* contacts */

void
RingAccount::addContact(const std::string& uri, bool confirmed)
{
   
	Contact c = contactsManager_->addContact(uri,confirmed);
	trust_.setCertificateStatus(uri, tls::TrustStore::PermissionStatus::ALLOWED);
    	saveContacts();
    	emitSignal<DRing::ConfigurationSignal::ContactAdded>(getAccountID(), uri, c.confirmed);
    	syncDevices();
}

void
RingAccount::removeContact(const std::string& uri, bool ban)
{
    
	Contact c = contactsManager_->removeContact(uri,ban);
	if(c.banned){
		trust_.setCertificateStatus(uri, tls::TrustStore::PermissionStatus::BANNED);
	}
	saveContacts();
    	emitSignal<DRing::ConfigurationSignal::ContactRemoved>(getAccountID(), uri, ban);
    	syncDevices();


}

std::vector<std::map<std::string, std::string>>
RingAccount::getContacts() const
{
    
	return contactsManager_->getContacts();
}

void
RingAccount::updateContact(const dht::InfoHash& id, const Contact& contact)
{
        Contact returnedContact;
	bool state = contactsManager_->updateContact(id,contact,returnedContact);
	if (state) {
        	if (returnedContact.isActive()) {
            trust_.setCertificateStatus(id.toString(), tls::TrustStore::PermissionStatus::ALLOWED);
            emitSignal<DRing::ConfigurationSignal::ContactAdded>(getAccountID(), id.toString(), returnedContact.confirmed);
        } 
	else {
            	if (returnedContact.banned)
              trust_.setCertificateStatus(id.toString(), tls::TrustStore::PermissionStatus::BANNED);
              emitSignal<DRing::ConfigurationSignal::ContactRemoved>(getAccountID(), id.toString(), returnedContact.banned);
        }
	
    }
}

void
RingAccount::loadContacts()
{
   contactsManager_->loadContacts();
}

void
RingAccount::saveContacts() const
{
   
contactsManager_->saveContacts();
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
            {"from", r.first.toString()},
            {"received", std::to_string(r.second.received)},
            {"payload", std::string(r.second.payload.begin(), r.second.payload.end())}
        });
    }
    return ret;
}

bool
RingAccount::acceptTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    auto i = trustRequests_.find(f);
    if (i == trustRequests_.end())
        return false;

    // The contact sent us a TR so we are in its contact list
    addContact(from, true);

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
    addContact(to);
    auto toH = dht::InfoHash(to);
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
            sync_data.trust_requests.emplace(req->first, TrustRequest{req->second.device, req->second.received, {}});
            if (++req == trustRequests_.end())
                req = trustRequests_.begin();
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
        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        findCertificate(d.first, [shared,d](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (not crt)
                return;
            shared->foundAccountDevice(crt, d.second);
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
    if ( upnpEnabled_ ) {
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
                           std::function<void(bool)> end)
{
    auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
    auto treatedDevices = std::make_shared<std::set<dht::InfoHash>>();
    dht_.get<dht::crypto::RevocationList>(to, [to](dht::crypto::RevocationList&& crl){
        tls::CertificateStore().instance().pinRevocationList(to.toString(), std::move(crl));
        return true;
    });
    dht_.get<DeviceAnnouncement>(to, [shared,to,treatedDevices,op](DeviceAnnouncement&& dev) {
        if (dev.from != to)
            return true;
        if (treatedDevices->emplace(dev.dev).second)
            op(shared, dev.dev);
        return true;
    }, [=](bool /*ok*/){
        {
            std::lock_guard<std::recursive_mutex> lock(shared->buddyInfoMtx);
            auto buddy_info_it = shared->trackedBuddies_.find(to);
            if (buddy_info_it != shared->trackedBuddies_.end()) {
                if (not treatedDevices->empty()) {
                    for (auto& device_id : *treatedDevices)
                        shared->onTrackedBuddyOnline(buddy_info_it, device_id);
                } else
                    shared->onTrackedBuddyOffline(buddy_info_it);
            }
        }
        RING_WARN("forEachDevice: found %lu devices", treatedDevices->size());
        if (end) end(not treatedDevices->empty());
    });
}

void
RingAccount::sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads, uint64_t token)
{
    if (to.empty() or payloads.empty()) {
        messageEngine_.onMessageSent(token, false);
        return;
    }
    if (payloads.size() != 1) {
        // Multi-part message
        // TODO: not supported yet
        RING_ERR("Multi-part im is not supported yet by RingAccount");
        messageEngine_.onMessageSent(token, false);
        return;
    }

    auto toUri = parseRingUri(to);
    auto toH = dht::InfoHash(toUri);
    auto now = clock::to_time_t(clock::now());

    struct PendingConfirmation {
        bool replied {false};
        std::map<dht::InfoHash, std::future<size_t>> listenTokens {};
    };
    auto confirm = std::make_shared<PendingConfirmation>();

    // Find listening Ring devices for this account
    forEachDevice(toH, [confirm,token,payloads,now](const std::shared_ptr<RingAccount>& shared, const dht::InfoHash& dev)
    {
        auto e = shared->sentMessages_.emplace(token, PendingMessage {});
        e.first->second.to = dev;

        auto h = dht::InfoHash::get("inbox:"+dev.toString());
        std::weak_ptr<RingAccount> wshared = shared;
        auto list_token = shared->dht_.listen<dht::ImMessage>(h, [h,wshared,token,confirm](dht::ImMessage&& msg) {
            if (auto sthis = wshared.lock()) {
                auto& this_ = *sthis;
                // check expected message confirmation
                if (msg.id != token)
                    return true;
                auto e = this_.sentMessages_.find(msg.id);
                if (e == this_.sentMessages_.end() or e->second.to != msg.from) {
                    RING_DBG("[Account %s] [message %" PRIx64 "] message not found", this_.getAccountID().c_str(), token);
                    return true;
                }
                this_.sentMessages_.erase(e);
                RING_DBG("[Account %s] [message %" PRIx64 "] received text message reply", this_.getAccountID().c_str(), token);

                // add treated message
                auto res = this_.treatedMessages_.insert(msg.id);
                if (!res.second)
                    return true;

                this_.saveTreatedMessages();

                // report message as confirmed received
                for (auto& t : confirm->listenTokens)
                    this_.dht_.cancelListen(t.first, t.second.get());
                confirm->listenTokens.clear();
                confirm->replied = true;
                this_.messageEngine_.onMessageSent(token, true);
            }
            return false;
        });
        confirm->listenTokens.emplace(h, std::move(list_token));
        shared->dht_.putEncrypted(h, dev,
            dht::ImMessage(token, std::string(payloads.begin()->second), now),
            [wshared,token,confirm,h](bool ok) {
                if (auto this_ = wshared.lock()) {
                    RING_DBG("[Account %s] [message %" PRIx64 "] put encrypted %s", this_->getAccountID().c_str(), token, ok ? "ok" : "failed");
                    if (not ok) {
                        auto lt = confirm->listenTokens.find(h);
                        if (lt != confirm->listenTokens.end()) {
                            this_->dht_.cancelListen(h, lt->second.get());
                            confirm->listenTokens.erase(lt);
                        }
                        if (confirm->listenTokens.empty() and not confirm->replied)
                            this_->messageEngine_.onMessageSent(token, false);
                    }
                }
            });

        RING_DBG("[Account %s] [message %" PRIx64 "] sending message for device %s", shared->getAccountID().c_str(), token, dev.toString().c_str());
    });

    // Timeout cleanup
    std::weak_ptr<RingAccount> wshared = shared();
    Manager::instance().scheduleTask([wshared, confirm, token]() {
        if (not confirm->replied and not confirm->listenTokens.empty()) {
            if (auto this_ = wshared.lock()) {
                RING_DBG("[Account %s] [message %" PRIx64 "] timeout", this_->getAccountID().c_str(), token);
                for (auto& t : confirm->listenTokens)
                    this_->dht_.cancelListen(t.first, t.second.get());
                confirm->listenTokens.clear();
                confirm->replied = true;
                this_->messageEngine_.onMessageSent(token, false);
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

        // IPv4
        const auto& addr4 = dht_.getPublicAddress(AF_INET);
        if (addr4.size())
            setPublishedAddress(reg_addr(ice, addr4[0].first));

        // IPv6 (must be put after IPv4 as SDP support only one address, we priorize IPv6)
        const auto& addr6 = dht_.getPublicAddress(AF_INET6);
        if (addr6.size())
            setPublishedAddress(reg_addr(ice, addr6[0].first));

    } else {
        reg_addr(ice, ip);
    }
}

} // namespace ring
