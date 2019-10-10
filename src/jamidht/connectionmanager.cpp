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
#include "manager.h"
#include "ice_transport.h"

#include <mutex>
#include <map>
#include <condition_variable>

static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT{10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT{10};
static constexpr std::chrono::seconds DHT_MSG_TIMEOUT{30};

namespace jami
{

struct ConnectionInfo {
    // TODO clean Response
    std::condition_variable responseCv_{};
    std::atomic_bool responseReceived_ {false};
    PeerConnectionRequest response_;
    std::mutex mutex_;
    std::shared_ptr<IceTransport> ice_ {nullptr};
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

    void connectDevice(const std::string& deviceId, const std::string& uri /* OnConnect */);
    void onPeerRequest(const PeerConnectionRequest& req);
    void onPeerResponse(const PeerConnectionRequest& req);


    JamiAccount& account;

    std::map<std::string, ConnectionInfo> connectionsInfos_;




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
};

void
ConnectionManager::Impl::connectDevice(const std::string& deviceId, const std::string& uri /* OnConnect */)
{
    if (!account.dht()) return;

    auto& connectionInfo = connectionsInfos_[deviceId+","+uri];

    auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
    auto ice_config = account.getIceOptions();
    ice_config.tcpEnable = true;
    auto& ice = connectionInfo.ice_;
    ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, false, ice_config);

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        // TODO ON CONNECT FAILURE
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
    val.uri = uri;
    val.ice_msg = icemsg.str();

    // Send connection request through DHT
    JAMI_DBG() << account << "Request connection to " << deviceId;
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + deviceId),
        dht::InfoHash(deviceId),
        val,
        [](bool ok) {
            JAMI_ERR("@@@ REQUEST SENT: ok = %i", ok);
        }
    );

    // Wait for call to onResponse() operated by DHT
    std::unique_lock<std::mutex> lk{ connectionInfo.mutex_ };
    connectionInfo.responseCv_.wait_for(lk, DHT_MSG_TIMEOUT);
    if (!connectionInfo.responseReceived_) {
        JAMI_ERR("no response from DHT to E2E request. Cancel transfer");
        // TODO ON CONNECT FAILURE
        return;
    }

    auto& response = connectionInfo.response_;
    auto sdp = parse_SDP(response.ice_msg, *ice);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd},
                                sdp.rem_candidates)) {
        JAMI_WARN("[Account:%s] start ICE failed", account.getAccountID().c_str());
        // TODO ON CONNECT FAILURE
        ice = nullptr;
        return;
    }

    ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
    if (!ice->isRunning()) {
        JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
        // TODO ON CONNECT FAILURE
        ice = nullptr;
        return;
    }

    JAMI_WARN("@@@ TODO");

    // TODO build socket
    // TODO TLS
    // TODO onConnect success
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
ConnectionManager::Impl::onPeerRequest(const PeerConnectionRequest& req)
{
    auto device = req.from;
    JAMI_INFO() << account << "New connection requested by " << device.toString().c_str();

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

    auto& connectionInfo = connectionsInfos_[req.from.toString() + "," + req.uri];
    if (connectionInfo.ice_) {
        JAMI_WARN("@@@ TODO already initialized request. Remove previous one?");
        return;
    }
    auto& ice = connectionInfo.ice_;
    ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, true, ice_config);

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        // TODO onError
        ice = nullptr;
        return;
    }

    //TODO account.registerDhtAddress(*ice);

    auto sdp = parse_SDP(req.ice_msg, *ice);
    if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
        JAMI_ERR("[Account:%s] start ICE failed - fallback to TURN",
                    account.getAccountID().c_str());
        // TODO onError
        ice = nullptr;
        return;
    }

    ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
    if (ice->isRunning()) {
        JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
    } else {
        JAMI_ERR("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
        ice = nullptr;
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
    val.uri = req.uri;
    val.ice_msg = icemsg.str();
    val.isResponse = true;

    JAMI_DBG() << account << "[CNX] connection accepted, DHT reply to " << req.from;
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + req.from.toString()),
        req.from, val);

    // TODO create socket
}

ConnectionManager::ConnectionManager(JamiAccount& account)
    : pimpl_ {new Impl {account}}
{}

ConnectionManager::~ConnectionManager()
{}

void
ConnectionManager::connectDevice(const std::string& deviceId, const std::string& uri /* OnConnect */)
{
    pimpl_->connectDevice(deviceId, uri);
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
            if (req.isResponse) {
                pimpl_->onPeerResponse(req);
            } else {
                pimpl_->onPeerRequest(req);
            }
            return true;
        });
}


}
