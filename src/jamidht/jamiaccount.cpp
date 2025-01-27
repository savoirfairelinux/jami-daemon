/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
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
#include "conversation_channel_handler.h"
#include "sync_channel_handler.h"
#include "message_channel_handler.h"
#include "transfer_channel_handler.h"
#include "swarm/swarm_channel_handler.h"
#include "jami/media_const.h"

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "connectivity/sip_utils.h"

#include "uri.h"

#include "client/ring_signal.h"
#include "jami/call_const.h"
#include "jami/account_const.h"

#include "system_codec_container.h"

#include "account_schema.h"
#include "manager.h"
#include "connectivity/utf8_utils.h"
#include "connectivity/ip_utils.h"

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
#include "json_utils.h"

#include "libdevcrypto/Common.h"
#include "base64.h"
#include "vcard.h"
#include "im/instant_messaging.h"

#include <dhtnet/ice_transport.h>
#include <dhtnet/ice_transport_factory.h>
#include <dhtnet/upnp/upnp_control.h>
#include <dhtnet/multiplexed_socket.h>
#include <dhtnet/certstore.h>

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
static constexpr const char MIME_TYPE_PIDF[] {"application/pidf+xml"};
static constexpr const char MIME_TYPE_INVITE_JSON[] {"application/invite+json"};
static constexpr const char DEVICE_ID_PATH[] {"ring_device"};
static constexpr auto TREATED_PATH = "treatedImMessages"sv;

// Used to pass info to a pjsip callback (pjsip_endpt_send_request)
struct TextMessageCtx
{
    std::weak_ptr<JamiAccount> acc;
    std::string to;
    DeviceId deviceId;
    uint64_t id;
    bool retryOnTimeout;
    std::shared_ptr<dhtnet::ChannelSocket> channel;
    bool onlyConnected;
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
        throw std::invalid_argument("ID must be a Jami infohash");

    const std::string_view toUri = sufix.substr(0, 40);
    if (std::find_if_not(toUri.cbegin(), toUri.cend(), ::isxdigit) != toUri.cend())
        throw std::invalid_argument("ID must be a Jami infohash");
    return toUri;
}

static constexpr const char*
dhtStatusStr(dht::NodeStatus status)
{
    return status == dht::NodeStatus::Connected
               ? "connected"
               : (status == dht::NodeStatus::Connecting ? "connecting" : "disconnected");
}

JamiAccount::JamiAccount(const std::string& accountId)
    : SIPAccountBase(accountId)
    , cachePath_(fileutils::get_cache_dir() / accountId)
    , dataPath_(cachePath_ / "values")
    , certStore_ {std::make_unique<dhtnet::tls::CertificateStore>(idPath_, Logger::dhtLogger())}
    , dht_(new dht::DhtRunner)
    , treatedMessages_(cachePath_ / TREATED_PATH)
    , connectionManager_ {}
    , nonSwarmTransferManager_()
{}

JamiAccount::~JamiAccount() noexcept
{
    if (dht_)
        dht_->join();
}

void
JamiAccount::shutdownConnections()
{
    JAMI_DBG("[Account %s] Shutdown connections", getAccountID().c_str());

    decltype(gitServers_) gservers;
    {
        std::lock_guard lk(gitServersMtx_);
        gservers = std::move(gitServers_);
    }
    for (auto& [_id, gs] : gservers)
        gs->stop();
    {
        std::lock_guard lk(connManagerMtx_);
        // Just move destruction on another thread.
        dht::ThreadPool::io().run([conMgr = std::make_shared<decltype(connectionManager_)>(
                                       std::move(connectionManager_))] {});
        connectionManager_.reset();
        channelHandlers_.clear();
    }
    if (convModule_) {
        convModule_->shutdownConnections();
    }

    std::lock_guard lk(sipConnsMtx_);
    sipConns_.clear();
}

void
JamiAccount::flush()
{
    // Class base method
    SIPAccountBase::flush();

    dhtnet::fileutils::removeAll(cachePath_);
    dhtnet::fileutils::removeAll(dataPath_);
    dhtnet::fileutils::removeAll(idPath_, true);
}

std::shared_ptr<SIPCall>
JamiAccount::newIncomingCall(const std::string& from,
                             const std::vector<libjami::MediaMap>& mediaList,
                             const std::shared_ptr<SipTransport>& sipTransp)
{
    JAMI_DEBUG("New incoming call from {:s} with {:d} media", from, mediaList.size());

    if (sipTransp) {
        auto call = Manager::instance().callFactory.newSipCall(shared(),
                                                               Call::CallType::INCOMING,
                                                               mediaList);
        call->setPeerUri(JAMI_URI_PREFIX + from);
        call->setPeerNumber(from);

        call->setSipTransport(sipTransp, getContactHeader(sipTransp));

        return call;
    }

    JAMI_ERR("newIncomingCall: unable to find matching call for %s", from.c_str());
    return nullptr;
}

std::shared_ptr<Call>
JamiAccount::newOutgoingCall(std::string_view toUrl, const std::vector<libjami::MediaMap>& mediaList)
{
    auto uri = Uri(toUrl);
    if (uri.scheme() == Uri::Scheme::SWARM || uri.scheme() == Uri::Scheme::RENDEZVOUS) {
        // NOTE: In this case newOutgoingCall can act as "unholdConference" and just attach the
        // host to the current hosted conference. So, no call will be returned in that case.
        return newSwarmOutgoingCallHelper(uri, mediaList);
    }

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

    connectionManager_->getIceOptions([call, w = weak(), uri = std::move(uri)](auto&& opts) {
        if (call->isIceEnabled()) {
            if (not call->createIceMediaTransport(false)
                or not call->initIceMediaTransport(true,
                                                   std::forward<dhtnet::IceTransportOptions>(opts))) {
                return;
            }
        }
        auto shared = w.lock();
        if (!shared)
            return;
        JAMI_DBG() << "New outgoing call with " << uri.toString();
        call->setPeerNumber(uri.authority());
        call->setPeerUri(uri.toString());

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
                                 [wthis_ = weak(), call](const std::string& regName, const std::string& address,
                                                         NameDirectory::Response response) {
                                     // we may run inside an unknown thread, but following code must
                                     // be called in main thread
                                     runOnMainThread([wthis_, regName, address, response, call]() {
                                         if (response != NameDirectory::Response::found) {
                                             call->onFailure(EINVAL);
                                             return;
                                         }
                                         if (auto sthis = wthis_.lock()) {
                                             try {
                                                 sthis->startOutgoingCall(call, regName);
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

std::shared_ptr<SIPCall>
JamiAccount::newSwarmOutgoingCallHelper(const Uri& uri,
                                        const std::vector<libjami::MediaMap>& mediaList)
{
    JAMI_DEBUG("[Account {}] Calling conversation {}", getAccountID(), uri.authority());
    return convModule()->call(
        uri.authority(),
        mediaList,
        [this, uri](const auto& accountUri, const auto& deviceId, const auto& call) {
            if (!call)
                return;
            std::unique_lock lkSipConn(sipConnsMtx_);
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
                    // link (connectivity change, killed by the operating system, etc)
                    // Here, we don't need to do anything, the TLS will fail and will delete
                    // the cached transport
                    continue;
                }
            }
            lkSipConn.unlock();
            {
                std::lock_guard lkP(pendingCallsMutex_);
                pendingCalls_[deviceId].emplace_back(call);
            }

            // Else, ask for a channel (for future calls/text messages)
            auto type = call->hasVideo() ? "videoCall" : "audioCall";
            JAMI_WARNING("[call {}] No channeled socket with this peer. Send request",
                         call->getCallId());
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

    // Avoid concurrent checks in this part
    std::lock_guard lk(rdvMtx_);
    auto isNotHosting = !convModule()->isHosting(conversationId, confId);
    if (confId == "0") {
        auto currentCalls = convModule()->getActiveCalls(conversationId);
        if (!currentCalls.empty()) {
            confId = currentCalls[0]["id"];
            isNotHosting = false;
        } else {
            confId = callId;
            JAMI_DEBUG("No active call to join, create conference");
        }
    }
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
        JAMI_ERROR("Call {} not found", callId);
        return;
    }

    if (isNotHosting && !canHost) {
        JAMI_DEBUG("Request for hosting a conference declined");
        Manager::instance().hangupCall(getAccountID(), callId);
        return;
    }

    std::shared_ptr<Conference> conf;
    std::vector<libjami::MediaMap> currentMediaList;
    if (!isNotHosting) {
        conf = getConference(confId);
        if (!conf) {
            JAMI_ERROR("Conference {} not found", confId);
            return;
        }
        for (const auto& m : conf->currentMediaList()) {
            if (m.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE)
                    == libjami::Media::MediaAttributeValue::VIDEO
                && !call->hasVideo()) {
                continue;
            }
            currentMediaList.emplace_back(m);
        }
    }
    if (currentMediaList.empty()) {
        currentMediaList = MediaAttribute::mediaAttributesToMediaMaps(
            createDefaultMediaList(call->hasVideo(), true));
    }
    Manager::instance().answerCall(*call, currentMediaList);

    if (isNotHosting) {
        JAMI_DEBUG("Creating conference for swarm {} with ID {}", conversationId, confId);
        // Create conference and host it.
        convModule()->hostConference(conversationId, confId, callId);
    } else {
        JAMI_DEBUG("Adding participant {} for swarm {} with ID {}", callId, conversationId, confId);
        Manager::instance().addAudio(*call);
        conf->addSubCall(callId);
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
    setCertificateStatus(toUri, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);

    call->setState(Call::ConnectionState::TRYING);
    std::weak_ptr<SIPCall> wCall = call;

#if HAVE_RINGNS
    accountManager_->lookupAddress(toUri,
                                   [wCall](const std::string& regName, const std::string& address,
                                           const NameDirectory::Response& response) {
                                       if (response == NameDirectory::Response::found)
                                           if (auto call = wCall.lock()) {
                                               call->setPeerRegisteredName(regName);
                                           }
                                   });
#endif

    dht::InfoHash peer_account(toUri);

    // Call connected devices
    std::set<DeviceId> devices;
    std::unique_lock lkSipConn(sipConnsMtx_);
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
                std::lock_guard lk(pendingCallsMutex_);
                pendingCalls_[deviceId].emplace_back(dev_call);
            }

            JAMI_WARNING("[call {}] No channeled socket with this peer. Send request",
                         call->getCallId());
            // Else, ask for a channel (for future calls/text messages)
            auto type = call->hasVideo() ? "videoCall" : "audioCall";
            requestSIPConnection(toUri, deviceId, type, true, dev_call);
        };

    std::vector<std::shared_ptr<dhtnet::ChannelSocket>> channels;
    for (auto& [key, value] : sipConns_) {
        if (key.first != toUri)
            continue;
        if (value.empty())
            continue;
        auto& sipConn = value.back();

        if (!sipConn.channel) {
            JAMI_WARNING("A SIP transport exists without Channel, this is a bug. Please report");
            continue;
        }

        auto transport = sipConn.transport;
        auto remote_address = sipConn.channel->getRemoteAddress();
        if (!transport or !remote_address)
            continue;

        channels.emplace_back(sipConn.channel);

        JAMI_WARNING("[call {}] A channeled socket is detected with this peer.", call->getCallId());

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
            std::lock_guard lk(onConnectionClosedMtx_);
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
                std::lock_guard lk(onConnectionClosedMtx_);
                onConnectionClosed_[deviceId] = sendRequest;
            }
            sendRequest(deviceId, false);
        },
        [wCall](bool ok) {
            if (not ok) {
                if (auto call = wCall.lock()) {
                    JAMI_WARNING("[call:{}] No devices found", call->getCallId());
                    // Note: if a P2P connection exists, the call will be at least in CONNECTING
                    if (call->getConnectionState() == Call::ConnectionState::TRYING)
                        call->onFailure(static_cast<int>(std::errc::no_such_device_or_address));
                }
            }
        });
}

void
JamiAccount::onConnectedOutgoingCall(const std::shared_ptr<SIPCall>& call,
                                     const std::string& to_id,
                                     dhtnet::IpAddr target)
{
    if (!call)
        return;
    JAMI_LOG("[call:{}] Outgoing call connected to {}", call->getCallId(), to_id);

    const auto localAddress = dhtnet::ip_utils::getInterfaceAddr(getLocalInterface(),
                                                                 target.getFamily());

    dhtnet::IpAddr addrSdp = getPublishedSameasLocal()
                                 ? localAddress
                                 : connectionManager_->getPublishedIpAddress(target.getFamily());

    // fallback on local address
    if (not addrSdp)
        addrSdp = localAddress;

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    if (!getSystemCodecContainer()->searchCodecByName("PCMA", jami::MEDIA_AUDIO))
        JAMI_WARNING("[call:{}] Unable to instantiate codec for early media", call->getCallId());

    // Building the local SDP offer
    auto& sdp = call->getSDP();

    sdp.setPublishedIP(addrSdp);

    auto mediaAttrList = call->getMediaAttributeList();

    if (mediaAttrList.empty()) {
        JAMI_ERROR("[call:{}] No media. Abort!", call->getCallId());
        return;
    }

    if (not sdp.createOffer(mediaAttrList)) {
        JAMI_ERROR("[call:{}] Unable to send outgoing INVITE request for new call",
                   call->getCallId());
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
            JAMI_ERROR("[call:{}] Unable to send outgoing INVITE request for new call",
                       call->getCallId());
        }
    });
}

bool
JamiAccount::SIPStartCall(SIPCall& call, const dhtnet::IpAddr& target)
{
    JAMI_LOG("[call:{}] Start SIP call", call.getCallId());

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

    JAMI_LOG("[call:{}] Contact header: {} / {} -> {} / {}",
             call.getCallId(),
             contact,
             from,
             toUri,
             targetStr);

    auto local_sdp = call.getSDP().getLocalSdpSession();
    pjsip_dialog* dialog {nullptr};
    pjsip_inv_session* inv {nullptr};
    if (!CreateClientDialogAndInvite(&pjFrom, &pjContact, &pjTo, &pjTarget, local_sdp, &dialog, &inv))
        return false;

    inv->mod_data[link_.getModId()] = &call;
    call.setInviteSession(inv);

    pjsip_tx_data* tdata;

    if (pjsip_inv_invite(call.inviteSession_.get(), &tdata) != PJ_SUCCESS) {
        JAMI_ERROR("[call:{}] Unable to initialize invite", call.getCallId());
        return false;
    }

    pjsip_tpselector tp_sel;
    tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
    if (!call.getTransport()) {
        JAMI_ERROR("[call:{}] Unable to get transport", call.getCallId());
        return false;
    }
    tp_sel.u.transport = call.getTransport()->get();
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        JAMI_ERROR("[call:{}] Unable to associate transport for invite session dialog",
                   call.getCallId());
        return false;
    }

    JAMI_LOG("[call:{}] Sending SIP invite", call.getCallId());

    // Add user-agent header
    sip_utils::addUserAgentHeader(getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(call.inviteSession_.get(), tdata) != PJ_SUCCESS) {
        JAMI_ERROR("[call:{}] Unable to send invite message", call.getCallId());
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
        auto accountConfig = config().path / "config.yml";
        std::lock_guard lock(dhtnet::fileutils::getFileLock(accountConfig));
        std::ofstream fout(accountConfig);
        fout.write(accountOut.c_str(), accountOut.size());
        JAMI_LOG("Saved account config to {}", accountConfig);
    } catch (const std::exception& e) {
        JAMI_ERROR("Error saving account config: {}", e.what());
    }
}

void
JamiAccount::loadConfig()
{
    SIPAccountBase::loadConfig();
    registeredName_ = config().registeredName;
    if (accountManager_)
        accountManager_->setAccountDeviceName(config().deviceName);
    if (connectionManager_) {
        if (auto c = connectionManager_->getConfig()) {
            // Update connectionManager's config
            c->upnpEnabled = config().upnpEnabled;
            c->turnEnabled = config().turnEnabled;
            c->turnServer = config().turnServer;
            c->turnServerUserName = config().turnServerUserName;
            c->turnServerPwd = config().turnServerPwd;
            c->turnServerRealm = config().turnServerRealm;
        }
    }
    if (config().proxyEnabled) {
        try {
            auto str = fileutils::loadCacheTextFile(cachePath_ / "dhtproxy",
                                                    std::chrono::hours(24 * 7));
            Json::Value root;
            if (parseJson(str, root)) {
                proxyServerCached_ = root[getProxyConfigKey()].asString();
            }
        } catch (const std::exception& e) {
            JAMI_LOG("[Account {}] Unable to load proxy URL from cache: {}",
                     getAccountID(),
                     e.what());
            proxyServerCached_.clear();
        }
    } else {
        proxyServerCached_.clear();
        std::error_code ec;
        std::filesystem::remove(cachePath_ / "dhtproxy", ec);
    }
    auto credentials = consumeConfigCredentials();
    loadAccount(credentials.archive_password_scheme,
                credentials.archive_password,
                credentials.archive_pin,
                credentials.archive_path);
}

bool
JamiAccount::changeArchivePassword(const std::string& password_old, const std::string& password_new)
{
    try {
        if (!accountManager_->changePassword(password_old, password_new)) {
            JAMI_ERROR("[Account {}] Unable to change archive password", getAccountID());
            return false;
        }
        editConfig([&](JamiAccountConfig& config) {
            config.archiveHasPassword = not password_new.empty();
        });
    } catch (const std::exception& ex) {
        JAMI_ERROR("[Account {}] Unable to change archive password: {}", getAccountID(), ex.what());
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

std::vector<uint8_t>
JamiAccount::getPasswordKey(const std::string& password)
{
    return accountManager_ ? accountManager_->getPasswordKey(password) : std::vector<uint8_t>();
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
JamiAccount::exportArchive(const std::string& destinationPath,
                           std::string_view scheme,
                           const std::string& password)
{
    if (auto manager = dynamic_cast<ArchiveAccountManager*>(accountManager_.get())) {
        return manager->exportArchive(destinationPath, scheme, password);
    }
    return false;
}

bool
JamiAccount::setValidity(std::string_view scheme,
                         const std::string& pwd,
                         const dht::InfoHash& id,
                         int64_t validity)
{
    if (auto manager = dynamic_cast<ArchiveAccountManager*>(accountManager_.get())) {
        if (manager->setValidity(scheme, pwd, id_, id, validity)) {
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
    std::lock_guard lock(configurationMutex_);
    if (auto info = accountManager_->getInfo()) {
        auto contacts = info->contacts->getContacts();
        for (auto& [id, c] : contacts) {
            if (removed.find(c.conversationId) != removed.end()) {
                info->contacts->updateConversation(id, "");
                JAMI_WARNING(
                    "[Account {}] Detected removed conversation ({}) in contact details for {}",
                    getAccountID(),
                    c.conversationId,
                    id.toString());
            }
        }
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
JamiAccount::revokeDevice(const std::string& device,
                          std::string_view scheme,
                          const std::string& password)
{
    if (not accountManager_)
        return false;
    return accountManager_->revokeDevice(
        device, scheme, password, [this, device](AccountManager::RevokeDeviceResult result) {
            emitSignal<libjami::ConfigurationSignal::DeviceRevocationEnded>(getAccountID(),
                                                                            device,
                                                                            static_cast<int>(
                                                                                result));
        });
    return true;
}

std::pair<std::string, std::string>
JamiAccount::saveIdentity(const dht::crypto::Identity id,
                          const std::filesystem::path& path,
                          const std::string& name)
{
    auto names = std::make_pair(name + ".key", name + ".crt");
    if (id.first)
        fileutils::saveFile(path / names.first, id.first->serialize(), 0600);
    if (id.second)
        fileutils::saveFile(path / names.second, id.second->getPacked(), 0600);
    return names;
}

// must be called while configurationMutex_ is locked
void
JamiAccount::loadAccount(const std::string& archive_password_scheme,
                         const std::string& archive_password,
                         const std::string& archive_pin,
                         const std::string& archive_path)
{
    if (registrationState_ == RegistrationState::INITIALIZING)
        return;

    JAMI_DEBUG("[Account {:s}] Loading account", getAccountID());
    AccountManager::OnChangeCallback callbacks {
        [this](const std::string& uri, bool confirmed) {
            if (!id_.first)
                return;
            if (jami::Manager::instance().syncOnRegister) {
                dht::ThreadPool::io().run([w = weak(), uri, confirmed] {
                    if (auto shared = w.lock()) {
                        if (auto cm = shared->convModule(true)) {
                            auto activeConv = cm->getOneToOneConversation(uri);
                            if (!activeConv.empty())
                                cm->bootstrap(activeConv);
                        }
                        emitSignal<libjami::ConfigurationSignal::ContactAdded>(shared->getAccountID(),
                                                                               uri,
                                                                               confirmed);
                    }
                });
            }
        },
        [this](const std::string& uri, bool banned) {
            if (!id_.first)
                return;
            dht::ThreadPool::io().run([w = weak(), uri, banned] {
                if (auto shared = w.lock()) {
                    // Erase linked conversation's requests
                    if (auto convModule = shared->convModule(true))
                        convModule->removeContact(uri, banned);
                    // Remove current connections with contact
                    // Note: if contact is ourself, we don't close the connection
                    // because it's used for syncing other conversations.
                    if (shared->connectionManager_ && uri != shared->getUsername()) {
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
            dht::ThreadPool::io().run([w = weak(), uri, conversationId, payload, received] {
                if (auto shared = w.lock()) {
                    shared->clearProfileCache(uri);
                    if (conversationId.empty()) {
                        // Old path
                        emitSignal<libjami::ConfigurationSignal::IncomingTrustRequest>(
                            shared->getAccountID(), conversationId, uri, payload, received);
                        return;
                    }
                    // Here account can be initializing
                    if (auto cm = shared->convModule(true)) {
                        auto activeConv = cm->getOneToOneConversation(uri);
                        if (activeConv != conversationId)
                            cm->onTrustRequest(uri, conversationId, payload, received);
                    }
                }
            });
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
        [this](const std::string& conversationId, const std::string& deviceId) {
            // Note: Do not retrigger on another thread. This has to be done
            // at the same time of acceptTrustRequest a synced state between TrustRequest
            // and convRequests.
            if (auto cm = convModule(true))
                cm->acceptConversationRequest(conversationId, deviceId);
        },
        [this](const std::string& uri, const std::string& convFromReq) {
            dht::ThreadPool::io().run([w = weak(), convFromReq, uri] {
                if (auto shared = w.lock()) {
                    auto cm = shared->convModule(true);
                    // Remove cached payload if there is one
                    auto requestPath = shared->cachePath_ / "requests" / uri;
                    dhtnet::fileutils::remove(requestPath);
                    if (!convFromReq.empty()) {
                        auto oldConv = cm->getOneToOneConversation(uri);
                        // If we previously removed the contact, and re-add it, we may
                        // receive a convId different from the request. In that case,
                        // we need to remove the current conversation and clone the old
                        // one (given by convFromReq).
                        // TODO: In the future, we may want to re-commit the messages we
                        // may have send in the request we sent.
                        if (oldConv != convFromReq
                            && cm->updateConvForContact(uri, oldConv, convFromReq)) {
                            cm->initReplay(oldConv, convFromReq);
                            cm->cloneConversationFrom(convFromReq, uri, oldConv);
                        }
                    }
                }
            });
        }};

    const auto& conf = config();
    try {
        auto oldIdentity = id_.first ? id_.first->getPublicKey().getLongId() : DeviceId();
        if (conf.managerUri.empty()) {
            accountManager_ = std::make_shared<ArchiveAccountManager>(
                getAccountID(),
                getPath(),
                [this]() { return getAccountDetails(); },
                conf.archivePath.empty() ? "archive.gz" : conf.archivePath,
                conf.nameServer);
        } else {
            accountManager_ = std::make_shared<ServerAccountManager>(getAccountID(),
                                                                     getPath(),
                                                                     conf.managerUri,
                                                                     conf.nameServer);
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
            JAMI_WARNING("[Account {:s}] Loaded account identity", getAccountID());
            if (oldIdentity && info->identity.first->getPublicKey().getLongId() != oldIdentity) {
                JAMI_WARNING("[Account {:s}] Identity changed from {} to {}", getAccountID(), oldIdentity, info->identity.first->getPublicKey().getLongId());
                {
                    std::lock_guard lk(moduleMtx_);
                    convModule_.reset();
                }
                convModule(); // convModule must absolutely be initialized in
                              // both branches of the if statement here in order
                              // for it to exist for subsequent use.
            } else {
                convModule()->setAccountManager(accountManager_);
            }
            if (not isEnabled()) {
                setRegistrationState(RegistrationState::UNREGISTERED);
            }
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
                bool hasArchive = std::filesystem::is_regular_file(archivePath);

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
                    acreds->uri = archivePath.string();
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
            if (hasPassword && archive_password_scheme.empty())
                creds->password_scheme = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;
            else
                creds->password_scheme = archive_password_scheme;

            accountManager_->initAuthentication(
                fDeviceKey,
                ip_utils::getDeviceName(),
                std::move(creds),
                [w = weak(),
                 this,
                 migrating,
                 hasPassword](const AccountInfo& info,
                              const std::map<std::string, std::string>& config,
                              std::string&& receipt,
                              std::vector<uint8_t>&& receipt_signature) {
                    auto sthis = w.lock();
                    if (not sthis)
                        return;
                    JAMI_LOG("[Account {}] Auth success! Device: {}", getAccountID(), info.deviceId);

                    dhtnet::fileutils::check_dir(idPath_, 0700);

                    auto id = info.identity;
                    editConfig([&](JamiAccountConfig& conf) {
                        conf.fromMap(config);
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
                    });
                    id_ = std::move(id);
                    {
                        std::lock_guard lk(moduleMtx_);
                        convModule_.reset();
                    }
                    if (migrating) {
                        Migration::setState(getAccountID(), Migration::State::SUCCESS);
                    }
                    if (not info.photo.empty() or not config_->displayName.empty())
                        emitSignal<libjami::ConfigurationSignal::AccountProfileReceived>(
                            getAccountID(), config_->displayName, info.photo);
                    setRegistrationState(RegistrationState::UNREGISTERED);
                    doRegister();
                },
                [w = weak(),
                 id,
                 accountId = getAccountID(),
                 migrating](AccountManager::AuthError error, const std::string& message) {
                    JAMI_WARNING("[Account {}] Auth error: {} {}", accountId, (int) error, message);
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
        JAMI_WARNING("[Account {}] Error loading account: {}", getAccountID(), e.what());
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
    auto registeredName = getRegisteredName();
    if (not registeredName.empty())
        a.emplace(libjami::Account::VolatileProperties::REGISTERED_NAME, registeredName);
#endif
    a.emplace(libjami::Account::ConfProperties::PROXY_SERVER, proxyServerCached_);
    a.emplace(libjami::Account::VolatileProperties::DHT_BOUND_PORT, std::to_string(dhtBoundPort_));
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
    std::lock_guard lock(configurationMutex_);
    if (accountManager_)
        accountManager_->lookupUri(name,
                                   config().nameServer,
                                   [acc = getAccountID(), name](const std::string& regName,
                                                                const std::string& address,
                                                                NameDirectory::Response response) {
                                       emitSignal<libjami::ConfigurationSignal::RegisteredNameFound>(
                                           acc, name, (int) response, address, regName);
                                   });
}

void
JamiAccount::lookupAddress(const std::string& addr)
{
    std::lock_guard lock(configurationMutex_);
    auto acc = getAccountID();
    if (accountManager_)
        accountManager_->lookupAddress(
            addr, [acc, addr](const std::string& regName, const std::string& address, NameDirectory::Response response) {
                emitSignal<libjami::ConfigurationSignal::RegisteredNameFound>(acc, addr,
                                                                              (int) response,
                                                                              address,
                                                                              regName);
            });
}

void
JamiAccount::registerName(const std::string& name,
                          const std::string& scheme,
                          const std::string& password)
{
    std::lock_guard lock(configurationMutex_);
    if (accountManager_)
        accountManager_->registerName(
            name,
            scheme,
            password,
            [acc = getAccountID(), name, w = weak()](NameDirectory::RegistrationResponse response,
                                                     const std::string& regName) {
                auto res = (int) std::min(response, NameDirectory::RegistrationResponse::error);
                if (response == NameDirectory::RegistrationResponse::success) {
                    if (auto this_ = w.lock()) {
                        if (this_->setRegisteredName(regName)) {
                            this_->editConfig([&](JamiAccountConfig& config) {
                                config.registeredName = regName;
                            });
                            emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
                                this_->accountID_, this_->getVolatileAccountDetails());
                        }
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
        std::lock_guard lk(pendingCallsMutex_);
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
                    std::lock_guard lock(s->configurationMutex_);
                    s->doRegister_();
                }
            });
        }
    };

    loadCachedProxyServer([onLoad](const std::string&) { onLoad(); });

    if (upnpCtrl_) {
        JAMI_LOG("[Account {:s}] UPnP: attempting to map ports", getAccountID());

        // Release current mapping if any.
        if (dhtUpnpMapping_.isValid()) {
            upnpCtrl_->releaseMapping(dhtUpnpMapping_);
        }

        dhtUpnpMapping_.enableAutoUpdate(true);

        // Set the notify callback.
        dhtUpnpMapping_.setNotifyCallback([w = weak(),
                                           onLoad,
                                           update = std::make_shared<bool>(false)](
                                              dhtnet::upnp::Mapping::sharedPtr_t mapRes) {
            if (auto accPtr = w.lock()) {
                auto& dhtMap = accPtr->dhtUpnpMapping_;
                const auto& accId = accPtr->getAccountID();

                JAMI_LOG("[Account {:s}] DHT UPnP mapping changed to {:s}",
                         accId,
                         mapRes->toString(true));

                if (*update) {
                    // Check if we need to update the mapping and the registration.
                    if (dhtMap.getMapKey() != mapRes->getMapKey()
                        or dhtMap.getState() != mapRes->getState()) {
                        // The connectivity must be restarted, if either:
                        // - the state changed to "OPEN",
                        // - the state changed to "FAILED" and the mapping was in use.
                        if (mapRes->getState() == dhtnet::upnp::MappingState::OPEN
                            or (mapRes->getState() == dhtnet::upnp::MappingState::FAILED
                                and dhtMap.getState() == dhtnet::upnp::MappingState::OPEN)) {
                            // Update the mapping and restart the registration.
                            dhtMap.updateFrom(mapRes);

                            JAMI_WARNING(
                                "[Account {:s}] Allocated port changed to {}. Restarting the "
                                "registration",
                                accId,
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
                    if (mapRes->getState() == dhtnet::upnp::MappingState::OPEN) {
                        dhtMap.updateFrom(mapRes);
                        JAMI_LOG(
                            "[Account {:s}] Mapping {:s} successfully allocated: starting the DHT",
                            accId,
                            dhtMap.toString());
                    } else {
                        JAMI_WARNING("[Account {:s}] Mapping request is in {:s} state: starting "
                                     "the DHT anyway",
                                     accId,
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
        if (not map) {
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
    std::lock_guard lock(configurationMutex_);
    if (not isUsable()) {
        JAMI_WARNING("[Account {:s}] Account must be enabled and active to register, ignoring",
                     getAccountID());
        return;
    }

    JAMI_LOG("[Account {:s}] Starting account…", getAccountID());

    // invalid state transitions:
    // INITIALIZING: generating/loading certificates, unable to register
    // NEED_MIGRATION: old account detected, user needs to migrate
    if (registrationState_ == RegistrationState::INITIALIZING
        || registrationState_ == RegistrationState::ERROR_NEED_MIGRATION)
        return;

    convModule(); // Init conv module before passing in trying
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
        JAMI_ERROR("[Account {:s}] Failed to track presence: invalid URI {:s}",
                 getAccountID(), buddy_id);
        return;
    }
    JAMI_LOG("[Account {:s}] {:s} presence for {:s}",
             getAccountID(),
             track ? "Track" : "Untrack",
             buddy_id);

    auto h = dht::InfoHash(buddyUri);
    std::unique_lock lock(buddyInfoMtx);
    if (track) {
        auto buddy = trackedBuddies_.emplace(h, BuddyInfo {h});
        if (buddy.second) {
            trackPresence(buddy.first->first, buddy.first->second);
        }
        auto it = presenceState_.find(buddyUri);
        if (it != presenceState_.end() && it->second != PresenceState::DISCONNECTED) {
            lock.unlock();
            emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                                      buddyUri,
                                                                      static_cast<int>(it->second),
                                                                      "");
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
        = dht->listen<DeviceAnnouncement>(h, [this, h](DeviceAnnouncement&& dev, bool expired) {
              bool wasConnected, isConnected;
              {
                  std::lock_guard lock(buddyInfoMtx);
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
    std::lock_guard lock(buddyInfoMtx);
    std::map<std::string, bool> presence_info;
    for (const auto& buddy_info_p : trackedBuddies_)
        presence_info.emplace(buddy_info_p.first.toString(), buddy_info_p.second.devices_cnt > 0);
    return presence_info;
}

void
JamiAccount::onTrackedBuddyOnline(const dht::InfoHash& contactId)
{
    std::string id(contactId.toString());
    JAMI_DEBUG("[Account {:s}] Buddy {} online", getAccountID(), id);
    auto& state = presenceState_[id];
    if (state < PresenceState::AVAILABLE) {
        state = PresenceState::AVAILABLE;
        emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                                  id,
                                                                  static_cast<int>(
                                                                      PresenceState::AVAILABLE),
                                                                  "");
    }

    auto details = getContactDetails(id);
    auto it = details.find("confirmed");
    if (it == details.end() or it->second == "false") {
        auto convId = convModule()->getOneToOneConversation(id);
        if (convId.empty())
            return;
        // In this case, the TrustRequest was sent but never confirmed (cause the contact was
        // offline maybe) To avoid the contact to never receive the conv request, retry there
        std::lock_guard lock(configurationMutex_);
        if (accountManager_) {
            // Retrieve cached payload for trust request.
            auto requestPath = cachePath_ / "requests" / id;
            std::vector<uint8_t> payload;
            try {
                payload = fileutils::loadFile(requestPath);
            } catch (...) {
            }

            if (payload.size() >= 64000) {
                JAMI_WARNING("[Account {:s}] Trust request for contact {:s} is too big, reset payload", getAccountID(), id);
                payload.clear();
            }

            accountManager_->sendTrustRequest(id, convId, payload);
        }
    }
}

void
JamiAccount::onTrackedBuddyOffline(const dht::InfoHash& contactId)
{
    auto id = contactId.toString();
    JAMI_DEBUG("[Account {:s}] Buddy {} offline", getAccountID(), id);
    auto& state = presenceState_[id];
    if (state > PresenceState::DISCONNECTED) {
        if (state == PresenceState::CONNECTED) {
            JAMI_WARNING("[Account {:s}] Buddy {} is not present on the DHT, but P2P connected", getAccountID(), id);
        }
        state = PresenceState::DISCONNECTED;
        emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                                  id,
                                                                  static_cast<int>(
                                                                      PresenceState::DISCONNECTED),
                                                                  "");
    }
}

void
JamiAccount::doRegister_()
{
    if (registrationState_ != RegistrationState::TRYING) {
        JAMI_ERROR("[Account {}] Already registered", getAccountID());
        return;
    }

    JAMI_DEBUG("[Account {}] Starting account…", getAccountID());
    const auto& conf = config();

    try {
        if (not accountManager_ or not accountManager_->getInfo())
            throw std::runtime_error("No identity configured for this account.");

        if (dht_->isRunning()) {
            JAMI_ERROR("[Account {}] DHT already running (stopping it first).", getAccountID());
            dht_->join();
        }

        convModule()->clearPendingFetch();

#if HAVE_RINGNS
        // Look for registered name
        accountManager_->lookupAddress(
            accountManager_->getInfo()->accountId,
            [w = weak()](const std::string& regName, const std::string& address, const NameDirectory::Response& response) {
                if (auto this_ = w.lock()) {
                    if (response == NameDirectory::Response::found
                        or response == NameDirectory::Response::notFound) {
                        const auto& nameResult = response == NameDirectory::Response::found ? regName
                                                                                            : "";
                        if (this_->setRegisteredName(nameResult)) {
                            this_->editConfig([&](JamiAccountConfig& config) {
                                config.registeredName = nameResult;
                            });
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
        config.dht_config.node_config.persist_path = (cachePath_ / "dhtstate").string();
        config.dht_config.id = id_;
        config.dht_config.cert_cache_all = true;
        config.push_node_id = getAccountID();
        config.push_token = conf.deviceKey;
        config.push_topic = conf.notificationTopic;
        config.push_platform = conf.platform;
        config.proxy_user_agent = jami::userAgent();
        config.threaded = true;
        config.peer_discovery = conf.dhtPeerDiscovery;
        config.peer_publish = conf.dhtPeerDiscovery;
        if (conf.proxyEnabled)
            config.proxy_server = proxyServerCached_;

        if (not config.proxy_server.empty()) {
            JAMI_LOG("[Account {}] Using proxy server {}", getAccountID(), config.proxy_server);
            if (not config.push_token.empty()) {
                JAMI_LOG(
                    "[Account {}] using push notifications with platform: {}, topic: {}, token: {}",
                    getAccountID(),
                    config.push_platform,
                    config.push_topic,
                    config.push_token);
            }
        }

        // check if dht peer service is enabled
        if (conf.accountPeerDiscovery or conf.accountPublish) {
            peerDiscovery_ = std::make_shared<dht::PeerDiscovery>();
            if (conf.accountPeerDiscovery) {
                JAMI_LOG("[Account {}] Starting Jami account discovery…", getAccountID());
                startAccountDiscovery();
            }
            if (conf.accountPublish)
                startAccountPublish();
        }
        dht::DhtRunner::Context context {};
        context.peerDiscovery = peerDiscovery_;
        context.rng = std::make_unique<std::mt19937_64>(dht::crypto::getDerivedRandomEngine(rand));

        auto dht_log_level = Manager::instance().dhtLogLevel.load();
        if (dht_log_level > 0) {
            context.logger = Logger::dhtLogger();
        }
        context.certificateStore = [&](const dht::InfoHash& pk_id) {
            std::vector<std::shared_ptr<dht::crypto::Certificate>> ret;
            if (auto cert = certStore().getCertificate(pk_id.toString()))
                ret.emplace_back(std::move(cert));
            JAMI_LOG("Query for local certificate store: {}: {} found.",
                     pk_id.toString(),
                     ret.size());
            return ret;
        };

        context.statusChangedCallback = [this](dht::NodeStatus s4, dht::NodeStatus s6) {
            JAMI_DBG("[Account %s] DHT status: IPv4 %s; IPv6 %s",
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
                    if (jami::Manager::instance().syncOnRegister) {
                        if (!crt)
                            return;
                        auto deviceId = crt->getLongId().toString();
                        if (accountManager_->getInfo()->deviceId == deviceId)
                            return;

                        std::unique_lock lk(connManagerMtx_);
                        initConnectionManager();
                        lk.unlock();
                        requestSIPConnection(
                            getUsername(),
                            crt->getLongId(),
                            "sync"); // For git notifications, will use the same socket as sync
                    }
                },
                [this] {
                    if (jami::Manager::instance().syncOnRegister) {
                        deviceAnnounced_ = true;

                        // Bootstrap at the end to avoid to be long to load.
                        dht::ThreadPool::io().run([w = weak()] {
                            if (auto shared = w.lock())
                                shared->convModule()->bootstrap();
                        });
                        emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
                            accountID_, getVolatileAccountDetails());
                    }
                },
                publishPresence_);
        };

        dht_->run(dhtPortUsed(), config, std::move(context));

        for (const auto& bootstrap : loadBootstrap())
            dht_->bootstrap(bootstrap);

        dhtBoundPort_ = dht_->getBoundPort();

        accountManager_->setDht(dht_);

        std::unique_lock lkCM(connManagerMtx_);
        initConnectionManager();
        connectionManager_->onDhtConnected(*accountManager_->getInfo()->devicePk);
        connectionManager_->onICERequest([this](const DeviceId& deviceId) {
            std::promise<bool> accept;
            std::future<bool> fut = accept.get_future();
            accountManager_->findCertificate(
                deviceId, [this, &accept](const std::shared_ptr<dht::crypto::Certificate>& cert) {
                    dht::InfoHash peer_account_id;
                    auto res = accountManager_->onPeerCertificate(cert,
                                                                  this->config().dhtPublicInCalls,
                                                                  peer_account_id);
                    JAMI_LOG("{} ICE request from {}",
                             res ? "Accepting" : "Discarding",
                             peer_account_id);
                    accept.set_value(res);
                });
            fut.wait();
            auto result = fut.get();
            return result;
        });
        connectionManager_->onChannelRequest(
            [this](const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name) {
                JAMI_WARNING("[Account {}] New channel asked with name {} from {}",
                             getAccountID(),
                             name,
                             cert->issuer->getId());

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
                std::lock_guard lk(connManagerMtx_);
                auto itHandler = channelHandlers_.find(uri.scheme());
                if (itHandler != channelHandlers_.end() && itHandler->second)
                    return itHandler->second->onRequest(cert, name);
                return name == "sip";
            });
        connectionManager_->onConnectionReady([this](const DeviceId& deviceId,
                                                     const std::string& name,
                                                     std::shared_ptr<dhtnet::ChannelSocket> channel) {
            if (channel) {
                auto cert = channel->peerCertificate();
                if (!cert || !cert->issuer)
                    return;
                auto peerId = cert->issuer->getId().toString();
                // A connection request can be sent just before member is banned and this must be ignored.
                if (accountManager()->getCertificateStatus(peerId)
                    == dhtnet::tls::TrustStore::PermissionStatus::BANNED) {
                    channel->shutdown();
                    return;
                }
                if (name == "sip") {
                    cacheSIPConnection(std::move(channel), peerId, deviceId);
                } else if (name.find("git://") == 0) {
                    auto sep = name.find_last_of('/');
                    auto conversationId = name.substr(sep + 1);
                    auto remoteDevice = name.substr(6, sep - 6);

                    if (channel->isInitiator()) {
                        // Check if wanted remote is our side (git://remoteDevice/conversationId)
                        return;
                    }

                    // Check if pull from banned device
                    if (convModule()->isBanned(conversationId, remoteDevice)) {
                        JAMI_WARNING(
                            "[Account {:s}] Git server requested for conversation {:s}, but the "
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
                    JAMI_WARNING(
                        "[Account {:s}] Git server requested for conversation {:s}, device {:s}, "
                        "channel {}",
                        accountID_,
                        conversationId,
                        deviceId.toString(),
                        channel->channel());
                    auto gs = std::make_unique<GitServer>(accountID_, conversationId, channel);
                    syncCnt_.fetch_add(1);
                    gs->setOnFetched(
                        [w = weak(), conversationId, deviceId](const std::string& commit) {
                            dht::ThreadPool::computation().run([w, conversationId, deviceId, commit]() {
                                if (auto shared = w.lock()) {
                                    shared->convModule()->setFetched(conversationId,
                                                                    deviceId.toString(),
                                                                    commit);
                                    if (shared->syncCnt_.fetch_sub(1) == 1) {
                                        emitSignal<libjami::ConversationSignal::ConversationCloned>(
                                            shared->getAccountID().c_str());
                                    }
                                }
                            });
                        });
                    const dht::Value::Id serverId = ValueIdDist()(rand);
                    {
                        std::lock_guard lk(gitServersMtx_);
                        gitServers_[serverId] = std::move(gs);
                    }
                    channel->onShutdown([w = weak(), serverId]() {
                        // Run on main thread to avoid to be in mxSock's eventLoop
                        runOnMainThread([serverId, w]() {
                            auto shared = w.lock();
                            if (!shared)
                                return;
                            std::lock_guard lk(shared->gitServersMtx_);
                            shared->gitServers_.erase(serverId);
                        });
                    });
                } else {
                    // TODO move git://
                    std::lock_guard lk(connManagerMtx_);
                    auto uri = Uri(name);
                    auto itHandler = channelHandlers_.find(uri.scheme());
                    if (itHandler != channelHandlers_.end() && itHandler->second)
                        itHandler->second->onReady(cert, name, std::move(channel));
                }
            }
        });
        lkCM.unlock();

        if (!conf.managerUri.empty() && accountManager_) {
            dynamic_cast<ServerAccountManager*>(accountManager_.get())->onNeedsMigration([this]() {
                editConfig([&](JamiAccountConfig& conf) {
                    conf.receipt.clear();
                    conf.receiptSignature.clear();
                });
                Migration::setState(accountID_, Migration::State::INVALID);
                setRegistrationState(RegistrationState::ERROR_NEED_MIGRATION);
            });
            dynamic_cast<ServerAccountManager*>(accountManager_.get())
                ->syncBlueprintConfig([this](const std::map<std::string, std::string>& config) {
                    editConfig([&](JamiAccountConfig& conf) { conf.fromMap(config); });
                    emitSignal<libjami::ConfigurationSignal::AccountDetailsChanged>(
                        getAccountID(), getAccountDetails());
                });
        }

        std::lock_guard lock(buddyInfoMtx);
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
JamiAccount::convModule(bool noCreation)
{
    if (noCreation)
        return convModule_.get();
    if (!accountManager() || currentDeviceId() == "") {
        JAMI_ERROR("[Account {}] Calling convModule() with an uninitialized account",
                   getAccountID());
        return nullptr;
    }
    std::unique_lock<std::recursive_mutex> lock(configurationMutex_);
    std::lock_guard lk(moduleMtx_);
    if (!convModule_) {
        convModule_ = std::make_unique<ConversationModule>(
            shared(),
            accountManager_,
            [this](auto&& syncMsg) {
                dht::ThreadPool::io().run([w = weak(), syncMsg] {
                    if (auto shared = w.lock()) {
                        auto& config = shared->config();
                        // For JAMS account, we must update the server
                        // for now, only sync with the JAMS server for changes to the conversation list
                        if (!config.managerUri.empty() && !syncMsg)
                            if (auto am = shared->accountManager())
                                am->syncDevices();
                        if (auto sm = shared->syncModule())
                            sm->syncWithConnected(syncMsg);
                    }
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
                dht::ThreadPool::io().run([w = weak(), convId, deviceId, cb = std::move(cb), type] {
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
                    std::unique_lock lkCM(shared->connManagerMtx_);
                    if (!shared->connectionManager_) {
                        lkCM.unlock();
                        cb({});
                        return;
                    }

                    shared->connectionManager_->connectDevice(
                        DeviceId(deviceId),
                        fmt::format("git://{}/{}", deviceId, convId),
                        [w,
                         cb = std::move(cb),
                         convId](std::shared_ptr<dhtnet::ChannelSocket> socket, const DeviceId&) {
                            dht::ThreadPool::io().run([w,
                                                       cb = std::move(cb),
                                                       socket = std::move(socket),
                                                       convId] {
                                if (socket) {
                                    socket->onShutdown([w, deviceId = socket->deviceId(), convId] {
                                        dht::ThreadPool::io().run([w, deviceId, convId] {
                                            if (auto shared = w.lock())
                                                shared->convModule()
                                                    ->removeGitSocket(deviceId.toString(), convId);
                                        });
                                    });
                                    if (!cb(socket))
                                        socket->shutdown();
                                } else
                                    cb({});
                            });
                        },
                        false,
                        false,
                        type);
                });
            },
            [this](const auto& convId, const auto& deviceId, auto&& cb, const auto& connectionType) {
                dht::ThreadPool::io().run([w = weak(),
                                           convId,
                                           deviceId,
                                           cb = std::move(cb),
                                           connectionType] {
                    auto shared = w.lock();
                    if (!shared)
                        return;
                    auto cm = shared->convModule();
                    std::lock_guard lkCM(shared->connManagerMtx_);
                    if (!shared->connectionManager_ || !cm || cm->isBanned(convId, deviceId)) {
                        Manager::instance().ioContext()->post([cb] { cb({}); });
                        return;
                    }
                    if (!shared->connectionManager_->isConnecting(DeviceId(deviceId),
                                                                  fmt::format("swarm://{}",
                                                                              convId))) {
                        shared->connectionManager_->connectDevice(
                            DeviceId(deviceId),
                            fmt::format("swarm://{}", convId),
                            [w, cb = std::move(cb)](std::shared_ptr<dhtnet::ChannelSocket> socket,
                                                    const DeviceId& deviceId) {
                                dht::ThreadPool::io().run([w,
                                                           cb = std::move(cb),
                                                           socket = std::move(socket),
                                                           deviceId] {
                                    if (socket) {
                                        auto shared = w.lock();
                                        if (!shared)
                                            return;
                                        auto remoteCert = socket->peerCertificate();
                                        auto uri = remoteCert->issuer->getId().toString();
                                        if (shared->accountManager()->getCertificateStatus(uri)
                                            == dhtnet::tls::TrustStore::PermissionStatus::BANNED) {
                                            cb(nullptr);
                                            return;
                                        }

                                        shared->requestSIPConnection(uri, deviceId, "");
                                    }
                                    cb(socket);
                                });
                            });
                    }
                });
            },
            [this](auto&& convId, auto&& from) {
                accountManager_
                    ->findCertificate(dht::InfoHash(from),
                                      [this, from, convId](
                                          const std::shared_ptr<dht::crypto::Certificate>& cert) {
                                          auto info = accountManager_->getInfo();
                                          if (!cert || !info)
                                              return;
                                          info->contacts->onTrustRequest(dht::InfoHash(from),
                                                                         cert->getSharedPublicKey(),
                                                                         time(nullptr),
                                                                         false,
                                                                         convId,
                                                                         {});
                                      });
            },
            autoLoadConversations_);
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
    std::lock_guard lk(moduleMtx_);
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
JamiAccount::loadConversation(const std::string& convId)
{
    if (auto cm = convModule(true))
        cm->loadSingleConversation(convId);
}

void
JamiAccount::doUnregister(bool forceShutdownConnections)
{
    std::unique_lock<std::recursive_mutex> lock(configurationMutex_);
    if (registrationState_ >= RegistrationState::ERROR_GENERIC) {
        return;
    }

    std::mutex mtx;
    std::condition_variable cv;
    bool shutdown_complete {false};

    if (peerDiscovery_) {
        peerDiscovery_->stopPublish(PEER_DISCOVERY_JAMI_SERVICE);
        peerDiscovery_->stopDiscovery(PEER_DISCOVERY_JAMI_SERVICE);
    }

    JAMI_WARN("[Account %s] Unregistering account %p", getAccountID().c_str(), this);
    dht_->shutdown(
        [&] {
            JAMI_WARN("[Account %s] DHT shutdown complete", getAccountID().c_str());
            std::lock_guard lock(mtx);
            shutdown_complete = true;
            cv.notify_all();
        },
        true);

    {
        std::lock_guard lk(pendingCallsMutex_);
        pendingCalls_.clear();
    }

    // Stop all current P2P connections if account is disabled
    // or if explicitly requested by the caller.
    // NOTE: Leaving the connections open is useful when changing an account's config.
    if (not isEnabled() || forceShutdownConnections)
        shutdownConnections();

    // Release current UPnP mapping if any.
    if (upnpCtrl_ and dhtUpnpMapping_.isValid()) {
        upnpCtrl_->releaseMapping(dhtUpnpMapping_);
    }

    {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&] { return shutdown_complete; });
    }
    dht_->join();
    setRegistrationState(RegistrationState::UNREGISTERED);

    lock.unlock();

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
            JAMI_WARNING("[Account {}] Connected", getAccountID());
            turnCache_->refresh();
            if (connectionManager_)
                connectionManager_->storeActiveIpAddress();
        } else if (state == RegistrationState::TRYING) {
            JAMI_WARNING("[Account {}] Connecting…", getAccountID());
        } else {
            deviceAnnounced_ = false;
            JAMI_WARNING("[Account {}] Disconnected", getAccountID());
        }
    }
    // Update registrationState_ & emit signals
    Account::setRegistrationState(state, detail_code, detail_str);
}

void
JamiAccount::reloadContacts()
{
    accountManager_->reloadContacts();
}

void
JamiAccount::connectivityChanged()
{
    JAMI_WARN("connectivityChanged");
    if (not isUsable()) {
        // nothing to do
        return;
    }

    if (auto cm = convModule())
        cm->connectivityChanged();
    dht_->connectivityChanged();
    {
        std::lock_guard lkCM(connManagerMtx_);
        if (connectionManager_) {
            connectionManager_->connectivityChanged();
            // reset cache
            connectionManager_->setPublishedAddress({});
        }
    }
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
                                  dhtnet::tls::TrustStore::PermissionStatus status)
{
    bool done = accountManager_ ? accountManager_->setCertificateStatus(cert_id, status) : false;
    if (done) {
        findCertificate(cert_id);
        emitSignal<libjami::ConfigurationSignal::CertificateStateChanged>(
            getAccountID(), cert_id, dhtnet::tls::TrustStore::statusToStr(status));
    }
    return done;
}

bool
JamiAccount::setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                                  dhtnet::tls::TrustStore::PermissionStatus status,
                                  bool local)
{
    bool done = accountManager_ ? accountManager_->setCertificateStatus(cert, status, local)
                                : false;
    if (done) {
        findCertificate(cert->getId().toString());
        emitSignal<libjami::ConfigurationSignal::CertificateStateChanged>(
            getAccountID(), cert->getId().toString(), dhtnet::tls::TrustStore::statusToStr(status));
    }
    return done;
}

std::vector<std::string>
JamiAccount::getCertificatesByStatus(dhtnet::tls::TrustStore::PermissionStatus status)
{
    if (accountManager_)
        return accountManager_->getCertificatesByStatus(status);
    return {};
}

bool
JamiAccount::isMessageTreated(dht::Value::Id id)
{
    std::lock_guard lock(messageMutex_);
    return !treatedMessages_.add(id);
}

bool
JamiAccount::sha3SumVerify() const
{
    return !noSha3sumVerification_;
}

#ifdef LIBJAMI_TESTABLE
void
JamiAccount::noSha3sumVerification(bool newValue)
{
    noSha3sumVerification_ = newValue;
}
#endif

std::map<std::string, std::string>
JamiAccount::getKnownDevices() const
{
    std::lock_guard lock(configurationMutex_);
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

void
JamiAccount::loadCachedUrl(const std::string& url,
                           const std::filesystem::path& cachePath,
                           const std::chrono::seconds& cacheDuration,
                           std::function<void(const dht::http::Response& response)> cb)
{
    dht::ThreadPool::io().run([cb, url, cachePath, cacheDuration, w = weak()]() {
        try {
            std::string data;
            {
                std::lock_guard lk(dhtnet::fileutils::getFileLock(cachePath));
                data = fileutils::loadCacheTextFile(cachePath, cacheDuration);
            }
            dht::http::Response ret;
            ret.body = std::move(data);
            ret.status_code = 200;
            cb(ret);
        } catch (const std::exception& e) {
            JAMI_LOG("Failed to load '{}' from '{}': {}", url, cachePath, e.what());

            if (auto sthis = w.lock()) {
                auto req = std::make_shared<dht::http::Request>(
                    *Manager::instance().ioContext(),
                    url,
                    [cb, cachePath, w](const dht::http::Response& response) {
                        if (response.status_code == 200) {
                            try {
                                std::lock_guard lk(dhtnet::fileutils::getFileLock(cachePath));
                                fileutils::saveFile(cachePath,
                                                    (const uint8_t*) response.body.data(),
                                                    response.body.size(),
                                                    0600);
                                JAMI_LOG("Cached result to '{}'", cachePath);
                            } catch (const std::exception& ex) {
                                JAMI_WARNING("Failed to save result to '{}': {}",
                                             cachePath,
                                             ex.what());
                            }
                            cb(response);
                        } else {
                            try {
                                if (std::filesystem::exists(cachePath)) {
                                    JAMI_WARNING("Failed to download URL, using cached data");
                                    std::string data;
                                    {
                                        std::lock_guard lk(
                                            dhtnet::fileutils::getFileLock(cachePath));
                                        data = fileutils::loadTextFile(cachePath);
                                    }
                                    dht::http::Response ret;
                                    ret.body = std::move(data);
                                    ret.status_code = 200;
                                    cb(ret);
                                } else
                                    throw std::runtime_error("No cached data");
                            } catch (...) {
                                cb(response);
                            }
                        }
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
        JAMI_DEBUG("[Account {:s}] Loading DHT proxy URL: {:s}", getAccountID(), conf.proxyListUrl);
        if (conf.proxyListUrl.empty() or not conf.proxyListEnabled) {
            cb(getDhtProxyServer(conf.proxyServer));
        } else {
            loadCachedUrl(conf.proxyListUrl,
                          cachePath_ / "dhtproxylist",
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
        dhtnet::fileutils::check_dir(cachePath_, 0700);
        auto proxyCachePath = cachePath_ / "dhtproxy";
        std::ofstream file(proxyCachePath);
        JAMI_DEBUG("Cache DHT proxy server: {}", proxyServerCached_);
        Json::Value node(Json::objectValue);
        node[getProxyConfigKey()] = proxyServerCached_;
        if (file.is_open())
            file << node;
        else
            JAMI_WARNING("Unable to write into {}", proxyCachePath);
    }
    return proxyServerCached_;
}

MatchRank
JamiAccount::matches(std::string_view userName, std::string_view server) const
{
    if (not accountManager_ or not accountManager_->getInfo())
        return MatchRank::NONE;

    if (userName == accountManager_->getInfo()->accountId
        || server == accountManager_->getInfo()->accountId
        || userName == accountManager_->getInfo()->deviceId) {
        JAMI_LOG("Matching account ID in request with username {}", userName);
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
    auto username = to;
    string_replace(username, "sip:", "");
    return fmt::format("<sips:{};transport=tls>", username);
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

std::string
getPIDF(const std::string& note)
{
    // implementing https://datatracker.ietf.org/doc/html/rfc3863
    return fmt::format("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\">\n"
                       "    <tuple>\n"
                       "    <status>\n"
                       "        <basic>{}</basic>\n"
                       "    </status>\n"
                       "    </tuple>\n"
                       "</presence>",
                       note);
}

void
JamiAccount::setIsComposing(const std::string& conversationUri, bool isWriting)
{
    Uri uri(conversationUri);
    std::string conversationId = {};
    if (uri.scheme() == Uri::Scheme::SWARM) {
        conversationId = uri.authority();
    } else {
        return;
    }

    if (auto cm = convModule(true)) {
        if (auto typer = cm->getTypers(conversationId)) {
            if (isWriting)
                typer->addTyper(getUsername(), true);
            else
                typer->removeTyper(getUsername(), true);
        }
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
    dht::InfoHash h(uri);
    if (not h) {
        JAMI_ERROR("addContact: invalid contact URI");
        return;
    }
    auto conversation = convModule()->getOneToOneConversation(uri);
    if (!confirmed && conversation.empty())
        conversation = convModule()->startConversation(ConversationMode::ONE_TO_ONE, h);
    std::unique_lock<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_)
        accountManager_->addContact(h, confirmed, conversation);
    else
        JAMI_WARNING("[Account {}] addContact: account not loaded", getAccountID());
}

void
JamiAccount::removeContact(const std::string& uri, bool ban)
{
    std::lock_guard lock(configurationMutex_);
    if (accountManager_)
        accountManager_->removeContact(uri, ban);
    else
        JAMI_WARNING("[Account {}] removeContact: account not loaded", getAccountID());
}

std::map<std::string, std::string>
JamiAccount::getContactDetails(const std::string& uri) const
{
    std::lock_guard lock(configurationMutex_);
    return accountManager_ ? accountManager_->getContactDetails(uri)
                           : std::map<std::string, std::string> {};
}

std::vector<std::map<std::string, std::string>>
JamiAccount::getContacts(bool includeRemoved) const
{
    std::lock_guard lock(configurationMutex_);
    if (not accountManager_)
        return {};
    return accountManager_->getContacts(includeRemoved);
}

/* trust requests */

std::vector<std::map<std::string, std::string>>
JamiAccount::getTrustRequests() const
{
    std::lock_guard lock(configurationMutex_);
    return accountManager_ ? accountManager_->getTrustRequests()
                           : std::vector<std::map<std::string, std::string>> {};
}

bool
JamiAccount::acceptTrustRequest(const std::string& from, bool includeConversation)
{
    dht::InfoHash h(from);
    if (not h) {
        JAMI_ERROR("addContact: invalid contact URI");
        return false;
    }
    std::unique_lock<std::recursive_mutex> lock(configurationMutex_);
    if (accountManager_) {
        if (!accountManager_->acceptTrustRequest(from, includeConversation)) {
            // Note: unused for swarm
            // Typically the case where the trust request doesn't exists, only incoming DHT messages
            return accountManager_->addContact(h, true);
        }
        return true;
    }
    JAMI_WARNING("[Account {}] acceptTrustRequest: account not loaded", getAccountID());
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
    std::lock_guard lock(configurationMutex_);
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
        std::lock_guard lock(configurationMutex_);
        if (auto info = accountManager_->getInfo()) {
            // Verify if we have a trust request with this peer + convId
            auto req = info->contacts->getTrustRequest(dht::InfoHash(peerId));
            if (req.find(libjami::Account::TrustRequest::CONVERSATIONID) != req.end()
                && req.at(libjami::Account::TrustRequest::CONVERSATIONID) == conversationId) {
                accountManager_->discardTrustRequest(peerId);
                JAMI_DEBUG("[Account {:s}] Declined trust request with {:s}",
                           getAccountID(),
                           peerId);
            }
        }
    }
}

void
JamiAccount::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    dht::InfoHash h(to);
    if (not h) {
        JAMI_ERROR("addContact: invalid contact URI");
        return;
    }
    // Here we cache payload sent by the client
    auto requestPath = cachePath_ / "requests";
    dhtnet::fileutils::recursive_mkdir(requestPath, 0700);
    auto cachedFile = requestPath / to;
    std::ofstream req(cachedFile, std::ios::trunc | std::ios::binary);
    if (!req.is_open()) {
        JAMI_ERROR("Unable to write data to {}", cachedFile);
        return;
    }

    if (not payload.empty()) {
        req.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    }

    if (payload.size() >= 64000) {
        JAMI_WARN() << "Trust request is too big. Remove payload";
    }

    auto conversation = convModule()->getOneToOneConversation(to);
    if (conversation.empty())
        conversation = convModule()->startConversation(ConversationMode::ONE_TO_ONE, h);
    if (not conversation.empty()) {
        std::lock_guard lock(configurationMutex_);
        if (accountManager_)
            accountManager_->sendTrustRequest(to,
                                              conversation,
                                              payload.size() >= 64000 ? std::vector<uint8_t> {}
                                                                      : payload);
        else
            JAMI_WARNING("[Account {}] sendTrustRequest: account not loaded", getAccountID());
    } else
        JAMI_WARNING("[Account {}] sendTrustRequest: account not loaded", getAccountID());
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
                             uint64_t refreshToken,
                             bool onlyConnected)
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
    return SIPAccountBase::sendTextMessage(toUri, deviceId, payloads, refreshToken, onlyConnected);
}

void
JamiAccount::sendMessage(const std::string& to,
                         const std::string& deviceId,
                         const std::map<std::string, std::string>& payloads,
                         uint64_t token,
                         bool retryOnTimeout,
                         bool onlyConnected)
{
    std::string toUri;
    try {
        toUri = parseJamiUri(to);
    } catch (...) {
        JAMI_ERROR("[Account {}] Failed to send a text message due to an invalid URI {}",
                   getAccountID(),
                   to);
        if (!onlyConnected)
            messageEngine_.onMessageSent(to, token, false, deviceId);
        return;
    }
    if (payloads.size() != 1) {
        JAMI_ERROR("Multi-part im is not supported");
        if (!onlyConnected)
            messageEngine_.onMessageSent(toUri, token, false, deviceId);
        return;
    }

    auto devices = std::make_shared<std::set<DeviceId>>();

    // Use the Message channel if available
    std::unique_lock clk(connManagerMtx_);
    auto* handler = static_cast<MessageChannelHandler*>(
        channelHandlers_[Uri::Scheme::MESSAGE].get());
    if (!handler) {
        clk.unlock();
        if (!onlyConnected)
            messageEngine_.onMessageSent(to, token, false, deviceId);
        return;
    }
    const auto& payload = *payloads.begin();
    MessageChannelHandler::Message msg;
    msg.t = payload.first;
    msg.c = payload.second;
    auto device = deviceId.empty() ? DeviceId() : DeviceId(deviceId);
    if (deviceId.empty()) {
        bool sent = false;
        auto conns = handler->getChannels(toUri);
        clk.unlock();
        for (const auto& conn : conns) {
            if (MessageChannelHandler::sendMessage(conn, msg)) {
                devices->emplace(conn->deviceId());
                sent = true;
            }
        }
        if (sent) {
            if (!onlyConnected)
                runOnMainThread([w = weak(), to, token, deviceId]() {
                    if (auto acc = w.lock())
                        acc->messageEngine_.onMessageSent(to, token, true, deviceId);
                });
            return;
        }
    } else {
        if (auto conn = handler->getChannel(toUri, device)) {
            clk.unlock();
            if (MessageChannelHandler::sendMessage(conn, msg)) {
                if (!onlyConnected)
                    runOnMainThread([w = weak(), to, token, deviceId]() {
                        if (auto acc = w.lock())
                            acc->messageEngine_.onMessageSent(to, token, true, deviceId);
                    });
                return;
            }
        }
    }
    if (clk)
        clk.unlock();

    std::unique_lock lk(sipConnsMtx_);
    for (auto& [key, value] : sipConns_) {
        if (key.first != to or value.empty())
            continue;
        if (!deviceId.empty() && key.second != device)
            continue;
        if (!devices->emplace(key.second).second)
            continue;

        auto& conn = value.back();
        auto& channel = conn.channel;

        // Set input token into callback
        auto ctx = std::make_unique<TextMessageCtx>();
        ctx->acc = weak();
        ctx->to = to;
        ctx->deviceId = device;
        ctx->id = token;
        ctx->onlyConnected = onlyConnected;
        ctx->retryOnTimeout = retryOnTimeout;
        ctx->channel = channel;

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
                                                  if (auto acc = c->acc.lock())
                                                      acc->onSIPMessageSent(std::move(c), code);
                                              }
                                          });
                                      });
            if (!res) {
                if (!onlyConnected)
                    messageEngine_.onMessageSent(to, token, false, deviceId);
                devices->erase(key.second);
                continue;
            }
        } catch (const std::runtime_error& ex) {
            JAMI_WARNING("{}", ex.what());
            if (!onlyConnected)
                messageEngine_.onMessageSent(to, token, false, deviceId);
            devices->erase(key.second);
            // Remove connection in incorrect state
            shutdownSIPConnection(channel, to, key.second);
            continue;
        }

        if (key.second == device) {
            return;
        }
    }
    lk.unlock();

    if (onlyConnected)
        return;
    // We are unable to send the message directly, try connecting
    messageEngine_.onMessageSent(to, token, false, deviceId);

    // Get conversation id, which will be used by the iOS notification extension
    // to load the conversation.
    auto extractIdFromJson = [](const std::string& jsonData) -> std::string {
        Json::Value parsed;
        if (parseJson(jsonData, parsed)) {
            auto value = parsed.get("id", Json::nullValue);
            if (value && value.isString()) {
                return value.asString();
            }
        } else {
            JAMI_WARNING("Unable to parse jsonData to get conversation ID");
        }
        return "";
    };

    // get request type
    auto payload_type = msg.t;
    if (payload_type == MIME_TYPE_GIT) {
        std::string id = extractIdFromJson(msg.c);
        if (!id.empty()) {
            payload_type += "/" + id;
        }
    }

    if (deviceId.empty()) {
        auto toH = dht::InfoHash(toUri);
        // Find listening devices for this account
        accountManager_->forEachDevice(toH,
                                       [this,
                                        to,
                                        devices,
                                        payload_type,
                                        currentDevice = DeviceId(currentDeviceId())](
                                           const std::shared_ptr<dht::crypto::PublicKey>& dev) {
                                           // Test if already sent
                                           auto deviceId = dev->getLongId();
                                           if (!devices->emplace(deviceId).second
                                               || deviceId == currentDevice) {
                                               return;
                                           }

                                           // Else, ask for a channel to send the message
                                           requestSIPConnection(to, deviceId, payload_type);
                                       });
    } else {
        requestSIPConnection(to, device, payload_type);
    }
}

void
JamiAccount::onSIPMessageSent(const std::shared_ptr<TextMessageCtx>& ctx, int code)
{
    if (code == PJSIP_SC_OK) {
        if (!ctx->onlyConnected)
            messageEngine_.onMessageSent(ctx->to,
                                         ctx->id,
                                         true,
                                         ctx->deviceId ? ctx->deviceId.toString() : "");
    } else {
        // Note: This can be called from PJSIP's eventloop while
        // sipConnsMtx_ is locked. So we should retrigger the shutdown.
        auto acc = ctx->acc.lock();
        if (not acc)
            return;
        JAMI_WARN("Timeout when send a message, close current connection");
        shutdownSIPConnection(ctx->channel, ctx->to, ctx->deviceId);
        // This MUST be done after closing the connection to avoid race condition
        // with messageEngine_
        if (!ctx->onlyConnected)
            messageEngine_.onMessageSent(ctx->to,
                                         ctx->id,
                                         false,
                                         ctx->deviceId ? ctx->deviceId.toString() : "");

        // In that case, the peer typically changed its connectivity.
        // After closing sockets with that peer, we try to re-connect to
        // that peer one time.
        if (ctx->retryOnTimeout)
            messageEngine_.onPeerOnline(ctx->to, ctx->deviceId ? ctx->deviceId.toString() : "");
    }
}

dhtnet::IceTransportOptions
JamiAccount::getIceOptions() const noexcept
{
    return connectionManager_->getIceOptions();
}

void
JamiAccount::getIceOptions(std::function<void(dhtnet::IceTransportOptions&&)> cb) const noexcept
{
    return connectionManager_->getIceOptions(std::move(cb));
}

dhtnet::IpAddr
JamiAccount::getPublishedIpAddress(uint16_t family) const
{
    return connectionManager_->getPublishedIpAddress(family);
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
JamiAccount::setPushNotificationConfig(const std::map<std::string, std::string>& data)
{
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
    auto ret_future = dht_->pushNotificationReceived(data);
    dht::ThreadPool::computation().run([id = getAccountID(), ret_future = ret_future.share()] {
        JAMI_WARNING("[Account {:s}] pushNotificationReceived: {}", id, (uint8_t)ret_future.get());
    });
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
            std::lock_guard lc(discoveryMapMtx_);
            // Make sure that account itself will not be recorded
            if (v.accountId != id) {
                // Create or find the old one
                auto& dp = discoveredPeers_[v.accountId];
                dp.displayName = v.displayName;
                discoveredPeerMap_[v.accountId.toString()] = v.displayName;
                if (dp.cleanupTask) {
                    dp.cleanupTask->cancel();
                } else {
                    // Avoid repeat reception of same peer
                    JAMI_LOG("Account discovered: {}: {}", v.displayName, v.accountId.to_c_str());
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
                                std::lock_guard lc(this_->discoveryMapMtx_);
                                this_->discoveredPeers_.erase(p);
                                this_->discoveredPeerMap_.erase(p.toString());
                            }
                            // Send deleted peer
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
JamiAccount::sendProfileToPeers()
{
    if (!connectionManager_)
        return;
    std::set<std::string> peers;
    const auto& accountUri = accountManager_->getInfo()->accountId;
    // TODO: avoid using getConnectionList
    for (const auto& connection : connectionManager_->getConnectionList()) {
        const auto& device = connection.at("device");
        const auto& peer = connection.at("peer");
        if (!peers.emplace(peer).second)
            continue;
        if (peer == accountUri) {
            sendProfile("", accountUri, device);
            continue;
        }
        const auto& conversationId = convModule()->getOneToOneConversation(peer);
        if (!conversationId.empty()) {
            sendProfile(conversationId, peer, device);
        }
    }
}

void
JamiAccount::updateProfile(const std::string& displayName,
                           const std::string& avatar,
                           const std::string& fileType,
                           int32_t flag)
{
    // if the fileType is empty then only the display name will be upated

    const auto& accountUri = accountManager_->getInfo()->accountId;
    const auto& path = profilePath();
    const auto& profiles = idPath_ / "profiles";

    try {
        if (!std::filesystem::exists(profiles)) {
            std::filesystem::create_directories(profiles);
        }
    } catch (const std::exception& e) {
        JAMI_ERROR("Failed to create profiles directory: {}", e.what());
        return;
    }

    const auto& vCardPath = profiles / fmt::format("{}.vcf", base64::encode(accountUri));

    auto profile = getProfileVcard();
    if (profile.empty()) {
        profile = vCard::utils::initVcard();
    }

    profile["FN"] = displayName;
    editConfig([&](JamiAccountConfig& config) { config.displayName = displayName; });
    emitSignal<libjami::ConfigurationSignal::AccountDetailsChanged>(getAccountID(),
                                                                    getAccountDetails());

    if (!fileType.empty()) {
        const std::string& key = "PHOTO;ENCODING=BASE64;TYPE=" + fileType;
        if (flag == 0) {
            vCard::utils::removeByKey(profile, "PHOTO");
            const auto& avatarPath = std::filesystem::path(avatar);
            if (std::filesystem::exists(avatarPath)) {
                try {
                    profile[key] = base64::encode(fileutils::loadFile(avatarPath));
                } catch (const std::exception& e) {
                    JAMI_ERROR("Failed to load avatar: {}", e.what());
                }
            }
        } else if (flag == 1) {
            vCard::utils::removeByKey(profile, "PHOTO");
            profile[key] = avatar;
        }
    }
    if (flag == 2) {
        vCard::utils::removeByKey(profile, "PHOTO");
    }
    try {
        std::filesystem::path tmpPath = vCardPath.string() + ".tmp";
        std::ofstream file(tmpPath);
        if (file.is_open()) {
            file << vCard::utils::toString(profile);
            file.close();
            std::filesystem::rename(tmpPath, vCardPath);
            fileutils::createFileLink(path, vCardPath, true);
            emitSignal<libjami::ConfigurationSignal::ProfileReceived>(getAccountID(),
                                                                      accountUri,
                                                                      path.string());
            sendProfileToPeers();
        } else {
            JAMI_ERROR("Unable to open file for writing: {}", tmpPath.string());
        }
    } catch (const std::exception& e) {
        JAMI_ERROR("Error writing profile: {}", e.what());
    }
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
        const auto& uri = m.at("uri");
        auto token = std::uniform_int_distribution<uint64_t> {1, JAMI_ID_MAX_VAL}(rand);
        // Announce to all members that a new message is sent
        sendMessage(uri, "", msg, token, false, true);
    }
}

bool
JamiAccount::handleMessage(const std::string& from, const std::pair<std::string, std::string>& m)
{
    if (m.first == MIME_TYPE_GIT) {
        Json::Value json;
        if (!parseJson(m.second, json)) {
            return false;
        }

        std::string deviceId = json["deviceId"].asString();
        std::string id = json["id"].asString();
        std::string commit = json["commit"].asString();
        // fetchNewCommits will do heavy stuff like fetching, avoid to block SIP socket
        dht::ThreadPool::io().run([w = weak(), from, deviceId, id, commit] {
            if (auto shared = w.lock()) {
                if (auto cm = shared->convModule())
                    cm->fetchNewCommits(from, deviceId, id, commit);
            }
        });
        return true;
    } else if (m.first == MIME_TYPE_INVITE) {
        convModule()->onNeedConversationRequest(from, m.second);
        return true;
    } else if (m.first == MIME_TYPE_INVITE_JSON) {
        Json::Value json;
        if (!parseJson(m.second, json)) {
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
            if (!conversationId.empty()) {
                if (auto cm = convModule(true)) {
                    if (auto typer = cm->getTypers(conversationId)) {
                        if (isComposing)
                            typer->addTyper(from);
                        else
                            typer->removeTyper(from);
                    }
                }
            }
            return true;
        } catch (const std::exception& e) {
            JAMI_WARNING("Error parsing composing state: {}", e.what());
        }
    } else if (m.first == MIME_TYPE_IMDN) {
        try {
            static const std::regex IMDN_MSG_ID_REGEX("<message-id>\\s*(\\w+)\\s*<\\/message-id>");
            std::smatch matched_pattern;

            std::regex_search(m.second, matched_pattern, IMDN_MSG_ID_REGEX);
            std::string messageId;
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                messageId = matched_pattern[1];
            } else {
                JAMI_WARNING("Message displayed: unable to parse message ID");
                return false;
            }

            static const std::regex STATUS_REGEX("<status>\\s*<(\\w+)\\/>\\s*<\\/status>");
            std::regex_search(m.second, matched_pattern, STATUS_REGEX);
            bool isDisplayed {false};
            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                isDisplayed = matched_pattern[1] == "displayed";
            } else {
                JAMI_WARNING("Message displayed: unable to parse status");
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
            if (isDisplayed) {
                if (convModule()->onMessageDisplayed(from, conversationId, messageId)) {
                    JAMI_DEBUG("[message {}] Displayed by peer", messageId);
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
            JAMI_ERROR("Error parsing display notification: {}", e.what());
        }
    } else if (m.first == MIME_TYPE_PIDF) {
        std::smatch matched_pattern;
        static const std::regex BASIC_REGEX("<basic>([\\w\\s]+)<\\/basic>");
        std::regex_search(m.second, matched_pattern, BASIC_REGEX);
        std::string customStatus {};
        if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
            customStatus = matched_pattern[1];
            emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                                      from,
                                                                      static_cast<int>(
                                                                          PresenceState::CONNECTED),
                                                                      customStatus);
            return true;
        } else {
            JAMI_WARNING("Presence: unable to parse status");
        }
    }

    return false;
}

void
JamiAccount::callConnectionClosed(const DeviceId& deviceId, bool eraseDummy)
{
    std::function<void(const DeviceId&, bool)> cb;
    {
        std::lock_guard lk(onConnectionClosedMtx_);
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
JamiAccount::requestMessageConnection(const std::string& peerId,
                                      const DeviceId& deviceId,
                                      const std::string& connectionType)
{
    auto* handler = static_cast<MessageChannelHandler*>(
        channelHandlers_[Uri::Scheme::MESSAGE].get());
    if (deviceId) {
        if (auto connected = handler->getChannel(peerId, deviceId)) {
            return;
        }
    } else {
        auto connected = handler->getChannels(peerId);
        if (!connected.empty()) {
            return;
        }
    }
    handler->connect(
        deviceId,
        "",
        [w = weak(), peerId](std::shared_ptr<dhtnet::ChannelSocket> socket,
                             const DeviceId& deviceId) {
            if (socket)
                if (auto acc = w.lock()) {
                    acc->messageEngine_.onPeerOnline(peerId);
                    acc->messageEngine_.onPeerOnline(peerId, deviceId.toString(), true);
                }
        },
        connectionType);
}

void
JamiAccount::requestSIPConnection(const std::string& peerId,
                                  const DeviceId& deviceId,
                                  const std::string& connectionType,
                                  bool forceNewConnection,
                                  const std::shared_ptr<SIPCall>& pc)
{
    requestMessageConnection(peerId, deviceId, connectionType);
    if (peerId == getUsername()) {
        if (!syncModule()->isConnected(deviceId))
            channelHandlers_[Uri::Scheme::SYNC]
                ->connect(deviceId,
                          "",
                          [](std::shared_ptr<dhtnet::ChannelSocket> socket,
                             const DeviceId& deviceId) {});
    }

    JAMI_LOG("[Account {}] Request SIP connection to peer {} on device {}",
             getAccountID(),
             peerId,
             deviceId);

    // If a connection already exists or is in progress, no need to do this
    std::lock_guard lk(sipConnsMtx_);
    auto id = std::make_pair(peerId, deviceId);

    if (sipConns_.find(id) != sipConns_.end()) {
        JAMI_LOG("[Account {}] A SIP connection with {} already exists", getAccountID(), deviceId);
        return;
    }
    // If not present, create it
    std::lock_guard lkCM(connManagerMtx_);
    if (!connectionManager_)
        return;
    // Note, Even if we send 50 "sip" request, the connectionManager_ will only use one socket.
    // however, this will still ask for multiple channels, so only ask
    // if there is no pending request
    if (!forceNewConnection && connectionManager_->isConnecting(deviceId, "sip")) {
        JAMI_LOG("[Account {}] Already connecting to {}", getAccountID(), deviceId);
        return;
    }
    JAMI_LOG("[Account {}] Ask {} for a new SIP channel", getAccountID(), deviceId);
    connectionManager_->connectDevice(
        deviceId,
        "sip",
        [w = weak(),
         id = std::move(id),
         pc = std::move(pc)](std::shared_ptr<dhtnet::ChannelSocket> socket, const DeviceId&) {
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

bool
JamiAccount::isConnectedWith(const DeviceId& deviceId) const
{
    std::lock_guard lkCM(connManagerMtx_);
    if (connectionManager_)
        return connectionManager_->isConnected(deviceId);
    return false;
}

void
JamiAccount::sendPresenceNote(const std::string& note)
{
    if (auto info = accountManager_->getInfo()) {
        if (!info || !info->contacts)
            return;
        presenceNote_ = note;
        auto contacts = info->contacts->getContacts();
        std::vector<SipConnectionKey> keys;
        {
            std::lock_guard lk(sipConnsMtx_);
            for (auto& [key, conns] : sipConns_) {
                keys.push_back(key);
            }
        }
        auto token = std::uniform_int_distribution<uint64_t> {1, JAMI_ID_MAX_VAL}(rand);
        std::map<std::string, std::string> msg = {{MIME_TYPE_PIDF, getPIDF(presenceNote_)}};
        for (auto& key : keys) {
            sendMessage(key.first, key.second.toString(), msg, token, false, true);
        }
    }
}

void
JamiAccount::sendProfile(const std::string& convId,
                         const std::string& peerUri,
                         const std::string& deviceId)
{
    auto accProfilePath = profilePath();
    if (not std::filesystem::is_regular_file(accProfilePath))
        return;
    auto currentSha3 = fileutils::sha3File(accProfilePath);
    // VCard sync for peerUri
    if (not needToSendProfile(peerUri, deviceId, currentSha3)) {
        JAMI_DEBUG("Peer {} already got an up-to-date vCard", peerUri);
        return;
    }
    // We need a new channel
    transferFile(convId,
                 accProfilePath.string(),
                 deviceId,
                 "profile.vcf",
                 "",
                 0,
                 0,
                 currentSha3,
                 fileutils::lastWriteTimeInSeconds(accProfilePath),
                 [accId = getAccountID(), peerUri, deviceId]() {
                     // Mark the VCard as sent
                     auto sendDir = fileutils::get_cache_dir() / accId / "vcard" / peerUri;
                     auto path = sendDir / deviceId;
                     dhtnet::fileutils::recursive_mkdir(sendDir);
                     std::lock_guard lock(dhtnet::fileutils::getFileLock(path));
                     if (std::filesystem::is_regular_file(path))
                         return;
                     std::ofstream p(path);
                 });
}

bool
JamiAccount::needToSendProfile(const std::string& peerUri,
                               const std::string& deviceId,
                               const std::string& sha3Sum)
{
    std::string previousSha3 {};
    auto vCardPath = cachePath_ / "vcard";
    auto sha3Path = vCardPath / "sha3";
    dhtnet::fileutils::check_dir(vCardPath, 0700);
    try {
        previousSha3 = fileutils::loadTextFile(sha3Path);
    } catch (...) {
        fileutils::saveFile(sha3Path, (const uint8_t*) sha3Sum.data(), sha3Sum.size(), 0600);
        return true;
    }
    if (sha3Sum != previousSha3) {
        // Incorrect sha3 stored. Update it
        dhtnet::fileutils::removeAll(vCardPath, true);
        dhtnet::fileutils::check_dir(vCardPath, 0700);
        fileutils::saveFile(sha3Path, (const uint8_t*) sha3Sum.data(), sha3Sum.size(), 0600);
        return true;
    }
    auto peerPath = vCardPath / peerUri;
    dhtnet::fileutils::recursive_mkdir(peerPath);
    return not std::filesystem::is_regular_file(peerPath / deviceId);
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

    // Build SIP message
    constexpr pjsip_method msg_method = {PJSIP_OTHER_METHOD,
                                         sip_utils::CONST_PJ_STR(sip_utils::SIP_METHODS::MESSAGE)};
    pj_str_t pjFrom = sip_utils::CONST_PJ_STR(from);
    pj_str_t pjTo = sip_utils::CONST_PJ_STR(toURI);

    // Create request.
    pjsip_tx_data* tdata = nullptr;
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
        JAMI_ERROR("Unable to create request: {}", sip_utils::sip_strerror(status));
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
        JAMI_ERROR("Unable to create request: {}", sip_utils::sip_strerror(status));
        return false;
    }
    im::fillPJSIPMessageBody(*tdata, data);

    // Because pjsip_endpt_send_request can take quite some time, move it in a io thread to avoid to block
    dht::ThreadPool::io().run([w = weak(), tdata, ctx, cb = std::move(cb)] {
        auto shared = w.lock();
        if (!shared)
            return;
        auto status = pjsip_endpt_send_request(shared->link_.getEndpoint(), tdata, -1, ctx, cb);
        if (status != PJ_SUCCESS)
            JAMI_ERROR("Unable to send request: {}", sip_utils::sip_strerror(status));
    });
    return true;
}

void
JamiAccount::clearProfileCache(const std::string& peerUri)
{
    std::error_code ec;
    std::filesystem::remove_all(cachePath_ / "vcard" / peerUri, ec);
}

std::filesystem::path
JamiAccount::profilePath() const
{
    return idPath_ / "profile.vcf";
}

void
JamiAccount::cacheSIPConnection(std::shared_ptr<dhtnet::ChannelSocket>&& socket,
                                const std::string& peerId,
                                const DeviceId& deviceId)
{
    std::unique_lock lk(sipConnsMtx_);
    // Verify that the connection is not already cached
    SipConnectionKey key(peerId, deviceId);
    auto& connections = sipConns_[key];
    auto conn = std::find_if(connections.begin(), connections.end(), [&](const auto& v) {
        return v.channel == socket;
    });
    if (conn != connections.end()) {
        JAMI_WARNING("[Account {}] Channel socket already cached with this peer", getAccountID());
        return;
    }

    // Convert to SIP transport
    auto onShutdown = [w = weak(), peerId, key, socket]() {
        dht::ThreadPool::io().run([w = std::move(w), peerId, key, socket] {
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
    JAMI_WARNING("[Account {:s}] New SIP channel opened with {:s}", getAccountID(), deviceId);
    lk.unlock();

    dht::ThreadPool::io().run([w = weak(), peerId, deviceId] {
        if (auto shared = w.lock()) {
            if (shared->presenceNote_ != "") {
                // If a presence note is set, send it to this device.
                auto token = std::uniform_int_distribution<uint64_t> {1, JAMI_ID_MAX_VAL}(
                    shared->rand);
                std::map<std::string, std::string> msg = {
                    {MIME_TYPE_PIDF, getPIDF(shared->presenceNote_)}};
                shared->sendMessage(peerId, deviceId.toString(), msg, token, false, true);
            }
            shared->convModule()->syncConversations(peerId, deviceId.toString());
        }
    });

    // Retry messages
    messageEngine_.onPeerOnline(peerId);
    messageEngine_.onPeerOnline(peerId, deviceId.toString(), true);

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

    auto& state = presenceState_[peerId];
    if (state != PresenceState::CONNECTED) {
        state = PresenceState::CONNECTED;
        emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                                  peerId,
                                                                  static_cast<int>(
                                                                      PresenceState::CONNECTED),
                                                                  "");
    }
}

void
JamiAccount::shutdownSIPConnection(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                                   const std::string& peerId,
                                   const DeviceId& deviceId)
{
    std::unique_lock lk(sipConnsMtx_);
    SipConnectionKey key(peerId, deviceId);
    auto it = sipConns_.find(key);
    if (it != sipConns_.end()) {
        auto& conns = it->second;
        conns.erase(std::remove_if(conns.begin(),
                                   conns.end(),
                                   [&](auto v) { return v.channel == channel; }),
                    conns.end());
        if (conns.empty()) {
            sipConns_.erase(it);
            // If all devices of an account are disconnected, we need to update the presence state
            auto it = std::find_if(sipConns_.begin(), sipConns_.end(), [&](const auto& v) {
                return v.first.first == peerId;
            });
            if (it == sipConns_.end()) {
                auto& state = presenceState_[peerId];
                if (state == PresenceState::CONNECTED) {
                    std::lock_guard lock(buddyInfoMtx);
                    auto buddy = trackedBuddies_.find(dht::InfoHash(peerId));
                    if (buddy == trackedBuddies_.end())
                        state = PresenceState::DISCONNECTED;
                    else
                        state = buddy->second.devices_cnt > 0 ? PresenceState::AVAILABLE
                                                              : PresenceState::DISCONNECTED;
                    emitSignal<libjami::PresenceSignal::NewBuddyNotification>(getAccountID(),
                                                                              peerId,
                                                                              static_cast<int>(
                                                                                  state),
                                                                              "");
                }
            }
        }
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

    if (auto cm = convModule())
        cm->monitor();
    std::lock_guard lkCM(connManagerMtx_);
    if (connectionManager_)
        connectionManager_->monitor();
}

std::vector<std::map<std::string, std::string>>
JamiAccount::getConnectionList(const std::string& conversationId)
{
    std::lock_guard lkCM(connManagerMtx_);
    if (connectionManager_ && conversationId.empty()) {
        return connectionManager_->getConnectionList();
    } else if (connectionManager_ && convModule_) {
        std::vector<std::map<std::string, std::string>> connectionList;
        if (auto conv = convModule_->getConversation(conversationId)) {
            for (const auto& deviceId : conv->getDeviceIdList()) {
                auto connections = connectionManager_->getConnectionList(deviceId);
                connectionList.reserve(connectionList.size() + connections.size());
                std::move(connections.begin(),
                          connections.end(),
                          std::back_inserter(connectionList));
            }
        }
        return connectionList;
    } else {
        return {};
    }
}

std::vector<std::map<std::string, std::string>>
JamiAccount::getChannelList(const std::string& connectionId)
{
    std::lock_guard lkCM(connManagerMtx_);
    if (!connectionManager_)
        return {};
    return connectionManager_->getChannelList(connectionId);
}

void
JamiAccount::sendFile(const std::string& conversationId,
                      const std::filesystem::path& path,
                      const std::string& name,
                      const std::string& replyTo)
{
    if (!std::filesystem::is_regular_file(path)) {
        JAMI_ERROR("Invalid filename '{}'", path);
        return;
    }
    // NOTE: this sendMessage is in a computation thread because
    // sha3sum can take quite some time to computer if the user decide
    // to send a big file
    dht::ThreadPool::computation().run([w = weak(), conversationId, path, name, replyTo]() {
        if (auto shared = w.lock()) {
            Json::Value value;
            auto tid = jami::generateUID(shared->rand);
            value["tid"] = std::to_string(tid);
            value["displayName"] = name.empty() ? path.filename().string() : name;
            value["totalSize"] = std::to_string(fileutils::size(path));
            value["sha3sum"] = fileutils::sha3File(path);
            value["type"] = "application/data-transfer+json";

            shared->convModule()->sendMessage(
                conversationId,
                std::move(value),
                replyTo,
                true,
                [accId = shared->getAccountID(), conversationId, tid, path](
                    const std::string& commitId) {
                    // Create a symlink to answer to re-ask
                    auto filelinkPath = fileutils::get_data_dir() / accId / "conversation_data"
                                        / conversationId / (commitId + "_" + std::to_string(tid));
                    filelinkPath += path.extension();
                    if (path != filelinkPath && !std::filesystem::is_symlink(filelinkPath)) {
                        if (!fileutils::createFileLink(filelinkPath, path, true)) {
                            JAMI_WARNING(
                                "Unable to create symlink for file transfer {} - {}. Copy file",
                                filelinkPath,
                                path);
                            if (!std::filesystem::copy_file(path, filelinkPath)) {
                                JAMI_ERROR("Unable to copy file for file transfer {} - {}",
                                           filelinkPath,
                                           path);
                                // Signal to notify clients that the operation failed.
                                // The fileId field sends the filePath.
                                // libjami::DataTransferEventCode::unsupported (2) is unused elsewhere.
                                emitSignal<libjami::DataTransferSignal::DataTransferEvent>(
                                    accId,
                                    conversationId,
                                    commitId,
                                    path.u8string(),
                                    uint32_t(libjami::DataTransferEventCode::invalid));
                            } else {
                                // Signal to notify clients that the file is copied and can be
                                // safely deleted. The fileId field sends the filePath.
                                // libjami::DataTransferEventCode::created (1) is unused elsewhere.
                                emitSignal<libjami::DataTransferSignal::DataTransferEvent>(
                                    accId,
                                    conversationId,
                                    commitId,
                                    path.u8string(),
                                    uint32_t(libjami::DataTransferEventCode::created));
                            }
                        } else {
                            emitSignal<libjami::DataTransferSignal::DataTransferEvent>(
                                accId,
                                conversationId,
                                commitId,
                                path.u8string(),
                                uint32_t(libjami::DataTransferEventCode::created));
                        }
                    }
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
                          size_t end,
                          const std::string& sha3Sum,
                          uint64_t lastWriteTime,
                          std::function<void()> onFinished)
{
    std::string modified;
    if (lastWriteTime != 0) {
        modified = fmt::format("&modified={}", lastWriteTime);
    }
    auto fid = fileId == "profile.vcf" ? fmt::format("profile.vcf?sha3={}{}", sha3Sum, modified)
                                       : fileId;
    auto channelName = conversationId.empty() ? fmt::format("{}profile.vcf?sha3={}{}",
                                                            DATA_TRANSFER_SCHEME,
                                                            sha3Sum,
                                                            modified)
                                              : fmt::format("{}{}/{}/{}",
                                                            DATA_TRANSFER_SCHEME,
                                                            conversationId,
                                                            currentDeviceId(),
                                                            fid);
    std::lock_guard lkCM(connManagerMtx_);
    if (!connectionManager_)
        return;
    connectionManager_
        ->connectDevice(DeviceId(deviceId),
                        channelName,
                        [this,
                         conversationId,
                         path = std::move(path),
                         fileId,
                         interactionId,
                         start,
                         end,
                         onFinished = std::move(
                             onFinished)](std::shared_ptr<dhtnet::ChannelSocket> socket,
                                          const DeviceId&) {
                            if (!socket)
                                return;
                            dht::ThreadPool::io().run([w = weak(),
                                                       path = std::move(path),
                                                       socket = std::move(socket),
                                                       conversationId = std::move(conversationId),
                                                       fileId,
                                                       interactionId,
                                                       start,
                                                       end,
                                                       onFinished = std::move(onFinished)] {
                                if (auto shared = w.lock())
                                    if (auto dt = shared->dataTransfer(conversationId))
                                        dt->transferFile(socket,
                                                         fileId,
                                                         interactionId,
                                                         path,
                                                         start,
                                                         end,
                                                         std::move(onFinished));
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
        std::lock_guard lkCM(connManagerMtx_);
        if (!connectionManager_)
            return;

        auto channelName = fmt::format("{}{}/{}/{}",
                                       DATA_TRANSFER_SCHEME,
                                       conversationId,
                                       currentDeviceId(),
                                       fileId);
        if (start != 0 || end != 0) {
            channelName += fmt::format("?start={}&end={}", start, end);
        }
        // We can avoid to negotiate new sessions, as the file notif
        // probably came from an online device or last connected device.
        connectionManager_->connectDevice(
            did,
            channelName,
            [w = weak(),
             conversationId,
             fileId,
             interactionId](std::shared_ptr<dhtnet::ChannelSocket> channel, const DeviceId&) {
                if (!channel)
                    return;
                dht::ThreadPool::io().run([w, conversationId, channel, fileId, interactionId] {
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
        // Only ask for connected devices. For others we will attempt
        // with new peer online
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
    std::lock_guard lkCM(connManagerMtx_);
    if (!connectionManager_)
        return;

    auto channelName = fmt::format("{}{}/profile/{}.vcf",
                                   DATA_TRANSFER_SCHEME,
                                   conversationId,
                                   memberUri);
    // We can avoid to negotiate new sessions, as the file notif
    // probably came from an online device or last connected device.
    connectionManager_->connectDevice(
        DeviceId(deviceId),
        channelName,
        [this, conversationId](std::shared_ptr<dhtnet::ChannelSocket> channel, const DeviceId&) {
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
    if (!nonSwarmTransferManager_)
        nonSwarmTransferManager_
            = std::make_shared<TransferManager>(accountID_,
                                                config().username,
                                                "",
                                                dht::crypto::getDerivedRandomEngine(rand));
    if (!connectionManager_) {
        auto connectionManagerConfig = std::make_shared<dhtnet::ConnectionManager::Config>();
        connectionManagerConfig->ioContext = Manager::instance().ioContext();
        connectionManagerConfig->dht = dht();
        connectionManagerConfig->certStore = certStore_;
        connectionManagerConfig->id = identity();
        connectionManagerConfig->upnpCtrl = upnpCtrl_;
        connectionManagerConfig->turnServer = config().turnServer;
        connectionManagerConfig->upnpEnabled = config().upnpEnabled;
        connectionManagerConfig->turnServerUserName = config().turnServerUserName;
        connectionManagerConfig->turnServerPwd = config().turnServerPwd;
        connectionManagerConfig->turnServerRealm = config().turnServerRealm;
        connectionManagerConfig->turnEnabled = config().turnEnabled;
        connectionManagerConfig->cachePath = cachePath_;
        connectionManagerConfig->logger = Logger::dhtLogger();
        connectionManagerConfig->factory = Manager::instance().getIceTransportFactory();
        connectionManagerConfig->turnCache = turnCache_;
        connectionManagerConfig->rng = std::make_unique<std::mt19937_64>(
            dht::crypto::getDerivedRandomEngine(rand));
        connectionManager_ = std::make_unique<dhtnet::ConnectionManager>(connectionManagerConfig);
        channelHandlers_[Uri::Scheme::SWARM]
            = std::make_unique<SwarmChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::GIT]
            = std::make_unique<ConversationChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::SYNC]
            = std::make_unique<SyncChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::DATA_TRANSFER]
            = std::make_unique<TransferChannelHandler>(shared(), *connectionManager_.get());
        channelHandlers_[Uri::Scheme::MESSAGE]
            = std::make_unique<MessageChannelHandler>(shared(), *connectionManager_.get());

#if TARGET_OS_IOS
        connectionManager_->oniOSConnected([&](const std::string& connType, dht::InfoHash peer_h) {
            if ((connType == "videoCall" || connType == "audioCall")
                && jami::Manager::instance().isIOSExtension) {
                bool hasVideo = connType == "videoCall";
                emitSignal<libjami::ConversationSignal::CallConnectionRequest>("",
                                                                               peer_h.toString(),
                                                                               hasVideo);
                return true;
            }
            return false;
        });
#endif
    }
}

void
JamiAccount::updateUpnpController()
{
    Account::updateUpnpController();
    if (connectionManager_) {
        auto config = connectionManager_->getConfig();
        if (config)
            config->upnpCtrl = upnpCtrl_;
    }
}

} // namespace jami
