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

static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT{10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT{10};
static constexpr std::chrono::seconds DHT_MSG_TIMEOUT{30};
static constexpr std::chrono::seconds SOCK_TIMEOUT{3};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;


namespace jami
{

// TODO move socket code
class MultiplexedSocket::Impl
{
public:
    Impl(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint)
    : deviceId(deviceId), endpoint(std::move(endpoint))
    , eventLoopFut_ {std::async(std::launch::async, [this] {
        try {
            eventLoop();
        } catch (const std::exception& e) {
            JAMI_ERR() << "[CNX] peer connection event loop failure: " << e.what();
        }
    })} { }

    ~Impl() {
        stop.store(true);
    }

    void eventLoop();

    std::string deviceId;
    // TODO multiplex
    std::unique_ptr<TlsSocketEndpoint> endpoint;
    std::mutex socketsMutex;
    std::map<uint16_t, std::shared_ptr<ChannelSocket>> sockets;
    // TODO only receive from accepted
    std::map<uint16_t, std::vector<uint8_t>> socketsData;
    std::future<void> eventLoopFut_;
    std::atomic_bool stop {false};
};

void
MultiplexedSocket::Impl::eventLoop()
{
    std::vector<uint8_t> buf;
    std::error_code ec;
    while (!stop) {
        auto data_len = endpoint->waitForData(std::chrono::milliseconds(0), ec);
        if (data_len > 0) {
            JAMI_ERR("READ %i", data_len);
            //buf.resize(3000 /* TODO */);
            //auto res = endpoint->read(buf, ec);
            //
            //if (ec) {
            //    JAMI_ERR("READ ERROR: %i", ec);
            //    return;
            //}
        }
    }
}


MultiplexedSocket::MultiplexedSocket(const std::string& deviceId, std::unique_ptr<TlsSocketEndpoint> endpoint)
: pimpl_ { std::make_unique<Impl>(deviceId, std::move(endpoint)) }
{

}

std::shared_ptr<ChannelSocket>
MultiplexedSocket::addChannel(const std::string name)
{
    // TODO improve this
    std::lock_guard<std::mutex> lk(pimpl_->socketsMutex);
    for (int i = 0; i < 65536; ++i) {
        if (pimpl_->sockets.find(i) == pimpl_->sockets.end()) {
            pimpl_->sockets[i] = std::make_shared<ChannelSocket>(*this, name, i);
            return pimpl_->sockets[i];
        }
    }
    return {};
}

std::string
MultiplexedSocket::deviceId() const
{
    return pimpl_->deviceId;
}

bool
MultiplexedSocket::isInitiator() const
{
    return pimpl_->endpoint->isInitiator();
}

int
MultiplexedSocket::maxPayload() const
{
    return pimpl_->endpoint->maxPayload();
}

std::size_t
MultiplexedSocket::read(const uint16_t& channel, uint8_t* buf, std::size_t len, std::error_code& ec)
{
    // TODO CHANNEL
    return pimpl_->endpoint->read(buf, len, ec);
}

std::size_t
MultiplexedSocket::write(const uint16_t& channel, const uint8_t* buf, std::size_t len, std::error_code& ec)
{
    // TODO CHANNEL
    return pimpl_->endpoint->write(buf, len, ec);
}

int
MultiplexedSocket::waitForData(const uint16_t& channel, std::chrono::milliseconds timeout, std::error_code& ec) const
{
    // TODO CHANNEL
    return pimpl_->endpoint->waitForData(timeout, ec);
}
// TODO move
class ChannelSocket::Impl
{
public:
    Impl(MultiplexedSocket& endpoint, const std::string& name, const uint16_t& channel)
    : endpoint(endpoint), name(name), channel(channel) {}

    MultiplexedSocket& endpoint;
    std::string name;
    uint16_t channel;
};

ChannelSocket::ChannelSocket(MultiplexedSocket& endpoint, const std::string& name, const uint16_t& channel)
: pimpl_ { std::make_unique<Impl>(endpoint, name, channel) }
{ }

std::string
ChannelSocket::deviceId() const
{
    return pimpl_->endpoint.deviceId();
}

std::string
ChannelSocket::name() const
{
    return pimpl_->name;
}

uint16_t
ChannelSocket::channel() const
{
    return pimpl_->channel;
}

bool
ChannelSocket::isReliable() const
{
    return pimpl_->endpoint.isReliable();
}

bool
ChannelSocket::isInitiator() const
{
    return pimpl_->endpoint.isInitiator();
}

int
ChannelSocket::maxPayload() const
{
    return pimpl_->endpoint.maxPayload();
}

std::size_t
ChannelSocket::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    return pimpl_->endpoint.read(pimpl_->channel, buf, len, ec);
}

std::size_t
ChannelSocket::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    return pimpl_->endpoint.write(pimpl_->channel, buf, len, ec);
}

int
ChannelSocket::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    return pimpl_->endpoint.waitForData(pimpl_->channel, timeout, ec);
}

struct ConnectionInfo {
    // TODO clean Response
    // TODO multiplexed
    std::condition_variable responseCv_{};
    std::atomic_bool responseReceived_ {false};
    PeerConnectionRequest response_;
    std::mutex mutex_;
    std::shared_ptr<IceTransport> ice_ {nullptr};
    std::shared_ptr<IceSocketEndpoint> iceSocket_ {nullptr};
};

struct ICESDP {
  std::vector<IceCandidate> rem_candidates;
  std::string rem_ufrag;
  std::string rem_pwd;
};

class ConnectionManager::Impl {
public:
    explicit Impl(JamiAccount& account) : account {account} {}
    ~Impl() {}

    void connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb);
    void onPeerRequest(const PeerConnectionRequest& req, const std::shared_ptr<dht::crypto::Certificate>& cert);
    void onPeerResponse(const PeerConnectionRequest& req);


    JamiAccount& account;

    std::map<std::string /* device id */, ConnectionInfo> connectionsInfos_;
    std::map<std::string /* device id */, MultiplexedSocket> multiplexedSockets_;
    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>> certMap_;

    ICESDP parse_SDP(const std::string& sdp_msg, const IceTransport& ice) const {
        ICESDP res;
        std::istringstream stream(sdp_msg);
        std::string line;
        int nr = 0;
        while (std::getline(stream, line)) {
            if (nr == 0) {
                res.rem_ufrag = line;
            } else if (nr == 1) {
                res.rem_pwd = line;
            } else {
                IceCandidate cand;
                if (ice.getCandidateFromSDP(line, cand)) {
                    JAMI_DBG("[Account:%s] add remote ICE candidate: %s",
                            account.getAccountID().c_str(),
                            line.c_str());
                    res.rem_candidates.emplace_back(cand);
                }
            }
            nr++;
        }
        return res;
    }

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
        // TODO use runOnMainThread, no wait needed
        dht::ThreadPool::io().run([this, deviceId, name, cert, cb=std::move(cb)] {
            auto& connectionInfo = connectionsInfos_[deviceId];
            auto it = multiplexedSockets_.find(deviceId);
            if (it != multiplexedSockets_.end()) {
                JAMI_DBG("Peer already connected. Add a new channel");
                // Inform peer for channel creation
                // TODO improve this
                auto channelSock = it->second.addChannel(name);
                PeerConnectionRequest val;
                val.name = channelSock->name();
                val.channel = channelSock->channel();
                auto buf = static_cast<dht::Value>(val).getPacked();
                std::error_code ec;
                it->second.write(0, reinterpret_cast<const uint8_t*>(&buf[0]), buf.size(), ec);
                cb(channelSock);
                return;
            }

            // TODO ICE + TLS => remove waitFor. Should never block main thread

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
            val.name = "";
            val.channel = 0;
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
            auto sdp = parse_SDP(response.ice_msg, *ice);
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


            multiplexedSockets_.emplace(deviceId, MultiplexedSocket { deviceId, std::move(tlsSocket) });
            auto channelSock = multiplexedSockets_.find(deviceId)->second.addChannel(name);
            // TODO improve this
            PeerConnectionRequest pkt;
            pkt.name = channelSock->name();
            pkt.channel = channelSock->channel();
            auto buf = static_cast<dht::Value>(pkt).getPacked();
            std::error_code ec;
            it->second.write(0, reinterpret_cast<const uint8_t*>(&buf[0]), buf.size(), ec);
            cb(channelSock);

        });
    });

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
ConnectionManager::Impl::onPeerRequest(const PeerConnectionRequest& req, const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto deviceId = req.from.toString();
    if (!peerReqCb_(deviceId, req.name)) {
        JAMI_INFO("[Account:%s] refuse connection from %s",
                    account.getAccountID().c_str(), deviceId.c_str());
        return;
    }
    auto device = req.from;
    JAMI_INFO() << account << "New connection requested by " << device.toString().c_str();
    auto vid = req.id;

    certMap_.emplace(cert->getId(), std::make_pair(cert, deviceId));

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

    // TODO reset socket if already present?
    // TODO reopen channels!
    auto& connectionInfo = connectionsInfos_[deviceId];
    auto& ice = connectionInfo.ice_;
    ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, true, ice_config);

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        ice = nullptr;
        connReadyCb_(deviceId, req.name, nullptr);
        return;
    }

    account.registerDhtAddress(*ice);

    auto sdp = parse_SDP(req.ice_msg, *ice);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN",
                    account.getAccountID().c_str());
        ice = nullptr;
        connReadyCb_(deviceId, req.name, nullptr);
        return;
    }

    ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
    if (ice->isRunning()) {
        JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
    } else {
        JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
        ice = nullptr;
        connReadyCb_(deviceId, req.name, nullptr);
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

    // Prepare connection request as a DHT message
    PeerConnectionRequest val;
    val.id = req.id;
    val.name = req.name;
    val.channel = req.channel;
    val.ice_msg = icemsg.str();
    val.isResponse = true;

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
        connReadyCb_(deviceId, req.name, nullptr);
        return;
    }

    multiplexedSockets_.emplace(deviceId, MultiplexedSocket { deviceId, std::move(tlsSocket) });
    connReadyCb_(deviceId, req.name, multiplexedSockets_.find(deviceId)->second.addChannel(req.name));
    JAMI_ERR("!!!");
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
    : pimpl_ {new Impl {account}}
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
            if (req.isResponse) {
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
                            // TODO race cond with this. weak this
                            runOnMainThread([this,req,cert] {
                                pimpl_->onPeerRequest(req, cert);
                            });
                        } else
                            JAMI_WARN() << pimpl_->account << "Rejected untrusted connection request from "
                                        << req.from;
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
