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
#include "logger.h"
#include "jamiaccount.h"
#include "account_const.h"
#include "account_manager.h"
#include "manager.h"
#include "ice_transport.h"
#include "peer_connection.h"

#include <mutex>
#include <map>
#include <condition_variable>
#include <set>
#include <opendht/thread_pool.h>
#include <opendht/value.h>

static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT {10};
static constexpr std::chrono::seconds DHT_MSG_TIMEOUT {30};
static constexpr std::chrono::seconds SOCK_TIMEOUT {10};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;


namespace jami
{

struct ConnectionInfo {
    std::condition_variable responseCv_ {};
    std::atomic_bool responseReceived_ {false};
    PeerConnectionRequest response_;
    std::mutex mutex_;
    std::unique_ptr<IceTransport> ice_ {nullptr};
};

class ConnectionManager::Impl : public std::enable_shared_from_this<ConnectionManager::Impl> {
public:
    explicit Impl(JamiAccount& account) : account {account} {}
    ~Impl() {}
    void shutdown() {
        isDestroying_ = true;
        {
            std::lock_guard<std::mutex> lk(connectCbsMtx_);
            pendingCbs_.clear();
        }
        for (auto& connection: connectionsInfos_) {
            for (auto& info: connection.second) {
                if (info.second.ice_) {
                    info.second.ice_->cancelOperations();
                    info.second.ice_->stop();
                }
                info.second.responseCv_.notify_all();
            }
        }
        // Move this on the thread pool because each
        // IceTransport takes 500ms to delete, and it's sequential
        // So, it can increase quickly the time to unregister an account
        dht::ThreadPool::io().run([w=weak()] {
            if (auto shared = w.lock())
                shared->connectionsInfos_.clear();
        });
        {
            std::lock_guard<std::mutex> lk(nonReadySocketsMutex_);
            for (auto& listSocks : nonReadySockets_) {
                for (auto& tlsSock : listSocks.second) {
                    if (tlsSock.second) tlsSock.second->shutdown();
                }
            }
            nonReadySockets_.clear();
        }
        {
            std::lock_guard<std::mutex> lk(msocketsMutex_);
            for (auto& listSocks : multiplexedSockets_) {
                for (auto& mxSock : listSocks.second) {
                    if (mxSock.second) mxSock.second->shutdown();
                }
            }
            multiplexedSockets_.clear();
        }
    }

    void connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb);
    /**
     * Send a ChannelRequest on the TLS socket. Triggers cb when ready
     * @param sock      socket used to send the request
     * @param name      channel's name
     * @param vid       channel's id
     * @param deviceId  to identify the linked ConnectCallback
     */
    void sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock, const std::string& name, const std::string& deviceId, const dht::Value::Id& vid);
    /**
     * Triggered when a PeerConnectionRequest comes from the DHT
     */
    void onDhtPeerRequest(const PeerConnectionRequest& req, const std::shared_ptr<dht::crypto::Certificate>& cert);
    void addNewMultiplexedSocket(const std::string& deviceId, const dht::Value::Id& vid, std::unique_ptr<TlsSocketEndpoint>&& tlsSocket);
    void onPeerResponse(const PeerConnectionRequest& req);

    bool hasPublicIp(const ICESDP& sdp) {
        for (const auto& cand: sdp.rem_candidates)
            if (cand.type == PJ_ICE_CAND_TYPE_SRFLX)
                return true;
        return false;
    }

    JamiAccount& account;

    // Note: Someone can ask multiple sockets, so to avoid any race condition,
    // each device can have multiple multiplexed sockets.
    std::map<std::string /* device id */,
             std::map<dht::Value::Id /* uid */, ConnectionInfo>
        > connectionsInfos_;
    // Used to store currently non ready TLS Socket
    std::mutex nonReadySocketsMutex_;
    std::map<std::string /* device id */,
             std::map<dht::Value::Id /* uid */, std::unique_ptr<TlsSocketEndpoint>>
        > nonReadySockets_;
    std::mutex msocketsMutex_;
    // Note: Multiplexed sockets is also stored in ChannelSockets, so has to be shared_ptr
    std::map<std::string /* device id */,
             std::map<dht::Value::Id /* uid */, std::shared_ptr<MultiplexedSocket>>
        > multiplexedSockets_;

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>> certMap_;
    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);

    ChannelRequestCallBack channelReqCb_;
    ConnectionReadyCallBack connReadyCb_;
    onICERequestCallback iceReqCb_;

    std::mutex connectCbsMtx_ {};
    std::map<std::pair<std::string, dht::Value::Id>, ConnectCallback> pendingCbs_ {};

    std::shared_ptr<ConnectionManager::Impl> shared() {
        return std::static_pointer_cast<ConnectionManager::Impl>(shared_from_this());
    }
    std::shared_ptr<ConnectionManager::Impl const> shared() const {
        return std::static_pointer_cast<ConnectionManager::Impl const>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager::Impl> weak() {
        return std::static_pointer_cast<ConnectionManager::Impl>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager::Impl const> weak() const {
        return std::static_pointer_cast<ConnectionManager::Impl const>(shared_from_this());
    }

    std::atomic_bool isDestroying_ {false};
};

void
ConnectionManager::Impl::connectDevice(const std::string& deviceId, const std::string& name, ConnectCallback cb)
{
    JAMI_WARN("ConnectionManager::Impl::connectDevice %s %s", deviceId.c_str(), name.c_str());
    if (!account.dht()) {
        cb(nullptr);
        return;
    }
    account.findCertificate(dht::InfoHash(deviceId),
    [w=weak(), deviceId, name, cb=std::move(cb)] (const std::shared_ptr<dht::crypto::Certificate>& cert) {
        if (!cert) {
            JAMI_ERR("Invalid certificate found for device %s", deviceId.c_str());
            cb(nullptr);
            return;
        }

        // Avoid dht operation in a DHT callback to avoid deadlocks
        // TODO use runOnMainThread instead but first, this needs to make the
        // TLSSession and ICETransport async.
        dht::ThreadPool::io().run([w, deviceId, name, cert, cb=std::move(cb)] {
            auto sthis = w.lock();
            if (!sthis || sthis->isDestroying_) return;
            auto vid = ValueIdDist()(sthis->account.rand);
            std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
            {
                std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                if (!sthis->pendingCbs_.empty() && sthis->pendingCbs_.find(cbId) != sthis->pendingCbs_.end()) {
                    JAMI_WARN("Already have a current callback for same channel");
                }
                sthis->pendingCbs_[cbId] = std::move(cb);
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
            auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
            auto ice_config = sthis->account.getIceOptions();
            ice_config.tcpEnable = true;
            auto& connectionInfo = sthis->connectionsInfos_[deviceId][vid];
            std::unique_lock<std::mutex> lk{ connectionInfo.mutex_ };
            connectionInfo.ice_ = iceTransportFactory.createUTransport(sthis->account.getAccountID().c_str(), 1, false, ice_config);
            auto& ice = connectionInfo.ice_;

            if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
                JAMI_ERR("Cannot initialize ICE session.");
                ice.reset();
                std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                if (!sthis->pendingCbs_.empty() && sthis->pendingCbs_.find(cbId) != sthis->pendingCbs_.end()) {
                    if (sthis->pendingCbs_[cbId]) sthis->pendingCbs_[cbId](nullptr);
                    sthis->pendingCbs_.erase(cbId);
                }
                return;
            }

            sthis->account.registerDhtAddress(*ice);

            auto iceAttributes = ice->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto &addr : ice->getLocalCandidates(0)) {
                icemsg << addr << "\n";
            }

            // Prepare connection request as a DHT message
            PeerConnectionRequest val;

            val.id = vid; /* Random id for the message unicity */
            val.ice_msg = icemsg.str();
            auto value = std::make_shared<dht::Value>(std::move(val));
            value->user_type = "peer_request";

            // Send connection request through DHT
            JAMI_DBG() << sthis->account << "Request connection to " << deviceId;
            sthis->account.dht()->putEncrypted(
                dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
                dht::InfoHash(deviceId),
                value
            );

            // Wait for call to onResponse() operated by DHT
            if (sthis->isDestroying_) return; // This avoid to wait new negotiation when destroying
            connectionInfo.responseCv_.wait_for(lk, DHT_MSG_TIMEOUT);
            if (sthis->isDestroying_) return; // The destructor can wake a pending wait here.
            if (!connectionInfo.responseReceived_) {
                JAMI_ERR("no response from DHT to E2E request.");
                ice.reset();
                std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                if (!sthis->pendingCbs_.empty() && sthis->pendingCbs_.find(cbId) != sthis->pendingCbs_.end()) {
                    if (sthis->pendingCbs_[cbId]) sthis->pendingCbs_[cbId](nullptr);
                    sthis->pendingCbs_.erase(cbId);
                }
                return;
            }

            auto& response = connectionInfo.response_;
            if (!ice) return;
            auto sdp = IceTransport::parse_SDP(response.ice_msg, *ice);
            auto hasPubIp = sthis->hasPublicIp(sdp);
            if (!hasPubIp) ice->setInitiatorSession();
            if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd},
                                        sdp.rem_candidates)) {
                JAMI_WARN("[Account:%s] start ICE failed", sthis->account.getAccountID().c_str());
                ice.reset();
                std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                if (!sthis->pendingCbs_.empty() && sthis->pendingCbs_.find(cbId) != sthis->pendingCbs_.end()) {
                    if (sthis->pendingCbs_[cbId]) sthis->pendingCbs_[cbId](nullptr);
                    sthis->pendingCbs_.erase(cbId);
                }
                return;
            }

            ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);

            if (!ice->isRunning()) {
                JAMI_ERR("[Account:%s] ICE negotation failed", sthis->account.getAccountID().c_str());
                ice.reset();
                std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                if (!sthis->pendingCbs_.empty() && sthis->pendingCbs_.find(cbId) != sthis->pendingCbs_.end()) {
                    if (sthis->pendingCbs_[cbId]) sthis->pendingCbs_[cbId](nullptr);
                    sthis->pendingCbs_.erase(cbId);
                }
                return;
            }

            // Build socket
            std::lock_guard<std::mutex> lknrs(sthis->nonReadySocketsMutex_);
            auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(std::move(ice)), true);

            // Negotiate a TLS session
            JAMI_DBG() << sthis->account << "Start TLS session";
            auto tlsSocket = std::make_unique<TlsSocketEndpoint>(
                std::move(endpoint), sthis->account.identity(), sthis->account.dhParams(),
                *cert);

            sthis->nonReadySockets_[deviceId][vid] = std::move(tlsSocket);
            sthis->nonReadySockets_[deviceId][vid]->setOnReady([w, deviceId=std::move(deviceId), vid=std::move(vid), name=std::move(name)] (bool ok) {
                auto sthis = w.lock();
                if (!sthis) return;
                if (sthis->multiplexedSockets_[deviceId].find(vid) != sthis->multiplexedSockets_[deviceId].end())
                    return;
                if (!ok) {
                    JAMI_ERR() << "TLS connection failure for peer " << deviceId;
                    std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
                    std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
                    if (!sthis->pendingCbs_.empty() && sthis->pendingCbs_.find(cbId) != sthis->pendingCbs_.end()) {
                        if (sthis->pendingCbs_[cbId]) sthis->pendingCbs_[cbId](nullptr);
                        sthis->pendingCbs_.erase(cbId);
                    }
                } else {
                    // The socket is ready, store it in multiplexedSockets_
                    std::lock_guard<std::mutex> lkmSockets(sthis->msocketsMutex_);
                    std::lock_guard<std::mutex> lknrs(sthis->nonReadySocketsMutex_);
                    sthis->addNewMultiplexedSocket(deviceId, vid, std::move(sthis->nonReadySockets_[deviceId][vid]));
                    sthis->nonReadySockets_[deviceId].erase(vid);
                    if (sthis->nonReadySockets_[deviceId].size() == 0) {
                        sthis->nonReadySockets_.erase(deviceId);
                    }
                    // Finally, open the channel
                    if (!sthis->multiplexedSockets_.at(deviceId).empty())
                        sthis->sendChannelRequest(sthis->multiplexedSockets_.at(deviceId).rbegin()->second, name, deviceId, vid);
                }
            });
        });
    });

}

void
ConnectionManager::Impl::sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock, const std::string& name, const std::string& deviceId, const dht::Value::Id& vid)
{
    auto channelSock = sock->addChannel(name);
    ChannelRequest val;
    val.name = channelSock->name();
    val.channel = channelSock->channel();
    std::stringstream ss;
    msgpack::pack(ss, val);
    auto toSend = ss.str();
    sock->setOnChannelReady(channelSock->channel(), [channelSock, deviceId, vid, this]() {
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        std::pair<std::string, dht::Value::Id> cbId(deviceId, vid);
        if (!pendingCbs_.empty() && pendingCbs_.find(cbId) != pendingCbs_.end()) {
            pendingCbs_[cbId](channelSock);
            pendingCbs_.erase(cbId);
        }
    });
    std::error_code ec;
    int res = sock->write(CONTROL_CHANNEL, reinterpret_cast<const uint8_t*>(&toSend[0]), toSend.size(), ec);
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
ConnectionManager::Impl::onDhtPeerRequest(const PeerConnectionRequest& req, const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto vid = req.id;
    auto deviceId = req.from.toString();
    JAMI_INFO() << account << "New connection requested by " << deviceId.c_str();
    if (!iceReqCb_ || !iceReqCb_(deviceId)) {
        JAMI_INFO("[Account:%s] refuse connection from %s", account.getAccountID().c_str(), deviceId.c_str());
        return;
    }

    certMap_.emplace(cert->getId(), std::make_pair(cert, deviceId));

    // Because the connection is accepted, create an ICE socket.
    auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
    struct IceReady {
        std::mutex mtx {};
        std::condition_variable cv {};
        bool ready {false};
    };
    auto iceReady = std::make_shared<IceReady>();
    auto ice_config = account.getIceOptions();
    ice_config.tcpEnable = true;
    ice_config.onRecvReady = [iceReady]() {
        auto& ir = *iceReady;
        std::lock_guard<std::mutex> lk{ir.mtx};
        ir.ready = true;
        ir.cv.notify_one();
    };

    // 1. Create a new Multiplexed Socket

    // Negotiate a new ICE socket
    auto& connectionInfo = connectionsInfos_[deviceId][req.id];
    connectionInfo.ice_  = iceTransportFactory.createUTransport(account.getAccountID().c_str(), 1, true, ice_config);
    auto& ice = connectionInfo.ice_;

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        ice = nullptr;
        if (connReadyCb_) connReadyCb_(deviceId, "", nullptr);
        return;
    }

    account.registerDhtAddress(*ice);

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    auto hasPubIp = hasPublicIp(sdp);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN",
                    account.getAccountID().c_str());
        ice = nullptr;
        if (connReadyCb_) connReadyCb_(deviceId, "", nullptr);
        return;
    }

    if (!hasPubIp) {
        ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
        if (ice->isRunning()) {
            JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
        } else {
            JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
            ice = nullptr;
            if (connReadyCb_) connReadyCb_(deviceId, "", nullptr);
            return;
        }
    }

    // NOTE: This is a shortest version of a real SDP message to save some bits
    auto iceAttributes = ice->getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto &addr : ice->getLocalCandidates(0)) {
        icemsg << addr << "\n";
    }

    // Send PeerConnection response
    PeerConnectionRequest val;
    val.id = req.id;
    val.ice_msg = icemsg.str();
    val.isAnswer = true;
    auto value = std::make_shared<dht::Value>(std::move(val));
    value->user_type = "peer_request";

    JAMI_DBG() << account << "[CNX] connection accepted, DHT reply to " << req.from;
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
        req.from, value);

    if (hasPubIp) {
        ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
        if (ice->isRunning()) {
            JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
        } else {
            JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
            ice = nullptr;
            if (connReadyCb_) connReadyCb_(deviceId, "", nullptr);
            return;
        }
    }

    // Build socket
    std::lock_guard<std::mutex> lknrs(nonReadySocketsMutex_);
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(std::move(ice)), false);

    // init TLS session
    auto ph = req.from;
    auto tlsSocket = std::make_unique<TlsSocketEndpoint>(
        std::move(endpoint), account.identity(), account.dhParams(),
        [ph, this](const dht::crypto::Certificate &cert) {
            dht::InfoHash peer_h;
            return validatePeerCertificate(cert, peer_h) && peer_h == ph;
        });

    nonReadySockets_[deviceId][vid] = std::move(tlsSocket);
    nonReadySockets_[deviceId][vid]->setOnReady([this, deviceId, vid=std::move(vid)] (bool ok) {
        if (multiplexedSockets_[deviceId].find(vid) != multiplexedSockets_[deviceId].end())
            return;
        if (!ok) {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId;
            if (connReadyCb_) connReadyCb_(deviceId, "", nullptr);
        } else {
            // The socket is ready, store it in multiplexedSockets_
            std::lock_guard<std::mutex> lk(msocketsMutex_);
            std::lock_guard<std::mutex> lknrs(nonReadySocketsMutex_);
            addNewMultiplexedSocket(deviceId, vid, std::move(nonReadySockets_[deviceId][vid]));
            JAMI_DBG("Connection to %s is ready", deviceId.c_str());
            nonReadySockets_[deviceId].erase(vid);
            if (nonReadySockets_[deviceId].size() == 0) {
                nonReadySockets_.erase(deviceId);
            }
        }
    });
}

void
ConnectionManager::Impl::addNewMultiplexedSocket(const std::string& deviceId, const dht::Value::Id& vid, std::unique_ptr<TlsSocketEndpoint>&& tlsSocket)
{
    // mSocketsMutex_ MUST be locked
    auto mSock = std::make_shared<MultiplexedSocket>(deviceId, std::move(tlsSocket));
    mSock->setOnReady([this](const std::string& deviceId, const std::shared_ptr<ChannelSocket>& socket) {
        if (connReadyCb_)
            connReadyCb_(deviceId, socket->name(), socket);
    });
    mSock->setOnRequest([this](const std::string& deviceId, const uint16_t&, const std::string& name) {
        if (channelReqCb_)
            return channelReqCb_(deviceId, name);
        return false;
    });
    mSock->onShutdown([w=weak(), deviceId, vid]() {
        JAMI_WARN("ON SHUTDOWN MTX SOCK");
        auto sthis = w.lock();
        if (!sthis) return;
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
        // TODO run on Main thread
        dht::ThreadPool::io().run([w, deviceId, vid] {
            auto sthis = w.lock();
            if (!sthis) return;
            // Delete the socket
            std::lock_guard<std::mutex> lk(sthis->msocketsMutex_);
            if (sthis->multiplexedSockets_.find(deviceId) != sthis->multiplexedSockets_.end()) {
                if (sthis->multiplexedSockets_[deviceId].find(vid) != sthis->multiplexedSockets_[deviceId].end()) {
                    sthis->multiplexedSockets_[deviceId][vid]->shutdown();
                }
            }

            if (sthis->connectionsInfos_.find(deviceId) != sthis->connectionsInfos_.end()) {
                auto it = sthis->connectionsInfos_[deviceId].find(vid);
                if (it != sthis->connectionsInfos_[deviceId].end()) {
                    if (it->second.ice_) it->second.ice_->cancelOperations();
                    sthis->connectionsInfos_[deviceId].erase(vid);
                    if (sthis->connectionsInfos_[deviceId].empty()) {
                        sthis->connectionsInfos_.erase(deviceId);
                    }
                }
                // This will close the TLS Session
                std::lock_guard<std::mutex> lk (sthis->nonReadySocketsMutex_);
                if (sthis->nonReadySockets_[deviceId].find(vid) != sthis->nonReadySockets_[deviceId].end()) {
                    sthis->nonReadySockets_[deviceId].erase(vid);
                    if (sthis->nonReadySockets_[deviceId].empty()) {
                        sthis->nonReadySockets_.erase(deviceId);

                    }
                }
            }

            if (sthis->multiplexedSockets_.find(deviceId) != sthis->multiplexedSockets_.end()) {
                sthis->multiplexedSockets_[deviceId].erase(vid);
                if (sthis->multiplexedSockets_[deviceId].empty())
                    sthis->multiplexedSockets_.erase(deviceId);
            }
        });
    });
    if (multiplexedSockets_.find(deviceId) == multiplexedSockets_.end()) {
        std::map<dht::Value::Id /* uid */, std::shared_ptr<MultiplexedSocket>> elem;
        elem[vid] = std::move(mSock);
        multiplexedSockets_.emplace(deviceId, std::move(elem));
    } else
        multiplexedSockets_[deviceId][vid] = std::move(mSock);

}

bool
ConnectionManager::Impl::validatePeerCertificate(const dht::crypto::Certificate& cert, dht::InfoHash& peer_h)
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
    : pimpl_ { std::make_shared<Impl>(account) }
{}

ConnectionManager::~ConnectionManager()
{
    if (pimpl_) pimpl_->shutdown();
}

void
ConnectionManager::connectDevice(const std::string& deviceId, const std::string& name, ConnectCallback cb)
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
                it = pimpl_->pendingCbs_.erase(it);
            } else {
                ++it;
            }
        }
    }
    auto it = pimpl_->connectionsInfos_.find(deviceId);
    if (it != pimpl_->connectionsInfos_.end()) {
        for (auto& info: it->second) {
            if (info.second.ice_) {
                info.second.ice_->cancelOperations();
                info.second.ice_->stop();
            }
            info.second.responseCv_.notify_all();
            if (info.second.ice_) {
                info.second.ice_.reset();
            }
        }
    }
    // This will close the TLS Session
    {
        std::lock_guard<std::mutex> lk (pimpl_->nonReadySocketsMutex_);
        pimpl_->nonReadySockets_.erase(deviceId);
    }
    {
        std::lock_guard<std::mutex> lk(pimpl_->msocketsMutex_);
        pimpl_->multiplexedSockets_.erase(deviceId);
    }
}

void
ConnectionManager::onDhtConnected(const std::string& deviceId)
{
    if (!pimpl_->account.dht()) return;
    pimpl_->account.dht()->listen<PeerConnectionRequest>(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
        [this](PeerConnectionRequest&& req) {
            if (pimpl_->account.isMessageTreated(req.id)) {
                // Message already treated. Just ignore
                return true;
            }
            if (req.isAnswer) {
                pimpl_->onPeerResponse(req);
            } else {
                // Async certificate checking
                pimpl_->account.findCertificate(
                    req.from,
                    [this, req=std::move(req)] (const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                        dht::InfoHash peer_h;
                        if (AccountManager::foundPeerDevice(cert, peer_h)) {
                            runOnMainThread([this, req, cert] {
                                pimpl_->onDhtPeerRequest(req, cert);
                            });
                        } else {
                            JAMI_WARN() << pimpl_->account << "Rejected untrusted connection request from "
                                        << req.from;
                        }
                });
            }
            return true;
        }, dht::Value::UserTypeFilter("peer_request"));
}

void
ConnectionManager::onICERequest(onICERequestCallback&& cb)
{
    pimpl_->iceReqCb_ = std::move(cb);
}

void
ConnectionManager::onChannelRequest(ChannelRequestCallBack&& cb)
{
    pimpl_->channelReqCb_ = std::move(cb);
}

void
ConnectionManager::onConnectionReady(ConnectionReadyCallBack&& cb)
{
    pimpl_->connReadyCb_ = std::move(cb);
}

}
