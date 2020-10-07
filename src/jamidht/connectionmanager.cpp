/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "connectionmanager.h"
#include "jamiaccount.h"
#include "account_const.h"
#include "account_manager.h"
#include "manager.h"
#include "ice_transport.h"
#include "peer_connection.h"
#include "multiplexed_socket.h"
#include "logger.h"

#include <opendht/thread_pool.h>
#include <opendht/value.h>

#include <mutex>
#include <map>
#include <condition_variable>
#include <set>

static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT {10};
static constexpr std::chrono::seconds DHT_MSG_TIMEOUT {30};
static constexpr std::chrono::seconds SOCK_TIMEOUT {10};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

namespace jami {

struct ConnectionInfo
{
    std::condition_variable responseCv_ {};
    std::atomic_bool responseReceived_ {false};
    PeerConnectionRequest response_ {};
    std::mutex mutex_ {};
    std::unique_ptr<IceTransport> ice_ {nullptr};
};

class ConnectionManager::Impl : public std::enable_shared_from_this<ConnectionManager::Impl>
{
public:
    explicit Impl(JamiAccount& account)
        : account {account}
    {}
    ~Impl() {}

    void removeUnusedConnections(const std::string& deviceId = "")
    {
        {
            std::lock_guard<std::mutex> lk(nonReadySocketsMutex_);
            for (auto& listSocks : nonReadySockets_) {
                if (!deviceId.empty() && listSocks.first != deviceId)
                    continue;
                for (auto& tlsSock : listSocks.second) {
                    if (tlsSock.second)
                        tlsSock.second->shutdown();
                }
            }
            if (deviceId.empty()) {
                dht::ThreadPool::io().run([nrs = std::make_shared<decltype(nonReadySockets_)>(
                                               std::move(nonReadySockets_))] { nrs->clear(); });
            } else {
                nonReadySockets_.erase(deviceId);
            }
        }
        {
            std::lock_guard<std::mutex> lk(msocketsMutex_);
            for (auto& listSocks : multiplexedSockets_) {
                if (!deviceId.empty() && listSocks.first != deviceId)
                    continue;
                for (auto& mxSock : listSocks.second) {
                    if (mxSock.second)
                        mxSock.second->shutdown();
                }
            }
            if (deviceId.empty()) {
                dht::ThreadPool::io().run([ms = std::make_shared<decltype(multiplexedSockets_)>(
                                               std::move(multiplexedSockets_))] { ms->clear(); });
            } else {
                multiplexedSockets_.erase(deviceId);
            }
        }
    }
    void shutdown()
    {
        if (isDestroying_)
            return;
        isDestroying_ = true;
        {
            std::lock_guard<std::mutex> lk(connectCbsMtx_);
            pendingCbs_.clear();
        }
        for (auto& connection : connectionsInfos_) {
            for (auto& info : connection.second) {
                if (info.second.ice_) {
                    info.second.ice_->cancelOperations();
                    info.second.ice_->stop();
                }
                info.second.responseCv_.notify_all();
            }
        }
        // This is called when the account is only disabled.
        // Move this on the thread pool because each
        // IceTransport takes 500ms to delete, and it's sequential
        // So, it can increase quickly the time to unregister an account
        dht::ThreadPool::io().run([co = std::make_shared<decltype(connectionsInfos_)>(
                                       std::move(connectionsInfos_))] { co->clear(); });
        removeUnusedConnections();
    }

    void connectDeviceStartIce(const std::string& deviceId, const dht::Value::Id& vid);
    void connectDeviceOnNegoDone(const std::string& deviceId,
                                 const std::string& name,
                                 const dht::Value::Id& vid,
                                 const std::shared_ptr<dht::crypto::Certificate>& cert);
    void connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb);
    /**
     * Send a ChannelRequest on the TLS socket. Triggers cb when ready
     * @param sock      socket used to send the request
     * @param name      channel's name
     * @param vid       channel's id
     * @param deviceId  to identify the linked ConnectCallback
     */
    void sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock,
                            const std::string& name,
                            const std::string& deviceId,
                            const dht::Value::Id& vid);
    /**
     * Triggered when a PeerConnectionRequest comes from the DHT
     */
    void answerTo(IceTransport& ice, const dht::Value::Id& id, const dht::InfoHash& from);
    void onRequestStartIce(const PeerConnectionRequest& req);
    void onRequestOnNegoDone(const PeerConnectionRequest& req);
    void onDhtPeerRequest(const PeerConnectionRequest& req,
                          const std::shared_ptr<dht::crypto::Certificate>& cert);

    void addNewMultiplexedSocket(const std::string& deviceId,
                                 const dht::Value::Id& vid,
                                 std::unique_ptr<TlsSocketEndpoint>&& tlsSocket);
    void onPeerResponse(const PeerConnectionRequest& req);
    void onDhtConnected(const std::string& deviceId);

    bool hasPublicIp(const ICESDP& sdp)
    {
        for (const auto& cand : sdp.rem_candidates)
            if (cand.type == PJ_ICE_CAND_TYPE_SRFLX)
                return true;
        return false;
    }

    JamiAccount& account;

    // Note: Someone can ask multiple sockets, so to avoid any race condition,
    // each device can have multiple multiplexed sockets.
    std::map<std::string /* device id */, std::map<dht::Value::Id /* uid */, ConnectionInfo>>
        connectionsInfos_ {};
    // Used to store currently non ready TLS Socket
    std::mutex nonReadySocketsMutex_ {};
    std::map<std::string /* device id */,
             std::map<dht::Value::Id /* uid */, std::unique_ptr<TlsSocketEndpoint>>>
        nonReadySockets_ {};
    std::mutex msocketsMutex_ {};
    // Note: Multiplexed sockets is also stored in ChannelSockets, so has to be shared_ptr
    std::map<std::string /* device id */,
             std::map<dht::Value::Id /* uid */, std::shared_ptr<MultiplexedSocket>>>
        multiplexedSockets_ {};

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>>
        certMap_ {};
    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);

    ChannelRequestCallback channelReqCb_ {};
    ConnectionReadyCallback connReadyCb_ {};
    onICERequestCallback iceReqCb_ {};

    std::mutex connectCbsMtx_ {};
    std::map<std::pair<std::string, dht::Value::Id>, ConnectCallback> pendingCbs_ {};

    std::shared_ptr<ConnectionManager::Impl> shared()
    {
        return std::static_pointer_cast<ConnectionManager::Impl>(shared_from_this());
    }
    std::shared_ptr<ConnectionManager::Impl const> shared() const
    {
        return std::static_pointer_cast<ConnectionManager::Impl const>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager::Impl> weak()
    {
        return std::static_pointer_cast<ConnectionManager::Impl>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager::Impl const> weak() const
    {
        return std::static_pointer_cast<ConnectionManager::Impl const>(shared_from_this());
    }

    std::atomic_bool isDestroying_ {false};
};

void
ConnectionManager::Impl::connectDeviceStartIce(const std::string& deviceId,
                                               const dht::Value::Id& vid)
{
    auto tit = connectionsInfos_.find(deviceId);
    if (tit == connectionsInfos_.end())
        return;
    auto it = tit->second.find(vid);
    if (it == tit->second.end())
        return;

    std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
    std::unique_lock<std::mutex> lk {it->second.mutex_};
    auto& ice = it->second.ice_;

    auto onError = [&]() {
        ice.reset();
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto cbIt = pendingCbs_.find(cbId);
        if (cbIt != pendingCbs_.end()) {
            if (cbIt->second)
                cbIt->second(nullptr);
            pendingCbs_.erase(cbIt);
        }
    };

    if (!ice) {
        JAMI_ERR("No ICE detected");
        onError();
        return;
    }

    account.registerDhtAddress(*ice);

    auto iceAttributes = ice->getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto& addr : ice->getLocalCandidates(0)) {
        icemsg << addr << "\n";
    }

    // Prepare connection request as a DHT message
    PeerConnectionRequest val;

    val.id = vid; /* Random id for the message unicity */
    val.ice_msg = icemsg.str();
    auto value = std::make_shared<dht::Value>(std::move(val));
    value->user_type = "peer_request";

    // Send connection request through DHT
    JAMI_DBG() << account << "Request connection to " << deviceId;
    account.dht()->putEncrypted(dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
                                dht::InfoHash(deviceId),
                                value);

    // Wait for call to onResponse() operated by DHT
    if (isDestroying_)
        return; // This avoid to wait new negotiation when destroying
    it->second.responseCv_.wait_for(lk, DHT_MSG_TIMEOUT);
    if (isDestroying_)
        return; // The destructor can wake a pending wait here.
    if (!it->second.responseReceived_) {
        JAMI_ERR("no response from DHT to E2E request.");
        onError();
        return;
    }

    auto& response = it->second.response_;
    if (!ice)
        return;
    auto sdp = IceTransport::parse_SDP(response.ice_msg, *ice);
    auto hasPubIp = hasPublicIp(sdp);
    if (!hasPubIp)
        ice->setInitiatorSession();
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_WARN("[Account:%s] start ICE failed", account.getAccountID().c_str());
        onError();
    }
}

void
ConnectionManager::Impl::connectDeviceOnNegoDone(
    const std::string& deviceId,
    const std::string& name,
    const dht::Value::Id& vid,
    const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto tit = connectionsInfos_.find(deviceId);
    if (tit == connectionsInfos_.end())
        return;
    auto it = tit->second.find(vid);
    if (it == tit->second.end())
        return;

    std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
    std::unique_lock<std::mutex> lk {it->second.mutex_};
    auto& ice = it->second.ice_;

    auto onError = [&]() {
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto cbIt = pendingCbs_.find(cbId);
        if (cbIt != pendingCbs_.end()) {
            if (cbIt->second)
                cbIt->second(nullptr);
            pendingCbs_.erase(cbIt);
        }
    };

    if (!ice || !ice->isRunning()) {
        JAMI_ERR("No ICE detected or not running");
        onError();
        return;
    }

    // Build socket
    std::lock_guard<std::mutex> lknrs(nonReadySocketsMutex_);
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(
                                                            std::move(ice)),
                                                        true);

    // Negotiate a TLS session
    JAMI_DBG() << account << "Start TLS session";
    auto tlsSocket = std::make_unique<TlsSocketEndpoint>(std::move(endpoint),
                                                         account.identity(),
                                                         account.dhParams(),
                                                         *cert);

    auto& nonReadyIt = nonReadySockets_[deviceId][vid];
    nonReadyIt = std::move(tlsSocket);
    nonReadyIt->setOnReady([w = weak(),
                            deviceId = std::move(deviceId),
                            vid = std::move(vid),
                            name = std::move(name)](bool ok) {
        auto sthis = w.lock();
        if (!sthis)
            return;
        auto mSockIt = sthis->multiplexedSockets_[deviceId];
        if (mSockIt.find(vid) != mSockIt.end())
            return;
        if (!ok) {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId;
            std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
            std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
            auto cbIt = sthis->pendingCbs_.find(cbId);
            if (cbIt != sthis->pendingCbs_.end()) {
                if (cbIt->second)
                    cbIt->second(nullptr);
                sthis->pendingCbs_.erase(cbIt);
            }
        } else {
            // The socket is ready, store it in multiplexedSockets_
            std::lock_guard<std::mutex> lkmSockets(sthis->msocketsMutex_);
            std::lock_guard<std::mutex> lknrs(sthis->nonReadySocketsMutex_);
            auto nonReadyIt = sthis->nonReadySockets_.find(deviceId);
            if (nonReadyIt != sthis->nonReadySockets_.end()) {
                sthis->addNewMultiplexedSocket(deviceId, vid, std::move(nonReadyIt->second[vid]));
                nonReadyIt->second.erase(vid);
                if (nonReadyIt->second.empty()) {
                    sthis->nonReadySockets_.erase(nonReadyIt);
                }
            }
            // Finally, open the channel
            auto mxSockIt = sthis->multiplexedSockets_.at(deviceId);
            if (!mxSockIt.empty())
                sthis->sendChannelRequest(mxSockIt.rbegin()->second, name, deviceId, vid);
        }
    });
}

void
ConnectionManager::Impl::connectDevice(const std::string& deviceId,
                                       const std::string& name,
                                       ConnectCallback cb)
{
    if (!account.dht()) {
        cb(nullptr);
        return;
    }
    account.findCertificate(
        dht::InfoHash(deviceId),
        [w = weak(), deviceId, name, cb = std::move(cb)](
            const std::shared_ptr<dht::crypto::Certificate>& cert) {
            if (!cert) {
                JAMI_ERR("Invalid certificate found for device %s", deviceId.c_str());
                cb(nullptr);
                return;
            }

            // Avoid dht operation in a DHT callback to avoid deadlocks
            runOnMainThread([w,
                             deviceId = std::move(deviceId),
                             name = std::move(name),
                             cert = std::move(cert),
                             cb = std::move(cb)] {
                auto sthis = w.lock();
                if (!sthis || sthis->isDestroying_) {
                    cb(nullptr);
                    return;
                }
                auto vid = ValueIdDist()(sthis->account.rand);
                std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
                {
                    std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                    auto cbIt = sthis->pendingCbs_.find(cbId);
                    if (cbIt != sthis->pendingCbs_.end()) {
                        JAMI_WARN("Already have a current callback for same channel");
                        cbIt->second = std::move(cb);
                    } else {
                        sthis->pendingCbs_[cbId] = std::move(cb);
                    }
                }

                {
                    // Test if a socket already exists for this device
                    std::lock_guard<std::mutex> lk(sthis->msocketsMutex_);
                    auto it = sthis->multiplexedSockets_.find(deviceId);
                    if (it != sthis->multiplexedSockets_.end() && !it->second.empty()) {
                        JAMI_DBG("Peer already connected. Add a new channel");
                        sthis->sendChannelRequest(it->second.rbegin()->second, name, deviceId, vid);
                        return;
                    }
                }
                // If no socket exists, we need to initiate an ICE connection.
                auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
                auto ice_config = sthis->account.getIceOptions();
                ice_config.tcpEnable = true;
                ice_config.onInitDone = [w,
                                         cbId,
                                         deviceId = std::move(deviceId),
                                         name = std::move(name),
                                         cert = std::move(cert),
                                         vid](bool ok) {
                    auto sthis = w.lock();
                    if (!sthis)
                        return;
                    if (!ok) {
                        JAMI_ERR("Cannot initialize ICE session.");
                        std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                        auto cbIt = sthis->pendingCbs_.find(cbId);
                        if (cbIt != sthis->pendingCbs_.end()) {
                            if (cbIt->second)
                                cbIt->second(nullptr);
                            sthis->pendingCbs_.erase(cbIt);
                        }
                        return;
                    }

                    dht::ThreadPool::io().run(
                        [w = std::move(w), deviceId = std::move(deviceId), vid = std::move(vid)] {
                            auto sthis = w.lock();
                            if (!sthis)
                                return;
                            sthis->connectDeviceStartIce(deviceId, vid);
                        });
                };
                ice_config.onNegoDone = [w,
                                         cbId,
                                         deviceId = std::move(deviceId),
                                         name = std::move(name),
                                         cert = std::move(cert),
                                         vid](bool ok) {
                    auto sthis = w.lock();
                    if (!sthis)
                        return;
                    if (!ok) {
                        JAMI_ERR("ICE negotiation failed.");
                        std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                        auto cbIt = sthis->pendingCbs_.find(cbId);
                        if (cbIt != sthis->pendingCbs_.end()) {
                            if (cbIt->second)
                                cbIt->second(nullptr);
                            sthis->pendingCbs_.erase(cbIt);
                        }
                        return;
                    }

                    dht::ThreadPool::io().run([w = std::move(w),
                                               deviceId = std::move(deviceId),
                                               name = std::move(name),
                                               cert = std::move(cert),
                                               vid = std::move(vid)] {
                        auto sthis = w.lock();
                        if (!sthis)
                            return;
                        sthis->connectDeviceOnNegoDone(deviceId, name, vid, cert);
                    });
                };

                auto& connectionInfo = sthis->connectionsInfos_[deviceId][vid];
                std::unique_lock<std::mutex> lk {connectionInfo.mutex_};
                connectionInfo.ice_ = iceTransportFactory
                                          .createUTransport(sthis->account.getAccountID().c_str(),
                                                            1,
                                                            false,
                                                            ice_config);

                if (!connectionInfo.ice_) {
                    JAMI_ERR("Cannot initialize ICE session.");
                    std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                    auto cbIt = sthis->pendingCbs_.find(cbId);
                    if (cbIt != sthis->pendingCbs_.end()) {
                        if (cbIt->second)
                            cbIt->second(nullptr);
                        sthis->pendingCbs_.erase(cbIt);
                    }
                    return;
                }
            });
        });
}

void
ConnectionManager::Impl::sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock,
                                            const std::string& name,
                                            const std::string& deviceId,
                                            const dht::Value::Id& vid)
{
    auto channelSock = sock->addChannel(name);
    ChannelRequest val;
    val.name = channelSock->name();
    val.state = ChannelRequestState::REQUEST;
    val.channel = channelSock->channel();
    std::stringstream ss;
    msgpack::pack(ss, val);
    auto toSend = ss.str();
    sock->setOnChannelReady(channelSock->channel(), [channelSock, deviceId, vid, w = weak()]() {
        auto shared = w.lock();
        if (!shared)
            return;
        std::lock_guard<std::mutex> lk(shared->connectCbsMtx_);
        std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
        auto cbIt = shared->pendingCbs_.find(cbId);
        if (cbIt != shared->pendingCbs_.end()) {
            if (cbIt->second)
                cbIt->second(channelSock);
            shared->pendingCbs_.erase(cbIt);
        }
    });
    std::error_code ec;
    int res = sock->write(CONTROL_CHANNEL,
                          reinterpret_cast<const uint8_t*>(&toSend[0]),
                          toSend.size(),
                          ec);
    if (res < 0) {
        // TODO check if we should handle errors here
        JAMI_ERR("sendChannelRequest failed - error: %s", ec.message().c_str());
    }
}

void
ConnectionManager::Impl::onPeerResponse(const PeerConnectionRequest& req)
{
    auto device = req.from;
    JAMI_INFO() << account << " New response received from " << device.toString().c_str();
    auto& infos = connectionsInfos_[device.toString().c_str()];
    auto it = infos.find(req.id);
    if (it == infos.end()) {
        JAMI_WARN() << account << " respond received, but cannot find request";
        return;
    }
    auto& connectionInfo = it->second;
    connectionInfo.responseReceived_ = true;
    connectionInfo.response_ = std::move(req);
    connectionInfo.responseCv_.notify_one();
}

void
ConnectionManager::Impl::onDhtConnected(const std::string& deviceId)
{
    if (!account.dht())
        return;
    account.dht()->listen<PeerConnectionRequest>(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
        [w = weak()](PeerConnectionRequest&& req) {
            auto shared = w.lock();
            if (!shared)
                return false;
            if (shared->account.isMessageTreated(to_hex_string(req.id))) {
                // Message already treated. Just ignore
                return true;
            }
            if (req.isAnswer) {
                shared->onPeerResponse(req);
            } else {
                // Async certificate checking
                shared->account.findCertificate(
                    req.from,
                    [w, req = std::move(req)](
                        const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                        auto shared = w.lock();
                        if (!shared)
                            return;
                        dht::InfoHash peer_h;
                        if (AccountManager::foundPeerDevice(cert, peer_h)) {
                            shared->onDhtPeerRequest(req, cert);
                        } else {
                            JAMI_WARN()
                                << shared->account << "Rejected untrusted connection request from "
                                << req.from;
                        }
                    });
            }
            return true;
        },
        dht::Value::UserTypeFilter("peer_request"));
}

void
ConnectionManager::Impl::answerTo(IceTransport& ice,
                                  const dht::Value::Id& id,
                                  const dht::InfoHash& from)
{
    // NOTE: This is a shortest version of a real SDP message to save some bits
    auto iceAttributes = ice.getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto& addr : ice.getLocalCandidates(0)) {
        icemsg << addr << "\n";
    }

    // Send PeerConnection response
    PeerConnectionRequest val;
    val.id = id;
    val.ice_msg = icemsg.str();
    val.isAnswer = true;
    auto value = std::make_shared<dht::Value>(std::move(val));
    value->user_type = "peer_request";

    JAMI_DBG() << account << "[CNX] connection accepted, DHT reply to " << from;
    account.dht()->putEncrypted(dht::InfoHash::get(PeerConnectionRequest::key_prefix
                                                   + from.toString()),
                                from,
                                value);
}

void
ConnectionManager::Impl::onRequestStartIce(const PeerConnectionRequest& req)
{
    auto tit = connectionsInfos_.find(req.from.toString());
    if (tit == connectionsInfos_.end())
        return;
    auto it = tit->second.find(req.id);
    if (it == tit->second.end())
        return;

    std::unique_lock<std::mutex> lk {it->second.mutex_};
    auto& ice = it->second.ice_;
    if (!ice) {
        JAMI_ERR("No ICE detected");
        if (connReadyCb_)
            connReadyCb_(req.from.toString(), "", nullptr);
        return;
    }

    account.registerDhtAddress(*ice);

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    auto hasPubIp = hasPublicIp(sdp);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN", account.getAccountID().c_str());
        ice = nullptr;
        if (connReadyCb_)
            connReadyCb_(req.from.toString(), "", nullptr);
        return;
    }

    if (hasPubIp)
        answerTo(*ice, req.id, req.from);
}

void
ConnectionManager::Impl::onRequestOnNegoDone(const PeerConnectionRequest& req)
{
    auto deviceId = req.from.toString();
    auto tit = connectionsInfos_.find(deviceId);
    if (tit == connectionsInfos_.end())
        return;
    auto it = tit->second.find(req.id);
    if (it == tit->second.end())
        return;

    std::unique_lock<std::mutex> lk {it->second.mutex_};
    auto& ice = it->second.ice_;
    if (!ice) {
        JAMI_ERR("No ICE detected");
        if (connReadyCb_)
            connReadyCb_(deviceId, "", nullptr);
        return;
    }

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    auto hasPubIp = hasPublicIp(sdp);
    if (!hasPubIp)
        answerTo(*ice, req.id, req.from);

    // Build socket
    std::lock_guard<std::mutex> lknrs(nonReadySocketsMutex_);
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(
                                                            std::move(ice)),
                                                        false);

    // init TLS session
    auto ph = req.from;
    auto tlsSocket = std::make_unique<TlsSocketEndpoint>(
        std::move(endpoint),
        account.identity(),
        account.dhParams(),
        [ph, w = weak()](const dht::crypto::Certificate& cert) {
            auto shared = w.lock();
            if (!shared)
                return false;
            dht::InfoHash peer_h;
            return shared->validatePeerCertificate(cert, peer_h) && peer_h == ph;
        });

    auto& nonReadyIt = nonReadySockets_[deviceId][req.id];
    nonReadyIt = std::move(tlsSocket);
    nonReadyIt->setOnReady([w = weak(), deviceId, vid = std::move(req.id)](bool ok) {
        auto shared = w.lock();
        if (!shared)
            return;
        if (shared->multiplexedSockets_[deviceId].find(vid)
            != shared->multiplexedSockets_[deviceId].end())
            return;
        if (!ok) {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId;
            if (shared->connReadyCb_)
                shared->connReadyCb_(deviceId, "", nullptr);
        } else {
            // The socket is ready, store it in multiplexedSockets_
            std::lock_guard<std::mutex> lk(shared->msocketsMutex_);
            std::lock_guard<std::mutex> lknrs(shared->nonReadySocketsMutex_);
            auto nonReadyIt = shared->nonReadySockets_.find(deviceId);
            if (nonReadyIt != shared->nonReadySockets_.end()) {
                JAMI_DBG("Connection to %s is ready", deviceId.c_str());
                shared->addNewMultiplexedSocket(deviceId, vid, std::move(nonReadyIt->second[vid]));
                nonReadyIt->second.erase(vid);
                if (nonReadyIt->second.empty()) {
                    shared->nonReadySockets_.erase(nonReadyIt);
                }
            }
        }
    });
}

void
ConnectionManager::Impl::onDhtPeerRequest(const PeerConnectionRequest& req,
                                          const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto deviceId = req.from.toString();
    JAMI_INFO() << account << "New connection requested by " << deviceId.c_str();
    if (!iceReqCb_ || !iceReqCb_(deviceId)) {
        JAMI_INFO("[Account:%s] refuse connection from %s",
                  account.getAccountID().c_str(),
                  deviceId.c_str());
        return;
    }

    auto crt = cert; // This copy the shared_ptr for gcc 6
    certMap_.emplace(cert->getId(), std::make_pair(crt, dht::InfoHash(deviceId)));

    // Because the connection is accepted, create an ICE socket.
    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    struct IceReady
    {
        std::mutex mtx {};
        std::condition_variable cv {};
        bool ready {false};
    };
    auto iceReady = std::make_shared<IceReady>();
    auto ice_config = account.getIceOptions();
    ice_config.tcpEnable = true;
    ice_config.onRecvReady = [iceReady]() {
        auto& ir = *iceReady;
        std::lock_guard<std::mutex> lk {ir.mtx};
        ir.ready = true;
        ir.cv.notify_one();
    };
    ice_config.onInitDone = [w = weak(), deviceId, req](bool ok) {
        auto shared = w.lock();
        if (!shared)
            return;
        if (!ok) {
            JAMI_ERR("Cannot initialize ICE session.");
            if (shared->connReadyCb_)
                shared->connReadyCb_(deviceId, "", nullptr);
            return;
        }

        dht::ThreadPool::io().run([w = std::move(w), deviceId, req = std::move(req)] {
            auto shared = w.lock();
            if (!shared)
                return;
            shared->onRequestStartIce(req);
        });
    };

    ice_config.onNegoDone = [w = weak(), deviceId, req](bool ok) {
        auto shared = w.lock();
        if (!shared)
            return;
        if (!ok) {
            JAMI_ERR("ICE negotiation failed");
            if (shared->connReadyCb_)
                shared->connReadyCb_(deviceId, "", nullptr);
            return;
        }

        dht::ThreadPool::io().run([w = std::move(w), deviceId, req = std::move(req)] {
            auto shared = w.lock();
            if (!shared)
                return;
            shared->onRequestOnNegoDone(req);
        });
    };

    // 1. Create a new Multiplexed Socket

    // Negotiate a new ICE socket
    auto& connectionInfo = connectionsInfos_[deviceId][req.id];
    std::unique_lock<std::mutex> lk {connectionInfo.mutex_};
    connectionInfo.ice_ = iceTransportFactory.createUTransport(account.getAccountID().c_str(),
                                                               1,
                                                               true,
                                                               ice_config);
    if (not connectionInfo.ice_) {
        JAMI_ERR("Cannot initialize ICE session.");
        if (connReadyCb_)
            connReadyCb_(deviceId, "", nullptr);
        return;
    }
}

void
ConnectionManager::Impl::addNewMultiplexedSocket(const std::string& deviceId,
                                                 const dht::Value::Id& vid,
                                                 std::unique_ptr<TlsSocketEndpoint>&& tlsSocket)
{
    // mSocketsMutex_ MUST be locked
    auto mSock = std::make_shared<MultiplexedSocket>(deviceId, std::move(tlsSocket));
    mSock->setOnReady(
        [w = weak()](const std::string& deviceId, const std::shared_ptr<ChannelSocket>& socket) {
            if (auto sthis = w.lock())
                if (sthis->connReadyCb_)
                    sthis->connReadyCb_(deviceId, socket->name(), socket);
        });
    mSock->setOnRequest(
        [w = weak()](const std::string& deviceId, const uint16_t&, const std::string& name) {
            if (auto sthis = w.lock())
                if (sthis->channelReqCb_)
                    return sthis->channelReqCb_(deviceId, name);
            return false;
        });
    mSock->onShutdown([w = weak(), deviceId, vid]() {
        auto sthis = w.lock();
        if (!sthis)
            return;
        // Cancel current outgoing connections
        {
            std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
            if (!sthis->pendingCbs_.empty()) {
                auto it = sthis->pendingCbs_.begin();
                while (it != sthis->pendingCbs_.end()) {
                    if (it->first.first == deviceId && it->first.second == vid) {
                        it->second(nullptr);
                        it = sthis->pendingCbs_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        dht::ThreadPool::io().run([w, deviceId, vid] {
            auto sthis = w.lock();
            if (!sthis)
                return;
            // Delete the socket
            std::lock_guard<std::mutex> lk(sthis->msocketsMutex_);
            auto mxSockIt = sthis->multiplexedSockets_.find(deviceId);
            if (mxSockIt != sthis->multiplexedSockets_.end()) {
                auto vidIt = mxSockIt->second.find(vid);
                if (vidIt != mxSockIt->second.end() && vidIt->second) {
                    vidIt->second->shutdown();
                }
            }

            auto connIt = sthis->connectionsInfos_.find(deviceId);
            if (connIt != sthis->connectionsInfos_.end()) {
                auto it = connIt->second.find(vid);
                if (it != connIt->second.end()) {
                    if (it->second.ice_)
                        it->second.ice_->cancelOperations();
                    connIt->second.erase(vid);
                    if (connIt->second.empty())
                        sthis->connectionsInfos_.erase(deviceId);
                }
                // This will close the TLS Session
                std::lock_guard<std::mutex> lk(sthis->nonReadySocketsMutex_);
                auto nonReadyIt = sthis->nonReadySockets_.find(deviceId);
                if (nonReadyIt != sthis->nonReadySockets_.end()) {
                    nonReadyIt->second.erase(vid);
                    if (nonReadyIt->second.empty()) {
                        sthis->nonReadySockets_.erase(nonReadyIt);
                    }
                }
            }

            if (mxSockIt != sthis->multiplexedSockets_.end()) {
                mxSockIt->second.erase(vid);
                if (mxSockIt->second.empty())
                    sthis->multiplexedSockets_.erase(mxSockIt);
            }
        });
    });
    multiplexedSockets_[deviceId][vid] = std::move(mSock);
}

bool
ConnectionManager::Impl::validatePeerCertificate(const dht::crypto::Certificate& cert,
                                                 dht::InfoHash& peer_h)
{
    const auto& iter = certMap_.find(cert.getId());
    if (iter != std::cend(certMap_)) {
        if (iter->second.first->getPacked() == cert.getPacked()) {
            peer_h = iter->second.second;
            return true;
        }
    }
    return false;
}

ConnectionManager::ConnectionManager(JamiAccount& account)
    : pimpl_ {std::make_shared<Impl>(account)}
{}

ConnectionManager::~ConnectionManager()
{
    if (pimpl_)
        pimpl_->shutdown();
}

void
ConnectionManager::connectDevice(const std::string& deviceId,
                                 const std::string& name,
                                 ConnectCallback cb)
{
    pimpl_->connectDevice(deviceId, name, std::move(cb));
}

void
ConnectionManager::closeConnectionsWith(const std::string& deviceId)
{
    {
        std::lock_guard<std::mutex> lk(pimpl_->connectCbsMtx_);
        auto it = pimpl_->pendingCbs_.begin();
        while (it != pimpl_->pendingCbs_.end()) {
            if (it->first.first == deviceId) {
                if (it->second)
                    it->second(nullptr);
                it = pimpl_->pendingCbs_.erase(it);
            } else {
                ++it;
            }
        }
    }
    auto it = pimpl_->connectionsInfos_.find(deviceId);
    if (it != pimpl_->connectionsInfos_.end()) {
        for (auto& info : it->second) {
            if (info.second.ice_) {
                info.second.ice_->cancelOperations();
                info.second.ice_->stop();
            }
            info.second.responseCv_.notify_all();
            if (info.second.ice_) {
                std::unique_lock<std::mutex> lk {info.second.mutex_};
                info.second.ice_.reset();
            }
        }
    }
    // This will close the TLS Session
    pimpl_->removeUnusedConnections(deviceId);
}

void
ConnectionManager::onDhtConnected(const std::string& deviceId)
{
    pimpl_->onDhtConnected(deviceId);
}

void
ConnectionManager::onICERequest(onICERequestCallback&& cb)
{
    pimpl_->iceReqCb_ = std::move(cb);
}

void
ConnectionManager::onChannelRequest(ChannelRequestCallback&& cb)
{
    pimpl_->channelReqCb_ = std::move(cb);
}

void
ConnectionManager::onConnectionReady(ConnectionReadyCallback&& cb)
{
    pimpl_->connReadyCb_ = std::move(cb);
}

} // namespace jami
