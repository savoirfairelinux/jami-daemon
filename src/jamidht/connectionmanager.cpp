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

static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT{10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT{10};
static constexpr std::chrono::seconds DHT_MSG_TIMEOUT{30};
static constexpr std::chrono::seconds SOCK_TIMEOUT{3};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

// TODO runOnMainThread?
// TODO crash for second connection

namespace jami
{

struct ConnectionInfo {
    // TODO clean Response
    std::condition_variable responseCv_{};
    std::atomic_bool responseReceived_ {false};
    PeerConnectionRequest response_;
    std::mutex mutex_;
    std::shared_ptr<IceTransport> ice_ {nullptr};
    std::shared_ptr<IceSocketEndpoint> iceSocket_ {nullptr};
    std::unique_ptr<TlsSocketEndpoint> tlsSocket_ {nullptr};
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

    std::map<std::string, ConnectionInfo> connectionsInfos_;
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
ConnectionManager::Impl::connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb)
{
    if (!account.dht()) {
        cb(nullptr);
        return;
    }
    account.findCertificate(dht::InfoHash(deviceId),
    [this, deviceId, uri, cb=std::move(cb)] (const std::shared_ptr<dht::crypto::Certificate>& cert) {
        if (!cert) {
            // TODO on connect error
            JAMI_ERR("Invalid certificate found for device %s", deviceId.c_str());
            cb(nullptr);
            return;
        }
        auto& connectionInfo = connectionsInfos_[deviceId+","+uri];
        if (connectionInfo.ice_) {
            JAMI_WARN("Requesting connection for an already connected device");
            return;
        }

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
        // TODO account.registerDhtAddress(*ice);

        auto iceAttributes = ice->getLocalAttributes();
        std::stringstream icemsg;
        icemsg << iceAttributes.ufrag << "\n";
        icemsg << iceAttributes.pwd << "\n";
        for (const auto &addr : ice->getLocalCandidates(0)) {
            icemsg << addr << "\n";
        }

        // Prepare connection request as a DHT message
        PeerConnectionRequest val;
        val.id = ValueIdDist()(account.rand); /* Random id for the message unicity */
        val.uri = uri;
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
        connectionInfo.tlsSocket_ = std::make_unique<TlsSocketEndpoint>(
            *connectionInfo.iceSocket_, account.identity(), account.dhParams(),
            *cert);
        // block until TLS is negotiated (with 3 secs of
        // timeout) (must throw in case of error)
        try {
            connectionInfo.tlsSocket_->waitForReady(SOCK_TIMEOUT);
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

        cb(std::make_unique<PeerSocket>());
    });

}

void
ConnectionManager::Impl::onPeerResponse(const PeerConnectionRequest& req)
{
    auto device = req.from;
    JAMI_INFO() << account << "New response received from " << device.toString().c_str();
    auto& connectionInfo = connectionsInfos_[req.from.toString() + "," + req.uri];
    connectionInfo.responseReceived_ = true;
    connectionInfo.response_ = std::move(req);
    connectionInfo.responseCv_.notify_one();
}

void
ConnectionManager::Impl::onPeerRequest(const PeerConnectionRequest& req, const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    if (!peerReqCb_(req.from.toString(), req.uri)) {
        JAMI_INFO("[Account:%s] refuse connection from %s",
                    account.getAccountID().c_str(), req.from.toString().c_str());
        return;
    }
    auto device = req.from;
    JAMI_INFO() << account << "New connection requested by " << device.toString().c_str();

    certMap_.emplace(cert->getId(), std::make_pair(cert, req.from.toString()));

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

    if (connectionsInfos_[req.from.toString() + "," + req.uri].ice_) {
        // TODO multiplexed object. For now it only remove the previous socket.
        connectionsInfos_.erase(req.from.toString() + "," + req.uri);
    }
    auto& connectionInfo = connectionsInfos_[req.from.toString() + "," + req.uri];
    auto& ice = connectionInfo.ice_;
    ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, true, ice_config);

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        ice = nullptr;
        connReadyCb_(req.from.toString(), req.uri, nullptr);
        return;
    }

    account.registerDhtAddress(*ice);

    auto sdp = parse_SDP(req.ice_msg, *ice);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN",
                    account.getAccountID().c_str());
        ice = nullptr;
        connReadyCb_(req.from.toString(), req.uri, nullptr);
        return;
    }

    ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
    if (ice->isRunning()) {
        JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
    } else {
        JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
        ice = nullptr;
        connReadyCb_(req.from.toString(), req.uri, nullptr);
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
    val.id = ValueIdDist()(account.rand); /* Random id for the message unicity */
    val.uri = req.uri;
    val.ice_msg = icemsg.str();
    val.isResponse = true;

    JAMI_DBG() << account << "[CNX] connection accepted, DHT reply to " << req.from;
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + req.from.toString()),
        req.from, val);

    // Build socket
    connectionInfo.iceSocket_ = std::make_shared<IceSocketEndpoint>(ice, false);

    // init TLS session
    connectionInfo.tlsSocket_ = std::make_unique<TlsSocketEndpoint>(
        *connectionInfo.iceSocket_, account.identity(), account.dhParams(),
        [&, this](const dht::crypto::Certificate &cert) {
            auto peer_h = req.from;
            return validatePeerCertificate(cert, peer_h);
        });
    // block until TLS is negotiated (with 3 secs of timeout)
    // (must throw in case of error)
    try {
        connectionInfo.tlsSocket_->waitForReady(SOCK_TIMEOUT);
    } catch (const std::exception &e) {
        // In case of a timeout
        JAMI_WARN() << "TLS connection timeout " << e.what();
        ice = nullptr;
        connReadyCb_(req.from.toString(), req.uri, nullptr);
    }

    connReadyCb_(req.from.toString(), req.uri, std::make_unique<PeerSocket>());
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
ConnectionManager::connectDevice(const std::string& deviceId, const std::string& uri, ConnectCallback cb)
{
    pimpl_->connectDevice(deviceId, uri, std::move(cb));
}

void
ConnectionManager::onDhtConnected(const std::string& deviceId)
{
    if (!pimpl_->account.dht()) return;
    auto key = dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId);
    JAMI_ERR("@@@ LISTEN %s", key.toString().c_str());
    pimpl_->account.dht()->listen<PeerConnectionRequest>(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
        [this](PeerConnectionRequest&& req) {
            if (pimpl_->account.isMessageTreated(req.id)) {
                // Already treated. Ignore
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
                            // TODO race cond with this
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
