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

#include "turn_transport.h"

#include "logger.h"
#include "ip_utils.h"
#include "sip/sip_utils.h"
#include "map_utils.h"

#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdexcept>
#include <future>
#include <atomic>
#include <thread>
#include <vector>
#include <iterator>
#include <mutex>
#include <sstream>
#include <limits>
#include <map>
#include <condition_variable>

namespace ring {

using MutexGuard = std::lock_guard<std::mutex>;
using MutexLock = std::unique_lock<std::mutex>;

enum class RelayState
{
    NONE,
    READY,
    DOWN,
};

class PeerChannel
{
public:
    PeerChannel() {}
    ~PeerChannel() {
        MutexGuard lk {mutex_};
        stop_ = true;
        cv_.notify_all();
    }

    PeerChannel(PeerChannel&&o) {
        MutexGuard lk {o.mutex_};
        stream_ = std::move(o.stream_);
    }
    PeerChannel& operator =(PeerChannel&& o) {
        MutexGuard lk {o.mutex_};
        stream_  = std::move(o.stream_);
        return *this;
    }

    void operator <<(const std::string& data) {
        MutexGuard lk {mutex_};
        stream_.clear();
        stream_ << data;
        cv_.notify_all();
    }

    void read(std::vector<char>& output) {
        MutexLock lk {mutex_};
        cv_.wait(lk, [&, this]{
                stream_.read(&output[0], output.size());
                return stream_.gcount() > 0 or stop_;
            });
        output.resize(stop_ ? 0 : stream_.gcount());
    }

    std::vector<char> readline() {
        MutexLock lk {mutex_};
        std::vector<char> result(3000);
        cv_.wait(lk, [&, this] {
                if (stop_)
                    return true;
                stream_.getline(&result[0], 3000);
                if (stream_) {
                    result.resize(stream_.gcount());
                    return result.size() > 0;
                }
                return false;
            });
        if (stop_)
            return {};
        return result;
    }

private:
    PeerChannel(const PeerChannel&o) = delete;
    PeerChannel& operator =(const PeerChannel& o) = delete;
    std::mutex mutex_ {};
    std::condition_variable cv_ {};
    std::stringstream stream_ {};
    bool stop_ {false};

    friend void operator <<(std::vector<char>&, PeerChannel&);
};

class TurnTransportPimpl
{
public:
    TurnTransportPimpl() = default;
    ~TurnTransportPimpl();

    void onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state);
    void onRxData(const uint8_t* pkt, unsigned pkt_len, const pj_sockaddr_t* peer_addr, unsigned addr_len);
    void onPeerConnection(pj_uint32_t conn_id, const pj_sockaddr_t* peer_addr, unsigned addr_len, pj_status_t status);
    void ioJob();

    std::mutex apiMutex_;

    std::map<IpAddr, PeerChannel> peerChannels_;

    TurnTransportParams settings;
    pj_caching_pool poolCache {};
    pj_pool_t* pool {nullptr};
    pj_stun_config stunConfig {};
    pj_turn_sock* relay {nullptr};
    pj_str_t relayAddr {};
    IpAddr peerRelayAddr; // address where peers should connect to
    IpAddr mappedAddr;

    std::atomic<RelayState> state {RelayState::NONE};
    std::atomic_bool ioJobQuit {false};
    std::thread ioWorker;
};

TurnTransportPimpl::~TurnTransportPimpl()
{
    if (relay)
        pj_turn_sock_destroy(relay);
    ioJobQuit = true;
    if (ioWorker.joinable())
        ioWorker.join();
    if (pool)
        pj_pool_release(pool);
    pj_caching_pool_destroy(&poolCache);
}

void
TurnTransportPimpl::onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state)
{
    if (new_state == PJ_TURN_STATE_READY) {
        pj_turn_session_info info;
        pj_turn_sock_get_info(relay, &info);
        peerRelayAddr = IpAddr {info.relay_addr};
        mappedAddr = IpAddr {info.mapped_addr};
        RING_DBG("TURN server ready, peer relay address: %s", peerRelayAddr.toString(true, true).c_str());
        state = RelayState::READY;
    } else if (old_state <= PJ_TURN_STATE_READY and new_state > PJ_TURN_STATE_READY) {
        RING_WARN("TURN server disconnected (%s)", pj_turn_state_name(new_state));
        state = RelayState::DOWN;
        MutexGuard lk {apiMutex_};
        peerChannels_.clear();
    }
}

void
TurnTransportPimpl::onRxData(const uint8_t* pkt, unsigned pkt_len,
                             const pj_sockaddr_t* addr, unsigned addr_len)
{
    IpAddr peer_addr (*static_cast<const pj_sockaddr*>(addr), addr_len);

    decltype(peerChannels_)::iterator channel_it;
    {
        MutexGuard lk {apiMutex_};
        channel_it = peerChannels_.find(peer_addr);
        if (channel_it == std::end(peerChannels_))
            return;
    }

    (channel_it->second) << std::string(reinterpret_cast<const char*>(pkt), pkt_len);
}

void
TurnTransportPimpl::onPeerConnection(pj_uint32_t conn_id,
                                     const pj_sockaddr_t* addr, unsigned addr_len,
                                     pj_status_t status)
{
    IpAddr peer_addr (*static_cast<const pj_sockaddr*>(addr), addr_len);
    if (status == PJ_SUCCESS) {
        RING_DBG() << "Received connection attempt from " << peer_addr.toString(true, true)
                   << ", id=" << std::hex << conn_id;
        pj_turn_connect_peer(relay, conn_id, addr, addr_len);

        {
            MutexGuard lk {apiMutex_};
            peerChannels_.emplace(peer_addr, PeerChannel {});
        }
    }

    if (settings.onPeerConnection)
        settings.onPeerConnection(conn_id, peer_addr, status == PJ_SUCCESS);
}

void
TurnTransportPimpl::ioJob()
{
    sip_utils::register_thread();

    while (!ioJobQuit.load()) {
        const pj_time_val delay = {0, 10};
        pj_ioqueue_poll(stunConfig.ioqueue, &delay);
        pj_timer_heap_poll(stunConfig.timer_heap, nullptr);
  }
}

class PjsipError final : public std::exception {
public:
    PjsipError() = default;
    explicit PjsipError(pj_status_t st) : std::exception() {
        char err_msg[PJ_ERR_MSG_SIZE];
        pj_strerror(st, err_msg, sizeof(err_msg));
        what_msg_ += ": ";
        what_msg_ += err_msg;
    }
    const char* what() const noexcept override {
        return what_msg_.c_str();
    };
private:
    std::string what_msg_ {"PJSIP api error"};
};

template <class Callable, class... Args>
inline void
PjsipCall(Callable& func, Args... args)
{
    auto status = func(args...);
    if (status != PJ_SUCCESS)
        throw PjsipError(status);
}

template <class Callable, class... Args>
inline auto
PjsipCallReturn(const Callable& func, Args... args) -> decltype(func(args...))
{
    auto res = func(args...);
    if (!res)
        throw PjsipError();
    return res;
}

//==================================================================================================

TurnTransport::TurnTransport(const TurnTransportParams& params)
    : pimpl_ {new TurnTransportPimpl}
{
    sip_utils::register_thread();

    auto server = params.server;
    if (!server.getPort())
        server.setPort(PJ_STUN_PORT);

    if (server.isUnspecified())
        throw std::invalid_argument("invalid turn server address");

    pimpl_->settings = params;

    // PJSIP memory pool
    pj_caching_pool_init(&pimpl_->poolCache, &pj_pool_factory_default_policy, 0);
    pimpl_->pool = PjsipCallReturn(pj_pool_create, &pimpl_->poolCache.factory,
                                   "RgTurnTr", 512, 512, nullptr);

    // STUN config
    pj_stun_config_init(&pimpl_->stunConfig, &pimpl_->poolCache.factory, 0, nullptr, nullptr);

    // create global timer heap
    PjsipCall(pj_timer_heap_create, pimpl_->pool, 1000, &pimpl_->stunConfig.timer_heap);

    // create global ioqueue
    PjsipCall(pj_ioqueue_create, pimpl_->pool, 16, &pimpl_->stunConfig.ioqueue);

    // run a thread to handles timer/ioqueue events
    pimpl_->ioWorker = std::thread([this]{ pimpl_->ioJob(); });

    // TURN callbacks
    pj_turn_sock_cb relay_cb;
    pj_bzero(&relay_cb, sizeof(relay_cb));
    relay_cb.on_rx_data = [](pj_turn_sock* relay, void* pkt, unsigned pkt_len,
                             const pj_sockaddr_t* peer_addr, unsigned addr_len) {
        auto tr = static_cast<TurnTransport*>(pj_turn_sock_get_user_data(relay));
        tr->pimpl_->onRxData(reinterpret_cast<uint8_t*>(pkt), pkt_len, peer_addr, addr_len);
    };
    relay_cb.on_state = [](pj_turn_sock* relay, pj_turn_state_t old_state,
                           pj_turn_state_t new_state) {
        auto tr = static_cast<TurnTransport*>(pj_turn_sock_get_user_data(relay));
        tr->pimpl_->onTurnState(old_state, new_state);
    };
    relay_cb.on_peer_connection = [](pj_turn_sock* relay, pj_uint32_t conn_id,
                                     const pj_sockaddr_t* peer_addr, unsigned addr_len,
                                     pj_status_t status) {
        auto tr = static_cast<TurnTransport*>(pj_turn_sock_get_user_data(relay));
        tr->pimpl_->onPeerConnection(conn_id, peer_addr, addr_len, status);
    };

    // TURN socket config
    pj_turn_sock_cfg turn_sock_cfg;
    pj_turn_sock_cfg_default(&turn_sock_cfg);
    turn_sock_cfg.max_pkt_size = params.maxPacketSize;

    // TURN socket creation
    PjsipCall(pj_turn_sock_create,
              &pimpl_->stunConfig, server.getFamily(), PJ_TURN_TP_TCP,
              &relay_cb, &turn_sock_cfg, this, &pimpl_->relay);

    // TURN allocation setup
    pj_turn_alloc_param turn_alloc_param;
	pj_turn_alloc_param_default(&turn_alloc_param);

    if (params.isPeerConnection)
        turn_alloc_param.peer_conn_type = PJ_TURN_TP_TCP; // RFC 6062!!!

    pj_stun_auth_cred cred;
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    pj_cstr(&cred.data.static_cred.realm, pimpl_->settings.realm.c_str());
    pj_cstr(&cred.data.static_cred.username, pimpl_->settings.username.c_str());
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    pj_cstr(&cred.data.static_cred.data, pimpl_->settings.password.c_str());

    pimpl_->relayAddr = pj_strdup3(pimpl_->pool, server.toString().c_str());

    // TURN connection/allocation
    RING_DBG() << "Connecting to TURN " << server.toString(true, true);
    PjsipCall(pj_turn_sock_alloc,
              pimpl_->relay, &pimpl_->relayAddr, server.getPort(),
              nullptr, &cred, &turn_alloc_param);
}

TurnTransport::~TurnTransport()
{}

void
TurnTransport::permitPeer(const IpAddr& addr)
{
    if (addr.isUnspecified())
        throw std::invalid_argument("invalid peer address");

    if (addr.getFamily() != pimpl_->peerRelayAddr.getFamily())
        throw std::invalid_argument("mismatching peer address family");

    PjsipCall(pj_turn_sock_set_perm, pimpl_->relay, 1, addr.pjPtr(), 1);
}

bool
TurnTransport::isReady() const
{
    return pimpl_->state.load() == RelayState::READY;
}

void
TurnTransport::waitServerReady()
{
    while (pimpl_->state.load() != RelayState::READY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

const IpAddr&
TurnTransport::peerRelayAddr() const
{
    return pimpl_->peerRelayAddr;
}

const IpAddr&
TurnTransport::mappedAddr() const
{
    return pimpl_->mappedAddr;
}

bool
TurnTransport::sendto(const IpAddr& peer, const char* const buffer, std::size_t length)
{
    sip_utils::register_thread();
    auto status = pj_turn_sock_sendto(pimpl_->relay,
                                      reinterpret_cast<const pj_uint8_t*>(buffer), length,
                                      peer.pjPtr(), peer.getLength());
    if (status != PJ_SUCCESS && status != PJ_EPENDING)
        throw PjsipError(status);

    return status == PJ_SUCCESS;
}

bool
TurnTransport::sendto(const IpAddr& peer, const std::vector<char>& buffer)
{
    return sendto(peer, &buffer[0], buffer.size());
}

bool
TurnTransport::writelineto(const IpAddr& peer, const char* const buffer, std::size_t length)
{
    if (sendto(peer, buffer, length))
        return sendto(peer, "\n", 1);
    return false;
}

void
TurnTransport::recvfrom(const IpAddr& peer, std::vector<char>& result)
{
    if (result.empty())
        throw std::runtime_error("TurnTransport::recvfrom() called with an empty output buffer");

    MutexLock lk {pimpl_->apiMutex_};
    auto& channel = pimpl_->peerChannels_.at(peer);
    lk.unlock();
    channel.read(result);
}

void
TurnTransport::readlinefrom(const IpAddr& peer, std::vector<char>& result)
{
    MutexLock lk {pimpl_->apiMutex_};
    auto& channel = pimpl_->peerChannels_.at(peer);
    lk.unlock();
    result = channel.readline();
}

std::vector<IpAddr>
TurnTransport::peerAddresses() const
{
    MutexLock lk {pimpl_->apiMutex_};
    return map_utils::extractKeys(pimpl_->peerChannels_);
}

} // namespace ring
