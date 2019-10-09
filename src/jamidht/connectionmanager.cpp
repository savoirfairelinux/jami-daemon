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
static constexpr uint16_t CONTROL_CHANNEL {0};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;


namespace jami
{

struct ConnectionInfo {
    std::condition_variable responseCv_ {};
    std::atomic_bool responseReceived_ {false};
    PeerConnectionRequest response_;
    std::mutex mutex_;
    std::shared_ptr<IceTransport> ice_ {nullptr};
    std::shared_ptr<IceSocketEndpoint> iceSocket_ {nullptr};
};

class ConnectionManager::Impl {
public:
    explicit Impl(JamiAccount& account) : account {account} {}
    ~Impl() {}

    void connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb);
    /**
     * Send a ChannelRequest on the TLS socket. Triggers cb when ready
     * @param sock      socket used to send the request
     * @param name      channel's name
     * @param cb        on channel ready callback
     */
    void sendChannelRequest(std::unique_ptr<MultiplexedSocket>& sock, const std::string& name, ConnectCallback cb);
    /**
     * Triggered when a PeerConnectionRequest comes from the DHT
     */
    void onDhtPeerRequest(const PeerConnectionRequest& req, const std::shared_ptr<dht::crypto::Certificate>& cert);
    void addNewMultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint>&& tlsSocket);
    void onPeerResponse(const PeerConnectionRequest& req);

    JamiAccount& account;

    std::map<std::string /* device id */, ConnectionInfo> connectionsInfos_;
    std::mutex msocketsMutex_;
    std::map<std::string /* device id */, std::unique_ptr<MultiplexedSocket>> multiplexedSockets_;

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>> certMap_;
    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);

    PeerRequestCallBack peerReqCb_;
    ConnectionReadyCallBack connReadyCb_;
};

void
ConnectionManager::Impl::connectDevice(const std::string& deviceId, const std::string& name, ConnectCallback cb)
{
    if (!account.dht()) {
        cb(nullptr);
        return;
    }
    account.findCertificate(dht::InfoHash(deviceId),
    [this, deviceId, name, cb=std::move(cb)] (const std::shared_ptr<dht::crypto::Certificate>& cert) {
        if (!cert) {
            JAMI_ERR("Invalid certificate found for device %s", deviceId.c_str());
            cb(nullptr);
            return;
        }

        // Avoid dht operation in a DHT callback to avoid deadlocks
        // TODO use runOnMainThread instead but first, this needs to make the
        // TLSSession and ICETransport async.
        dht::ThreadPool::io().run([this, deviceId, name, cert, cb=std::move(cb)] {
            auto& connectionInfo = connectionsInfos_[deviceId];
            {
                // Test if a socket already exists for this device
                std::lock_guard<std::mutex> lk(msocketsMutex_);
                auto it = multiplexedSockets_.find(deviceId);
                if (it != multiplexedSockets_.end()) {
                    JAMI_DBG("Peer already connected. Add a new channel");
                    sendChannelRequest(it->second, name, cb);
                    return;
                }
            }

            // If no socket exists, we need to initiate an ICE connection.
            auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
            auto ice_config = account.getIceOptions();
            ice_config.tcpEnable = true;
            auto& ice = connectionInfo.ice_;
            ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, false, ice_config);

            if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
                JAMI_ERR("Cannot initialize ICE session.");
                ice.reset();
                cb(nullptr);
                return;
            }

            account.registerDhtAddress(*ice);

            auto iceAttributes = ice->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto &addr : ice->getLocalCandidates(0)) {
                icemsg << addr << "\n";
            }

            // Prepare connection request as a DHT message
            PeerConnectionRequest val;
            auto vid = ValueIdDist()(account.rand);
            val.id = vid; /* Random id for the message unicity */
            val.ice_msg = icemsg.str();

            // Send connection request through DHT
            JAMI_DBG() << account << "Request connection to " << deviceId;
            account.dht()->putEncrypted(
                dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
                dht::InfoHash(deviceId),
                val
            );

            // Wait for call to onResponse() operated by DHT
            std::unique_lock<std::mutex> lk{ connectionInfo.mutex_ };
            connectionInfo.responseCv_.wait_for(lk, DHT_MSG_TIMEOUT);
            if (!connectionInfo.responseReceived_) {
                JAMI_ERR("no response from DHT to E2E request.");
                ice.reset();
                cb(nullptr);
                return;
            }

            auto& response = connectionInfo.response_;
            auto sdp = IceTransport::parse_SDP(response.ice_msg, *ice);
            if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd},
                                        sdp.rem_candidates)) {
                JAMI_WARN("[Account:%s] start ICE failed", account.getAccountID().c_str());
                ice.reset();
                cb(nullptr);
                return;
            }

            ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
            if (!ice->isRunning()) {
                JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
                ice.reset();
                cb(nullptr);
                return;
            }

            // Build socket
            connectionInfo.iceSocket_ = std::make_shared<IceSocketEndpoint>(ice, true);

            // Negotiate a TLS session
            JAMI_DBG() << account << "Start TLS session";
            auto tlsSocket = std::make_unique<TlsSocketEndpoint>(
                *connectionInfo.iceSocket_, account.identity(), account.dhParams(),
                *cert);
            // block until TLS is negotiated (with 3 secs of
            // timeout) (must throw in case of error)
            try {
                tlsSocket->waitForReady(SOCK_TIMEOUT);
            } catch (const std::logic_error &e) {
                // In case of a timeout
                JAMI_ERR() << "TLS connection failure for peer " << deviceId;
                ice.reset();
                cb(nullptr);
                return;
            } catch (...) {
                JAMI_ERR() << "TLS connection failure for peer " << deviceId;
                ice.reset();
                cb(nullptr);
                return;
            }

            // The socket is ready, store it in multiplexedSockets_
            std::lock_guard<std::mutex> lkmSockets(msocketsMutex_);
            addNewMultiplexedSocket(deviceId, std::move(tlsSocket));
            // Finally, open the channel
            sendChannelRequest(multiplexedSockets_.at(deviceId), name, cb);
        });
    });

}

void
ConnectionManager::Impl::sendChannelRequest(std::unique_ptr<MultiplexedSocket>& sock, const std::string& name, ConnectCallback cb)
{
    auto channelSock = sock->addChannel(name);
    ChannelRequest val;
    val.name = channelSock->name();
    val.channel = channelSock->channel();
    std::stringstream ss;
    msgpack::pack(ss, val);
    auto toSend = ss.str();
    sock->setOnChannelReady(channelSock->channel(), [channelSock = channelSock, cb = std::move(cb)]() {
        cb(channelSock);
    });
    std::error_code ec;
    auto res = sock->write(CONTROL_CHANNEL, reinterpret_cast<const uint8_t*>(&toSend[0]), toSend.size(), ec);
    if (res < 0) {
        // TODO check if we should handle errors here
        JAMI_ERR("sendChannelRequest failed - error: %i", ec);
    }
}

void
ConnectionManager::Impl::onPeerResponse(const PeerConnectionRequest& req)
{
    auto device = req.from;
    JAMI_INFO() << account << "New response received from " << device.toString().c_str();
    auto& connectionInfo = connectionsInfos_[device.toString().c_str()];
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
    if (!peerReqCb_(deviceId, "")) {
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

    // TODO what do we do if a PeerConnectionRequest comes and a socket is already present
    // 1. We should check that the connection is writable. If so, ignore the new request
    // 2. If not writable, close the previous socket, renegotiate one + re-ask for old channels.
    // 3. Inform client?

    // Negotiate a new ICE socket
    auto& connectionInfo = connectionsInfos_[deviceId];
    auto& ice = connectionInfo.ice_;
    ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, true, ice_config);

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        ice = nullptr;
        connReadyCb_(deviceId, "", nullptr);
        return;
    }

    account.registerDhtAddress(*ice);

    auto sdp = IceTransport::parse_SDP(req.ice_msg, *ice);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN",
                    account.getAccountID().c_str());
        ice = nullptr;
        connReadyCb_(deviceId, "", nullptr);
        return;
    }

    ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
    if (ice->isRunning()) {
        JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
    } else {
        JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
        ice = nullptr;
        connReadyCb_(deviceId, "", nullptr);
        return;
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

    JAMI_DBG() << account << "[CNX] connection accepted, DHT reply to " << req.from;
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
        req.from, val);

    // Build socket
    connectionInfo.iceSocket_ = std::make_shared<IceSocketEndpoint>(ice, false);

    // init TLS session
    auto tlsSocket = std::make_unique<TlsSocketEndpoint>(
        *connectionInfo.iceSocket_, account.identity(), account.dhParams(),
        [&, this](const dht::crypto::Certificate &cert) {
            auto peer_h = req.from;
            return validatePeerCertificate(cert, peer_h);
        });
    // block until TLS is negotiated (with 3 secs of timeout)
    // (must throw in case of error)
    try {
        tlsSocket->waitForReady(SOCK_TIMEOUT);
    } catch (const std::exception &e) {
        // In case of a timeout
        JAMI_WARN() << "TLS connection timeout " << e.what();
        ice = nullptr;
        connReadyCb_(deviceId, "", nullptr);
        return;
    }

    std::lock_guard<std::mutex> lk(msocketsMutex_);
    addNewMultiplexedSocket(deviceId, std::move(tlsSocket));
    JAMI_DBG("Connection to %s is ready", deviceId.c_str());
}

void
ConnectionManager::Impl::addNewMultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint>&& tlsSocket)
{
    // mSocketsMutex_ MUST be locked
    multiplexedSockets_.emplace(deviceId, std::unique_ptr<MultiplexedSocket>(new MultiplexedSocket(deviceId, std::move(tlsSocket))));
    multiplexedSockets_.at(deviceId)->setOnReady([this](const std::string& deviceId, const std::shared_ptr<ChannelSocket>& socket) {
        if (connReadyCb_)
            connReadyCb_(deviceId, socket->name(), socket);
    });
    multiplexedSockets_.at(deviceId)->setOnRequest([this](const std::string& deviceId, const uint16_t& channel, const std::string& name) {
        if (peerReqCb_)
            return peerReqCb_(deviceId, name);
        return false;
    });
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
    : pimpl_ { new Impl { account } }
{}

ConnectionManager::~ConnectionManager()
{}

void
ConnectionManager::connectDevice(const std::string& deviceId, const std::string& name, ConnectCallback cb)
{
    pimpl_->connectDevice(deviceId, name, std::move(cb));
}

void
ConnectionManager::onDhtConnected(const std::string& deviceId)
{
    if (!pimpl_->account.dht()) return;
    auto key = dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId);
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
                if (accept)
                // Async certificate checking
                // TODO this should not be necessary because we can get cert from first tls connection
                pimpl_->account.findCertificate(
                    req.from,
                    [this, req=std::move(req)] (const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                        dht::InfoHash peer_h;
                        if (AccountManager::foundPeerDevice(cert, peer_h)) {
                            runOnMainThread([wthis_ = weak(), req, cert] {
                                if (auto sthis = wthis_.lock()) {
                                    sthis->pimpl_->onDhtPeerRequest(req, cert);
                                }
                            });
                        } else {
                            JAMI_WARN() << pimpl_->account << "Rejected untrusted connection request from "
                                        << req.from;
                        }
                });
            }
            return true;
        });
}

void
ConnectionManager::onPeerRequest(PeerRequestCallBack cb)
{
    pimpl_->peerReqCb_ = std::move(cb);
}

void
ConnectionManager::onConnectionReady(ConnectionReadyCallBack cb)
{
    pimpl_->connReadyCb_ = std::move(cb);
}

}
