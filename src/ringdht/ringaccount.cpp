/*
 *  Copyright (C) 2014-2016 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "ringaccount.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "sip/sip_utils.h"

#include "sips_transport_ice.h"
#include "ice_transport.h"

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
using std::chrono::system_clock;

static constexpr int ICE_COMPONENTS {1};
static constexpr int ICE_COMP_SIP_TRANSPORT {0};
static constexpr int ICE_INIT_TIMEOUT {10};
static constexpr auto ICE_NEGOTIATION_TIMEOUT = std::chrono::seconds(60);
static constexpr auto TLS_TIMEOUT = std::chrono::seconds(30);

static constexpr const char* const RING_URI_PREFIX = "ring:";

constexpr const char* const RingAccount::ACCOUNT_TYPE;
/* constexpr */ const std::pair<uint16_t, uint16_t> RingAccount::DHT_PORT_RANGE {4000, 8888};

static std::uniform_int_distribution<dht::Value::Id> udist;

static const std::string
parseRingUri(const std::string& toUrl)
{
    auto dhtf = toUrl.find("ring:");
    if (dhtf != std::string::npos) {
        dhtf = dhtf+5;
    } else {
        dhtf = toUrl.find("sips:");
        dhtf = (dhtf == std::string::npos) ? 0 : dhtf+5;
    }
    while (dhtf < toUrl.length() && toUrl[dhtf] == '/')
        dhtf++;

    if (toUrl.length() - dhtf < 40)
        throw std::invalid_argument("id must be a ring infohash");

    const std::string toUri = toUrl.substr(dhtf, 40);
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
RingAccount::createIceTransport(Args... args)
{
    // We need a public address in case of NAT'ed network
    // Trying to use one discovered by DHT service
    if (getPublishedAddress().empty()) {
        const auto& addresses = dht_.getPublicAddress(AF_INET);
        if (addresses.size())
            setPublishedAddress(IpAddr{addresses[0].first});
    }

    auto ice = Manager::instance().getIceTransportFactory().createTransport(args...);
    if (!ice or ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0)
        throw std::runtime_error("ICE transport creation failed");

    if (const auto& publicIP = getPublishedIpAddress()) {
        for (unsigned compId = 1; compId <= ice->getComponentCount(); ++compId)
            ice->registerPublicIP(compId, publicIP);
    }

    return ice;
}

RingAccount::RingAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID), via_addr_(),
    cachePath_(fileutils::get_cache_dir()+DIR_SEPARATOR_STR+getAccountID()),
    dataPath_(cachePath_ + DIR_SEPARATOR_STR "values"),
    idPath_(fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID())
{}

RingAccount::~RingAccount()
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    dht_.join();
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
    const std::string toUri = parseRingUri(toUrl);
    RING_DBG("Calling DHT peer %s", toUri.c_str());

    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, RingAccount>(*this, manager.getNewCallID(),
                                                                  Call::CallType::OUTGOING);

    call->setIPToIP(true);
    call->setSecure(isTlsEnabled());

    auto shared_this = std::static_pointer_cast<RingAccount>(shared_from_this());

    // TODO: for now, we automatically trust all explicitly called peers
    setCertificateStatus(toUri, tls::TrustStore::PermissionStatus::ALLOWED);

    const auto toH = dht::InfoHash(toUri);

    call->setState(Call::ConnectionState::TRYING);
    std::weak_ptr<SIPCall> weak_call = call;

    auto treatedDevices_ = std::make_shared<std::set<dht::InfoHash>>();

    // Find listening Ring devices for this account
    shared_this->dht_.get<DeviceAnnouncement>(toH, [=](DeviceAnnouncement&& dev) {
        if (dev.from != toH)
            return true;
        if (not treatedDevices_->emplace(dev.dev).second)
            return true;
        RING_WARN("DeviceAnnouncement callback %s", dev.dev.toString().c_str());

        runOnMainThread([=](){
            if (auto call = weak_call.lock()) {
                RING_WARN("[call %s] Found device %s", call->getCallId().c_str(), dev.dev.toString().c_str());

                auto& manager = Manager::instance();
                auto dev_call = manager.callFactory.newCall<SIPCall, RingAccount>(*this, manager.getNewCallID(),
                                                                              Call::CallType::OUTGOING);
                std::weak_ptr<SIPCall> weak_dev_call = dev_call;
                /*dev_call->addStateListener([weak_dev_call](Call::CallState new_state, Call::ConnectionState new_cstate, int code) {
                    if (auto call = weak_dev_call.lock())
                        RING_WARN("DeviceCall call %s state changed %d %d", call->getCallId().c_str(), new_state, new_cstate);
                        //if (new_state == Call::)
                });
                dev_call->quiet = true;*/
                dev_call->setIPToIP(true);
                dev_call->setSecure(isTlsEnabled());
                auto ice = createIceTransport(("sip:" + dev_call->getCallId()).c_str(),
                                              ICE_COMPONENTS, true, getIceOptions());
                if (not ice) {
                    RING_WARN("Can't create ICE");
                    dev_call->removeCall();
                    return;
                }

                call->addSubCall(dev_call);

                auto iceInitTimeout = std::chrono::steady_clock::now() + std::chrono::seconds {ICE_INIT_TIMEOUT};

                manager.addTask([shared_this, weak_dev_call, ice, iceInitTimeout, toUri, dev] {
                    auto call = weak_dev_call.lock();

                    if (not call)
                        return false;

                    if (ice->isFailed() or std::chrono::steady_clock::now() >= iceInitTimeout) {
                        RING_DBG("ice init failed (or timeout)");
                        call->onFailure();
                        return false;
                    }

                    if (not ice->isInitialized())
                        return true; // process task again!

                    RING_WARN("ICE initialised");

                    // Next step: sent the ICE data to peer through DHT
                    const dht::Value::Id callvid  = udist(shared_this->rand_);
                    const dht::Value::Id vid  = udist(shared_this->rand_);
                    const auto callkey = dht::InfoHash::get("callto:" + dev.dev.toString());
                    dht::Value val { dht::IceCandidates(callvid, ice->getLocalAttributesAndCandidates()) };
                    val.id = vid;

                    //RING_WARN("ICE initialised");
                    shared_this->dht_.putEncrypted(
                        callkey, dev.dev,
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

                    auto listenKey = shared_this->dht_.listen<dht::IceCandidates>(
                        callkey,
                        [=] (dht::IceCandidates&& msg) {
                            if (msg.id != callvid or msg.from != dev.dev)
                                return true;
                            RING_WARN("ICE request replied from DHT peer %s\n%s", dev.dev.toString().c_str(),
                                      std::string(msg.ice_data.cbegin(), msg.ice_data.cend()).c_str());
                            if (auto call = weak_dev_call.lock())
                                call->setState(Call::ConnectionState::PROGRESSING);
                            if (!ice->start(msg.ice_data)) {
                                call->onFailure();
                                return true;
                            }
                            return false;
                        }
                    );

                    shared_this->pendingCalls_.emplace_back(PendingCall{
                        std::chrono::steady_clock::now(),
                        ice, weak_dev_call,
                        std::move(listenKey),
                        callkey, dev.dev
                    });
                    return false;
                });
            }
        });
        return true;
    }, [=](bool ok){
        RING_WARN("newOutgoingCall: found %lu devices", treatedDevices_->size());
        if (treatedDevices_->empty()) {
            if (auto call = weak_call.lock()) {
                call->onFailure();
            }
        }
    });

    return call;
}

void
RingAccount::createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to_id, IpAddr target)
{
    RING_WARN("RingAccount::createOutgoingCall to: %s target: %s",
              to_id.c_str(), target.toString(true).c_str());
    call->initIceTransport(true);
    call->setIPToIP(true);
    call->setPeerNumber(getToUri(to_id+"@"+target.toString(true).c_str()));
    call->initRecFilename(to_id);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface());
    call->setCallMediaLocal(call->getIceTransport()->getDefaultLocalAddress());

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

    pjsip_dialog *dialog = NULL;
    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, &pjTarget, &dialog) != PJ_SUCCESS) {
        RING_ERR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    pj_str_t subj_hdr_name = CONST_PJ_STR("Subject");
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
    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << Conf::DHT_PORT_KEY << YAML::Value << dhtPort_;
    out << YAML::Key << Conf::DHT_PUBLIC_IN_CALLS << YAML::Value << dhtPublicInCalls_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_HISTORY << YAML::Value << allowPeersFromHistory_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_CONTACT << YAML::Value << allowPeersFromContact_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_TRUSTED << YAML::Value << allowPeersFromTrusted_;

    out << YAML::Key << DRing::Account::ConfProperties::ARCHIVE_PATH << YAML::Value << archivePath_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT << YAML::Value << receipt_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT_SIG << YAML::Value << YAML::Binary(receiptSignature_.data(), receiptSignature_.size());
    out << YAML::Key << DRing::Account::ConfProperties::ETH::ACCOUNT << YAML::Value << ethAccount_;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void RingAccount::unserialize(const YAML::Node &node)
{
    using yaml_utils::parseValue;

    SIPAccountBase::unserialize(node);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_HISTORY, allowPeersFromHistory_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_CONTACT, allowPeersFromContact_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_TRUSTED, allowPeersFromTrusted_);

    try {
        parseValue(node, DRing::Account::ConfProperties::ETH::ACCOUNT, ethAccount_);
    } catch (const std::exception& e) {
        RING_WARN("can't read eth account: %s", e.what());
    }

    try {
        parseValue(node, DRing::Account::ConfProperties::ARCHIVE_PATH, archivePath_);
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

    parseValue(node, Conf::DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);

    loadAccount();
}

void
RingAccount::createRingDevice(const dht::crypto::Identity& id)
{
    RING_WARN("createRingDevice");
    auto dev_id = dht::crypto::generateIdentity("Ring device", id);
    if (!dev_id.first || !dev_id.second) {
        throw VoipLinkException("Can't generate identity for this account.");
    }
    idPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + getAccountID();
    fileutils::check_dir(idPath_.c_str(), 0700);

    // save the chain including CA
    saveIdentity(dev_id, idPath_ + DIR_SEPARATOR_STR "dht");
    tlsCertificateFile_ = idPath_ + DIR_SEPARATOR_STR "dht.crt";
    tlsPrivateKeyFile_ = idPath_ + DIR_SEPARATOR_STR "dht.key";
    tlsPassword_ = {};
    identity_ = dev_id;

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
    createRingDevice(a.id);
    Manager::instance().saveConfig();
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
RingAccount::hasSignedReceipt()
{
    RING_WARN("hasSignedReceipt() %s %zu", receipt_.c_str(), receiptSignature_.size());
    if (receipt_.empty() or receiptSignature_.empty())
        return false;

    if (!identity_.second) {
        RING_WARN("hasSignedReceipt() no identity");
        return false;
    }

    auto pk = identity_.second->issuer->getPublicKey();
    RING_WARN("hasSignedReceipt() with %s", pk.getId().toString().c_str());
    if (!pk.checkSignature({receipt_.begin(), receipt_.end()}, receiptSignature_)) {
        RING_WARN("hasSignedReceipt() signature check failed");
        return false;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(receipt_, root))
        return false;

    auto dev_id = root["dev"].asString();
    if (dev_id != identity_.second->getId().toString()) {
        RING_WARN("hasSignedReceipt() dev_id not matching");
        return false;
    }
    auto id = root["id"].asString();
    if (id != pk.getId().toString()) {
        RING_WARN("hasSignedReceipt() id not matching");
        return false;
    }

    dht::Value announce_val;
    try {
        auto announce = base64::decode(root["announce"].asString());
        msgpack::object_handle announce_msg = msgpack::unpack((const char*)announce.data(), announce.size());
        //dht::Value announce_val (announce_msg.get());
        announce_val.msgpack_unpack(announce_msg.get());
        if (not announce_val.checkSignature()) {
            RING_WARN("hasSignedReceipt() announce signature check failed");
            return false;
        }
        DeviceAnnouncement da;
        da.unpackValue(announce_val);
        if (da.from.toString() != id or da.dev.toString() != dev_id) {
            RING_WARN("hasSignedReceipt() announce not matching");
            return false;
        }
    } catch (const std::exception& e) {
        RING_WARN("hasSignedReceipt(): can't read announce: %s", e.what());
        return false;
    }

    ringAccountId_ = id;
    username_ = RING_URI_PREFIX + id;
    announce_ = std::make_shared<dht::Value>(std::move(announce_val));

    auto eth_addr = root["eth"].asString();
    if (eth_addr != ethAccount_) {
        RING_WARN("hasSignedReceipt() eth_addr not matching");
        ethAccount_ = eth_addr;
    }

    RING_WARN("hasSignedReceipt() -> true");
    return true;
}

dht::crypto::Identity
RingAccount::loadIdentity()
{
    RING_WARN("loadIdentity() %s %s", tlsCertificateFile_.c_str(), tlsPrivateKeyFile_.c_str());
    dht::crypto::Certificate dht_cert;
    dht::crypto::PrivateKey dht_key;
    try {
#if TARGET_OS_IPHONE
        const auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + getAccountID() + DIR_SEPARATOR_STR;
        dht_cert = dht::crypto::Certificate(fileutils::loadFile(path + tlsCertificateFile_));
        dht_key = dht::crypto::PrivateKey(fileutils::loadFile(path + tlsPrivateKeyFile_), tlsPassword_);
#else
        dht_cert = dht::crypto::Certificate(fileutils::loadFile(tlsCertificateFile_));
        dht_key = dht::crypto::PrivateKey(fileutils::loadFile(tlsPrivateKeyFile_), tlsPassword_);
#endif
        auto crt_id = dht_cert.getId();
        if (crt_id != dht_key.getPublicKey().getId())
            return {};

        identity_ = {
            std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)),
            std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))
        };
    }
    catch (const std::exception& e) {
        RING_ERR("Error loading identity: %s", e.what());
    }

    return identity_;
}

RingAccount::ArchiveContent
RingAccount::readArchive(const std::string& pwd)
{
    RING_WARN("readArchive()");

    // Read file
    std::vector<uint8_t> file = fileutils::loadFile(archivePath_);

    // Decrypt
    file = dht::crypto::aesDecrypt(file, pwd);

    // Load
    return loadArchive(file);
}


RingAccount::ArchiveContent
RingAccount::loadArchive(const std::vector<uint8_t>& dat)
{
    ArchiveContent c;
    RING_WARN("loadArchive()");

    std::vector<uint8_t> file;

    // Decompress
    try {
        file = archiver::decompress(dat);
    } catch (const std::exception& ex) {
        RING_ERR("Decompression failed: %s", ex.what());
        throw std::runtime_error("failed to read file.");
    }

    // Decode string
    std::string decoded {file.begin(), file.end()};
    Json::Value value;
    Json::Reader reader;
    if (!reader.parse(decoded.c_str(),value)) {
        RING_ERR("Failed to parse %s", reader.getFormattedErrorMessages().c_str());
        throw std::runtime_error("failed to parse JSON.");
    }

    // Import content
    try {
        c.config = DRing::getAccountTemplate(ACCOUNT_TYPE);
        for (Json::ValueIterator itr = value.begin() ; itr != value.end() ; itr++) {
            if (itr->asString().empty())
                continue;
            if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::CA_LIST_FILE) == 0) {
            } else if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::PRIVATE_KEY_FILE) == 0) {
            } else if (itr.key().asString().compare(DRing::Account::ConfProperties::TLS::CERTIFICATE_FILE) == 0) {
            } else if (itr.key().asString().compare(Conf::RING_CA_KEY) == 0) {
                c.ca_key = {base64::decode(itr->asString())};
            } else if (itr.key().asString().compare(Conf::RING_ACCOUNT_KEY) == 0) {
                c.id.first = std::make_shared<dht::crypto::PrivateKey>(base64::decode(itr->asString()));
            } else if (itr.key().asString().compare(Conf::RING_ACCOUNT_CERT) == 0) {
                c.id.second = std::make_shared<dht::crypto::Certificate>(base64::decode(itr->asString()));
            } else if (itr.key().asString().compare(Conf::ETH_KEY) == 0) {
                c.eth_key = base64::decode(itr->asString());
            } else
                c.config[itr.key().asString()] = itr->asString();
        }
    } catch (const std::exception& ex) {
        RING_ERR("Can't parse JSON: %s", ex.what());
    }

    return c;
}


std::vector<uint8_t>
RingAccount::makeArchive(const ArchiveContent& archive)
{
    RING_WARN("makeArchive()");

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

    root[Conf::RING_CA_KEY] = base64::encode(archive.ca_key.serialize());
    root[Conf::RING_ACCOUNT_KEY] = base64::encode(archive.id.first->serialize());
    root[Conf::RING_ACCOUNT_CERT] = base64::encode(archive.id.second->getPacked());
    root[Conf::ETH_KEY] = base64::encode(archive.eth_key);

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
        RING_ERR("Can't export archive: %s", ex.what());
        return;
    }

    // Encrypt using provided password
    auto encrypted = dht::crypto::aesEncrypt(archive, pwd);

    // Write
    try {
        if (archivePath_.empty())
            archivePath_ = idPath_ + DIR_SEPARATOR_STR "export.gz";
        fileutils::saveFile(archivePath_, encrypted);
    } catch (const std::runtime_error& ex) {
        RING_ERR("Export failed: %s", ex.what());
        return;
    }
}

std::pair<std::vector<uint8_t>, dht::InfoHash>
RingAccount::computeKeys(const std::string& password, const std::string& pin)
{
    // Generate key for archive encryption, using PIN as the salt
    std::vector<uint8_t> salt_key {pin.begin(), pin.end()};
    auto key = dht::crypto::stretchKey(password, salt_key, 256/8);

    // Generate public storage location, using SHA1(PIN) as the salt
    //auto pin_h = dht::InfoHash::get(archive_pin);
    //std::vector<uint8_t> salt_loc {pin_h.begin(), pin_h.end()};
    auto loc = dht::InfoHash::get(key/*dht::crypto::stretchKey(archive_password, salt_loc)*/);

    return {key, loc};
}

std::string
RingAccount::addDevice(const std::string& password)
{
    try {
        RING_DBG("Exporting Ring account %s", getAccountID().c_str());

        // Generate random 32bits PIN
        std::uniform_int_distribution<uint32_t> dis;
        auto pin = dis(rand_);
        // Manipulate PIN as hex
        std::stringstream ss;
        ss << std::hex << pin;
        auto pin_str = ss.str();

        std::vector<uint8_t> key;
        dht::InfoHash loc;
        std::tie(key, loc) = computeKeys(password, pin_str);

        auto archive = makeArchive(readArchive(password));
        auto encrypted = dht::crypto::aesEncrypt(archive, key);
        dht_.put(loc, encrypted, [](bool ok) {
            RING_WARN("Done publishing account archive: %s", ok ? "success" : "failure");
        });
        RING_WARN("Adding new device with PIN: %s at %s (size %zu)", pin_str.c_str(), loc.toString().c_str(), encrypted.size());

        return pin_str;
    } catch (const std::exception& e) {
        RING_ERR("Can't add device: %s", e.what());
        return {};
    }
}

void
RingAccount::saveIdentity(const dht::crypto::Identity id, const std::string& path) const
{
    if (id.first)
        fileutils::saveFile(path + ".key", id.first->serialize(), 0600);
    if (id.second)
        fileutils::saveFile(path + ".crt", id.second->getPacked(), 0600);
}

void
RingAccount::loadAccount(const std::string& archive_password, const std::string& archive_pin)
{
    try {
        loadIdentity();

        // have archive
        if (archivePath_.empty() or not fileutils::isFile(archivePath_)) {
            // have identity
            if (hasSignedReceipt()) {
                RING_WARN("Account archive not found, won't be able to add new devices.");
                // good
            } else {
                if (archive_password.empty()) {
                    RING_WARN("Password needed to create archive");
                    // need password
                    //emitSignal<>();
                    //if (identity_.first && identity_.second && identity_.first->getPublicKey)
                    if (identity_.first and identity_.second) {
                        RING_WARN("Loading bare Ring account");
                        ringAccountId_ = identity_.second->getId().toString();
                        username_ = RING_URI_PREFIX+ringAccountId_;
                    }
                } else {
                    if (not archive_pin.empty()) {
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

                        std::vector<uint8_t> key;
                        dht::InfoHash loc;
                        std::tie(key, loc) = computeKeys(archive_password, archive_pin);

                        RING_WARN("Trying to load account from DHT with %s at %s (port %u)", archive_pin.c_str(), loc.toString().c_str(), (in_port_t)dhtPortUsed_);

                        std::weak_ptr<RingAccount> w = std::static_pointer_cast<RingAccount>(shared_from_this());
                        dht_.get(loc, [=](std::shared_ptr<dht::Value> val) {
                            RING_WARN("Found something on the DHT");
                            std::vector<uint8_t> decrypted;
                            try {
                                decrypted = dht::crypto::aesDecrypt(val->data, key);
                            } catch (const std::exception& ex) {
                                RING_ERR("Decryption failed: %s", ex.what());
                                return true;
                            }
                            RING_WARN("Found archive on the DHT");
                            runOnMainThread([=]() {
                                try {
                                    auto a = loadArchive(decrypted);
                                    if (auto this_ = w.lock()) {
                                        this_->initRingDevice(a);
                                        this_->saveArchive(a, archive_password);
                                        Manager::instance().saveConfig();
                                        this_->doRegister();
                                    }
                                } catch (const std::exception& e) {
                                    RING_WARN("Error reading archive: %s", e.what());
                                }
                            });
                            return false;
                        }, [](bool ok){
                            RING_WARN("Done looking for account archive on DHT: %s", ok ? "success" : "failure");
                        });
                    } else {
                        RING_WARN("Creating new Ring account");
                        ArchiveContent a;

                        RING_WARN("Generating ETH key");
                        auto keypair = dev::KeyPair::create();
                        ethAccount_ = keypair.address().hex();
                        a.eth_key = keypair.secret().makeInsecure().asBytes();

                        if (identity_.first and identity_.second) {
                            RING_WARN("Converting certificate from old ring account");
                            a.id = identity_;
                            try {
                                a.ca_key = fileutils::loadFile(idPath_ + DIR_SEPARATOR_STR "ca.key");
                            } catch (...) {}
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
                            a.ca_key = std::move(*ca.first);
                        }
                        ringAccountId_ = a.id.second->getId().toString();
                        username_ = RING_URI_PREFIX+ringAccountId_;
                        createRingDevice(a.id);
                        saveArchive(a, archive_password);
                        Manager::instance().saveConfig();
                    }
                }
            }
        } else {
            if (hasSignedReceipt()) {
                // good
            } else {
                if (archive_password.empty()) {
                    RING_WARN("Password needed to read archive");
                } else {
                    RING_WARN("Archive present but no valid receipt : creating new device");
                    auto a = readArchive(archive_password);
                    initRingDevice(a);
                }
            }
        }
    } catch (const std::exception& e) {
        RING_WARN("Error laoding account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
RingAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    SIPAccountBase::setAccountDetails(details);
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

    std::string archive_password = "toto";
    std::string archive_pin;
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PASSWORD, archive_password);
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PIN,      archive_pin);
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PATH,     archivePath_);

    loadAccount(archive_password, archive_pin);
}

std::map<std::string, std::string>
RingAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    a.emplace(Conf::CONFIG_DHT_PORT, ring::to_string(dhtPort_));
    a.emplace(Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_ ? TRUE_STR : FALSE_STR);

    /* these settings cannot be changed (read only), but clients should still be
     * able to read what they are */
    a.emplace(Conf::CONFIG_SRTP_KEY_EXCHANGE, sip_utils::getKeyExchangeName(getSrtpKeyExchange()));
    a.emplace(Conf::CONFIG_SRTP_ENABLE,       isSrtpEnabled() ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_SRTP_RTP_FALLBACK, getSrtpFallback() ? TRUE_STR : FALSE_STR);

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
    a.emplace(DRing::Account::ConfProperties::ETH::ACCOUNT,                ethAccount_);

    return a;
}

std::map<std::string, std::string>
RingAccount::getVolatileAccountDetails() const
{
    auto a = SIPAccountBase::getVolatileAccountDetails();
    a.emplace(DRing::Account::VolatileProperties::InstantMessaging::OFF_CALL, TRUE_STR);
    return a;
}

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
    tls::TlsParams tlsParams {
        .ca_list = "",
        .cert = identity_.second,
        .cert_key = identity_.first,
        .dh_params = dhParams_,
        .timeout = std::chrono::duration_cast<decltype(tls::TlsParams::timeout)>(TLS_TIMEOUT),
        .cert_check = [w,call,remote_h,incoming](unsigned status, const gnutls_datum_t* cert_list, unsigned cert_num) -> pj_status_t {
            try {
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
                } else
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

        dht_.setOnStatusChanged([this](dht::NodeStatus s4, dht::NodeStatus s6) {
                RING_WARN("Dht status : IPv4 %s; IPv6 %s", dhtStatusStr(s4), dhtStatusStr(s6));
                RegistrationState state;
                switch (std::max(s4, s6)) {
                    case dht::NodeStatus::Connecting:
                        state = RegistrationState::TRYING;
                        break;
                    case dht::NodeStatus::Connected:
                        state = RegistrationState::REGISTERED;
                        break;
                    case dht::NodeStatus::Disconnected:
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
            auto& store = tls::CertificateStore::instance();
            auto cert = store.getCertificate(pk_id.toString());
            std::vector<std::shared_ptr<dht::crypto::Certificate>> ret;
            if (cert)
                ret.emplace_back(std::move(cert));
            RING_DBG("Query for local certificate store: %s: %zu found.", pk_id.toString().c_str(), ret.size());
            return ret;
        });

#if 0 // enable if dht_ logging is needed
        dht_.setLoggers(
            [](char const* m, va_list args){ vlogger(LOG_ERR, m, args); },
            [](char const* m, va_list args){ vlogger(LOG_WARNING, m, args); },
            [](char const* m, va_list args){ /*vlogger(LOG_DEBUG, m, args);*/ }
        );
#endif

        dht_.importValues(loadValues());

        Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
        setRegistrationState(RegistrationState::TRYING);

        dht_.bootstrap(loadNodes());
        auto bootstrap = loadBootstrap();
        if (not bootstrap.empty())
            dht_.bootstrap(bootstrap);

        // Put device annoucement
        if (announce_) {
            RING_DBG("Announcing device at %s: %s", dht::InfoHash(ringAccountId_).toString().c_str(), announce_->toString().c_str());
            dht_.put(dht::InfoHash(ringAccountId_), announce_, dht::DoneCallback{}, {}, true);
        }

        // Listen for incoming calls
        auto ringDeviceId = identity_.first->getPublicKey().getId().toString();
        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        callKey_ = dht::InfoHash::get("callto:"+ringDeviceId);
        RING_DBG("Listening on callto:%s : %s", ringDeviceId.c_str(), callKey_.toString().c_str());
        dht_.listen<dht::IceCandidates>(
            callKey_,
            [shared] (dht::IceCandidates&& msg) {
                // callback for incoming call
                auto& this_ = *shared;
                if (msg.from == this_.dht_.getId())
                    return true;

                RING_WARN("ICE candidate from %s.", msg.from.toString().c_str());

                // quick check in case we already explicilty banned this public key
                auto trustStatus = this_.trust_.getCertificateStatus(msg.from.toString());
                if (trustStatus == tls::TrustStore::PermissionStatus::BANNED) {
                    RING_WARN("Discarding incoming DHT call request from banned peer %s", msg.from.toString().c_str());
                    return true;
                }

                auto res = this_.treatedCalls_.insert(msg.id);
                this_.saveTreatedCalls();
                if (!res.second)
                    return true;

                RING_WARN("findCertificate");
                this_.findCertificate( msg.from,
                    [shared, msg, trustStatus](const std::shared_ptr<dht::crypto::Certificate> cert) mutable {
                    RING_WARN("findCertificate: found %p", cert.get());
                    auto& this_ = *shared;
                    if (not this_.dhtPublicInCalls_ and trustStatus != tls::TrustStore::PermissionStatus::ALLOWED) {
                        if (!cert or cert->getId() != msg.from) {
                            RING_WARN("Can't find certificate of %s for incoming call.",
                                      msg.from.toString().c_str());
                            return;
                        }

                        tls::CertificateStore::instance().pinCertificate(cert);

                        auto& this_ = *shared;
                        if (!this_.trust_.isAllowed(*cert)) {
                            RING_WARN("Discarding incoming DHT call from untrusted peer %s.",
                                      msg.from.toString().c_str());
                            return;
                        }

                        runOnMainThread([=]() mutable { shared->incomingCall(std::move(msg), cert); });
                    } else if (this_.dhtPublicInCalls_ and trustStatus != tls::TrustStore::PermissionStatus::BANNED) {
                        //this_.findCertificate(msg.from.toString().c_str());
                        runOnMainThread([=]() mutable { shared->incomingCall(std::move(msg), cert); });
                    }
                });
                // public incoming calls allowed or we explicitly authorised this public key
                //runOnMainThread([=]() mutable { shared->incomingCall(std::move(msg), {}); });
                return true;
            }
        );

        auto inboxKey = dht::InfoHash::get("inbox:"+ringAccountId_);
        dht_.listen<dht::TrustRequest>(
            inboxKey,
            [shared](dht::TrustRequest&& v) {
                auto& this_ = *shared.get();
                if (v.service != DHT_TYPE_NS)
                    return true;
                // if the invite exists, update it
                auto req = this_.trustRequests_.begin();
                for (;req != this_.trustRequests_.end(); ++req)
                    if (req->from == v.from) {
                        req->received = std::chrono::system_clock::now();
                        req->payload = v.payload;
                        break;
                    }
                if (req == this_.trustRequests_.end()) {
                    this_.trustRequests_.emplace_back(TrustRequest{
                        .from = v.from,
                        .received = std::chrono::system_clock::now(),
                        .payload = v.payload
                    });
                    req = std::prev(this_.trustRequests_.end());
                }
                emitSignal<DRing::ConfigurationSignal::IncomingTrustRequest>(
                    this_.getAccountID(),
                    req->from.toString(),
                    req->payload,
                    std::chrono::system_clock::to_time_t(req->received)
                );
                return true;
            }
        );

        dht_.listen<dht::ImMessage>(
            inboxKey,
            [shared, inboxKey](dht::ImMessage&& v) {
                auto& this_ = *shared.get();
                auto res = this_.treatedMessages_.insert(v.id);
                if (!res.second)
                    return true;
                this_.saveTreatedMessages();

                auto from = v.from.toString();
                auto now = system_clock::to_time_t(system_clock::now());
                std::map<std::string, std::string> payloads = {{"text/plain",
                                                                utf8_make_valid(v.msg)}};
                shared->onTextMessage(from, payloads);
                RING_DBG("Sending message confirmation %" PRIu64, v.id);
                this_.dht_.putEncrypted(inboxKey,
                          v.from,
                          dht::ImMessage(v.id, std::string(), now));
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
RingAccount::incomingCall(dht::IceCandidates&& msg, std::shared_ptr<dht::crypto::Certificate> from_cert)
{
    auto from = msg.from.toString();
    RING_WARN("ICE incoming from DHT peer %s\n%s", from.c_str(),
              std::string(msg.ice_data.cbegin(), msg.ice_data.cend()).c_str());
    auto call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(*this, Manager::instance().getNewCallID(), Call::CallType::INCOMING);
    auto ice = createIceTransport(("sip:"+call->getCallId()).c_str(), ICE_COMPONENTS, false, getIceOptions());
    const auto vid = udist(rand_);
    dht::Value val { dht::IceCandidates(msg.id, ice->getLocalAttributesAndCandidates()) };
    val.id = vid;

    std::weak_ptr<SIPCall> weak_call = call;
    auto shared_this = std::static_pointer_cast<RingAccount>(shared_from_this());
    dht_.putEncrypted(
        callKey_,
        msg.from,
        std::move(val),
        [weak_call, shared_this, vid](bool ok) {
            if (!ok) {
                RING_WARN("Can't put ICE descriptor reply on DHT");
                if (auto call = weak_call.lock())
                    call->onFailure();
            } else
                RING_DBG("Successfully put ICE descriptor reply on DHT");
        }
    );
    if (!ice->start(msg.ice_data)) {
        call->onFailure();
        return;
    }
    call->setPeerNumber(from);
    call->initRecFilename(from);
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.emplace_back(PendingCall {
            .start = std::chrono::steady_clock::now(),
            .ice_sp = ice,
            .call = weak_call,
            .listen_key = {},
            .call_key = {},
            .from = msg.from,
            .from_cert = from_cert
        });
    }
}

void
RingAccount::doUnregister(std::function<void(bool)> released_cb)
{
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
    if (not isUsable()) {
        // nothing to do
        return;
    }

    auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
    doUnregister([shared](bool /* transport_free */) {
        if (shared->isUsable())
            shared->doRegister();
    });
}


bool
RingAccount::findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>)> cb)
{
    if (auto cert = tls::CertificateStore::instance().getCertificate(h.toString())) {
        if (cb)
            cb(cert);
    } else {
        dht_.findCertificate(h, [=](const std::shared_ptr<dht::crypto::Certificate> crt) {
            if (crt)
                tls::CertificateStore::instance().pinCertificate(std::move(crt));
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

std::set<dht::Value::Id>
loadIdList(const std::string& path)
{
    std::set<dht::Value::Id> ids;
    std::ifstream file(path);
    if (!file.is_open()) {
        RING_DBG("Could not load %s", path.c_str());
        return ids;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        dht::Value::Id vid;
        if (!(iss >> std::hex >> vid)) { break; }
        ids.insert(vid);
    }
    return ids;
}

void
saveIdList(const std::string& path, const std::set<dht::Value::Id>& ids)
{
    std::ofstream file(path, std::ios::trunc);
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
RingAccount::saveNodes(const std::vector<dht::NodeExport>& nodes) const
{
    if (nodes.empty())
        return;
    fileutils::check_dir(cachePath_.c_str());
    std::string nodesPath = cachePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ofstream file(nodesPath, std::ios::trunc);
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
            RING_ERR("Error reading value: %s", e.what());
        }
        remove(file.c_str());
    }
    RING_DBG("Loaded %zu values", values.size());
    return values;
}

tls::DhParams
RingAccount::loadDhParams(const std::string path)
{
    try {
        // writeTime throw exception if file doesn't exist
        auto duration = system_clock::now() - fileutils::writeTime(path);
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

    std::packaged_task<decltype(loadDhParams)> task(loadDhParams);
    dhParams_ = task.get_future();
    std::thread task_td(std::move(task), cachePath_ + DIR_SEPARATOR_STR "dhParams");
    task_td.detach();
}

MatchRank
RingAccount::matches(const std::string &userName, const std::string &server) const
{
    //auto dhtId = dht_.getId().toString();
    if (userName == ringAccountId_ || server == ringAccountId_ || userName == identity_.second->getId().toString()) {
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
    return "<sips:" + to + ";transport=tls>";
}

pj_str_t
RingAccount::getContactHeader(pjsip_transport* t)
{
    if (t) {
        // FIXME: be sure that given transport is from SipIceTransport
        auto tlsTr = reinterpret_cast<tls::SipsIceTransport::TransportData*>(t)->self;
        auto address = tlsTr->getLocalAddress().toString(true);
        contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                         "%s%s<sips:%s%s%s;transport=tls>",
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

/**
 *  Enable the presence module
 */
void
RingAccount::enablePresence(const bool& /* enabled */)
{
}

/**
 *  Set the presence (PUBLISH/SUBSCRIBE) support flags
 *  and process the change.
 */
void
RingAccount::supportPresence(int /* function */, bool /* enabled*/)
{
}

/* trust requests */
std::map<std::string, std::string>
RingAccount::getTrustRequests() const
{
    std::map<std::string, std::string> ret;
    for (const auto& r : trustRequests_)
        ret.emplace(r.from.toString(), ring::to_string(std::chrono::system_clock::to_time_t(r.received)));
    return ret;
}

bool
RingAccount::acceptTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    for (auto i = std::begin(trustRequests_); i != std::end(trustRequests_); ++i) {
        if (i->from == f) {
            trust_.setCertificateStatus(from, tls::TrustStore::PermissionStatus::ALLOWED);
            trustRequests_.erase(i);
            return true;
        }
    }
    return false;
}

bool
RingAccount::discardTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    for (auto i = std::begin(trustRequests_); i != std::end(trustRequests_); ++i) {
        if (i->from == f) {
            trustRequests_.erase(i);
            return true;
        }
    }
    return false;
}

void
RingAccount::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    setCertificateStatus(to, tls::TrustStore::PermissionStatus::ALLOWED);
    dht_.putEncrypted(dht::InfoHash::get("inbox:"+to),
                      dht::InfoHash(to),
                      dht::TrustRequest(DHT_TYPE_NS, payload));
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
RingAccount::sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads, uint64_t token)
{
    if (to.empty() or payloads.empty()) {
        messageEngine_.onMessageSent(token, false);
        return;
    }

    auto toUri = parseRingUri(to);
    auto toH = dht::InfoHash(toUri);
    auto now = system_clock::to_time_t(system_clock::now());

    // Single-part message
    if (payloads.size() == 1) {
        auto e = sentMessages_.emplace(token, PendingMessage {});
        e.first->second.to = toH;

        auto h = dht::InfoHash::get("inbox:"+toUri);
        std::weak_ptr<RingAccount> wshared = std::static_pointer_cast<RingAccount>(shared_from_this());

        dht_.listen<dht::ImMessage>(h, [wshared,token](dht::ImMessage&& msg) {
            if (auto this_ = wshared.lock()) {
                // check expected message confirmation
                if (msg.id != token)
                    return true;
                auto e = this_->sentMessages_.find(msg.id);
                if (e == this_->sentMessages_.end()) {
                        RING_DBG("Message not found for %" PRIu64, token);
                }
                if (e->second.to != msg.from) {
                        RING_DBG("Unrelated text message : from %s != second %s",
                                 msg.from.toString().c_str(), e->second.to.toString().c_str());
                }
                if (e == this_->sentMessages_.end() || e->second.to != msg.from) {
                    RING_DBG("Unrelated text message reply for %" PRIu64, token);
                    return true;
                }
                this_->sentMessages_.erase(e);
                RING_DBG("Relevant text message reply for %" PRIu64, token);

                // add treated message
                auto res = this_->treatedMessages_.insert(msg.id);
                if (!res.second)
                    return true;

                this_->saveTreatedMessages();

                // report message as confirmed received
                this_->messageEngine_.onMessageSent(token, true);
            }
            return false;
        });

        dht_.putEncrypted(h,
                          toH,
                          dht::ImMessage(token, std::string(payloads.begin()->second), now),
                          [wshared,token](bool ok) {
                            if (auto this_ = wshared.lock()) {
                                if (not ok)
                                    this_->messageEngine_.onMessageSent(token, false);
                            }
                        });
        return;
    }

    // Multi-part message
    // TODO: not supported yet
    RING_ERR("Multi-part im is not supported yet by RingAccount");
    messageEngine_.onMessageSent(token, false);
}

} // namespace ring
