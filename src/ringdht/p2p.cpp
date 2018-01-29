/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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
#include "security/tls_session.h"

#include <opendht/default_types.h>
#include <opendht/rng.h>

#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <array>
#include <future>
#include <algorithm>
#include <type_traits>

namespace ring {

static constexpr auto DHT_MSG_TIMEOUT = std::chrono::seconds(20);
static constexpr auto NET_CONNECTION_TIMEOUT = std::chrono::seconds(10);

using Clock = std::chrono::system_clock;
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

//==============================================================================

// This namespace prevents a nasty ODR violation with definitions in peer_connection.cpp
inline namespace
{

template <typename CT>
class Timeout
{
public:
    using clock = CT;
    using duration = typename CT::duration;
    using time_point = typename CT::time_point;

    explicit Timeout(const duration& delay) : delay {delay} {}

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
    static constexpr const dht::ValueType& TYPE = dht::ValueType::USER_DATA;
    static constexpr uint32_t protocol_version = 0x01000002; ///< Supported protocol
    static constexpr const char* key_prefix = "peer:"; ///< base to compute the DHT listen key

    dht::Value::Id id = dht::Value::INVALID_ID;
    uint32_t protocol {protocol_version}; ///< Protocol identification. First bit reserved to indicate a request (0) or a response (1)
    std::vector<std::string> addresses; ///< Request: public addresses for TURN permission. Response: TURN relay addresses (only 1 in current implementation)
    MSGPACK_DEFINE_MAP(id, protocol, addresses)

    PeerConnectionMsg() = default;
    PeerConnectionMsg(dht::Value::Id id, uint32_t aprotocol, const std::string& arelay)
        : id {id}, protocol {aprotocol}, addresses {{arelay}} {}

    bool isRequest() const noexcept { return (protocol & 1) == 0; }

    PeerConnectionMsg respond(const IpAddr& relay) const {
        return {id, protocol|1, relay.toString(true, true)};
    }
};

//==============================================================================

enum class CtrlMsgType
{
    STOP,
    CANCEL,
    TURN_PEER_CONNECT,
    TURN_PEER_DISCONNECT,
    DHT_REQUEST,
    DHT_RESPONSE,
    ADD_DEVICE,
};

struct CtrlMsgBase
{
    CtrlMsgBase() = delete;
    explicit CtrlMsgBase(CtrlMsgType id) : id_ {id} {}
    CtrlMsgType type() const noexcept { return id_; }
private:
    const CtrlMsgType id_;
};

template <class... Args>
using DataTypeSet = std::tuple<Args...>;

template <CtrlMsgType id, typename DataTypeSet=void>
struct CtrlMsg : CtrlMsgBase
{
    template <class... Args>
    explicit CtrlMsg(const Args&... args) : CtrlMsgBase(id), data {args...} {}

    DataTypeSet data;
};

template <CtrlMsgType id, class... Args>
auto makeMsg(const Args&... args)
{
    return std::make_unique<CtrlMsg<id, DataTypeSet<Args...>>>(args...);
}

template <std::size_t N, CtrlMsgType id, typename R, typename T>
auto msgData(const T& msg)
{
    using MsgType = typename std::tuple_element<std::size_t(id), R>::type;
    auto& x = static_cast<const MsgType&>(msg);
    return std::get<N>(x.data);
}

//==============================================================================

using DhtInfoHashMsgData = DataTypeSet<dht::InfoHash>;
using TurnConnectMsgData = DataTypeSet<IpAddr>;
using PeerCnxMsgData = DataTypeSet<PeerConnectionMsg>;
using AddDeviceMsgData = DataTypeSet<dht::InfoHash,
                                     std::shared_ptr<dht::crypto::Certificate>,
                                     std::vector<std::string>,
                                     std::function<void(PeerConnection*)>>;

using AllCtrlMsg = DataTypeSet<CtrlMsg<CtrlMsgType::STOP>,
                               CtrlMsg<CtrlMsgType::CANCEL, DhtInfoHashMsgData>,
                               CtrlMsg<CtrlMsgType::TURN_PEER_CONNECT, TurnConnectMsgData>,
                               CtrlMsg<CtrlMsgType::TURN_PEER_DISCONNECT, TurnConnectMsgData>,
                               CtrlMsg<CtrlMsgType::DHT_REQUEST, PeerCnxMsgData>,
                               CtrlMsg<CtrlMsgType::DHT_RESPONSE, PeerCnxMsgData>,
                               CtrlMsg<CtrlMsgType::ADD_DEVICE, AddDeviceMsgData>>;

template <CtrlMsgType id, std::size_t N=0, typename T>
auto ctrlMsgData(const T& msg)
{
    return msgData<N, id, AllCtrlMsg>(msg);
}

} // namespace <anonymous>

//==============================================================================

class DhtPeerConnector::Impl
{
public:
    class ClientConnector;

    explicit Impl(RingAccount& account)
        : account {account}
        , loopFut_ {std::async(std::launch::async, [this]{ eventLoop(); })} {}

    ~Impl() {
        servers_.clear();
        clients_.clear();
        turn_.reset();
        ctrl << makeMsg<CtrlMsgType::STOP>();
    }

    RingAccount& account;
    Channel<std::unique_ptr<CtrlMsgBase>> ctrl;

private:
    std::unique_ptr<ConnectedTurnTransport> turn_ep_;
    std::unique_ptr<TurnTransport> turn_;

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>> certMap_;
    std::map<IpAddr, dht::InfoHash> connectedPeers_;

protected:
    std::map<IpAddr, std::unique_ptr<PeerConnection>> servers_;
    std::map<dht::InfoHash, std::unique_ptr<ClientConnector>> clients_;

private:
    void onTurnPeerConnection(const IpAddr&);
    void onTurnPeerDisconnection(const IpAddr&);
    void onRequestMsg(PeerConnectionMsg&&);
    void onTrustedRequestMsg(PeerConnectionMsg&&, const std::shared_ptr<dht::crypto::Certificate>&,
                             const dht::InfoHash&);
    void onResponseMsg(PeerConnectionMsg&&);
    void onAddDevice(const dht::InfoHash&,
                     const std::shared_ptr<dht::crypto::Certificate>&,
                     const std::vector<std::string>&,
                     const std::function<void(PeerConnection*)>&);
    void turnConnect();
    void eventLoop();
    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);

    std::future<void> loopFut_; // keep it last member
};

//==============================================================================

/// This class is responsible of connection to a specific peer.
/// The connected peer acting as server (responsible of the TURN session).
/// When the TURN session is created and your IP is permited, we'll connect it
/// using a system socket. Later the TLS session is negotiated on this socket.
class DhtPeerConnector::Impl::ClientConnector
{
public:
    using ListenerFunction = std::function<void(PeerConnection*)>;

    ClientConnector(Impl& parent,
                    const dht::InfoHash& peer_h,
                    const std::shared_ptr<dht::crypto::Certificate>& peer_cert,
                    const std::vector<std::string>& public_addresses,
                    const ListenerFunction& connect_cb)
        : parent_ {parent}
        , peer_ {peer_h}
        , publicAddresses_ {public_addresses}
        , peerCertificate_ {peer_cert} {
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
        parent_.ctrl << makeMsg<CtrlMsgType::CANCEL>(peer_);
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
        request.id = ValueIdDist()(parent_.account.rand); /* Random id for the message unicity */

        // Send connection request through DHT
        RING_DBG() << parent_.account << "[CNX] request connection to " << peer_;
        parent_.account.dht().putEncrypted(
            dht::InfoHash::get(PeerConnectionMsg::key_prefix + peer_.toString()), peer_, request);

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

        // Connect to TURN peer using a raw socket
        RING_DBG() << parent_.account << "[CNX] connecting to TURN relay "
                   << relay_addr.toString(true, true);
        auto peer_ep = std::make_unique<TcpSocketEndpoint>(relay_addr);
        peer_ep->connect(); // IMPROVE_ME: socket timeout?

        // Negotiate a TLS session
        RING_DBG() << parent_.account << "[CNX] start TLS session";
        auto tls_ep = std::make_unique<TlsSocketEndpoint>(*peer_ep,
                                                          parent_.account.identity(),
                                                          parent_.account.dhParams(),
                                                          *peerCertificate_);
        tls_ep->connect();

        // Connected!
        connection_ = std::make_unique<PeerConnection>([this] { cancel(); }, parent_.account,
                                                       peer_.toString(), std::move(tls_ep));
        peer_ep_ = std::move(peer_ep);

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
    std::shared_ptr<dht::crypto::Certificate> peerCertificate_;
    std::unique_ptr<TcpSocketEndpoint> peer_ep_;
    std::unique_ptr<PeerConnection> connection_;

    std::atomic_bool connected_ {false};
    std::mutex listenersMutex_;
    std::vector<ListenerFunction> listeners_;

    std::future<void> processTask_;
};

//==============================================================================

/// Synchronous TCP connect to a TURN server
/// \note TCP peer connection mode is enabled for reliable data transfer.
void
DhtPeerConnector::Impl::turnConnect()
{
    if (turn_)
        return;

    auto turn_param = TurnTransportParams {};
    turn_param.server = IpAddr {"turn.ring.cx"};
    turn_param.realm = "ring";
    turn_param.username = "ring";
    turn_param.password = "ring";
    turn_param.isPeerConnection = true; // Request for TCP peer connections, not UDP
    turn_param.onPeerConnection = [this](uint32_t conn_id, const IpAddr& peer_addr, bool connected) {
        (void)conn_id;
        if (connected)
            ctrl << makeMsg<CtrlMsgType::TURN_PEER_CONNECT>(peer_addr);
        else
            ctrl << makeMsg<CtrlMsgType::TURN_PEER_DISCONNECT>(peer_addr);
    };
    turn_ = std::make_unique<TurnTransport>(turn_param);

    // Wait until TURN server READY state (or timeout)
    Timeout<Clock> timeout {NET_CONNECTION_TIMEOUT};
    timeout.start();
    while (!turn_->isReady()) {
        if (timeout)
            throw std::runtime_error("no response from TURN");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/// Find who is connected by using connection certificate
bool
DhtPeerConnector::Impl::validatePeerCertificate(const dht::crypto::Certificate& cert,
                                                dht::InfoHash& peer_h)
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

/// Negotiate a TLS session over a TURN socket this method does [yoda].
/// At this stage both endpoints has a dedicated TCP connection on each other.
void
DhtPeerConnector::Impl::onTurnPeerConnection(const IpAddr& peer_addr)
{
    RING_DBG() << account << "[CNX] TURN connection attempt from "
               << peer_addr.toString(true, true);

    auto turn_ep = std::make_unique<ConnectedTurnTransport>(*turn_, peer_addr);

    RING_DBG() << account << "[CNX] start TLS session over TURN socket";
    dht::InfoHash peer_h;
    auto tls_ep = std::make_unique<TlsTurnEndpoint>(
        *turn_ep, account.identity(), account.dhParams(),
        [&, this] (const dht::crypto::Certificate& cert) { return validatePeerCertificate(cert, peer_h); });

    // block until TLS is negotiated (must throw in case of error)
    try {
        tls_ep->connect();
    } catch (...) {
        RING_WARN() << "[CNX] TLS connection failure from peer " << peer_addr.toString(true, true);
        return;
    }

    RING_DBG() << account << "[CNX] Accepted TLS-TURN connection from RingID " << peer_h;
    connectedPeers_.emplace(peer_addr, tls_ep->peerCertificate().getId());
    auto connection = std::make_unique<PeerConnection>([] {}, account, peer_addr.toString(),
                                                       std::move(tls_ep));
    connection->attachOutputStream(std::make_shared<FtpServer>(account.getAccountID(), peer_h.toString()));
    servers_.emplace(peer_addr, std::move(connection));

    // note: operating this way let endpoint to be deleted safely in case of exceptions
    turn_ep_ = std::move(turn_ep);
}

void
DhtPeerConnector::Impl::onTurnPeerDisconnection(const IpAddr& peer_addr)
{
    const auto& iter = servers_.find(peer_addr);
    if (iter == std::end(servers_))
        return;
    RING_WARN() << account << "[CNX] disconnection from peer " << peer_addr.toString(true, true);
    servers_.erase(iter);
    connectedPeers_.erase(peer_addr);
}

void
DhtPeerConnector::Impl::onRequestMsg(PeerConnectionMsg&& request)
{
    RING_DBG() << account << "[CNX] rx DHT request from " << request.from;

    // Asynch certificate checking -> trig onTrustedRequestMsg when trusted certificate is found
    account.findCertificate(
        request.from,
        [this, request=std::move(request)] (const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
            dht::InfoHash peer_h;
            if (account.foundPeerDevice(cert, peer_h))
                onTrustedRequestMsg(std::move(request), cert, peer_h);
            else
                RING_WARN() << account << "[CNX] rejected untrusted connection request from "
                            << request.from;
    });
}

void
DhtPeerConnector::Impl::onTrustedRequestMsg(PeerConnectionMsg&& request,
                                            const std::shared_ptr<dht::crypto::Certificate>& cert,
                                            const dht::InfoHash& peer_h)
{
    turnConnect(); // start a TURN client connection on first pass, next ones just add new peer cnx handlers

    // Save peer certificate for later TLS session (MUST BE DONE BEFORE TURN PEER AUTHORIZATION)
    certMap_.emplace(cert->getId(), std::make_pair(cert, peer_h));

    for (auto& ip: request.addresses) {
        try {
            turn_->permitPeer(ip);
            RING_DBG() << account << "[CNX] authorized peer connection from " << ip;
        } catch (const std::exception& e) {
            RING_WARN() << account << "[CNX] ignored peer connection '" << ip << "', " << e.what();
        }
    }

    RING_DBG() << account << "[CNX] connection accepted, DHT reply to " << request.from;
    account.dht().putEncrypted(
        dht::InfoHash::get(PeerConnectionMsg::key_prefix + request.from.toString()),
        request.from, request.respond(turn_->peerRelayAddr()));

    // Now wait for a TURN connection from peer (see onTurnPeerConnection)
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
DhtPeerConnector::Impl::onAddDevice(const dht::InfoHash& dev_h,
                                    const std::shared_ptr<dht::crypto::Certificate>& peer_cert,
                                    const std::vector<std::string>& public_addresses,
                                    const std::function<void(PeerConnection*)>& connect_cb)
{
    const auto& iter = clients_.find(dev_h);
    if (iter == std::end(clients_)) {
        clients_.emplace(
            dev_h,
            std::make_unique<Impl::ClientConnector>(*this, dev_h, peer_cert, public_addresses, connect_cb));
    } else {
        iter->second->addListener(connect_cb);
    }
}

void
DhtPeerConnector::Impl::eventLoop()
{
    // Loop until STOP msg
    while (true) {
        std::unique_ptr<CtrlMsgBase> msg;
        ctrl >> msg;
        switch (msg->type()) {
            case CtrlMsgType::STOP:
                return;

            case CtrlMsgType::TURN_PEER_CONNECT:
                onTurnPeerConnection(ctrlMsgData<CtrlMsgType::TURN_PEER_CONNECT>(*msg));
                break;

            case CtrlMsgType::TURN_PEER_DISCONNECT:
                onTurnPeerDisconnection(ctrlMsgData<CtrlMsgType::TURN_PEER_DISCONNECT>(*msg));
                break;

            case CtrlMsgType::CANCEL:
                clients_.erase(ctrlMsgData<CtrlMsgType::CANCEL>(*msg));
                break;

            case CtrlMsgType::DHT_REQUEST:
                onRequestMsg(ctrlMsgData<CtrlMsgType::DHT_REQUEST>(*msg));
                break;

            case CtrlMsgType::DHT_RESPONSE:
                onResponseMsg(ctrlMsgData<CtrlMsgType::DHT_RESPONSE>(*msg));
                break;

            case CtrlMsgType::ADD_DEVICE:
                onAddDevice(ctrlMsgData<CtrlMsgType::ADD_DEVICE, 0>(*msg),
                            ctrlMsgData<CtrlMsgType::ADD_DEVICE, 1>(*msg),
                            ctrlMsgData<CtrlMsgType::ADD_DEVICE, 2>(*msg),
                            ctrlMsgData<CtrlMsgType::ADD_DEVICE, 3>(*msg));
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
/// The DHT key is Hash(PeerConnectionMsg::key_prefix + device_id), where '+' is the string concatenation.
void
DhtPeerConnector::onDhtConnected(const std::string& device_id)
{
    pimpl_->account.dht().listen<PeerConnectionMsg>(
        dht::InfoHash::get(PeerConnectionMsg::key_prefix + device_id),
        [this](PeerConnectionMsg&& msg) {
            if (msg.from == pimpl_->account.dht().getId())
                return true;
            if (!pimpl_->account.isMessageTreated(msg.id)) {
                if (msg.isRequest()) {
                    // TODO: filter-out request from non trusted peer
                    pimpl_->ctrl << makeMsg<CtrlMsgType::DHT_REQUEST>(std::move(msg));
                } else
                    pimpl_->ctrl << makeMsg<CtrlMsgType::DHT_RESPONSE>(std::move(msg));
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

            account->findCertificate(
                dev_h,
                [this, dev_h, addresses, connect_cb] (const std::shared_ptr<dht::crypto::Certificate>& cert) {
                    pimpl_->ctrl << makeMsg<CtrlMsgType::ADD_DEVICE>(dev_h, cert, addresses, connect_cb);
                });
        },

        [this, peer_h, connect_cb](bool found) {
            if (!found) {
                RING_WARN() << pimpl_->account << "[CNX] aborted, no devices for " << peer_h;
                connect_cb(nullptr);
            }
        });
}

} // namespace ring
