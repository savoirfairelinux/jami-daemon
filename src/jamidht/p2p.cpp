/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "p2p.h"

#include "account_schema.h"
#include "jamiaccount.h"
#include "channel.h"
#include "ice_transport.h"
#include "ftp_server.h"
#include "manager.h"
#include "peer_connection.h"
#include "security/tls_session.h"
#include "turn_transport.h"
#include "account_manager.h"

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

namespace jami {

static constexpr auto DHT_MSG_TIMEOUT = std::chrono::seconds(30);
static constexpr auto NET_CONNECTION_TIMEOUT = std::chrono::seconds(10);
static constexpr auto SOCK_TIMEOUT = std::chrono::seconds(3);
static constexpr auto ICE_READY_TIMEOUT = std::chrono::seconds(10);
static constexpr int ICE_INIT_TIMEOUT{10};

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
    PeerConnectionMsg(dht::Value::Id id, uint32_t aprotocol, const std::vector<std::string>& asrelay)
        : id {id}, protocol {aprotocol}, addresses {asrelay} {}

    bool isRequest() const noexcept { return (protocol & 1) == 0; }

    PeerConnectionMsg respond(const IpAddr& relay) const {
        return {id, protocol|1, relay.toString(true, true)};
    }

    PeerConnectionMsg respond(const std::vector<std::string>& addresses) const {
        return {id, protocol|1, addresses};
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
    virtual ~CtrlMsgBase() = default;
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

using DhtInfoHashMsgData = DataTypeSet<dht::InfoHash, DRing::DataTransferId>;
using TurnConnectMsgData = DataTypeSet<IpAddr>;
using PeerCnxMsgData = DataTypeSet<PeerConnectionMsg>;
using AddDeviceMsgData = DataTypeSet<dht::InfoHash,
                                     DRing::DataTransferId,
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

struct ICESDP {
  std::vector<IceCandidate> rem_candidates;
  std::string rem_ufrag;
  std::string rem_pwd;
};

class DhtPeerConnector::Impl {
public:
    class ClientConnector;

    explicit Impl(JamiAccount& account)
        : account {account}
        , loopFut_ {std::async(std::launch::async, [this]{ eventLoop(); })} {}

    ~Impl() {
      for (auto &thread : answer_threads_)
        thread.join();
      servers_.clear();
      clients_.clear();
      turnAuthv4_.reset();
      turnAuthv6_.reset();
      ctrl << makeMsg<CtrlMsgType::STOP>();
    }

    JamiAccount& account;
    Channel<std::unique_ptr<CtrlMsgBase>> ctrl;

    ICESDP parse_SDP(const std::string& sdp_msg) {
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
                if (ice_->getCandidateFromSDP(line, cand)) {
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

private:
    std::map<IpAddr, std::unique_ptr<ConnectedTurnTransport>> turnEndpoints_;
    std::map<std::pair<dht::InfoHash, IpAddr>, std::unique_ptr<AbstractSocketEndpoint>> p2pEndpoints_;
    std::unique_ptr<TurnTransport> turnAuthv4_;
    std::unique_ptr<TurnTransport> turnAuthv6_;

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>> certMap_;
    std::map<IpAddr, dht::InfoHash> connectedPeers_;

protected:
    std::map<std::pair<dht::InfoHash, IpAddr>, std::unique_ptr<PeerConnection>> servers_;
    std::map<std::pair<dht::InfoHash, DRing::DataTransferId>, std::unique_ptr<ClientConnector>> clients_;
    std::mutex clientsMutex_;

private:
    void onTurnPeerConnection(const IpAddr&);
    void onTurnPeerDisconnection(const IpAddr&);
    void onRequestMsg(PeerConnectionMsg&&);
    void onTrustedRequestMsg(PeerConnectionMsg&&, const std::shared_ptr<dht::crypto::Certificate>&,
                             const dht::InfoHash&);
    void answerToRequest(PeerConnectionMsg&&, const std::shared_ptr<dht::crypto::Certificate>&,
                             const dht::InfoHash&);
    void onResponseMsg(PeerConnectionMsg&&);
    void onAddDevice(const dht::InfoHash&,
                     const DRing::DataTransferId&,
                     const std::shared_ptr<dht::crypto::Certificate>&,
                     const std::vector<std::string>&,
                     const std::function<void(PeerConnection*)>&);
    bool turnConnect();
    void eventLoop();
    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);

    std::future<void> loopFut_; // keep it last member

    std::shared_ptr<IceTransport> ice_;

    std::vector<std::thread> answer_threads_;
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
                    const DRing::DataTransferId& tid,
                    const dht::InfoHash& peer_h,
                    const std::shared_ptr<dht::crypto::Certificate>& peer_cert,
                    const std::vector<std::string>& public_addresses,
                    const ListenerFunction& connect_cb)
        : parent_ {parent}
        , tid_ {tid}
        , peer_ {peer_h}
        , publicAddresses_ {public_addresses}
        , peerCertificate_ {peer_cert} {
            addListener(connect_cb);
            processTask_ = std::async(
                std::launch::async,
                [this] {
                    try { process(); }
                    catch (const std::exception& e) {
                        JAMI_ERR() << "[CNX] exception during client processing: " << e.what();
                        cancel();
                    }
                });
        }

    ~ClientConnector() {
        for (auto& cb: listeners_)
            cb(nullptr);
        connection_.reset();
    }

    bool hasAlreadyAResponse() {
        return responseReceived_;
    }

    void addListener(const ListenerFunction& cb) {
        if (!connected_) {
            std::lock_guard<std::mutex> lk {listenersMutex_};
            listeners_.push_back(cb);
        } else {
            cb(connection_.get());
        }
    }

    void cancel() {
        parent_.ctrl << makeMsg<CtrlMsgType::CANCEL>(peer_, tid_);
    }

    void onDhtResponse(PeerConnectionMsg&& response) {
        if (responseReceived_) return;
        response_ = std::move(response);
        responseReceived_ = true;
        responseCV_.notify_all();
    }

private:
    void process() {
        // Add ice msg into the addresses
        // TODO remove publicAddresses in the future and only use the iceMsg
        // For now it's here for compability with old version
        auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
        auto ice_config = parent_.account.getIceOptions();
        ice_config.tcpEnable = true;
        ice_config.aggressive = true; // This will directly select the first candidate.
        parent_.ice_ = iceTransportFactory.createTransport(parent_.account.getAccountID().c_str(), 1, false, ice_config);

        if (parent_.ice_->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
            JAMI_ERR("Cannot initialize ICE session.");
            cancel();
            return;
        }

        auto iceAttributes = parent_.ice_->getLocalAttributes();
        std::stringstream icemsg;
        icemsg << iceAttributes.ufrag << "\n";
        icemsg << iceAttributes.pwd << "\n";
        for (const auto &addr : parent_.ice_->getLocalCandidates(0)) {
            icemsg << addr << "\n";
        }

        // Prepare connection request as a DHT message
        PeerConnectionMsg request;
        request.id = ValueIdDist()(parent_.account.rand); /* Random id for the message unicity */
        request.addresses = {icemsg.str()};
        request.addresses.insert(request.addresses.end(), publicAddresses_.begin(), publicAddresses_.end());

        // Send connection request through DHT
        JAMI_DBG() << parent_.account << "[CNX] request connection to " << peer_;
        parent_.account.dht()->putEncrypted(
            dht::InfoHash::get(PeerConnectionMsg::key_prefix + peer_.toString()), peer_, request);

        // Wait for call to onResponse() operated by DHT
        std::mutex mtx;
        std::unique_lock<std::mutex> lk{mtx};
        responseCV_.wait_for(lk, DHT_MSG_TIMEOUT);
        if (!responseReceived_) {
            JAMI_ERR("no response from DHT to E2E request. Cancel transfer");
            cancel();
            return;
        }

        // Check response validity
        std::shared_ptr<AbstractSocketEndpoint> peer_ep;
        if (response_.from != peer_ or
            response_.id != request.id or
            response_.addresses.empty())
            throw std::runtime_error("invalid connection reply");

        IpAddr relay_addr;
        for (const auto& address: response_.addresses) {
            if (!(relay_addr = address)) {
                // Should be ICE SDP
                // P2P File transfer. We received an ice SDP message:
                auto sdp = parent_.parse_SDP(address);

                parent_.ice_->setInitiatorSession();
                if (not parent_.ice_->start({sdp.rem_ufrag, sdp.rem_pwd},
                                            sdp.rem_candidates)) {
                  JAMI_WARN("[Account:%s] start ICE failed - fallback to TURN",
                            parent_.account.getAccountID().c_str());
                  break;
                }

                parent_.ice_->waitForNegotiation(10);
                if (parent_.ice_->isRunning()) {
                    peer_ep = std::make_shared<IceSocketEndpoint>(parent_.ice_, true);
                    JAMI_DBG("[Account:%s] ICE negotiation succeed. Starting file transfer",
                             parent_.account.getAccountID().c_str());
                    break;
                } else {
                  JAMI_ERR("[Account:%s] ICE negotation failed",
                           parent_.account.getAccountID().c_str());
                }
            } else {
                try {
                    // Connect to TURN peer using a raw socket
                    JAMI_DBG() << parent_.account << "[CNX] connecting to TURN relay "
                            << relay_addr.toString(true, true);
                    peer_ep = std::make_shared<TcpSocketEndpoint>(relay_addr);
                    try {
                        peer_ep->connect(SOCK_TIMEOUT);
                    } catch (const std::logic_error& e) {
                        // In case of a timeout
                        JAMI_WARN() << "TcpSocketEndpoint timeout for addr " << relay_addr.toString(true, true) << ": " << e.what();
                        cancel();
                        return;
                    } catch (...) {
                        JAMI_WARN() << "TcpSocketEndpoint failure for addr " << relay_addr.toString(true, true);
                        cancel();
                        return;
                    }
                    break;
                } catch (std::system_error&) {
                    JAMI_DBG() << parent_.account << "[CNX] Failed to connect to TURN relay "
                            << relay_addr.toString(true, true);
                }
            }
        }

        // Negotiate a TLS session
        JAMI_DBG() << parent_.account << "[CNX] start TLS session";
        auto tls_ep = std::make_unique<TlsSocketEndpoint>(
            *peer_ep, parent_.account.identity(), parent_.account.dhParams(),
            *peerCertificate_);
        // block until TLS is negotiated (with 3 secs of
        // timeout) (must throw in case of error)
        try {
          tls_ep->waitForReady(SOCK_TIMEOUT);
        } catch (const std::logic_error &e) {
          // In case of a timeout
          JAMI_WARN() << "TLS connection timeout from peer " << peer_.toString()
                      << ": " << e.what();
          cancel();
          return;
        } catch (...) {
          JAMI_WARN() << "TLS connection failure from peer "
                      << peer_.toString();
          cancel();
          return;
        }

        // Connected!
        connection_ = std::make_unique<PeerConnection>(
            [this] { cancel(); }, peer_.toString(),
            std::move(tls_ep));
        peer_ep_ = std::move(peer_ep);

        connected_ = true;
        for (auto &cb : listeners_) {
          cb(connection_.get());
        }
    }

    Impl& parent_;
    const DRing::DataTransferId tid_;
    const dht::InfoHash peer_;

    std::vector<std::string> publicAddresses_;
    std::atomic_bool responseReceived_ {false};
    std::condition_variable responseCV_{};
    PeerConnectionMsg response_;
    std::shared_ptr<dht::crypto::Certificate> peerCertificate_;
    std::shared_ptr<AbstractSocketEndpoint> peer_ep_;
    std::unique_ptr<PeerConnection> connection_;

    std::atomic_bool connected_ {false};
    std::mutex listenersMutex_;
    std::mutex turnMutex_;
    std::vector<ListenerFunction> listeners_;

    std::future<void> processTask_;
};

//==============================================================================

/// Synchronous TCP connect to a TURN server
/// \note TCP peer connection mode is enabled for reliable data transfer.
/// \return if connected to the turn
bool
DhtPeerConnector::Impl::turnConnect()
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    // Don't retry to reconnect to the TURN server if already connected
    if (turnAuthv4_ && turnAuthv4_->isReady()
        && turnAuthv6_ && turnAuthv6_->isReady())
        return true;

    auto details = account.getAccountDetails();
    auto server = details[Conf::CONFIG_TURN_SERVER];
    auto realm = details[Conf::CONFIG_TURN_SERVER_REALM];
    auto username = details[Conf::CONFIG_TURN_SERVER_UNAME];
    auto password = details[Conf::CONFIG_TURN_SERVER_PWD];

    auto turn_param_v4 = TurnTransportParams {};
    turn_param_v4.server = IpAddr {server.empty() ? "turn.jami.net" : server};
    turn_param_v4.realm = realm.empty() ? "ring" : realm;
    turn_param_v4.username = username.empty() ? "ring" : username;
    turn_param_v4.password = password.empty() ? "ring" : password;
    turn_param_v4.isPeerConnection = true; // Request for TCP peer connections, not UDP
    turn_param_v4.onPeerConnection = [this](uint32_t conn_id, const IpAddr& peer_addr, bool connected) {
        (void)conn_id;
        if (connected)
            ctrl << makeMsg<CtrlMsgType::TURN_PEER_CONNECT>(peer_addr);
        else
            ctrl << makeMsg<CtrlMsgType::TURN_PEER_DISCONNECT>(peer_addr);
    };

    // If a previous turn server exists, but is not ready, we should try to reconnect
    if (turnAuthv4_ && !turnAuthv4_->isReady())
        turnAuthv4_.reset();
    if (turnAuthv6_ && !turnAuthv6_->isReady())
        turnAuthv6_.reset();

    if (!turnAuthv4_ || !turnAuthv4_->isReady()) {
        turn_param_v4.authorized_family = PJ_AF_INET;
        turnAuthv4_ = std::make_unique<TurnTransport>(turn_param_v4);
    }

    if (!turnAuthv6_ || !turnAuthv6_->isReady()) {
        auto turn_param_v6 = turn_param_v4;
        turn_param_v6.authorized_family = PJ_AF_INET6;
        turnAuthv6_ = std::make_unique<TurnTransport>(turn_param_v6);
    }

    // Wait until TURN server READY state (or timeout)
    Timeout<Clock> timeout {NET_CONNECTION_TIMEOUT};
    timeout.start();
    while (!turnAuthv4_->isReady() && !turnAuthv6_->isReady()) {
        if (timeout) {
            JAMI_WARN("Turn: connection timeout, will only try p2p file transfer.");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
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
    JAMI_DBG() << account << "[CNX] TURN connection attempt from "
               << peer_addr.toString(true, true);

    auto turn_ep = std::unique_ptr<ConnectedTurnTransport>(nullptr);
    if (peer_addr.isIpv4())
        turn_ep = std::make_unique<ConnectedTurnTransport>(*turnAuthv4_, peer_addr);
    else
        turn_ep = std::make_unique<ConnectedTurnTransport>(*turnAuthv6_, peer_addr);

    JAMI_DBG() << account << "[CNX] start TLS session over TURN socket";
    dht::InfoHash peer_h;
    auto tls_ep = std::make_unique<TlsTurnEndpoint>(
        *turn_ep, account.identity(), account.dhParams(),
        [&, this] (const dht::crypto::Certificate& cert) { return validatePeerCertificate(cert, peer_h); });

    // block until TLS is negotiated (with 3 secs of timeout) (must throw in case of error)
    try {
        tls_ep->waitForReady(SOCK_TIMEOUT);
    } catch (const std::logic_error& e) {
        // In case of a timeout
        JAMI_WARN() << "TLS connection timeout from peer " << peer_addr.toString(true, true) << ": " << e.what();
        return;
    } catch (...) {
        JAMI_WARN() << "[CNX] TLS connection failure from peer " << peer_addr.toString(true, true);
        return;
    }

    JAMI_DBG() << account << "[CNX] Accepted TLS-TURN connection from RingID " << peer_h;
    connectedPeers_.emplace(peer_addr, tls_ep->peerCertificate().getId());
    auto connection = std::make_unique<PeerConnection>(
        [] {}, peer_addr.toString(), std::move(tls_ep));
    connection->attachOutputStream(std::make_shared<FtpServer>(account.getAccountID(), peer_h.toString()));
    servers_.emplace(std::make_pair(peer_h, peer_addr), std::move(connection));

    // note: operating this way let endpoint to be deleted safely in case of exceptions
    turnEndpoints_.emplace(std::make_pair(peer_addr, std::move(turn_ep)));
}

void
DhtPeerConnector::Impl::onTurnPeerDisconnection(const IpAddr& peer_addr)
{
    auto it = std::find_if(servers_.begin(), servers_.end(),
                [&peer_addr](const auto& element) {
                    return element.first.second == peer_addr;});
    if (it == servers_.end()) return;
    JAMI_WARN() << account << "[CNX] disconnection from peer " << peer_addr.toString(true, true);
    servers_.erase(it);
    connectedPeers_.erase(peer_addr);
    turnEndpoints_.erase(peer_addr);
}

void
DhtPeerConnector::Impl::onRequestMsg(PeerConnectionMsg&& request)
{
    JAMI_DBG() << account << "[CNX] rx DHT request from " << request.from;

    // Asynch certificate checking -> trig onTrustedRequestMsg when trusted certificate is found
    account.findCertificate(
        request.from,
        [this, request=std::move(request)] (const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
            dht::InfoHash peer_h;
            if (AccountManager::foundPeerDevice(cert, peer_h))
                onTrustedRequestMsg(std::move(request), cert, peer_h);
            else
                JAMI_WARN() << account << "[CNX] rejected untrusted connection request from "
                            << request.from;
    });
}

void
DhtPeerConnector::Impl::onTrustedRequestMsg(PeerConnectionMsg&& request,
                                            const std::shared_ptr<dht::crypto::Certificate>& cert,
                                            const dht::InfoHash& peer_h)
{
    answer_threads_.emplace_back(&DhtPeerConnector::Impl::answerToRequest, this, request, cert, peer_h);
}

void
DhtPeerConnector::Impl::answerToRequest(PeerConnectionMsg&& request,
                                            const std::shared_ptr<dht::crypto::Certificate>& cert,
                                            const dht::InfoHash& peer_h)
{
    // start a TURN client connection on first pass, next ones just add new peer cnx handlers
    bool sendTurn = turnConnect();

    // Save peer certificate for later TLS session (MUST BE DONE BEFORE TURN PEER AUTHORIZATION)
    certMap_.emplace(cert->getId(), std::make_pair(cert, peer_h));

    auto sendRelayV4 = false, sendRelayV6 = false, sendIce = false;
    std::shared_ptr<bool> iceReady = std::make_shared<bool>(false);

    std::shared_ptr<std::condition_variable> cv =
        std::make_shared < std::condition_variable>();
    for (auto& ip: request.addresses) {
        try {
            if (IpAddr(ip).isIpv4()) {
                if (!sendTurn) continue;
                sendRelayV4 = true;
                turnAuthv4_->permitPeer(ip);
                JAMI_DBG() << account << "[CNX] authorized peer connection from " << ip;
            } else if (IpAddr(ip).isIpv6()) {
                if (!sendTurn) continue;
                sendRelayV6 = true;
                turnAuthv6_->permitPeer(ip);
                JAMI_DBG() << account << "[CNX] authorized peer connection from " << ip;
            } else {
                // P2P File transfer. We received an ice SDP message:
                JAMI_DBG() << account << "[CNX] receiving ICE session request";
                auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
                auto ice_config = account.getIceOptions();
                ice_config.tcpEnable = true;
                ice_config.aggressive = true;
                ice_config.onRecvReady = [this, cv, iceReady]() {
                    if (iceReady) *iceReady = true;
                    if (cv) cv->notify_one();
                };
                ice_ = iceTransportFactory.createTransport(account.getAccountID().c_str(), 1, true, ice_config);

                if (ice_->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
                    JAMI_ERR("Cannot initialize ICE session.");
                    continue;
                }

                auto sdp = parse_SDP(ip);
                if (not ice_->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
                  JAMI_WARN("[Account:%s] start ICE failed - fallback to TURN",
                            account.getAccountID().c_str());
                  continue;
                }

                ice_->waitForNegotiation(10);
                if (ice_->isRunning()) {
                    sendIce = true;
                    JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP", account.getAccountID().c_str());
                } else {
                    JAMI_WARN("[Account:%s] ICE negotation failed", account.getAccountID().c_str());
                }
            }
        } catch (const std::exception& e) {
            JAMI_WARN() << account << "[CNX] ignored peer connection '" << ip << "', " << e.what();
        }
    }

    // Prepare connection request as a DHT message
    std::vector<std::string> addresses;

    if (sendIce) {
        // NOTE: This is a shortest version of a real SDP message to save some bits
        auto iceAttributes = ice_->getLocalAttributes();
        std::stringstream icemsg;
        icemsg << iceAttributes.ufrag << "\n";
        icemsg << iceAttributes.pwd << "\n";
        for (const auto &addr : ice_->getLocalCandidates(0)) {
          icemsg << addr << "\n";
        }
        addresses = {icemsg.str()};
    }

    if (sendTurn) {
        auto relayIpv4 = turnAuthv4_->peerRelayAddr();
        if (sendRelayV4 && relayIpv4)
            addresses.emplace_back(relayIpv4.toString(true, true));
        auto relayIpv6 = turnAuthv6_->peerRelayAddr();
        if (sendRelayV6 && relayIpv6)
            addresses.emplace_back(relayIpv6.toString(true, true));
    }

    if (addresses.empty()) {
        JAMI_DBG() << account << "[CNX] connection aborted, no family address found";
        return;
    }

    JAMI_DBG() << account << "[CNX] connection accepted, DHT reply to " << request.from;
    account.dht()->putEncrypted(
        dht::InfoHash::get(PeerConnectionMsg::key_prefix + request.from.toString()),
        request.from, request.respond(addresses));

    if (sendIce) {

        std::mutex mtx;
        std::unique_lock<std::mutex> lk{mtx};
        ice_->setSlaveSession();
        cv->wait_for(lk, ICE_READY_TIMEOUT);
        if (!*iceReady) {
            // This will fallback on TURN if ICE is not ready
            return;
        }
        std::unique_ptr<AbstractSocketEndpoint> peer_ep =
            std::make_unique<IceSocketEndpoint>(ice_, false);
        JAMI_DBG() << account << "[CNX] start TLS session";
        auto ph = peer_h;
        auto tls_ep = std::make_unique<TlsSocketEndpoint>(
            *peer_ep, account.identity(), account.dhParams(),
            [&, this](const dht::crypto::Certificate &cert) {
              return validatePeerCertificate(cert, ph);
            });
        // block until TLS is negotiated (with 3 secs of timeout)
        // (must throw in case of error)
        try {
          tls_ep->waitForReady(SOCK_TIMEOUT);
        } catch (const std::exception &e) {
          // In case of a timeout
          JAMI_WARN() << "TLS connection timeout " << e.what();
          return;
        }

        auto connection = std::make_unique<PeerConnection>(
            [] {}, peer_h.toString(),
            std::move(tls_ep));
        connection->attachOutputStream(std::make_shared<FtpServer>(
            account.getAccountID(), peer_h.toString()));
        servers_.emplace(std::make_pair(peer_h, ice_->getRemoteAddress(0)),
                        std::move(connection));
        p2pEndpoints_.emplace(std::make_pair(peer_h, ice_->getRemoteAddress(0)),
                        std::move(peer_ep));
    }
    // Now wait for a TURN connection from peer (see onTurnPeerConnection) if fallbacking
}

void
DhtPeerConnector::Impl::onResponseMsg(PeerConnectionMsg&& response)
{
    JAMI_DBG() << account << "[CNX] rx DHT reply from " << response.from;
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client: clients_) {
        // NOTE We can receives multiple files from one peer. So fill unanswered clients.
        if (client.first.first == response.from
            && client.second && !client.second->hasAlreadyAResponse()) {
            client.second->onDhtResponse(std::move(response));
            break;
        }
    }
}

void
DhtPeerConnector::Impl::onAddDevice(const dht::InfoHash& dev_h,
                                    const DRing::DataTransferId& tid,
                                    const std::shared_ptr<dht::crypto::Certificate>& peer_cert,
                                    const std::vector<std::string>& public_addresses,
                                    const std::function<void(PeerConnection*)>& connect_cb)
{
    auto client = std::make_pair(dev_h, tid);
    std::lock_guard<std::mutex> lock(clientsMutex_);
    const auto& iter = clients_.find(client);
    if (iter == std::end(clients_)) {
        clients_.emplace(
            client,
            std::make_unique<Impl::ClientConnector>(*this, tid, dev_h, peer_cert, public_addresses, connect_cb));
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
              onTurnPeerConnection(
                  ctrlMsgData<CtrlMsgType::TURN_PEER_CONNECT>(*msg));
              break;

            case CtrlMsgType::TURN_PEER_DISCONNECT:
              onTurnPeerDisconnection(
                  ctrlMsgData<CtrlMsgType::TURN_PEER_DISCONNECT>(*msg));
              break;

            case CtrlMsgType::CANCEL:
                {
                    auto dev_h = ctrlMsgData<CtrlMsgType::CANCEL, 0>(*msg);
                    auto id = ctrlMsgData<CtrlMsgType::CANCEL, 1>(*msg);
                    // Cancel outgoing files
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clients_.erase(std::make_pair(dev_h, id));
                    // Cancel incoming files
                    auto it = std::find_if(
                        servers_.begin(), servers_.end(),
                        [&dev_h, &id](const auto &element) {
                          return (element.first.first == dev_h &&
                                  element.second &&
                                  element.second->hasStreamWithId(id));
                        });
                    if (it == servers_.end())  {
                      Manager::instance().dataTransfers->close(id);
                      break;
                    }
                    auto peer = it->first.second; // tmp copy to prevent use-after-free below
                    servers_.erase(it);
                    // Remove the file transfer if p2p
                    auto p2p_it = std::find_if(p2pEndpoints_.begin(), p2pEndpoints_.end(),
                                    [&dev_h, &peer](const auto &element) {
                                        return (element.first.first == dev_h &&
                                                element.first.second == peer);
                                    });
                    if (p2p_it != p2pEndpoints_.end())
                        p2pEndpoints_.erase(p2p_it);
                    connectedPeers_.erase(peer);
                    // Else it's via TURN!
                    turnEndpoints_.erase(peer);
                    Manager::instance().dataTransfers->close(id);
                }
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
                          ctrlMsgData<CtrlMsgType::ADD_DEVICE, 3>(*msg),
                          ctrlMsgData<CtrlMsgType::ADD_DEVICE, 4>(*msg));
              break;

            default: JAMI_ERR("BUG: got unhandled control msg!"); break;
        }
    }
}

//==============================================================================

DhtPeerConnector::DhtPeerConnector(JamiAccount& account)
    : pimpl_ {new Impl {account}}
{}

DhtPeerConnector::~DhtPeerConnector() = default;

/// Called by a JamiAccount when it's DHT is connected
/// Install a DHT LISTEN operation on given device to receive data connection requests and replies
/// The DHT key is Hash(PeerConnectionMsg::key_prefix + device_id), where '+' is the string concatenation.
void
DhtPeerConnector::onDhtConnected(const std::string& device_id)
{
    pimpl_->account.dht()->listen<PeerConnectionMsg>(
        dht::InfoHash::get(PeerConnectionMsg::key_prefix + device_id),
        [this](PeerConnectionMsg&& msg) {
            if (msg.from == pimpl_->account.dht()->getId())
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
                                    const DRing::DataTransferId& tid,
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
        [this, addresses, connect_cb, tid](const dht::InfoHash& dev_h) {
            if (dev_h == pimpl_->account.dht()->getId()) {
                JAMI_ERR() << pimpl_->account.getAccountID() << "[CNX] no connection to yourself, bad person!";
                return;
            }

            pimpl_->account.findCertificate(
                dev_h,
                [this, dev_h, addresses, connect_cb, tid] (const std::shared_ptr<dht::crypto::Certificate>& cert) {
                    pimpl_->ctrl << makeMsg<CtrlMsgType::ADD_DEVICE>(dev_h, tid, cert, addresses, connect_cb);
                });
        },

        [this, peer_h, connect_cb](bool found) {
            if (!found) {
                JAMI_WARN() << pimpl_->account.getAccountID() << "[CNX] aborted, no devices for " << peer_h;
                connect_cb(nullptr);
            }
        });
}

void
DhtPeerConnector::closeConnection(const std::string& peer_id, const DRing::DataTransferId& tid) {
    // NOTE: see TODO in previous function.

    // NOTE: There is two ways to store current PeerConnection. Indeed, if it is
    // an incoming data transfer, it will be stored in servers, and will be
    // determined via the pair (peer_h, tid), because we receives the file from
    // a contact. If it's an outgoing file, we send the file to a device, so,
    // will be determined via the pair (dev_h, tid).
    // That's why we need to call CANCEL for the peer_h and each dev_h.
    const auto peer_h = dht::InfoHash(peer_id);

    pimpl_->ctrl << makeMsg<CtrlMsgType::CANCEL>(peer_h, tid);
    pimpl_->account.forEachDevice(
        peer_h,
        [this, tid](const dht::InfoHash& dev_h) {
            if (dev_h == pimpl_->account.dht()->getId()) {
                JAMI_ERR() << pimpl_->account.getAccountID() << "[CNX] no connection to yourself, bad person!";
                return;
            }
            pimpl_->ctrl << makeMsg<CtrlMsgType::CANCEL>(dev_h, tid);
        },
        [this, peer_h](bool found) {
            if (!found) {
                JAMI_WARN() << pimpl_->account.getAccountID() << "[CNX] aborted, no devices for " << peer_h;
            }
        });
}

} // namespace jami
