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
#include "logger.h"

#include <opendht/crypto.h>
#include <opendht/thread_pool.h>
#include <opendht/value.h>

#include <algorithm>
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
    // Used to store currently non ready TLS Socket
    std::unique_ptr<TlsSocketEndpoint> tls_ {nullptr};
    std::shared_ptr<MultiplexedSocket> socket_ {};
};

class ConnectionManager::Impl : public std::enable_shared_from_this<ConnectionManager::Impl>
{
public:
    using ConnectionKey = std::pair<DeviceId /* device id */, dht::Value::Id /* uid */>;

    explicit Impl(JamiAccount& account)
        : account {account}
    {}
    ~Impl() {}

    void removeUnusedConnections(const DeviceId& deviceId = {})
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        for (auto it = infos_.begin(); it != infos_.end();) {
            auto& [key, info] = *it;
            bool erased = false;
            if (info) {
                if (info->tls_)
                    info->tls_->shutdown();
                if (info->socket_)
                    info->socket_->shutdown();
                if (deviceId && key.first == deviceId) {
                    erased = true;
                    it = infos_.erase(it);
                }
            }
            if (!erased)
                ++it;
        }
        if (!deviceId) {
            dht::ThreadPool::io().run([infos = std::make_shared<decltype(infos_)>(
                                           std::move(infos_))] { infos->clear(); });
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
        {
            std::lock_guard<std::mutex> lk(infosMtx_);
            for (auto& [key, connInfo] : infos_) {
                if (!connInfo)
                    continue;
                if (connInfo->ice_) {
                    connInfo->ice_->cancelOperations();
                    connInfo->ice_->stop();
                }
                connInfo->responseCv_.notify_all();
            }
            // This is called when the account is only disabled.
            // Move this on the thread pool because each
            // IceTransport takes 500ms to delete, and it's sequential
            // So, it can increase quickly the time to unregister an account
            dht::ThreadPool::io().run(
                [co = std::make_shared<decltype(infos_)>(std::move(infos_))] { co->clear(); });
        }
        removeUnusedConnections();
    }

    void connectDeviceStartIce(const DeviceId& deviceId, const dht::Value::Id& vid);
    void connectDeviceOnNegoDone(const DeviceId& deviceId,
                                 const std::string& name,
                                 const dht::Value::Id& vid,
                                 const std::shared_ptr<dht::crypto::Certificate>& cert);
    void connectDevice(const DeviceId& deviceId, const std::string& uri, ConnectCallback cb);
    /**
     * Send a ChannelRequest on the TLS socket. Triggers cb when ready
     * @param sock      socket used to send the request
     * @param name      channel's name
     * @param vid       channel's id
     * @param deviceId  to identify the linked ConnectCallback
     */
    void sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock,
                            const std::string& name,
                            const DeviceId& deviceId,
                            const dht::Value::Id& vid);
    /**
     * Triggered when a PeerConnectionRequest comes from the DHT
     */
    void answerTo(IceTransport& ice, const dht::Value::Id& id, const DeviceId& from);
    void onRequestStartIce(const PeerConnectionRequest& req);
    void onRequestOnNegoDone(const PeerConnectionRequest& req);
    void onDhtPeerRequest(const PeerConnectionRequest& req,
                          const std::shared_ptr<dht::crypto::Certificate>& cert);

    void addNewMultiplexedSocket(const DeviceId& deviceId, const dht::Value::Id& vid);
    void onPeerResponse(const PeerConnectionRequest& req);
    void onDhtConnected(const DeviceId& deviceId);

    bool hasPublicIp(const ICESDP& sdp)
    {
        for (const auto& cand : sdp.rem_candidates)
            if (cand.type == PJ_ICE_CAND_TYPE_SRFLX)
                return true;
        return false;
    }

    JamiAccount& account;

    std::mutex infosMtx_ {};
    // Note: Someone can ask multiple sockets, so to avoid any race condition,
    // each device can have multiple multiplexed sockets.
    std::map<ConnectionKey, std::shared_ptr<ConnectionInfo>> infos_ {};

    std::shared_ptr<ConnectionInfo> getInfo(const DeviceId& deviceId, const dht::Value::Id& id)
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        auto it = infos_.find({deviceId, id});
        if (it == infos_.end())
            return {};
        return it->second;
    }

    ChannelRequestCallback channelReqCb_ {};
    ConnectionReadyCallback connReadyCb_ {};
    onICERequestCallback iceReqCb_ {};

    std::mutex connectCbsMtx_ {};
    using CallbackId = std::pair<DeviceId, dht::Value::Id>;
    std::map<CallbackId, ConnectCallback> pendingCbs_ {};
    std::map<std::shared_ptr<MultiplexedSocket>, std::set<CallbackId>> socketRelatedId_ {};

    ConnectCallback getPendingCallback(const CallbackId& cbId)
    {
        ConnectCallback ret;
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto cbIt = pendingCbs_.find(cbId);
        if (cbIt != pendingCbs_.end()) {
            ret = std::move(cbIt->second);
            pendingCbs_.erase(cbIt);
        }
        return ret;
    }

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
ConnectionManager::Impl::connectDeviceStartIce(const DeviceId& deviceId, const dht::Value::Id& vid)
{
    auto info = getInfo(deviceId, vid);
    if (!info) {
        return;
    }

    std::unique_lock<std::mutex> lk(info->mutex_);
    auto& ice = info->ice_;

    auto onError = [&]() {
        ice.reset();
        if (auto cb = getPendingCallback({deviceId, vid}))
            cb(nullptr);
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
    account.dht()->putEncrypted(dht::InfoHash::get(PeerConnectionRequest::key_prefix
                                                   + deviceId.toString()),
                                deviceId,
                                value);

    // Wait for call to onResponse() operated by DHT
    if (isDestroying_)
        return; // This avoid to wait new negotiation when destroying
    info->responseCv_.wait_for(lk, DHT_MSG_TIMEOUT);
    if (isDestroying_)
        return; // The destructor can wake a pending wait here.
    if (!info->responseReceived_) {
        JAMI_ERR("no response from DHT to E2E request.");
        onError();
        return;
    }

    auto& response = info->response_;
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
    const DeviceId& deviceId,
    const std::string& name,
    const dht::Value::Id& vid,
    const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto info = getInfo(deviceId, vid);
    if (!info)
        return;

    std::unique_lock<std::mutex> lk {info->mutex_};
    auto& ice = info->ice_;
    if (!ice || !ice->isRunning()) {
        JAMI_ERR("No ICE detected or not running");
        if (auto cb = getPendingCallback({deviceId, vid}))
            cb(nullptr);
        return;
    }

    // Build socket
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(
                                                            std::move(ice)),
                                                        true);

    // Negotiate a TLS session
    JAMI_DBG() << account << "Start TLS session";
    info->tls_ = std::make_unique<TlsSocketEndpoint>(std::move(endpoint),
                                                     account.identity(),
                                                     account.dhParams(),
                                                     *cert);

    info->tls_->setOnReady(
        [w = weak(), deviceId = std::move(deviceId), vid = std::move(vid), name = std::move(name)](
            bool ok) {
            auto sthis = w.lock();
            if (!sthis)
                return;
            auto info = sthis->getInfo(deviceId, vid);
            if (!info)
                return;
            if (!ok) {
                JAMI_ERR() << "TLS connection failure for peer " << deviceId;
                if (auto cb = sthis->getPendingCallback({deviceId, vid}))
                    cb(nullptr);
            } else {
                // The socket is ready, store it
                sthis->addNewMultiplexedSocket(deviceId, vid);
                // Finally, open the channel
                if (info->socket_)
                    sthis->sendChannelRequest(info->socket_, name, deviceId, vid);
            }
        });
}

void
ConnectionManager::Impl::connectDevice(const DeviceId& deviceId,
                                       const std::string& name,
                                       ConnectCallback cb)
{
    if (!account.dht()) {
        cb(nullptr);
        return;
    }
    account.findCertificate(
        deviceId,
        [w = weak(), deviceId, name, cb = std::move(cb)](
            const std::shared_ptr<dht::crypto::Certificate>& cert) {
            if (!cert) {
                JAMI_ERR("Invalid certificate found for device %s", deviceId.to_c_str());
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
                ConnectionKey cbId(deviceId, vid);
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

                std::shared_ptr<MultiplexedSocket> sock;
                {
                    // Test if a socket already exists for this device
                    std::lock_guard<std::mutex> lk(sthis->infosMtx_);
                    auto it = std::find_if(sthis->infos_.begin(),
                                           sthis->infos_.end(),
                                           [deviceId](const auto& item) {
                                               auto& [key, value] = item;
                                               return key.first == deviceId;
                                           });
                    if (it != sthis->infos_.end() && it->second) {
                        sock = it->second->socket_;
                    }
                }
                if (sock) {
                    JAMI_DBG("Peer already connected. Add a new channel");
                    {
                        std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                        auto it = sthis->socketRelatedId_.find(sock);
                        if (it != sthis->socketRelatedId_.end()) {
                            it->second.emplace(cbId);
                        } else {
                            sthis->socketRelatedId_.insert({sock, {cbId}});
                        }
                    }
                    sthis->sendChannelRequest(sock, name, deviceId, vid);
                    return;
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
                        if (auto cb = sthis->getPendingCallback(cbId))
                            cb(nullptr);
                        return;
                    }

                    dht::ThreadPool::io().run(
                        [w = std::move(w), deviceId = std::move(deviceId), vid = std::move(vid)] {
                            if (auto sthis = w.lock())
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
                        if (auto cb = sthis->getPendingCallback(cbId))
                            cb(nullptr);
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

                auto info = std::make_shared<ConnectionInfo>();
                {
                    std::lock_guard<std::mutex> lk(sthis->infosMtx_);
                    sthis->infos_[{deviceId, vid}] = info;
                }
                std::unique_lock<std::mutex> lk {info->mutex_};
                info->ice_ = iceTransportFactory
                                 .createUTransport(sthis->account.getAccountID().c_str(),
                                                   1,
                                                   false,
                                                   ice_config);

                if (!info->ice_) {
                    JAMI_ERR("Cannot initialize ICE session.");
                    if (auto cb = sthis->getPendingCallback(cbId))
                        cb(nullptr);
                    return;
                }
            });
        });
}

void
ConnectionManager::Impl::sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock,
                                            const std::string& name,
                                            const DeviceId& deviceId,
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
        if (auto shared = w.lock()) {
            if (auto cb = shared->getPendingCallback({deviceId, vid}))
                cb(channelSock);
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
    JAMI_INFO() << account << " New response received from " << device.to_c_str();
    auto info = getInfo(device, req.id);
    if (!info) {
        JAMI_WARN() << account << " respond received, but cannot find request";
        return;
    }
    info->responseReceived_ = true;
    info->response_ = std::move(req);
    info->responseCv_.notify_one();
}

void
ConnectionManager::Impl::onDhtConnected(const DeviceId& deviceId)
{
    if (!account.dht())
        return;
    account.dht()->listen<PeerConnectionRequest>(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId.toString()),
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
ConnectionManager::Impl::answerTo(IceTransport& ice, const dht::Value::Id& id, const DeviceId& from)
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
    auto info = getInfo(req.from, req.id);
    if (!info)
        return;

    std::unique_lock<std::mutex> lk {info->mutex_};
    auto& ice = info->ice_;
    if (!ice) {
        JAMI_ERR("No ICE detected");
        if (connReadyCb_)
            connReadyCb_(req.from, "", nullptr);
        return;
    }

    account.registerDhtAddress(*ice);

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    auto hasPubIp = hasPublicIp(sdp);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN", account.getAccountID().c_str());
        ice = nullptr;
        if (connReadyCb_)
            connReadyCb_(req.from, "", nullptr);
        return;
    }

    if (hasPubIp)
        answerTo(*ice, req.id, req.from);
}

void
ConnectionManager::Impl::onRequestOnNegoDone(const PeerConnectionRequest& req)
{
    auto info = getInfo(req.from, req.id);
    if (!info)
        return;

    std::unique_lock<std::mutex> lk {info->mutex_};
    auto& ice = info->ice_;
    if (!ice) {
        JAMI_ERR("No ICE detected");
        if (connReadyCb_)
            connReadyCb_(req.from, "", nullptr);
        return;
    }

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    auto hasPubIp = hasPublicIp(sdp);
    if (!hasPubIp)
        answerTo(*ice, req.id, req.from);

    // Build socket
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(
                                                            std::move(ice)),
                                                        false);

    // init TLS session
    auto ph = req.from;
    info->tls_ = std::make_unique<TlsSocketEndpoint>(
        std::move(endpoint),
        account.identity(),
        account.dhParams(),
        [ph, w = weak()](const dht::crypto::Certificate& cert) {
            auto shared = w.lock();
            if (!shared)
                return false;
            auto crt = tls::CertificateStore::instance().getCertificate(ph.toString());
            if (!crt)
                return false;
            return crt->getPacked() == cert.getPacked();
        });

    info->tls_->setOnReady(
        [w = weak(), deviceId = std::move(req.from), vid = std::move(req.id)](bool ok) {
            auto shared = w.lock();
            if (!shared)
                return;
            auto info = shared->getInfo(deviceId, vid);
            if (!info)
                return;
            if (!ok) {
                JAMI_ERR() << "TLS connection failure for peer " << deviceId;
                if (shared->connReadyCb_)
                    shared->connReadyCb_(deviceId, "", nullptr);
            } else {
                // The socket is ready, store it
                JAMI_DBG("Connection to %s is ready", deviceId.to_c_str());
                shared->addNewMultiplexedSocket(deviceId, vid);
            }
        });
}

void
ConnectionManager::Impl::onDhtPeerRequest(const PeerConnectionRequest& req,
                                          const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto deviceId = req.from.toString();
    JAMI_INFO() << account << "New connection requested by " << deviceId.c_str();
    if (!iceReqCb_ || !iceReqCb_(req.from)) {
        JAMI_INFO("[Account:%s] refuse connection from %s",
                  account.getAccountID().c_str(),
                  deviceId.c_str());
        return;
    }

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
    ice_config.onInitDone = [w = weak(), req](bool ok) {
        auto shared = w.lock();
        if (!shared)
            return;
        if (!ok) {
            JAMI_ERR("Cannot initialize ICE session.");
            if (shared->connReadyCb_)
                shared->connReadyCb_(req.from, "", nullptr);
            return;
        }

        dht::ThreadPool::io().run([w = std::move(w), req = std::move(req)] {
            auto shared = w.lock();
            if (!shared)
                return;
            shared->onRequestStartIce(req);
        });
    };

    ice_config.onNegoDone = [w = weak(), req](bool ok) {
        auto shared = w.lock();
        if (!shared)
            return;
        if (!ok) {
            JAMI_ERR("ICE negotiation failed");
            if (shared->connReadyCb_)
                shared->connReadyCb_(req.from, "", nullptr);
            return;
        }

        dht::ThreadPool::io().run([w = std::move(w), req = std::move(req)] {
            auto shared = w.lock();
            if (!shared)
                return;
            shared->onRequestOnNegoDone(req);
        });
    };

    // Negotiate a new ICE socket
    auto info = std::make_shared<ConnectionInfo>();
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        infos_[{req.from, req.id}] = info;
    }
    std::unique_lock<std::mutex> lk {info->mutex_};
    info->ice_ = iceTransportFactory.createUTransport(account.getAccountID().c_str(),
                                                      1,
                                                      true,
                                                      ice_config);
    if (not info->ice_) {
        JAMI_ERR("Cannot initialize ICE session.");
        if (connReadyCb_)
            connReadyCb_(req.from, "", nullptr);
        return;
    }
}

void
ConnectionManager::Impl::addNewMultiplexedSocket(const DeviceId& deviceId, const dht::Value::Id& vid)
{
    auto info = getInfo(deviceId, vid);
    if (!info)
        return;
    info->socket_ = std::make_shared<MultiplexedSocket>(deviceId, std::move(info->tls_));
    info->socket_->setOnReady(
        [w = weak()](const DeviceId& deviceId, const std::shared_ptr<ChannelSocket>& socket) {
            if (auto sthis = w.lock())
                if (sthis->connReadyCb_)
                    sthis->connReadyCb_(deviceId, socket->name(), socket);
        });
    info->socket_->setOnRequest(
        [w = weak()](const DeviceId& deviceId, const uint16_t&, const std::string& name) {
            if (auto sthis = w.lock())
                if (sthis->channelReqCb_)
                    return sthis->channelReqCb_(deviceId, name);
            return false;
        });
    info->socket_->onShutdown([w = weak(), deviceId, vid]() {
        // Cancel current outgoing connections
        dht::ThreadPool::io().run([w, deviceId = dht::InfoHash(deviceId), vid] {
            auto sthis = w.lock();
            if (!sthis)
                return;
            auto info = sthis->getInfo(deviceId, vid);
            if (!info)
                return;

            if (info->socket_) {
                std::set<CallbackId> ids;
                {
                    std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                    auto it = sthis->socketRelatedId_.find(info->socket_);
                    if (it != sthis->socketRelatedId_.end()) {
                        ids = std::move(it->second);
                        sthis->socketRelatedId_.erase(it);
                    }
                }
                for (const auto& cbId : ids) {
                    if (auto cb = sthis->getPendingCallback(cbId)) {
                        cb(nullptr);
                    }
                }
                info->socket_->shutdown();
            }

            if (info && info->ice_)
                info->ice_->cancelOperations();

            std::lock_guard<std::mutex> lk(sthis->infosMtx_);
            sthis->infos_.erase({deviceId, vid});
        });
    });
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
ConnectionManager::connectDevice(const DeviceId& deviceId,
                                 const std::string& name,
                                 ConnectCallback cb)
{
    pimpl_->connectDevice(deviceId, name, std::move(cb));
}

void
ConnectionManager::closeConnectionsWith(const DeviceId& deviceId)
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
    std::vector<std::shared_ptr<ConnectionInfo>> connInfos;
    {
        std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
        for (auto iter = pimpl_->infos_.begin(); iter != pimpl_->infos_.end();) {
            auto const& [key, value] = *iter;
            if (key.first == deviceId) {
                connInfos.emplace_back(value);
                iter = pimpl_->infos_.erase(iter);
            } else {
                iter++;
            }
        }
    }
    for (auto& info : connInfos) {
        if (info->ice_) {
            info->ice_->cancelOperations();
            info->ice_->stop();
        }
        info->responseCv_.notify_all();
        if (info->ice_) {
            std::unique_lock<std::mutex> lk {info->mutex_};
            info->ice_.reset();
        }
    }
    // This will close the TLS Session
    pimpl_->removeUnusedConnections(deviceId);
}

void
ConnectionManager::onDhtConnected(const DeviceId& deviceId)
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
