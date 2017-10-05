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
#include <algorithm>

// includes needed by DummyStream (testing)
#include <stdexcept>
#include <cstdlib> // strtoull, mkstemp
#include <fstream>

namespace ring {

static constexpr auto DHT_MSG_TIMEOUT = std::chrono::seconds(20);
static constexpr auto NET_CONNECTION_TIMEOUT = std::chrono::seconds(10);

static constexpr auto PING = "PING";
static constexpr auto PONG = "PONG";

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

//------------------------------------------------------------------------------

class DhtPeerConnector::Impl::Connector
{
public:
    using ListenerFunction = std::function<void(PeerConnection*)>;

    Connector(Impl& parent, const dht::InfoHash& peer_h, const ListenerFunction& cb)
        : parent_ {parent}, peer_ {peer_h} {
            addListener(cb);
        }
    virtual ~Connector() = default;
    virtual void ready() = 0;
    virtual bool isClient() const noexcept = 0;

    void addListener(const ListenerFunction& cb) {
        if (!connected_) {
            std::lock_guard<std::mutex> lk {listenersMutex_};
            listeners_.push_back(cb);
        } else {
            cb(connection_.get());
        }
    }

    void connected(std::unique_ptr<ConnectionEndpoint>&& endpoint) {
        RING_DBG() << "peer connection on " << peer_;
        connection_ = std::make_unique<PeerConnection>(
            parent_.account, peer_.toString(), std::move(endpoint));
        connected_ = true;
        for (auto& cb: listeners_)
            cb(connection_.get());
    }

    void cancel() {
        parent_.account.jobQueue() << [&] {
            parent_.connectors.erase(peer_);
            RING_DBG() << "cancelled peer connection to " << peer_;
            for (auto& cb: listeners_)
                cb(connection_.get());
        };
    }

protected:
    Impl& parent_;
    const dht::InfoHash peer_;
    std::atomic_bool connected_ {false};
    std::mutex listenersMutex_;
    std::vector<ListenerFunction> listeners_;
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
            processTask_ = std::async(std::launch::async, [this] {
                    try { process(); } catch (const std::exception& e) {
                        RING_ERR() << "client connection failed: " << e.what();
                        cancel();
                    }
                });
        }

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

        RING_DBG() << "request connection to " << peer_;
        parent_.account.dht().putEncrypted(
            dht::InfoHash::get(PeerConnectionMsg::BASEKEY + peer_.toString()), peer_, request);

        // Wait for call to onResponse() operated by DHT
        Timeout<Clock> dhtTimeout {DHT_MSG_TIMEOUT};
        dhtTimeout.start();
        while (!dhtTimeout and !responseReceived_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (dhtTimeout)
            throw std::runtime_error("no response from DHT to E2E request");

        // Check response validity
        IpAddr relay_addr;
        if (response_.from != peer_ or
            response_.id != request.id or
            response_.addresses.empty() or
            !(relay_addr = response_.addresses[0])) {
            throw std::runtime_error("invalid connection reply");
        }

        RING_DBG() << "using relay " << relay_addr.toString(true, true)
                   << " from " << response_.from;
        endpoint_ = std::make_unique<TcpSocketEndpoint>(relay_addr);
        endpoint_->connect(); // IMPROVEME: socket timeout?
        doSynchro();
        ready();
    }

    void onResponse(PeerConnectionMsg&& response) {
        response_ = std::move(response);
        responseReceived_ = true;
    }

    void ready() override {
        connected(std::move(endpoint_));
    }

private:
    void doSynchro() {
        // PING!
        endpoint_->writeline(PING, ::strlen(PING));

        // PONG?
        auto answer = endpoint_->readline();
        if (!std::equal(&answer[0], &answer[answer.size()], &PONG[0]))
            throw std::logic_error("invalid PONG sequence");
    }

    std::vector<std::string> publicAddresses_;
    std::atomic_bool responseReceived_ {false};
    PeerConnectionMsg response_;
    std::unique_ptr<TcpSocketEndpoint> endpoint_;
    std::future<void> processTask_;
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
            processTask_ = std::async(std::launch::async, [this] {
                    try { process(); } catch (const std::exception& e) {
                        RING_ERR() << "client connection failed: " << e.what();
                        cancel();
                    }
                });
        }

    void process() {
            auto param = TurnTransportParams {};
            param.server = IpAddr {"turn.ring.cx"};
            param.realm = "ring";
            param.username = "ring";
            param.password = "ring";
            param.isPeerConnection = true; // the most important one (TCP peer connection)
            param.onPeerConnection = [this](uint32_t conn_id, const IpAddr& peer_addr) {
                onPeerConnection(conn_id, peer_addr);
            };

            turn_ = std::make_unique<TurnTransport>(param);

            Timeout<Clock> timeout {NET_CONNECTION_TIMEOUT};
            timeout.start();
            while (!timeout and !turn_->isReady()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (timeout)
                throw std::runtime_error("no response from TURN");

            for (auto& addr : request_.addresses) {
                RING_DBG() << "authorize connections from peer " << addr;
                turn_->permitPeer(addr);
            }

            RING_DBG("send connection response");
            parent_.account.dht().putEncrypted(
                dht::InfoHash::get(PeerConnectionMsg::BASEKEY + peer_.toString()),
                peer_, request_.respond(turn_->peerRelayAddr()));

            IpAddr peer_addr;
            channel_ >> peerAddr_; // IMPROVEME: timeout?

            RING_DBG("process synchro");
            doSynchro();
            ready();
    }

    void onPeerConnection(uint32_t conn_id, const IpAddr& peer_addr) {
        (void)conn_id;
        channel_ << peer_addr;
    }

    void ready() override {
        connected(std::make_unique<TcpTurnEndpoint>(*turn_, peerAddr_));
    }

private:
    void doSynchro() {
        // PING?
        std::vector<char> answer;
        turn_->readlinefrom(peerAddr_, answer);
        if (!std::equal(&answer[0], &answer[answer.size()], &PING[0]))
            throw std::logic_error("invalid PING sequence");

        // PONG!
        turn_->writelineto(peerAddr_, PONG, ::strlen(PONG));
    }

    std::unique_ptr<TurnTransport> turn_;
    PeerConnectionMsg request_;
    IpAddr peerAddr_;
    Channel<IpAddr> channel_;
    std::future<void> processTask_;
};

//------------------------------------------------------------------------------

// DO NOT MERGE THAT: this class is for testing only
class DummyStream final : public Stream
{
public:
    DummyStream() : Stream() {
        char filename[] = "/tmp/ring_XXXXXX";
        if (::mkstemp(filename) < 0)
            throw std::system_error(errno, std::system_category());
        out_.open(&filename[0], std::ios::binary);
        if (!out_)
            throw std::system_error(errno, std::system_category());
        RING_WARN() << "Receiving file " << filename;
    }

    bool write(const std::vector<char>& buffer) override {
        if (fileSize_ > 0) {
            out_.write(&buffer[0], buffer.size());
            rx_ += buffer.size();
            if (rx_ >= fileSize_)
                return false; // EOF
        } else {
            // First '\n' ended line (header) contains the file size.
            // Then file data follow.
            // We suppose that the header length is <= 100 characters.
            header_.insert(std::begin(header_), std::cbegin(buffer), std::cend(buffer));
            auto h_end = &header_[header_.size()];
            auto rc_pos = std::find(&header_[0], h_end, '\n');
            if (rc_pos != h_end) {
                fileSize_ = ::strtoull(&header_[0], nullptr, 10);
                RING_DBG() << "Detected file size: " << fileSize_;

                // push remaining bytes into file
                auto size = std::distance(rc_pos, h_end);
                out_.write(rc_pos, size);
                rx_ = size;
                RING_DBG() << rx_;
            } else if (header_.size() > 100) {
                throw std::runtime_error("invalid stream protocol");
            }
        }
        return true; // continue
    }

    DRing::DataTransferId getId() const override {
        return 0;
    }

    void close() noexcept override {
        RING_WARN() << "Output closed, " << rx_ << " bytes received";
        out_.close();
    }

private:
    std::ofstream out_;
    std::vector<char> header_;
    std::size_t fileSize_ {0};
    std::size_t rx_ {0};
};

void
DhtPeerConnector::Impl::onRequestMsg(PeerConnectionMsg&& request)
{
    const auto& iter = connectors.find(request.from);
    if (iter != std::end(connectors)) {
        RING_WARN() << "Re-use existing connection with " << request.from;
        return;
    }

    RING_DBG() << "connection requested from " << request.from;
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
    RING_DBG() << "connection response from " << response.from;
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
DhtPeerConnector::requestConnection(const std::string& peer_id,
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
        [this, addresses, connect_cb](const std::shared_ptr<RingAccount>& account,
                                      const dht::InfoHash& dev_h) {
            (void)account;
            const auto& iter = pimpl_->connectors.find(dev_h);
            if (iter == std::end(pimpl_->connectors)) {
                // New device connector
                pimpl_->connectors.emplace(
                    dev_h,
                    std::make_unique<Impl::ClientConnector>(*pimpl_, dev_h, addresses, connect_cb));
            } else {
                // Already existing device connector
                iter->second->addListener(connect_cb); // WARN: defered call of connect_cb if connector is not in ready state
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
