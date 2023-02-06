/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "manager.h"

#include <atomic>
#include <thread>

#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>

#define TRY(ret) \
    do { \
        if ((ret) != PJ_SUCCESS) \
            throw std::runtime_error(#ret " failed"); \
    } while (0)

namespace jami {

class TurnLock
{
    pj_grp_lock_t* lk_;

public:
    TurnLock(pj_turn_sock* sock)
        : lk_(pj_turn_sock_get_grp_lock(sock))
    {
        lock();
    }

    ~TurnLock() { unlock(); }

    void lock() { pj_grp_lock_add_ref(lk_); }

    void unlock() { pj_grp_lock_dec_ref(lk_); }
};

class TurnTransport::Impl
{
public:
    Impl(std::function<void(bool)>&& cb) { cb_ = std::move(cb); }
    ~Impl();

    /**
     * Detect new TURN state
     */
    void onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state);

    /**
     * Pool events from pjsip
     */
    void ioJob();

    void start()
    {
        ioWorker = std::thread([this] { ioJob(); });
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(shutdownMtx_);
        if (relay) {
            pj_turn_sock_destroy(relay);
            relay = nullptr;
        }
        turnLock.reset();
        if (ioWorker.joinable())
            ioWorker.join();
     }

    TurnTransportParams settings;

    pj_caching_pool poolCache {};
    pj_pool_t* pool {nullptr};
    pj_stun_config stunConfig {};
    pj_turn_sock* relay {nullptr};
    std::unique_ptr<TurnLock> turnLock;
    pj_str_t relayAddr {};
    IpAddr peerRelayAddr; // address where peers should connect to
    IpAddr mappedAddr;
    std::function<void(bool)> cb_;

    std::thread ioWorker;
    std::atomic_bool stopped_ {false};
    std::atomic_bool cbCalled_ {false};
    std::mutex shutdownMtx_;
};

TurnTransport::Impl::~Impl()
{
    shutdown();
    pj_caching_pool_destroy(&poolCache);
}
void
TurnTransport::Impl::onTurnState(pj_turn_state_t old_state, pj_turn_state_t new_state)
{
    if (new_state == PJ_TURN_STATE_DESTROYING) {
        stopped_ = true;
        return;
    }

    if (new_state == PJ_TURN_STATE_READY) {
        pj_turn_session_info info;
        pj_turn_sock_get_info(relay, &info);
        peerRelayAddr = IpAddr {info.relay_addr};
        mappedAddr = IpAddr {info.mapped_addr};
        JAMI_DEBUG("TURN server ready, peer relay address: {:s}",
                   peerRelayAddr.toString(true, true).c_str());
        cbCalled_ = true;
        cb_(true);
    } else if (old_state <= PJ_TURN_STATE_READY and new_state > PJ_TURN_STATE_READY and not cbCalled_) {
        JAMI_WARNING("TURN server disconnected ({:s})", pj_turn_state_name(new_state));
        cb_(false);
    }
}
void
TurnTransport::Impl::ioJob()
{
    const pj_time_val delay = {0, 10};
    while (!stopped_) {
        pj_ioqueue_poll(stunConfig.ioqueue, &delay);
        pj_timer_heap_poll(stunConfig.timer_heap, nullptr);
    }
}

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
    pimpl_->pool = pj_pool_create(&pimpl_->poolCache.factory, "TurnTransport", 512, 512, nullptr);
    if (not pimpl_->pool)
        throw std::runtime_error("pj_pool_create() failed");
    // STUN config
    pj_stun_config_init(&pimpl_->stunConfig, &pimpl_->poolCache.factory, 0, nullptr, nullptr);
    // create global timer heap
    TRY(pj_timer_heap_create(pimpl_->pool, 1000, &pimpl_->stunConfig.timer_heap));
    // create global ioqueue
    TRY(pj_ioqueue_create(pimpl_->pool, 16, &pimpl_->stunConfig.ioqueue));
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
    TRY(pj_turn_sock_create(&pimpl_->stunConfig,
                            server.getFamily(),
                            PJ_TURN_TP_TCP,
                            &relay_cb,
                            &turn_sock_cfg,
                            &*this->pimpl_,
                            &pimpl_->relay));
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
    JAMI_DEBUG("Connecting to TURN {:s}", server.toString(true, true));
    TRY(pj_turn_sock_alloc(pimpl_->relay,
                           &pimpl_->relayAddr,
                           server.getPort(),
                           nullptr,
                           &cred,
                           &turn_alloc_param));
    pimpl_->turnLock = std::make_unique<TurnLock>(pimpl_->relay);
    pimpl_->start();
}

TurnTransport::~TurnTransport() {}

void
TurnTransport::shutdown()
{
    pimpl_->shutdown();
}

} // namespace jami
