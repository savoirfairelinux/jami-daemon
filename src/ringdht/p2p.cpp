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
#include "ftp_server.h"
#include "channel.h"

#include <opendht/default_types.h>
#include <opendht/rng.h>

#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <array>
#include <random>
#include <future>
#include <algorithm>

namespace ring {

static constexpr auto DHT_MSG_TIMEOUT = std::chrono::seconds(20);
static constexpr auto NET_CONNECTION_TIMEOUT = std::chrono::seconds(10);

static constexpr auto PING = "PING";
static constexpr auto PONG = "PONG";

using random_device = dht::crypto::random_device;
using Clock = std::chrono::system_clock;
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

//==============================================================================

// following namespace prevents an ODR violation with definitions in peer_connection.cpp
namespace {

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

//==============================================================================

/**
 * DHT message to convey a end2end connection request to a peer
 */
class PeerConnectionMsg : public dht::EncryptedValue<PeerConnectionMsg>
{
public:
    static const constexpr dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr uint32_t PEER_CNX_PROTOCOL = 0x01000002; ///< Current protocol
    static constexpr auto BASEKEY = "peer:"; ///< base to compute the DHT listen key

    using SecretType = std::array<uint8_t, 32>;

    dht::Value::Id id = dht::Value::INVALID_ID;
    uint32_t protocol {PEER_CNX_PROTOCOL}; ///< Protocol identification. First bit reserved to indicate a request (0) or a response (1)
    std::vector<std::string> addresses; ///< Request: public addresses for TURN permission. Response: TURN relay addresses (only 1 in current implementation)
    SecretType secret; ///< Shared secret to encrypt payload data
    MSGPACK_DEFINE_MAP(id, protocol, addresses, secret)

    PeerConnectionMsg() = default;
    PeerConnectionMsg(dht::Value::Id id, uint32_t aprotocol, const std::string& arelay, const SecretType& secret)
        : id {id}, protocol {aprotocol}, addresses {{arelay}}, secret {secret} {}

    bool isRequest() const noexcept { return (protocol & 1) == 0; }

    PeerConnectionMsg respond(const IpAddr& relay) const {
        return {id, protocol|1, relay.toString(true, true), secret};
    }
};

//==============================================================================

enum class CtrlMsgType
{
    EMPTY,
    STOP,
    TURN_PEER_CONNECT,
    TURN_PEER_DISCONNECT,
    ADD_DEVICE,
    CANCEL,
    DHT_REQUEST,
    DHT_RESPONSE,
};

struct CtrlMsg
{
    CtrlMsg() { RING_ERR("Other Ctrl"); }
    virtual CtrlMsgType type() const { return CtrlMsgType::EMPTY; }
    virtual ~CtrlMsg() = default;
};

struct StopCtrlMsg final : public CtrlMsg
{
    explicit StopCtrlMsg() : CtrlMsg () {}
    CtrlMsgType type() const override { return CtrlMsgType::STOP; }
};

struct TurnPeerConnectCtrlMsg final : public CtrlMsg
{
    explicit TurnPeerConnectCtrlMsg(const IpAddr& peer_addr)
        : CtrlMsg ()
        , peer_addr {peer_addr} {}
    CtrlMsgType type() const override { return CtrlMsgType::TURN_PEER_CONNECT; }
    const IpAddr peer_addr;
};

struct TurnPeerDisconnectCtrlMsg final : public CtrlMsg
{
    explicit TurnPeerDisconnectCtrlMsg(const IpAddr& peer_addr)
        : CtrlMsg ()
        , peer_addr {peer_addr} {}
    CtrlMsgType type() const override { return CtrlMsgType::TURN_PEER_DISCONNECT; }
    const IpAddr peer_addr;
};

struct AddDeviceCtrlMsg final : public CtrlMsg
{
    explicit AddDeviceCtrlMsg(const dht::InfoHash& dev_h,
                              const std::vector<std::string>& public_addresses,
                              const std::function<void(PeerConnection*)>& connect_cb)
        : CtrlMsg ()
        , dev_h {dev_h}, public_addresses {public_addresses}, connect_cb {connect_cb} {}
    CtrlMsgType type() const override { return CtrlMsgType::ADD_DEVICE; }
    const dht::InfoHash dev_h;
    const std::vector<std::string> public_addresses;
    const std::function<void(PeerConnection*)> connect_cb;
};

struct CancelCtrlMsg final : public CtrlMsg
{
    explicit CancelCtrlMsg(const dht::InfoHash& peer)
        : CtrlMsg ()
        , peer {peer} {}
    CtrlMsgType type() const override { return CtrlMsgType::CANCEL; }
    const dht::InfoHash peer;
};

struct DhtRequestCtrlMsg final : public CtrlMsg
{
    explicit DhtRequestCtrlMsg(PeerConnectionMsg&& request)
        : CtrlMsg ()
        , request {std::move(request)} {}
    CtrlMsgType type() const override { return CtrlMsgType::DHT_REQUEST; }
    PeerConnectionMsg request;
};

struct DhtResponseCtrlMsg final : public CtrlMsg
{
    explicit DhtResponseCtrlMsg(PeerConnectionMsg&& response)
        : CtrlMsg ()
        , response {std::move(response)} {}
    CtrlMsgType type() const override { return CtrlMsgType::DHT_RESPONSE; }
    PeerConnectionMsg response;
};

} // namespace <anonymous>

//==============================================================================

class DhtPeerConnector::Impl
{
public:
    using RandomGenerator = std::mt19937_64;

    class ClientConnector;

    explicit Impl(RingAccount& account)
        : account {account}
        , loopFut_ {std::async(std::launch::async, [this]{ eventLoop(); })} {}

    ~Impl() {
        channel << std::make_unique<StopCtrlMsg>();
    }

    RandomGenerator rand;
    RingAccount& account;
    Channel<std::unique_ptr<CtrlMsg>> channel;

protected:
    std::unique_ptr<TurnTransport> turn_;
    std::map<IpAddr, std::unique_ptr<PeerConnection>> servers_;
    std::map<dht::InfoHash, std::unique_ptr<ClientConnector>> clients_;

private:
    void onTurnPeerConnection(const IpAddr&);
    void onTurnPeerDisconnection(const IpAddr&);
    void onRequestMsg(PeerConnectionMsg&&);
    void onResponseMsg(PeerConnectionMsg&&);
    void onAddDevice(const AddDeviceCtrlMsg&);
    void connectTurn();
    void eventLoop();

    std::future<void> loopFut_;
};

//==============================================================================

class DhtPeerConnector::Impl::ClientConnector
{
public:
    using ListenerFunction = std::function<void(PeerConnection*)>;

    ClientConnector(Impl& parent,
                    const dht::InfoHash& peer_h,
                    const std::vector<std::string>& public_addresses,
                    const ListenerFunction& connect_cb)
        : parent_ {parent}
        , peer_ {peer_h}
        , publicAddresses_ {public_addresses} {
            addListener(connect_cb);
            processTask_ = std::async(
                std::launch::async,
                [this] {
                    try { process(); }
                    catch (const std::exception& e) {
                        RING_ERR() << "[CNX] exception during client processing: " << e.what();
                        cancel();
                    }
                });
        }

    ~ClientConnector() {
        for (auto& cb: listeners_)
            cb(nullptr);
        connection_.reset();
    };

    void addListener(const ListenerFunction& cb) {
        if (!connected_) {
            std::lock_guard<std::mutex> lk {listenersMutex_};
            listeners_.push_back(cb);
        } else {
            cb(connection_.get());
        }
    }

    void cancel() {
        parent_.channel << std::make_unique<CancelCtrlMsg>(peer_);
    }

    void onDhtResponse(PeerConnectionMsg&& response) {
        response_ = std::move(response);
        responseReceived_ = true;
    }

private:
    void process() {
        // Prepare connection request as a DHT message
        PeerConnectionMsg request;
        request.addresses = std::move(publicAddresses_);
        request.id = ValueIdDist()(parent_.rand); /* Random id for the message unicity
                                                   * WARNING: don't use the secret, the msg id can be disclosed
                                                   */

        // Generate a shared secret
        std::uniform_int_distribution<uint8_t> dis;
        std::generate(std::begin(request.secret), std::end(request.secret),
                      [&]{ return dis(parent_.rand); });

        // Send connection request through DHT
        RING_DBG() << parent_.account << "[CNX] request connection to " << peer_;
        parent_.account.dht().putEncrypted(
            dht::InfoHash::get(PeerConnectionMsg::BASEKEY + peer_.toString()), peer_, request);

        // Wait for call to onResponse() operated by DHT
        Timeout<Clock> dhtMsgTimeout {DHT_MSG_TIMEOUT};
        dhtMsgTimeout.start();
        while (!responseReceived_) {
            if (dhtMsgTimeout)
                throw std::runtime_error("no response from DHT to E2E request");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Check response validity
        IpAddr relay_addr;
        if (response_.from != peer_ or
            response_.id != request.id or
            response_.addresses.empty() or
            !(relay_addr = response_.addresses[0])) {
            throw std::runtime_error("invalid connection reply");
        }

        // Connect to TURN peer socket
        RING_DBG() << parent_.account << "[CNX] connecting TURN relay "
                   << relay_addr.toString(true, true);
        endpoint_ = std::make_unique<TcpSocketEndpoint>(relay_addr);
        endpoint_->connect(); // IMPROVEME: socket timeout?

        // PING!
        RING_DBG() << parent_.account << "[CNX] tx PING";
        endpoint_->writeline(PING, ::strlen(PING));

        // PONG?
        RING_DBG() << parent_.account << "[CNX] wait PONG";
        auto answer = endpoint_->readline();
        if (!std::equal(&answer[0], &answer[answer.size()], &PONG[0]))
            throw std::logic_error("invalid PONG sequence");

        // Connected!
        connection_ = std::make_unique<PeerConnection>(parent_.account, peer_.toString(),
                                                       std::move(endpoint_));
        connected_ = true;
        for (auto& cb: listeners_) {
            cb(connection_.get());
        }
    }

    Impl& parent_;
    const dht::InfoHash peer_;

    std::vector<std::string> publicAddresses_;
    std::atomic_bool responseReceived_ {false};
    PeerConnectionMsg response_;
    std::unique_ptr<TcpSocketEndpoint> endpoint_;
    std::unique_ptr<PeerConnection> connection_;

    std::atomic_bool connected_ {false};
    std::mutex listenersMutex_;
    std::vector<ListenerFunction> listeners_;

    std::future<void> processTask_;
};

//==============================================================================

void
DhtPeerConnector::Impl::connectTurn()
{
    if (turn_)
        return;

    // Connect to a TCP TURN server with TCP peer connection mode
    auto param = TurnTransportParams {};
    param.server = IpAddr {"turn.ring.cx"};
    param.realm = "ring";
    param.username = "ring";
    param.password = "ring";
    param.isPeerConnection = true; // Request for TCP peer connections, not UDP
    param.onPeerConnection = [this](uint32_t conn_id, const IpAddr& peer_addr, bool connected) {
        (void)conn_id;
        if (connected)
            channel << std::make_unique<TurnPeerConnectCtrlMsg>(peer_addr);
        else
            channel << std::make_unique<TurnPeerDisconnectCtrlMsg>(peer_addr);
    };
    turn_ = std::make_unique<TurnTransport>(param);

    // Wait until TURN server READY state (or timeout)
    Timeout<Clock> timeout {NET_CONNECTION_TIMEOUT};
    timeout.start();
    while (!turn_->isReady()) {
        if (timeout)
            throw std::runtime_error("no response from TURN");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void
DhtPeerConnector::Impl::onTurnPeerConnection(const IpAddr& peer_addr)
{
    RING_DBG() << account << "[CNX] TURN connection attempt from "
               << peer_addr.toString(true, true);

    // PING?
    std::vector<char> answer;
    turn_->readlinefrom(peer_addr, answer);
    if (!std::equal(&answer[0], &answer[answer.size()], &PING[0]))
        throw std::logic_error("[FTP] invalid PING sequence");

    // PONG!
    turn_->writelineto(peer_addr, PONG, ::strlen(PONG));

    auto endpoint = std::make_unique<TcpTurnEndpoint>(*turn_, peer_addr);
    auto connection = std::make_unique<PeerConnection>(account, peer_addr.toString(),
                                                       std::move(endpoint));
    connection->attachOutputStream(std::make_shared<FtpServer>());
    servers_.emplace(peer_addr, std::move(connection));
}

void
DhtPeerConnector::Impl::onTurnPeerDisconnection(const IpAddr& peer_addr)
{
    const auto& iter = servers_.find(peer_addr);
    if (iter == std::end(servers_))
        return;
    RING_WARN() << account << "[CNX] disconnection from peer " << peer_addr.toString(true, true);
    servers_.erase(iter);
}

void
DhtPeerConnector::Impl::onRequestMsg(PeerConnectionMsg&& request)
{
    RING_DBG() << account << "[CNX] rx DHT request from " << request.from;

    connectTurn(); // THIS CALL MAY BLOCK (ensure to have an operational TURN server)

    for (auto& ip: request.addresses) {
        try {
            turn_->permitPeer(ip);
            RING_DBG() << account << "[CNX] authorize peer connection from " << ip;
        } catch (const std::exception& e) {
            RING_WARN() << account << "[CNX] ignore peer connection '"<< ip << "', " << e.what();
        }
    }

    RING_DBG() << account << "[CNX] tx DHT reply to " << request.from;
    account.dht().putEncrypted(
        dht::InfoHash::get(PeerConnectionMsg::BASEKEY + request.from.toString()),
        request.from, request.respond(turn_->peerRelayAddr()));

    // Now wait for peer connection...
}

void
DhtPeerConnector::Impl::onResponseMsg(PeerConnectionMsg&& response)
{
    RING_DBG() << account << "[CNX] rx DHT reply from " << response.from;
    const auto& iter = clients_.find(response.from);
    if (iter == std::end(clients_))
        return; // no corresponding request
    iter->second->onDhtResponse(std::move(response));
}

void
DhtPeerConnector::Impl::onAddDevice(const AddDeviceCtrlMsg& msg)
{
    const auto& iter = clients_.find(msg.dev_h);
    if (iter == std::end(clients_)) {
        clients_.emplace(
            msg.dev_h,
            std::make_unique<Impl::ClientConnector>(*this, msg.dev_h,
                                                    msg.public_addresses,
                                                    msg.connect_cb));
    } else {
        iter->second->addListener(msg.connect_cb);
    }
}

void
DhtPeerConnector::Impl::eventLoop()
{
    // Setup random generator (/!\ CRYPTOGRAPHIC USAGE)
    random_device rdev;
    std::seed_seq seed {rdev(), rdev(), rdev(), rdev(), rdev(), rdev(), rdev(), rdev()};
    rand.seed(seed);

    // Loop until STOP msg
    while (true) {
        std::unique_ptr<CtrlMsg> msg;
        channel >> msg;
        RING_ERR() << "[GOOD] " << int(msg->type());
        switch (msg->type()) {
            case CtrlMsgType::STOP:
                turn_.reset();
                return;

            case CtrlMsgType::TURN_PEER_CONNECT:
                onTurnPeerConnection(static_cast<TurnPeerConnectCtrlMsg&>(*msg).peer_addr);
                break;

            case CtrlMsgType::TURN_PEER_DISCONNECT:
                onTurnPeerDisconnection(static_cast<TurnPeerDisconnectCtrlMsg&>(*msg).peer_addr);
                break;

            case CtrlMsgType::CANCEL:
                clients_.erase(static_cast<CancelCtrlMsg&>(*msg).peer);
                break;

            case CtrlMsgType::DHT_REQUEST:
                onRequestMsg(std::move(static_cast<DhtRequestCtrlMsg&>(*msg).request));
                break;

            case CtrlMsgType::DHT_RESPONSE:
                onResponseMsg(std::move(static_cast<DhtResponseCtrlMsg&>(*msg).response));
                break;

            case CtrlMsgType::ADD_DEVICE:
                onAddDevice(static_cast<AddDeviceCtrlMsg&>(*msg));
                break;

            default: RING_ERR("BUG: got unhandled control msg!"); break;
        }
    }
}

//==============================================================================

DhtPeerConnector::DhtPeerConnector(RingAccount& account)
    : pimpl_ {new Impl {account}}
{}

DhtPeerConnector::~DhtPeerConnector() = default;

/// Called by a RingAccount when it's DHT is connected
/// Install a DHT LISTEN operation on given device to receive data connection requests and replies
/// The DHT key is Hash(PeerConnectionMsg::BASEKEY + device_id), where '+' is the string concatenation.
void
DhtPeerConnector::onDhtConnected(const std::string& device_id)
{
    pimpl_->account.dht().listen<PeerConnectionMsg>(
        dht::InfoHash::get(PeerConnectionMsg::BASEKEY + device_id),
        [this](PeerConnectionMsg&& msg) {
            if (msg.from == pimpl_->account.dht().getId())
                return true;
            if (!pimpl_->account.isMessageTreated(msg.id)) {
                if (msg.isRequest()) {
                    // TODO: filter-out request from non trusted peer
                    pimpl_->channel << std::make_unique<DhtRequestCtrlMsg>(std::move(msg));
                } else
                    pimpl_->channel << std::make_unique<DhtResponseCtrlMsg>(std::move(msg));
            }
            return true;
        });
}

void
DhtPeerConnector::requestConnection(const std::string& peer_id,
                                    const std::function<void(PeerConnection*)>& connect_cb)
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

    // Add local addresses
    // XXX: is it really needed? use-case? a local TURN server?
    //addresses.emplace_back(ip_utils::getLocalAddr(AF_INET));
    //addresses.emplace_back(ip_utils::getLocalAddr(AF_INET6));

    // TODO: bypass DHT devices lookup if connection already exist

    pimpl_->account.forEachDevice(
        peer_h,
        [this, addresses, connect_cb](const std::shared_ptr<RingAccount>& account,
                                      const dht::InfoHash& dev_h) {
            if (dev_h == account->dht().getId()) {
                RING_ERR() << account << "[CNX] no connection to yourself, bad boy!";
                return;
            }
            pimpl_->channel << std::make_unique<AddDeviceCtrlMsg>(dev_h, addresses, connect_cb);
        },

        [this, peer_h, connect_cb](bool found) {
            if (!found) {
                RING_WARN() << pimpl_->account << "[CNX] aborted, no devices for " << peer_h;
                connect_cb(nullptr);
            }
        });
}

} // namespace ring
