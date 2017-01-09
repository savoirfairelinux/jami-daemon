/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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

#include "ice_transport.h"
#include "ice_socket.h"
#include "logger.h"
#include "sip/sip_utils.h"
#include "manager.h"
#include "upnp/upnp_control.h"

#include <pjlib.h>
#include <msgpack.hpp>

#include <utility>
#include <tuple>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <cerrno>

#define TRY(ret) do {      \
        if ((ret) != PJ_SUCCESS)                             \
            throw std::runtime_error(#ret " failed");      \
    } while (0)

namespace ring {

static constexpr unsigned STUN_MAX_PACKET_SIZE {8192};

// TODO: C++14 ? remove me and use std::min
template< class T >
static constexpr const T& min( const T& a, const T& b ) {
    return (b < a) ? b : a;
}

static void
register_thread()
{
    // We have to register the external thread so it could access the pjsip frameworks
    if (!pj_thread_is_registered()) {
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || defined RING_UWP
        static thread_local pj_thread_desc desc;
        static thread_local pj_thread_t *this_thread;
#else
        static __thread pj_thread_desc desc;
        static __thread pj_thread_t *this_thread;
#endif
        pj_thread_register(NULL, desc, &this_thread);
        RING_DBG("Registered thread %p (0x%X)", this_thread, pj_getpid());
    }
}

/**
 * Add stun/turn servers or default host as candidates
 */
static void
add_stun_server(pj_ice_strans_cfg& cfg, int af)
{
    if (cfg.stun_tp_cnt >= PJ_ICE_MAX_STUN)
        throw std::runtime_error("Too many STUN servers");
    auto& stun = cfg.stun_tp[cfg.stun_tp_cnt++];

    pj_ice_strans_stun_cfg_default(&stun);
    stun.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;
    stun.af = af;

    RING_DBG("[ice] added host stun server");
}

static void
add_stun_server(pj_pool_t& pool, pj_ice_strans_cfg& cfg, const StunServerInfo& info)
{
    if (cfg.stun_tp_cnt >= PJ_ICE_MAX_STUN)
        throw std::runtime_error("Too many STUN servers");

    IpAddr ip {info.uri};

    // Given URI cannot be DNS resolved or not IPv4 or IPv6?
    // This prevents a crash into PJSIP when ip.toString() is called.
    if (ip.getFamily() == AF_UNSPEC) {
        RING_WARN("[ice] STUN server '%s' not used, unresolvable address", info.uri.c_str());
        return;
    }

    auto& stun = cfg.stun_tp[cfg.stun_tp_cnt++];
    pj_ice_strans_stun_cfg_default(&stun);
    pj_strdup2_with_null(&pool, &stun.server, ip.toString().c_str());
    stun.af = ip.getFamily();
    stun.port = PJ_STUN_PORT;
    stun.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;

    RING_DBG("[ice] added stun server '%s', port %d", pj_strbuf(&stun.server), stun.port);
}

static void
add_turn_server(pj_pool_t& pool, pj_ice_strans_cfg& cfg, const TurnServerInfo& info)
{
    if (cfg.turn_tp_cnt >= PJ_ICE_MAX_TURN)
        throw std::runtime_error("Too many TURN servers");

    IpAddr ip {info.uri};

    // Same comment as add_stun_server()
    if (ip.getFamily() == AF_UNSPEC) {
        RING_WARN("[ice] TURN server '%s' not used, unresolvable address", info.uri.c_str());
        return;
    }

    auto& turn = cfg.turn_tp[cfg.turn_tp_cnt++];
    pj_ice_strans_turn_cfg_default(&turn);
    pj_strdup2_with_null(&pool, &turn.server, ip.toString().c_str());
    turn.af = ip.getFamily();
    turn.port = PJ_STUN_PORT;
    turn.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;

    // Authorization (only static plain password supported yet)
    if (not info.password.empty()) {
        turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
        turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
        pj_cstr(&turn.auth_cred.data.static_cred.realm, info.realm.c_str());
        pj_cstr(&turn.auth_cred.data.static_cred.username, info.username.c_str());
        pj_cstr(&turn.auth_cred.data.static_cred.data, info.password.c_str());
    }

    RING_DBG("[ice] added turn server '%s', port %d", pj_strbuf(&turn.server), turn.port);
}

//##################################################################################################

IceTransport::Packet::Packet(void *pkt, pj_size_t size)
    : data(new char[size]), datalen(size)
{
    std::copy_n(reinterpret_cast<char*>(pkt), size, data.get());
}

void
IceTransport::cb_on_rx_data(pj_ice_strans* ice_st,
                            unsigned comp_id,
                            void *pkt, pj_size_t size,
                            const pj_sockaddr_t* /*src_addr*/,
                            unsigned /*src_addr_len*/)
{
    if (auto tr = static_cast<IceTransport*>(pj_ice_strans_get_user_data(ice_st)))
        tr->onReceiveData(comp_id, pkt, size);
    else
        RING_WARN("null IceTransport");
}

void
IceTransport::cb_on_ice_complete(pj_ice_strans* ice_st,
                                 pj_ice_strans_op op,
                                 pj_status_t status)
{
    if (auto tr = static_cast<IceTransport*>(pj_ice_strans_get_user_data(ice_st)))
        tr->onComplete(ice_st, op, status);
    else
        RING_WARN("null IceTransport");
}


IceTransport::IceTransport(const char* name, int component_count, bool master,
                           const IceTransportOptions& options)
    : pool_(nullptr, pj_pool_release)
    , on_initdone_cb_(options.onInitDone)
    , on_negodone_cb_(options.onNegoDone)
    , component_count_(component_count)
    , compIO_(component_count)
    , initiatorSession_(master)
    , thread_()
{
    if (options.upnpEnable)
        upnp_.reset(new upnp::Controller());

    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    config_ = iceTransportFactory.getIceCfg(); // config copy

    pool_.reset(pj_pool_create(iceTransportFactory.getPoolFactory(),
                               "IceTransport.pool", 512, 512, NULL));
    if (not pool_)
        throw std::runtime_error("pj_pool_create() failed");

    pj_ice_strans_cb icecb;
    pj_bzero(&icecb, sizeof(icecb));
    icecb.on_rx_data = cb_on_rx_data;
    icecb.on_ice_complete = cb_on_ice_complete;

    // Add STUN servers
    for (auto& server : options.stunServers)
        add_stun_server(*pool_, config_, server);

    // Add TURN servers
    for (auto& server : options.turnServers)
        add_turn_server(*pool_, config_, server);

    static constexpr auto IOQUEUE_MAX_HANDLES = min(PJ_IOQUEUE_MAX_HANDLES, 64);
    TRY( pj_timer_heap_create(pool_.get(), 100, &config_.stun_cfg.timer_heap) );
    TRY( pj_ioqueue_create(pool_.get(), IOQUEUE_MAX_HANDLES, &config_.stun_cfg.ioqueue) );

    pj_ice_strans* icest = nullptr;
    pj_status_t status = pj_ice_strans_create(name, &config_, component_count,
                                              this, &icecb, &icest);

    if (status != PJ_SUCCESS || icest == nullptr) {
        throw std::runtime_error("pj_ice_strans_create() failed");
    }

    // Must be created after any potential failure
    thread_ = std::thread([this]{
            register_thread();
            while (not threadTerminateFlags_) {
                handleEvents(500); // limit polling to 500ms
            }
        });
}

IceTransport::~IceTransport()
{
    register_thread();

    threadTerminateFlags_ = true;
    if (thread_.joinable())
        thread_.join();

    icest_.reset(); // must be done before ioqueue/timer destruction

    if (config_.stun_cfg.ioqueue)
        pj_ioqueue_destroy(config_.stun_cfg.ioqueue);

    if (config_.stun_cfg.timer_heap)
        pj_timer_heap_destroy(config_.stun_cfg.timer_heap);
}

void
IceTransport::handleEvents(unsigned max_msec)
{
    // By tests, never seen more than two events per 500ms
    static constexpr auto MAX_NET_EVENTS = 2;

    pj_time_val max_timeout = {0, 0};
    pj_time_val timeout = {0, 0};
    unsigned net_event_count = 0;

    max_timeout.msec = max_msec;

    timeout.sec = timeout.msec = 0;
    pj_timer_heap_poll(config_.stun_cfg.timer_heap, &timeout);

    // timeout limitation
    if (timeout.msec >= 1000)
        timeout.msec = 999;
    if (PJ_TIME_VAL_GT(timeout, max_timeout))
        timeout = max_timeout;

    do {
        auto n_events = pj_ioqueue_poll(config_.stun_cfg.ioqueue, &timeout);

        // timeout
        if (not n_events)
            return;

        // error
        if (n_events < 0) {
            const auto err = pj_get_os_error();
            // Kept as debug as some errors are "normal" in regular context
            last_errmsg_ = sip_utils::sip_strerror(err);
            RING_DBG("IceIOQueue: error %d - %s", err, last_errmsg_.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(PJ_TIME_VAL_MSEC(timeout)));
            return;
        }

        net_event_count += n_events;
        timeout.sec = timeout.msec = 0;
    } while (net_event_count < MAX_NET_EVENTS);
}

void
IceTransport::onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                         pj_status_t status)
{
    const char *opname =
        op == PJ_ICE_STRANS_OP_INIT ? "initialization" :
        op == PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op";

    const bool done = status == PJ_SUCCESS;
    if (done) {
        RING_DBG("ICE %s success", opname);
    }
    else {
        last_errmsg_ = sip_utils::sip_strerror(status);
        RING_ERR("ICE %s failed: %s", opname, last_errmsg_.c_str());
    }

    {
        std::lock_guard<std::mutex> lk(iceMutex_);
        if (!icest_.get())
            icest_.reset(ice_st);
    }

    if (done and op == PJ_ICE_STRANS_OP_INIT) {
        if (initiatorSession_)
            setInitiatorSession();
        else
            setSlaveSession();
        selectUPnPIceCandidates();
    }

    if (op == PJ_ICE_STRANS_OP_INIT and on_initdone_cb_)
        on_initdone_cb_(*this, done);
    else if (op == PJ_ICE_STRANS_OP_NEGOTIATION and on_negodone_cb_)
        on_negodone_cb_(*this, done);

    // Unlock waitForXXX APIs
    iceCV_.notify_all();
}

void
IceTransport::getUFragPwd()
{
    pj_str_t local_ufrag, local_pwd;
    pj_ice_strans_get_ufrag_pwd(icest_.get(), &local_ufrag, &local_pwd, NULL, NULL);
    local_ufrag_.assign(local_ufrag.ptr, local_ufrag.slen);
    local_pwd_.assign(local_pwd.ptr, local_pwd.slen);
}

std::string
IceTransport::getLastErrMsg() const
{
    return last_errmsg_;
}

void
IceTransport::getDefaultCanditates()
{
    for (unsigned i=0; i < component_count_; ++i)
        pj_ice_strans_get_def_cand(icest_.get(), i+1, &cand_[i]);
}

bool
IceTransport::createIceSession(pj_ice_sess_role role)
{
    if (pj_ice_strans_init_ice(icest_.get(), role, nullptr, nullptr) != PJ_SUCCESS) {
        RING_ERR("pj_ice_strans_init_ice() failed");
        return false;
    }

    // Fetch some information on local configuration
    getUFragPwd();
    getDefaultCanditates();
    RING_DBG("ICE [local] ufrag=%s, pwd=%s", local_ufrag_.c_str(), local_pwd_.c_str());
    return true;
}

bool
IceTransport::setInitiatorSession()
{
    RING_DBG("ICE as master");
    initiatorSession_ = true;
    if (isInitialized()) {
        auto status = pj_ice_strans_change_role(icest_.get(), PJ_ICE_SESS_ROLE_CONTROLLING);
        if (status != PJ_SUCCESS) {
            last_errmsg_ = sip_utils::sip_strerror(status);
            RING_ERR("ICE role change failed: %s", last_errmsg_.c_str());
            return false;
        }
        return true;
    }
    return createIceSession(PJ_ICE_SESS_ROLE_CONTROLLING);
}

bool
IceTransport::setSlaveSession()
{
    RING_DBG("ICE as slave");
    initiatorSession_ = false;
    if (isInitialized()) {
        auto status = pj_ice_strans_change_role(icest_.get(), PJ_ICE_SESS_ROLE_CONTROLLED);
        if (status != PJ_SUCCESS) {
            last_errmsg_ = sip_utils::sip_strerror(status);
            RING_ERR("ICE role change failed: %s", last_errmsg_.c_str());
            return false;
        }
        return true;
    }
    return createIceSession(PJ_ICE_SESS_ROLE_CONTROLLED);
}

bool
IceTransport::isInitiator() const
{
    if (isInitialized())
        return pj_ice_strans_get_role(icest_.get()) == PJ_ICE_SESS_ROLE_CONTROLLING;
    return initiatorSession_;
}

bool
IceTransport::start(const Attribute& rem_attrs,
                    const std::vector<IceCandidate>& rem_candidates)
{
    if (not isInitialized()) {
        RING_ERR("ICE: not initialized transport");
        return false;
    }

    // pj_ice_strans_start_ice crashes if remote candidates array is empty
    if (rem_candidates.empty()) {
        RING_ERR("ICE start failed: no remote candidates");
        return false;
    }

    pj_str_t ufrag, pwd;
    RING_DBG("ICE negotiation starting (%zu remote candidates)", rem_candidates.size());
    auto status = pj_ice_strans_start_ice(icest_.get(),
                                          pj_cstr(&ufrag, rem_attrs.ufrag.c_str()),
                                          pj_cstr(&pwd, rem_attrs.pwd.c_str()),
                                          rem_candidates.size(),
                                          rem_candidates.data());
    if (status != PJ_SUCCESS) {
        last_errmsg_ = sip_utils::sip_strerror(status);
        RING_ERR("ICE start failed: %s", last_errmsg_.c_str());
        return false;
    }
    return true;
}

bool
IceTransport::start(const std::vector<uint8_t>& rem_data)
{
    std::string rem_ufrag;
    std::string rem_pwd;
    std::vector<IceCandidate> rem_candidates;

    auto data = reinterpret_cast<const char*>(rem_data.data());
    auto size = rem_data.size();

    try {
        std::size_t offset = 0;
        auto result = msgpack::unpack(data, size, offset);
        auto version = result.get().as<uint8_t>();
        RING_DBG("[ice:%p] rx msg v%u", this, version);
        if (version == 1) {
            result = msgpack::unpack(data, size, offset);
            std::tie(rem_ufrag, rem_pwd) = result.get().as<std::pair<std::string, std::string>>();
            result = msgpack::unpack(data, size, offset);
            auto comp_cnt = result.get().as<uint8_t>();
            while (comp_cnt-- > 0) {
                result = msgpack::unpack(data, size, offset);
                IceCandidate cand;
                for (const auto& line : result.get().as<std::vector<std::string>>()) {
                    if (getCandidateFromSDP(line, cand))
                        rem_candidates.emplace_back(std::move(cand));
                }
            }
        } else {
            RING_ERR("[ice:%p] invalid msg version", this);
            return false;
        }
    } catch (const msgpack::unpack_error& e) {
        RING_ERR("[ice:%p] remote msg unpack error: %s", this, e.what());
        return false;
    }

    if (rem_pwd.empty() or rem_pwd.empty() or rem_candidates.empty()) {
        RING_ERR("[ice:%p] invalid remote attributes", this);
        return false;
    }

    return start({rem_ufrag, rem_pwd}, rem_candidates);
}

bool
IceTransport::stop()
{
    if (isStarted()) {
        auto status = pj_ice_strans_stop_ice(icest_.get());
        if (status != PJ_SUCCESS) {
            last_errmsg_ = sip_utils::sip_strerror(status);
            RING_ERR("ICE stop failed: %s", last_errmsg_.c_str());
            return false;
        }
    }
    return true;
}

bool
IceTransport::_isInitialized() const
{
    if (auto icest = icest_.get()) {
        auto state = pj_ice_strans_get_state(icest);
        return state >= PJ_ICE_STRANS_STATE_SESS_READY and state != PJ_ICE_STRANS_STATE_FAILED;
    }
    return false;
}

bool
IceTransport::_isStarted() const
{
    if (auto icest = icest_.get()) {
        auto state = pj_ice_strans_get_state(icest);
        return state >= PJ_ICE_STRANS_STATE_NEGO and state != PJ_ICE_STRANS_STATE_FAILED;
    }
    return false;
}

bool
IceTransport::_isRunning() const
{
    if (auto icest = icest_.get()) {
        auto state = pj_ice_strans_get_state(icest);
        return state >= PJ_ICE_STRANS_STATE_RUNNING and state != PJ_ICE_STRANS_STATE_FAILED;
    }
    return false;
}

bool
IceTransport::_isFailed() const
{
    if (auto icest = icest_.get())
        return pj_ice_strans_get_state(icest) == PJ_ICE_STRANS_STATE_FAILED;
    return false;
}

IpAddr
IceTransport::getLocalAddress(unsigned comp_id) const
{
    // Return the local IP of negotiated connection pair
    if (isRunning()) {
        if (auto sess = pj_ice_strans_get_valid_pair(icest_.get(), comp_id+1)) {
            return sess->lcand->addr;
        }
        RING_WARN("Non-negotiated transport: try to return default local IP");
    }

    // Return the default IP (could be not nominated and valid after negotiation)
    if (isInitialized())
        return cand_[comp_id].addr;

    RING_ERR("bad call: non-initialized transport");
    return {};
}

IpAddr
IceTransport::getRemoteAddress(unsigned comp_id) const
{
    // Return the remote IP of negotiated connection pair
    if (isRunning()) {
        if (auto sess = pj_ice_strans_get_valid_pair(icest_.get(), comp_id+1))
            return sess->rcand->addr;
        RING_ERR("runtime error: negotiated transport without valid pair");
    }

    RING_ERR("bad call: non-negotiated transport");
    return {};
}

const IceTransport::Attribute
IceTransport::getLocalAttributes() const
{
    return {local_ufrag_, local_pwd_};
}

std::vector<std::string>
IceTransport::getLocalCandidates(unsigned comp_id) const
{
    std::vector<std::string> res;
    pj_ice_sess_cand cand[PJ_ARRAY_SIZE(cand_)];
    unsigned cand_cnt = PJ_ARRAY_SIZE(cand);

    if (pj_ice_strans_enum_cands(icest_.get(), comp_id+1, &cand_cnt, cand) != PJ_SUCCESS) {
        RING_ERR("pj_ice_strans_enum_cands() failed");
        return res;
    }

    for (unsigned i=0; i<cand_cnt; ++i) {
        std::ostringstream val;
        char ipaddr[PJ_INET6_ADDRSTRLEN];

        val << std::string(cand[i].foundation.ptr, cand[i].foundation.slen);
        val << " " << (unsigned)cand[i].comp_id << " UDP " << cand[i].prio;
        val << " " << pj_sockaddr_print(&cand[i].addr, ipaddr, sizeof(ipaddr), 0);
        val << " " << (unsigned)pj_sockaddr_get_port(&cand[i].addr);
        val << " typ " << pj_ice_get_cand_type_name(cand[i].type);

        res.push_back(val.str());
    }

    return res;
}

std::vector<IpAddr>
IceTransport::getLocalCandidatesAddr(unsigned comp_id) const
{
    std::vector<IpAddr> cand_addrs;
    pj_ice_sess_cand cand[PJ_ARRAY_SIZE(cand_)];
    unsigned cand_cnt = PJ_ARRAY_SIZE(cand);

    if (pj_ice_strans_enum_cands(icest_.get(), comp_id, &cand_cnt, cand) != PJ_SUCCESS) {
        RING_ERR("pj_ice_strans_enum_cands() failed");
        return cand_addrs;
    }

    for (unsigned i=0; i<cand_cnt; ++i)
        cand_addrs.push_back(cand[i].addr);

    return cand_addrs;
}

bool
IceTransport::registerPublicIP(unsigned compId, const IpAddr& publicIP)
{
    if (not isInitialized())
        return false;

    // Find the local candidate corresponding to local host,
    // then register a rflx candidate using given public address
    // and this local address as base. It's port is used for both address
    // even if on the public side it have strong probabilities to not exist.
    // But as this candidate is made after initialization, it's not used during
    // negotiation, only to exchanged candidates between peers.
    auto localIP = ip_utils::getLocalAddr(publicIP.getFamily());
    auto pubIP = publicIP;
    for (const auto& addr : getLocalCandidatesAddr(compId)) {
        auto port = addr.getPort();
        localIP.setPort(port);
        if (addr != localIP)
            continue;
        pubIP.setPort(port);
        addReflectiveCandidate(compId, addr, pubIP);
        return true;
    }
    return false;
}

void
IceTransport::addReflectiveCandidate(int comp_id, const IpAddr& base, const IpAddr& addr)
{
    // HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK
    // WARNING: following implementation is a HACK of PJNATH !!
    // ice_strans doesn't have any API that permit to inject ICE any kind of candidates.
    // So, the hack consists in accessing hidden ICE session using a patched PJPNATH
    // library with a new API exposing this session (pj_ice_strans_get_ice_sess).
    // Then call pj_ice_sess_add_cand() with a carfully forged candidate:
    // the transport_id field uses an index in ICE transport STUN servers array
    // corresponding to a STUN server with the same address familly.
    // This implies we hope they'll not be modification of transport_id meaning in future
    // and no conflics with the borrowed STUN config.
    // HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK

    // borrowed from pjproject/pjnath/ice_strans.c, modified to be C++11'ized.
    static auto CREATE_TP_ID = [](pj_uint8_t type, pj_uint8_t idx) {
        return (pj_uint8_t)((type << 6) | idx);
    };
    static constexpr int SRFLX_PREF = 65535;
    static constexpr int TP_STUN = 1;

    // find a compatible STUN host with same address familly, normally all system enabled
    // host addresses are represented, so we expect to always found this host
    int idx = -1;
    auto af = addr.getFamily();
    if (af == AF_UNSPEC) {
        RING_ERR("[ice] Unable to add reflective IP %s: unknown addess familly",
                 addr.toString().c_str());
        return;
    }

    for (unsigned i=0; i < config_.stun_tp_cnt; ++i) {
        if (config_.stun_tp[i].af == af) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        RING_ERR("[ice] Unable to add reflective IP %s: no suitable local STUN host found",
                 addr.toString().c_str());
        return;
    }

    pj_ice_sess_cand cand;

    cand.type = PJ_ICE_CAND_TYPE_SRFLX;
    cand.status = PJ_EPENDING; // not used
    cand.comp_id = comp_id;
    cand.transport_id = CREATE_TP_ID(TP_STUN, idx); // HACK!!
    cand.local_pref = SRFLX_PREF; // reflective
    /* cand.foundation = ? */
    /* cand.prio = calculated by ice session */
    /* make base and addr the same since we're not going through a server */
    pj_sockaddr_cp(&cand.base_addr, base.pjPtr());
    pj_sockaddr_cp(&cand.addr, addr.pjPtr());
    pj_sockaddr_cp(&cand.rel_addr, &cand.base_addr);
    pj_ice_calc_foundation(pool_.get(), &cand.foundation, cand.type, &cand.base_addr);

    auto ret = pj_ice_sess_add_cand(pj_ice_strans_get_ice_sess(icest_.get()),
        cand.comp_id,
        cand.transport_id,
        cand.type,
        cand.local_pref,
        &cand.foundation,
        &cand.addr,
        &cand.base_addr,
        &cand.rel_addr,
        pj_sockaddr_get_len(&cand.addr),
        NULL);

    if (ret != PJ_SUCCESS) {
        last_errmsg_ = sip_utils::sip_strerror(ret);
        RING_ERR("pj_ice_sess_add_cand failed with error %d: %s", ret,
                 last_errmsg_.c_str());
        RING_ERR("failed to add candidate for comp_id=%d : %s : %s", comp_id,
                 base.toString().c_str(), addr.toString().c_str());
    } else {
        RING_DBG("succeed to add candidate for comp_id=%d : %s : %s", comp_id,
                 base.toString().c_str(), addr.toString().c_str());
    }
}

void
IceTransport::selectUPnPIceCandidates()
{
    /* use upnp to open ports and add the proper candidates */
    if (upnp_) {
        /* for every component, get the candidate(s)
         * create a port mapping either with that port, or with an available port
         * add candidate with that port and public IP
         */
        if (auto publicIP = upnp_->getExternalIP()) {
            /* comp_id start at 1 */
            for (unsigned comp_id = 1; comp_id <= component_count_; ++comp_id) {
                RING_DBG("UPnP: Opening port(s) for ICE comp %d and adding candidate with public IP",
                         comp_id);
                auto candidates = getLocalCandidatesAddr(comp_id);
                for (IpAddr addr : candidates) {
                    auto localIP = upnp_->getLocalIP();
                    localIP.setPort(addr.getPort());
                    if (addr != localIP)
                        continue;
                    uint16_t port = addr.getPort();
                    uint16_t port_used;
                    if (upnp_->addAnyMapping(port, upnp::PortType::UDP, true, &port_used)) {
                        publicIP.setPort(port_used);
                        addReflectiveCandidate(comp_id, addr, publicIP);
                    } else
                        RING_WARN("UPnP: Could not create a port mapping for the ICE candide");
                }
            }
        } else {
            RING_WARN("UPnP: Could not determine public IP for ICE candidates");
        }
    }
}

std::vector<uint8_t>
IceTransport::packIceMsg() const
{
    static constexpr uint8_t ICE_MSG_VERSION = 1;

    if (not isInitialized())
        return {};

    std::stringstream ss;
    msgpack::pack(ss, ICE_MSG_VERSION);
    msgpack::pack(ss, std::make_pair(local_ufrag_, local_pwd_));
    msgpack::pack(ss, static_cast<uint8_t>(component_count_));
    for (unsigned i=0; i<component_count_; i++)
        msgpack::pack(ss, getLocalCandidates(i));

    auto str(ss.str());
    return std::vector<uint8_t>(str.begin(), str.end());
}

void
IceTransport::onReceiveData(unsigned comp_id, void *pkt, pj_size_t size)
{
    if (!comp_id or comp_id > component_count_) {
        RING_ERR("rx: invalid comp_id (%u)", comp_id);
        return;
    }
    if (!size)
        return;
    auto& io = compIO_[comp_id-1];
    std::lock_guard<std::mutex> lk(io.mutex);
    if (io.cb) {
        io.cb((uint8_t*)pkt, size);
    } else {
        io.queue.emplace_back(pkt, size);
        io.cv.notify_one();
    }
}

bool
IceTransport::getCandidateFromSDP(const std::string& line, IceCandidate& cand)
{
    char foundation[33], transport[13], ipaddr[81], type[33];
    pj_str_t tmpaddr;
    int af, comp_id, prio, port;
    int cnt = sscanf(line.c_str(), "%32s %d %12s %d %80s %d typ %32s",
                     foundation,
                     &comp_id,
                     transport,
                     &prio,
                     ipaddr,
                     &port,
                     type);

    if (cnt != 7) {
        RING_WARN("ICE: invalid remote candidate line");
        return false;
    }

    pj_bzero(&cand, sizeof(IceCandidate));

    if (strcmp(type, "host")==0)
        cand.type = PJ_ICE_CAND_TYPE_HOST;
    else if (strcmp(type, "srflx")==0)
        cand.type = PJ_ICE_CAND_TYPE_SRFLX;
    else if (strcmp(type, "relay")==0)
        cand.type = PJ_ICE_CAND_TYPE_RELAYED;
    else {
        RING_WARN("ICE: invalid remote candidate type '%s'", type);
        return false;
    }

    cand.comp_id = (pj_uint8_t)comp_id;
    cand.prio = prio;

    if (strchr(ipaddr, ':'))
        af = pj_AF_INET6();
    else
        af = pj_AF_INET();

    tmpaddr = pj_str(ipaddr);
    pj_sockaddr_init(af, &cand.addr, NULL, 0);
    auto status = pj_sockaddr_set_str_addr(af, &cand.addr, &tmpaddr);
    if (status != PJ_SUCCESS) {
        RING_ERR("ICE: invalid remote IP address '%s'", ipaddr);
        return false;
    }

    pj_sockaddr_set_port(&cand.addr, (pj_uint16_t)port);
    pj_strdup2(pool_.get(), &cand.foundation, foundation);

    return true;
}

ssize_t
IceTransport::recv(int comp_id, unsigned char* buf, size_t len)
{
    register_thread();
    auto& io = compIO_[comp_id];
    std::lock_guard<std::mutex> lk(io.mutex);

    if (io.queue.empty())
        return 0;

    auto& packet = io.queue.front();
    const auto count = std::min(len, packet.datalen);
    std::copy_n(packet.data.get(), count, buf);
    io.queue.pop_front();

    return count;
}

void
IceTransport::setOnRecv(unsigned comp_id, IceRecvCb cb)
{
    auto& io = compIO_[comp_id];
    std::lock_guard<std::mutex> lk(io.mutex);
    io.cb = cb;

    if (cb) {
        // Flush existing queue using the callback
        for (const auto& packet : io.queue)
            io.cb((uint8_t*)packet.data.get(), packet.datalen);
        io.queue.clear();
    }
}

ssize_t
IceTransport::send(int comp_id, const unsigned char* buf, size_t len)
{
    register_thread();
    auto remote = getRemoteAddress(comp_id);
    if (!remote) {
        RING_ERR("Can't find remote address for component %d", comp_id);
        errno = EINVAL;
        return -1;
    }
    auto status = pj_ice_strans_sendto(icest_.get(), comp_id+1, buf, len, remote.pjPtr(), remote.getLength());
    if (status != PJ_SUCCESS) {
        if (status == PJ_EBUSY) {
            errno = EAGAIN;
        } else {
            last_errmsg_ = sip_utils::sip_strerror(status);
            RING_ERR("ice send failed: %s", last_errmsg_.c_str());
            errno = EIO;
        }
        return -1;
    }

    return len;
}

ssize_t
IceTransport::getNextPacketSize(int comp_id)
{
    auto& io = compIO_[comp_id];
    std::lock_guard<std::mutex> lk(io.mutex);
    if (io.queue.empty()) {
        return 0;
    }
    return io.queue.front().datalen;
}

int
IceTransport::waitForInitialization(unsigned timeout)
{
    std::unique_lock<std::mutex> lk(iceMutex_);
    if (!iceCV_.wait_for(lk, std::chrono::seconds(timeout),
                         [this]{ return _isInitialized() or _isFailed(); })) {
        RING_WARN("waitForInitialization: timeout");
        return -1;
    }
    return not _isFailed();
}

int
IceTransport::waitForNegotiation(unsigned timeout)
{
    std::unique_lock<std::mutex> lk(iceMutex_);
    if (!iceCV_.wait_for(lk, std::chrono::seconds(timeout),
                         [this]{ return _isRunning() or _isFailed(); })) {
        RING_WARN("waitForIceNegotiation: timeout");
        return -1;
    }
    return not _isFailed();
}

ssize_t
IceTransport::waitForData(int comp_id, unsigned int timeout)
{
    auto& io = compIO_[comp_id];
    std::unique_lock<std::mutex> lk(io.mutex);
    if (!io.cv.wait_for(lk, std::chrono::milliseconds(timeout),
                        [this, &io]{ return !io.queue.empty() or !isRunning(); })) {
        return 0;
    }
    if (!isRunning())
        return -1; // acknowledged as an error
    return io.queue.front().datalen;
}

//##################################################################################################

IceTransportFactory::IceTransportFactory()
    : cp_()
    , pool_(nullptr, pj_pool_release)
    , ice_cfg_()
{
    pj_caching_pool_init(&cp_, NULL, 0);
    pool_.reset(pj_pool_create(&cp_.factory, "IceTransportFactory.pool",
                               512, 512, NULL));
    if (not pool_)
        throw std::runtime_error("pj_pool_create() failed");

    pj_ice_strans_cfg_default(&ice_cfg_);
    ice_cfg_.stun_cfg.pf = &cp_.factory;

    // v2.4.5 of PJNATH has a default of 100ms but RFC 5389 since version 14 requires
    // a minimum of 500ms on fixed-line links. Our usual case is wireless links.
    // This solves too long ICE exchange by DHT.
    // Using 500ms with default PJ_STUN_MAX_TRANSMIT_COUNT (7) gives around 33s before timeout.
    ice_cfg_.stun_cfg.rto_msec = 500;

    // Add local hosts (IPv4, IPv6) as stun candidates
    add_stun_server(ice_cfg_, pj_AF_INET6());
    add_stun_server(ice_cfg_, pj_AF_INET());

    ice_cfg_.opt.aggressive = PJ_FALSE;
}

IceTransportFactory::~IceTransportFactory()
{
    pool_.reset();
    pj_caching_pool_destroy(&cp_);
}

std::shared_ptr<IceTransport>
IceTransportFactory::createTransport(const char* name, int component_count,
                                     bool master,
                                     const IceTransportOptions& options)
{
    try {
        return std::make_shared<IceTransport>(name, component_count, master, options);
    } catch(const std::exception& e) {
        RING_ERR("%s",e.what());
        return nullptr;
    }
}

void
IceSocket::close()
{
    ice_transport_.reset();
}

ssize_t
IceSocket::recv(unsigned char* buf, size_t len)
{
    if (!ice_transport_.get())
        return -1;
    return ice_transport_->recv(compId_, buf, len);
}

ssize_t
IceSocket::send(const unsigned char* buf, size_t len)
{
    if (!ice_transport_.get())
        return -1;
    return ice_transport_->send(compId_, buf, len);
}

ssize_t
IceSocket::getNextPacketSize() const
{
    if (!ice_transport_.get())
        return -1;
    return ice_transport_->getNextPacketSize(compId_);
}

ssize_t
IceSocket::waitForData(unsigned int timeout)
{
    if (!ice_transport_.get())
        return -1;

    return ice_transport_->waitForData(compId_, timeout);
}

void
IceSocket::setOnRecv(IceRecvCb cb)
{
    if (!ice_transport_.get())
        return;
    return ice_transport_->setOnRecv(compId_, cb);
}

} // namespace ring
