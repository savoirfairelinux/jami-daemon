/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <https://www.gnu.org/licenses/>.
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
#include "connectivity/multiplexed_socket.h"
#include "conversation_channel_handler.h"
#include "sync_channel_handler.h"
#include "transfer_channel_handler.h"
#include "swarm/swarm_channel_handler.h"

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "connectivity/sip_utils.h"
#include "connectivity/ice_transport.h"

#include "uri.h"

#include "client/ring_signal.h"
#include "jami/call_const.h"
#include "jami/account_const.h"

#include "connectivity/upnp/upnp_control.h"
#include "system_codec_container.h"

#include "account_schema.h"
#include "manager.h"
#include "connectivity/utf8_utils.h"

#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#include "plugin/chatservicesmanager.h"
#endif

#ifdef ENABLE_VIDEO
#include "libav_utils.h"
#endif
#include "fileutils.h"
#include "string_utils.h"
#include "archiver.h"
#include "data_transfer.h"

#include "connectivity/security/certstore.h"
#include "libdevcrypto/Common.h"
#include "base64.h"
#include "vcard.h"
#include "im/instant_messaging.h"

#include <opendht/thread_pool.h>
#include <opendht/peer_discovery.h>
#include <opendht/http.h>

#include <yaml-cpp/yaml.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
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
static constexpr const char MIME_TYPE_IMDN[] {"message/imdn+xml"};
static constexpr const char MIME_TYPE_IM_COMPOSING[] {"application/im-iscomposing+xml"};
static constexpr const char MIME_TYPE_INVITE[] {"application/invite"};
static constexpr const char MIME_TYPE_INVITE_JSON[] {"application/invite+json"};
static constexpr const char MIME_TYPE_GIT[] {"application/im-gitmessage-id"};
static constexpr const char FILE_URI[] {"file://"};
static constexpr const char VCARD_URI[] {"vcard://"};
static constexpr const char DATA_TRANSFER_URI[] {"data-transfer://"};
static constexpr const char DEVICE_ID_PATH[] {"ring_device"};
static constexpr std::chrono::steady_clock::duration COMPOSING_TIMEOUT {std::chrono::seconds(12)};

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
    std::shared_ptr<ChannelSocket> channel;
    bool onlyConnected;
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
    emitSignal<libjami::ConfigurationSignal::MigrationEnded>(accountID,
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
    std::set<DeviceId> to;
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

static constexpr const char* const RING_URI_PREFIX = "ring:";
static constexpr const char* const JAMI_URI_PREFIX = "jami:";
static const auto PROXY_REGEX = std::regex(
    "(https?://)?([\\w\\.\\-_\\~]+)(:(\\d+)|:\\[(.+)-(.+)\\])?");
static const std::string PEER_DISCOVERY_JAMI_SERVICE = "jami";
const constexpr auto PEER_DISCOVERY_EXPIRATION = std::chrono::minutes(1);

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
 * JamiAccount must use this helper than direct IceTransportFactory API
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

JamiAccount::JamiAccount(const std::string& accountId)
    : SIPAccountBase(accountId)
    , dht_(new dht::DhtRunner)
    , idPath_(fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId)
    , cachePath_(fileutils::get_cache_dir() + DIR_SEPARATOR_STR + accountId)
    , dataPath_(cachePath_ + DIR_SEPARATOR_STR "values")
    , connectionManager_ {}
    , nonSwarmTransferManager_(std::make_shared<TransferManager>(accountId, ""))
{}

JamiAccount::~JamiAccount() noexcept
{
    if (auto dht = dht_)
        dht->join();
}

void
JamiAccount::shutdownConnections()
{
    JAMI_DBG("[Account %s] Shutdown connections", getAccountID().c_str());

    decltype(gitServers_) gservers;
    {
        std::lock_guard<std::mutex> lk(gitServersMtx_);
        gservers = std::move(gitServers_);
    }
    for (auto& [_id, gs] : gservers)
        gs->stop();
    {
        std::lock_guard<std::mutex> lk(connManagerMtx_);
        // Just move destruction on another thread.
        dht::ThreadPool::io().run([conMgr = std::make_shared<decltype(connectionManager_)>(
                                       std::move(connectionManager_))] {});
        channelHandlers_.clear();
        connectionManager_.reset();
    }
    if (convModule_)
        convModule_->shutdownConnections();

    std::lock_guard<std::mutex> lk(sipConnsMtx_);
    sipConns_.clear();
}

void
JamiAccount::flush()
{
    // Class base method
    SIPAccountBase::flush();

    fileutils::removeAll(cachePath_);
    fileutils::removeAll(dataPath_);
    fileutils::removeAll(idPath_, true);
}

std::shared_ptr<SIPCall>
JamiAccount::newIncomingCall(const std::string& from,
                             const std::vector<libjami::MediaMap>& mediaList,
                             const std::shared_ptr<SipTransport>& sipTransp)
{
    JAMI_DEBUG("New incoming call from {:s} with {:d} media", from, mediaList.size());

    if (sipTransp) {
        std::unique_lock<std::mutex> connLock(sipConnsMtx_);
        for (auto& [key, value] : sipConns_) {
            if (key.first == from) {
                // Search for a matching linked SipTransport in connection list.
                for (auto conIter = value.rbegin(); conIter != value.rend(); conIter++) {
                    if (conIter->transport != sipTransp)
                        continue;

                    auto call = Manager::instance().callFactory.newSipCall(shared(),
                                                                           Call::CallType::INCOMING,
                                                                           mediaList);
                    call->setPeerUri(JAMI_URI_PREFIX + from);
                    call->setPeerNumber(from);

                    call->setSipTransport(sipTransp, getContactHeader(sipTransp));

                    return call;
                }
            }
        }
    }

    JAMI_ERR("newIncomingCall: can't find matching call for %s", from.c_str());
    return nullptr;
}

std::shared_ptr<Call>
JamiAccount::newOutgoingCall(std::string_view toUrl, const std::vector<libjami::MediaMap>& mediaList)
{
    auto& manager = Manager::instance();
    std::shared_ptr<SIPCall> call;

    // SIP allows sending empty invites, this use case is not used with Jami accounts.
    if (not mediaList.empty()) {
        call = manager.callFactory.newSipCall(shared(), Call::CallType::OUTGOING, mediaList);
    } else {
        JAMI_WARN("Media list is empty, setting a default list");
        call = manager.callFactory.newSipCall(shared(),
                                              Call::CallType::OUTGOING,
                                              MediaAttribute::mediaAttributesToMediaMaps(
                                                  createDefaultMediaList(isVideoEnabled())));
    }

    if (not call)
        return {};

    auto uri = Uri(toUrl);
    getIceOptions([call, w = weak(), uri = std::move(uri)](auto&& opts) {
        if (call->isIceEnabled()) {
            if (not call->createIceMediaTransport(false)
                or not call->initIceMediaTransport(true, std::forward<IceTransportOptions>(opts))) {
                return;
            }
        }
        auto shared = w.lock();
        if (!shared)
            return;
        JAMI_DBG() << "New outgoing call with " << uri.toString();
        call->setPeerNumber(uri.authority());
        call->setPeerUri(uri.toString());

        if (uri.scheme() == Uri::Scheme::SWARM || uri.scheme() == Uri::Scheme::RENDEZVOUS)
            shared->newSwarmOutgoingCallHelper(call, uri);
        else
            shared->newOutgoingCallHelper(call, uri);
    });

    return call;
}

void
JamiAccount::newOutgoingCallHelper(const std::shared_ptr<SIPCall>& call, const Uri& uri)
{
    JAMI_DBG() << this << "Calling peer " << uri.authority();
    try {
        startOutgoingCall(call, uri.authority());
    } catch (...) {
#if HAVE_RINGNS
        auto suffix = stripPrefix(uri.toString());
        NameDirectory::lookupUri(suffix,
                                 config().nameServer,
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
                                                 sthis->startOutgoingCall(call, result);
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
}

void
JamiAccount::newSwarmOutgoingCallHelper(const std::shared_ptr<SIPCall>& call, const Uri& uri)
{
    JAMI_DBG("[Account %s] Calling conversation %s",
             getAccountID().c_str(),
             uri.authority().c_str());
    convModule()->call(
        uri.authority(),
        call,
        [this, uri, call](const std::string& accountUri, const DeviceId& deviceId) {
            std::unique_lock<std::mutex> lkSipConn(sipConnsMtx_);
            for (auto& [key, value] : sipConns_) {
                if (key.first != accountUri || key.second != deviceId)
                    continue;
                if (value.empty())
                    continue;
                auto& sipConn = value.back();

                if (!sipConn.channel) {
                    JAMI_WARN(
                        "A SIP transport exists without Channel, this is a bug. Please report");
                    continue;
                }

                auto transport = sipConn.transport;
                if (!transport or !sipConn.channel)
                    continue;
                call->setState(Call::ConnectionState::PROGRESSING);

                auto remoted_address = sipConn.channel->getRemoteAddress();
                try {
                    onConnectedOutgoingCall(call, uri.authority(), remoted_address);
                    return;
                } catch (const VoipLinkException&) {
                    // In this case, the main scenario is that SIPStartCall failed because
                    // the ICE is dead and the TLS session didn't send any packet on that dead
                    // link (connectivity change, killed by the os, etc)
                    // Here, we don't need to do anything, the TLS will fail and will delete
                    // the cached transport
                    continue;
                }
            }
            lkSipConn.unlock();
            {
                std::lock_guard<std::mutex> lkP(pendingCallsMutex_);
                pendingCalls_[deviceId].emplace_back(call);
            }

            // Else, ask for a channel (for future calls/text messages)
            auto type = call->hasVideo() ? "videoCall" : "audioCall";
            JAMI_WARN("[call %s] No channeled socket with this peer. Send request",
                      call->getCallId().c_str());
            requestSIPConnection(accountUri, deviceId, type, true, call);
        });
}

void
JamiAccount::handleIncomingConversationCall(const std::string& callId,
                                            const std::string& destination)
{
    auto split = jami::split_string(destination, '/');
    if (split.size() != 4)
        return;
    auto conversationId = std::string(split[0]);
    auto accountUri = std::string(split[1]);
    auto deviceId = std::string(split[2]);
    auto confId = std::string(split[3]);

    if (getUsername() != accountUri || currentDeviceId() != deviceId)
        return;

    auto isNotHosting = !convModule()->isHosting(conversationId, confId);
    auto preferences = convModule()->getConversationPreferences(conversationId);
    auto canHost = true;
#if defined(__ANDROID__) || defined(__APPLE__)
    // By default, mobile devices SHOULD NOT host conferences.
    canHost = false;
#endif
    auto itPref = preferences.find(ConversationPreferences::HOST_CONFERENCES);
    if (itPref != preferences.end()) {
        canHost = itPref->second == TRUE_STR;
    }

    auto call = getCall(callId);
    if (!call) {
        JAMI_ERR("Call %s not found", callId.c_str());
        return;
    }

    if (isNotHosting && !canHost) {
        JAMI_DBG("Request for hosting a conference declined");
        Manager::instance().hangupCall(getAccountID(), callId);
        return;
    }
    Manager::instance().answerCall(*call);

    if (isNotHosting) {
        // Create conference and host it.
        convModule()->hostConference(conversationId, confId, callId);
        if (auto conf = getConference(confId))
            conf->detachLocalParticipant();
    } else {
        auto conf = getConference(confId);
        if (!conf) {
            JAMI_ERR("Conference %s not found", confId.c_str());
            return;
        }

        conf->addParticipant(callId);
        emitSignal<libjami::CallSignal::ConferenceChanged>(getAccountID(),
                                                           conf->getConfId(),
                                                           conf->getStateStr());
    }
}

std::shared_ptr<SIPCall>
JamiAccount::createSubCall(const std::shared_ptr<SIPCall>& mainCall)
{
    auto mediaList = MediaAttribute::mediaAttributesToMediaMaps(mainCall->getMediaAttributeList());
    return Manager::instance().callFactory.newSipCall(shared(), Call::CallType::OUTGOING, mediaList);
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

    call->setState(Call::ConnectionState::TRYING);
    std::weak_ptr<SIPCall> wCall = call;

#if HAVE_RINGNS
    accountManager_->lookupAddress(toUri,
                                   [wCall](const std::string& result,
                                           const NameDirectory::Response& response) {
                                       if (response == NameDirectory::Response::found)
                                           if (auto call = wCall.lock()) {
                                               call->setPeerRegisteredName(result);
                                           }
                                   });
#endif

    dht::InfoHash peer_account(toUri);

    // Call connected devices
    std::set<DeviceId> devices;
    std::unique_lock<std::mutex> lkSipConn(sipConnsMtx_);
    // NOTE: dummyCall is a call used to avoid to mark the call as failed if the
    // cached connection is failing with ICE (close event still not detected).
    auto dummyCall = createSubCall(call);

    call->addSubCall(*dummyCall);
    dummyCall->setIceMedia(call->getIceMedia());
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

            auto dev_call = createSubCall(call);
            dev_call->setPeerNumber(call->getPeerNumber());
            dev_call->setState(Call::ConnectionState::TRYING);
            call->addStateListener(
                [w = weak(), deviceId](Call::CallState, Call::ConnectionState state, int) {
                    if (state != Call::ConnectionState::PROGRESSING
                        and state != Call::ConnectionState::TRYING) {
                        if (auto shared = w.lock())
                            shared->callConnectionClosed(deviceId, true);
                        return false;
                    }
                    return true;
                });
            call->addSubCall(*dev_call);
            dev_call->setIceMedia(call->getIceMedia());
            {
                std::lock_guard<std::mutex> lk(pendingCallsMutex_);
                pendingCalls_[deviceId].emplace_back(dev_call);
            }

            JAMI_WARN("[call %s] No channeled socket with this peer. Send request",
                      call->getCallId().c_str());
            // Else, ask for a channel (for future calls/text messages)
            auto type = call->hasVideo() ? "videoCall" : "audioCall";
            requestSIPConnection(toUri, deviceId, type, true, dev_call);
        };

    std::vector<std::shared_ptr<ChannelSocket>> channels;
    for (auto& [key, value] : sipConns_) {
        if (key.first != toUri)
            continue;
        if (value.empty())
            continue;
        auto& sipConn = value.back();

        if (!sipConn.channel) {
            JAMI_WARN("A SIP transport exists without Channel, this is a bug. Please report");
            continue;
        }

        auto transport = sipConn.transport;
        auto remote_address = sipConn.channel->getRemoteAddress();
        if (!transport or !remote_address)
            continue;

        channels.emplace_back(sipConn.channel);

        JAMI_WARN("[call %s] A channeled socket is detected with this peer.",
                  call->getCallId().c_str());

        auto dev_call = createSubCall(call);
        dev_call->setPeerNumber(call->getPeerNumber());
        dev_call->setSipTransport(transport, getContactHeader(transport));
        call->addSubCall(*dev_call);
        dev_call->setIceMedia(call->getIceMedia());

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
                    return false;
                }
                return true;
            });

        try {
            onConnectedOutgoingCall(dev_call, toUri, remote_address);
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

    lkSipConn.unlock();
    // Note: Send beacon can destroy the socket (if storing last occurence of shared_ptr)
    // causing sipConn to be destroyed. So, do it while sipConns_ not locked.
    for (const auto& channel : channels)
        channel->sendBeacon();

    // Find listening devices for this account
    accountManager_->forEachDevice(
        peer_account,
        [this, devices = std::move(devices), sendRequest](
            const std::shared_ptr<dht::crypto::PublicKey>& dev) {
            // Test if already sent via a SIP transport
            auto deviceId = dev->getLongId();
            if (devices.find(deviceId) != devices.end())
                return;
            {
                std::lock_guard<std::mutex> lk(onConnectionClosedMtx_);
                onConnectionClosed_[deviceId] = sendRequest;
            }
            sendRequest(deviceId, false);
        },
        [wCall](bool ok) {
            if (not ok) {
                if (auto call = wCall.lock()) {
                    JAMI_WARN("[call:%s] no devices found", call->getCallId().c_str());
                    // Note: if a p2p connection exists, the call will be at least in CONNECTING
                    if (call->getConnectionState() == Call::ConnectionState::TRYING)
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

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface(), target.getFamily());

    IpAddr addrSdp = getPublishedSameasLocal() ? localAddress
                                               : getPublishedIpAddress(target.getFamily());

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

    auto mediaAttrList = call->getMediaAttributeList();

    if (mediaAttrList.empty()) {
        JAMI_ERR("Call [%s] has no media. Abort!", call->getCallId().c_str());
        return;
    }

    if (not sdp.createOffer(mediaAttrList)) {
        JAMI_ERR("Could not send outgoing INVITE request for new call");
        return;
    }

    // Note: pj_ice_strans_create can call onComplete in the same thread
    // This means that iceMutex_ in IceTransport can be locked when onInitDone is called
    // So, we need to run the call creation in the main thread
    // Also, we do not directly call SIPStartCall before receiving onInitDone, because
    // there is an inside waitForInitialization that can block the thread.
    // Note: avoid runMainThread as SIPStartCall use transportMutex
    dht::ThreadPool::io().run([w = weak(), call = std::move(call), target] {
        auto account = w.lock();
        if (not account)
            return;

        if (not account->SIPStartCall(*call, target)) {
            JAMI_ERR("Could not send outgoing INVITE request for new call");
        }
    });
}

bool
JamiAccount::SIPStartCall(SIPCall& call, const IpAddr& target)
{
    JAMI_DBG("Start SIP call [%s]", call.getCallId().c_str());

    if (call.isIceEnabled())
        call.addLocalIceAttributes();

    std::string toUri(getToUri(call.getPeerNumber() + "@"
                               + target.toString(true))); // expecting a fully well formed sip uri

    pj_str_t pjTo = sip_utils::CONST_PJ_STR(toUri);

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = sip_utils::CONST_PJ_STR(from);

    std::string targetStr = getToUri(target.toString(true));
    pj_str_t pjTarget = sip_utils::CONST_PJ_STR(targetStr);

    auto contact = call.getContactHeader();
    auto pjContact = sip_utils::CONST_PJ_STR(contact);

    JAMI_DBG("contact header: %s / %s -> %s / %s",
             contact.c_str(),
             from.c_str(),
             toUri.c_str(),
             targetStr.c_str());

    auto local_sdp = call.getSDP().getLocalSdpSession();
    pjsip_dialog* dialog {nullptr};
    pjsip_inv_session* inv {nullptr};
    if (!CreateClientDialogAndInvite(&pjFrom, &pjContact, &pjTo, &pjTarget, local_sdp, &dialog, &inv))
        return false;

    inv->mod_data[link_.getModId()] = &call;
    call.setInviteSession(inv);

    pjsip_tx_data* tdata;

    if (pjsip_inv_invite(call.inviteSession_.get(), &tdata) != PJ_SUCCESS) {
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
    sip_utils::addUserAgentHeader(getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(call.inviteSession_.get(), tdata) != PJ_SUCCESS) {
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
        config().serialize(accountOut);
        auto accountConfig = config().path + DIR_SEPARATOR_STR + "config.yml";
        std::lock_guard<std::mutex> lock(fileutils::getFileLock(accountConfig));
        std::ofstream fout = fileutils::ofstream(accountConfig);
        fout.write(accountOut.c_str(), accountOut.size());
        JAMI_DBG("Saved account config to %s", accountConfig.c_str());
    } catch (const std::exception& e) {
        JAMI_ERR("Error saving account config: %s", e.what());
    }
}

void
JamiAccount::loadConfig()
{
    SIPAccountBase::loadConfig();
    registeredName_ = config().registeredName;
    if (accountManager_)
        accountManager_->setAccountDeviceName(config().deviceName);
    try {
        auto str = fileutils::loadCacheTextFile(cachePath_ + DIR_SEPARATOR_STR "dhtproxy",
                                                std::chrono::hours(24 * 7));
        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (reader->parse(str.data(), str.data() + str.size(), &root, &err)) {
            auto key = root[getProxyConfigKey()];
            if (key.isString())
                proxyServerCached_ = key.asString();
        }
    } catch (const std::exception& e) {
        JAMI_DBG("[Account %s] Can't load proxy URL from cache: %s",
                 getAccountID().c_str(),
                 e.what());
    }
    loadAccount(config().archive_password, config().archive_pin, config().archive_path);
}

bool
JamiAccount::changeArchivePassword(const std::string& password_old, const std::string& password_new)
{
    try {
        if (!accountManager_->changePassword(password_old, password_new)) {
            JAMI_ERR("[Account %s] Can't change archive password", getAccountID().c_str());
            return false;
        }
        editConfig([&](JamiAccountConfig& config) {
            config.archiveHasPassword = not password_new.empty();
        });
    } catch (const std::exception& ex) {
        JAMI_ERR("[Account %s] Can't change archive password: %s",
                 getAccountID().c_str(),
                 ex.what());
        if (password_old.empty()) {
            editConfig([&](JamiAccountConfig& config) { config.archiveHasPassword = true; });
            emitSignal<libjami::ConfigurationSignal::AccountDetailsChanged>(getAccountID(),
                                                                            getAccountDetails());
        }
        return false;
    }
    if (password_old != password_new)
        emitSignal<libjami::ConfigurationSignal::AccountDetailsChanged>(getAccountID(),
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
        emitSignal<libjami::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 2, "");
        return;
    }
    accountManager_
        ->addDevice(password, [this](AccountManager::AddDeviceResult result, std::string pin) {
            switch (result) {
            case AccountManager::AddDeviceResult::SUCCESS_SHOW_PIN:
                emitSignal<libjami::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 0, pin);
                break;
            case AccountManager::AddDeviceResult::ERROR_CREDENTIALS:
                emitSignal<libjami::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 1, "");
                break;
            case AccountManager::AddDeviceResult::ERROR_NETWORK:
                emitSignal<libjami::ConfigurationSignal::ExportOnRingEnded>(getAccountID(), 2, "");
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
JamiAccount::setValidity(const std::string& pwd, const dht::InfoHash& id, int64_t validity)
{
    if (auto manager = dynamic_cast<ArchiveAccountManager*>(accountManager_.get())) {
        if (manager->setValidity(pwd, id_, id, validity)) {
            saveIdentity(id_, idPath_, DEVICE_ID_PATH);
            return true;
        }
    }
    return false;
}

void
JamiAccount::forceReloadAccount()
{
    editConfig([&](JamiAccountConfig& conf) {
        conf.receipt.clear();
        conf.receiptSignature.clear();
    });
    loadAccount();
}

void
JamiAccount::unlinkConversations(const std::set<std::string>& removed)
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (auto info = accountManager_->getInfo()) {
        auto contacts = info->contacts->getContacts();
        for (auto& [id, c] : contacts) {
            if (removed.find(c.conversationId) != removed.end()) {
                JAMI_WARN(
                    "[Account %s] Detected removed conversation (%s) in contact details for %s",
                    getAccountID().c_str(),
                    c.conversationId.c_str(),
                    id.to_c_str());
                c.conversationId.clear();
            }
        }
        info->contacts->setContacts(contacts);
    }
}

bool
JamiAccount::isValidAccountDevice(const dht::crypto::Certificate& cert) const
{
    if (accountManager_) {
        if (auto info = accountManager_->getInfo()) {
            if (info->contacts)
                return info->contacts->isValidAccountDevice(cert).isValid();
        }
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
            emitSignal<libjami::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(),
                                                                            device,
                                                                            static_cast<int>(
                                                                                result));
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

    JAMI_DEBUG("[Account {}] loading account", getAccountID());
    AccountManager::OnChangeCallback callbacks {
        [this](const std::string& uri, bool confirmed) {
            if (!id_.first)
                return;
            runOnMainThread([id = getAccountID(), uri, confirmed] {
                emitSignal<libjami::ConfigurationSignal::ContactAdded>(id, uri, confirmed);
            });
        },
        [this](const std::string& uri, bool banned) {
            if (!id_.first)
                return;

            dht::ThreadPool::io().run([w = weak(), uri, banned] {
                if (auto shared = w.lock()) {
                    // Erase linked conversation's requests
                    if (auto convModule = shared->convModule())
                        convModule->removeContact(uri, banned);
                    // Remove current connections with contact
                    if (shared->connectionManager_) {
                        shared->connectionManager_->closeConnectionsWith(uri);
                    }
                    // Update client.
                    emitSignal<libjami::ConfigurationSignal::ContactRemoved>(shared->getAccountID(),
                                                                             uri,
                                                                             banned);
                }
            });
        },
        [this](const std::string& uri,
               const std::string& conversationId,
               const std::vector<uint8_t>& payload,
               time_t received) {
            if (!id_.first)
                return;
            clearProfileCache(uri);
            if (conversationId.empty()) {
                // Old path
                emitSignal<libjami::ConfigurationSignal::IncomingTrustRequest>(getAccountID(),
                                                                               conversationId,
                                                                               uri,
                                                                               payload,
                                                                               received);
                return;
            }
            // Here account can be initializing
            if (auto cm = convModule()) {
                auto activeConv = cm->getOneToOneConversation(uri);
                if (activeConv != conversationId) {
                    cm->onTrustRequest(uri, conversationId, payload, received);
                }
            }
        },
        [this](const std::map<DeviceId, KnownDevice>& devices) {
            std::map<std::string, std::string> ids;
            for (auto& d : devices) {
                auto id = d.first.toString();
                auto label = d.second.name.empty() ? id.substr(0, 8) : d.second.name;
                ids.emplace(std::move(id), std::move(label));
            }
            runOnMainThread([id = getAccountID(), devices = std::move(ids)] {
                emitSignal<libjami::ConfigurationSignal::KnownDevicesChanged>(id, devices);
            });
        },
        [this](const std::string& conversationId) {
            // Note: Do not retrigger on another thread. This has to be done
            // at the same time of acceptTrustRequest a synced state between TrustRequest
            // and convRequests.
            convModule()->acceptConversationRequest(conversationId);
        },
        [this](const std::string& uri, const std::string& convFromReq) {
            // Remove cached payload if there is one
            auto requestPath = cachePath_ + DIR_SEPARATOR_STR + "requests" + DIR_SEPARATOR_STR
                               + uri;
            fileutils::remove(requestPath);
            if (!convFromReq.empty()) {
                auto oldConv = convModule()->getOneToOneConversation(uri);
                // If we previously removed the contact, and re-add it, we may
                // receive a convId different from the request. In that case,
                // we need to remove the current conversation and clone the old
                // one (given by convFromReq).
                // TODO: In the future, we may want to re-commit the messages we
                // may have send in the request we sent.
                if (oldConv != convFromReq && updateConvForContact(uri, oldConv, convFromReq)) {
                    convModule()->initReplay(oldConv, convFromReq);
                    convModule()->cloneConversationFrom(convFromReq, uri, oldConv);
                }
            }
        }};

    const auto& conf = config();
    try {
        auto onAsync = [w = weak()](AccountManager::AsyncUser&& cb) {
            if (auto this_ = w.lock())
                cb(*this_->accountManager_);
        };
        if (conf.managerUri.empty()) {
            accountManager_.reset(new ArchiveAccountManager(
                getPath(),
                onAsync,
                [this]() { return getAccountDetails(); },
                conf.archivePath.empty() ? "archive.gz" : conf.archivePath,
                conf.nameServer));
        } else {
            accountManager_.reset(
                new ServerAccountManager(getPath(), onAsync, conf.managerUri, conf.nameServer));
        }

        auto id = accountManager_->loadIdentity(conf.tlsCertificateFile,
                                                conf.tlsPrivateKeyFile,
                                                conf.tlsPassword);

        if (auto info = accountManager_->useIdentity(id,
                                                     conf.receipt,
                                                     conf.receiptSignature,
                                                     conf.managerUsername,
                                                     callbacks)) {
            // normal loading path
            id_ = std::move(id);
            config_->username = info->accountId;
            JAMI_WARN("[Account %s] loaded account identity", getAccountID().c_str());
            if (not isEnabled()) {
                setRegistrationState(RegistrationState::UNREGISTERED);
            }
            convModule()->loadConversations();
        } else if (isEnabled()) {
            JAMI_WARNING("[Account {}] useIdentity failed!", getAccountID());
            if (not conf.managerUri.empty() and archive_password.empty()) {
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
            if (conf.managerUri.empty()) {
                auto acreds = std::make_unique<ArchiveAccountManager::ArchiveAccountCredentials>();
                auto archivePath = fileutils::getFullPath(idPath_, conf.archivePath);
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
                    acreds->dhtPort = dhtPortUsed();
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
                screds->username = conf.managerUsername;
                creds = std::move(screds);
            }
            creds->password = archive_password;
            bool hasPassword = !archive_password.empty();

            accountManager_->initAuthentication(
                fDeviceKey,
                ip_utils::getDeviceName(),
                std::move(creds),
                [this, migrating, hasPassword](const AccountInfo& info,
                                               const std::map<std::string, std::string>& config,
                                               std::string&& receipt,
                                               std::vector<uint8_t>&& receipt_signature) {
                    JAMI_LOG("[Account {}] Auth success!", getAccountID());

                    fileutils::check_dir(idPath_.c_str(), 0700);

                    auto id = info.identity;
                    editConfig([&](JamiAccountConfig& conf) {
                        std::tie(conf.tlsPrivateKeyFile, conf.tlsCertificateFile)
                            = saveIdentity(id, idPath_, DEVICE_ID_PATH);
                        conf.tlsPassword = {};
                        conf.archiveHasPassword = hasPassword;
                        if (not conf.managerUri.empty()) {
                            conf.registeredName = conf.managerUsername;
                            registeredName_ = conf.managerUsername;
                        }
                        conf.username = info.accountId;
                        conf.deviceName = accountManager_->getAccountDeviceName();

                        auto nameServerIt = config.find(
                            libjami::Account::ConfProperties::RingNS::URI);
                        if (nameServerIt != config.end() && !nameServerIt->second.empty()) {
                            conf.nameServer = nameServerIt->second;
                        }
                        auto displayNameIt = config.find(
                            libjami::Account::ConfProperties::DISPLAYNAME);
                        if (displayNameIt != config.end() && !displayNameIt->second.empty()) {
                            conf.displayName = displayNameIt->second;
                        }
                        conf.receipt = std::move(receipt);
                        conf.receiptSignature = std::move(receipt_signature);
                        conf.fromMap(config);
                    });
                    id_ = std::move(id);
                    if (migrating) {
                        Migration::setState(getAccountID(), Migration::State::SUCCESS);
                    }
                    if (not info.photo.empty() or not config_->displayName.empty())
                        emitSignal<libjami::ConfigurationSignal::AccountProfileReceived>(
                            getAccountID(), config_->displayName, info.photo);
                    setRegistrationState(RegistrationState::UNREGISTERED);
                    convModule()->loadConversations();
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
                callbacks);
        }
    } catch (const std::exception& e) {
        JAMI_WARN("[Account %s] error loading account: %s", getAccountID().c_str(), e.what());
        accountManager_.reset();
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

std::map<std::string, std::string>
JamiAccount::getVolatileAccountDetails() const
{
    auto a = SIPAccountBase::getVolatileAccountDetails();
    a.emplace(libjami::Account::VolatileProperties::InstantMessaging::OFF_CALL, TRUE_STR);
#if HAVE_RINGNS
    if (not registeredName_.empty())
        a.emplace(libjami::Account::VolatileProperties::REGISTERED_NAME, registeredName_);
#endif
    a.emplace(libjami::Account::ConfProperties::PROXY_SERVER, proxyServerCached_);
    a.emplace(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED,
              deviceAnnounced_ ? TRUE_STR : FALSE_STR);
    if (accountManager_) {
        if (auto info = accountManager_->getInfo()) {
            a.emplace(libjami::Account::ConfProperties::DEVICE_ID, info->deviceId);
        }
    }
    return a;
}

#if HAVE_RINGNS
void
JamiAccount::lookupName(const std::string& name)
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->lookupUri(name,
                                   config().nameServer,
                                   [acc = getAccountID(), name](const std::string& result,
                                                                NameDirectory::Response response) {
                                       emitSignal<libjami::ConfigurationSignal::RegisteredNameFound>(
                                           acc, (int) response, result, name);
                                   });
}

void
JamiAccount::lookupAddress(const std::string& addr)
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    auto acc = getAccountID();
    if (accountManager_)
        accountManager_->lookupAddress(
            addr, [acc, addr](const std::string& result, NameDirectory::Response response) {
                emitSignal<libjami::ConfigurationSignal::RegisteredNameFound>(acc,
                                                                              (int) response,
                                                                              addr,
                                                                              result);
            });
}

void
JamiAccount::registerName(const std::string& password, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
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
                    if (auto this_ = w.lock()) {
                        this_->registeredName_ = name;
                        this_->editConfig(
                            [&](JamiAccountConfig& config) { config.registeredName = name; });
                        emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
                            this_->accountID_, this_->getVolatileAccountDetails());
                    }
                }
                emitSignal<libjami::ConfigurationSignal::NameRegistrationEnded>(acc, res, name);
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
                jami::emitSignal<libjami::ConfigurationSignal::UserSearchEnded>(acc,
                                                                                (int) response,
                                                                                query,
                                                                                result);
            });
    return false;
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
                    std::lock_guard<std::recursive_mutex> lock(s->configurationMutex_);
                    s->doRegister_();
                }
            });
        }
    };

    loadCachedProxyServer([onLoad](const std::string&) { onLoad(); });

    if (upnpCtrl_) {
        JAMI_DBG("UPnP: Attempting to map ports for Jami account");

        // Release current mapping if any.
        if (dhtUpnpMapping_.isValid()) {
            upnpCtrl_->releaseMapping(dhtUpnpMapping_);
        }

        dhtUpnpMapping_.enableAutoUpdate(true);

        // Set the notify callback.
        dhtUpnpMapping_.setNotifyCallback([w = weak(),
                                           onLoad,
                                           update = std::make_shared<bool>(false)](
                                              upnp::Mapping::sharedPtr_t mapRes) {
            if (auto accPtr = w.lock()) {
                auto& dhtMap = accPtr->dhtUpnpMapping_;
                auto& accId = accPtr->getAccountID();

                JAMI_WARN("[Account %s] DHT UPNP mapping changed to %s",
                          accId.c_str(),
                          mapRes->toString(true).c_str());

                if (*update) {
                    // Check if we need to update the mapping and the registration.
                    if (dhtMap.getMapKey() != mapRes->getMapKey()
                        or dhtMap.getState() != mapRes->getState()) {
                        // The connectivity must be restarted, if either:
                        // - the state changed to "OPEN",
                        // - the state changed to "FAILED" and the mapping was in use.
                        if (mapRes->getState() == upnp::MappingState::OPEN
                            or (mapRes->getState() == upnp::MappingState::FAILED
                                and dhtMap.getState() == upnp::MappingState::OPEN)) {
                            // Update the mapping and restart the registration.
                            dhtMap.updateFrom(mapRes);

                            JAMI_WARN("[Account %s] Allocated port changed to %u. Restarting the "
                                      "registration",
                                      accId.c_str(),
                                      accPtr->dhtPortUsed());

                            accPtr->dht_->connectivityChanged();

                        } else {
                            // Only update the mapping.
                            dhtMap.updateFrom(mapRes);
                        }
                    }
                } else {
                    *update = true;
                    // Set connection info and load the account.
                    if (mapRes->getState() == upnp::MappingState::OPEN) {
                        dhtMap.updateFrom(mapRes);
                        JAMI_DBG("[Account %s] Mapping %s successfully allocated: starting the DHT",
                                 accId.c_str(),
                                 dhtMap.toString().c_str());
                    } else {
                        JAMI_WARN(
                            "[Account %s] Mapping request is in %s state: starting the DHT anyway",
                            accId.c_str(),
                            mapRes->getStateStr());
                    }

                    // Load the account and start the DHT.
                    onLoad();
                }
            }
        });

        // Request the mapping.
        auto map = upnpCtrl_->reserveMapping(dhtUpnpMapping_);
        // The returned mapping is invalid. Load the account now since
        // we may never receive the callback.
        if (not map or not map->isValid()) {
            onLoad();
        }

    } else {
        // No UPNP. Load the account and start the DHT. The local DHT
        // might not be reachable for peers if we are behind a NAT.
        onLoad();
    }
}

void
JamiAccount::doRegister()
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
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
    if (upnpCtrl_ or proxyServerCached_.empty()) {
        registerAsyncOps();
    } else {
        doRegister_();
    }
}

std::vector<std::string>
JamiAccount::loadBootstrap() const
{
    std::vector<std::string> bootstrap;
    std::string_view stream(config().hostname), node_addr;
    while (jami::getline(stream, node_addr, ';'))
        bootstrap.emplace_back(node_addr);
    for (const auto& b : bootstrap)
        JAMI_DBG("[Account %s] Bootstrap node: %s", getAccountID().c_str(), b.c_str());
    return bootstrap;
}

void
JamiAccount::trackBuddyPresence(const std::string& buddy_id, bool track)
{
    std::string buddyUri;
    try {
        buddyUri = parseJamiUri(buddy_id);
    } catch (...) {
        JAMI_ERR("[Account %s] Failed to track presence: invalid URI %s",
                 getAccountID().c_str(),
                 buddy_id.c_str());
        return;
    }
    JAMI_DBG("[Account %s] %s presence for %s",
             getAccountID().c_str(),
             track ? "Track" : "Untrack",
             buddy_id.c_str());

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
    buddy.listenToken = dht->listen<
        DeviceAnnouncement>(h, [this, h](DeviceAnnouncement&& dev, bool expired) {
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
        // NOTE: the rest can use configurationMtx_, that can be locked during unregister so
        // do not retrigger on dht
        runOnMainThread([w = weak(), h, dev, expired, isConnected, wasConnected]() {
            auto sthis = w.lock();
            if (!sthis)
                return;
            if (not expired) {
                // Retry messages every time a new device announce its presence
                sthis->messageEngine_.onPeerOnline(h.toString());
                sthis->findCertificate(
                    dev.dev, [sthis, h](const std::shared_ptr<dht::crypto::Certificate>& cert) {
                        if (cert) {
                            auto pk = std::make_shared<dht::crypto::PublicKey>(cert->getPublicKey());
                            if (sthis->convModule()->needsSyncingWith(h.toString(),
                                                                      pk->getLongId().toString()))
                                sthis->requestSIPConnection(
                                    h.toString(),
                                    pk->getLongId(),
                                    "sync"); // Both sides will sync conversations
                        }
                    });
            }
            if (isConnected and not wasConnected) {
                sthis->onTrackedBuddyOnline(h);
            } else if (not isConnected and wasConnected) {
                sthis->onTrackedBuddyOffline(h);
            }
        });

        return true;
    });
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
    std::string id(contactId.toString());
    JAMI_DBG("Buddy %s online", id.c_str());
    emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(), id, 1, "");

    auto details = getContactDetails(id);
    auto it = details.find("confirmed");
    if (it == details.end() or it->second == "false") {
        auto convId = convModule()->getOneToOneConversation(id);
        if (convId.empty())
            return;
        // In this case, the TrustRequest was sent but never confirmed (cause the contact was
        // offline maybe) To avoid the contact to never receive the conv request, retry there
        std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
        if (accountManager_) {
            // Retrieve cached payload for trust request.
            auto requestPath = cachePath_ + DIR_SEPARATOR_STR + "requests" + DIR_SEPARATOR_STR
                               + contactId.toString();
            std::vector<uint8_t> payload;
            try {
                payload = fileutils::loadFile(requestPath);
            } catch (...) {
            }

            if (payload.size() > 64000) {
                JAMI_WARN() << "Trust request is too big, reset payload";
                payload.clear();
            }

            accountManager_->sendTrustRequest(id, convId, payload);
        }
    }
}

void
JamiAccount::onTrackedBuddyOffline(const dht::InfoHash& contactId)
{
    JAMI_DBG("Buddy %s offline", contactId.toString().c_str());
    emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
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
    const auto& conf = config();

    try {
        if (not accountManager_ or not accountManager_->getInfo())
            throw std::runtime_error("No identity configured for this account.");

        loadTreatedMessages();
        if (dht_->isRunning()) {
            JAMI_ERR("[Account %s] DHT already running (stopping it first).",
                     getAccountID().c_str());
            dht_->join();
        }

        convModule()->clearPendingFetch();

#if HAVE_RINGNS
        // Look for registered name
        accountManager_->lookupAddress(
            accountManager_->getInfo()->accountId,
            [w = weak()](const std::string& result, const NameDirectory::Response& response) {
                if (auto this_ = w.lock()) {
                    if (response == NameDirectory::Response::found) {
                        if (this_->registeredName_ != result) {
                            this_->registeredName_ = result;
                            this_->editConfig(
                                [&](JamiAccountConfig& config) { config.registeredName = result; });
                            emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
                                this_->accountID_, this_->getVolatileAccountDetails());
                        }
                    } else if (response == NameDirectory::Response::notFound) {
                        if (not this_->registeredName_.empty()) {
                            this_->registeredName_.clear();
                            this_->editConfig(
                                [&](JamiAccountConfig& config) { config.registeredName.clear(); });
                            emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
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
        config.push_token = conf.deviceKey;
        config.push_topic = conf.notificationTopic;
        config.push_platform = conf.platform;
        config.threaded = true;
        config.peer_discovery = conf.dhtPeerDiscovery;
        config.peer_publish = conf.dhtPeerDiscovery;
        if (conf.proxyEnabled)
            config.proxy_server = proxyServerCached_;

        if (not config.proxy_server.empty()) {
            JAMI_LOG("[Account {}] using proxy server {}",
                      getAccountID(),
                      config.proxy_server);
            if (not config.push_token.empty()) {
                JAMI_LOG("[Account %s] using push notifications with platform: {}, topic: {}, token: {}", getAccountID(), config.push_platform, config.push_topic, config.push_token);
            }
        }

        // check if dht peer service is enabled
        if (conf.accountPeerDiscovery or conf.accountPublish) {
            peerDiscovery_ = std::make_shared<dht::PeerDiscovery>();
            if (conf.accountPeerDiscovery) {
                JAMI_INFO("[Account %s] starting Jami account discovery...", getAccountID().c_str());
                startAccountDiscovery();
            }
            if (conf.accountPublish)
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
                jami::emitSignal<libjami::ConfigurationSignal::MessageSend>(
                    std::to_string(now) + " " + std::string(tmp));
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

        context.statusChangedCallback = [this](dht::NodeStatus s4, dht::NodeStatus s6) {
            JAMI_DBG("[Account %s] Dht status: IPv4 %s; IPv6 %s",
                     getAccountID().c_str(),
                     dhtStatusStr(s4),
                     dhtStatusStr(s6));
            RegistrationState state;
            auto newStatus = std::max(s4, s6);
            switch (newStatus) {
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
        };
        context.identityAnnouncedCb = [this](bool ok) {
            if (!ok)
                return;
            accountManager_->startSync(
                [this](const std::shared_ptr<dht::crypto::Certificate>& crt) {
                    if (!crt)
                        return;
                    auto deviceId = crt->getLongId().toString();
                    if (accountManager_->getInfo()->deviceId == deviceId)
                        return;

                    std::unique_lock<std::mutex> lk(connManagerMtx_);
                    initConnectionManager();
                    channelHandlers_[Uri::Scheme::SYNC]
                        ->connect(crt->getLongId(),
                                  "",
                                  [this](std::shared_ptr<ChannelSocket> socket,
                                         const DeviceId& deviceId) {
                                      if (socket)
                                          syncModule()->syncWith(deviceId, socket);
                                  });
                    lk.unlock();
                    requestSIPConnection(
                        getUsername(),
                        crt->getLongId(),
                        "sync"); // For git notifications, will use the same socket as sync
                },
                [this] {
                    deviceAnnounced_ = true;

                    // Bootstrap at the end to avoid to be long to load.
                    dht::ThreadPool::io().run([w = weak()] {
                        if (auto shared = w.lock())
                            shared->convModule()->bootstrap();
                    });
                    emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
                        accountID_, getVolatileAccountDetails());
                });
        };

        setRegistrationState(RegistrationState::TRYING);
        dht_->run(dhtPortUsed(), config, std::move(context));

        for (const auto& bootstrap : loadBootstrap())
            dht_->bootstrap(bootstrap);

        accountManager_->setDht(dht_);

        std::unique_lock<std::mutex> lkCM(connManagerMtx_);
        initConnectionManager();
        connectionManager_->onDhtConnected(*accountManager_->getInfo()->devicePk);
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
        connectionManager_->onChannelRequest(
            [this](const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name) {
                JAMI_WARN("[Account %s] New channel asked with name %s",
                          getAccountID().c_str(),
                          name.c_str());

                if (this->config().turnEnabled && turnCache_) {
                    auto addr = turnCache_->getResolvedTurn();
                    if (addr == std::nullopt) {
                        // If TURN is enabled, but no TURN cached, there can be a temporary
                        // resolution error to solve. Sometimes, a connectivity change is not
                        // enough, so even if this case is really rare, it should be easy to avoid.
                        turnCache_->refresh();
                    }
                }

                auto uri = Uri(name);
                auto itHandler = channelHandlers_.find(uri.scheme());
                if (itHandler != channelHandlers_.end() && itHandler->second)
                    return itHandler->second->onRequest(cert, name);
                if (name == "sip") {
                    return true;
                }
                return false;
            });
        connectionManager_->onConnectionReady([this](const DeviceId& deviceId,
                                                     const std::string& name,
                                                     std::shared_ptr<ChannelSocket> channel) {
            if (channel) {
                auto cert = channel->peerCertificate();
                if (!cert || !cert->issuer)
                    return;
                auto peerId = cert->issuer->getId().toString();
                if (name == "sip") {
                    cacheSIPConnection(std::move(channel), peerId, deviceId);
                } else if (name.find("git://") == 0) {
                    auto sep = name.find_last_of('/');
                    auto conversationId = name.substr(sep + 1);
                    auto remoteDevice = name.substr(6, sep - 6);

                    if (channel->isInitiator()) {
                        // Check if wanted remote it's our side (git://remoteDevice/conversationId)
                        return;
                    }

                    // Check if pull from banned device
                    if (convModule()->isBannedDevice(conversationId, remoteDevice)) {
                        JAMI_WARNING("[Account {:s}] Git server requested for conversation {:s}, but the "
                                  "device is "
                                  "unauthorized ({:s}) ",
                                  getAccountID(),
                                  conversationId,
                                  remoteDevice);
                        channel->shutdown();
                        return;
                    }

                    auto sock = convModule()->gitSocket(deviceId.toString(), conversationId);
                    if (sock == channel) {
                        // The onConnectionReady is already used as client (for retrieving messages)
                        // So it's not the server socket
                        return;
                    }
                    JAMI_WARNING("[Account {:s}] Git server requested for conversation {:s}, device {:s}, "
                              "channel {}",
                              accountID_,
                              conversationId,
                              deviceId.toString(),
                              channel->channel());
                    auto gs = std::make_unique<GitServer>(accountID_, conversationId, channel);
                    gs->setOnFetched(
                        [w = weak(), conversationId, deviceId](const std::string& commit) {
                            if (auto shared = w.lock())
                                shared->convModule()->setFetched(conversationId,
                                                                 deviceId.toString(),
                                                                 commit);
                        });
                    const dht::Value::Id serverId = ValueIdDist()(rand);
                    {
                        std::lock_guard<std::mutex> lk(gitServersMtx_);
                        gitServers_[serverId] = std::move(gs);
                    }
                    channel->onShutdown([w = weak(), serverId]() {
                        // Run on main thread to avoid to be in mxSock's eventLoop
                        runOnMainThread([serverId, w]() {
                            auto shared = w.lock();
                            if (!shared)
                                return;
                            std::lock_guard<std::mutex> lk(shared->gitServersMtx_);
                            shared->gitServers_.erase(serverId);
                        });
                    });
                } else {
                    // TODO move git://
                    auto uri = Uri(name);
                    auto itHandler = channelHandlers_.find(uri.scheme());
                    if (itHandler != channelHandlers_.end() && itHandler->second)
                        itHandler->second->onReady(cert, name, std::move(channel));
                }
            }
        });
        lkCM.unlock();

        // Note: this code should be unused unless for DHT text messages
        auto inboxDeviceKey = dht::InfoHash::get(
            "inbox:" + accountManager_->getInfo()->devicePk->getId().toString());
        dht_->listen<dht::ImMessage>(inboxDeviceKey, [this, inboxDeviceKey](dht::ImMessage&& v) {
            auto msgId = to_hex_string(v.id);
            if (isMessageTreated(msgId))
                return true;
            accountManager_
                ->onPeerMessage(*v.owner,
                                dhtPublicInCalls_,
                                [this,
                                 v,
                                 inboxDeviceKey,
                                 msgId](const std::shared_ptr<dht::crypto::Certificate>& cert,
                                        const dht::InfoHash& peer_account) {
                                    auto now = clock::to_time_t(clock::now());
                                    std::string datatype = utf8_make_valid(v.datatype);
                                    if (datatype.empty()) {
                                        datatype = "text/plain";
                                    }
                                    std::map<std::string, std::string> payloads = {
                                        {datatype, utf8_make_valid(v.msg)}};
                                    onTextMessage(msgId,
                                                  peer_account.toString(),
                                                  cert->getPublicKey().getLongId().toString(),
                                                  payloads);
                                    JAMI_DBG() << "Sending message confirmation " << v.id;
                                    dht_->putEncrypted(inboxDeviceKey,
                                                       v.from,
                                                       dht::ImMessage(v.id, std::string(), now));
                                });
            return true;
        });

        std::lock_guard<std::mutex> lock(buddyInfoMtx);
        for (auto& buddy : trackedBuddies_) {
            buddy.second.devices_cnt = 0;
            trackPresence(buddy.first, buddy.second);
        }
    } catch (const std::exception& e) {
        JAMI_ERR("Error registering DHT account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

ConversationModule*
JamiAccount::convModule()
{
    if (!accountManager() || currentDeviceId() == "") {
        JAMI_ERR("[Account %s] Calling convModule() with an uninitialized account",
                 getAccountID().c_str());
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(moduleMtx_);
    if (!convModule_) {
        convModule_ = std::make_unique<ConversationModule>(
            weak(),
            [this](auto&& syncMsg) {
                runOnMainThread([w = weak(), syncMsg] {
                    if (auto shared = w.lock())
                        shared->syncModule()->syncWithConnected(syncMsg);
                });
            },
            [this](auto&& uri, auto&& device, auto&& msg, auto token = 0) {
                // No need to retrigger, sendTextMessage will call
                // messageEngine_.sendMessage, already retriggering on
                // main thread.
                auto deviceId = device ? device.toString() : "";
                return sendTextMessage(uri, deviceId, msg, token);
            },
            [this](const auto& convId, const auto& deviceId, auto cb, const auto& type) {
                runOnMainThread([w = weak(), convId, deviceId, cb = std::move(cb), type] {
                    auto shared = w.lock();
                    if (!shared)
                        return;
                    if (auto socket = shared->convModule()->gitSocket(deviceId, convId)) {
                        if (!cb(socket))
                            socket->shutdown();
                        else
                            cb({});
                        return;
                    }
                    std::lock_guard<std::mutex> lkCM(shared->connManagerMtx_);
                    if (!shared->connectionManager_) {
                        cb({});
                        return;
                    }
                    shared->connectionManager_->connectDevice(
                        DeviceId(deviceId),
                        "git://" + deviceId + "/" + convId,
                        [shared, cb, convId](std::shared_ptr<ChannelSocket> socket,
                                             const DeviceId&) {
                            if (socket) {
                                socket->onShutdown([shared, deviceId = socket->deviceId(), convId] {
                                    shared->convModule()->removeGitSocket(deviceId.toString(),
                                                                          convId);
                                });
                                if (!cb(socket))
                                    socket->shutdown();
                            } else
                                cb({});
                        },
                        false,
                        false,
                        type);
                });
            },
            [this](const auto& convId, const auto& deviceId, auto&& cb, const auto& connectionType) {
                runOnMainThread([w = weak(), convId, deviceId, cb, connectionType] {
                    auto shared = w.lock();
                    if (!shared)
                        return;
                    std::lock_guard<std::mutex> lkCM(shared->connManagerMtx_);
                    if (!shared->connectionManager_) {
                        Manager::instance().ioContext()->post([cb] { cb({}); });
                        return;
                    }
                    if (!shared->connectionManager_->hasSwarmChannel(DeviceId(deviceId), convId)) {
                        shared->connectionManager_->connectDevice(
                            DeviceId(deviceId),
                            fmt::format("swarm://{}", convId),
                            [w, cb](std::shared_ptr<ChannelSocket> socket,
                                    const DeviceId& deviceId) {
                                if (socket) {
                                    auto shared = w.lock();
                                    if (!shared)
                                        return;
                                    auto remoteCert = socket->peerCertificate();
                                    auto uri = remoteCert->issuer->getId().toString();
                                    std::unique_lock<std::mutex> lk(shared->sipConnsMtx_);
                                    // Verify that the connection is not already cached
                                    SipConnectionKey key(uri, deviceId.toString());
                                    auto it = shared->sipConns_.find(key);
                                    if (it == shared->sipConns_.end()) {
                                        lk.unlock();
                                        shared->requestSIPConnection(uri, deviceId, "");
                                    }
                                }

                                cb(socket);
                            });
                    }
                });
            },
            [this](auto&& convId, auto&& contactUri, bool accept) {
                // NOTE: do not reschedule as the conversation's requests
                // should be synched with trust requests
                if (accept) {
                    accountManager_->acceptTrustRequest(contactUri, true);
                } else {
                    updateConvForContact(contactUri, convId, "");
                }
            });
    }
    return convModule_.get();
}

SyncModule*
JamiAccount::syncModule()
{
    if (!accountManager() || currentDeviceId() == "") {
        JAMI_ERR() << "Calling syncModule() with an uninitialized account.";
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(moduleMtx_);
    if (!syncModule_)
        syncModule_ = std::make_unique<SyncModule>(weak());
    return syncModule_.get();
}

void
JamiAccount::onTextMessage(const std::string& id,
                           const std::string& from,
                           const std::string& deviceId,
                           const std::map<std::string, std::string>& payloads)
{
    try {
        const std::string fromUri {parseJamiUri(from)};
        SIPAccountBase::onTextMessage(id, fromUri, deviceId, payloads);
    } catch (...) {
    }
}

void
JamiAccount::doUnregister(std::function<void(bool)> released_cb)
{
    std::unique_lock<std::recursive_mutex> lock(configurationMutex_);
    if (registrationState_ >= RegistrationState::ERROR_GENERIC) {
        lock.unlock();
        if (released_cb)
            released_cb(false);
        return;
    }

    std::mutex mtx;
    std::condition_variable cv;
    bool shutdown_complete {false};

    if (peerDiscovery_) {
        peerDiscovery_->stopPublish(PEER_DISCOVERY_JAMI_SERVICE);
        peerDiscovery_->stopDiscovery(PEER_DISCOVERY_JAMI_SERVICE);
    }

    JAMI_WARN("[Account %s] unregistering account %p", getAccountID().c_str(), this);
    dht_->shutdown(
        [&] {
            JAMI_WARN("[Account %s] dht shutdown complete", getAccountID().c_str());
            std::lock_guard<std::mutex> lock(mtx);
            shutdown_complete = true;
            cv.notify_all();
        },
        true);

    {
        std::lock_guard<std::mutex> lk(pendingCallsMutex_);
        pendingCalls_.clear();
    }

    // Stop all current p2p connections if account is disabled
    // Else, we let the system managing if the co is down or not
    // NOTE: this is used for changing account's config.
    if (not isEnabled())
        shutdownConnections();

    // Release current upnp mapping if any.
    if (upnpCtrl_ and dhtUpnpMapping_.isValid()) {
        upnpCtrl_->releaseMapping(dhtUpnpMapping_);
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return shutdown_complete; });
    }
    dht_->join();
    setRegistrationState(RegistrationState::UNREGISTERED);

    lock.unlock();

    if (released_cb)
        released_cb(false);
#ifdef ENABLE_PLUGIN
    jami::Manager::instance().getJamiPluginManager().getChatServicesManager().cleanChatSubjects(
        getAccountID());
#endif
}

void
JamiAccount::setRegistrationState(RegistrationState state,
                                  int detail_code,
                                  const std::string& detail_str)
{
    if (registrationState_ != state) {
        if (state == RegistrationState::REGISTERED) {
            JAMI_WARNING("[Account {}] connected", getAccountID());
            turnCache_->refresh();
            storeActiveIpAddress();
        } else if (state == RegistrationState::TRYING) {
            JAMI_WARNING("[Account {}] connecting…", getAccountID());
        } else {
            deviceAnnounced_ = false;
            JAMI_WARNING("[Account {}] disconnected", getAccountID());
        }
    }
    // Update registrationState_ & emit signals
    Account::setRegistrationState(state, detail_code, detail_str);
}

void
JamiAccount::connectivityChanged()
{
    JAMI_WARN("connectivityChanged");
    if (not isUsable()) {
        // nothing to do
        return;
    }
    convModule()->connectivityChanged();
    dht_->connectivityChanged();
    {
        std::lock_guard<std::mutex> lkCM(connManagerMtx_);
        if (connectionManager_)
            connectionManager_->connectivityChanged();
    }
    // reset cache
    setPublishedAddress({});
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
JamiAccount::findCertificate(
    const dht::PkId& id, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (accountManager_)
        return accountManager_->findCertificate(id, std::move(cb));
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
        emitSignal<libjami::ConfigurationSignal::CertificateStateChanged>(
            getAccountID(), cert_id, tls::TrustStore::statusToStr(status));
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
std::set<ID, std::less<>>
loadIdList(const std::string& path)
{
    std::set<ID, std::less<>> ids;
    std::ifstream file = fileutils::ifstream(path);
    if (!file.is_open()) {
        JAMI_DBG("Could not load %s", path.c_str());
        return ids;
    }
    std::string line;
    while (std::getline(file, line)) {
        if constexpr (std::is_same<ID, std::string>::value) {
            ids.emplace(std::move(line));
        } else if constexpr (std::is_integral<ID>::value) {
            ID vid;
            if (auto [p, ec] = std::from_chars(line.data(), line.data() + line.size(), vid, 16);
                ec == std::errc()) {
                ids.emplace(vid);
            }
        }
    }
    return ids;
}

template<typename List = std::set<dht::Value::Id>>
void
saveIdList(const std::string& path, const List& ids)
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
            saveIdList<decltype(this_.treatedMessages_)>(this_.cachePath_
                                                             + DIR_SEPARATOR_STR "treatedMessages",
                                                         this_.treatedMessages_);
        }
    });
}

bool
JamiAccount::isMessageTreated(std::string_view id)
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
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (not accountManager_ or not accountManager_->getInfo())
        return {};
    std::map<std::string, std::string> ids;
    for (const auto& d : accountManager_->getKnownDevices()) {
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
    dht::ThreadPool::io().run([cb, url, cachePath, cacheDuration, w = weak()]() {
        try {
            std::vector<uint8_t> data;
            {
                std::lock_guard<std::mutex> lk(fileutils::getFileLock(cachePath));
                data = fileutils::loadCacheFile(cachePath, cacheDuration);
            }
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
                    [cb, cachePath, w](const dht::http::Response& response) {
                        if (response.status_code == 200) {
                            try {
                                std::lock_guard<std::mutex> lk(fileutils::getFileLock(cachePath));
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
    const auto& conf = config();
    if (conf.proxyEnabled and proxyServerCached_.empty()) {
        JAMI_DEBUG("[Account {:s}] loading DHT proxy URL: {:s}", getAccountID(), conf.proxyListUrl);
        if (conf.proxyListUrl.empty()) {
            cb(getDhtProxyServer(conf.proxyServer));
        } else {
            loadCachedUrl(conf.proxyListUrl,
                          cachePath_ + DIR_SEPARATOR_STR "dhtproxylist",
                          std::chrono::hours(24 * 3),
                          [w = weak(), cb = std::move(cb)](const dht::http::Response& response) {
                              if (auto sthis = w.lock()) {
                                  if (response.status_code == 200) {
                                      cb(sthis->getDhtProxyServer(response.body));
                                  } else {
                                      cb(sthis->getDhtProxyServer(sthis->config().proxyServer));
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
        JAMI_DEBUG("Cache DHT proxy server: {}", proxyServerCached_);
        Json::Value node(Json::objectValue);
        node[getProxyConfigKey()] = proxyServerCached_;
        if (file.is_open())
            file << node;
        else
            JAMI_WARNING("Cannot write into {}", proxyCachePath);
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
    if (not config().displayName.empty())
        return "\"" + config().displayName + "\" " + uri;
    return uri;
}

std::string
JamiAccount::getToUri(const std::string& to) const
{
    return fmt::format("<sips:{};transport=tls>", to);
}

std::string
getIsComposing(const std::string& conversationId, bool isWriting)
{
    // implementing https://tools.ietf.org/rfc/rfc3994.txt
    return fmt::format("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                       "<isComposing><state>{}</state>{}</isComposing>",
                       isWriting ? "active"sv : "idle"sv,
                       conversationId.empty()
                           ? ""
                           : "<conversation>" + conversationId + "</conversation>");
}

std::string
getDisplayed(const std::string& conversationId, const std::string& messageId)
{
    // implementing https://tools.ietf.org/rfc/rfc5438.txt
    return fmt::format(
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
        "<imdn><message-id>{}</message-id>\n"
        "{}"
        "<display-notification><status><displayed/></status></display-notification>\n"
        "</imdn>",
        messageId,
        conversationId.empty() ? "" : "<conversation>" + conversationId + "</conversation>");
}

void
JamiAccount::setIsComposing(const std::string& conversationUri, bool isWriting)
{
    Uri uri(conversationUri);
    std::string conversationId = {};
    if (uri.scheme() == Uri::Scheme::SWARM)
        conversationId = uri.authority();
    const auto& uid = uri.authority();

    if (not isWriting and conversationUri != composingUri_)
        return;

    if (composingTimeout_) {
        composingTimeout_->cancel();
        composingTimeout_.reset();
    }
    if (isWriting) {
        if (not composingUri_.empty() and composingUri_ != conversationUri) {
            sendInstantMessage(uid,
                               {{MIME_TYPE_IM_COMPOSING, getIsComposing(conversationId, false)}});
            composingTime_ = std::chrono::steady_clock::time_point::min();
        }
        composingUri_.clear();
        composingUri_.insert(composingUri_.end(), conversationUri.begin(), conversationUri.end());
        auto now = std::chrono::steady_clock::now();
        if (now >= composingTime_ + COMPOSING_TIMEOUT) {
            sendInstantMessage(uid,
                               {{MIME_TYPE_IM_COMPOSING, getIsComposing(conversationId, true)}});
            composingTime_ = now;
        }
        composingTimeout_ = Manager::instance().scheduleTask(
            [w = weak(), uid, conversationId]() {
                if (auto sthis = w.lock()) {
                    sthis->sendInstantMessage(uid,
                                              {{MIME_TYPE_IM_COMPOSING,
                                                getIsComposing(conversationId, false)}});
                    sthis->composingUri_.clear();
                    sthis->composingTime_ = std::chrono::steady_clock::time_point::min();
                }
            },
            now + COMPOSING_TIMEOUT);
    } else {
        sendInstantMessage(uid, {{MIME_TYPE_IM_COMPOSING, getIsComposing(conversationId, false)}});
        composingUri_.clear();
        composingTime_ = std::chrono::steady_clock::time_point::min();
    }
}

bool
JamiAccount::setMessageDisplayed(const std::string& conversationUri,
                                 const std::string& messageId,
                                 int status)
{
    Uri uri(conversationUri);
    std::string conversationId = {};
    if (uri.scheme() == Uri::Scheme::SWARM)
        conversationId = uri.authority();
    auto sendMessage = status == (int) libjami::Account::MessageStates::DISPLAYED
                       && isReadReceiptEnabled();
    if (!conversationId.empty())
        sendMessage &= convModule()->onMessageDisplayed(getUsername(), conversationId, messageId);
    if (sendMessage)
        sendInstantMessage(uri.authority(),
                           {{MIME_TYPE_IMDN, getDisplayed(conversationId, messageId)}});
    return true;
}

std::string
JamiAccount::getContactHeader(const std::shared_ptr<SipTransport>& sipTransport)
{
    if (sipTransport and sipTransport->get() != nullptr) {
        auto transport = sipTransport->get();
        auto* td = reinterpret_cast<tls::AbstractSIPTransport::TransportData*>(transport);
        auto address = td->self->getLocalAddress().toString(true);
        bool reliable = transport->flag & PJSIP_TRANSPORT_RELIABLE;
        return fmt::format("\"{}\" <sips:{}{}{};transport={}>",
                           config().displayName,
                           id_.second->getId().toString(),
                           address.empty() ? "" : "@",
                           address,
                           reliable ? "tls" : "dtls");
    } else {
        JAMI_ERR("getContactHeader: no SIP transport provided");
        return fmt::format("\"{}\" <sips:{}@ring.dht>",
                           config().displayName,
                           id_.second->getId().toString());
    }
}

void
JamiAccount::addContact(const std::string& uri, bool confirmed)
{
    auto conversation = convModule()->getOneToOneConversation(uri);
    if (!confirmed && conversation.empty())
        conversation = convModule()->startConversation(ConversationMode::ONE_TO_ONE, uri);
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->addContact(uri, confirmed, conversation);
    else
        JAMI_WARN("[Account %s] addContact: account not loaded", getAccountID().c_str());
}

void
JamiAccount::removeContact(const std::string& uri, bool ban)
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->removeContact(uri, ban);
    else
        JAMI_WARN("[Account %s] removeContact: account not loaded", getAccountID().c_str());
}

bool
JamiAccount::updateConvForContact(const std::string& uri,
                                  const std::string& oldConv,
                                  const std::string& newConv)
{
    if (newConv != oldConv) {
        std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
        if (auto info = accountManager_->getInfo()) {
            auto urih = dht::InfoHash(uri);
            auto details = getContactDetails(uri);
            auto itDetails = details.find(libjami::Account::TrustRequest::CONVERSATIONID);
            if (itDetails != details.end() && itDetails->second != oldConv) {
                JAMI_DBG("Old conversation is not found in details %s", oldConv.c_str());
                return false;
            }
            info->contacts->updateConversation(urih, newConv);
            // Also decline trust request if there is one
            auto req = info->contacts->getTrustRequest(urih);
            if (req.find(libjami::Account::TrustRequest::CONVERSATIONID) != req.end()
                && req.at(libjami::Account::TrustRequest::CONVERSATIONID) == oldConv)
                accountManager_->discardTrustRequest(uri);
        }
        return true;
    }
    return false;
}

std::map<std::string, std::string>
JamiAccount::getContactDetails(const std::string& uri) const
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    return (accountManager_ and accountManager_->getInfo())
               ? accountManager_->getContactDetails(uri)
               : std::map<std::string, std::string> {};
}

std::vector<std::map<std::string, std::string>>
JamiAccount::getContacts() const
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (not accountManager_)
        return {};
    return accountManager_->getContacts();
}

/* trust requests */

std::vector<std::map<std::string, std::string>>
JamiAccount::getTrustRequests() const
{
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    return accountManager_ ? accountManager_->getTrustRequests()
                           : std::vector<std::map<std::string, std::string>> {};
}

bool
JamiAccount::acceptTrustRequest(const std::string& from, bool includeConversation)
{
    std::unique_lock<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_) {
        if (!accountManager_->acceptTrustRequest(from, includeConversation)) {
            // Note: unused for swarm
            // Typically the case where the trust request doesn't exists, only incoming DHT messages
            return accountManager_->addContact(from, true);
        }
        return true;
    }
    JAMI_WARN("[Account %s] acceptTrustRequest: account not loaded", getAccountID().c_str());
    return false;
}

bool
JamiAccount::discardTrustRequest(const std::string& from)
{
    // Remove 1:1 generated conv requests
    auto requests = getTrustRequests();
    for (const auto& req : requests) {
        if (req.at(libjami::Account::TrustRequest::FROM) == from) {
            convModule()->declineConversationRequest(
                req.at(libjami::Account::TrustRequest::CONVERSATIONID));
        }
    }

    // Remove trust request
    std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_)
        return accountManager_->discardTrustRequest(from);
    JAMI_WARNING("[Account {:s}] discardTrustRequest: account not loaded", getAccountID());
    return false;
}

void
JamiAccount::declineConversationRequest(const std::string& conversationId)
{
    auto peerId = convModule()->peerFromConversationRequest(conversationId);
    convModule()->declineConversationRequest(conversationId);
    if (!peerId.empty()) {
        std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
        if (auto info = accountManager_->getInfo()) {
            // Verify if we have a trust request with this peer + convId
            auto req = info->contacts->getTrustRequest(dht::InfoHash(peerId));
            if (req.find(libjami::Account::TrustRequest::CONVERSATIONID) != req.end()
                && req.at(libjami::Account::TrustRequest::CONVERSATIONID) == conversationId) {
                accountManager_->discardTrustRequest(peerId);
                JAMI_DEBUG("[Account {:s}] declined trust request with {:s}",
                           getAccountID(),
                           peerId);
            }
        }
    }
}

void
JamiAccount::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    // Here we cache payload sent by the client
    auto requestPath = cachePath_ + DIR_SEPARATOR_STR + "requests";
    fileutils::recursive_mkdir(requestPath, 0700);
    auto cachedFile = requestPath + DIR_SEPARATOR_STR + to;
    std::ofstream req = fileutils::ofstream(cachedFile, std::ios::trunc | std::ios::binary);
    if (!req.is_open()) {
        JAMI_ERR("Could not write data to %s", cachedFile.c_str());
        return;
    }

    if (not payload.empty()) {
        req.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    }

    if (payload.size() > 64000) {
        JAMI_WARN() << "Trust request is too big";
    }

    auto conversation = convModule()->getOneToOneConversation(to);
    if (conversation.empty())
        conversation = convModule()->startConversation(ConversationMode::ONE_TO_ONE, to);
    if (not conversation.empty()) {
        std::lock_guard<std::recursive_mutex> lock(configurationMutex_);
        if (accountManager_)
            accountManager_->sendTrustRequest(to, conversation, payload);
        else
            JAMI_WARN("[Account %s] sendTrustRequest: account not loaded", getAccountID().c_str());
    } else
        JAMI_WARN("[Account %s] sendTrustRequest: account not loaded", getAccountID().c_str());
}

void
JamiAccount::forEachDevice(const dht::InfoHash& to,
                           std::function<void(const std::shared_ptr<dht::crypto::PublicKey>&)>&& op,
                           std::function<void(bool)>&& end)
{
    accountManager_->forEachDevice(to, std::move(op), std::move(end));
}

uint64_t
JamiAccount::sendTextMessage(const std::string& to,
                             const std::string& deviceId,
                             const std::map<std::string, std::string>& payloads,
                             uint64_t refreshToken)
{
    Uri uri(to);
    if (uri.scheme() == Uri::Scheme::SWARM) {
        sendInstantMessage(uri.authority(), payloads);
        return 0;
    }

    std::string toUri;
    try {
        toUri = parseJamiUri(to);
    } catch (...) {
        JAMI_ERROR("Failed to send a text message due to an invalid URI {}", to);
        return 0;
    }
    if (payloads.size() != 1) {
        JAMI_ERROR("Multi-part im is not supported yet by JamiAccount");
        return 0;
    }
    return SIPAccountBase::sendTextMessage(toUri, deviceId, payloads, refreshToken);
}

void
JamiAccount::sendMessage(const std::string& to,
                         const std::map<std::string, std::string>& payloads,
                         uint64_t token,
                         bool retryOnTimeout,
                         bool onlyConnected,
                         const std::string& deviceId)
{
    std::string toUri;
    try {
        toUri = parseJamiUri(to);
    } catch (...) {
        JAMI_ERR("Failed to send a text message due to an invalid URI %s", to.c_str());
        if (!onlyConnected)
            messageEngine_.onMessageSent(to, token, false, deviceId);
        return;
    }
    if (payloads.size() != 1) {
        // Multi-part message
        // TODO: not supported yet
        JAMI_ERR("Multi-part im is not supported yet by JamiAccount");
        if (!onlyConnected)
            messageEngine_.onMessageSent(toUri, token, false);
        return;
    }

    auto confirm = std::make_shared<PendingConfirmation>();
    if (onlyConnected) {
        confirm->replied = true;
    }

    std::shared_ptr<std::set<DeviceId>> devices = std::make_shared<std::set<DeviceId>>();
    std::unique_lock<std::mutex> lk(sipConnsMtx_);

    for (auto it = sipConns_.begin(); it != sipConns_.end();) {
        auto& [key, value] = *it;
        if (key.first != to or value.empty()) {
            ++it;
            continue;
        }
        if (!deviceId.empty() && key.second.toString() != deviceId) {
            ++it;
            continue;
        }
        auto& conn = value.back();
        auto& channel = conn.channel;

        // Set input token into callback
        std::unique_ptr<TextMessageCtx> ctx {std::make_unique<TextMessageCtx>()};
        ctx->acc = weak();
        ctx->to = to;
        ctx->deviceId = DeviceId(deviceId);
        ctx->id = token;
        ctx->onlyConnected = onlyConnected;
        ctx->retryOnTimeout = retryOnTimeout;
        ctx->channel = channel;
        ctx->confirmation = confirm;

        try {
            auto res = sendSIPMessage(conn,
                                      to,
                                      ctx.release(),
                                      token,
                                      payloads,
                                      [](void* token, pjsip_event* event) {
                                          std::shared_ptr<TextMessageCtx> c {
                                              (TextMessageCtx*) token};
                                          auto code = event->body.tsx_state.tsx->status_code;
                                          runOnMainThread([c = std::move(c), code]() {
                                              if (c) {
                                                  auto acc = c->acc.lock();
                                                  if (not acc)
                                                      return;
                                                  acc->onSIPMessageSent(std::move(c), code);
                                              }
                                          });
                                      });
            if (!res) {
                if (!onlyConnected)
                    messageEngine_.onMessageSent(to, token, false, deviceId);
                ++it;
                continue;
            }
        } catch (const std::runtime_error& ex) {
            JAMI_WARN("%s", ex.what());
            if (!onlyConnected)
                messageEngine_.onMessageSent(to, token, false, deviceId);
            ++it;
            // Remove connection in incorrect state
            shutdownSIPConnection(channel, to, key.second);
            continue;
        }

        devices->emplace(key.second);
        ++it;
        if (key.second.toString() == deviceId) {
            return;
        }
    }
    lk.unlock();

    if (onlyConnected)
        return;

    if (deviceId.empty()) {
        auto toH = dht::InfoHash(toUri);
        // Find listening devices for this account
        accountManager_->forEachDevice(
            toH,
            [this, confirm, to, token, payloads, devices](
                const std::shared_ptr<dht::crypto::PublicKey>& dev) {
                // Test if already sent
                auto deviceId = dev->getLongId();
                if (devices->find(deviceId) != devices->end()) {
                    return;
                }
                if (deviceId.toString() == currentDeviceId()) {
                    devices->emplace(deviceId);
                    return;
                }

                // Else, ask for a channel and send a DHT message
                auto payload_type = payloads.cbegin()->first;
                requestSIPConnection(to, deviceId, payload_type);
                {
                    std::lock_guard<std::mutex> lock(messageMutex_);
                    sentMessages_[token].to.emplace(deviceId);
                }

                auto h = dht::InfoHash::get("inbox:" + dev->getId().toString());
                std::lock_guard<std::mutex> l(confirm->lock);
                auto list_token = dht_->listen<
                    dht::ImMessage>(h, [this, to, token, confirm](dht::ImMessage&& msg) {
                    // check expected message confirmation
                    if (msg.id != token)
                        return true;

                    {
                        std::lock_guard<std::mutex> lock(messageMutex_);
                        auto e = sentMessages_.find(msg.id);
                        if (e == sentMessages_.end()
                            or e->second.to.find(msg.owner->getLongId()) == e->second.to.end()) {
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
                auto now = clock::to_time_t(clock::now());
                dht_->putEncrypted(h,
                                   dev,
                                   dht::ImMessage(token,
                                                  std::string(payloads.begin()->first),
                                                  std::string(payloads.begin()->second),
                                                  now),
                                   [this, to, token, confirm, h](bool ok) {
                                       JAMI_DBG()
                                           << "[Account " << getAccountID() << "] [message "
                                           << token << "] Put encrypted " << (ok ? "ok" : "failed");
                                       if (not ok
                                           && connectionManager_ /* Check if not joining */) {
                                           std::unique_lock<std::mutex> l(confirm->lock);
                                           auto lt = confirm->listenTokens.find(h);
                                           if (lt != confirm->listenTokens.end()) {
                                               std::shared_future<size_t> tok = std::move(
                                                   lt->second);
                                               confirm->listenTokens.erase(lt);
                                               dht_->cancelListen(h, tok);
                                           }
                                           if (confirm->listenTokens.empty()
                                               and not confirm->replied) {
                                               l.unlock();
                                               messageEngine_.onMessageSent(to, token, false);
                                           }
                                       }
                                   });

                JAMI_DBG() << "[Account " << getAccountID() << "] [message " << token
                           << "] Sending message for device " << deviceId.toString();
            },
            [this, to, token, devices, confirm](bool ok) {
                if (devices->size() == 1 && devices->begin()->toString() == currentDeviceId()) {
                    // Current user only have devices, so no message are sent
                    {
                        std::lock_guard<std::mutex> l(confirm->lock);
                        for (auto& t : confirm->listenTokens)
                            dht_->cancelListen(t.first, std::move(t.second));
                        confirm->listenTokens.clear();
                        confirm->replied = true;
                    }
                    messageEngine_.onMessageSent(to, token, true);
                } else if (not ok) {
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

    } else {
        // Set message as not sent in order to be re-triggered
        messageEngine_.onMessageSent(to, token, false, deviceId);
        auto payload_type = payloads.cbegin()->first;
        requestSIPConnection(to, DeviceId(deviceId), payload_type);
    }
}

void
JamiAccount::onSIPMessageSent(const std::shared_ptr<TextMessageCtx>& ctx, int code)
{
    if (code == PJSIP_SC_OK) {
        std::unique_lock<std::mutex> l(ctx->confirmation->lock);
        ctx->confirmation->replied = true;
        l.unlock();
        if (!ctx->onlyConnected)
            messageEngine_.onMessageSent(ctx->to, ctx->id, true, ctx->deviceId.toString());
    } else {
        // Note: This can be called from pjsip's eventloop while
        // sipConnsMtx_ is locked. So we should retrigger the shutdown.
        auto acc = ctx->acc.lock();
        if (not acc)
            return;
        JAMI_WARN("Timeout when send a message, close current connection");
        shutdownSIPConnection(ctx->channel, ctx->to, ctx->deviceId);
        // This MUST be done after closing the connection to avoid race condition
        // with messageEngine_
        if (!ctx->onlyConnected)
            messageEngine_.onMessageSent(ctx->to, ctx->id, false);

        // In that case, the peer typically changed its connectivity.
        // After closing sockets with that peer, we try to re-connect to
        // that peer one time.
        if (ctx->retryOnTimeout)
            messageEngine_.onPeerOnline(ctx->to, false, ctx->deviceId.toString());
    }
}

void
JamiAccount::onIsComposing(const std::string& conversationId,
                           const std::string& peer,
                           bool isWriting)
{
    try {
        const std::string fromUri {parseJamiUri(peer)};
        emitSignal<libjami::ConfigurationSignal::ComposingStatusChanged>(accountID_,
                                                                         conversationId,
                                                                         peer,
                                                                         isWriting ? 1 : 0);
    } catch (...) {
        JAMI_ERR("[Account %s] Can't parse URI: %s", getAccountID().c_str(), peer.c_str());
    }
}

void
JamiAccount::getIceOptions(std::function<void(IceTransportOptions&&)> cb) noexcept
{
    storeActiveIpAddress([this, cb = std::move(cb)] {
        auto opts = SIPAccountBase::getIceOptions();
        auto publishedAddr = getPublishedIpAddress();

        if (publishedAddr) {
            auto interfaceAddr = ip_utils::getInterfaceAddr(getLocalInterface(),
                                                            publishedAddr.getFamily());
            if (interfaceAddr) {
                opts.accountLocalAddr = interfaceAddr;
                opts.accountPublicAddr = publishedAddr;
            }
        }
        if (cb)
            cb(std::move(opts));
    });
}

void
JamiAccount::storeActiveIpAddress(std::function<void()>&& cb)
{
    dht_->getPublicAddress([this, cb = std::move(cb)](std::vector<dht::SockAddr>&& results) {
        bool hasIpv4 {false}, hasIpv6 {false};
        for (auto& result : results) {
            auto family = result.getFamily();
            if (family == AF_INET) {
                if (not hasIpv4) {
                    hasIpv4 = true;
                    JAMI_DBG("[Account %s] Store DHT public IPv4 address : %s",
                             getAccountID().c_str(),
                             result.toString().c_str());
                    setPublishedAddress(*result.get());
                    if (upnpCtrl_) {
                        upnpCtrl_->setPublicAddress(*result.get());
                    }
                }
            } else if (family == AF_INET6) {
                if (not hasIpv6) {
                    hasIpv6 = true;
                    JAMI_DBG("[Account %s] Store DHT public IPv6 address : %s",
                             getAccountID().c_str(),
                             result.toString().c_str());
                    setPublishedAddress(*result.get());
                }
            }
            if (hasIpv4 and hasIpv6)
                break;
        }
        if (cb)
            cb();
    });
}

bool
JamiAccount::setPushNotificationToken(const std::string& token)
{
    if (SIPAccountBase::setPushNotificationToken(token)) {
        JAMI_WARNING("[Account {:s}] setPushNotificationToken: {:s}", getAccountID(), token);
        if (dht_)
            dht_->setPushNotificationToken(token);
        return true;
    }
    return false;
}

bool
JamiAccount::setPushNotificationTopic(const std::string& topic)
{
    if (SIPAccountBase::setPushNotificationTopic(topic)) {
        if (dht_)
            dht_->setPushNotificationTopic(topic);
        return true;
    }
    return false;
}

bool
JamiAccount::setPushNotificationConfig(const std::map<std::string, std::string>& data) {
    if (SIPAccountBase::setPushNotificationConfig(data)) {
        if (dht_) {
            dht_->setPushNotificationPlatform(config_->platform);
            dht_->setPushNotificationTopic(config_->notificationTopic);
            dht_->setPushNotificationToken(config_->deviceKey);
        }
        return true;
    }
    return false;
}


/**
 * To be called by clients with relevant data when a push notification is received.
 */
void
JamiAccount::pushNotificationReceived(const std::string& from,
                                      const std::map<std::string, std::string>& data)
{
    JAMI_WARNING("[Account {:s}] pushNotificationReceived: {:s}", getAccountID(), from);
    dht_->pushNotificationReceived(data);
}

std::string
JamiAccount::getUserUri() const
{
#ifdef HAVE_RINGNS
    if (not registeredName_.empty())
        return JAMI_URI_PREFIX + registeredName_;
#endif
    return JAMI_URI_PREFIX + config().username;
}

std::vector<libjami::Message>
JamiAccount::getLastMessages(const uint64_t& base_timestamp)
{
    return SIPAccountBase::getLastMessages(base_timestamp);
}

void
JamiAccount::startAccountPublish()
{
    AccountPeerInfo info_pub;
    info_pub.accountId = dht::InfoHash(accountManager_->getInfo()->accountId);
    info_pub.displayName = config().displayName;
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
                    emitSignal<libjami::PresenceSignal::NearbyPeerNotification>(getAccountID(),
                                                                                v.accountId
                                                                                    .toString(),
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
                            emitSignal<libjami::PresenceSignal::NearbyPeerNotification>(
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
    config_->activeCodecs = getActiveCodecs(MEDIA_ALL);
}

// Member management
void
JamiAccount::saveMembers(const std::string& convId, const std::vector<std::string>& members)
{
    convModule()->setConversationMembers(convId, members);
}

void
JamiAccount::sendInstantMessage(const std::string& convId,
                                const std::map<std::string, std::string>& msg)
{
    auto members = convModule()->getConversationMembers(convId);
    if (convId.empty() && members.empty()) {
        // TODO remove, it's for old API for contacts
        sendTextMessage(convId, "", msg);
        return;
    }
    for (const auto& m : members) {
        auto uri = m.at("uri");
        auto token = std::uniform_int_distribution<uint64_t> {1, JAMI_ID_MAX_VAL}(rand);
        // Announce to all members that a new message is sent
        sendMessage(uri, msg, token, false, true);
    }
}

bool
JamiAccount::handleMessage(const std::string& from, const std::pair<std::string, std::string>& m)
{
    if (m.first == MIME_TYPE_GIT) {
        Json::Value json;
        std::string err;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(m.second.data(), m.second.data() + m.second.size(), &json, &err)) {
            JAMI_ERR("Can't parse server response: %s", err.c_str());
            return false;
        }

        std::string deviceId = json["deviceId"].asString();
        std::string id = json["id"].asString();
        std::string commit = json["commit"].asString();
        // onNewCommit will do heavy stuff like fetching, avoid to block SIP socket
        dht::ThreadPool::io().run([w = weak(), from, deviceId, id, commit] {
            if (auto shared = w.lock()) {
                shared->convModule()->onNewCommit(from, deviceId, id, commit);
            }
        });
        return true;
    } else if (m.first == MIME_TYPE_INVITE) {
        convModule()->onNeedConversationRequest(from, m.second);
        return true;
    } else if (m.first == MIME_TYPE_INVITE_JSON) {
        Json::Value json;
        std::string err;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(m.second.data(), m.second.data() + m.second.size(), &json, &err)) {
            JAMI_ERR("Can't parse server response: %s", err.c_str());
            return false;
        }
        convModule()->onConversationRequest(from, json);
        return true;
    } else if (m.first == MIME_TYPE_IM_COMPOSING) {
        try {
            static const std::regex COMPOSING_REGEX("<state>\\s*(\\w+)\\s*<\\/state>");
            std::smatch matched_pattern;
            std::regex_search(m.second, matched_pattern, COMPOSING_REGEX);
            bool isComposing {false};
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                isComposing = matched_pattern[1] == "active";
            }
            static const std::regex CONVID_REGEX("<conversation>\\s*(\\w+)\\s*<\\/conversation>");
            std::regex_search(m.second, matched_pattern, CONVID_REGEX);
            std::string conversationId = "";
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                conversationId = matched_pattern[1];
            }
            onIsComposing(conversationId, from, isComposing);
            return true;
        } catch (const std::exception& e) {
            JAMI_WARN("Error parsing composing state: %s", e.what());
        }
    } else if (m.first == MIME_TYPE_IMDN) {
        try {
            static const std::regex IMDN_MSG_ID_REGEX("<message-id>\\s*(\\w+)\\s*<\\/message-id>");
            static const std::regex IMDN_STATUS_REGEX("<status>\\s*<(\\w+)\\/>\\s*<\\/status>");
            std::smatch matched_pattern;

            std::regex_search(m.second, matched_pattern, IMDN_MSG_ID_REGEX);
            std::string messageId;
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                messageId = matched_pattern[1];
            } else {
                JAMI_WARN("Message displayed: can't parse message ID");
                return false;
            }

            std::regex_search(m.second, matched_pattern, IMDN_STATUS_REGEX);
            bool isDisplayed {false};
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                isDisplayed = matched_pattern[1] == "displayed";
            } else {
                JAMI_WARN("Message displayed: can't parse status");
                return false;
            }

            static const std::regex CONVID_REGEX("<conversation>\\s*(\\w+)\\s*<\\/conversation>");
            std::regex_search(m.second, matched_pattern, CONVID_REGEX);
            std::string conversationId = "";
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                conversationId = matched_pattern[1];
            }

            if (!isReadReceiptEnabled())
                return true;
            if (conversationId.empty()) // Old method
                messageEngine_.onMessageDisplayed(from, from_hex_string(messageId), isDisplayed);
            else if (isDisplayed) {
                if (convModule()->onMessageDisplayed(from, conversationId, messageId)) {
                    JAMI_DBG() << "[message " << messageId << "] Displayed by peer";
                    emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                        accountID_,
                        conversationId,
                        from,
                        messageId,
                        static_cast<int>(libjami::Account::MessageStates::DISPLAYED));
                }
            }
            return true;
        } catch (const std::exception& e) {
            JAMI_WARN("Error parsing display notification: %s", e.what());
        }
    }

    return false;
}

void
JamiAccount::callConnectionClosed(const DeviceId& deviceId, bool eraseDummy)
{
    std::function<void(const DeviceId&, bool)> cb;
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
    dht::ThreadPool::io().run(
        [w = weak(), cb = std::move(cb), id = deviceId, erase = std::move(eraseDummy)] {
            if (auto acc = w.lock()) {
                if (cb)
                    cb(id, erase);
            }
        });
}

void
JamiAccount::requestSIPConnection(const std::string& peerId,
                                  const DeviceId& deviceId,
                                  const std::string& connectionType,
                                  bool forceNewConnection,
                                  const std::shared_ptr<SIPCall>& pc)
{
    JAMI_DBG("[Account %s] Request SIP connection to peer %s on device %s",
             getAccountID().c_str(),
             peerId.c_str(),
             deviceId.to_c_str());

    // If a connection already exists or is in progress, no need to do this
    std::lock_guard<std::mutex> lk(sipConnsMtx_);
    auto id = std::make_pair(peerId, deviceId);

    if (sipConns_.find(id) != sipConns_.end()) {
        JAMI_DBG("[Account %s] A SIP connection with %s already exists",
                 getAccountID().c_str(),
                 deviceId.to_c_str());
        return;
    }
    // If not present, create it
    std::lock_guard<std::mutex> lkCM(connManagerMtx_);
    if (!connectionManager_)
        return;
    // Note, Even if we send 50 "sip" request, the connectionManager_ will only use one socket.
    // however, this will still ask for multiple channels, so only ask
    // if there is no pending request
    if (!forceNewConnection && connectionManager_->isConnecting(deviceId, "sip")) {
        JAMI_INFO("[Account %s] Already connecting to %s",
                  getAccountID().c_str(),
                  deviceId.to_c_str());
        return;
    }
    JAMI_INFO("[Account %s] Ask %s for a new SIP channel",
              getAccountID().c_str(),
              deviceId.to_c_str());
    connectionManager_->connectDevice(
        deviceId,
        "sip",
        [w = weak(), id = std::move(id), pc = std::move(pc)](std::shared_ptr<ChannelSocket> socket,
                                                             const DeviceId&) {
            if (socket)
                return;
            auto shared = w.lock();
            if (!shared)
                return;
            // If this is triggered, this means that the
            // connectDevice didn't get any response from the DHT.
            // Stop searching pending call.
            shared->callConnectionClosed(id.second, true);
            if (pc)
                pc->onFailure();
        },
        false,
        forceNewConnection,
        connectionType);
}

void
JamiAccount::sendProfile(const std::string& convId,
                         const std::string& peerUri,
                         const std::string& deviceId)
{
    // VCard sync for peerUri
    if (not needToSendProfile(peerUri, deviceId)) {
        JAMI_DEBUG("Peer {} already got an up-to-date vcard", peerUri);
        return;
    }
    // We need a new channel
    transferFile(convId, profilePath(), deviceId, "profile.vcf", "");
    // Mark the VCard as sent
    auto sendDir = fmt::format("{}/{}/vcard/{}",
                               fileutils::get_cache_dir(),
                               getAccountID(),
                               peerUri);
    auto path = fmt::format("{}/{}", sendDir, deviceId);
    fileutils::recursive_mkdir(sendDir);
    std::lock_guard<std::mutex> lock(fileutils::getFileLock(path));
    if (fileutils::isFile(path))
        return;
    fileutils::ofstream(path);
}

bool
JamiAccount::needToSendProfile(const std::string& peerUri, const std::string& deviceId)
{
    auto currentSha3 = fileutils::sha3File(fmt::format("{}/profile.vcf", idPath_));
    std::string previousSha3 {};
    auto vCardPath = fmt::format("{}/vcard", cachePath_);
    auto sha3Path = fmt::format("{}/sha3", vCardPath);
    fileutils::check_dir(vCardPath.c_str(), 0700);
    try {
        previousSha3 = fileutils::loadTextFile(sha3Path);
    } catch (...) {
        fileutils::saveFile(sha3Path, {currentSha3.begin(), currentSha3.end()}, 0600);
        return true;
    }
    if (currentSha3 != previousSha3) {
        // Incorrect sha3 stored. Update it
        fileutils::removeAll(vCardPath, true);
        fileutils::check_dir(vCardPath.c_str(), 0700);
        fileutils::saveFile(sha3Path, {currentSha3.begin(), currentSha3.end()}, 0600);
        return true;
    }
    fileutils::recursive_mkdir(fmt::format("{}/{}/", vCardPath, peerUri));
    return not fileutils::isFile(fmt::format("{}/{}/{}", vCardPath, peerUri, deviceId));
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
    if (!channel)
        throw std::runtime_error(
            "A SIP transport exists without Channel, this is a bug. Please report");
    auto remote_address = channel->getRemoteAddress();
    if (!remote_address)
        return false;

    // Build SIP Message
    // "deviceID@IP"
    auto toURI = getToUri(fmt::format("{}@{}", to, remote_address.toString(true)));
    std::string from = getFromUri();
    pjsip_tx_data* tdata;

    // Build SIP message
    constexpr pjsip_method msg_method = {PJSIP_OTHER_METHOD,
                                         sip_utils::CONST_PJ_STR(sip_utils::SIP_METHODS::MESSAGE)};
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
    sip_utils::addUserAgentHeader(getUserAgentName(), tdata);

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
        auto status = pjsip_endpt_send_request(shared->link_.getEndpoint(), tdata, -1, ctx, cb);
        if (status != PJ_SUCCESS)
            JAMI_ERR("Unable to send request: %s", sip_utils::sip_strerror(status).c_str());
    });
    return true;
}

void
JamiAccount::clearProfileCache(const std::string& peerUri)
{
    fileutils::removeAll(fmt::format("{}/vcard/{}", cachePath_, peerUri));
}

std::string
JamiAccount::profilePath() const
{
    return idPath_ + DIR_SEPARATOR_STR + "profile.vcf";
}

void
JamiAccount::cacheSIPConnection(std::shared_ptr<ChannelSocket>&& socket,
                                const std::string& peerId,
                                const DeviceId& deviceId)
{
    std::unique_lock<std::mutex> lk(sipConnsMtx_);
    // Verify that the connection is not already cached
    SipConnectionKey key(peerId, deviceId);
    auto& connections = sipConns_[key];
    auto conn = std::find_if(connections.begin(), connections.end(), [&](const auto& v) {
        return v.channel == socket;
    });
    if (conn != connections.end()) {
        JAMI_WARN("[Account %s] Channel socket already cached with this peer",
                  getAccountID().c_str());
        return;
    }

    // Convert to SIP transport
    auto onShutdown = [w = weak(), peerId, key, socket]() {
        runOnMainThread([w = std::move(w), peerId, key, socket] {
            auto shared = w.lock();
            if (!shared)
                return;
            shared->shutdownSIPConnection(socket, key.first, key.second);
            // The connection can be closed during the SIP initialization, so
            // if this happens, the request should be re-sent to ask for a new
            // SIP channel to make the call pass through
            shared->callConnectionClosed(key.second, false);
        });
    };
    auto sip_tr = link_.sipTransportBroker->getChanneledTransport(shared(),
                                                                  socket,
                                                                  std::move(onShutdown));
    if (!sip_tr) {
        JAMI_ERROR("No channeled transport found");
        return;
    }
    // Store the connection
    connections.emplace_back(SipConnection {sip_tr, socket});
    JAMI_WARNING("[Account {:s}] New SIP channel opened with {:s}",
                 getAccountID(),
                 deviceId.to_c_str());
    lk.unlock();

    convModule()->syncConversations(peerId, deviceId.toString());
    // Retry messages

    messageEngine_.onPeerOnline(peerId);
    messageEngine_.onPeerOnline(peerId, true, deviceId.toString());

    // Connect pending calls
    forEachPendingCall(deviceId, [&](const auto& pc) {
        if (pc->getConnectionState() != Call::ConnectionState::TRYING
            and pc->getConnectionState() != Call::ConnectionState::PROGRESSING)
            return;
        pc->setSipTransport(sip_tr, getContactHeader(sip_tr));
        pc->setState(Call::ConnectionState::PROGRESSING);
        if (auto remote_address = socket->getRemoteAddress()) {
            try {
                onConnectedOutgoingCall(pc, peerId, remote_address);
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

void
JamiAccount::shutdownSIPConnection(const std::shared_ptr<ChannelSocket>& channel,
                                   const std::string& peerId,
                                   const DeviceId& deviceId)
{
    std::unique_lock<std::mutex> lk(sipConnsMtx_);
    SipConnectionKey key(peerId, deviceId);
    auto it = sipConns_.find(key);
    if (it != sipConns_.end()) {
        auto& conns = it->second;
        conns.erase(std::remove_if(conns.begin(),
                                   conns.end(),
                                   [&](auto v) { return v.channel == channel; }),
                    conns.end());
        if (conns.empty())
            sipConns_.erase(it);
    }
    lk.unlock();
    // Shutdown after removal to let the callbacks do stuff if needed
    if (channel)
        channel->shutdown();
}

std::string_view
JamiAccount::currentDeviceId() const
{
    if (!accountManager_ or not accountManager_->getInfo())
        return {};
    return accountManager_->getInfo()->deviceId;
}

std::shared_ptr<TransferManager>
JamiAccount::dataTransfer(const std::string& id)
{
    if (id.empty())
        return nonSwarmTransferManager_;
    return convModule()->dataTransfer(id);
}

void
JamiAccount::monitor()
{
    JAMI_DEBUG("[Account {:s}] Monitor connections", getAccountID());
    JAMI_DEBUG("[Account {:s}] Using proxy: {:s}", getAccountID(), proxyServerCached_);

    convModule()->monitor();
    /*std::lock_guard<std::mutex> lkCM(connManagerMtx_);
    if (connectionManager_)
        connectionManager_->monitor();*/
}

void
JamiAccount::sendFile(const std::string& conversationId,
                      const std::string& path,
                      const std::string& name,
                      const std::string& replyTo)
{
    if (!fileutils::isFile(path)) {
        JAMI_ERR() << "invalid filename '" << path << "'";
        return;
    }
    // NOTE: this sendMessage is in a computation thread because
    // sha3sum can take quite some time to computer if the user decide
    // to send a big file
    dht::ThreadPool::computation().run([w = weak(), conversationId, path, name, replyTo]() {
        if (auto shared = w.lock()) {
            Json::Value value;
            auto tid = jami::generateUID();
            value["tid"] = std::to_string(tid);
            std::size_t found = path.find_last_of(DIR_SEPARATOR_CH);
            value["displayName"] = name.empty() ? path.substr(found + 1) : name;
            value["totalSize"] = std::to_string(fileutils::size(path));
            value["sha3sum"] = fileutils::sha3File(path);
            value["type"] = "application/data-transfer+json";

            shared->convModule()
                ->sendMessage(conversationId,
                              std::move(value),
                              replyTo,
                              true,
                              [accId = shared->getAccountID(), conversationId, tid, path](
                                  const std::string& commitId) {
                                  // Create a symlink to answer to re-ask
                                  auto filelinkPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                                                      + accId + DIR_SEPARATOR_STR
                                                      + "conversation_data" + DIR_SEPARATOR_STR
                                                      + conversationId + DIR_SEPARATOR_STR
                                                      + commitId + "_" + std::to_string(tid);
                                  auto extension = fileutils::getFileExtension(path);
                                  if (!extension.empty())
                                      filelinkPath += "." + extension;
                                  if (path != filelinkPath && !fileutils::isSymLink(filelinkPath))
                                      fileutils::createFileLink(filelinkPath, path, true);
                              });
        }
    });
}

void
JamiAccount::transferFile(const std::string& conversationId,
                          const std::string& path,
                          const std::string& deviceId,
                          const std::string& fileId,
                          const std::string& interactionId,
                          size_t start,
                          size_t end)
{
    auto channelName = conversationId.empty() ? fmt::format("{}profile.vcf", DATA_TRANSFER_URI)
                                              : fmt::format("{}{}/{}/{}",
                                                            DATA_TRANSFER_URI,
                                                            conversationId,
                                                            currentDeviceId(),
                                                            fileId);
    std::lock_guard<std::mutex> lkCM(connManagerMtx_);
    if (!connectionManager_)
        return;
    connectionManager_->connectDevice(
        DeviceId(deviceId),
        channelName,
        [this, conversationId, path = std::move(path), fileId, interactionId, start, end](
            std::shared_ptr<ChannelSocket> socket, const DeviceId&) {
            if (!socket)
                return;
            dht::ThreadPool::io().run([w = weak(),
                                       path = std::move(path),
                                       socket = std::move(socket),
                                       conversationId = std::move(conversationId),
                                       fileId,
                                       interactionId,
                                       start,
                                       end] {
                if (auto shared = w.lock())
                    if (auto dt = shared->dataTransfer(conversationId))
                        dt->transferFile(socket, fileId, interactionId, path, start, end);
            });
        });
}

void
JamiAccount::askForFileChannel(const std::string& conversationId,
                               const std::string& deviceId,
                               const std::string& interactionId,
                               const std::string& fileId,
                               size_t start,
                               size_t end)
{
    auto tryDevice = [=](const auto& did) {
        std::lock_guard<std::mutex> lkCM(connManagerMtx_);
        if (!connectionManager_)
            return;

        auto channelName = fmt::format("{}{}/{}/{}",
                                       DATA_TRANSFER_URI,
                                       conversationId,
                                       currentDeviceId(),
                                       fileId);
        if (start != 0 || end != 0) {
            channelName += "?start=" + std::to_string(start) + "&end=" + std::to_string(end);
        }
        // We can avoid to negotiate new sessions, as the file notif
        // probably come from an online device or last connected device.
        connectionManager_->connectDevice(
            did,
            channelName,
            [this, conversationId, fileId, interactionId](std::shared_ptr<ChannelSocket> channel,
                                                          const DeviceId&) {
                if (!channel)
                    return;
                dht::ThreadPool::io().run(
                    [w = weak(), conversationId, channel, fileId, interactionId] {
                        auto shared = w.lock();
                        if (!shared)
                            return;
                        auto dt = shared->dataTransfer(conversationId);
                        if (!dt)
                            return;
                        if (interactionId.empty())
                            dt->onIncomingProfile(channel);
                        else
                            dt->onIncomingFileTransfer(fileId, channel);
                    });
            },
            false);
    };

    if (!deviceId.empty()) {
        // Only ask for device
        tryDevice(DeviceId(deviceId));
    } else {
        // Only ask for connected devices. For others we will try
        // on new peer online
        for (const auto& m : convModule()->getConversationMembers(conversationId)) {
            accountManager_->forEachDevice(dht::InfoHash(m.at("uri")),
                                           [tryDevice = std::move(tryDevice)](
                                               const std::shared_ptr<dht::crypto::PublicKey>& dev) {
                                               tryDevice(dev->getLongId());
                                           });
        }
    }
}

void
JamiAccount::askForProfile(const std::string& conversationId,
                           const std::string& deviceId,
                           const std::string& memberUri)
{
    std::lock_guard<std::mutex> lkCM(connManagerMtx_);
    if (!connectionManager_)
        return;

    auto channelName = fmt::format("{}{}/profile/{}.vcf",
                                   DATA_TRANSFER_URI,
                                   conversationId,
                                   memberUri);
    // We can avoid to negotiate new sessions, as the file notif
    // probably come from an online device or last connected device.
    connectionManager_->connectDevice(
        DeviceId(deviceId),
        channelName,
        [this, conversationId](std::shared_ptr<ChannelSocket> channel, const DeviceId&) {
            if (!channel)
                return;
            dht::ThreadPool::io().run([w = weak(), conversationId, channel] {
                if (auto shared = w.lock())
                    if (auto dt = shared->dataTransfer(conversationId))
                        dt->onIncomingProfile(channel);
            });
        },
        false);
}

void
JamiAccount::initConnectionManager()
{
    if (!connectionManager_) {
        connectionManager_ = std::make_unique<ConnectionManager>(*this);
        channelHandlers_[Uri::Scheme::SWARM]
            = std::make_unique<SwarmChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::GIT]
            = std::make_unique<ConversationChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::SYNC]
            = std::make_unique<SyncChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::DATA_TRANSFER]
            = std::make_unique<TransferChannelHandler>(shared(), *connectionManager_.get());
    }
}

} // namespace jami
