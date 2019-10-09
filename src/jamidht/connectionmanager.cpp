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

static constexpr std::chrono::seconds ICE_INIT_TIMEOUT{10};
static constexpr std::chrono::seconds DHT_MSG_TIMEOUT{30};

namespace jami
{

struct ConnectionInfo {
    std::condition_variable responseCv_{};
    std::atomic_bool responseReceived_ {false};
    std::mutex mutex_;
};

class ConnectionManager::Impl {
public:
    explicit Impl(JamiAccount& account) : account {account} {}
    ~Impl() {}

    void connectDevice(const std::string& deviceId, const std::string& uri /* OnConnect */);
    void onPeerRequest(const PeerConnectionRequest& req);


    JamiAccount& account;

    std::map<std::string, ConnectionInfo> connectionsInfos_;
    std::map<std::string, PeerConnectionRequest> responses_;

};

void
ConnectionManager::Impl::connectDevice(const std::string& deviceId, const std::string& uri /* OnConnect */)
{
    if (!account.dht()) return;

    auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
    auto ice_config = account.getIceOptions();
    ice_config.tcpEnable = true;
    auto ice = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, false, ice_config);

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("Cannot initialize ICE session.");
        // TODO ON CONNECT FAILURE
        return;
    }
    // TODO seems to block account.registerDhtAddress(*ice);

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
    auto& connectionInfo = connectionsInfos_[deviceId+","+uri];
    std::unique_lock<std::mutex> lk{ connectionInfo.mutex_ };
    connectionInfo.responseCv_.wait_for(lk, DHT_MSG_TIMEOUT);
    if (!connectionInfo.responseReceived_) {
        JAMI_ERR("no response from DHT to E2E request. Cancel transfer");
        // TODO ON CONNECT FAILURE
        return;
    }

    //auto& response = responses_[deviceId+","+uri]

    // TODO build socket
    // TODO TLS
    // TODO onConnect success
}

void
ConnectionManager::Impl::onPeerRequest(const PeerConnectionRequest& req)
{
    auto device = req.from;
    JAMI_ERR("REQUEST FROM %s", device);
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
            pimpl_->onPeerRequest(req);
            return true;
        });
}


}
