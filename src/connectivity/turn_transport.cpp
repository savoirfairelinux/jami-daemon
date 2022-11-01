/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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

#include "connectivity/turn_transport.h"
#include "connectivity/sip_utils.h"

#include <thread> // TODO remove

#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>

namespace jami {

template<class Callable, class... Args>
inline void
PjsipCall(Callable& func, Args... args)
{
    auto status = func(args...);
    if (status != PJ_SUCCESS)
        throw sip_utils::PjsipFailure(status);
}
template<class Callable, class... Args>
inline auto
PjsipCallReturn(const Callable& func, Args... args) -> decltype(func(args...))
{
    auto res = func(args...);
    if (!res)
        throw sip_utils::PjsipFailure();
    return res;
}
//==============================================================================
class TurnTransport::Impl
{
public:
    Impl(std::function<void(bool)>&& cb) { cb_ = std::move(cb); }
    ~Impl();
    void onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state);
    void ioJob();
    std::mutex apiMutex_;
    TurnTransportParams settings;
    pj_caching_pool poolCache {};
    pj_pool_t* pool {nullptr};
    pj_stun_config stunConfig {};
    pj_turn_sock* relay {nullptr};
    pj_str_t relayAddr {};
    IpAddr peerRelayAddr; // address where peers should connect to
    IpAddr mappedAddr;
    std::atomic_bool ioJobQuit {false};
    std::thread ioWorker;
    std::function<void(bool)> cb_;
};
TurnTransport::Impl::~Impl()
{
    // TODO move + cleanup!
    // TODO check leaks
    ioJobQuit = true;
    if (ioWorker.joinable())
        ioWorker.join();
    pj_caching_pool_destroy(&poolCache);
}
void
TurnTransport::Impl::onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state)
{
    if (new_state == PJ_TURN_STATE_READY) {
        pj_turn_session_info info;
        pj_turn_sock_get_info(relay, &info);
        peerRelayAddr = IpAddr {info.relay_addr};
        mappedAddr = IpAddr {info.mapped_addr};
        JAMI_DEBUG("TURN server ready, peer relay address: {:s}",
                   peerRelayAddr.toString(true, true).c_str());
        cb_(true);
    } else if (old_state <= PJ_TURN_STATE_READY and new_state > PJ_TURN_STATE_READY) {
        JAMI_WARNING("TURN server disconnected ({:s})", pj_turn_state_name(new_state));
        cb_(false);
    }
}
void
TurnTransport::Impl::ioJob()
{
    while (!ioJobQuit.load()) {
        const pj_time_val delay = {0, 10};
        pj_ioqueue_poll(stunConfig.ioqueue, &delay);
        pj_timer_heap_poll(stunConfig.timer_heap, nullptr);
    }
}
//==============================================================================
TurnTransport::TurnTransport(const TurnTransportParams& params, std::function<void(bool)>&& cb)
    : pimpl_ {new Impl(std::move(cb))}
{
    auto server = params.server;
    if (!server.getPort())
        server.setPort(PJ_STUN_PORT);
    if (server.isUnspecified())
        throw std::invalid_argument("invalid turn server address");
    pimpl_->settings = params;
    // PJSIP memory pool
    pj_caching_pool_init(&pimpl_->poolCache, &pj_pool_factory_default_policy, 0);
    pimpl_->pool = PjsipCallReturn(pj_pool_create,
                                   &pimpl_->poolCache.factory,
                                   "RgTurnTr",
                                   512,
                                   512,
                                   nullptr);
    // STUN config
    pj_stun_config_init(&pimpl_->stunConfig, &pimpl_->poolCache.factory, 0, nullptr, nullptr);
    // create global timer heap
    PjsipCall(pj_timer_heap_create, pimpl_->pool, 1000, &pimpl_->stunConfig.timer_heap);
    // create global ioqueue
    PjsipCall(pj_ioqueue_create, pimpl_->pool, 16, &pimpl_->stunConfig.ioqueue);
    // run a thread to handles timer/ioqueue events
    pimpl_->ioWorker = std::thread([this] { pimpl_->ioJob(); });
    // TURN callbacks
    pj_turn_sock_cb relay_cb;
    pj_bzero(&relay_cb, sizeof(relay_cb));
    relay_cb.on_state =
        [](pj_turn_sock* relay, pj_turn_state_t old_state, pj_turn_state_t new_state) {
            auto pimpl = static_cast<Impl*>(pj_turn_sock_get_user_data(relay));
            pimpl->onTurnState(old_state, new_state);
        };
    // TURN socket config
    pj_turn_sock_cfg turn_sock_cfg;
    pj_turn_sock_cfg_default(&turn_sock_cfg);
    turn_sock_cfg.max_pkt_size = 4096;
    // TURN socket creation
    PjsipCall(pj_turn_sock_create,
              &pimpl_->stunConfig,
              server.getFamily(),
              PJ_TURN_TP_TCP,
              &relay_cb,
              &turn_sock_cfg,
              &*this->pimpl_,
              &pimpl_->relay);
    // TURN allocation setup
    pj_turn_alloc_param turn_alloc_param;
    pj_turn_alloc_param_default(&turn_alloc_param);
    turn_alloc_param.peer_conn_type = PJ_TURN_TP_TCP;
    pj_stun_auth_cred cred;
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    pj_cstr(&cred.data.static_cred.realm, pimpl_->settings.realm.c_str());
    pj_cstr(&cred.data.static_cred.username, pimpl_->settings.username.c_str());
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    pj_cstr(&cred.data.static_cred.data, pimpl_->settings.password.c_str());
    pimpl_->relayAddr = pj_strdup3(pimpl_->pool, server.toString().c_str());
    // TURN connection/allocation
    JAMI_DBG() << "Connecting to TURN " << server.toString(true, true);
    PjsipCall(pj_turn_sock_alloc,
              pimpl_->relay,
              &pimpl_->relayAddr,
              server.getPort(),
              nullptr,
              &cred,
              &turn_alloc_param);
}
TurnTransport::~TurnTransport() {}

void
TurnTransport::shutdown()
{
    pimpl_->ioJobQuit = true;
}

} // namespace jami
