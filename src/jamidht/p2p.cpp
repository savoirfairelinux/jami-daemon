/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
#include "turn_transport.h"
#include "account_manager.h"
#include "multiplexed_socket.h"
#include "connectionmanager.h"
#include "fileutils.h"

#include <opendht/default_types.h>
#include <opendht/rng.h>
#include <opendht/thread_pool.h>

#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <array>
#include <future>
#include <algorithm>
#include <type_traits>

namespace jami {

static constexpr std::chrono::seconds DHT_MSG_TIMEOUT {30};
static constexpr std::chrono::seconds NET_CONNECTION_TIMEOUT {10};
static constexpr std::chrono::seconds SOCK_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_READY_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_INIT_TIMEOUT {10};
static constexpr std::chrono::seconds ICE_NEGOTIATION_TIMEOUT {10};

using Clock = std::chrono::system_clock;
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;

//==============================================================================

// This namespace prevents a nasty ODR violation with definitions in peer_connection.cpp
inline namespace {

template<typename CT>
class Timeout
{
public:
    using clock = CT;
    using duration = typename CT::duration;
    using time_point = typename CT::time_point;

    explicit Timeout(const duration& delay)
        : delay {delay}
    {}

    void start() { start_ = clock::now(); }

    explicit operator bool() const { return (clock::now() - start_) >= delay; }

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
    static constexpr const char* key_prefix = "peer:";       ///< base to compute the DHT listen key

    dht::Value::Id id = dht::Value::INVALID_ID;
    uint32_t protocol {protocol_version}; ///< Protocol identification. First bit reserved to
                                          ///< indicate a request (0) or a response (1)
    std::vector<std::string> addresses; ///< Request: public addresses for TURN permission. Response:
                                        ///< TURN relay addresses (only 1 in current implementation)
    uint64_t tid {0};
    MSGPACK_DEFINE_MAP(id, protocol, addresses, tid)

    PeerConnectionMsg() = default;
    PeerConnectionMsg(dht::Value::Id id,
                      uint32_t aprotocol,
                      const std::string& arelay,
                      uint64_t transfer_id)
        : id {id}
        , protocol {aprotocol}
        , addresses {{arelay}}
        , tid {transfer_id}
    {}
    PeerConnectionMsg(dht::Value::Id id,
                      uint32_t aprotocol,
                      const std::vector<std::string>& asrelay,
                      uint64_t transfer_id)
        : id {id}
        , protocol {aprotocol}
        , addresses {asrelay}
        , tid {transfer_id}
    {}
    bool isRequest() const noexcept { return (protocol & 1) == 0; }
    PeerConnectionMsg respond(const IpAddr& relay) const
    {
        return {id, protocol | 1, relay.toString(true, true), tid};
    }
    PeerConnectionMsg respond(const std::vector<std::string>& addresses) const
    {
        return {id, protocol | 1, addresses, tid};
    }
};

} // namespace

//==============================================================================

class DhtPeerConnector::Impl : public std::enable_shared_from_this<DhtPeerConnector::Impl>
{
public:
    class ClientConnector;

    explicit Impl(const std::weak_ptr<JamiAccount>& account)
        : account {account}
    {}

    ~Impl()
    {
        for (auto& thread : answer_threads_)
            thread.join();
        {
            std::lock_guard<std::mutex> lk(serversMutex_);
            servers_.clear();
        }
        {
            std::lock_guard<std::mutex> lk(clientsMutex_);
            clients_.clear();
        }
        std::lock_guard<std::mutex> lk(waitForReadyMtx_);
        waitForReadyEndpoints_.clear();
    }

    std::weak_ptr<JamiAccount> account;

    bool hasPublicIp(const ICESDP& sdp)
    {
        for (const auto& cand : sdp.rem_candidates)
            if (cand.type == PJ_ICE_CAND_TYPE_SRFLX)
                return true;
        return false;
    }

    std::map<std::pair<dht::InfoHash, IpAddr>, std::unique_ptr<TlsSocketEndpoint>>
        waitForReadyEndpoints_;
    std::mutex waitForReadyMtx_ {};

    // key: Stored certificate PublicKey id (normaly it's the DeviceId)
    // value: pair of shared_ptr<Certificate> and associated RingId
    std::map<dht::InfoHash, std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::InfoHash>>
        certMap_;
    std::map<IpAddr, dht::InfoHash> connectedPeers_;

    std::map<std::pair<dht::InfoHash, IpAddr>, std::unique_ptr<PeerConnection>> servers_;
    std::mutex serversMutex_;
    std::map<IpAddr, std::unique_ptr<TlsTurnEndpoint>> tls_turn_ep_;

    std::map<std::pair<dht::InfoHash, DRing::DataTransferId>, std::unique_ptr<ClientConnector>>
        clients_;
    std::mutex clientsMutex_;

    void cancel(const std::string& peer_id, const DRing::DataTransferId& tid);
    void cancelChanneled(const std::string& peer_id, const DRing::DataTransferId& tid);

    void onRequestMsg(PeerConnectionMsg&&);
    void onTrustedRequestMsg(PeerConnectionMsg&&,
                             const std::shared_ptr<dht::crypto::Certificate>&,
                             const dht::InfoHash&);
    void answerToRequest(PeerConnectionMsg&&,
                         const std::shared_ptr<dht::crypto::Certificate>&,
                         const dht::InfoHash&);
    void onResponseMsg(PeerConnectionMsg&&);
    void onAddDevice(const dht::InfoHash&,
                     const DRing::DataTransferId&,
                     const std::shared_ptr<dht::crypto::Certificate>&,
                     const std::vector<std::string>&,
                     const std::function<void(PeerConnection*)>&);
    bool turnConnect();
    bool validatePeerCertificate(const dht::crypto::Certificate&, dht::InfoHash&);
    void stateChanged(const std::string& peer_id,
                      const DRing::DataTransferId& tid,
                      const DRing::DataTransferEventCode& code);
    void closeConnection(const std::string& peer_id, const DRing::DataTransferId& tid);

    std::future<void> loopFut_; // keep it last member

    std::vector<std::thread> answer_threads_;

    std::shared_ptr<DhtPeerConnector::Impl> shared()
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl>(shared_from_this());
    }
    std::shared_ptr<DhtPeerConnector::Impl const> shared() const
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl const>(shared_from_this());
    }
    std::weak_ptr<DhtPeerConnector::Impl> weak()
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl>(shared_from_this());
    }
    std::weak_ptr<DhtPeerConnector::Impl const> weak() const
    {
        return std::static_pointer_cast<DhtPeerConnector::Impl const>(shared_from_this());
    }

    // For Channeled transports
    std::mutex channeledIncomingMtx_;
    std::map<DRing::DataTransferId, std::unique_ptr<ChanneledIncomingTransfer>> channeledIncoming_;
    std::mutex channeledOutgoingMtx_;
    std::map<DRing::DataTransferId, std::vector<std::shared_ptr<ChanneledOutgoingTransfer>>>
        channeledOutgoing_;
    std::mutex incomingTransfersMtx_;
    std::set<DRing::DataTransferId> incomingTransfers_;
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
        : tid_ {tid}
        , parent_ {parent}
        , peer_ {peer_h}
        , publicAddresses_ {public_addresses}
        , peerCertificate_ {peer_cert}
    {
        auto shared = parent_.account.lock();
        if (!shared)
            return;
        waitId_ = ValueIdDist()(shared->rand);
        addListener(connect_cb);
        processTask_ = std::async(std::launch::async, [this] {
            try {
                process();
            } catch (const std::exception& e) {
                JAMI_ERR() << "[CNX] exception during client processing: " << e.what();
                cancel();
            }
        });
    }

    ~ClientConnector()
    {
        {
            std::lock_guard<std::mutex> lk {listenersMutex_};
            for (auto& cb : listeners_)
                cb(nullptr);
        }
        connection_.reset();
    }

    bool hasAlreadyAResponse() { return responseReceived_; }

    bool waitId(uint64_t id) { return waitId_ == id; }

    void addListener(const ListenerFunction& cb)
    {
        if (!connected_) {
            std::lock_guard<std::mutex> lk {listenersMutex_};
            listeners_.emplace_back(cb);
        } else {
            cb(connection_.get());
        }
    }

    void cancel() { parent_.cancel(peer_.toString(), tid_); }

    void onDhtResponse(PeerConnectionMsg&& response)
    {
        if (responseReceived_)
            return;
        response_ = std::move(response);
        responseReceived_ = true;
        responseCV_.notify_all();
    }

    const DRing::DataTransferId tid_;

private:
    void process()
    {
        // Add ice msg into the addresses
        // TODO remove publicAddresses in the future and only use the iceMsg
        // For now it's here for compability with old version
        auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
        auto acc = parent_.account.lock();
        if (!acc)
            return;
        auto ice_config = acc->getIceOptions();
        ice_config.tcpEnable = true;
        auto ice = iceTransportFactory.createTransport(acc->getAccountID().c_str(),
                                                       1,
                                                       false,
                                                       ice_config);

        if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
            JAMI_ERR("Cannot initialize ICE session.");
            cancel();
            return;
        }

        acc->registerDhtAddress(*ice);

        auto iceAttributes = ice->getLocalAttributes();
        std::stringstream icemsg;
        icemsg << iceAttributes.ufrag << "\n";
        icemsg << iceAttributes.pwd << "\n";
        for (const auto& addr : ice->getLocalCandidates(0)) {
            icemsg << addr << "\n";
        }

        // Prepare connection request as a DHT message
        PeerConnectionMsg request;
        request.id = waitId_; /* Random id for the message unicity */
        request.addresses = {icemsg.str()};
        request.addresses.insert(request.addresses.end(),
                                 publicAddresses_.begin(),
                                 publicAddresses_.end());
        request.tid = tid_;

        // Send connection request through DHT
        JAMI_DBG() << acc << "[CNX] request connection to " << peer_;
        acc->dht()->putEncrypted(dht::InfoHash::get(PeerConnectionMsg::key_prefix
                                                    + peer_.toString()),
                                 peer_,
                                 request,
                                 [](bool ok) {
                                     if (ok)
                                         JAMI_DBG("[CNX] successfully put CNX request on DHT");
                                     else
                                         JAMI_ERR("[CNX] error putting CNX request on DHT");
                                 });

        // Wait for call to onResponse() operated by DHT
        std::mutex mtx;
        std::unique_lock<std::mutex> lk {mtx};
        responseCV_.wait_for(lk, DHT_MSG_TIMEOUT);
        if (!responseReceived_) {
            JAMI_ERR("no response from DHT to E2E request. Cancel transfer");
            cancel();
            return;
        }

        // Check response validity
        std::unique_ptr<AbstractSocketEndpoint> peer_ep;
        if (response_.from != peer_ or response_.id != request.id or response_.addresses.empty())
            throw std::runtime_error("invalid connection reply");

        IpAddr relay_addr;
        for (const auto& address : response_.addresses) {
            if (!(address.size() <= PJ_MAX_HOSTNAME && (relay_addr = address))) {
                // Should be ICE SDP
                // P2P File transfer. We received an ice SDP message:
                auto sdp = IceTransport::parse_SDP(address, *ice);
                // NOTE: hasPubIp is used for compability (because ICE is waiting for a certain
                // state in old versions) This can be removed when old versions will be unsupported.
                auto hasPubIp = parent_.hasPublicIp(sdp);
                if (!hasPubIp)
                    ice->setInitiatorSession();
                if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
                    JAMI_WARN("[Account:%s] start ICE failed - fallback to TURN",
                              acc->getAccountID().c_str());
                    break;
                }

                ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
                if (ice->isRunning()) {
                    peer_ep = std::make_unique<IceSocketEndpoint>(ice, true);
                    JAMI_DBG("[Account:%s] ICE negotiation succeed. Starting file transfer",
                             acc->getAccountID().c_str());
                    if (hasPubIp)
                        ice->setInitiatorSession();
                    break;
                } else {
                    JAMI_ERR("[Account:%s] ICE negotation failed", acc->getAccountID().c_str());
                }
            } else {
                try {
                    // Connect to TURN peer using a raw socket
                    JAMI_DBG() << acc << "[CNX] connecting to TURN relay "
                               << relay_addr.toString(true, true);
                    peer_ep = std::make_unique<TcpSocketEndpoint>(relay_addr);
                    try {
                        peer_ep->connect(SOCK_TIMEOUT);
                    } catch (const std::logic_error& e) {
                        // In case of a timeout
                        JAMI_WARN() << "TcpSocketEndpoint timeout for addr "
                                    << relay_addr.toString(true, true) << ": " << e.what();
                        cancel();
                        return;
                    } catch (...) {
                        JAMI_WARN() << "TcpSocketEndpoint failure for addr "
                                    << relay_addr.toString(true, true);
                        cancel();
                        return;
                    }
                    break;
                } catch (std::system_error&) {
                    JAMI_DBG() << acc << "[CNX] Failed to connect to TURN relay "
                               << relay_addr.toString(true, true);
                }
            }
        }

        if (!peer_ep) {
            cancel();
            return;
        }

        // Negotiate a TLS session
        JAMI_DBG() << acc << "[CNX] start TLS session";
        tls_ep_ = std::make_unique<TlsSocketEndpoint>(std::move(peer_ep),
                                                      acc->identity(),
                                                      acc->dhParams(),
                                                      *peerCertificate_,
                                                      ice->isRunning());
        tls_ep_->setOnStateChange([this, ice = std::move(ice)](tls::TlsSessionState state) {
            if (state == tls::TlsSessionState::SHUTDOWN) {
                if (!connected_)
                    JAMI_WARN() << "TLS connection failure from peer " << peer_.toString();
                ice->cancelOperations(); // This will stop current PeerChannel operations
                cancel();
                return false;
            } else if (state == tls::TlsSessionState::ESTABLISHED) {
                // Connected!
                connected_ = true;
                connection_ = std::make_unique<PeerConnection>([this] { cancel(); },
                                                               peer_.toString(),
                                                               std::move(tls_ep_));
                connection_->setOnStateChangedCb(
                    [p = parent_.weak(),
                     peer = peer_.toString()](const DRing::DataTransferId& id,
                                              const DRing::DataTransferEventCode& code) {
                        // NOTE: this callback is shared by all potential inputs/output, not
                        // only used by connection_, weak pointers MUST be used.
                        auto parent = p.lock();
                        if (!parent)
                            return;
                        parent->stateChanged(peer, id, code);
                    });

                std::lock_guard<std::mutex> lk(listenersMutex_);
                for (auto& cb : listeners_) {
                    cb(connection_.get());
                }
            }
            return true;
        });
    }

    Impl& parent_;
    const dht::InfoHash peer_;

    std::vector<std::string> publicAddresses_;
    std::atomic_bool responseReceived_ {false};
    std::condition_variable responseCV_ {};
    PeerConnectionMsg response_;
    uint64_t waitId_ {0};
    std::shared_ptr<dht::crypto::Certificate> peerCertificate_;
    std::unique_ptr<PeerConnection> connection_;
    std::unique_ptr<TlsSocketEndpoint> tls_ep_;

    std::atomic_bool connected_ {false};
    std::mutex listenersMutex_;
    std::vector<ListenerFunction> listeners_;

    std::future<void> processTask_;
};

//==============================================================================

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

void
DhtPeerConnector::Impl::onRequestMsg(PeerConnectionMsg&& request)
{
    auto acc = account.lock();
    if (!acc)
        return;
    JAMI_DBG() << acc << "[CNX] rx DHT request from " << request.from;

    // Asynch certificate checking -> trig onTrustedRequestMsg when trusted certificate is found
    acc->findCertificate(request.from,
                         [this, request = std::move(request)](
                             const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                             auto acc = account.lock();
                             if (!acc)
                                 return;
                             dht::InfoHash peer_h;
                             if (AccountManager::foundPeerDevice(cert, peer_h))
                                 onTrustedRequestMsg(std::move(request), cert, peer_h);
                             else
                                 JAMI_WARN()
                                     << acc << "[CNX] rejected untrusted connection request from "
                                     << request.from;
                         });
}

void
DhtPeerConnector::Impl::onTrustedRequestMsg(PeerConnectionMsg&& request,
                                            const std::shared_ptr<dht::crypto::Certificate>& cert,
                                            const dht::InfoHash& peer_h)
{
    answer_threads_.emplace_back(&DhtPeerConnector::Impl::answerToRequest,
                                 this,
                                 request,
                                 cert,
                                 peer_h);
}

void
DhtPeerConnector::Impl::answerToRequest(PeerConnectionMsg&& request,
                                        const std::shared_ptr<dht::crypto::Certificate>& cert,
                                        const dht::InfoHash& peer_h)
{
    auto acc = account.lock();
    if (!acc)
        return;

    if (request.tid != 0) {
        std::lock_guard<std::mutex> lk(incomingTransfersMtx_);
        if (incomingTransfers_.find(request.tid) != incomingTransfers_.end()) {
            JAMI_INFO("Incoming request for id(%lu) is already treated via channeled socket",
                      request.tid);
            return;
        }
        incomingTransfers_.emplace(request.tid);
    }

    // Save peer certificate for later TLS session (MUST BE DONE BEFORE TURN PEER AUTHORIZATION)
    certMap_.emplace(cert->getId(), std::make_pair(cert, peer_h));

    auto sendIce = false, hasPubIp = false;

    struct IceReady
    {
        std::mutex mtx {};
        std::condition_variable cv {};
        bool ready {false};
    };
    auto iceReady = std::make_shared<IceReady>();
    std::shared_ptr<IceTransport> ice;
    for (auto& ip : request.addresses) {
        try {
            if (ip.size() <= PJ_MAX_HOSTNAME) {
                IpAddr addr(ip);
                if (addr.isIpv4() || addr.isIpv6()) {
                    JAMI_WARN() << "Deprecated TURN connection. Ignore";
                    continue;
                }
            }

            // P2P File transfer. We received an ice SDP message:
            JAMI_DBG() << acc << "[CNX] receiving ICE session request";
            auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
            auto ice_config = acc->getIceOptions();
            ice_config.tcpEnable = true;
            ice_config.onRecvReady = [iceReady]() {
                auto& ir = *iceReady;
                std::lock_guard<std::mutex> lk {ir.mtx};
                ir.ready = true;
                ir.cv.notify_one();
            };
            ice = iceTransportFactory.createTransport(acc->getAccountID().c_str(),
                                                      1,
                                                      true,
                                                      ice_config);

            if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
                JAMI_ERR("Cannot initialize ICE session.");
                continue;
            }

            acc->registerDhtAddress(*ice);

            auto sdp = IceTransport::parse_SDP(ip, *ice);
            // NOTE: hasPubIp is used for compability (because ICE is waiting for a certain state in
            // old versions) This can be removed when old versions will be unsupported (version
            // before this patch)
            hasPubIp = hasPublicIp(sdp);
            if (not ice->start({sdp.rem_ufrag, sdp.rem_pwd}, sdp.rem_candidates)) {
                JAMI_WARN("[Account:%s] start ICE failed - fallback to TURN",
                          acc->getAccountID().c_str());
                continue;
            }

            if (!hasPubIp) {
                ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
                if (ice->isRunning()) {
                    sendIce = true;
                    JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP",
                             acc->getAccountID().c_str());
                } else {
                    JAMI_WARN("[Account:%s] ICE negotation failed", acc->getAccountID().c_str());
                    ice->cancelOperations();
                }
            } else
                sendIce = true; // Ice started with success, we can use it.
        } catch (const std::exception& e) {
            JAMI_WARN() << acc << "[CNX] ignored peer connection '" << ip << "', " << e.what();
        }
    }

    // Prepare connection request as a DHT message
    std::vector<std::string> addresses;

    if (sendIce) {
        // NOTE: This is a shortest version of a real SDP message to save some bits
        auto iceAttributes = ice->getLocalAttributes();
        std::stringstream icemsg;
        icemsg << iceAttributes.ufrag << "\n";
        icemsg << iceAttributes.pwd << "\n";
        for (const auto& addr : ice->getLocalCandidates(0)) {
            icemsg << addr << "\n";
        }
        addresses = {icemsg.str()};
    }

    if (addresses.empty()) {
        JAMI_DBG() << acc << "[CNX] connection aborted, no family address found";
        return;
    }

    JAMI_DBG() << acc << "[CNX] connection accepted, DHT reply to " << request.from;
    acc->dht()->putEncrypted(dht::InfoHash::get(PeerConnectionMsg::key_prefix
                                                + request.from.toString()),
                             request.from,
                             request.respond(addresses));

    if (sendIce) {
        if (hasPubIp) {
            ice->waitForNegotiation(ICE_NEGOTIATION_TIMEOUT);
            if (ice->isRunning()) {
                JAMI_DBG("[Account:%s] ICE negotiation succeed. Answering with local SDP",
                         acc->getAccountID().c_str());
            } else {
                JAMI_WARN("[Account:%s] ICE negotation failed - Fallbacking to TURN",
                          acc->getAccountID().c_str());
                return; // wait for onTurnPeerConnection
            }
        }

        if (not iceReady->ready) {
            if (!hasPubIp)
                ice->setSlaveSession();
            std::unique_lock<std::mutex> lk {iceReady->mtx};
            if (not iceReady->cv.wait_for(lk, ICE_READY_TIMEOUT, [&] { return iceReady->ready; })) {
                // This will fallback on TURN if ICE is not ready
                return;
            }
        }

        std::unique_ptr<AbstractSocketEndpoint> peer_ep = std::make_unique<IceSocketEndpoint>(ice,
                                                                                              false);
        JAMI_DBG() << acc << "[CNX] start TLS session";
        if (hasPubIp)
            ice->setSlaveSession();

        auto idx = std::make_pair(peer_h, ice->getRemoteAddress(0));
        std::lock_guard<std::mutex> lk(waitForReadyMtx_);
        auto it = waitForReadyEndpoints_.emplace(
            idx,
            std::make_unique<TlsSocketEndpoint>(std::move(peer_ep),
                                                acc->identity(),
                                                acc->dhParams(),
                                                [peer_h,
                                                 this](const dht::crypto::Certificate& cert) {
                                                    dht::InfoHash peer_h_found;
                                                    return validatePeerCertificate(cert,
                                                                                   peer_h_found)
                                                           and peer_h_found == peer_h;
                                                }));

        it.first->second->setOnStateChange([this,
                                            accountId = acc->getAccountID(),
                                            idx = std::move(idx)](tls::TlsSessionState state) {
            std::lock_guard<std::mutex> lk(waitForReadyMtx_);
            if (waitForReadyEndpoints_.find(idx) == waitForReadyEndpoints_.end())
                return false;
            if (state == tls::TlsSessionState::SHUTDOWN) {
                JAMI_WARN() << "TLS connection failure";
                waitForReadyEndpoints_.erase(idx);
                return false;
            } else if (state == tls::TlsSessionState::ESTABLISHED) {
                // Connected!
                auto peer_h = idx.first.toString();
                auto connection = std::make_unique<PeerConnection>([] {},
                                                                   peer_h,
                                                                   std::move(
                                                                       waitForReadyEndpoints_[idx]));
                connection->setOnStateChangedCb(
                    [w = weak(), peer_h](const DRing::DataTransferId& id,
                                         const DRing::DataTransferEventCode& code) {
                        if (auto sthis = w.lock()) {
                            sthis->stateChanged(peer_h, id, code);
                        }
                    });
                connection->attachOutputStream(std::make_shared<FtpServer>(accountId, peer_h));
                {
                    std::lock_guard<std::mutex> lk(serversMutex_);
                    servers_.emplace(idx, std::move(connection));
                }
                waitForReadyEndpoints_.erase(idx);
                return false;
            }
            return true;
        });
    } else {
        JAMI_WARN() << "No connection negotiated. Abort file transfer";
    }
}

void
DhtPeerConnector::Impl::onResponseMsg(PeerConnectionMsg&& response)
{
    auto acc = account.lock();
    if (!acc)
        return;
    JAMI_DBG() << acc << "[CNX] rx DHT reply from " << response.from;
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : clients_) {
        // NOTE We can receives multiple files from one peer. So fill unanswered clients with linked id.
        if (client.first.first == response.from && client.second
            && !client.second->hasAlreadyAResponse() && client.second->waitId(response.id)) {
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
        clients_.emplace(client,
                         std::make_unique<Impl::ClientConnector>(*this,
                                                                 tid,
                                                                 dev_h,
                                                                 peer_cert,
                                                                 public_addresses,
                                                                 connect_cb));
    } else {
        iter->second->addListener(connect_cb);
    }
}

void
DhtPeerConnector::Impl::cancel(const std::string& peer_id, const DRing::DataTransferId& tid)
{
    dht::ThreadPool::io().run([w = weak(), dev_h = dht::InfoHash(peer_id), tid] {
        auto shared = w.lock();
        if (!shared)
            return;
        // Cancel outgoing files
        {
            std::lock_guard<std::mutex> lock(shared->clientsMutex_);
            shared->clients_.erase(std::make_pair(dev_h, tid));
        }
        // Cancel incoming files
        std::unique_lock<std::mutex> lk(shared->serversMutex_);
        auto it = std::find_if(shared->servers_.begin(),
                               shared->servers_.end(),
                               [&dev_h, &tid](const auto& element) {
                                   return (element.first.first == dev_h && element.second
                                           && element.second->hasStreamWithId(tid));
                               });
        if (it == shared->servers_.end()) {
            Manager::instance().dataTransfers->close(tid);
            return;
        }
        auto peer = it->first.second; // tmp copy to prevent use-after-free below
        shared->servers_.erase(it);
        lk.unlock();
        // Remove the file transfer if p2p
        shared->connectedPeers_.erase(peer);
        Manager::instance().dataTransfers->close(tid);
    });
}

void
DhtPeerConnector::Impl::cancelChanneled(const std::string& peerId, const DRing::DataTransferId& tid)
{
    dht::ThreadPool::io().run([w = weak(), tid] {
        auto shared = w.lock();
        if (!shared)
            return;
        // Cancel outgoing files
        {
            std::lock_guard<std::mutex> lk(shared->channeledIncomingMtx_);
            auto it = shared->channeledIncoming_.erase(tid);
        }
        {
            std::lock_guard<std::mutex> lk(shared->channeledOutgoingMtx_);
            shared->channeledOutgoing_.erase(tid);
        }
    });
}

void
DhtPeerConnector::Impl::stateChanged(const std::string& peer_id,
                                     const DRing::DataTransferId& tid,
                                     const DRing::DataTransferEventCode& code)
{
    if (code == DRing::DataTransferEventCode::finished
        or code == DRing::DataTransferEventCode::closed_by_peer
        or code == DRing::DataTransferEventCode::timeout_expired)
        closeConnection(peer_id, tid);
}

void
DhtPeerConnector::Impl::closeConnection(const std::string& peer_id, const DRing::DataTransferId& tid)
{
    cancel(peer_id, tid);
    cancelChanneled(peer_id, tid);
}

//==============================================================================

DhtPeerConnector::DhtPeerConnector(JamiAccount& account)
    : pimpl_ {std::make_shared<Impl>(account.weak())}
{}

DhtPeerConnector::~DhtPeerConnector() = default;

/// Called by a JamiAccount when it's DHT is connected
/// Install a DHT LISTEN operation on given device to receive data connection requests and replies
/// The DHT key is Hash(PeerConnectionMsg::key_prefix + device_id), where '+' is the string concatenation.
void
DhtPeerConnector::onDhtConnected(const std::string& device_id)
{
    auto acc = pimpl_->account.lock();
    if (!acc)
        return;
    acc->dht()->listen<PeerConnectionMsg>(
        dht::InfoHash::get(PeerConnectionMsg::key_prefix + device_id),
        [this](PeerConnectionMsg&& msg) {
            auto acc = pimpl_->account.lock();
            if (!acc)
                return false;
            if (msg.from == acc->dht()->getId())
                return true;
            if (!acc->isMessageTreated(to_hex_string(msg.id))) {
                if (msg.isRequest())
                    pimpl_->onRequestMsg(std::move(msg));
                else
                    pimpl_->onResponseMsg(std::move(msg));
            }
            return true;
        },
        [](const dht::Value& v) {
            // Avoid to answer for peer requests
            return v.user_type != "peer_request";
        });
}

void
DhtPeerConnector::requestConnection(
    const std::string& peer_id,
    const DRing::DataTransferId& tid,
    bool isVCard,
    const std::function<void(PeerConnection*)>& connect_cb,
    const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>&
        channeledConnectedCb,
    const std::function<void()>& onChanneledCancelled)
{
    auto acc = pimpl_->account.lock();
    if (!acc)
        return;

    const auto peer_h = dht::InfoHash(peer_id);

    auto channelReadyCb =
        [this, tid, peer_id, channeledConnectedCb, onChanneledCancelled, connect_cb](
            const std::shared_ptr<ChannelSocket>& channel) {
            auto shared = pimpl_->account.lock();
            if (!channel) {
                onChanneledCancelled();
                return;
            }
            if (!shared)
                return;
            JAMI_INFO("New file channel for outgoing transfer with id(%lu)", tid);

            auto outgoingFile = std::make_shared<ChanneledOutgoingTransfer>(
                channel,
                [this, peer_id](const DRing::DataTransferId& id,
                                const DRing::DataTransferEventCode& code) {
                    pimpl_->stateChanged(peer_id, id, code);
                });
            if (!outgoingFile)
                return;
            {
                std::lock_guard<std::mutex> lk(pimpl_->channeledOutgoingMtx_);
                pimpl_->channeledOutgoing_[tid].emplace_back(outgoingFile);
            }

            channel->onShutdown([this, tid, onChanneledCancelled, peer = outgoingFile->peer()]() {
                JAMI_INFO("Channel down for outgoing transfer with id(%lu)", tid);
                onChanneledCancelled();
                dht::ThreadPool::io().run([w = pimpl_->weak(), tid, peer] {
                    auto shared = w.lock();
                    if (!shared)
                        return;
                    // Cancel outgoing files
                    {
                        std::lock_guard<std::mutex> lk(shared->channeledOutgoingMtx_);
                        auto outgoingTransfers = shared->channeledOutgoing_.find(tid);
                        if (outgoingTransfers != shared->channeledOutgoing_.end()) {
                            auto& currentTransfers = outgoingTransfers->second;
                            auto it = currentTransfers.begin();
                            while (it != currentTransfers.end()) {
                                auto& transfer = *it;
                                if (transfer && transfer->peer() == peer)
                                    it = currentTransfers.erase(it);
                                else
                                    ++it;
                            }
                            if (currentTransfers.empty())
                                shared->channeledOutgoing_.erase(outgoingTransfers);
                        }
                    }
                });
            });
            // Cancel via DHT because we will use the channeled path
            connect_cb(nullptr);
            channeledConnectedCb(outgoingFile);
        };

    if (isVCard) {
        acc->connectionManager().connectDevice(peer_id,
                                               "vcard://" + std::to_string(tid),
                                               channelReadyCb);
        return;
    }

    // Notes for reader:
    // 1) dht.getPublicAddress() suffers of a non-usability into forEachDevice() callbacks.
    //    If you call it in forEachDevice callbacks, it'll never not return...
    //    Seems that getPublicAddress() and forEachDevice() need to process into the same thread
    //    (here the one where dht_ loop runs).
    // 2) anyway its good to keep this processing here in case of multiple device
    //    as the result is the same for each device.
    auto addresses = acc->publicAddresses();

    acc->forEachDevice(
        peer_h,
        [this, addresses, connect_cb, tid, channelReadyCb = std::move(channelReadyCb)](
            const dht::InfoHash& dev_h) {
            auto acc = pimpl_->account.lock();
            if (!acc)
                return;
            if (dev_h == acc->dht()->getId()) {
                JAMI_ERR() << acc->getAccountID() << "[CNX] no connection to yourself, bad person!";
                return;
            }

            acc->connectionManager().connectDevice(dev_h.toString(),
                                                   "file://" + std::to_string(tid),
                                                   channelReadyCb);

            acc->findCertificate(dev_h,
                                 [this, dev_h, addresses, connect_cb, tid](
                                     const std::shared_ptr<dht::crypto::Certificate>& cert) {
                                     pimpl_->onAddDevice(dev_h, tid, cert, addresses, connect_cb);
                                 });
        },

        [this, peer_h, connect_cb, onChanneledCancelled, accId = acc->getAccountID()](bool found) {
            if (!found) {
                JAMI_WARN() << accId << "[CNX] aborted, no devices for " << peer_h;
                connect_cb(nullptr);
                onChanneledCancelled();
            }
        });
}

void
DhtPeerConnector::closeConnection(const std::string& peer_id, const DRing::DataTransferId& tid)
{
    pimpl_->closeConnection(peer_id, tid);
}

bool
DhtPeerConnector::onIncomingChannelRequest(const DRing::DataTransferId& tid)
{
    std::lock_guard<std::mutex> lk(pimpl_->incomingTransfersMtx_);
    if (pimpl_->incomingTransfers_.find(tid) != pimpl_->incomingTransfers_.end()) {
        JAMI_INFO("Incoming transfer request with id(%lu) is already treated via DHT", tid);
        return false;
    }
    pimpl_->incomingTransfers_.emplace(tid);
    JAMI_INFO("Incoming transfer request with id(%lu)", tid);
    return true;
}

void
DhtPeerConnector::onIncomingConnection(const std::string& peer_id,
                                       const DRing::DataTransferId& tid,
                                       const std::shared_ptr<ChannelSocket>& channel,
                                       InternalCompletionCb&& cb)
{
    if (!channel)
        return;
    auto acc = pimpl_->account.lock();
    if (!acc)
        return;
    auto incomingFile = std::make_unique<ChanneledIncomingTransfer>(
        channel,
        std::make_shared<FtpServer>(acc->getAccountID(), peer_id, tid, std::move(cb)),
        [this, peer_id](const DRing::DataTransferId& id, const DRing::DataTransferEventCode& code) {
            pimpl_->stateChanged(peer_id, id, code);
        });
    {
        std::lock_guard<std::mutex> lk(pimpl_->channeledIncomingMtx_);
        pimpl_->channeledIncoming_.emplace(tid, std::move(incomingFile));
    }
    channel->onShutdown([this, tid]() {
        JAMI_INFO("Channel down for incoming transfer with id(%lu)", tid);
        dht::ThreadPool::io().run([w = pimpl_->weak(), tid] {
            auto shared = w.lock();
            if (!shared)
                return;
            // Cancel incoming files
            // Note: erasing the channeled transfer will close the file via ftp_->close()
            std::lock_guard<std::mutex> lk(shared->channeledIncomingMtx_);
            shared->channeledIncoming_.erase(tid);
        });
    });
}

} // namespace jami
