/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "p2p.h"

#include "ringaccount.h"
#include "peer_connection.h"
#include "turn_transport.h"

#include <opendht/default_types.h>
#include <opendht/rng.h>

#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <array>
#include <random>
#include <future>
#include <fstream> // for testing only

namespace ring {

static constexpr auto DHT_MSG_TIMEOUT = std::chrono::seconds(20);
static constexpr auto NET_CONNECTION_TIMEOUT = std::chrono::seconds(10);

using random_device = dht::crypto::random_device;
using Clock = std::chrono::system_clock;
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

//------------------------------------------------------------------------------

template <typename CT>
class Timeout
{
public:
    using clock = CT;
    using duration = typename CT::duration;
    using time_point = typename CT::time_point;

    Timeout(const duration& delay) : delay {delay} {}

    void start() {
        start_ = clock::now();
    }

    explicit operator bool() const {
        return (clock::now() - start_) >= delay;
    }

    const duration delay {duration::zero()};

private:
    time_point start_ {};
};

//------------------------------------------------------------------------------

class DhtPeerConnector::Impl
{
public:
    using RandomGenerator = std::mt19937_64;

    class Connector;
    class ServerConnector;
    class ClientConnector;

    Impl(RingAccount& account) : account {account} {
        random_device rdev;
        std::seed_seq seed {rdev(), rdev()};
        rand.seed(seed);
    };

    void onRequestMsg(PeerConnectionMsg&&);
    void onResponseMsg(PeerConnectionMsg&&);

    //std::ostream& operator <<(std::ostream& os);

    RandomGenerator rand;
    std::map<dht::InfoHash, std::unique_ptr<Connector>> connectors;
    RingAccount& account;
};

//------------------------------------------------------------------------------

/**
 * DHT message to convey a end2end connection request to a peer
 */
class PeerConnectionMsg : public dht::EncryptedValue<PeerConnectionMsg>
{
public:
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr uint32_t PEER_CNX_PROTOCOL = 0x01000002; ///< Current protocol
    static constexpr auto BASEKEY = "peer:"; ///< base to compute the DHT listen key

    using NonceType = std::array<uint8_t, 32>;

    dht::Value::Id id = dht::Value::INVALID_ID;
    uint32_t protocol {PEER_CNX_PROTOCOL}; ///< Protocol identification. First bit reserved to indicate a request (0) or a response (1)
    std::vector<std::string> addresses; ///< Request: public addresses for TURN permission. Response: TURN relay addresses (only 1 in current implementation)
    NonceType nonce; ///< Nonce used inside socket protocol to authenticate the origin
    MSGPACK_DEFINE_MAP(id, protocol, addresses, nonce)

    PeerConnectionMsg() = default;
    PeerConnectionMsg(dht::Value::Id id, uint32_t aprotocol, const std::string& arelay, const NonceType& anonce)
        : id {id}, protocol {aprotocol}, addresses {{arelay}}, nonce {anonce} {}

    bool isRequest() const noexcept { return (protocol & 1) == 0; }

    PeerConnectionMsg respond(const IpAddr& relay) const {
        return {id, protocol|1, relay.toString(true, true), nonce};
    }
};

//------------------------------------------------------------------------------

class DhtPeerConnector::Impl::Connector
{
public:
    Connector(Impl& parent, const dht::InfoHash& peer_h,
              const std::function<void(PeerConnection*)>& connect_cb = [](PeerConnection*){})
        : parent_ {parent}, peer_ {peer_h}, connect_cb_ {connect_cb} {}
    virtual ~Connector() = default;
    virtual void ready() = 0;
    virtual bool isClient() const noexcept = 0;

    void connected(std::unique_ptr<ConnectionEndpoint>&& endpoint) {
        connection_ = std::make_unique<PeerConnection>(
            parent_.account, peer_.toString(), std::move(endpoint));
        RING_DBG() << "connection done on " << peer_;
        connect_cb_(connection_.get());
    }

    void cancel() {
        parent_.account.jobQueue() << [&] {
            parent_.connectors.erase(peer_);
            RING_DBG() << "cancelled peer connection to " << peer_;
            connect_cb_(nullptr);
        };
    }

protected:
    Impl& parent_;
    const dht::InfoHash peer_;
    const std::function<void(PeerConnection*)> connect_cb_;
    std::unique_ptr<PeerConnection> connection_;
};

//------------------------------------------------------------------------------

class DhtPeerConnector::Impl::ClientConnector final : public DhtPeerConnector::Impl::Connector
{
public:
    bool isClient() const noexcept override { return true; }

    ClientConnector(Impl& parent,
                    const dht::InfoHash& peer_h,
                    const std::vector<std::string>& public_addresses,
                    const std::function<void(PeerConnection*)>& connect_cb)
        : Connector {parent, peer_h, connect_cb}, publicAddresses_ {public_addresses} {
            std::uniform_int_distribution<uint8_t> dis;
            std::generate(std::begin(nonce_), std::end(nonce_),
                          [&]{ return dis(parent.rand); });
            process();
        }

    void process() {
        RING_DBG() << "sending peer cnx request to " << peer_;

        PeerConnectionMsg msg;
        msg.addresses = std::move(publicAddresses_);
        msg.nonce = nonce_;
        msg.id = ValueIdDist()(parent_.rand);

        parent_.account.dht().putEncrypted(
            dht::InfoHash::get(PeerConnectionMsg::BASEKEY + peer_.toString()), peer_, msg);

        dhtTimeout_.start();
        asyncWaitDhtResponse();
    }

    void asyncWaitDhtResponse() {
        if (dhtTimeout_) {
            RING_ERR() << "no response from DHT to E2E request";
            cancel();
        } else if (!responseReceived_) {
            parent_.account.jobQueue() << [this] { asyncWaitDhtResponse(); };
        }
    }

    void onResponse(PeerConnectionMsg&& response) {
        responseReceived_ = true;
        if (response.from == peer_ and response.nonce == nonce_ and !response.addresses.empty()) {
            if (auto relay = IpAddr(response.addresses[0])) {
                RING_DBG() << "using relay " << relay.toString(true, true)
                           << " from " << response.from;
                connectRelay(relay);
                return;
            } else
                RING_ERR()<< "invalid E2E relay address";
        } else {
            RING_ERR() << "invalid E2E response from " << response.from;
        }

        cancel();
    }

    void connectRelay(const IpAddr& relay) {
        endpoint_ = std::make_unique<TcpSocketEndpoint>(relay);
        relayConnect_ = std::async(std::launch::async, [this] {
                RING_DBG("wait for socket connect");
                endpoint_->connect(); // TODO: timeout
                RING_DBG() << "socket connected";
                ready();
            });
    }

    void ready() override {
        connected(std::move(endpoint_));
    }

private:
    Timeout<Clock> dhtTimeout_ {DHT_MSG_TIMEOUT};
    std::future<void> responseTimer_;
    PeerConnectionMsg::NonceType nonce_;
    std::vector<std::string> publicAddresses_;
    std::atomic_bool responseReceived_ {false};
    std::unique_ptr<TcpSocketEndpoint> endpoint_;
    std::future<void> relayConnect_;
    std::future<void> relayReady_;
};

//------------------------------------------------------------------------------

class DhtPeerConnector::Impl::ServerConnector final : public DhtPeerConnector::Impl::Connector
{
public:
    bool isClient() const noexcept override { return false; }

    ServerConnector(Impl& parent, PeerConnectionMsg&& request,
                    const std::function<void(PeerConnection*)>& connect_cb)
        : Connector {parent, request.from, connect_cb}
        , request_ {std::move(request)} {
            auto param = TurnTransportParams {};
            param.server = IpAddr {"turn.ring.cx"};
            param.realm = "ring";
            param.username = "ring";
            param.password = "ring";
            param.isPeerConnection = true; // the most important one
            param.onPeerConnection = [this](uint32_t conn_id, const IpAddr& peer_addr) {
                onPeerConnection(conn_id, peer_addr);
            };

            turn_ = std::make_unique<TurnTransport>(param);

            turnReadyTimeout_.start();
            asyncWaitTurnServer();
        }

    void asyncWaitTurnServer() {
        if (turnReadyTimeout_) {
            RING_ERR() << "TURN ready timeout";
            cancel();
        } else if (turn_->isReady()) {
            RING_DBG("TURN ready!");
            reply();
        } else {
            parent_.account.jobQueue() << [this] { asyncWaitTurnServer(); };
        }
    }

    void onPeerConnection(uint32_t conn_id, const IpAddr& peer_addr) {
        (void)conn_id;
        peerAddr_ = peer_addr;
        ready();
    }

    void reply() {
        for (auto& addr : request_.addresses) {
            RING_DBG() << "authorize peer " << addr;
            turn_->permitPeer(addr);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        RING_DBG() << "E2E send response";
        parent_.account.dht().putEncrypted(
            dht::InfoHash::get(PeerConnectionMsg::BASEKEY + peer_.toString()),
            peer_, request_.respond(turn_->peerRelayAddr()));
    }

    void ready() override {
        connected(std::make_unique<TcpTurnEndpoint>(*turn_, peerAddr_));
    }

private:
    Timeout<Clock> turnReadyTimeout_ {NET_CONNECTION_TIMEOUT};
    std::future<void> futureTurnReady_;
    std::unique_ptr<TurnTransport> turn_;
    PeerConnectionMsg request_;
    IpAddr peerAddr_;
};

//------------------------------------------------------------------------------

class DummyStream final : public Stream
{
public:
    DummyStream() {
        out_.open("/tmp/foo", std::ios::binary);
        if (!out_)
            throw std::runtime_error("output file open failed");
    }

    void write(const std::vector<char>& buffer) override {
        out_.write(&buffer[0], buffer.size());
        rx_ += buffer.size();
        RING_WARN() << rx_ << " bytes received";
    }

    DRing::DataTransferId getId() const override {
        return 0;
    }

    void close() noexcept override {
        RING_WARN() << "Output closed, " << rx_ << " bytes received";
    }

private:
    std::ofstream out_;
    std::size_t rx_ {0};
};


void
DhtPeerConnector::Impl::onRequestMsg(PeerConnectionMsg&& request)
{
    const auto& iter = connectors.find(request.from);
    if (iter != std::end(connectors)) {
        RING_WARN() << "Already existing E2E connection with " << request.from;
        return; // connection already exist
    }

    RING_DBG() << "E2E connection requested from " << request.from;
    connectors.emplace(
        request.from,
        std::make_unique<ServerConnector>(*this, std::move(request),
                                          [](PeerConnection* connection){
                                              if (connection) {
                                                  auto transfer = std::make_shared<DummyStream>();
                                                  connection->attachOutputStream(transfer);
                                              }
                                          }));
}

void
DhtPeerConnector::Impl::onResponseMsg(PeerConnectionMsg&& response)
{
    RING_DBG() << "E2E response from " << response.from;
    const auto& iter = connectors.find(response.from);
    if (iter == std::end(connectors))
        return; // no corresponding request
    if (iter->second->isClient())
        static_cast<ClientConnector&>(*iter->second).onResponse(std::move(response));
}

//------------------------------------------------------------------------------

DhtPeerConnector::DhtPeerConnector(RingAccount& account)
    : pimpl_ {new Impl {account}}
{}

DhtPeerConnector::~DhtPeerConnector() = default;

void
DhtPeerConnector::onDhtConnected(const std::string& device_id)
{
    pimpl_->account.dht().listen<PeerConnectionMsg>(
        dht::InfoHash::get(PeerConnectionMsg::BASEKEY + device_id),
        [this](PeerConnectionMsg&& msg) {
            if (!pimpl_->account.isMessageTreated(msg.id)) {
                // TODO: filter-out non trusted msg
                if (msg.isRequest())
                    pimpl_->onRequestMsg(std::move(msg));
                else
                    pimpl_->onResponseMsg(std::move(msg));
            }
            return true;
        });
}

void
DhtPeerConnector::sendRequest(const std::string& peer_id,
                              std::function<void(PeerConnection*)> connect_cb)
{
    const auto peer_h = dht::InfoHash(peer_id);

    // Notes for reader:
    // 1) dht.getPublicAddress() suffers of a non-usability into forEachDevice() callbacks.
    //    If you call it in forEachDevice callbacks, it'll never not return...
    //    Seems that getPublicAddress() and forEachDevice() need to process into the same thread
    //    (here the one where dht_ loop runs).
    // 2) anyway its good to keep this processing here in case of multiple device
    //    as the result is the same for each device.
    auto addresses = pimpl_->account.publicAddresses();
    addresses.emplace_back(ip_utils::getLocalAddr(AF_INET));
    if (addresses.size() == 0)
        throw std::runtime_error("can't send connection request, no public address");

    pimpl_->account.forEachDevice(
        peer_h,
        [this, addresses, connect_cb]
        (const std::shared_ptr<RingAccount>& account, const dht::InfoHash& dev_h) {
            (void)account;
            const auto& iter = pimpl_->connectors.find(dev_h);
            if (iter == std::end(pimpl_->connectors)) {
                pimpl_->connectors.emplace(
                    dev_h,
                    std::make_unique<Impl::ClientConnector>(*pimpl_, dev_h, addresses, connect_cb));
            }
        },

        [peer_h, connect_cb](bool found) {
            if (!found) {
                RING_WARN() << "PeerConnection aborted, no devices for " << peer_h;
                connect_cb(nullptr);
            }
        });
}

} // namespace ring
