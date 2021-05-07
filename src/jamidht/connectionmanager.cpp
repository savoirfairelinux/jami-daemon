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
static constexpr int MAX_TENTATIVES {100};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;
using CallbackId = std::pair<jami::DeviceId, dht::Value::Id>;

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
    std::set<CallbackId> cbIds_ {};
};

class ConnectionManager::Impl : public std::enable_shared_from_this<ConnectionManager::Impl>
{
public:
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
                if (info->ice_) {
                    info->ice_->cancelOperations();
                    info->ice_->stop();
                }
                info->responseCv_.notify_all();
                if (deviceId && key.first == deviceId) {
                    erased = true;
                    it = infos_.erase(it);
                }
            }
            if (!erased)
                ++it;
        }
        if (!deviceId)
            dht::ThreadPool::io().run([infos = std::move(infos_)]() mutable { infos.clear(); });
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
        removeUnusedConnections();
    }

    struct PendingCb
    {
        std::string name;
        ConnectCallback cb;
        dht::Value::Id vid;
    };

    void connectDeviceStartIce(const DeviceId& deviceId, const dht::Value::Id& vid);
    void connectDeviceOnNegoDone(const DeviceId& deviceId,
                                 const std::string& name,
                                 const dht::Value::Id& vid,
                                 const std::shared_ptr<dht::crypto::Certificate>& cert);
    void connectDevice(const DeviceId& deviceId, const std::string& uri, ConnectCallback cb);
    void connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                       const std::string& name,
                       ConnectCallback cb);
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

    /**
     * Triggered when a new TLS socket is ready to use
     * @param ok        If succeed
     * @param deviceId  Related device
     * @param vid       vid of the connection request
     * @param name      non empty if TLS was created by connectDevice()
     */
    void onTlsNegotiationDone(bool ok,
                              const DeviceId& deviceId,
                              const dht::Value::Id& vid,
                              const std::string& name = "");

    JamiAccount& account;

    std::mutex infosMtx_ {};
    // Note: Someone can ask multiple sockets, so to avoid any race condition,
    // each device can have multiple multiplexed sockets.
    std::map<CallbackId, std::shared_ptr<ConnectionInfo>> infos_ {};

    std::shared_ptr<ConnectionInfo> getInfo(const DeviceId& deviceId,
                                            const dht::Value::Id& id = dht::Value::INVALID_ID)
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        decltype(infos_)::iterator it;
        if (id == dht::Value::INVALID_ID) {
            it = std::find_if(infos_.begin(), infos_.end(), [&](const auto& item) {
                auto& [key, value] = item;
                return key.first == deviceId;
            });
        } else {
            it = infos_.find({deviceId, id});
        }

        if (it != infos_.end())
            return it->second;
        return {};
    }

    ChannelRequestCallback channelReqCb_ {};
    ConnectionReadyCallback connReadyCb_ {};
    onICERequestCallback iceReqCb_ {};

    /**
     * Stores callback from connectDevice
     * @note: each device needs a vector because several connectDevice can
     * be done in parallel and we only want one socket
     */
    std::mutex connectCbsMtx_ {};
    std::map<DeviceId, std::vector<PendingCb>> pendingCbs_ {};

    std::vector<PendingCb> extractPendingCallbacks(const DeviceId& deviceId,
                                                   const dht::Value::Id vid = 0)
    {
        std::vector<PendingCb> ret;
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto pendingIt = pendingCbs_.find(deviceId);
        if (pendingIt == pendingCbs_.end())
            return ret;
        auto& pendings = pendingIt->second;
        if (vid == 0) {
            ret = std::move(pendings);
        } else {
            for (auto it = pendings.begin(); it != pendings.end(); ++it) {
                if (it->vid == vid) {
                    ret.emplace_back(std::move(*it));
                    pendings.erase(it);
                    break;
                }
            }
        }
        if (pendings.empty())
            pendingCbs_.erase(pendingIt);
        return ret;
    }

    std::vector<PendingCb> getPendingCallbacks(const DeviceId& deviceId,
                                               const dht::Value::Id vid = 0)
    {
        std::vector<PendingCb> ret;
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto pendingIt = pendingCbs_.find(deviceId);
        if (pendingIt == pendingCbs_.end())
            return ret;
        auto& pendings = pendingIt->second;
        if (vid == 0) {
            ret = pendings;
        } else {
            std::copy_if(pendings.begin(),
                         pendings.end(),
                         std::back_inserter(ret),
                         [&](auto pending) { return pending.vid == vid; });
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
        // Erase all pending connect
        for (const auto& pending : extractPendingCallbacks(deviceId))
            pending.cb(nullptr, deviceId);
    };

    if (!ice) {
        JAMI_ERR("No ICE detected");
        onError();
        return;
    }

    auto iceAttributes = ice->getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto& addr : ice->getLocalCandidates(1)) {
        icemsg << addr << "\n";
        JAMI_DBG() << "Added local ICE candidate " << addr;
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
                                value,
                                [this, deviceId](bool ok) {
                                    if (!ok)
                                        JAMI_ERR("Tried to send request to %s, but put failed",
                                                 deviceId.to_c_str());
                                });
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

    if (not ice->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates))) {
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
        for (const auto& pending : extractPendingCallbacks(deviceId))
            pending.cb(nullptr, deviceId);
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
            if (auto shared = w.lock())
                shared->onTlsNegotiationDone(ok, deviceId, vid, name);
        });
}

void
ConnectionManager::Impl::connectDevice(const DeviceId& deviceId,
                                       const std::string& name,
                                       ConnectCallback cb)
{
    if (!account.dht()) {
        cb(nullptr, deviceId);
        return;
    }
    if (deviceId.toString() == account.currentDeviceId()) {
        cb(nullptr, deviceId);
        return;
    }
    account.findCertificate(deviceId,
                            [w = weak(), deviceId, name, cb = std::move(cb)](
                                const std::shared_ptr<dht::crypto::Certificate>& cert) {
                                if (!cert) {
                                    JAMI_ERR("Invalid certificate found for device %s",
                                             deviceId.to_c_str());
                                    cb(nullptr, deviceId);
                                    return;
                                }
                                if (auto shared = w.lock()) {
                                    shared->connectDevice(cert, name, std::move(cb));
                                }
                            });
}

void
ConnectionManager::Impl::connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                       const std::string& name,
                                       ConnectCallback cb)
{
    // Avoid dht operation in a DHT callback to avoid deadlocks
    runOnMainThread([w = weak(), name = std::move(name), cert = std::move(cert), cb = std::move(cb)] {
        auto deviceId = cert->getId();
        auto sthis = w.lock();
        if (!sthis || sthis->isDestroying_) {
            cb(nullptr, deviceId);
            return;
        }
        dht::Value::Id vid;
        auto tentatives = 0;
        do {
            vid = ValueIdDist(1, DRING_ID_MAX_VAL)(sthis->account.rand);
            --tentatives;
        } while (sthis->getPendingCallbacks(deviceId, vid).size() != 0
                 && tentatives != MAX_TENTATIVES);
        if (tentatives == MAX_TENTATIVES) {
            JAMI_ERR("Couldn't get a current random channel number");
            cb(nullptr, deviceId);
            return;
        }
        auto isConnectingToDevice = false;
        {
            std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
            // Check if already connecting
            auto pendingsIt = sthis->pendingCbs_.find(deviceId);
            isConnectingToDevice = pendingsIt != sthis->pendingCbs_.end();
            // Save current request for sendChannelRequest.
            // Note: do not return here, cause we can be in a state where first
            // socket is negotiated and first channel is pending
            // so return only after we checked the info
            if (isConnectingToDevice)
                pendingsIt->second.emplace_back(PendingCb {name, std::move(cb), vid});
            else
                sthis->pendingCbs_[deviceId] = {{name, std::move(cb), vid}};
        }

        // Check if already negotiated
        CallbackId cbId(deviceId, vid);
        if (auto info = sthis->getInfo(deviceId)) {
            std::lock_guard<std::mutex> lk(info->mutex_);
            if (info->socket_) {
                JAMI_DBG("Peer already connected to %s. Add a new channel", deviceId.to_c_str());
                info->cbIds_.emplace(cbId);
                sthis->sendChannelRequest(info->socket_, name, deviceId, vid);
                return;
            }
        }

        if (isConnectingToDevice) {
            JAMI_DBG("Already connecting to %s, wait for the ICE negotiation", deviceId.to_c_str());
            return;
        }

        // Note: used when the ice negotiation fails to erase
        // all stored structures.
        auto eraseInfo = [w, cbId] {
            if (auto shared = w.lock()) {
                std::lock_guard<std::mutex> lk(shared->infosMtx_);
                shared->infos_.erase(cbId);
            }
        };

        // If no socket exists, we need to initiate an ICE connection.
        sthis->account.getIceOptions([w,
                                      cbId,
                                      deviceId = std::move(deviceId),
                                      name = std::move(name),
                                      cert = std::move(cert),
                                      vid,
                                      eraseInfo](auto&& ice_config) {
            auto sthis = w.lock();
            if (!sthis)
                return;
            ice_config.tcpEnable = true;
            ice_config.onInitDone = [w,
                                     cbId,
                                     deviceId = std::move(deviceId),
                                     name = std::move(name),
                                     cert = std::move(cert),
                                     vid,
                                     eraseInfo](bool ok) {
                auto sthis = w.lock();
                if (!sthis)
                    return;
                if (!ok) {
                    JAMI_ERR("Cannot initialize ICE session.");
                    for (const auto& pending : sthis->extractPendingCallbacks(deviceId))
                        pending.cb(nullptr, deviceId);
                    runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
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
                                     vid,
                                     eraseInfo](bool ok) {
                auto sthis = w.lock();
                if (!sthis)
                    return;
                if (!ok) {
                    JAMI_ERR("ICE negotiation failed.");
                    for (const auto& pending : sthis->extractPendingCallbacks(deviceId))
                        pending.cb(nullptr, deviceId);
                    runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
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
            info->ice_ = Manager::instance().getIceTransportFactory().createUTransport(
                sthis->account.getAccountID().c_str(), 1, false, ice_config);

            if (!info->ice_) {
                JAMI_ERR("Cannot initialize ICE session.");
                for (const auto& pending : sthis->extractPendingCallbacks(deviceId))
                    pending.cb(nullptr, deviceId);
                eraseInfo();
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
    msgpack::sbuffer buffer(256);
    msgpack::pack(buffer, val);

    sock->setOnChannelReady(channelSock->channel(), [channelSock, name, deviceId, vid, w = weak()]() {
        if (auto shared = w.lock())
            for (const auto& pending : shared->extractPendingCallbacks(deviceId, vid))
                pending.cb(channelSock, deviceId);
    });
    std::error_code ec;
    int res = sock->write(CONTROL_CHANNEL,
                          reinterpret_cast<const uint8_t*>(buffer.data()),
                          buffer.size(),
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
                JAMI_DBG() << "Received request answer from " << req.from;
            } else {
                JAMI_DBG() << "Received request from " << req.from;
            }
            // Hack:
            // Note: This reschedule on the io pool should not be necessary
            // however https://git.jami.net/savoirfairelinux/ring-daemon/-/issues/421
            // is a bit clueless and not reproductible in a debug env for now. However,
            // the behavior makes me think this callback is blocked (maybe in getInfos())
            // and this must never happen.
            dht::ThreadPool::io().run([w, req = std::move(req)] {
                auto shared = w.lock();
                if (!shared)
                    return;
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
                                    << shared->account
                                    << "Rejected untrusted connection request from " << req.from;
                            }
                        });
                }
            });

            return true;
        },
        dht::Value::UserTypeFilter("peer_request"));
}

void
ConnectionManager::Impl::onTlsNegotiationDone(bool ok,
                                              const DeviceId& deviceId,
                                              const dht::Value::Id& vid,
                                              const std::string& name)
{
    auto info = getInfo(deviceId, vid);
    if (!info)
        return;
    // Note: only handle pendingCallbacks here for TLS initied by connectDevice()
    // Note: if not initied by connectDevice() the channel name will be empty (because no channel
    // asked yet)
    auto isDhtRequest = name.empty();
    if (!ok) {
        if (isDhtRequest) {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId
                       << " - Initied by DHT request. Vid: " << vid;
            if (connReadyCb_)
                connReadyCb_(deviceId, "", nullptr);
        } else {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId
                       << " - Initied by connectDevice(). Initied by channel: " << name
                       << " - vid: " << vid;
            for (const auto& pending : extractPendingCallbacks(deviceId))
                pending.cb(nullptr, deviceId);
        }
    } else {
        // The socket is ready, store it
        if (isDhtRequest) {
            JAMI_DBG() << "Connection to " << deviceId << " is ready"
                       << " - Initied by DHT request. Vid: " << vid;
        } else {
            JAMI_DBG() << "Connection to " << deviceId << " is ready"
                       << " - Initied by connectDevice(). Initied by channel: " << name
                       << " - vid: " << vid;
        }
        addNewMultiplexedSocket(deviceId, vid);
        // Finally, open the channel and launch pending callbacks
        if (info->socket_ && !isDhtRequest) {
            // Note: do not remove pending there it's done in sendChannelRequest
            for (const auto& pending : getPendingCallbacks(deviceId)) {
                JAMI_DBG("Send request on TLS socket for channel %s to %s",
                         pending.name.c_str(),
                         deviceId.to_c_str());
                sendChannelRequest(info->socket_, pending.name, deviceId, pending.vid);
            }
        }
    }
}

void
ConnectionManager::Impl::answerTo(IceTransport& ice, const dht::Value::Id& id, const DeviceId& from)
{
    // NOTE: This is a shortest version of a real SDP message to save some bits
    auto iceAttributes = ice.getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto& addr : ice.getLocalCandidates(1)) {
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
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + from.toString()),
        from,
        value,
        [this, from](bool ok) {
            if (!ok)
                JAMI_ERR("Tried to answer to connection request from %s, but put failed",
                         from.to_c_str());
        });
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

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    answerTo(*ice, req.id, req.from);
    if (not ice->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates))) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN", account.getAccountID().c_str());
        ice = nullptr;
        if (connReadyCb_)
            connReadyCb_(req.from, "", nullptr);
        return;
    }
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
            if (auto shared = w.lock())
                shared->onTlsNegotiationDone(ok, deviceId, vid);
        });
}

void
ConnectionManager::Impl::onDhtPeerRequest(const PeerConnectionRequest& req,
                                          const std::shared_ptr<dht::crypto::Certificate>& /*cert*/)
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
    account.getIceOptions([w = weak(), req](auto&& ice_config) {
        auto deviceId = req.from.toString();
        auto shared = w.lock();
        if (!shared)
            return;
        // Note: used when the ice negotiation fails to erase
        // all stored structures.
        auto eraseInfo = [w, id = req.id, from = req.from] {
            if (auto shared = w.lock()) {
                std::lock_guard<std::mutex> lk(shared->infosMtx_);
                shared->infos_.erase({from, id});
            }
        };

        ice_config.tcpEnable = true;
        ice_config.onInitDone = [w, req, eraseInfo](bool ok) {
            auto shared = w.lock();
            if (!shared)
                return;
            if (!ok) {
                JAMI_ERR("Cannot initialize ICE session.");
                if (shared->connReadyCb_)
                    shared->connReadyCb_(req.from, "", nullptr);
                runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                return;
            }

            dht::ThreadPool::io().run([w = std::move(w), req = std::move(req)] {
                auto shared = w.lock();
                if (!shared)
                    return;
                shared->onRequestStartIce(req);
            });
        };

        ice_config.onNegoDone = [w, req, eraseInfo](bool ok) {
            auto shared = w.lock();
            if (!shared)
                return;
            if (!ok) {
                JAMI_ERR("ICE negotiation failed");
                if (shared->connReadyCb_)
                    shared->connReadyCb_(req.from, "", nullptr);
                runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                return;
            }

            dht::ThreadPool::io().run([w = std::move(w), req = std::move(req)] {
                if (auto shared = w.lock())
                    shared->onRequestOnNegoDone(req);
            });
        };

        // Negotiate a new ICE socket
        auto info = std::make_shared<ConnectionInfo>();
        {
            std::lock_guard<std::mutex> lk(shared->infosMtx_);
            shared->infos_[{req.from, req.id}] = info;
        }
        JAMI_INFO("[Account:%s] accepting connection from %s",
                  shared->account.getAccountID().c_str(),
                  deviceId.c_str());
        std::unique_lock<std::mutex> lk {info->mutex_};
        info->ice_ = Manager::instance().getIceTransportFactory().createUTransport(
            shared->account.getAccountID().c_str(), 1, true, ice_config);
        if (not info->ice_) {
            JAMI_ERR("Cannot initialize ICE session.");
            if (shared->connReadyCb_)
                shared->connReadyCb_(req.from, "", nullptr);
            eraseInfo();
        }
    });
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

            std::set<CallbackId> ids;
            if (auto info = sthis->getInfo(deviceId, vid)) {
                std::lock_guard<std::mutex> lk(info->mutex_);
                if (info->socket_) {
                    ids = std::move(info->cbIds_);
                    info->socket_->shutdown();
                }
                if (info->ice_)
                    info->ice_->cancelOperations();
            }
            for (const auto& cbId : ids)
                for (const auto& pending : sthis->extractPendingCallbacks(cbId.first, cbId.second))
                    pending.cb(nullptr, deviceId);

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
ConnectionManager::connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                 const std::string& name,
                                 ConnectCallback cb)
{
    pimpl_->connectDevice(cert, name, std::move(cb));
}

bool
ConnectionManager::isConnecting(const DeviceId& deviceId, const std::string& name) const
{
    auto pending = pimpl_->getPendingCallbacks(deviceId);
    return std::find_if(pending.begin(), pending.end(), [&](auto p) { return p.name == name; })
           != pending.end();
}

void
ConnectionManager::closeConnectionsWith(const DeviceId& deviceId)
{
    for (const auto& pending : pimpl_->extractPendingCallbacks(deviceId))
        pending.cb(nullptr, deviceId);

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
        if (info->socket_)
            info->socket_->shutdown();
        info->responseCv_.notify_all();
        if (info->ice_) {
            std::unique_lock<std::mutex> lk {info->mutex_};
            dht::ThreadPool::io().run(
                [ice = std::shared_ptr<IceTransport>(std::move(info->ice_))] {});
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

std::size_t
ConnectionManager::activeSockets() const
{
    std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
    return pimpl_->infos_.size();
}

void
ConnectionManager::monitor() const
{
    std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
    JAMI_DBG("ConnectionManager for account %s (%s), current status:",
             pimpl_->account.getAccountID().c_str(),
             pimpl_->account.getUserUri().c_str());
    for (const auto& [_, ci] : pimpl_->infos_) {
        if (ci->socket_)
            ci->socket_->monitor();
    }
    JAMI_DBG("ConnectionManager for account %s (%s), end status.",
             pimpl_->account.getAccountID().c_str(),
             pimpl_->account.getUserUri().c_str());
}

void
ConnectionManager::connectivityChanged()
{
    std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
    for (const auto& [_, ci] : pimpl_->infos_) {
        if (ci->socket_)
            ci->socket_->sendBeacon();
    }
}

} // namespace jami
