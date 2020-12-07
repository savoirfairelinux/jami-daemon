/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Simon Désaulniers <simon.desaulniers@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
 *  Author: Mingrui Zhang <mingrui.zhang@savoirfairelinux.com>
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

#include "jamiaccount.h"

#include "logger.h"

#include "accountarchive.h"
#include "jami_contact.h"
#include "configkeys.h"
#include "contact_list.h"
#include "archive_account_manager.h"
#include "server_account_manager.h"
#include "jamidht/channeled_transport.h"
#include "multiplexed_socket.h"

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

#ifdef ENABLE_VIDEO
#include "libav_utils.h"
#endif
#include "fileutils.h"
#include "string_utils.h"
#include "array_size.h"
#include "archiver.h"
#include "data_transfer.h"

#include "config/yamlparser.h"
#include "security/certstore.h"
#include "libdevcrypto/Common.h"
#include "base64.h"
#include "im/instant_messaging.h"

#include <opendht/thread_pool.h>
#include <opendht/peer_discovery.h>
#include <opendht/http.h>

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

using namespace std::placeholders;

namespace jami {

constexpr pj_str_t STR_MESSAGE_ID = jami::sip_utils::CONST_PJ_STR("Message-ID");

struct PendingConfirmation
{
    std::mutex lock;
    bool replied {false};
    std::map<dht::InfoHash, std::future<size_t>> listenTokens {};
};

// Used to pass infos to a pjsip callback (pjsip_endpt_send_request)
struct TextMessageCtx
{
    std::weak_ptr<JamiAccount> acc;
    std::string to;
    DeviceId deviceId;
    uint64_t id;
    bool retryOnTimeout;
    std::shared_ptr<PendingConfirmation> confirmation;
};

struct VCardMessageCtx
{
    std::shared_ptr<std::atomic_int> success;
    int total;
    std::string path;
};

namespace Migration {

enum class State { // Contains all the Migration states
    SUCCESS,
    INVALID
};

std::string
mapStateNumberToString(const State migrationState)
{
#define CASE_STATE(X) \
    case Migration::State::X: \
        return #X

    switch (migrationState) {
        CASE_STATE(INVALID);
        CASE_STATE(SUCCESS);
    }
    return {};
}

void
setState(const std::string& accountID, const State migrationState)
{
    emitSignal<DRing::ConfigurationSignal::MigrationEnded>(accountID,
                                                           mapStateNumberToString(migrationState));
}

} // namespace Migration

struct JamiAccount::BuddyInfo
{
    /* the buddy id */
    dht::InfoHash id;

    /* number of devices connected on the DHT */
    uint32_t devices_cnt {};

    /* The disposable object to update buddy info */
    std::future<size_t> listenToken;

    BuddyInfo(dht::InfoHash id)
        : id(id)
    {}
};

struct JamiAccount::PendingCall
{
    std::chrono::steady_clock::time_point start;
    std::shared_ptr<IceTransport> ice_sp;
    std::shared_ptr<IceTransport> ice_tcp_sp;
    std::weak_ptr<SIPCall> call;
    std::future<size_t> listen_key;
    dht::InfoHash call_key;
    dht::InfoHash from;
    dht::InfoHash from_account;
    std::shared_ptr<dht::crypto::Certificate> from_cert;
};

struct JamiAccount::PendingMessage
{
    std::set<dht::InfoHash> to;
};

struct AccountPeerInfo
{
    dht::InfoHash accountId;
    std::string displayName;
    MSGPACK_DEFINE(accountId, displayName)
};

struct JamiAccount::DiscoveredPeer
{
    std::string displayName;
    std::shared_ptr<Task> cleanupTask;
};

static constexpr int ICE_COMPONENTS {1};
static constexpr int ICE_COMP_SIP_TRANSPORT {0};
static constexpr auto ICE_NEGOTIATION_TIMEOUT = std::chrono::seconds(60);
static constexpr auto TLS_TIMEOUT = std::chrono::seconds(40);
const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);

static constexpr const char* const RING_URI_PREFIX = "ring:";
static constexpr const char* const JAMI_URI_PREFIX = "jami:";
static constexpr const char* DEFAULT_TURN_SERVER = "turn.jami.net";
static constexpr const char* DEFAULT_TURN_USERNAME = "ring";
static constexpr const char* DEFAULT_TURN_PWD = "ring";
static constexpr const char* DEFAULT_TURN_REALM = "ring";
static const auto PROXY_REGEX = std::regex(
    "(https?://)?([\\w\\.\\-_\\~]+)(:(\\d+)|:\\[(.+)-(.+)\\])?");
static const std::string PEER_DISCOVERY_JAMI_SERVICE = "jami";
const constexpr auto PEER_DISCOVERY_EXPIRATION = std::chrono::minutes(1);

constexpr const char* const JamiAccount::ACCOUNT_TYPE;
constexpr const std::pair<uint16_t, uint16_t> JamiAccount::DHT_PORT_RANGE {4000, 8888};

using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

static std::string_view
stripPrefix(std::string_view toUrl)
{
    auto dhtf = toUrl.find(RING_URI_PREFIX);
    if (dhtf != std::string_view::npos) {
        dhtf = dhtf + 5;
    } else {
        dhtf = toUrl.find(JAMI_URI_PREFIX);
        if (dhtf != std::string_view::npos) {
            dhtf = dhtf + 5;
        } else {
            dhtf = toUrl.find("sips:");
            dhtf = (dhtf == std::string_view::npos) ? 0 : dhtf + 5;
        }
    }
    while (dhtf < toUrl.length() && toUrl[dhtf] == '/')
        dhtf++;
    return toUrl.substr(dhtf);
}

static std::string_view
parseJamiUri(std::string_view toUrl)
{
    auto sufix = stripPrefix(toUrl);
    if (sufix.length() < 40)
        throw std::invalid_argument("id must be a Jami infohash");

    const std::string_view toUri = sufix.substr(0, 40);
    if (std::find_if_not(toUri.cbegin(), toUri.cend(), ::isxdigit) != toUri.cend())
        throw std::invalid_argument("id must be a Jami infohash");
    return toUri;
}

static constexpr const char*
dhtStatusStr(dht::NodeStatus status)
{
    return status == dht::NodeStatus::Connected
               ? "connected"
               : (status == dht::NodeStatus::Connecting ? "connecting" : "disconnected");
}

/**
 * Local ICE Transport factory helper
 *
 * JamiAccount must use this helper than direct IceTranportFactory API
 */
template<class... Args>
std::shared_ptr<IceTransport>
JamiAccount::createIceTransport(const Args&... args)
{
    auto ice = Manager::instance().getIceTransportFactory().createTransport(args...);
    if (!ice)
        throw std::runtime_error("ICE transport creation failed");

    return ice;
}

JamiAccount::JamiAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID)
    , dht_(new dht::DhtRunner)
    , idPath_(fileutils::get_data_dir() + DIR_SEPARATOR_STR + getAccountID())
    , cachePath_(fileutils::get_cache_dir() + DIR_SEPARATOR_STR + getAccountID())
    , dataPath_(cachePath_ + DIR_SEPARATOR_STR "values")
    , dhtPeerConnector_ {}
    , connectionManager_ {}
{
    // Force the SFL turn server if none provided yet
    turnServer_ = DEFAULT_TURN_SERVER;
    turnServerUserName_ = DEFAULT_TURN_USERNAME;
    turnServerPwd_ = DEFAULT_TURN_PWD;
    turnServerRealm_ = DEFAULT_TURN_REALM;
    turnEnabled_ = true;

    proxyListUrl_ = DHT_DEFAULT_PROXY_LIST_URL;
    proxyServer_ = DHT_DEFAULT_PROXY;

    try {
        std::istringstream is(fileutils::loadCacheTextFile(cachePath_ + DIR_SEPARATOR_STR "dhtproxy",
                                                           std::chrono::hours(24 * 7)));
        std::getline(is, proxyServerCached_);
    } catch (const std::exception& e) {
        JAMI_DBG("Can't load proxy URL from cache: %s", e.what());
    }

    setActiveCodecs({});
}

JamiAccount::~JamiAccount()
{
    hangupCalls();
    shutdownConnections();
    if (peerDiscovery_) {
        peerDiscovery_->stopPublish(PEER_DISCOVERY_JAMI_SERVICE);
        peerDiscovery_->stopDiscovery(PEER_DISCOVERY_JAMI_SERVICE);
    }
    if (auto dht = dht_)
        dht->join();
}

void
JamiAccount::shutdownConnections()
{
    connectionManager_.reset();
    dhtPeerConnector_.reset();
    std::lock_guard<std::mutex> lk(sipConnsMtx_);
    sipConns_.clear();
}

void
JamiAccount::flush()
{
    // Class base method
    SIPAccountBase::flush();

    fileutils::removeAll(dataPath_);
    fileutils::removeAll(cachePath_);
    fileutils::removeAll(idPath_, true);
}

std::shared_ptr<SIPCall>
JamiAccount::newIncomingCall(const std::string& from,
                             const std::map<std::string, std::string>& details,
                             const std::shared_ptr<SipTransport>& sipTr)
{
    std::lock_guard<std::mutex> lock(callsMutex_);

    if (sipTr) {
        std::unique_lock<std::mutex> lk(sipConnsMtx_);
        for (auto& [key, value] : sipConns_) {
            if (key.first == from) {
                // For each SipConnection of the device
                for (auto cit = value.rbegin(); cit != value.rend(); ++cit) {
                    // Search linked Sip Transport
                    if (cit->transport != sipTr)
                        continue;

                    auto call = Manager::instance().callFactory.newCall<SIPCall, JamiAccount>(
                        shared(), Manager::instance().getNewCallID(), Call::CallType::INCOMING);
                    if (!call)
                        return {};

                    std::weak_ptr<SIPCall> wcall = call;
                    call->setPeerUri(RING_URI_PREFIX + from);
                    call->setPeerNumber(from);

                    call->updateDetails(details);
                    return call;
                }
            }
        }
        lk.unlock();
    }

    auto call_it = pendingSipCalls_.begin();
    while (call_it != pendingSipCalls_.end()) {
        auto call = call_it->call.lock();
        if (not call) {
            JAMI_WARN("newIncomingCall: discarding deleted call");
            call_it = pendingSipCalls_.erase(call_it);
        } else if (call->getPeerNumber() == from
                   || (call_it->from_cert and call_it->from_cert->issuer
                       and call_it->from_cert->issuer->getId().toString() == from)) {
            JAMI_DBG("newIncomingCall: found matching call for %s", from.c_str());
            pendingSipCalls_.erase(call_it);
            call->updateDetails(details);
            return call;
        } else {
            ++call_it;
        }
    }
    JAMI_ERR("newIncomingCall: can't find matching call for %s", from.c_str());
    return nullptr;
}

template<>
std::shared_ptr<SIPCall>
JamiAccount::newOutgoingCall(std::string_view toUrl,
                             const std::map<std::string, std::string>& volatileCallDetails)
{
    auto suffix = stripPrefix(toUrl);
    JAMI_DBG() << *this << "Calling DHT peer " << suffix;
    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, JamiAccount>(shared(),
                                                                  manager.getNewCallID(),
                                                                  Call::CallType::OUTGOING,
                                                                  volatileCallDetails);

    call->setIPToIP(true);
    call->setSecure(isTlsEnabled());

    try {
        const std::string toUri {parseJamiUri(suffix)};
        startOutgoingCall(call, toUri);
    } catch (...) {
#if HAVE_RINGNS
        NameDirectory::lookupUri(suffix,
                                 nameServer_,
                                 [wthis_ = weak(), call](const std::string& result,
                                                         NameDirectory::Response response) {
                                     // we may run inside an unknown thread, but following code must
                                     // be called in main thread
                                     runOnMainThread([wthis_, result, response, call]() {
                                         if (response != NameDirectory::Response::found) {
                                             call->onFailure(EINVAL);
                                             return;
                                         }
                                         if (auto sthis = wthis_.lock()) {
                                             try {
                                                 const std::string toUri {parseJamiUri(result)};
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
initICE(const std::vector<uint8_t>& msg,
        const std::shared_ptr<IceTransport>& ice,
        const std::shared_ptr<IceTransport>& ice_tcp,
        bool& udp_failed,
        bool& tcp_failed)
{
    auto sdp_list = IceTransport::parseSDPList(msg);
    for (const auto& sdp : sdp_list) {
        if (sdp.candidates.size() > 0) {
            if (sdp.candidates[0].find("TCP") != std::string::npos) {
                // It is a SDP for the TCP component
                tcp_failed = (ice_tcp && !ice_tcp->start(sdp));
            } else {
                // For UDP
                udp_failed = (ice && !ice->start(sdp));
            }
        }
    }

    // During the ICE reply we can start the ICE negotiation
    if (tcp_failed && ice_tcp) {
        ice_tcp->stop();
        JAMI_WARN("ICE over TCP not started, will only use UDP");
    }
}

void
JamiAccount::startOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& toUri)
{
    if (not accountManager_ or not dht_) {
        call->onFailure(ENETDOWN);
        return;
    }
    // TODO: for now, we automatically trust all explicitly called peers
    setCertificateStatus(toUri, tls::TrustStore::PermissionStatus::ALLOWED);

    call->setPeerNumber(toUri + "@ring.dht");
    call->setPeerUri(RING_URI_PREFIX + toUri);
    call->setState(Call::ConnectionState::TRYING);
    std::weak_ptr<SIPCall> wCall = call;

#if HAVE_RINGNS
    accountManager_->lookupAddress(toUri,
                                   [wCall](const std::string& result,
                                           const NameDirectory::Response& response) {
                                       if (response == NameDirectory::Response::found)
                                           if (auto call = wCall.lock()) {
                                               call->setPeerRegisteredName(result);
                                               call->setPeerUri(RING_URI_PREFIX + result);
                                           }
                                   });
#endif

    dht::InfoHash peer_account(toUri);

    // Call connected devices
    std::set<DeviceId> devices;
    std::unique_lock<std::mutex> lk(sipConnsMtx_);
    // NOTE: dummyCall is a call used to avoid to mark the call as failed if the
    // cached connection is failing with ICE (close event still not detected).
    auto& manager = Manager::instance();
    auto dummyCall = manager.callFactory.newCall<SIPCall, JamiAccount>(shared(),
                                                                       manager.getNewCallID(),
                                                                       Call::CallType::OUTGOING,
                                                                       call->getDetails());
    dummyCall->setIPToIP(true);
    dummyCall->setSecure(isTlsEnabled());
    call->addSubCall(*dummyCall);
    auto sendRequest =
        [this, wCall, toUri, dummyCall = std::move(dummyCall)](const DeviceId& deviceId,
                                                               bool eraseDummy) {
            if (eraseDummy) {
                // Mark the temp call as failed to stop the main call if necessary
                if (dummyCall)
                    dummyCall->onFailure(static_cast<int>(std::errc::no_such_device_or_address));
                return;
            }
            auto call = wCall.lock();
            if (not call)
                return;
            auto state = call->getConnectionState();
            if (state != Call::ConnectionState::PROGRESSING
                and state != Call::ConnectionState::TRYING)
                return;

            auto dev_call = Manager::instance().callFactory.newCall<SIPCall, JamiAccount>(
                shared(),
                Manager::instance().getNewCallID(),
                Call::CallType::OUTGOING,
                call->getDetails());

            dev_call->setIPToIP(true);
            dev_call->setSecure(isTlsEnabled());
            dev_call->setState(Call::ConnectionState::TRYING);
            call->addStateListener(
                [w = weak(), deviceId](Call::CallState, Call::ConnectionState state, int) {
                    if (state != Call::ConnectionState::PROGRESSING
                        and state != Call::ConnectionState::TRYING) {
                        if (auto shared = w.lock())
                            shared->callConnectionClosed(deviceId, true);
                    }
                });
            call->addSubCall(*dev_call);
            {
                std::lock_guard<std::mutex> lk(pendingCallsMutex_);
                pendingCalls_[deviceId].emplace_back(dev_call);
            }

            JAMI_WARN("[call %s] No channeled socket with this peer. Send request",
                      call->getCallId().c_str());
            // Else, ask for a channel (for future calls/text messages)
            requestSIPConnection(toUri, deviceId);
        };

    for (auto it = sipConns_.begin(); it != sipConns_.end(); ++it) {
        auto& [key, value] = *it;
        if (key.first != toUri)
            continue;
        if (value.empty())
            continue;
        auto& sipConn = value.back();

        auto transport = sipConn.transport;
        if (!sipConn.channel->underlyingICE()) {
            JAMI_WARN("A SIP transport exists without Channel, this is a bug. Please report");
            continue;
        }
        if (!transport)
            continue;

        JAMI_WARN("[call %s] A channeled socket is detected with this peer.",
                  call->getCallId().c_str());

        auto dev_call = manager.callFactory.newCall<SIPCall, JamiAccount>(shared(),
                                                                          manager.getNewCallID(),
                                                                          Call::CallType::OUTGOING,
                                                                          call->getDetails());
        dev_call->setIPToIP(true);
        dev_call->setSecure(isTlsEnabled());
        dev_call->setTransport(transport);
        call->addSubCall(*dev_call);
        // Set the call in PROGRESSING State because the ICE session
        // is already ready. Note that this line should be after
        // addSubcall() to change the state of the main call
        // and avoid to get an active call in a TRYING state.
        dev_call->setState(Call::ConnectionState::PROGRESSING);

        {
            std::lock_guard<std::mutex> lk(onConnectionClosedMtx_);
            onConnectionClosed_[key.second] = sendRequest;
        }

        call->addStateListener(
            [w = weak(), deviceId = key.second](Call::CallState, Call::ConnectionState state, int) {
                if (state != Call::ConnectionState::PROGRESSING
                    and state != Call::ConnectionState::TRYING) {
                    if (auto shared = w.lock())
                        shared->callConnectionClosed(deviceId, true);
                }
            });

        auto remoted_address = sipConn.channel->underlyingICE()->getRemoteAddress(
            ICE_COMP_SIP_TRANSPORT);
        try {
            onConnectedOutgoingCall(dev_call, toUri, remoted_address);
        } catch (const VoipLinkException&) {
            // In this case, the main scenario is that SIPStartCall failed because
            // the ICE is dead and the TLS session didn't send any packet on that dead
            // link (connectivity change, killed by the os, etc)
            // Here, we don't need to do anything, the TLS will fail and will delete
            // the cached transport
            continue;
        }
        devices.emplace(key.second);
    }

    // Find listening devices for this account
    accountManager_->forEachDevice(
        peer_account,
        [this, devices = std::move(devices), sendRequest](const DeviceId& dev) {
            // Test if already sent via a SIP transport
            if (devices.find(dev) != devices.end())
                return;
            {
                std::lock_guard<std::mutex> lk(onConnectionClosedMtx_);
                onConnectionClosed_[dev] = sendRequest;
            }
            sendRequest(dev, false);
        },
        [wCall](bool ok) {
            if (not ok) {
                if (auto call = wCall.lock()) {
                    JAMI_WARN("[call:%s] no devices found", call->getCallId().c_str());
                    call->onFailure(static_cast<int>(std::errc::no_such_device_or_address));
                }
            }
        });
}

void
JamiAccount::onConnectedOutgoingCall(const std::shared_ptr<SIPCall>& call,
                                     const std::string& to_id,
                                     IpAddr target)
{
    if (!call)
        return;
    JAMI_DBG("[call:%s] outgoing call connected to %s", call->getCallId().c_str(), to_id.c_str());

    auto opts = getIceOptions();
    opts.onInitDone = [w = weak(), target = std::move(target), call](bool ok) {
        if (!ok) {
            JAMI_ERR("ICE medias are not initialized");
            return;
        }
        auto shared = w.lock();
        if (!shared or !call)
            return;

        const auto localAddress = ip_utils::getInterfaceAddr(shared->getLocalInterface(),
                                                             target.getFamily());

        IpAddr addrSdp;
        if (shared->getUPnPActive()) {
            // use UPnP addr, or published addr if its set
            addrSdp = shared->getPublishedSameasLocal() ? shared->getUPnPIpAddress()
                                                        : shared->getPublishedIpAddress();
        } else {
            addrSdp = shared->isStunEnabled() or (not shared->getPublishedSameasLocal())
                          ? shared->getPublishedIpAddress()
                          : localAddress;
        }

        // fallback on local address
        if (not addrSdp)
            addrSdp = localAddress;

        // Initialize the session using ULAW as default codec in case of early media
        // The session should be ready to receive media once the first INVITE is sent, before
        // the session initialization is completed
        if (!getSystemCodecContainer()->searchCodecByName("PCMA", jami::MEDIA_AUDIO))
            JAMI_WARN("Could not instantiate codec for early media");

        // Building the local SDP offer
        auto& sdp = call->getSDP();

        sdp.setPublishedIP(addrSdp);
        const bool created = sdp.createOffer(shared->getActiveAccountCodecInfoList(MEDIA_AUDIO),
                                             shared->getActiveAccountCodecInfoList(
                                                 shared->videoEnabled_ and not call->isAudioOnly()
                                                     ? MEDIA_VIDEO
                                                     : MEDIA_NONE),
                                             shared->getSrtpKeyExchange());
        if (not created) {
            JAMI_ERR("Could not send outgoing INVITE request for new call");
            return;
        }
        // Note: pj_ice_strans_create can call onComplete in the same thread
        // This means that iceMutex_ in IceTransport can be locked when onInitDone is called
        // So, we need to run the call creation in the main thread
        // Also, we do not directly call SIPStartCall before receiving onInitDone, because
        // there is an inside waitForInitialization that can block the thread.
        runOnMainThread([w = std::move(w), target = std::move(target), call = std::move(call)] {
            auto shared = w.lock();
            if (!shared)
                return;

            if (not shared->SIPStartCall(*call, target)) {
                JAMI_ERR("Could not send outgoing INVITE request for new call");
            }
        });
    };
    call->setIPToIP(true);
    call->setPeerNumber(to_id);
    call->initIceMediaTransport(true, std::move(opts));
}

std::shared_ptr<Call>
JamiAccount::newOutgoingCall(std::string_view toUrl,
                             const std::map<std::string, std::string>& volatileCallDetails)
{
    return newOutgoingCall<SIPCall>(toUrl, volatileCallDetails);
}

bool
JamiAccount::SIPStartCall(SIPCall& call, IpAddr target)
{
    call.setupLocalSDPFromIce();
    std::string toUri(getToUri(call.getPeerNumber() + "@"
                               + target.toString(true))); // expecting a fully well formed sip uri

    pj_str_t pjTo = sip_utils::CONST_PJ_STR(toUri);

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = sip_utils::CONST_PJ_STR(from);

    std::string targetStr = getToUri(target.toString(true) /*+";transport=ICE"*/);
    pj_str_t pjTarget = sip_utils::CONST_PJ_STR(targetStr);

    pj_str_t pjContact;
    {
        auto transport = call.getTransport();
        pjContact = getContactHeader(transport ? transport->get() : nullptr);
    }

    JAMI_DBG("contact header: %.*s / %s -> %s / %.*s",
             (int) pjContact.slen,
             pjContact.ptr,
             from.c_str(),
             toUri.c_str(),
             (int) pjTarget.slen,
             pjTarget.ptr);

    auto local_sdp = call.getSDP().getLocalSdpSession();
    pjsip_dialog* dialog {nullptr};
    pjsip_inv_session* inv {nullptr};
    if (!CreateClientDialogAndInvite(&pjFrom, &pjContact, &pjTo, &pjTarget, local_sdp, &dialog, &inv))
        return false;

    inv->mod_data[link_.getModId()] = &call;
    call.inv.reset(inv);

    /*
        updateDialogViaSentBy(dialog);
        if (hasServiceRoute())
            pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(),
       call->inv->pool));
    */

    pjsip_tx_data* tdata;

    if (pjsip_inv_invite(call.inv.get(), &tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not initialize invite messager for this call");
        return false;
    }

    pjsip_tpselector tp_sel;
    tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
    if (!call.getTransport()) {
        JAMI_ERR("Could not get transport for this call");
        return false;
    }
    tp_sel.u.transport = call.getTransport()->get();
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        JAMI_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    JAMI_DBG("[call:%s] Sending SIP invite", call.getCallId().c_str());

    // Add user-agent header
    sip_utils::addUserAgenttHeader(getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(call.inv.get(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("Unable to send invite message for this call");
        return false;
    }

    call.setState(Call::CallState::ACTIVE, Call::ConnectionState::PROGRESSING);

    return true;
}

void
JamiAccount::saveConfig() const
{
    try {
        YAML::Emitter accountOut;
        serialize(accountOut);
        auto accountConfig = getPath() + DIR_SEPARATOR_STR + "config.yml";

        std::lock_guard<std::mutex> lock(fileutils::getFileLock(accountConfig));
        std::ofstream fout = fileutils::ofstream(accountConfig);
        fout << accountOut.c_str();
        JAMI_DBG("Exported account to %s", accountConfig.c_str());
    } catch (const std::exception& e) {
        JAMI_ERR("Error exporting account: %s", e.what());
    }
}

void
JamiAccount::serialize(YAML::Emitter& out) const
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
    out << YAML::Key << DRing::Account::ConfProperties::DHT_PEER_DISCOVERY << YAML::Value
        << dhtPeerDiscovery_;
    out << YAML::Key << DRing::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY << YAML::Value
        << accountPeerDiscovery_;
    out << YAML::Key << DRing::Account::ConfProperties::ACCOUNT_PUBLISH << YAML::Value
        << accountPublish_;

    out << YAML::Key << Conf::PROXY_ENABLED_KEY << YAML::Value << proxyEnabled_;
    out << YAML::Key << Conf::PROXY_SERVER_KEY << YAML::Value << proxyServer_;
    out << YAML::Key << Conf::PROXY_PUSH_TOKEN_KEY << YAML::Value << deviceKey_;
    out << YAML::Key << DRing::Account::ConfProperties::DHT_PROXY_LIST_URL << YAML::Value
        << proxyListUrl_;

#if HAVE_RINGNS
    out << YAML::Key << DRing::Account::ConfProperties::RingNS::URI << YAML::Value << nameServer_;
    if (not registeredName_.empty())
        out << YAML::Key << DRing::Account::VolatileProperties::REGISTERED_NAME << YAML::Value
            << registeredName_;
#endif

    out << YAML::Key << DRing::Account::ConfProperties::ARCHIVE_PATH << YAML::Value << archivePath_;
    out << YAML::Key << DRing::Account::ConfProperties::ARCHIVE_HAS_PASSWORD << YAML::Value
        << archiveHasPassword_;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT << YAML::Value << receipt_;
    if (receiptSignature_.size() > 0)
        out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT_SIG << YAML::Value
            << YAML::Binary(receiptSignature_.data(), receiptSignature_.size());
    out << YAML::Key << DRing::Account::ConfProperties::RING_DEVICE_NAME << YAML::Value
        << ringDeviceName_;
    out << YAML::Key << DRing::Account::ConfProperties::MANAGER_URI << YAML::Value << managerUri_;
    out << YAML::Key << DRing::Account::ConfProperties::MANAGER_USERNAME << YAML::Value
        << managerUsername_;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void
JamiAccount::unserialize(const YAML::Node& node)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);

    using yaml_utils::parseValue;
    using yaml_utils::parseValueOptional;
    using yaml_utils::parsePath;

    SIPAccountBase::unserialize(node);

    // get tls submap
    const auto& tlsMap = node[Conf::TLS_KEY];
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
        parseValue(node, DRing::Account::ConfProperties::DHT_PROXY_LIST_URL, proxyListUrl_);
    } catch (const std::exception& e) {
        proxyListUrl_ = DHT_DEFAULT_PROXY_LIST_URL;
    }

    parseValueOptional(node, DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);
    parseValueOptional(node, DRing::Account::ConfProperties::MANAGER_URI, managerUri_);
    parseValueOptional(node, DRing::Account::ConfProperties::MANAGER_USERNAME, managerUsername_);

    try {
        parsePath(node, DRing::Account::ConfProperties::ARCHIVE_PATH, archivePath_, idPath_);
        parseValue(node, DRing::Account::ConfProperties::ARCHIVE_HAS_PASSWORD, archiveHasPassword_);
    } catch (const std::exception& e) {
        JAMI_WARN("can't read archive path: %s", e.what());
        archiveHasPassword_ = true;
    }

    try {
        parseValue(node, Conf::RING_ACCOUNT_RECEIPT, receipt_);
        auto receipt_sig = node[Conf::RING_ACCOUNT_RECEIPT_SIG].as<YAML::Binary>();
        receiptSignature_ = {receipt_sig.data(), receipt_sig.data() + receipt_sig.size()};
    } catch (const std::exception& e) {
        JAMI_WARN("can't read receipt: %s", e.what());
    }

    // HACK
    // MacOS doesn't seems to close the DHT port sometimes, so re-using the DHT port seems
    // to make the DHT unusable (Address already in use, and SO_REUSEADDR & SO_REUSEPORT
    // doesn't seems to work). For now, use a random port
    // See https://git.jami.net/savoirfairelinux/ring-client-macosx/issues/221
    // TODO: parseValueOptional(node, Conf::DHT_PORT_KEY, dhtPort_);
    if (not dhtPort_)
        dhtPort_ = getRandomEvenPort(DHT_PORT_RANGE);
    dhtPortUsed_ = dhtPort_;

    parseValueOptional(node, DRing::Account::ConfProperties::DHT_PEER_DISCOVERY, dhtPeerDiscovery_);
    parseValueOptional(node,
                       DRing::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY,
                       accountPeerDiscovery_);
    parseValueOptional(node, DRing::Account::ConfProperties::ACCOUNT_PUBLISH, accountPublish_);

#if HAVE_RINGNS
    parseValueOptional(node, DRing::Account::ConfProperties::RingNS::URI, nameServer_);
    if (registeredName_.empty()) {
        parseValueOptional(node,
                           DRing::Account::VolatileProperties::REGISTERED_NAME,
                           registeredName_);
    }
#endif

    parseValue(node, Conf::DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);

    loadAccount();
}

bool
JamiAccount::changeArchivePassword(const std::string& password_old, const std::string& password_new)
{
    try {
        if (!accountManager_->changePassword(password_old, password_new)) {
            JAMI_ERR("[Account %s] Can't change archive password", getAccountID().c_str());
            return false;
        }
        archiveHasPassword_ = not password_new.empty();
    } catch (const std::exception& ex) {
        JAMI_ERR("[Account %s] Can't change archive password: %s",
                 getAccountID().c_str(),
                 ex.what());
        if (password_old.empty()) {
            archiveHasPassword_ = true;
            emitSignal<DRing::ConfigurationSignal::AccountDetailsChanged>(getAccountID(),
                                                                          getAccountDetails());
        }
        return false;
    }
    if (password_old != password_new)
        emitSignal<DRing::ConfigurationSignal::AccountDetailsChanged>(getAccountID(),
                                                                      getAccountDetails());
    return true;
}

bool
JamiAccount::isPasswordValid(const std::string& password)
{
    return accountManager_ and accountManager_->isPasswordValid(password);
}

void
JamiAccount::addDevice(const std::string& password)
{
    if (not accountManager_) {
        emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 2, "");
        return;
    }
    accountManager_
        ->addDevice(password, [this](AccountManager::AddDeviceResult result, std::string pin) {
            switch (result) {
            case AccountManager::AddDeviceResult::SUCCESS_SHOW_PIN:
                emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 0, pin);
                break;
            case AccountManager::AddDeviceResult::ERROR_CREDENTIALS:
                emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 1, "");
                break;
            case AccountManager::AddDeviceResult::ERROR_NETWORK:
                emitSignal<DRing::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 2, "");
                break;
            }
        });
}

bool
JamiAccount::exportArchive(const std::string& destinationPath, const std::string& password)
{
    if (auto manager = dynamic_cast<ArchiveAccountManager*>(accountManager_.get())) {
        return manager->exportArchive(destinationPath, password);
    }
    return false;
}

bool
JamiAccount::revokeDevice(const std::string& password, const std::string& device)
{
    if (not accountManager_)
        return false;
    return accountManager_
        ->revokeDevice(password, device, [this, device](AccountManager::RevokeDeviceResult result) {
            switch (result) {
            case AccountManager::RevokeDeviceResult::SUCCESS:
                emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(),
                                                                              device,
                                                                              0);
                break;
            case AccountManager::RevokeDeviceResult::ERROR_CREDENTIALS:
                emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(),
                                                                              device,
                                                                              1);
                break;
            case AccountManager::RevokeDeviceResult::ERROR_NETWORK:
                emitSignal<DRing::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(),
                                                                              device,
                                                                              2);
                break;
            }
        });
    return true;
}

std::pair<std::string, std::string>
JamiAccount::saveIdentity(const dht::crypto::Identity id,
                          const std::string& path,
                          const std::string& name)
{
    auto names = std::make_pair(name + ".key", name + ".crt");
    if (id.first)
        fileutils::saveFile(path + DIR_SEPARATOR_STR + names.first, id.first->serialize(), 0600);
    if (id.second)
        fileutils::saveFile(path + DIR_SEPARATOR_STR + names.second, id.second->getPacked(), 0600);
    return names;
}

// must be called while configurationMutex_ is locked
void
JamiAccount::loadAccount(const std::string& archive_password,
                         const std::string& archive_pin,
                         const std::string& archive_path)
{
    if (registrationState_ == RegistrationState::INITIALIZING)
        return;

    JAMI_DBG("[Account %s] loading account", getAccountID().c_str());
    AccountManager::OnChangeCallback callbacks {
        [this](const std::string& uri, bool confirmed) {
            dht::ThreadPool::computation().run([id = getAccountID(), uri, confirmed] {
                emitSignal<DRing::ConfigurationSignal::ContactAdded>(id, uri, confirmed);
            });
        },
        [this](const std::string& uri, bool banned) {
            dht::ThreadPool::computation().run([id = getAccountID(), uri, banned] {
                emitSignal<DRing::ConfigurationSignal::ContactRemoved>(id, uri, banned);
            });
        },
        [this](const std::string& uri, const std::vector<uint8_t>& payload, time_t received) {
            dht::ThreadPool::computation().run(
                [id = getAccountID(), uri, payload = std::move(payload), received] {
                    emitSignal<DRing::ConfigurationSignal::IncomingTrustRequest>(id,
                                                                                 uri,
                                                                                 payload,
                                                                                 received);
                });
        },
        [this](const std::map<dht::InfoHash, KnownDevice>& devices) {
            std::map<std::string, std::string> ids;
            for (auto& d : devices) {
                auto id = d.first.toString();
                auto label = d.second.name.empty() ? id.substr(0, 8) : d.second.name;
                ids.emplace(std::move(id), std::move(label));
            }
            dht::ThreadPool::computation().run([id = getAccountID(), devices = std::move(ids)] {
                emitSignal<DRing::ConfigurationSignal::KnownDevicesChanged>(id, devices);
            });
        }};

    try {
        auto onAsync = [w = weak()](AccountManager::AsyncUser&& cb) {
            if (auto this_ = w.lock())
                cb(*this_->accountManager_);
        };
        if (managerUri_.empty()) {
            accountManager_.reset(new ArchiveAccountManager(
                getPath(),
                onAsync,
                [this]() { return getAccountDetails(); },
                archivePath_.empty() ? "archive.gz" : archivePath_,
                nameServer_));
        } else {
            accountManager_.reset(
                new ServerAccountManager(getPath(), onAsync, managerUri_, nameServer_));
        }

        auto id = accountManager_->loadIdentity(tlsCertificateFile_,
                                                tlsPrivateKeyFile_,
                                                tlsPassword_);
        if (auto info = accountManager_->useIdentity(id,
                                                     receipt_,
                                                     receiptSignature_,
                                                     managerUsername_,
                                                     std::move(callbacks))) {
            // normal loading path
            id_ = std::move(id);
            username_ = info->accountId;
            JAMI_WARN("[Account %s] loaded account identity", getAccountID().c_str());
            if (not isEnabled()) {
                setRegistrationState(RegistrationState::UNREGISTERED);
            }
        } else if (isEnabled()) {
            if (not managerUri_.empty() and archive_password.empty()) {
                Migration::setState(accountID_, Migration::State::INVALID);
                setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
                return;
            }

            bool migrating = registrationState_ == RegistrationState::ERROR_NEED_MIGRATION;
            setRegistrationState(RegistrationState::INITIALIZING);
            auto fDeviceKey = dht::ThreadPool::computation()
                                  .getShared<std::shared_ptr<dht::crypto::PrivateKey>>([]() {
                                      return std::make_shared<dht::crypto::PrivateKey>(
                                          dht::crypto::PrivateKey::generate());
                                  });

            std::unique_ptr<AccountManager::AccountCredentials> creds;
            if (managerUri_.empty()) {
                auto acreds = std::make_unique<ArchiveAccountManager::ArchiveAccountCredentials>();
                if (archivePath_.empty()) {
                    archivePath_ = "archive.gz";
                }
                auto archivePath = fileutils::getFullPath(idPath_, archivePath_);
                bool hasArchive = fileutils::isFile(archivePath);
                if (not archive_path.empty()) {
                    // Importing external archive
                    acreds->scheme = "file";
                    acreds->uri = archive_path;
                } else if (not archive_pin.empty()) {
                    // Importing from DHT
                    acreds->scheme = "dht";
                    acreds->uri = archive_pin;
                    acreds->dhtBootstrap = loadBootstrap();
                    acreds->dhtPort = (in_port_t) dhtPortUsed_;
                } else if (hasArchive) {
                    // Migrating local account
                    acreds->scheme = "local";
                    acreds->uri = std::move(archivePath);
                    acreds->updateIdentity = id;
                    migrating = true;
                }
                creds = std::move(acreds);
            } else {
                auto screds = std::make_unique<ServerAccountManager::ServerAccountCredentials>();
                screds->username = managerUsername_;
                creds = std::move(screds);
            }
            creds->password = archive_password;
            archiveHasPassword_ = !archive_password.empty();

            accountManager_->initAuthentication(
                fDeviceKey,
                ip_utils::getDeviceName(),
                std::move(creds),
                [this, fDeviceKey, migrating](const AccountInfo& info,
                                              const std::map<std::string, std::string>& config,
                                              std::string&& receipt,
                                              std::vector<uint8_t>&& receipt_signature) {
                    JAMI_WARN("[Account %s] Auth success !", getAccountID().c_str());

                    fileutils::check_dir(idPath_.c_str(), 0700);

                    // save the chain including CA
                    auto id = info.identity;
                    id.first = std::move(fDeviceKey.get());
                    std::tie(tlsPrivateKeyFile_, tlsCertificateFile_) = saveIdentity(id,
                                                                                     idPath_,
                                                                                     "ring_device");
                    id_ = std::move(id);
                    tlsPassword_ = {};

                    username_ = info.accountId;
                    registeredName_ = managerUsername_;
                    ringDeviceName_ = accountManager_->getAccountDeviceName();

                    auto nameServerIt = config.find(DRing::Account::ConfProperties::RingNS::URI);
                    if (nameServerIt != config.end() && !nameServerIt->second.empty()) {
                        nameServer_ = nameServerIt->second;
                    }
                    auto displayNameIt = config.find(DRing::Account::ConfProperties::DISPLAYNAME);
                    if (displayNameIt != config.end() && !displayNameIt->second.empty()) {
                        displayName_ = displayNameIt->second;
                    }

                    receipt_ = std::move(receipt);
                    receiptSignature_ = std::move(receipt_signature);
                    if (migrating) {
                        Migration::setState(getAccountID(), Migration::State::SUCCESS);
                    }

                    // Use the provided config by JAMS instead of default one
                    auto details = getAccountDetails();
                    for (const auto& [key, value] : config)
                        details[key] = value;
                    setAccountDetails(details);

                    if (not info.photo.empty() or not displayName_.empty())
                        emitSignal<DRing::ConfigurationSignal::AccountProfileReceived>(getAccountID(),
                                                                                       displayName_,
                                                                                       info.photo);
                    setRegistrationState(RegistrationState::UNREGISTERED);
                    saveConfig();
                    doRegister();
                },
                [w = weak(),
                 id,
                 accountId = getAccountID(),
                 migrating](AccountManager::AuthError error, const std::string& message) {
                    JAMI_WARN("[Account %s] Auth error: %d %s",
                              accountId.c_str(),
                              (int) error,
                              message.c_str());
                    if ((id.first || migrating)
                        && error == AccountManager::AuthError::INVALID_ARGUMENTS) {
                        // In cast of a migration or manager connexion failure stop the migration
                        // and block the account
                        Migration::setState(accountId, Migration::State::INVALID);
                        if (auto acc = w.lock())
                            acc->setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
                    } else {
                        // In case of a DHT or backup import failure, just remove the account
                        if (auto acc = w.lock())
                            acc->setRegistrationState(RegistrationState::ERROR_GENERIC);
                        runOnMainThread([accountId = std::move(accountId)] {
                            Manager::instance().removeAccount(accountId, true);
                        });
                    }
                },
                std::move(callbacks));
        }
    } catch (const std::exception& e) {
        JAMI_WARN("[Account %s] error loading account: %s", getAccountID().c_str(), e.what());
        accountManager_.reset();
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
JamiAccount::setAccountDetails(const std::map<std::string, std::string>& details)
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
    parseString(details, DRing::Account::ConfProperties::BOOTSTRAP_LIST_URL, bootstrapListUrl_);
    parseInt(details, Conf::CONFIG_DHT_PORT, dhtPort_);
    parseBool(details, Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);
    parseBool(details, DRing::Account::ConfProperties::DHT_PEER_DISCOVERY, dhtPeerDiscovery_);
    parseBool(details,
              DRing::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY,
              accountPeerDiscovery_);
    parseBool(details, DRing::Account::ConfProperties::ACCOUNT_PUBLISH, accountPublish_);
    parseBool(details,
              DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY,
              allowPeersFromHistory_);
    parseBool(details,
              DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT,
              allowPeersFromContact_);
    parseBool(details,
              DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED,
              allowPeersFromTrusted_);
    if (not dhtPort_)
        dhtPort_ = getRandomEvenPort(DHT_PORT_RANGE);
    dhtPortUsed_ = dhtPort_;

    parseString(details, DRing::Account::ConfProperties::MANAGER_URI, managerUri_);
    parseString(details, DRing::Account::ConfProperties::MANAGER_USERNAME, managerUsername_);
    parseString(details, DRing::Account::ConfProperties::USERNAME, username_);

    std::string archive_password;
    std::string archive_pin;
    std::string archive_path;
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PASSWORD, archive_password);
    parseString(details, DRing::Account::ConfProperties::ARCHIVE_PIN, archive_pin);
    std::transform(archive_pin.begin(), archive_pin.end(), archive_pin.begin(), ::toupper);
    parsePath(details, DRing::Account::ConfProperties::ARCHIVE_PATH, archive_path, idPath_);
    parseString(details, DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);

    auto oldProxyServer = proxyServer_, oldProxyServerList = proxyListUrl_;
    parseString(details, DRing::Account::ConfProperties::DHT_PROXY_LIST_URL, proxyListUrl_);
    parseBool(details, DRing::Account::ConfProperties::PROXY_ENABLED, proxyEnabled_);
    parseString(details, DRing::Account::ConfProperties::PROXY_SERVER, proxyServer_);
    parseString(details, DRing::Account::ConfProperties::PROXY_PUSH_TOKEN, deviceKey_);
    // Migrate from old versions
    if (proxyServer_.empty()
        || ((proxyServer_ == "dhtproxy.jami.net" || proxyServer_ == "dhtproxy.ring.cx")
            && proxyServerCached_.empty()))
        proxyServer_ = DHT_DEFAULT_PROXY;
    if (proxyServer_ != oldProxyServer || oldProxyServerList != proxyListUrl_) {
        JAMI_DBG("DHT Proxy configuration changed, resetting cache");
        proxyServerCached_ = {};
        auto proxyCachePath = cachePath_ + DIR_SEPARATOR_STR "dhtproxy";
        auto proxyListCachePath = cachePath_ + DIR_SEPARATOR_STR "dhtproxylist";
        std::remove(proxyCachePath.c_str());
        std::remove(proxyListCachePath.c_str());
    }
    if (not managerUri_.empty() and managerUri_.rfind("http", 0) != 0) {
        managerUri_ = "https://" + managerUri_;
    }

#if HAVE_RINGNS
    parseString(details, DRing::Account::ConfProperties::RingNS::URI, nameServer_);
#endif

    loadAccount(archive_password, archive_pin, archive_path);

    // update device name if necessary
    if (accountManager_)
        accountManager_->setAccountDeviceName(ringDeviceName_);
}

std::map<std::string, std::string>
JamiAccount::getAccountDetails() const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    a.emplace(Conf::CONFIG_DHT_PORT, std::to_string(dhtPort_));
    a.emplace(Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::DHT_PEER_DISCOVERY,
              dhtPeerDiscovery_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY,
              accountPeerDiscovery_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ACCOUNT_PUBLISH,
              accountPublish_ ? TRUE_STR : FALSE_STR);
    if (accountManager_) {
        if (auto info = accountManager_->getInfo()) {
            a.emplace(DRing::Account::ConfProperties::RING_DEVICE_ID, info->deviceId);
            a.emplace(DRing::Account::ConfProperties::RingNS::ACCOUNT, info->ethAccount);
        }
    }
    a.emplace(DRing::Account::ConfProperties::RING_DEVICE_NAME, ringDeviceName_);
    a.emplace(DRing::Account::ConfProperties::Presence::SUPPORT_SUBSCRIBE, TRUE_STR);
    if (not archivePath_.empty() or not managerUri_.empty())
        a.emplace(DRing::Account::ConfProperties::ARCHIVE_HAS_PASSWORD,
                  archiveHasPassword_ ? TRUE_STR : FALSE_STR);

    /* these settings cannot be changed (read only), but clients should still be
     * able to read what they are */
    a.emplace(Conf::CONFIG_SRTP_KEY_EXCHANGE, sip_utils::getKeyExchangeName(getSrtpKeyExchange()));
    a.emplace(Conf::CONFIG_SRTP_ENABLE, isSrtpEnabled() ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_SRTP_RTP_FALLBACK, getSrtpFallback() ? TRUE_STR : FALSE_STR);

    a.emplace(Conf::CONFIG_TLS_CA_LIST_FILE, fileutils::getFullPath(idPath_, tlsCaListFile_));
    a.emplace(Conf::CONFIG_TLS_CERTIFICATE_FILE,
              fileutils::getFullPath(idPath_, tlsCertificateFile_));
    a.emplace(Conf::CONFIG_TLS_PRIVATE_KEY_FILE,
              fileutils::getFullPath(idPath_, tlsPrivateKeyFile_));
    a.emplace(Conf::CONFIG_TLS_PASSWORD, tlsPassword_);
    a.emplace(Conf::CONFIG_TLS_METHOD, "Automatic");
    a.emplace(Conf::CONFIG_TLS_CIPHERS, "");
    a.emplace(Conf::CONFIG_TLS_SERVER_NAME, "");
    a.emplace(Conf::CONFIG_TLS_VERIFY_SERVER, TRUE_STR);
    a.emplace(Conf::CONFIG_TLS_VERIFY_CLIENT, TRUE_STR);
    a.emplace(Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, TRUE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY,
              allowPeersFromHistory_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT,
              allowPeersFromContact_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED,
              allowPeersFromTrusted_ ? TRUE_STR : FALSE_STR);
    /* GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT is defined as -1 */
    a.emplace(Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, "-1");
    a.emplace(DRing::Account::ConfProperties::PROXY_ENABLED, proxyEnabled_ ? TRUE_STR : FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::PROXY_SERVER, proxyServer_);
    a.emplace(DRing::Account::ConfProperties::DHT_PROXY_LIST_URL, proxyListUrl_);
    a.emplace(DRing::Account::ConfProperties::PROXY_PUSH_TOKEN, deviceKey_);
    a.emplace(DRing::Account::ConfProperties::MANAGER_URI, managerUri_);
    a.emplace(DRing::Account::ConfProperties::MANAGER_USERNAME, managerUsername_);
#if HAVE_RINGNS
    a.emplace(DRing::Account::ConfProperties::RingNS::URI, nameServer_);
#endif

    return a;
}

std::map<std::string, std::string>
JamiAccount::getVolatileAccountDetails() const
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
JamiAccount::lookupName(const std::string& name)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->lookupUri(name,
                                   nameServer_,
                                   [acc = getAccountID(), name](const std::string& result,
                                                                NameDirectory::Response response) {
                                       emitSignal<DRing::ConfigurationSignal::RegisteredNameFound>(
                                           acc, (int) response, result, name);
                                   });
}

void
JamiAccount::lookupAddress(const std::string& addr)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    auto acc = getAccountID();
    if (accountManager_)
        accountManager_->lookupAddress(
            addr, [acc, addr](const std::string& result, NameDirectory::Response response) {
                emitSignal<DRing::ConfigurationSignal::RegisteredNameFound>(acc,
                                                                            (int) response,
                                                                            addr,
                                                                            result);
            });
}

void
JamiAccount::registerName(const std::string& password, const std::string& name)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->registerName(
            password,
            name,
            [acc = getAccountID(), name, w = weak()](NameDirectory::RegistrationResponse response) {
                int res
                    = (response == NameDirectory::RegistrationResponse::success)
                          ? 0
                          : ((response == NameDirectory::RegistrationResponse::invalidCredentials)
                                 ? 1
                                 : ((response == NameDirectory::RegistrationResponse::invalidName)
                                        ? 2
                                        : ((response
                                            == NameDirectory::RegistrationResponse::alreadyTaken)
                                               ? 3
                                               : 4)));
                if (response == NameDirectory::RegistrationResponse::success) {
                    if (auto this_ = w.lock())
                        this_->registeredName_ = name;
                }
                emitSignal<DRing::ConfigurationSignal::NameRegistrationEnded>(acc, res, name);
            });
}
#endif

bool
JamiAccount::searchUser(const std::string& query)
{
    if (accountManager_)
        return accountManager_->searchUser(
            query,
            [acc = getAccountID(), query](const jami::NameDirectory::SearchResult& result,
                                          jami::NameDirectory::Response response) {
                jami::emitSignal<DRing::ConfigurationSignal::UserSearchEnded>(acc,
                                                                              (int) response,
                                                                              query,
                                                                              result);
            });
    return false;
}

void
JamiAccount::checkPendingCall(const std::string& callId)
{
    // Note only one check at a time. In fact, the UDP and TCP negotiation
    // can finish at the same time and we need to avoid potential race conditions.
    std::lock_guard<std::mutex> lk(callsMutex_);
    auto it = pendingCallsDht_.find(callId);
    if (it == pendingCallsDht_.end())
        return;

    bool incoming = !it->second.call_key;
    bool handled;
    try {
        handled = handlePendingCall(it->second, incoming);
    } catch (const std::exception& e) {
        JAMI_ERR("[DHT] exception during pending call handling: %s", e.what());
        handled = true; // drop from pending list
    }

    if (handled) {
        if (not incoming) {
            // Cancel pending listen (outgoing call)
            dht_->cancelListen(it->second.call_key, std::move(it->second.listen_key));
        }
        pendingCallsDht_.erase(it);
    }
}

pj_status_t
JamiAccount::checkPeerTlsCertificate(dht::InfoHash from,
                                     dht::InfoHash from_account,
                                     unsigned status,
                                     const gnutls_datum_t* cert_list,
                                     unsigned cert_num,
                                     std::shared_ptr<dht::crypto::Certificate>& cert_out)
{
    if (cert_num == 0) {
        JAMI_ERR("[peer:%s] No certificate", from.toString().c_str());
        return PJ_SSL_CERT_EUNKNOWN;
    }
    if (status & GNUTLS_CERT_EXPIRED or status & GNUTLS_CERT_NOT_ACTIVATED) {
        JAMI_ERR("[peer:%s] Expired certificate", from.toString().c_str());
        return PJ_SSL_CERT_EVALIDITY_PERIOD;
    }

    // Unserialize certificate chain
    std::vector<std::pair<uint8_t*, uint8_t*>> crt_data;
    crt_data.reserve(cert_num);
    for (unsigned i = 0; i < cert_num; i++)
        crt_data.emplace_back(cert_list[i].data, cert_list[i].data + cert_list[i].size);
    auto crt = std::make_shared<dht::crypto::Certificate>(crt_data);

    // Check expected peer identity
    dht::InfoHash tls_account_id;
    if (not accountManager_->onPeerCertificate(crt, dhtPublicInCalls_, tls_account_id)) {
        JAMI_ERR("[peer:%s] Discarding message from invalid peer certificate.",
                 from.toString().c_str());
        return PJ_SSL_CERT_EUNKNOWN;
    }
    if (from_account != tls_account_id) {
        JAMI_ERR("[peer:%s] Discarding message from wrong peer account %s.",
                 from.toString().c_str(),
                 tls_account_id.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    const auto tls_id = crt->getId();
    if (crt->getUID() != tls_id.toString()) {
        JAMI_ERR("[peer:%s] Certificate UID must be the public key ID", from.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }
    if (tls_id != from) {
        JAMI_ERR("[peer:%s] Certificate public key ID doesn't match (%s)",
                 from.toString().c_str(),
                 tls_id.toString().c_str());
        return PJ_SSL_CERT_EUNTRUSTED;
    }

    JAMI_DBG("[peer:%s] Certificate verified", from.toString().c_str());
    cert_out = std::move(crt);
    return PJ_SUCCESS;
}

bool
JamiAccount::handlePendingCall(PendingCall& pc, bool incoming)
{
    auto call = pc.call.lock();
    // Cleanup pending call if call is over (cancelled by user or any other reason)
    if (not call || call->getState() == Call::CallState::OVER)
        return true;

    if ((std::chrono::steady_clock::now() - pc.start) >= ICE_NEGOTIATION_TIMEOUT) {
        JAMI_WARN("[call:%s] Timeout on ICE negotiation", call->getCallId().c_str());
        call->onFailure();
        return true;
    }

    auto ice_tcp = pc.ice_tcp_sp.get();
    auto ice = pc.ice_sp.get();

    bool tcp_finished = ice_tcp == nullptr || ice_tcp->isStopped();
    bool udp_finished = ice == nullptr || ice->isStopped();

    if (not udp_finished and ice->isFailed()) {
        udp_finished = true;
    }

    if (not tcp_finished and ice_tcp->isFailed()) {
        tcp_finished = true;
    }

    // At least wait for TCP
    if (not tcp_finished and not ice_tcp->isRunning()) {
        return false;
    } else if (tcp_finished and (not ice_tcp or not ice_tcp->isRunning())) {
        // If TCP is finished but not running, wait for UDP
        if (not udp_finished and ice and not ice->isRunning()) {
            return false;
        }
    }

    udp_finished = ice && ice->isRunning();
    if (udp_finished)
        JAMI_INFO("[call:%s] UDP negotiation is ready", call->getCallId().c_str());
    tcp_finished = ice_tcp && ice_tcp->isRunning();
    if (tcp_finished)
        JAMI_INFO("[call:%s] TCP negotiation is ready", call->getCallId().c_str());
    // If both transport are not running, the negotiation failed
    if (not udp_finished and not tcp_finished) {
        JAMI_ERR("[call:%s] Both ICE negotations failed", call->getCallId().c_str());
        call->onFailure();
        return true;
    }

    // Securize a SIP transport with TLS (on top of ICE tranport) and assign the call with it
    auto remote_device = pc.from;
    auto remote_account = pc.from_account;
    auto id = id_;
    if (not id.first or not id.second)
        throw std::runtime_error("No identity configured for this account.");

    std::weak_ptr<JamiAccount> waccount = weak();
    std::weak_ptr<SIPCall> wcall = call;
    tls::TlsParams tlsParams {
        /*.ca_list = */ "",
        /*.ca = */ pc.from_cert,
        /*.cert = */ id.second,
        /*.cert_key = */ id.first,
        /*.dh_params = */ dhParams_,
        /*.timeout = */ TLS_TIMEOUT,
        /*.cert_check = */
        [waccount, wcall, remote_device, remote_account](unsigned status,
                                                         const gnutls_datum_t* cert_list,
                                                         unsigned cert_num) -> pj_status_t {
            try {
                if (auto call = wcall.lock()) {
                    if (auto sthis = waccount.lock()) {
                        auto& this_ = *sthis;
                        std::shared_ptr<dht::crypto::Certificate> peer_cert;
                        auto ret = this_.checkPeerTlsCertificate(remote_device,
                                                                 remote_account,
                                                                 status,
                                                                 cert_list,
                                                                 cert_num,
                                                                 peer_cert);
                        if (ret == PJ_SUCCESS and peer_cert) {
                            std::lock_guard<std::mutex> lock(this_.callsMutex_);
                            for (auto& pscall : this_.pendingSipCalls_) {
                                if (auto pcall = pscall.call.lock()) {
                                    if (pcall == call and not pscall.from_cert) {
                                        JAMI_DBG(
                                            "[call:%s] got peer certificate from TLS negotiation",
                                            call->getCallId().c_str());
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
                JAMI_ERR("[peer:%s] TLS certificate check exception: %s",
                         remote_device.toString().c_str(),
                         e.what());
                return PJ_SSL_CERT_EUNKNOWN;
            }
        }};

    auto best_transport = pc.ice_tcp_sp;
    if (!tcp_finished) {
        JAMI_DBG("TCP not running, will use SIP over UDP");
        best_transport = pc.ice_sp;
        // Move the ice destruction in its own thread to avoid
        // slow operations on main thread
        dht::ThreadPool::io().run([ice = std::move(pc.ice_tcp_sp)] {});
    } else {
        dht::ThreadPool::io().run([ice = std::move(pc.ice_sp)] {});
    }

    // Following can create a transport that need to be negotiated (TLS).
    // This is a asynchronous task. So we're going to process the SIP after this negotiation.
    auto transport = link_.sipTransportBroker->getTlsIceTransport(best_transport,
                                                                  ICE_COMP_SIP_TRANSPORT,
                                                                  tlsParams);
    if (!transport)
        throw std::runtime_error("transport creation failed");

    call->setTransport(transport);

    if (incoming) {
        pendingSipCalls_.emplace_back(std::move(pc)); // copy of pc
    } else {
        // Be acknowledged on transport connection/disconnection
        auto lid = reinterpret_cast<uintptr_t>(this);
        auto remote_id = remote_device.toString();
        auto remote_addr = best_transport->getRemoteAddress(ICE_COMP_SIP_TRANSPORT);
        auto& tr_self = *transport;

        transport->addStateListener(lid,
                                    [&tr_self,
                                     lid,
                                     wcall,
                                     waccount,
                                     remote_id,
                                     remote_addr](pjsip_transport_state state,
                                                  UNUSED const pjsip_transport_state_info* info) {
                                        if (state == PJSIP_TP_STATE_CONNECTED) {
                                            if (auto call = wcall.lock()) {
                                                if (auto account = waccount.lock()) {
                                                    // Start SIP layer when TLS negotiation is successful
                                                    account->onConnectedOutgoingCall(call,
                                                                                     remote_id,
                                                                                     remote_addr);
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

void
JamiAccount::forEachPendingCall(const DeviceId& deviceId,
                                const std::function<void(const std::shared_ptr<SIPCall>&)>& cb)
{
    std::vector<std::shared_ptr<SIPCall>> pc;
    {
        std::lock_guard<std::mutex> lk(pendingCallsMutex_);
        pc = std::move(pendingCalls_[deviceId]);
    }
    for (const auto& pendingCall : pc) {
        cb(pendingCall);
    }
}

void
JamiAccount::registerAsyncOps()
{
    auto onLoad = [this, loaded = std::make_shared<std::atomic_uint>()] {
        if (++(*loaded) == 2u) {
            runOnMainThread([w = weak()] {
                if (auto s = w.lock()) {
                    std::lock_guard<std::mutex> lock(s->configurationMutex_);
                    s->doRegister_();
                }
            });
        }
    };

    loadCachedProxyServer([onLoad](const std::string&) { onLoad(); });

    if (upnp_) {
        upnp_->requestMappingAdd(
            [this, onLoad, update = std::make_shared<bool>(false)](uint16_t port_used,
                                                                   bool success) {
                auto oldPort = static_cast<in_port_t>(dhtPortUsed_);
                auto newPort = success ? port_used : dhtPort_;
                if (*update) {
                    if (oldPort != newPort or not dht_->isRunning()) {
                        JAMI_WARN("[Account %s] DHT port changed to %u: restarting network",
                                  getAccountID().c_str(),
                                  newPort);
                        dht_->connectivityChanged();
                    }
                } else {
                    *update = true;
                    if (success)
                        JAMI_WARN("[Account %s] Starting DHT on port %u",
                                  getAccountID().c_str(),
                                  newPort);
                    else
                        JAMI_WARN("[Account %s] Failed to open port %u: starting DHT anyways",
                                  getAccountID().c_str(),
                                  oldPort);
                    onLoad();
                }
            },
            dhtPort_,
            upnp::PortType::UDP,
            false);
    } else
        onLoad();
}

void
JamiAccount::doRegister()
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (not isUsable()) {
        JAMI_WARN("Account must be enabled and active to register, ignoring");
        return;
    }

    JAMI_DBG("[Account %s] Starting account..", getAccountID().c_str());

    // invalid state transitions:
    // INITIALIZING: generating/loading certificates, can't register
    // NEED_MIGRATION: old account detected, user needs to migrate
    if (registrationState_ == RegistrationState::INITIALIZING
        || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION)
        return;

    if (not dhParams_.valid()) {
        generateDhParams();
    }

    setRegistrationState(RegistrationState::TRYING);
    /* if UPnP is enabled, then wait for IGD to complete registration */
    if (upnp_ or proxyServerCached_.empty()) {
        JAMI_DBG("UPnP: Attempting to map ports for Jami account");
        registerAsyncOps();
    } else {
        doRegister_();
    }

    cacheTurnServers(); // reset cache for TURN servers
}

std::vector<std::string>
JamiAccount::loadBootstrap() const
{
    std::vector<std::string> bootstrap;
    if (!hostname_.empty()) {
        std::stringstream ss(hostname_);
        std::string node_addr;
        while (std::getline(ss, node_addr, ';'))
            bootstrap.emplace_back(std::move(node_addr));
        for (const auto& b : bootstrap)
            JAMI_DBG("Bootstrap node: %s", b.c_str());
    }
    return bootstrap;
}

void
JamiAccount::trackBuddyPresence(const std::string& buddy_id, bool track)
{
    std::string buddyUri;

    try {
        buddyUri = parseJamiUri(buddy_id);
    } catch (...) {
        JAMI_ERR("[Account %s] Failed to track a buddy due to an invalid URI %s",
                 getAccountID().c_str(),
                 buddy_id.c_str());
        return;
    }
    auto h = dht::InfoHash(buddyUri);

    if (!track && dht_ && dht_->isRunning()) {
        // Remove current connections with contact
        std::set<DeviceId> devices;
        {
            std::unique_lock<std::mutex> lk(sipConnsMtx_);
            for (auto it = sipConns_.begin(); it != sipConns_.end();) {
                const auto& [key, value] = *it;
                if (key.first == buddyUri) {
                    devices.emplace(key.second);
                    it = sipConns_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (const auto& device : devices) {
            if (connectionManager_)
                connectionManager_->closeConnectionsWith(device);
        }
    }

    std::lock_guard<std::mutex> lock(buddyInfoMtx);
    if (track) {
        auto buddy = trackedBuddies_.emplace(h, BuddyInfo {h});
        if (buddy.second) {
            trackPresence(buddy.first->first, buddy.first->second);
        }
    } else {
        auto buddy = trackedBuddies_.find(h);
        if (buddy != trackedBuddies_.end()) {
            if (auto dht = dht_)
                if (dht->isRunning())
                    dht->cancelListen(h, std::move(buddy->second.listenToken));
            trackedBuddies_.erase(buddy);
        }
    }
}

void
JamiAccount::trackPresence(const dht::InfoHash& h, BuddyInfo& buddy)
{
    auto dht = dht_;
    if (not dht or not dht->isRunning()) {
        return;
    }
    buddy.listenToken
        = dht->listen<DeviceAnnouncement>(h, [this, h](DeviceAnnouncement&&, bool expired) {
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
              if (not expired) {
                  // Retry messages every time a new device announce its presence
                  messageEngine_.onPeerOnline(h.toString());
              }
              if (isConnected and not wasConnected) {
                  onTrackedBuddyOnline(h);
              } else if (not isConnected and wasConnected) {
                  onTrackedBuddyOffline(h);
              }

              return true;
          });
    JAMI_DBG("[Account %s] tracking buddy %s", getAccountID().c_str(), h.to_c_str());
}

std::map<std::string, bool>
JamiAccount::getTrackedBuddyPresence() const
{
    std::lock_guard<std::mutex> lock(buddyInfoMtx);
    std::map<std::string, bool> presence_info;
    for (const auto& buddy_info_p : trackedBuddies_)
        presence_info.emplace(buddy_info_p.first.toString(), buddy_info_p.second.devices_cnt > 0);
    return presence_info;
}

void
JamiAccount::onTrackedBuddyOnline(const dht::InfoHash& contactId)
{
    JAMI_DBG("Buddy %s online", contactId.toString().c_str());
    std::string id(contactId.toString());
    emitSignal<DRing::PresenceSignal::NewBuddyNotification>(getAccountID(), id, 1, "");
}

void
JamiAccount::onTrackedBuddyOffline(const dht::InfoHash& contactId)
{
    JAMI_DBG("Buddy %s offline", contactId.toString().c_str());
    emitSignal<DRing::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                            contactId.toString(),
                                                            0,
                                                            "");
}

void
JamiAccount::doRegister_()
{
    if (registrationState_ != RegistrationState::TRYING) {
        JAMI_ERR("[Account %s] already registered", getAccountID().c_str());
        return;
    }

    JAMI_DBG("[Account %s] Starting account...", getAccountID().c_str());

    try {
        if (not accountManager_ or not accountManager_->getInfo())
            throw std::runtime_error("No identity configured for this account.");

        loadTreatedCalls();
        loadTreatedMessages();
        if (dht_->isRunning()) {
            JAMI_ERR("[Account %s] DHT already running (stopping it first).",
                     getAccountID().c_str());
            dht_->join();
        }

#if HAVE_RINGNS
        // Look for registered name on the blockchain
        accountManager_->lookupAddress(
            accountManager_->getInfo()->accountId,
            [w = weak()](const std::string& result, const NameDirectory::Response& response) {
                if (auto this_ = w.lock()) {
                    if (response == NameDirectory::Response::found) {
                        if (this_->registeredName_ != result) {
                            this_->registeredName_ = result;
                            emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(
                                this_->accountID_, this_->getVolatileAccountDetails());
                        }
                    } else if (response == NameDirectory::Response::notFound) {
                        if (not this_->registeredName_.empty()) {
                            this_->registeredName_.clear();
                            emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(
                                this_->accountID_, this_->getVolatileAccountDetails());
                        }
                    }
                }
            });
#endif

        dht::DhtRunner::Config config {};
        config.dht_config.node_config.network = 0;
        config.dht_config.node_config.maintain_storage = false;
        config.dht_config.node_config.persist_path = cachePath_ + DIR_SEPARATOR_STR "dhtstate";
        config.dht_config.id = id_;
        config.dht_config.cert_cache_all = true;
        config.push_node_id = getAccountID();
        config.push_token = deviceKey_;
        config.threaded = true;
        config.peer_discovery = dhtPeerDiscovery_;
        config.peer_publish = dhtPeerDiscovery_;
        if (proxyEnabled_)
            config.proxy_server = proxyServerCached_;

        if (not config.proxy_server.empty()) {
            JAMI_INFO("[Account %s] using proxy server %s",
                      getAccountID().c_str(),
                      config.proxy_server.c_str());
            if (not config.push_token.empty()) {
                JAMI_INFO("[Account %s] using push notifications", getAccountID().c_str());
            }
        }

        // check if dht peer service is enabled
        if (accountPeerDiscovery_ or accountPublish_) {
            peerDiscovery_ = std::make_shared<dht::PeerDiscovery>();
            if (accountPeerDiscovery_) {
                JAMI_INFO("[Account %s] starting Jami account discovery...", getAccountID().c_str());
                startAccountDiscovery();
            }
            if (accountPublish_)
                startAccountPublish();
        }
        dht::DhtRunner::Context context {};
        context.peerDiscovery = peerDiscovery_;

        auto dht_log_level = Manager::instance().dhtLogLevel.load();
        if (dht_log_level > 0) {
            static auto silent = [](char const* /*m*/, va_list /*args*/) {
            };
            static auto log_error = [](char const* m, va_list args) {
                Logger::vlog(LOG_ERR, nullptr, 0, true, m, args);
            };
            static auto log_warn = [](char const* m, va_list args) {
                Logger::vlog(LOG_WARNING, nullptr, 0, true, m, args);
            };
            static auto log_debug = [](char const* m, va_list args) {
                Logger::vlog(LOG_DEBUG, nullptr, 0, true, m, args);
            };
#ifndef _MSC_VER
            context.logger = std::make_shared<dht::Logger>(log_error,
                                                           (dht_log_level > 1) ? log_warn : silent,
                                                           (dht_log_level > 2) ? log_debug : silent);
#elif RING_UWP
            static auto log_all = [](char const* m, va_list args) {
                char tmp[2048];
                vsprintf(tmp, m, args);
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
                jami::emitSignal<DRing::DebugSignal::MessageSend>(std::to_string(now) + " "
                                                                  + std::string(tmp));
            };
            context.logger = std::make_shared<dht::Logger>(log_all, log_all, silent);
#else
            if (dht_log_level > 2) {
                context.logger = std::make_shared<dht::Logger>(log_error, log_warn, log_debug);
            } else if (dht_log_level > 1) {
                context.logger = std::make_shared<dht::Logger>(log_error, log_warn, silent);
            } else {
                context.logger = std::make_shared<dht::Logger>(log_error, silent, silent);
            }
#endif
            // logger_ = std::make_shared<dht::Logger>(log_error, log_warn, log_debug);
        }
        context.certificateStore = [](const dht::InfoHash& pk_id) {
            std::vector<std::shared_ptr<dht::crypto::Certificate>> ret;
            if (auto cert = tls::CertificateStore::instance().getCertificate(pk_id.toString()))
                ret.emplace_back(std::move(cert));
            JAMI_DBG("Query for local certificate store: %s: %zu found.",
                     pk_id.toString().c_str(),
                     ret.size());
            return ret;
        };

        auto currentDhtStatus = std::make_shared<dht::NodeStatus>(dht::NodeStatus::Disconnected);
        context.statusChangedCallback = [this, currentDhtStatus](dht::NodeStatus s4,
                                                                 dht::NodeStatus s6) {
            JAMI_DBG("[Account %s] Dht status : IPv4 %s; IPv6 %s",
                     getAccountID().c_str(),
                     dhtStatusStr(s4),
                     dhtStatusStr(s6));
            RegistrationState state;
            auto newStatus = std::max(s4, s6);
            if (newStatus == *currentDhtStatus)
                return;
            switch (newStatus) {
            case dht::NodeStatus::Connecting:
                JAMI_WARN("[Account %s] connecting to the DHT network...", getAccountID().c_str());
                state = RegistrationState::TRYING;
                break;
            case dht::NodeStatus::Connected:
                JAMI_WARN("[Account %s] connected to the DHT network", getAccountID().c_str());
                state = RegistrationState::REGISTERED;
                break;
            case dht::NodeStatus::Disconnected:
                JAMI_WARN("[Account %s] disconnected from the DHT network", getAccountID().c_str());
                state = RegistrationState::UNREGISTERED;
                break;
            default:
                state = RegistrationState::ERROR_GENERIC;
                break;
            }
            *currentDhtStatus = newStatus;
            setRegistrationState(state);
        };

        setRegistrationState(RegistrationState::TRYING);
        dht_->run((in_port_t) dhtPortUsed_, config, std::move(context));

        for (const auto& bootstrap : loadBootstrap())
            dht_->bootstrap(bootstrap);

        accountManager_->setDht(dht_);
        accountManager_->startSync();

        // Init connection manager
        if (!connectionManager_)
            connectionManager_ = std::make_unique<ConnectionManager>(*this);
        connectionManager_->onDhtConnected(DeviceId(accountManager_->getInfo()->deviceId));
        connectionManager_->onICERequest([this](const DeviceId& deviceId) {
            std::promise<bool> accept;
            std::future<bool> fut = accept.get_future();
            accountManager_->findCertificate(
                deviceId, [this, &accept](const std::shared_ptr<dht::crypto::Certificate>& cert) {
                    dht::InfoHash peer_account_id;
                    auto res = accountManager_->onPeerCertificate(cert,
                                                                  dhtPublicInCalls_,
                                                                  peer_account_id);
                    if (res)
                        JAMI_INFO("Accepting ICE request from account %s",
                                  peer_account_id.toString().c_str());
                    else
                        JAMI_INFO("Discarding ICE request from account %s",
                                  peer_account_id.toString().c_str());
                    accept.set_value(res);
                });
            fut.wait();
            auto result = fut.get();
            return result;
        });
        connectionManager_->onChannelRequest([this](const DeviceId&, const std::string& name) {
            auto isFile = name.substr(0, 7) == "file://";
            auto isVCard = name.substr(0, 8) == "vcard://";
            if (name == "sip") {
                return true;
            } else if (isFile or isVCard) {
                auto tid_str = isFile ? name.substr(7) : name.substr(8);
                uint64_t tid;
                std::istringstream iss(tid_str);
                iss >> tid;
                if (dhtPeerConnector_->onIncomingChannelRequest(tid)) {
                    std::lock_guard<std::mutex> lk(transfersMtx_);
                    incomingFileTransfers_.emplace(tid_str);
                    return true;
                }
            }
            return false;
        });
        connectionManager_->onConnectionReady([this](const DeviceId& deviceId,
                                                     const std::string& name,
                                                     std::shared_ptr<ChannelSocket> channel) {
            if (channel) {
                auto cert = tls::CertificateStore::instance().getCertificate(deviceId.toString());
                if (!cert || !cert->issuer)
                    return;
                auto peerId = cert->issuer->getId().toString();
                auto isFile = name.substr(0, 7) == "file://";
                auto isVCard = name.substr(0, 8) == "vcard://";
                if (name == "sip") {
                    cacheSIPConnection(std::move(channel), peerId, deviceId);
                } else if (isFile or isVCard) {
                    auto tid_str = isFile ? name.substr(7) : name.substr(8);
                    std::unique_lock<std::mutex> lk(transfersMtx_);
                    auto it = incomingFileTransfers_.find(tid_str);
                    // Note, outgoing file transfers are ignored.
                    if (it == incomingFileTransfers_.end())
                        return;
                    incomingFileTransfers_.erase(it);
                    lk.unlock();
                    uint64_t tid;
                    std::istringstream iss(tid_str);
                    iss >> tid;
                    std::function<void(const std::string&)> cb;
                    if (isVCard)
                        cb = [peerId, accountId = getAccountID()](const std::string& path) {
                            emitSignal<DRing::ConfigurationSignal::ProfileReceived>(accountId,
                                                                                    peerId,
                                                                                    path);
                        };
                    dhtPeerConnector_->onIncomingConnection(peerId,
                                                            tid,
                                                            std::move(channel),
                                                            std::move(cb));
                }
            }
        });

        // Listen for incoming calls
        callKey_ = dht::InfoHash::get("callto:" + accountManager_->getInfo()->deviceId);
        JAMI_DBG("[Account %s] Listening on callto:%s : %s",
                 getAccountID().c_str(),
                 accountManager_->getInfo()->deviceId.c_str(),
                 callKey_.toString().c_str());
        dht_->listen<dht::IceCandidates>(callKey_, [this](dht::IceCandidates&& msg) {
            // callback for incoming call
            auto from = msg.from;
            if (from == dht_->getId())
                return true;

            auto res = treatedCalls_.insert(msg.id);
            saveTreatedCalls();
            if (!res.second)
                return true;

            JAMI_WARN("[Account %s] ICE candidate from %s.",
                      getAccountID().c_str(),
                      from.toString().c_str());

            accountManager_->onPeerMessage(
                from,
                dhtPublicInCalls_,
                [this, msg = std::move(msg)](const std::shared_ptr<dht::crypto::Certificate>& cert,
                                             const dht::InfoHash& account) mutable {
                    incomingCall(std::move(msg), cert, account);
                });
            return true;
        });

        auto inboxDeviceKey = dht::InfoHash::get("inbox:" + accountManager_->getInfo()->deviceId);
        dht_->listen<dht::ImMessage>(inboxDeviceKey, [this, inboxDeviceKey](dht::ImMessage&& v) {
            auto msgId = to_hex_string(v.id);
            if (isMessageTreated(msgId))
                return true;
            accountManager_->onPeerMessage(
                v.from,
                dhtPublicInCalls_,
                [this, v, inboxDeviceKey, msgId](const std::shared_ptr<dht::crypto::Certificate>&,
                                                 const dht::InfoHash& peer_account) {
                    auto now = clock::to_time_t(clock::now());
                    std::string datatype = utf8_make_valid(v.datatype);
                    if (datatype.empty()) {
                        datatype = "text/plain";
                    }
                    std::map<std::string, std::string> payloads = {
                        {datatype, utf8_make_valid(v.msg)}};
                    onTextMessage(msgId, peer_account.toString(), payloads);
                    JAMI_DBG() << "Sending message confirmation " << v.id;
                    dht_->putEncrypted(inboxDeviceKey,
                                       v.from,
                                       dht::ImMessage(v.id, std::string(), now));
                });
            return true;
        });

        if (!dhtPeerConnector_)
            dhtPeerConnector_ = std::make_unique<DhtPeerConnector>(*this);

        std::lock_guard<std::mutex> bLock(buddyInfoMtx);
        for (auto& buddy : trackedBuddies_) {
            buddy.second.devices_cnt = 0;
            trackPresence(buddy.first, buddy.second);
        }
    } catch (const std::exception& e) {
        JAMI_ERR("Error registering DHT account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
JamiAccount::onTextMessage(const std::string& id,
                           const std::string& from,
                           const std::map<std::string, std::string>& payloads)
{
    try {
        const std::string fromUri {parseJamiUri(from)};
        SIPAccountBase::onTextMessage(id, fromUri, payloads);
    } catch (...) {
    }
}

void
JamiAccount::incomingCall(dht::IceCandidates&& msg,
                          const std::shared_ptr<dht::crypto::Certificate>& from_cert,
                          const dht::InfoHash& from)
{
    auto call = Manager::instance()
                    .callFactory.newCall<SIPCall, JamiAccount>(shared(),
                                                               Manager::instance().getNewCallID(),
                                                               Call::CallType::INCOMING);
    if (!call) {
        return;
    }
    auto callId = call->getCallId();
    auto iceOptions = getIceOptions();
    iceOptions.onNegoDone = [callId, w = weak()](bool) {
        runOnMainThread([callId, w]() {
            if (auto shared = w.lock())
                shared->checkPendingCall(callId);
        });
    };
    auto ice = createIceTransport(("sip:" + call->getCallId()).c_str(),
                                  ICE_COMPONENTS,
                                  false,
                                  iceOptions);
    iceOptions.tcpEnable = true;
    auto ice_tcp = createIceTransport(("sip:" + call->getCallId()).c_str(),
                                      ICE_COMPONENTS,
                                      true,
                                      iceOptions);

    std::weak_ptr<SIPCall> wcall = call;
    Manager::instance().addTask([account = shared(), wcall, ice, ice_tcp, msg, from_cert, from] {
        auto call = wcall.lock();

        // call aborted?
        if (not call)
            return false;

        if (ice->isFailed()) {
            JAMI_ERR("[call:%s] ice init failed", call->getCallId().c_str());
            call->onFailure(EIO);
            return false;
        }

        // Loop until ICE transport is initialized.
        // Note: we suppose that ICE init routine has a an internal timeout (bounded in time)
        // and we let upper layers decide when the call shall be aborted (our first check upper).
        if ((not ice->isInitialized()) || (ice_tcp && !ice_tcp->isInitialized()))
            return true;

        account->replyToIncomingIceMsg(call, ice, ice_tcp, msg, from_cert, from);
        return false;
    });
}

void
JamiAccount::replyToIncomingIceMsg(const std::shared_ptr<SIPCall>& call,
                                   const std::shared_ptr<IceTransport>& ice,
                                   const std::shared_ptr<IceTransport>& ice_tcp,
                                   const dht::IceCandidates& peer_ice_msg,
                                   const std::shared_ptr<dht::crypto::Certificate>& from_cert,
                                   const dht::InfoHash& from_id)
{
    auto from = from_id.toString();
    call->setPeerUri(RING_URI_PREFIX + from);
    std::weak_ptr<SIPCall> wcall = call;
#if HAVE_RINGNS
    accountManager_->lookupAddress(from,
                                   [wcall](const std::string& result,
                                           const NameDirectory::Response& response) {
                                       if (response == NameDirectory::Response::found)
                                           if (auto call = wcall.lock()) {
                                               call->setPeerRegisteredName(result);
                                               call->setPeerUri(RING_URI_PREFIX + result);
                                           }
                                   });
#endif
    registerDhtAddress(*ice);
    if (ice_tcp)
        registerDhtAddress(*ice_tcp);

    auto blob = ice->packIceMsg();
    if (ice_tcp) {
        auto ice_tcp_msg = ice_tcp->packIceMsg(2);
        blob.insert(blob.end(), ice_tcp_msg.begin(), ice_tcp_msg.end());
    }

    // Asynchronous DHT put of our local ICE data
    dht_->putEncrypted(callKey_,
                       peer_ice_msg.from,
                       dht::Value {dht::IceCandidates(peer_ice_msg.id, blob)},
                       [wcall](bool ok) {
                           if (!ok) {
                               JAMI_WARN("Can't put ICE descriptor reply on DHT");
                               if (auto call = wcall.lock())
                                   call->onFailure();
                           } else
                               JAMI_DBG("Successfully put ICE descriptor reply on DHT");
                       });

    auto started_time = std::chrono::steady_clock::now();

    auto sdp_list = IceTransport::parseSDPList(peer_ice_msg.ice_data);
    auto udp_failed = true, tcp_failed = true;
    initICE(peer_ice_msg.ice_data, ice, ice_tcp, udp_failed, tcp_failed);

    if (udp_failed && tcp_failed) {
        call->onFailure(EIO);
        return;
    }

    call->setPeerNumber(from);

    // Let the call handled by the PendingCall handler loop
    std::lock_guard<std::mutex> lock(callsMutex_);
    pendingCallsDht_.emplace(call->getCallId(),
                             PendingCall {/*.start = */ started_time,
                                          /*.ice_sp = */ udp_failed ? nullptr : ice,
                                          /*.ice_tcp_sp = */ tcp_failed ? nullptr : ice_tcp,
                                          /*.call = */ wcall,
                                          /*.listen_key = */ {},
                                          /*.call_key = */ {},
                                          /*.from = */ peer_ice_msg.from,
                                          /*.from_account = */ from_id,
                                          /*.from_cert = */ from_cert});

    Manager::instance().scheduleTaskIn(
        [w = weak(), callId = call->getCallId()]() {
            if (auto shared = w.lock())
                shared->checkPendingCall(callId);
        },
        ICE_NEGOTIATION_TIMEOUT);
}

void
JamiAccount::doUnregister(std::function<void(bool)> released_cb)
{
    std::unique_lock<std::mutex> lock(configurationMutex_);

    if (registrationState_ == RegistrationState::INITIALIZING
        || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION) {
        lock.unlock();
        if (released_cb)
            released_cb(false);
        return;
    }

    JAMI_WARN("[Account %s] unregistering account %p", getAccountID().c_str(), this);
    dht_->shutdown(
        [this]() { JAMI_WARN("[Account %s] dht shutdown complete", getAccountID().c_str()); });

    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCallsDht_.clear();
        pendingSipCalls_.clear();
    }

    {
        std::lock_guard<std::mutex> lk(pendingCallsMutex_);
        pendingCalls_.clear();
    }

    dht_->join();

    if (upnp_)
        upnp_->requestMappingRemove(static_cast<in_port_t>(dhtPortUsed_), upnp::PortType::UDP);

    lock.unlock();

    // Stop all current p2p connections if account is disabled
    // Else, we let the system managing if the co is down or not
    if (not isEnabled())
        shutdownConnections();

    setRegistrationState(RegistrationState::UNREGISTERED);

    if (released_cb)
        released_cb(false);
}

void
JamiAccount::connectivityChanged()
{
    JAMI_WARN("connectivityChanged");
    if (not isUsable()) {
        // nothing to do
        return;
    }

    dht_->connectivityChanged();
    // reset cache
    setPublishedAddress({});
    cacheTurnServers();
}

bool
JamiAccount::findCertificate(
    const dht::InfoHash& h,
    std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (accountManager_)
        return accountManager_->findCertificate(h, std::move(cb));
    return false;
}

bool
JamiAccount::findCertificate(const std::string& crt_id)
{
    if (accountManager_)
        return accountManager_->findCertificate(dht::InfoHash(crt_id));
    return false;
}

bool
JamiAccount::setCertificateStatus(const std::string& cert_id,
                                  tls::TrustStore::PermissionStatus status)
{
    bool done = accountManager_ ? accountManager_->setCertificateStatus(cert_id, status) : false;
    if (done) {
        findCertificate(cert_id);
        emitSignal<DRing::ConfigurationSignal::CertificateStateChanged>(getAccountID(),
                                                                        cert_id,
                                                                        tls::TrustStore::statusToStr(
                                                                            status));
    }
    return done;
}

std::vector<std::string>
JamiAccount::getCertificatesByStatus(tls::TrustStore::PermissionStatus status)
{
    if (accountManager_)
        return accountManager_->getCertificatesByStatus(status);
    return {};
}

template<typename ID = dht::Value::Id>
std::set<ID>
loadIdList(const std::string& path)
{
    std::set<ID> ids;
    std::ifstream file = fileutils::ifstream(path);
    if (!file.is_open()) {
        JAMI_DBG("Could not load %s", path.c_str());
        return ids;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        ID vid;
        if (!(iss >> std::hex >> vid)) {
            break;
        }
        ids.insert(vid);
    }
    return ids;
}

template<typename ID = dht::Value::Id>
void
saveIdList(const std::string& path, const std::set<ID>& ids)
{
    std::ofstream file = fileutils::ofstream(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not save to %s", path.c_str());
        return;
    }
    for (auto& c : ids)
        file << std::hex << c << "\n";
}

void
JamiAccount::loadTreatedCalls()
{
    treatedCalls_ = loadIdList(cachePath_ + DIR_SEPARATOR_STR "treatedCalls");
}

void
JamiAccount::saveTreatedCalls() const
{
    fileutils::check_dir(cachePath_.c_str());
    saveIdList(cachePath_ + DIR_SEPARATOR_STR "treatedCalls", treatedCalls_);
}

void
JamiAccount::loadTreatedMessages()
{
    std::lock_guard<std::mutex> lock(messageMutex_);
    auto path = cachePath_ + DIR_SEPARATOR_STR "treatedMessages";
    treatedMessages_ = loadIdList<std::string>(path);
    if (treatedMessages_.empty()) {
        auto messages = loadIdList(path);
        for (const auto& m : messages)
            treatedMessages_.emplace(to_hex_string(m));
    }
}

void
JamiAccount::saveTreatedMessages() const
{
    dht::ThreadPool::io().run([w = weak()]() {
        if (auto sthis = w.lock()) {
            auto& this_ = *sthis;
            std::lock_guard<std::mutex> lock(this_.messageMutex_);
            fileutils::check_dir(this_.cachePath_.c_str());
            saveIdList<std::string>(this_.cachePath_ + DIR_SEPARATOR_STR "treatedMessages",
                                    this_.treatedMessages_);
        }
    });
}

bool
JamiAccount::isMessageTreated(const std::string& id)
{
    std::lock_guard<std::mutex> lock(messageMutex_);
    auto res = treatedMessages_.emplace(id);
    if (res.second) {
        saveTreatedMessages();
        return false;
    }
    return true;
}

std::map<std::string, std::string>
JamiAccount::getKnownDevices() const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (not accountManager_ or not accountManager_->getInfo())
        return {};
    std::map<std::string, std::string> ids;
    for (auto& d : accountManager_->getKnownDevices()) {
        auto id = d.first.toString();
        auto label = d.second.name.empty() ? id.substr(0, 8) : d.second.name;
        ids.emplace(std::move(id), std::move(label));
    }
    return ids;
}

tls::DhParams
JamiAccount::loadDhParams(std::string path)
{
    std::lock_guard<std::mutex> l(fileutils::getFileLock(path));
    try {
        // writeTime throw exception if file doesn't exist
        auto duration = clock::now() - fileutils::writeTime(path);
        if (duration >= std::chrono::hours(24 * 3)) // file is valid only 3 days
            throw std::runtime_error("file too old");

        JAMI_DBG("Loading DhParams from file '%s'", path.c_str());
        return {fileutils::loadFile(path)};
    } catch (const std::exception& e) {
        JAMI_DBG("Failed to load DhParams file '%s': %s", path.c_str(), e.what());
        if (auto params = tls::DhParams::generate()) {
            try {
                fileutils::saveFile(path, params.serialize(), 0600);
                JAMI_DBG("Saved DhParams to file '%s'", path.c_str());
            } catch (const std::exception& ex) {
                JAMI_WARN("Failed to save DhParams in file '%s': %s", path.c_str(), ex.what());
            }
            return params;
        }
        JAMI_ERR("Can't generate DH params.");
        return {};
    }
}

void
JamiAccount::loadCachedUrl(const std::string& url,
                           const std::string& cachePath,
                           const std::chrono::seconds& cacheDuration,
                           std::function<void(const dht::http::Response& response)> cb)
{
    auto lock = std::make_shared<std::lock_guard<std::mutex>>(fileutils::getFileLock(cachePath));
    dht::ThreadPool::io().run([lock, cb, url, cachePath, cacheDuration, w = weak()]() {
        try {
            auto data = fileutils::loadCacheFile(cachePath, cacheDuration);
            dht::http::Response ret;
            ret.body = {data.begin(), data.end()};
            ret.status_code = 200;
            cb(ret);
        } catch (const std::exception& e) {
            JAMI_DBG("Failed to load '%.*s' from '%.*s': %s",
                     (int) url.size(),
                     url.c_str(),
                     (int) cachePath.size(),
                     cachePath.c_str(),
                     e.what());

            if (auto sthis = w.lock()) {
                auto req = std::make_shared<dht::http::Request>(
                    *Manager::instance().ioContext(),
                    url,
                    [lock, cb, cachePath, w](const dht::http::Response& response) {
                        if (response.status_code == 200) {
                            try {
                                fileutils::saveFile(cachePath,
                                                    (const uint8_t*) response.body.data(),
                                                    response.body.size(),
                                                    0600);
                                JAMI_DBG("Cached result to '%.*s'",
                                         (int) cachePath.size(),
                                         cachePath.c_str());
                            } catch (const std::exception& ex) {
                                JAMI_WARN("Failed to save result to %.*s: %s",
                                          (int) cachePath.size(),
                                          cachePath.c_str(),
                                          ex.what());
                            }
                        } else {
                            JAMI_WARN("Failed to download url");
                        }
                        cb(response);
                        if (auto req = response.request.lock())
                            if (auto sthis = w.lock())
                                sthis->requests_.erase(req);
                    });
                sthis->requests_.emplace(req);
                req->send();
            }
        }
    });
}

void
JamiAccount::loadCachedProxyServer(std::function<void(const std::string& proxy)> cb)
{
    if (proxyEnabled_ and proxyServerCached_.empty()) {
        JAMI_DBG("[Account %s] loading DHT proxy URL: %s",
                 getAccountID().c_str(),
                 proxyListUrl_.c_str());
        if (proxyListUrl_.empty()) {
            cb(getDhtProxyServer(proxyServer_));
        } else {
            loadCachedUrl(proxyListUrl_,
                          cachePath_ + DIR_SEPARATOR_STR "dhtproxylist",
                          std::chrono::hours(24 * 3),
                          [w = weak(), cb = std::move(cb)](const dht::http::Response& response) {
                              if (auto sthis = w.lock()) {
                                  if (response.status_code == 200) {
                                      cb(sthis->getDhtProxyServer(response.body));
                                  } else {
                                      cb(sthis->getDhtProxyServer(sthis->proxyServer_));
                                  }
                              }
                          });
        }
    } else {
        cb(proxyServerCached_);
    }
}

std::string
JamiAccount::getDhtProxyServer(const std::string& serverList)
{
    if (proxyServerCached_.empty()) {
        std::vector<std::string> proxys;
        // Split the list of servers
        std::sregex_iterator begin = {serverList.begin(), serverList.end(), PROXY_REGEX}, end;
        for (auto it = begin; it != end; ++it) {
            auto& match = *it;
            if (match[5].matched and match[6].matched) {
                try {
                    auto start = std::stoi(match[5]), end = std::stoi(match[6]);
                    for (auto p = start; p <= end; p++)
                        proxys.emplace_back(match[1].str() + match[2].str() + ":"
                                            + std::to_string(p));
                } catch (...) {
                    JAMI_WARN("Malformed proxy, ignore it");
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
        std::advance(randIt,
                     std::uniform_int_distribution<unsigned long>(0, proxys.size() - 1)(rand));
        proxyServerCached_ = *randIt;
        // Cache it!
        fileutils::check_dir(cachePath_.c_str(), 0700);
        std::string proxyCachePath = cachePath_ + DIR_SEPARATOR_STR "dhtproxy";
        std::ofstream file = fileutils::ofstream(proxyCachePath);
        JAMI_DBG("Cache DHT proxy server: %s", proxyServerCached_.c_str());
        if (file.is_open())
            file << proxyServerCached_;
        else
            JAMI_WARN("Cannot write into %s", proxyCachePath.c_str());
    }
    return proxyServerCached_;
}

void
JamiAccount::generateDhParams()
{
    // make sure cachePath_ is writable
    fileutils::check_dir(cachePath_.c_str(), 0700);
    dhParams_ = dht::ThreadPool::computation().get<tls::DhParams>(
        std::bind(loadDhParams, cachePath_ + DIR_SEPARATOR_STR "dhParams"));
}

MatchRank
JamiAccount::matches(std::string_view userName, std::string_view server) const
{
    if (not accountManager_ or not accountManager_->getInfo())
        return MatchRank::NONE;

    if (userName == accountManager_->getInfo()->accountId
        || server == accountManager_->getInfo()->accountId
        || userName == accountManager_->getInfo()->deviceId) {
        JAMI_DBG("Matching account id in request with username %.*s",
                 (int) userName.size(),
                 userName.data());
        return MatchRank::FULL;
    } else {
        return MatchRank::NONE;
    }
}

std::string
JamiAccount::getFromUri() const
{
    const std::string uri = "<sip:" + accountManager_->getInfo()->accountId + "@ring.dht>";
    if (not displayName_.empty())
        return "\"" + displayName_ + "\" " + uri;
    return uri;
}

std::string
JamiAccount::getToUri(const std::string& to) const
{
    return "<sips:" + to + ";transport=tls>";
}

pj_str_t
JamiAccount::getContactHeader(pjsip_transport* t)
{
    std::string quotedDisplayName = "\"" + displayName_ + "\" " + (displayName_.empty() ? "" : " ");
    if (t) {
        auto* td = reinterpret_cast<tls::AbstractSIPTransport::TransportData*>(t);
        auto address = td->self->getLocalAddress().toString(true);
        bool reliable = t->flag & PJSIP_TRANSPORT_RELIABLE;

        contact_.slen = pj_ansi_snprintf(contact_.ptr,
                                         PJSIP_MAX_URL_SIZE,
                                         "%s<sips:%s%s%s;transport=%s>",
                                         quotedDisplayName.c_str(),
                                         id_.second->getId().toString().c_str(),
                                         (address.empty() ? "" : "@"),
                                         address.c_str(),
                                         reliable ? "tls" : "dtls");
    } else {
        JAMI_ERR("getContactHeader: no SIP transport provided");
        contact_.slen = pj_ansi_snprintf(contact_.ptr,
                                         PJSIP_MAX_URL_SIZE,
                                         "%s<sips:%s@ring.dht>",
                                         quotedDisplayName.c_str(),
                                         id_.second->getId().toString().c_str());
    }
    return contact_;
}

/* contacts */

void
JamiAccount::addContact(const std::string& uri, bool confirmed)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->addContact(uri, confirmed);
    else
        JAMI_WARN("[Account %s] addContact: account not loaded", getAccountID().c_str());
}

void
JamiAccount::removeContact(const std::string& uri, bool ban)
{
    {
        std::lock_guard<std::mutex> lock(configurationMutex_);
        if (accountManager_)
            accountManager_->removeContact(uri, ban);
        else
            JAMI_WARN("[Account %s] removeContact: account not loaded", getAccountID().c_str());
    }

    // Remove current connections with contact
    std::set<DeviceId> devices;
    {
        std::unique_lock<std::mutex> lk(sipConnsMtx_);
        for (auto it = sipConns_.begin(); it != sipConns_.end();) {
            const auto& [key, value] = *it;
            if (key.first == uri) {
                devices.emplace(key.second);
                it = sipConns_.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const auto& device : devices) {
        if (connectionManager_)
            connectionManager_->closeConnectionsWith(device);
    }
}

std::map<std::string, std::string>
JamiAccount::getContactDetails(const std::string& uri) const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    return (accountManager_ and accountManager_->getInfo())
               ? accountManager_->getContactDetails(uri)
               : std::map<std::string, std::string> {};
}

std::vector<std::map<std::string, std::string>>
JamiAccount::getContacts() const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (not accountManager_)
        return {};
    return accountManager_->getContacts();
}

/* trust requests */

std::vector<std::map<std::string, std::string>>
JamiAccount::getTrustRequests() const
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    return accountManager_ ? accountManager_->getTrustRequests()
                           : std::vector<std::map<std::string, std::string>> {};
}

bool
JamiAccount::acceptTrustRequest(const std::string& from)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        return accountManager_->acceptTrustRequest(from);
    JAMI_WARN("[Account %s] acceptTrustRequest: account not loaded", getAccountID().c_str());
    return false;
}

bool
JamiAccount::discardTrustRequest(const std::string& from)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        return accountManager_->discardTrustRequest(from);
    JAMI_WARN("[Account %s] discardTrustRequest: account not loaded", getAccountID().c_str());
    return false;
}

void
JamiAccount::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->sendTrustRequest(to, payload);
    else
        JAMI_WARN("[Account %s] sendTrustRequest: account not loaded", getAccountID().c_str());
}

void
JamiAccount::sendTrustRequestConfirm(const std::string& to)
{
    std::lock_guard<std::mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->sendTrustRequestConfirm(dht::InfoHash(to));
    else
        JAMI_WARN("[Account %s] sendTrustRequestConfirm: account not loaded",
                  getAccountID().c_str());
}

void
JamiAccount::forEachDevice(const dht::InfoHash& to,
                           std::function<void(const dht::InfoHash&)>&& op,
                           std::function<void(bool)>&& end)
{
    accountManager_->forEachDevice(to, std::move(op), std::move(end));
}

uint64_t
JamiAccount::sendTextMessage(const std::string& to,
                             const std::map<std::string, std::string>& payloads)
{
    std::string toUri;
    try {
        toUri = parseJamiUri(to);
    } catch (...) {
        JAMI_ERR("Failed to send a text message due to an invalid URI %s", to.c_str());
        return 0;
    }
    if (payloads.size() != 1) {
        JAMI_ERR("Multi-part im is not supported yet by JamiAccount");
        return 0;
    }
    return SIPAccountBase::sendTextMessage(toUri, payloads);
}

void
JamiAccount::sendTextMessage(const std::string& to,
                             const std::map<std::string, std::string>& payloads,
                             uint64_t token,
                             bool retryOnTimeout)
{
    std::string toUri;
    try {
        toUri = parseJamiUri(to);
    } catch (...) {
        JAMI_ERR("Failed to send a text message due to an invalid URI %s", to.c_str());
        messageEngine_.onMessageSent(to, token, false);
        return;
    }
    if (payloads.size() != 1) {
        // Multi-part message
        // TODO: not supported yet
        JAMI_ERR("Multi-part im is not supported yet by JamiAccount");
        messageEngine_.onMessageSent(toUri, token, false);
        return;
    }

    auto toH = dht::InfoHash(toUri);
    auto now = clock::to_time_t(clock::now());

    auto confirm = std::make_shared<PendingConfirmation>();

    std::set<DeviceId> devices;
    std::unique_lock<std::mutex> lk(sipConnsMtx_);
    sip_utils::register_thread();
    for (auto it = sipConns_.begin(); it != sipConns_.end();) {
        auto& [key, value] = *it;
        if (key.first != to or value.empty()) {
            ++it;
            continue;
        }
        auto& conn = value.back();

        // Set input token into callback
        std::unique_ptr<TextMessageCtx> ctx {std::make_unique<TextMessageCtx>()};
        ctx->acc = weak();
        ctx->to = to;
        ctx->deviceId = key.second;
        ctx->id = token;
        ctx->retryOnTimeout = retryOnTimeout;
        ctx->confirmation = confirm;

        try {
            auto res = sendSIPMessage(
                conn, to, ctx.release(), token, payloads, [](void* token, pjsip_event* event) {
                    std::unique_ptr<TextMessageCtx> c {(TextMessageCtx*) token};
                    auto code = event->body.tsx_state.tsx->status_code;
                    auto acc = c->acc.lock();
                    if (not acc)
                        return;

                    if (code == PJSIP_SC_OK) {
                        std::unique_lock<std::mutex> l(c->confirmation->lock);
                        c->confirmation->replied = true;
                        l.unlock();
                        acc->messageEngine_.onMessageSent(c->to, c->id, true);
                    } else {
                        JAMI_WARN("Timeout when send a message, close current connection");
                        {
                            std::unique_lock<std::mutex> lk(acc->sipConnsMtx_);
                            acc->sipConns_.erase(std::make_pair(c->to, c->deviceId));
                        }
                        acc->connectionManager_->closeConnectionsWith(c->deviceId);
                        // This MUST be done after closing the connection to avoid race condition
                        // with messageEngine_
                        acc->messageEngine_.onMessageSent(c->to, c->id, false);

                        // In that case, the peer typically changed its connectivity.
                        // After closing sockets with that peer, we try to re-connect to
                        // that peer one time.
                        if (c->retryOnTimeout)
                            acc->messageEngine_.onPeerOnline(c->to, false);
                    }
                });
            if (!res) {
                messageEngine_.onMessageSent(to, token, false);
                ++it;
                continue;
            }
        } catch (const std::runtime_error& ex) {
            JAMI_WARN("%s", ex.what());
            messageEngine_.onMessageSent(to, token, false);
            // Remove connection in incorrect state
            it = sipConns_.erase(it);
            continue;
        }

        devices.emplace(key.second);
        ++it;
    }
    lk.unlock();

    // Find listening devices for this account
    accountManager_->forEachDevice(
        toH,
        [this, confirm, to, token, payloads, now, devices {std::move(devices)}](
            const dht::InfoHash& dev) {
            // Test if already sent
            if (devices.find(dev) != devices.end()) {
                return;
            }

            // Else, ask for a channel and send a DHT message
            requestSIPConnection(to, dev);
            {
                std::lock_guard<std::mutex> lock(messageMutex_);
                sentMessages_[token].to.emplace(dev);
            }

            auto h = dht::InfoHash::get("inbox:" + dev.toString());
            std::lock_guard<std::mutex> l(confirm->lock);
            auto list_token
                = dht_->listen<dht::ImMessage>(h, [this, to, token, confirm](dht::ImMessage&& msg) {
                      // check expected message confirmation
                      if (msg.id != token)
                          return true;

                      {
                          std::lock_guard<std::mutex> lock(messageMutex_);
                          auto e = sentMessages_.find(msg.id);
                          if (e == sentMessages_.end()
                              or e->second.to.find(msg.from) == e->second.to.end()) {
                              JAMI_DBG() << "[Account " << getAccountID() << "] [message " << token
                                         << "] Message not found";
                              return true;
                          }
                          sentMessages_.erase(e);
                          JAMI_DBG() << "[Account " << getAccountID() << "] [message " << token
                                     << "] Received text message reply";

                          // add treated message
                          auto res = treatedMessages_.emplace(to_hex_string(msg.id));
                          if (!res.second)
                              return true;
                      }
                      saveTreatedMessages();

                      // report message as confirmed received
                      {
                          std::lock_guard<std::mutex> l(confirm->lock);
                          for (auto& t : confirm->listenTokens)
                              dht_->cancelListen(t.first, std::move(t.second));
                          confirm->listenTokens.clear();
                          confirm->replied = true;
                      }
                      messageEngine_.onMessageSent(to, token, true);
                      return false;
                  });
            confirm->listenTokens.emplace(h, std::move(list_token));
            dht_->putEncrypted(h,
                               dev,
                               dht::ImMessage(token,
                                              std::string(payloads.begin()->first),
                                              std::string(payloads.begin()->second),
                                              now),
                               [this, to, token, confirm, h](bool ok) {
                                   JAMI_DBG()
                                       << "[Account " << getAccountID() << "] [message " << token
                                       << "] Put encrypted " << (ok ? "ok" : "failed");
                                   if (not ok) {
                                       std::unique_lock<std::mutex> l(confirm->lock);
                                       auto lt = confirm->listenTokens.find(h);
                                       if (lt != confirm->listenTokens.end()) {
                                           dht_->cancelListen(h, std::move(lt->second));
                                           confirm->listenTokens.erase(lt);
                                       }
                                       if (confirm->listenTokens.empty() and not confirm->replied) {
                                           l.unlock();
                                           messageEngine_.onMessageSent(to, token, false);
                                       }
                                   }
                               });

            JAMI_DBG() << "[Account " << getAccountID() << "] [message " << token
                       << "] Sending message for device " << dev.toString();
        },
        [this, to, token](bool ok) {
            if (not ok) {
                messageEngine_.onMessageSent(to, token, false);
            }
        });

    // Timeout cleanup
    Manager::instance().scheduleTaskIn(
        [w = weak(), confirm, to, token]() {
            std::unique_lock<std::mutex> l(confirm->lock);
            if (not confirm->replied) {
                if (auto this_ = w.lock()) {
                    JAMI_DBG() << "[Account " << this_->getAccountID() << "] [message " << token
                               << "] Timeout";
                    for (auto& t : confirm->listenTokens)
                        this_->dht_->cancelListen(t.first, std::move(t.second));
                    confirm->listenTokens.clear();
                    confirm->replied = true;
                    l.unlock();
                    this_->messageEngine_.onMessageSent(to, token, false);
                }
            }
        },
        std::chrono::minutes(1));
}

void
JamiAccount::onIsComposing(const std::string& peer, bool isWriting)
{
    try {
        const std::string fromUri {parseJamiUri(peer)};
        Account::onIsComposing(fromUri, isWriting);
    } catch (...) {
        JAMI_ERR("[Account %s] Can't parse URI: %s", getAccountID().c_str(), peer.c_str());
    }
}

void
JamiAccount::registerDhtAddress(IceTransport& ice)
{
    const auto reg_addr = [&](IceTransport& ice, const IpAddr& ip) {
        JAMI_DBG("[Account %s] using public IP: %s", getAccountID().c_str(), ip.toString().c_str());
        for (unsigned compId = 1; compId <= ice.getComponentCount(); ++compId)
            ice.registerPublicIP(compId, ip);
        return ip;
    };

    auto ip = getPublishedAddress();
    if (ip.empty()) {
        // We need a public address in case of NAT'ed network
        // Trying to use one discovered by DHT service

        // IPv6 (sdp support only one IP, put IPv6 before IPv4 as this last has the priority over
        // IPv6 less NAT'able)
        const auto& addr6 = dht_->getPublicAddress(AF_INET6);
        if (addr6.size())
            setPublishedAddress(reg_addr(ice, *addr6[0].get()));

        // IPv4
        const auto& addr4 = dht_->getPublicAddress(AF_INET);
        if (addr4.size())
            setPublishedAddress(reg_addr(ice, *addr4[0].get()));
    } else {
        reg_addr(ice, {ip});
    }
}

std::vector<std::string>
JamiAccount::publicAddresses()
{
    std::vector<std::string> addresses;
    for (auto& addr : dht_->getPublicAddress(AF_INET)) {
        addresses.emplace_back(addr.toString());
    }
    for (auto& addr : dht_->getPublicAddress(AF_INET6)) {
        addresses.emplace_back(addr.toString());
    }
    return addresses;
}

void
JamiAccount::requestPeerConnection(
    const std::string& peer_id,
    const DRing::DataTransferId& tid,
    bool isVCard,
    const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>&
        channeledConnectedCb,
    const std::function<void(const std::string&)>& onChanneledCancelled)
{
    if (not dhtPeerConnector_) {
        runOnMainThread([onChanneledCancelled, peer_id] { onChanneledCancelled(peer_id); });
        return;
    }
    dhtPeerConnector_->requestConnection(peer_id,
                                         tid,
                                         isVCard,
                                         channeledConnectedCb,
                                         onChanneledCancelled);
}

void
JamiAccount::closePeerConnection(const DRing::DataTransferId& tid)
{
    if (dhtPeerConnector_)
        dhtPeerConnector_->closeConnection(tid);
}

void
JamiAccount::enableProxyClient(bool enable)
{
    JAMI_WARN("[Account %s] DHT proxy client: %s",
              getAccountID().c_str(),
              enable ? "enable" : "disable");
    dht_->enableProxy(enable);
}

void
JamiAccount::setPushNotificationToken(const std::string& token)
{
    JAMI_WARN("[Account %s] setPushNotificationToken: %s", getAccountID().c_str(), token.c_str());
    deviceKey_ = token;
    dht_->setPushNotificationToken(deviceKey_);
}

/**
 * To be called by clients with relevant data when a push notification is received.
 */
void
JamiAccount::pushNotificationReceived(const std::string& from,
                                      const std::map<std::string, std::string>& data)
{
    JAMI_WARN("[Account %s] pushNotificationReceived: %s", getAccountID().c_str(), from.c_str());
    dht_->pushNotificationReceived(data);
}

std::string
JamiAccount::getUserUri() const
{
#ifdef HAVE_RINGNS
    if (not registeredName_.empty())
        return RING_URI_PREFIX + registeredName_;
#endif
    return RING_URI_PREFIX + username_;
}

std::vector<DRing::Message>
JamiAccount::getLastMessages(const uint64_t& base_timestamp)
{
    return SIPAccountBase::getLastMessages(base_timestamp);
}

void
JamiAccount::startAccountPublish()
{
    AccountPeerInfo info_pub;
    info_pub.accountId = dht::InfoHash(accountManager_->getInfo()->accountId);
    info_pub.displayName = displayName_;
    peerDiscovery_->startPublish<AccountPeerInfo>(PEER_DISCOVERY_JAMI_SERVICE, info_pub);
}

void
JamiAccount::startAccountDiscovery()
{
    auto id = dht::InfoHash(accountManager_->getInfo()->accountId);
    peerDiscovery_->startDiscovery<AccountPeerInfo>(
        PEER_DISCOVERY_JAMI_SERVICE, [this, id](AccountPeerInfo&& v, dht::SockAddr&&) {
            std::lock_guard<std::mutex> lc(discoveryMapMtx_);
            // Make sure that account itself will not be recorded
            if (v.accountId != id) {
                // Create or Find the old one
                auto& dp = discoveredPeers_[v.accountId];
                dp.displayName = v.displayName;
                discoveredPeerMap_[v.accountId.toString()] = v.displayName;
                if (dp.cleanupTask) {
                    dp.cleanupTask->cancel();
                } else {
                    // Avoid Repeat Reception of Same peer
                    JAMI_INFO("Account discovered: %s: %s",
                              v.displayName.c_str(),
                              v.accountId.to_c_str());
                    // Send Added Peer and corrsponding accoundID
                    emitSignal<DRing::PresenceSignal::NearbyPeerNotification>(getAccountID(),
                                                                              v.accountId.toString(),
                                                                              0,
                                                                              v.displayName);
                }
                dp.cleanupTask = Manager::instance().scheduler().scheduleIn(
                    [w = weak(), p = v.accountId, a = v.displayName] {
                        if (auto this_ = w.lock()) {
                            {
                                std::lock_guard<std::mutex> lc(this_->discoveryMapMtx_);
                                this_->discoveredPeers_.erase(p);
                                this_->discoveredPeerMap_.erase(p.toString());
                            }
                            // Send Deleted Peer
                            emitSignal<DRing::PresenceSignal::NearbyPeerNotification>(
                                this_->getAccountID(), p.toString(), 1, a);
                        }
                        JAMI_INFO("Account removed from discovery list: %s", a.c_str());
                    },
                    PEER_DISCOVERY_EXPIRATION);
            }
        });
}

std::map<std::string, std::string>
JamiAccount::getNearbyPeers() const
{
    return discoveredPeerMap_;
}

void
JamiAccount::setActiveCodecs(const std::vector<unsigned>& list)
{
    Account::setActiveCodecs(list);
    if (!hasActiveCodec(MEDIA_AUDIO))
        setCodecActive(AV_CODEC_ID_OPUS);
    if (!hasActiveCodec(MEDIA_VIDEO)) {
        setCodecActive(AV_CODEC_ID_HEVC);
        setCodecActive(AV_CODEC_ID_H264);
        setCodecActive(AV_CODEC_ID_VP8);
    }
}

void
JamiAccount::cacheTurnServers()
{
    // The resolution of the TURN server can take quite some time (if timeout).
    // So, run this in its own io thread to avoid to block the main thread.
    dht::ThreadPool::io().run([w = weak()] {
        auto this_ = w.lock();
        if (not this_)
            return;
        // Avoid multiple refresh
        if (this_->isRefreshing_.exchange(true))
            return;
        if (!this_->turnEnabled_) {
            // In this case, we do not use any TURN server
            std::lock_guard<std::mutex> lk(this_->cachedTurnMutex_);
            this_->cacheTurnV4_.reset();
            this_->cacheTurnV6_.reset();
            this_->isRefreshing_ = false;
            return;
        }
        JAMI_INFO("Refresh cache for TURN server resolution");
        // Retrieve old cached value if available.
        // This means that we directly get the correct value when launching the application on the
        // same network
        std::string server = this_->turnServer_.empty() ? DEFAULT_TURN_SERVER : this_->turnServer_;
        fileutils::recursive_mkdir(this_->cachePath_ + DIR_SEPARATOR_STR + "domains", 0700);
        auto pathV4 = this_->cachePath_ + DIR_SEPARATOR_STR + "domains" + DIR_SEPARATOR_STR + "v4."
                      + server;
        if (auto turnV4File = std::ifstream(pathV4)) {
            std::string content((std::istreambuf_iterator<char>(turnV4File)),
                                std::istreambuf_iterator<char>());
            std::lock_guard<std::mutex> lk(this_->cachedTurnMutex_);
            this_->cacheTurnV4_ = std::make_unique<IpAddr>(content, AF_INET);
        }
        auto pathV6 = this_->cachePath_ + DIR_SEPARATOR_STR + "domains" + DIR_SEPARATOR_STR + "v6."
                      + server;
        if (auto turnV6File = std::ifstream(pathV6)) {
            std::string content((std::istreambuf_iterator<char>(turnV6File)),
                                std::istreambuf_iterator<char>());
            std::lock_guard<std::mutex> lk(this_->cachedTurnMutex_);
            this_->cacheTurnV6_ = std::make_unique<IpAddr>(content, AF_INET6);
        }
        // Resolve just in case. The user can have a different connectivity
        auto turnV4 = IpAddr {server, AF_INET};
        {
            if (turnV4) {
                // Cache value to avoid a delay when starting up Jami
                std::ofstream turnV4File(pathV4);
                turnV4File << turnV4.toString();
            } else {
                fileutils::remove(pathV4, true);
            }
            std::lock_guard<std::mutex> lk(this_->cachedTurnMutex_);
            // Update TURN
            this_->cacheTurnV4_ = std::make_unique<IpAddr>(std::move(turnV4));
        }
        auto turnV6 = IpAddr {server.empty() ? DEFAULT_TURN_SERVER : server, AF_INET6};
        {
            if (turnV6) {
                // Cache value to avoid a delay when starting up Jami
                std::ofstream turnV6File(pathV6);
                turnV6File << turnV6.toString();
            } else {
                fileutils::remove(pathV6, true);
            }
            std::lock_guard<std::mutex> lk(this_->cachedTurnMutex_);
            // Update TURN
            this_->cacheTurnV6_ = std::make_unique<IpAddr>(std::move(turnV6));
        }
        this_->isRefreshing_ = false;
        JAMI_INFO("Cache refreshed for TURN resolution");
    });
}

void
JamiAccount::callConnectionClosed(const DeviceId& deviceId, bool eraseDummy)
{
    std::function<void(const dht::InfoHash&, bool)> cb;
    {
        std::lock_guard<std::mutex> lk(onConnectionClosedMtx_);
        auto it = onConnectionClosed_.find(deviceId);
        if (it != onConnectionClosed_.end()) {
            if (eraseDummy) {
                cb = std::move(it->second);
                onConnectionClosed_.erase(it);
            } else {
                // In this case a new subcall is created and the callback
                // will be re-called once with eraseDummy = true
                cb = it->second;
            }
        }
    }
    if (cb)
        cb(deviceId, eraseDummy);
}

void
JamiAccount::requestSIPConnection(const std::string& peerId, const DeviceId& deviceId)
{
    // If a connection already exists or is in progress, no need to do this
    std::lock_guard<std::mutex> lk(sipConnsMtx_);
    auto id = std::make_pair(peerId, deviceId);
    if (sipConns_.find(id) != sipConns_.end()) {
        JAMI_DBG("A SIP connection with %s already exists", deviceId.to_c_str());
        return;
    }
    sipConns_[id] = {};
    // If not present, create it
    JAMI_INFO("Ask %s for a new SIP channel", deviceId.to_c_str());
    if (!connectionManager_)
        return;
    connectionManager_->connectDevice(deviceId,
                                      "sip",
                                      [w = weak(), id](std::shared_ptr<ChannelSocket> socket,
                                                       const DeviceId&) {
                                          auto shared = w.lock();
                                          if (!shared)
                                              return;
                                          // NOTE: No need to cache Connection there.
                                          // OnConnectionReady is called before this callback, so
                                          // the socket is already cached if succeed. We just need
                                          // to remove the pending request.
                                          if (!socket) {
                                              // If this is triggered, this means that the
                                              // connectDevice didn't get any response from the DHT.
                                              // Stop searching pending call.
                                              shared->callConnectionClosed(id.second, true);
                                              shared->forEachPendingCall(id.second,
                                                                         [](const auto& pc) {
                                                                             pc->onFailure();
                                                                         });
                                          }

                                          std::lock_guard<std::mutex> lk(shared->sipConnsMtx_);
                                          auto it = shared->sipConns_.find(id);
                                          if (it != shared->sipConns_.end() && it->second.empty()) {
                                              shared->sipConns_.erase(it);
                                          }
                                      });
}

bool
JamiAccount::needToSendProfile(const std::string& deviceId)
{
    auto vCardMd5 = fileutils::md5File(idPath_ + DIR_SEPARATOR_STR + "profile.vcf");
    std::string currentMd5 {};
    auto vCardPath = cachePath_ + DIR_SEPARATOR_STR + "vcard";
    auto md5Path = vCardPath + DIR_SEPARATOR_STR + "md5";
    fileutils::check_dir(vCardPath.c_str(), 0700);
    try {
        currentMd5 = fileutils::loadTextFile(md5Path);
    } catch (...) {
        fileutils::saveFile(md5Path, {vCardMd5.begin(), vCardMd5.end()}, 0600);
        return true;
    }
    if (currentMd5 != vCardMd5) {
        // Incorrect md5 stored. Update it
        fileutils::removeAll(vCardPath, true);
        fileutils::check_dir(vCardPath.c_str(), 0700);
        fileutils::saveFile(md5Path, {vCardMd5.begin(), vCardMd5.end()}, 0600);
        return true;
    }
    return not fileutils::isFile(vCardPath + DIR_SEPARATOR_STR + deviceId);
}

bool
JamiAccount::sendSIPMessage(SipConnection& conn,
                            const std::string& to,
                            void* ctx,
                            uint64_t token,
                            const std::map<std::string, std::string>& data,
                            pjsip_endpt_send_callback cb)
{
    auto transport = conn.transport;
    auto channel = conn.channel;
    if (!channel || !channel->underlyingICE())
        throw std::runtime_error(
            "A SIP transport exists without Channel, this is a bug. Please report");

    // Build SIP Message
    // "deviceID@IP"
    auto toURI = getToUri(to + "@" + channel->underlyingICE()->getRemoteAddress(0).toString(true));
    std::string from = getFromUri();
    pjsip_tx_data* tdata;

    // Build SIP message
    constexpr pjsip_method msg_method = {PJSIP_OTHER_METHOD, sip_utils::CONST_PJ_STR("MESSAGE")};
    pj_str_t pjFrom = sip_utils::CONST_PJ_STR(from);
    pj_str_t pjTo = sip_utils::CONST_PJ_STR(toURI);

    // Create request.
    pj_status_t status = pjsip_endpt_create_request(link_.getEndpoint(),
                                                    &msg_method,
                                                    &pjTo,
                                                    &pjFrom,
                                                    &pjTo,
                                                    nullptr,
                                                    nullptr,
                                                    -1,
                                                    nullptr,
                                                    &tdata);
    if (status != PJ_SUCCESS) {
        JAMI_ERR("Unable to create request: %s", sip_utils::sip_strerror(status).c_str());
        return false;
    }

    // Add Date Header.
    pj_str_t date_str;
    constexpr auto key = sip_utils::CONST_PJ_STR("Date");
    pjsip_hdr* hdr;
    auto time = std::time(nullptr);
    auto date = std::ctime(&time);
    // the erase-remove idiom for a cstring, removes _all_ new lines with in date
    *std::remove(date, date + strlen(date), '\n') = '\0';

    // Add Header
    hdr = reinterpret_cast<pjsip_hdr*>(
        pjsip_date_hdr_create(tdata->pool, &key, pj_cstr(&date_str, date)));
    pjsip_msg_add_hdr(tdata->msg, hdr);

    // https://tools.ietf.org/html/rfc5438#section-6.3
    auto token_str = to_hex_string(token);
    auto pjMessageId = sip_utils::CONST_PJ_STR(token_str);
    hdr = reinterpret_cast<pjsip_hdr*>(
        pjsip_generic_string_hdr_create(tdata->pool, &STR_MESSAGE_ID, &pjMessageId));
    pjsip_msg_add_hdr(tdata->msg, hdr);

    // Add user-agent header
    sip_utils::addUserAgenttHeader(getUserAgentName(), tdata);

    // Init tdata
    const pjsip_tpselector tp_sel = SIPVoIPLink::getTransportSelector(transport->get());
    status = pjsip_tx_data_set_transport(tdata, &tp_sel);
    if (status != PJ_SUCCESS) {
        JAMI_ERR("Unable to create request: %s", sip_utils::sip_strerror(status).c_str());
        return false;
    }
    im::fillPJSIPMessageBody(*tdata, data);

    // Because pjsip_endpt_send_request can take quite some time, move it in a io thread to avoid to block
    dht::ThreadPool::io().run([w = weak(), tdata, ctx = std::move(ctx), cb = std::move(cb)] {
        auto shared = w.lock();
        if (!shared)
            return;
        sip_utils::register_thread();
        auto status = pjsip_endpt_send_request(shared->link_.getEndpoint(), tdata, -1, ctx, cb);
        if (status != PJ_SUCCESS)
            JAMI_ERR("Unable to send request: %s", sip_utils::sip_strerror(status).c_str());
    });
    return true;
}

void
JamiAccount::sendProfile(const std::string& deviceId)
{
    try {
        if (not needToSendProfile(deviceId)) {
            JAMI_INFO() << "Peer " << deviceId << " already got an up-to-date vcard";
            return;
        }

        DRing::DataTransferInfo info;
        DRing::DataTransferId id {0};
        info.accountId = getAccountID();
        info.peer = deviceId;
        info.path = idPath_ + DIR_SEPARATOR_STR + "profile.vcf";
        info.displayName = "profile.vcf";
        info.bytesProgress = 0;

        Manager::instance().dataTransfers->sendFile(info, id, [info](const std::string&) {
            // Mark the VCard as sent
            auto path = fileutils::get_cache_dir() + DIR_SEPARATOR_STR + info.accountId
                        + DIR_SEPARATOR_STR + "vcard" + DIR_SEPARATOR_STR + info.peer;
            fileutils::ofstream(path);
        });
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
}

void
JamiAccount::cacheSIPConnection(std::shared_ptr<ChannelSocket>&& socket,
                                const std::string& peerId,
                                const DeviceId& deviceId)
{
    std::unique_lock<std::mutex> lk(sipConnsMtx_);
    // Verify that the connection is not already cached
    SipConnectionKey key(peerId, deviceId);
    auto it = sipConns_.find(key);
    if (it == sipConns_.end())
        it = sipConns_.insert(it, std::make_pair(key, std::vector<SipConnection> {}));
    auto& connections = it->second;
    auto conn = std::find_if(connections.begin(), connections.end(), [socket](auto v) {
        return v.channel == socket;
    });
    if (conn != connections.end()) {
        JAMI_WARN("Channel socket already cached with this peer");
        return;
    }

    // Convert to SIP transport
    sip_utils::register_thread();
    auto onShutdown = [w = weak(), peerId, key, socket]() {
        auto shared = w.lock();
        if (!shared)
            return;
        {
            std::lock_guard<std::mutex> lk(shared->sipConnsMtx_);
            auto it = shared->sipConns_.find(key);
            if (it == shared->sipConns_.end())
                return;
            auto& connections = it->second;
            auto conn = connections.begin();
            while (conn != connections.end()) {
                if (conn->channel == socket)
                    conn = connections.erase(conn);
                else
                    conn++;
            }
            if (connections.empty())
                shared->sipConns_.erase(it);
        }
        // The connection can be closed during the SIP initialization, so
        // if this happens, the request should be re-sent to ask for a new
        // SIP channel to make the call pass through
        shared->callConnectionClosed(key.second, false);
    };
    auto sip_tr = link_.sipTransportBroker->getChanneledTransport(socket, std::move(onShutdown));
    // Store the connection
    connections.emplace_back(SipConnection {sip_tr, socket});
    JAMI_WARN("New SIP channel opened with %s", deviceId.to_c_str());
    lk.unlock();

    sendProfile(deviceId.toString());

    // Retry messages
    messageEngine_.onPeerOnline(peerId);

    // Connect pending calls
    forEachPendingCall(deviceId, [&](const auto& pc) {
        if (pc->getConnectionState() != Call::ConnectionState::TRYING
            and pc->getConnectionState() != Call::ConnectionState::PROGRESSING)
            return;
        pc->setTransport(sip_tr);
        pc->setState(Call::ConnectionState::PROGRESSING);
        if (auto ice = socket->underlyingICE()) {
            auto remoted_address = ice->getRemoteAddress(ICE_COMP_SIP_TRANSPORT);
            try {
                onConnectedOutgoingCall(pc, peerId, remoted_address);
            } catch (const VoipLinkException&) {
                // In this case, the main scenario is that SIPStartCall failed because
                // the ICE is dead and the TLS session didn't send any packet on that dead
                // link (connectivity change, killed by the os, etc)
                // Here, we don't need to do anything, the TLS will fail and will delete
                // the cached transport
            }
        }
    });
}

std::string_view
JamiAccount::currentDeviceId() const
{
    if (!accountManager_)
        return {};
    return accountManager_->getInfo()->deviceId;
}

} // namespace jami
