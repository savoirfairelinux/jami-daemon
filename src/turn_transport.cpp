/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

namespace jami {

using MutexGuard = std::lock_guard<std::mutex>;
using MutexLock = std::unique_lock<std::mutex>;

inline
namespace {

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
        stop();
    }

    PeerChannel(PeerChannel&&o) {
        MutexGuard lk {o.mutex_};
        stream_ = std::move(o.stream_);
    }

    PeerChannel& operator =(PeerChannel&& o) {
        std::lock(mutex_, o.mutex_);
        MutexGuard lk1 {mutex_, std::adopt_lock};
        MutexGuard lk2 {o.mutex_, std::adopt_lock};
        stream_  = std::move(o.stream_);
        return *this;
    }

    void operator <<(const std::string& data) {
        MutexGuard lk {mutex_};
        stream_.clear();
        stream_ << data;
        cv_.notify_one();
    }

    template <typename Duration>
    bool wait(Duration timeout) {
        std::lock(apiMutex_, mutex_);
        MutexGuard lk_api {apiMutex_, std::adopt_lock};
        MutexLock lk {mutex_, std::adopt_lock};
        return cv_.wait_for(lk, timeout, [this]{ return stop_ or !stream_.eof(); });
    }

    std::size_t read(char* output, std::size_t size) {
        std::lock(apiMutex_, mutex_);
        MutexGuard lk_api {apiMutex_, std::adopt_lock};
        MutexLock lk {mutex_, std::adopt_lock};
        cv_.wait(lk, [&, this]{
                if (stop_)
                    return true;
                stream_.read(&output[0], size);
                return stream_.gcount() > 0;
            });
        return stop_ ? 0 : stream_.gcount();
    }

    void stop() noexcept {
        {
            MutexGuard lk {mutex_};
            if (stop_)
                return;
            stop_ = true;
        }
        cv_.notify_all();

        // Make sure that no thread is blocked into read() or wait() methods
        MutexGuard lk_api {apiMutex_};
    }

private:
    PeerChannel(const PeerChannel&o) = delete;
    PeerChannel& operator =(const PeerChannel& o) = delete;
    std::mutex apiMutex_ {};
    std::mutex mutex_ {};
    std::condition_variable cv_ {};
    std::stringstream stream_ {};
    bool stop_ {false};

    friend void operator <<(std::vector<char>&, PeerChannel&);
};

}

//==============================================================================

template <class Callable, class... Args>
inline void
PjsipCall(Callable& func, Args... args)
{
    auto status = func(args...);
    if (status != PJ_SUCCESS)
        throw sip_utils::PjsipFailure(status);
}

template <class Callable, class... Args>
inline auto
PjsipCallReturn(const Callable& func, Args... args) -> decltype(func(args...))
{
    auto res = func(args...);
    if (!res)
        throw sip_utils::PjsipFailure();
    return res;
}

//==============================================================================

class TurnTransportPimpl
{
public:
    TurnTransportPimpl() = default;
    ~TurnTransportPimpl();

    void onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state);
    void onRxData(const uint8_t* pkt, unsigned pkt_len, const pj_sockaddr_t* peer_addr, unsigned addr_len);
    pj_status_t onPeerConnection(pj_uint32_t conn_id, const pj_sockaddr_t* peer_addr, unsigned addr_len);
    void ioJob();

    std::mutex apiMutex_;

    std::map<IpAddr, PeerChannel> peerChannels_;

    GenericSocket<uint8_t>::RecvCb onRxDataCb;
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
    if (relay && state.load() != RelayState::DOWN) {
        try {
            pj_turn_sock_destroy(relay);
        } catch (...) {
            JAMI_ERR() << "exception during pj_turn_sock_destroy() call (ignored)";
        }
    }
    ioJobQuit = true;
    if (ioWorker.joinable())
        ioWorker.join();
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
        JAMI_DBG("TURN server ready, peer relay address: %s", peerRelayAddr.toString(true, true).c_str());
        state = RelayState::READY;
    } else if (old_state <= PJ_TURN_STATE_READY and new_state > PJ_TURN_STATE_READY) {
        JAMI_WARN("TURN server disconnected (%s)", pj_turn_state_name(new_state));
        state = RelayState::DOWN;
        MutexGuard lk {apiMutex_};
        peerChannels_.clear();
    }
}

void
TurnTransportPimpl::onRxData(const uint8_t* pkt, unsigned pkt_len,
                             const pj_sockaddr_t* addr, unsigned addr_len)
{
    JAMI_ERR("ON RX");
    IpAddr peer_addr (*static_cast<const pj_sockaddr*>(addr), addr_len);

    decltype(peerChannels_)::iterator channel_it;
    {
        MutexGuard lk {apiMutex_};
        channel_it = peerChannels_.find(peer_addr);
        if (channel_it == std::end(peerChannels_))
            return;
    }

    if (onRxDataCb)
        onRxDataCb(pkt, pkt_len);
    else
        (channel_it->second) << std::string(reinterpret_cast<const char*>(pkt), pkt_len);
}

pj_status_t
TurnTransportPimpl::onPeerConnection(pj_uint32_t conn_id,
                                     const pj_sockaddr_t* addr, unsigned addr_len)
{
    IpAddr peer_addr (*static_cast<const pj_sockaddr*>(addr), addr_len);
    JAMI_DBG() << "Received connection attempt from "
                << peer_addr.toString(true, true) << ", id=" << std::hex
                << conn_id;
    {
        MutexGuard lk {apiMutex_};
        peerChannels_.emplace(peer_addr, PeerChannel {});
    }

    if (settings.onPeerConnection)
        settings.onPeerConnection(conn_id, peer_addr, true);
    return PJ_SUCCESS;
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

//==============================================================================

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
                                 JAMI_WARN("ON RX B %i", pkt_len);
        auto pimpl = static_cast<TurnTransportPimpl*>(pj_turn_sock_get_user_data(relay));
        pimpl->onRxData(reinterpret_cast<uint8_t*>(pkt), pkt_len, peer_addr, addr_len);
    };
    relay_cb.on_state = [](pj_turn_sock* relay, pj_turn_state_t old_state,
                           pj_turn_state_t new_state) {
                                 JAMI_WARN("ON STATE");

        auto pimpl = static_cast<TurnTransportPimpl*>(pj_turn_sock_get_user_data(relay));
        pimpl->onTurnState(old_state, new_state);
    };
    /*relay_cb.on_connection_attempt = [](pj_turn_sock *relay,
                                        pj_uint32_t conn_id,
                                        const pj_sockaddr_t *peer_addr,
                                        unsigned addr_len) {
      auto pimpl = static_cast<TurnTransportPimpl *>(pj_turn_sock_get_user_data(relay));
      return pimpl->onPeerConnection(conn_id, peer_addr, addr_len);
    };*/
    relay_cb.on_connection_status = [](pj_turn_sock *relay,
                        				pj_status_t status,
                                        pj_uint32_t conn_id,
                                        const pj_sockaddr_t *peer_addr,
                                        unsigned addr_len) {
      JAMI_ERR("===================== %i", status);
      auto pimpl = static_cast<TurnTransportPimpl *>(pj_turn_sock_get_user_data(relay));
      pimpl->onPeerConnection(conn_id, peer_addr, addr_len);
    };

    // TURN socket config
    pj_turn_sock_cfg turn_sock_cfg;
    pj_turn_sock_cfg_default(&turn_sock_cfg);
    turn_sock_cfg.max_pkt_size = params.maxPacketSize;

    // TURN socket creation
    PjsipCall(pj_turn_sock_create,
              &pimpl_->stunConfig, server.getFamily(), PJ_TURN_TP_TCP,
              &relay_cb, &turn_sock_cfg, &*this->pimpl_, &pimpl_->relay);

    // TURN allocation setup
    pj_turn_alloc_param turn_alloc_param;
    pj_turn_alloc_param_default(&turn_alloc_param);
    if (params.authorized_family != 0)
        turn_alloc_param.af = params.authorized_family; // RFC 6156!!!

    if (params.isPeerConnection)
        turn_alloc_param.peer_conn_type = PJ_TURN_TP_TCP; // RFC 6062!!!

    pj_stun_auth_cred cred;
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    pj_strset(&cred.data.static_cred.realm, (char*)pimpl_->settings.realm.c_str(), pimpl_->settings.realm.size());
    pj_strset(&cred.data.static_cred.username, (char*)pimpl_->settings.username.c_str(), pimpl_->settings.username.size());
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    pj_strset(&cred.data.static_cred.data, (char*)pimpl_->settings.password.c_str(), pimpl_->settings.password.size());

    pimpl_->relayAddr = pj_strdup3(pimpl_->pool, server.toString().c_str());

    // TURN connection/allocation
    JAMI_DBG() << "Connecting to TURN " << server.toString(true, true);
    PjsipCall(pj_turn_sock_alloc,
              pimpl_->relay, &pimpl_->relayAddr, server.getPort(),
              nullptr, &cred, &turn_alloc_param);
}

TurnTransport::~TurnTransport() = default;

void
TurnTransport::shutdown(const IpAddr& addr)
{
    MutexLock lk {pimpl_->apiMutex_};
    auto& channel = pimpl_->peerChannels_.at(addr);
    lk.unlock();
    channel.stop();
}

bool
TurnTransport::isInitiator() const
{
    return !pimpl_->settings.server;
}

void
TurnTransport::permitPeer(const IpAddr& addr)
{
    if (addr.isUnspecified())
        throw std::invalid_argument("invalid peer address");

    if (addr.getFamily() != pimpl_->peerRelayAddr.getFamily())
        throw std::invalid_argument("mismatching peer address family");

    sip_utils::register_thread();
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
    if (status != PJ_SUCCESS && status != PJ_EPENDING && status != PJ_EBUSY)
        throw sip_utils::PjsipFailure(PJ_STATUS_TO_OS(status));

    return status != PJ_EBUSY;
}

bool
TurnTransport::sendto(const IpAddr& peer, const std::vector<char>& buffer)
{
    return sendto(peer, &buffer[0], buffer.size());
}

std::size_t
TurnTransport::recvfrom(const IpAddr& peer, char* buffer, std::size_t size)
{
    MutexLock lk {pimpl_->apiMutex_};
    auto& channel = pimpl_->peerChannels_.at(peer);
    lk.unlock();
    return channel.read(buffer, size);
}

void
TurnTransport::recvfrom(const IpAddr& peer, std::vector<char>& result)
{
    auto res = recvfrom(peer, result.data(), result.size());
    result.resize(res);
}

std::vector<IpAddr>
TurnTransport::peerAddresses() const
{
    MutexLock lk {pimpl_->apiMutex_};
    return map_utils::extractKeys(pimpl_->peerChannels_);
}

int
TurnTransport::waitForData(const IpAddr& peer, unsigned ms_timeout, std::error_code& ec) const
{
    (void)ec; ///< \todo handle errors
    MutexLock lk {pimpl_->apiMutex_};
    auto& channel = pimpl_->peerChannels_.at(peer);
    lk.unlock();
    return channel.wait(std::chrono::milliseconds(ms_timeout));
}

//==============================================================================

ConnectedTurnTransport::ConnectedTurnTransport(TurnTransport& turn, const IpAddr& peer)
    : turn_ {turn}
    , peer_ {peer}
{}

void
ConnectedTurnTransport::shutdown()
{
    turn_.shutdown(peer_);
}

int
ConnectedTurnTransport::waitForData(unsigned ms_timeout, std::error_code& ec) const
{
    return turn_.waitForData(peer_, ms_timeout, ec);
}

std::size_t
ConnectedTurnTransport::write(const ValueType* buf, std::size_t size, std::error_code& ec)
{
    try {
        auto success = turn_.sendto(peer_, reinterpret_cast<const char*>(buf), size);
        if (!success) {
            // if !success, pj_turn_sock_sendto returned EBUSY
            // So, we should retry to send this later
            ec.assign(EAGAIN, std::generic_category());
            return 0;
        }
    } catch (const sip_utils::PjsipFailure& ex) {
        ec = ex.code();
        return 0;
    }

    ec.clear();
    return size;
}

std::size_t
ConnectedTurnTransport::read(ValueType* buf, std::size_t size, std::error_code& ec)
{
    if (size > 0) {
        try {
            size = turn_.recvfrom(peer_, reinterpret_cast<char*>(buf), size);
        } catch (const sip_utils::PjsipFailure& ex) {
            ec = ex.code();
            return 0;
        }

        if (size == 0) {
            ec = std::make_error_code(std::errc::broken_pipe);
            return 0;
        }
    }

    ec.clear();
    return size;
}

} // namespace jami
