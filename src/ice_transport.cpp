/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "transport/peer_channel.h"

#include <pjlib.h>

#include <map>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <utility>
#include <tuple>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <cerrno>

#define TRY(ret) do {                                        \
        if ((ret) != PJ_SUCCESS)                             \
            throw std::runtime_error(#ret " failed");        \
    } while (0)

namespace jami {

static constexpr unsigned STUN_MAX_PACKET_SIZE {8192};
static constexpr uint16_t IPV6_HEADER_SIZE = 40; ///< Size in bytes of IPV6 packet header
static constexpr uint16_t IPV4_HEADER_SIZE = 20; ///< Size in bytes of IPV4 packet header
static constexpr int MAX_CANDIDATES {32};

//==============================================================================

using MutexGuard = std::lock_guard<std::mutex>;
using MutexLock = std::unique_lock<std::mutex>;

using namespace std::placeholders;

namespace
{

struct IceSTransDeleter
{
    void operator ()(pj_ice_strans* ptr) {
        pj_ice_strans_stop_ice(ptr);
        pj_ice_strans_destroy(ptr);
    }
};

} // namespace <anonymous>

//==============================================================================

class IceTransport::Impl
{
public:
    Impl(const char* name, int component_count, bool master, const IceTransportOptions& options);
    ~Impl();

    void onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                    pj_status_t status);

    void onReceiveData(unsigned comp_id, void *pkt, pj_size_t size);

    /**
     * Set/change transport role as initiator.
     * Should be called before start method.
     */
    bool setInitiatorSession();

    /**
     * Set/change transport role as slave.
     * Should be called before start method.
     */
    bool setSlaveSession();

    bool createIceSession(pj_ice_sess_role role);

    void getUFragPwd();

    void getDefaultCanditates();

    // Non-mutex protected of public versions
    bool _isInitialized() const;
    bool _isStarted() const;
    bool _isRunning() const;
    bool _isFailed() const;

    IpAddr getLocalAddress(unsigned comp_id) const;
    IpAddr getRemoteAddress(unsigned comp_id) const;

    std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>> pool_;
    IceTransportCompleteCb on_initdone_cb_;
    IceTransportCompleteCb on_negodone_cb_;
    IceRecvInfo on_recv_cb_;
    std::unique_ptr<pj_ice_strans, IceSTransDeleter> icest_;
    unsigned component_count_;
    pj_ice_sess_cand cand_[MAX_CANDIDATES] {};
    std::string local_ufrag_;
    std::string local_pwd_;
    pj_sockaddr remoteAddr_;
    std::condition_variable iceCV_ {};
    mutable std::mutex iceMutex_ {};
    pj_ice_strans_cfg config_;
    std::string last_errmsg_;

    std::atomic_bool is_stopped_ {false};

    struct Packet {
      Packet(void *pkt, pj_size_t size)
          : data{reinterpret_cast<char *>(pkt), reinterpret_cast<char *>(pkt) + size} { }
        std::vector<char> data;
    };

    std::vector<PeerChannel> peerChannels_;

    struct ComponentIO {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<Packet> queue;
        IceRecvCb cb;
    };

    std::vector<ComponentIO> compIO_;

    std::atomic_bool initiatorSession_ {true};

    /**
     * Returns the IP of each candidate for a given component in the ICE session
     */
    struct LocalCandidate {
        IpAddr addr;
        pj_ice_cand_transport transport;
    };
    std::vector<LocalCandidate> getLocalICECandidates(unsigned comp_id) const;

    /**
     * Adds a reflective candidate to ICE session
     * Must be called before negotiation
     */
    void addReflectiveCandidate(int comp_id, const IpAddr &base,
                                const IpAddr &addr,
                                const pj_ice_cand_transport& transport);

    /**
     * Creates UPnP port mappings and adds ICE candidates based on those mappings
     */
    void selectUPnPIceCandidates();

    /**
     * Add port mapping callback function.
     */
    void onPortMappingAdd(uint16_t* port_used, bool success);

    std::unique_ptr<upnp::Controller> upnp_;

    std::mutex upnpMutex_;
    std::mutex upnpOnAddMutex_;
    std::condition_variable upnpCv_;
    unsigned int upnpIceCntr_ {0};

    bool onlyIPv4Private_ {true};

    // IO/Timer events are handled by following thread
    std::thread thread_;
    std::atomic_bool threadTerminateFlags_ {false};
    void handleEvents(unsigned max_msec);

    // Wait data on components
    std::vector<pj_ssize_t> lastReadLen_;
    std::condition_variable waitDataCv_ = {};
};

//==============================================================================

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
    stun.conn_type = cfg.stun.conn_type;

    JAMI_DBG("[ice] added host stun server");
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
        JAMI_WARN("[ice] STUN server '%s' not used, unresolvable address", info.uri.c_str());
        return;
    }

    auto& stun = cfg.stun_tp[cfg.stun_tp_cnt++];
    pj_ice_strans_stun_cfg_default(&stun);
    pj_strdup2_with_null(&pool, &stun.server, ip.toString().c_str());
    stun.af = ip.getFamily();
    stun.port = PJ_STUN_PORT;
    stun.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;
    stun.conn_type = cfg.stun.conn_type;

    JAMI_DBG("[ice] added stun server '%s', port %d", pj_strbuf(&stun.server), stun.port);
}

static void
add_turn_server(pj_pool_t& pool, pj_ice_strans_cfg& cfg, const TurnServerInfo& info)
{
    if (cfg.turn_tp_cnt >= PJ_ICE_MAX_TURN)
        throw std::runtime_error("Too many TURN servers");

    IpAddr ip {info.uri};

    // Same comment as add_stun_server()
    if (ip.getFamily() == AF_UNSPEC) {
        JAMI_WARN("[ice] TURN server '%s' not used, unresolvable address", info.uri.c_str());
        return;
    }

    auto& turn = cfg.turn_tp[cfg.turn_tp_cnt++];
    pj_ice_strans_turn_cfg_default(&turn);
    pj_strdup2_with_null(&pool, &turn.server, ip.toString().c_str());
    turn.af = ip.getFamily();
    turn.port = PJ_STUN_PORT;
    turn.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;
    turn.conn_type = cfg.turn.conn_type;

    // Authorization (only static plain password supported yet)
    if (not info.password.empty()) {
        turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
        turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
        pj_strset(&turn.auth_cred.data.static_cred.realm, (char*)info.realm.c_str(), info.realm.size());
        pj_strset(&turn.auth_cred.data.static_cred.username, (char*)info.username.c_str(), info.username.size());
        pj_strset(&turn.auth_cred.data.static_cred.data, (char*)info.password.c_str(), info.password.size());
    }

    JAMI_DBG("[ice] added turn server '%s', port %d", pj_strbuf(&turn.server), turn.port);
}

//==============================================================================

IceTransport::Impl::Impl(const char* name, int component_count, bool master,
                         const IceTransportOptions& options)
    : pool_(nullptr, [](pj_pool_t* pool) { sip_utils::register_thread(); pj_pool_release(pool); })
    , on_initdone_cb_(options.onInitDone)
    , on_negodone_cb_(options.onNegoDone)
    , on_recv_cb_(options.onRecvReady)
    , component_count_(component_count)
    , compIO_(component_count)
    , initiatorSession_(master)
    , thread_()
{
    if (options.upnpEnable)
        upnp_.reset(new upnp::Controller(false));

    auto &iceTransportFactory = Manager::instance().getIceTransportFactory();
    config_ = iceTransportFactory.getIceCfg(); // config copy
    if (options.tcpEnable) {
      config_.protocol = PJ_ICE_TP_TCP;
      config_.stun.conn_type = PJ_STUN_TP_TCP;
      config_.turn.conn_type = PJ_TURN_TP_TCP;
    } else {
      config_.protocol = PJ_ICE_TP_UDP;
      config_.stun.conn_type = PJ_STUN_TP_UDP;
      config_.turn.conn_type = PJ_TURN_TP_UDP;
    }

    if (options.aggressive) {
        config_.opt.aggressive = PJ_TRUE;
    } else {
        config_.opt.aggressive = PJ_FALSE;
    }

    peerChannels_.resize(component_count_ + 1);
    lastReadLen_.resize(component_count_);

    // Add local hosts (IPv4, IPv6) as stun candidates
    add_stun_server(config_, pj_AF_INET6());
    add_stun_server(config_, pj_AF_INET());

    sip_utils::register_thread();
    pool_.reset(pj_pool_create(iceTransportFactory.getPoolFactory(),
                               "IceTransport.pool", 512, 512, NULL));
    if (not pool_)
        throw std::runtime_error("pj_pool_create() failed");

    pj_ice_strans_cb icecb;
    pj_bzero(&icecb, sizeof(icecb));

    icecb.on_rx_data =                                                  \
        [] (pj_ice_strans* ice_st, unsigned comp_id, void *pkt, pj_size_t size,
            const pj_sockaddr_t* /*src_addr*/, unsigned /*src_addr_len*/) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st)))
            tr->onReceiveData(comp_id, pkt, size);
        else
            JAMI_WARN("null IceTransport");
    };

    icecb.on_ice_complete = \
        [] (pj_ice_strans* ice_st, pj_ice_strans_op op, pj_status_t status) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st)))
            tr->onComplete(ice_st, op, status);
        else
            JAMI_WARN("null IceTransport");
    };

    icecb.on_data_sent = [](pj_ice_strans* ice_st, unsigned comp_id,
                                pj_ssize_t size) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st))) {
          if (comp_id > 0 && comp_id - 1 < tr->lastReadLen_.size()) {
            tr->lastReadLen_[comp_id - 1] = size;
            tr->waitDataCv_.notify_all();
          }
        } else
            JAMI_WARN("null IceTransport");
    };

    // Add STUN servers
    for (auto& server : options.stunServers)
        add_stun_server(*pool_, config_, server);

    // Add TURN servers
    for (auto& server : options.turnServers)
        add_turn_server(*pool_, config_, server);

    static constexpr auto IOQUEUE_MAX_HANDLES = std::min(PJ_IOQUEUE_MAX_HANDLES, 64);
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
            sip_utils::register_thread();
            while (not threadTerminateFlags_) {
                handleEvents(500); // limit polling to 500ms
            }
        });
}

IceTransport::Impl::~Impl()
{
    sip_utils::register_thread();
    
    threadTerminateFlags_ = true;
    if (thread_.joinable())
        thread_.join();

    icest_.reset(); // must be done before ioqueue/timer destruction

    if (config_.stun_cfg.ioqueue)
        pj_ioqueue_destroy(config_.stun_cfg.ioqueue);

    if (config_.stun_cfg.timer_heap)
        pj_timer_heap_destroy(config_.stun_cfg.timer_heap);
    
}

bool
IceTransport::Impl::_isInitialized() const
{
    if (auto icest = icest_.get()) {
        auto state = pj_ice_strans_get_state(icest);
        return state >= PJ_ICE_STRANS_STATE_SESS_READY and state != PJ_ICE_STRANS_STATE_FAILED;
    }
    return false;
}

bool
IceTransport::Impl::_isStarted() const
{
    if (auto icest = icest_.get()) {
        auto state = pj_ice_strans_get_state(icest);
        return state >= PJ_ICE_STRANS_STATE_NEGO and state != PJ_ICE_STRANS_STATE_FAILED;
    }
    return false;
}

bool
IceTransport::Impl::_isRunning() const
{
    if (auto icest = icest_.get()) {
        auto state = pj_ice_strans_get_state(icest);
        return state >= PJ_ICE_STRANS_STATE_RUNNING and state != PJ_ICE_STRANS_STATE_FAILED;
    }
    return false;
}

bool
IceTransport::Impl::_isFailed() const
{
    if (auto icest = icest_.get())
        return pj_ice_strans_get_state(icest) == PJ_ICE_STRANS_STATE_FAILED;
    return false;
}

void
IceTransport::Impl::handleEvents(unsigned max_msec)
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
            JAMI_DBG("[ice:%p] ioqueue error %d: %s", this, err, last_errmsg_.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(PJ_TIME_VAL_MSEC(timeout)));
            return;
        }

        net_event_count += n_events;
        timeout.sec = timeout.msec = 0;
    } while (net_event_count < MAX_NET_EVENTS);
}

void
IceTransport::Impl::onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op, pj_status_t status)
{
    const char *opname =
        op == PJ_ICE_STRANS_OP_INIT ? "initialization" :
        op == PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op";

    const bool done = status == PJ_SUCCESS;
    if (done) {
        JAMI_DBG("[ice:%p] %s success", this, opname);
    }
    else {
        last_errmsg_ = sip_utils::sip_strerror(status);
        JAMI_ERR("[ice:%p] %s failed: %s", this, opname, last_errmsg_.c_str());
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
        
        std::thread([this] {
            while (upnpIceCntr_ > 0) {
				JAMI_DBG("[ice%p] waiting for upnp to open port(s)", this);
                std::unique_lock<std::mutex> lk(upnpMutex_);
                upnpCv_.wait_for(lk, std::chrono::seconds(2), [this]{ return upnpIceCntr_ == 0; });
				if (upnpIceCntr_ == 0) {
					if (component_count_ > 1)
						JAMI_DBG("[ice%p] upnp opened all %u ports successfully", this, component_count_);
					else
						JAMI_DBG("[ice%p] upnp opened port successfully", this);
				} else {
					if (component_count_ > 1)
						JAMI_DBG("[ice%p] upnp opened %u%u ports", this, upnpIceCntr_, component_count_);
					else
						JAMI_DBG("[ice%p] upnp failed to open port", this);
				}
				break;
            }
        }).detach();
    }

    if (op == PJ_ICE_STRANS_OP_INIT and on_initdone_cb_)
        on_initdone_cb_(done);
    else if (op == PJ_ICE_STRANS_OP_NEGOTIATION) {
        if (done) {
            // Dump of connection pairs
            std::stringstream out;
            for (unsigned i=0; i < component_count_; ++i) {
                auto laddr = getLocalAddress(i);
                auto raddr = getRemoteAddress(i);
                if (laddr and raddr) {
                    out << " [" << i << "] "
                        << laddr.toString(true, true)
                        << " <-> "
                        << raddr.toString(true, true)
                        << '\n';
                } else {
                    out << " [" << i << "] disabled\n";
                }
            }
            JAMI_DBG("[ice:%p] connection pairs (local <-> remote):\n%s", this, out.str().c_str());
        }
        if (on_negodone_cb_)
            on_negodone_cb_(done);
    }

    // Unlock waitForXXX APIs
    iceCV_.notify_all();
}

bool
IceTransport::Impl::setInitiatorSession()
{
    JAMI_DBG("ICE as master");
    initiatorSession_ = true;
    if (_isInitialized()) {
        auto status = pj_ice_strans_change_role(icest_.get(), PJ_ICE_SESS_ROLE_CONTROLLING);
        if (status != PJ_SUCCESS) {
            last_errmsg_ = sip_utils::sip_strerror(status);
            JAMI_ERR("[ice:%p] role change failed: %s", this, last_errmsg_.c_str());
            return false;
        }
        return true;
    }
    return createIceSession(PJ_ICE_SESS_ROLE_CONTROLLING);
}

bool
IceTransport::Impl::setSlaveSession()
{
    JAMI_DBG("ICE as slave");
    initiatorSession_ = false;
    if (_isInitialized()) {
        auto status = pj_ice_strans_change_role(icest_.get(), PJ_ICE_SESS_ROLE_CONTROLLED);
        if (status != PJ_SUCCESS) {
            last_errmsg_ = sip_utils::sip_strerror(status);
            JAMI_ERR("[ice:%p] role change failed: %s", this, last_errmsg_.c_str());
            return false;
        }
        return true;
    }
    return createIceSession(PJ_ICE_SESS_ROLE_CONTROLLED);
}

IpAddr
IceTransport::Impl::getLocalAddress(unsigned comp_id) const
{
    // Return the local IP of negotiated connection pair
    if (_isRunning()) {
        if (auto sess = pj_ice_strans_get_valid_pair(icest_.get(), comp_id+1))
            return sess->lcand->addr;
        else
            return {}; // disabled component
    } else
        JAMI_WARN("[ice:%p] bad call: non-negotiated transport", this);

    // Return the default IP (could be not nominated and valid after negotiation)
    if (_isInitialized())
        return cand_[comp_id].addr;

    JAMI_ERR("[ice:%p] bad call: non-initialized transport", this);
    return {};
}

IpAddr
IceTransport::Impl::getRemoteAddress(unsigned comp_id) const
{
    // Return the remote IP of negotiated connection pair
    if (_isRunning()) {
        if (auto sess = pj_ice_strans_get_valid_pair(icest_.get(), comp_id+1))
            return sess->rcand->addr;
        else
            return {}; // disabled component
    } else
        JAMI_WARN("[ice:%p] bad call: non-negotiated transport", this);

    JAMI_ERR("[ice:%p] bad call: non-negotiated transport", this);
    return {};
}

void
IceTransport::Impl::getUFragPwd()
{
    pj_str_t local_ufrag, local_pwd;
    pj_ice_strans_get_ufrag_pwd(icest_.get(), &local_ufrag, &local_pwd, nullptr, nullptr);
    local_ufrag_.assign(local_ufrag.ptr, local_ufrag.slen);
    local_pwd_.assign(local_pwd.ptr, local_pwd.slen);
}

void
IceTransport::Impl::getDefaultCanditates()
{
    for (unsigned i=0; i < component_count_; ++i)
        pj_ice_strans_get_def_cand(icest_.get(), i+1, &cand_[i]);
}

bool
IceTransport::Impl::createIceSession(pj_ice_sess_role role)
{
    if (pj_ice_strans_init_ice(icest_.get(), role, nullptr, nullptr) != PJ_SUCCESS) {
        JAMI_ERR("[ice:%p] pj_ice_strans_init_ice() failed", this);
        return false;
    }

    // Fetch some information on local configuration
    getUFragPwd();
    getDefaultCanditates();
    JAMI_DBG("[ice:%p] (local) ufrag=%s, pwd=%s", this, local_ufrag_.c_str(), local_pwd_.c_str());
    return true;
}

std::vector<IceTransport::Impl::LocalCandidate>
IceTransport::Impl::getLocalICECandidates(unsigned comp_id) const
{
    std::vector<LocalCandidate> cand_addrs;
    pj_ice_sess_cand cand[PJ_ARRAY_SIZE(cand_)];
    unsigned cand_cnt = PJ_ARRAY_SIZE(cand);

    if (pj_ice_strans_enum_cands(icest_.get(), comp_id, &cand_cnt, cand) != PJ_SUCCESS) {
        JAMI_ERR("[ice:%p] pj_ice_strans_enum_cands() failed", this);
        return cand_addrs;
    }

    for (unsigned i=0; i<cand_cnt; ++i) {
      cand_addrs.push_back({cand[i].addr, cand[i].transport});
    }

    return cand_addrs;
}

void IceTransport::Impl::addReflectiveCandidate(int comp_id, const IpAddr &base,
                                                const IpAddr &addr,
                                                const pj_ice_cand_transport& transport) {
    // HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK
    // WARNING: following implementation is a HACK of PJNATH !!
    // ice_strans doesn't have any API that permit to inject ICE any kind of
    // candidates. So, the hack consists in accessing hidden ICE session using a
    // patched PJPNATH library with a new API exposing this session
    // (pj_ice_strans_get_ice_sess). Then call pj_ice_sess_add_cand() with a
    // carfully forged candidate: the transport_id field uses an index in ICE
    // transport STUN servers array corresponding to a STUN server with the same
    // address familly. This implies we hope they'll not be modification of
    // transport_id meaning in future and no conflics with the borrowed STUN
    // config.
    // HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK
    sip_utils::register_thread();

    // borrowed from pjproject/pjnath/ice_strans.c, modified to be C++11'ized.
    static auto CREATE_TP_ID = [](pj_uint8_t type, pj_uint8_t idx) {
        return (pj_uint8_t)((type << 6) | idx);
    };
    static constexpr int SRFLX_PREF = 65535;
    static constexpr int TP_STUN = 1;

    // find a compatible STUN host with same address familly, normally all system
    // enabled host addresses are represented, so we expect to always found this
    // host
    int idx = -1;
    auto af = addr.getFamily();
    if (af == AF_UNSPEC) {
        JAMI_ERR("[ice:%p] Unable to add reflective IP %s: unknown addess familly",
                this, addr.toString().c_str());
        return;
    }

    for (unsigned i = 0; i < config_.stun_tp_cnt; ++i) {
        if (config_.stun_tp[i].af == af) {
        idx = i;
        break;
        }
    }
    if (idx < 0) {
        JAMI_ERR("[ice:%p] Unable to add reflective IP %s: no suitable local STUN "
                "host found",
                this, addr.toString().c_str());
        return;
    }

    pj_ice_sess_cand cand;

    cand.type = PJ_ICE_CAND_TYPE_SRFLX;
    cand.status = PJ_EPENDING; // not used
    cand.comp_id = comp_id;
    cand.transport_id = CREATE_TP_ID(TP_STUN, idx); // HACK!!
    cand.local_pref = SRFLX_PREF;                   // reflective
    cand.transport = transport;
    /* cand.foundation = ? */
    /* cand.prio = calculated by ice session */
    /* make base and addr the same since we're not going through a server */
    pj_sockaddr_cp(&cand.base_addr, base.pjPtr());
    pj_sockaddr_cp(&cand.addr, addr.pjPtr());
    pj_sockaddr_cp(&cand.rel_addr, &cand.base_addr);
    pj_ice_calc_foundation(pool_.get(), &cand.foundation, cand.type,
                            &cand.base_addr);

    auto ret = pj_ice_sess_add_cand(
        pj_ice_strans_get_ice_sess(icest_.get()), cand.comp_id, cand.transport_id,
        cand.type, cand.local_pref, &cand.foundation, &cand.addr, &cand.base_addr,
        &cand.rel_addr, pj_sockaddr_get_len(&cand.addr), NULL, cand.transport);

    if (ret != PJ_SUCCESS) {
        last_errmsg_ = sip_utils::sip_strerror(ret);
        JAMI_ERR("[ice:%p] pj_ice_sess_add_cand failed with error %d: %s", this,
                ret, last_errmsg_.c_str());
        JAMI_ERR("[ice:%p] failed to add candidate for comp_id=%d : %s : %s", this,
                comp_id, base.toString().c_str(), addr.toString().c_str());
    } else {
        JAMI_DBG("[ice:%p] succeed to add candidate for comp_id=%d : %s : %s", this,
                comp_id, base.toString().c_str(), addr.toString().c_str());
    }
}

void
IceTransport::Impl::selectUPnPIceCandidates()
{
    // For every component, get the candidate(s)
    // Create a port mapping either with that port, or with an available port
    // Add candidate with that port and public IP
    std::lock_guard<std::mutex> lk(upnpMutex_);
    
	if (upnp_) {

        auto publicIP = upnp_->getExternalIP();
        if (not publicIP) {
            JAMI_WARN("[ice:%p] Could not determine public IP for ICE candidates", this);
            upnpCv_.notify_all();
            return;
        }
        auto localIP = upnp_->getLocalIP();
        if (not localIP) {
            JAMI_WARN("[ice:%p] Could not determine local IP for ICE candidates", this);    
            upnpCv_.notify_all();
            return;
        }

        // Use local list to store needed ports with their corresponding port type.
        std::map<uint16_t, upnp::PortType> portMapRequestList;
        for (unsigned comp_id = 1; comp_id <= component_count_; ++comp_id) {
            auto candidates = getLocalICECandidates(comp_id);
            for (const auto& candidate : candidates) {
                if (candidate.transport == PJ_CAND_TCP_ACTIVE)
                    continue; // We don't need to map port 9.
                localIP.setPort(candidate.addr.getPort());
                if (candidate.addr != localIP)
                    continue;
                uint16_t port = candidate.addr.getPort();
                auto portType = candidate.transport == PJ_CAND_UDP ? 
                                upnp::PortType::UDP : upnp::PortType::TCP;
                // Make list of ports we want to open.
                portMapRequestList.emplace(std::move(port), std::move(portType));
            }
        }

        // Send request for every port in the list.
        upnpIceCntr_ = 0;
        for (auto const& map : portMapRequestList) {
            upnpIceCntr_++;
            JAMI_DBG("[ice:%p] UPnP: Trying to open port %d for ICE comp %d/%d and adding candidate with public IP",
                     this, map.first, upnpIceCntr_, component_count_);
            upnp_->addMapping(std::bind(&IceTransport::Impl::onPortMappingAdd, this, _1, _2),
                              map.first, map.second, true);
        }
    }
}

void
IceTransport::Impl::onPortMappingAdd(uint16_t* port_used, bool success)
{
    if (upnp_) {
        
        std::lock_guard<std::mutex> lk(upnpMutex_);
        upnpIceCntr_--;

        auto publicIP = upnp_->getExternalIP();
        if (not publicIP) {
            JAMI_WARN("[ice:%p] Could not determine public IP for ICE candidates", this);
            return;
        }
        auto localIP = upnp_->getLocalIP();
        if (not localIP) {
            JAMI_WARN("[ice:%p] Could not determine local IP for ICE candidates", this);    
            return;
        }

        if (success and port_used) {
            uint16_t openedPort = *port_used;
            for (unsigned comp_id = 1; comp_id <= component_count_; ++comp_id) {
                auto candidates = getLocalICECandidates(comp_id);
                for (const auto& candidate : candidates) {
                    if (candidate.transport == PJ_CAND_TCP_ACTIVE)
                        continue; // We don't need to map port 9.
                    if (candidate.addr.toString() != localIP.toString())
                        continue;
                    auto portType = candidate.transport == PJ_CAND_UDP ? 
                                    upnp::PortType::UDP : upnp::PortType::TCP;
                    if (openedPort == candidate.addr.getPort()) {
                        publicIP.setPort(openedPort);
                        addReflectiveCandidate(comp_id, candidate.addr, publicIP, candidate.transport);
                        if (upnpIceCntr_ < 1) {
                            upnpCv_.notify_all();
                        }
                        return;
                    }
                }
            }
        }
        if (upnpIceCntr_ < 1) {
            upnpCv_.notify_all();
        }
    }
}

void
IceTransport::Impl::onReceiveData(unsigned comp_id, void *pkt, pj_size_t size)
{
    if (!comp_id or comp_id > component_count_) {
        JAMI_ERR("rx: invalid comp_id (%u)", comp_id);
        return;
    }
    if (!size)
        return;
    auto& io = compIO_[comp_id-1];
    std::unique_lock<std::mutex> lk(io.mutex);
    if (on_recv_cb_) {
        on_recv_cb_();
    }

    if (io.cb) {
        io.cb((uint8_t*)pkt, size);
    } else {
        std::error_code ec;
        auto err = peerChannels_.at(comp_id-1).write((char*)pkt, size, ec);
        if (err < 0) {
            JAMI_ERR("[ice:%p] rx: channel is closed", this);
        }
    }
}

//==============================================================================

IceTransport::IceTransport(const char* name, int component_count, bool master,
                           const IceTransportOptions& options)
    : pimpl_ {std::make_unique<Impl>(name, component_count, master, options)}
{}

bool
IceTransport::isInitialized() const
{
    std::lock_guard<std::mutex> lk(pimpl_->iceMutex_);
    return pimpl_->_isInitialized();
}

bool
IceTransport::isStarted() const
{
    std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
    return pimpl_->_isStarted();
}

bool
IceTransport::isRunning() const
{
    std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
    return pimpl_->_isRunning();
}

bool
IceTransport::isStopped() const
{
    std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
    return pimpl_->is_stopped_;
}

bool
IceTransport::isFailed() const
{
    std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
    return pimpl_->_isFailed();
}

unsigned
IceTransport::getComponentCount() const
{
    return pimpl_->component_count_;
}

bool
IceTransport::setSlaveSession()
{
    return pimpl_->setSlaveSession();
}
bool
IceTransport::setInitiatorSession()
{
    return pimpl_->setInitiatorSession();
}

std::string IceTransport::getLastErrMsg() const {
  return pimpl_->last_errmsg_;
}

bool
IceTransport::isInitiator() const
{
    if (isInitialized()) {
      return pj_ice_strans_get_role(pimpl_->icest_.get()) ==
             PJ_ICE_SESS_ROLE_CONTROLLING;
    }
    return pimpl_->initiatorSession_;
}

bool
IceTransport::start(const Attribute& rem_attrs, const std::vector<IceCandidate>& rem_candidates)
{
    if (not isInitialized()) {
        JAMI_ERR("[ice:%p] not initialized transport", this);
        pimpl_->is_stopped_ = true;
        return false;
    }

    // pj_ice_strans_start_ice crashes if remote candidates array is empty
    if (rem_candidates.empty()) {
        JAMI_ERR("[ice:%p] start failed: no remote candidates", this);
        pimpl_->is_stopped_ = true;
        return false;
    }

    pj_str_t ufrag, pwd;
    JAMI_DBG("[ice:%p] negotiation starting (%zu remote candidates)", this, rem_candidates.size());
    auto status = pj_ice_strans_start_ice(pimpl_->icest_.get(),
                                          pj_strset(&ufrag, (char*)rem_attrs.ufrag.c_str(), rem_attrs.ufrag.size()),
                                          pj_strset(&pwd, (char*)rem_attrs.pwd.c_str(), rem_attrs.pwd.size()),
                                          rem_candidates.size(),
                                          rem_candidates.data());
    if (status != PJ_SUCCESS) {
        pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
        JAMI_ERR("[ice:%p] start failed: %s", this, pimpl_->last_errmsg_.c_str());
        pimpl_->is_stopped_ = true;
        return false;
    }
    return true;
}

bool
IceTransport::start(const SDP& sdp)
{
    if (not isInitialized()) {
        JAMI_ERR("[ice:%p] not initialized transport", this);
        pimpl_->is_stopped_ = true;
        return false;
    }

    JAMI_DBG("[ice:%p] negotiation starting (%zu remote candidates)", this, sdp.candidates.size());
    pj_str_t ufrag, pwd;

    std::vector<IceCandidate> rem_candidates;
    rem_candidates.reserve(sdp.candidates.size());
    IceCandidate cand;
    for (const auto &line : sdp.candidates) {
        if (getCandidateFromSDP(line, cand))
            rem_candidates.emplace_back(cand);
    }
    auto status = pj_ice_strans_start_ice(pimpl_->icest_.get(),
                                          pj_strset(&ufrag, (char*)sdp.ufrag.c_str(), sdp.ufrag.size()),
                                          pj_strset(&pwd, (char*)sdp.pwd.c_str(), sdp.pwd.size()),
                                          rem_candidates.size(),
                                          rem_candidates.data());
    if (status != PJ_SUCCESS) {
        pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
        JAMI_ERR("[ice:%p] start failed: %s", this, pimpl_->last_errmsg_.c_str());
        pimpl_->is_stopped_ = true;
        return false;
    }
    return true;
}

bool
IceTransport::stop()
{
    pimpl_->is_stopped_ = true;
    if (isStarted()) {
        auto status = pj_ice_strans_stop_ice(pimpl_->icest_.get());
        if (status != PJ_SUCCESS) {
            pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
            JAMI_ERR("ICE stop failed: %s", pimpl_->last_errmsg_.c_str());
            return false;
        }
    }
    return true;
}

void
IceTransport::cancelOperations()
{
    for (auto& c: pimpl_->peerChannels_) {
        c.stop();
    }
}

IpAddr
IceTransport::getLocalAddress(unsigned comp_id) const
{
    return pimpl_->getLocalAddress(comp_id);
}

IpAddr
IceTransport::getRemoteAddress(unsigned comp_id) const
{
    return pimpl_->getRemoteAddress(comp_id);
}

const IceTransport::Attribute
IceTransport::getLocalAttributes() const
{
    return {pimpl_->local_ufrag_, pimpl_->local_pwd_};
}

std::vector<std::string>
IceTransport::getLocalCandidates(unsigned comp_id) const
{
    std::vector<std::string> res;
    pj_ice_sess_cand cand[PJ_ARRAY_SIZE(pimpl_->cand_)];
    unsigned cand_cnt = PJ_ARRAY_SIZE(cand);

    if (pj_ice_strans_enum_cands(pimpl_->icest_.get(), comp_id+1, &cand_cnt, cand) != PJ_SUCCESS) {
        JAMI_ERR("[ice:%p] pj_ice_strans_enum_cands() failed", this);
        return res;
    }

    for (unsigned i=0; i<cand_cnt; ++i) {
        std::ostringstream val;
        char ipaddr[PJ_INET6_ADDRSTRLEN];

        /**   Section 4.5, RFC 6544 (https://tools.ietf.org/html/rfc6544)
         *    candidate-attribute   = "candidate" ":" foundation SP component-id
         * SP "TCP" SP priority SP connection-address SP port SP cand-type [SP
         * rel-addr] [SP rel-port] SP tcp-type-ext
         *                             *(SP extension-att-name SP
         *                                  extension-att-value)
         *
         *     tcp-type-ext          = "tcptype" SP tcp-type
         *     tcp-type              = "active" / "passive" / "so"
         */
        val << std::string(cand[i].foundation.ptr, cand[i].foundation.slen);
        val << " " << (unsigned)cand[i].comp_id;
        val << (cand[i].transport == PJ_CAND_UDP ? " UDP " : " TCP ");
        val << cand[i].prio;
        val << " " << pj_sockaddr_print(&cand[i].addr, ipaddr, sizeof(ipaddr), 0);
        val << " " << (unsigned)pj_sockaddr_get_port(&cand[i].addr);
        val << " typ " << pj_ice_get_cand_type_name(cand[i].type);

        if (cand[i].transport != PJ_CAND_UDP) {
            val << " tcptype";
            switch (cand[i].transport) {
            case PJ_CAND_TCP_ACTIVE:
                val << " active";
                break;
            case PJ_CAND_TCP_PASSIVE:
                val << " passive";
                break;
            case PJ_CAND_TCP_SO:
            default:
                val << " so";
                break;
            }
        }

        res.push_back(val.str());
    }

    return res;
}

bool
IceTransport::registerPublicIP(unsigned compId, const IpAddr& publicIP)
{
    if (not isInitialized()) {
        JAMI_ERR("[ice:%p] registerPublicIP() called on non initialized transport", this);
        return false;
    }

    // Find the local candidate corresponding to local host,
    // then register a rflx candidate using given public address
    // and this local address as base. It's port is used for both address
    // even if on the public side it have strong probabilities to not exist.
    // But as this candidate is made after initialization, it's not used during
    // negotiation, only to exchanged candidates between peers.
    auto localIP = ip_utils::getLocalAddr(publicIP.getFamily());
    auto pubIP = publicIP;
    for (const auto& cand : pimpl_->getLocalICECandidates(compId)) {
        auto port = cand.addr.getPort();
        localIP.setPort(port);
        if (cand.addr != localIP)
            continue;
        pubIP.setPort(port);
        pimpl_->addReflectiveCandidate(compId, cand.addr, pubIP, cand.transport);
        return true;
    }
    return false;
}

std::vector<uint8_t>
IceTransport::packIceMsg(uint8_t version) const
{
    if (not isInitialized())
        return {};

    std::stringstream ss;
    if (version == 1) {
        msgpack::pack(ss, version);
        msgpack::pack(ss, std::make_pair(pimpl_->local_ufrag_, pimpl_->local_pwd_));
        msgpack::pack(ss, static_cast<uint8_t>(pimpl_->component_count_));
        for (unsigned i=0; i<pimpl_->component_count_; i++)
            msgpack::pack(ss, getLocalCandidates(i));
    } else {
        SDP sdp;
        sdp.ufrag = pimpl_->local_ufrag_;
        sdp.pwd = pimpl_->local_pwd_;
        for (unsigned i = 0; i < pimpl_->component_count_; i++) {
            auto candidates = getLocalCandidates(i);
            sdp.candidates.reserve(sdp.candidates.size() + candidates.size());
            sdp.candidates.insert(sdp.candidates.end(), candidates.begin(), candidates.end());
        }
        msgpack::pack(ss, sdp);
    }
    auto str(ss.str());
    return std::vector<uint8_t>(str.begin(), str.end());
}

bool
IceTransport::getCandidateFromSDP(const std::string& line, IceCandidate& cand) const
{
    /**   Section 4.5, RFC 6544 (https://tools.ietf.org/html/rfc6544)
     *    candidate-attribute   = "candidate" ":" foundation SP component-id SP
     *                             "TCP" SP
     *                             priority SP
     *                             connection-address SP
     *                             port SP
     *                             cand-type
     *                             [SP rel-addr]
     *                             [SP rel-port]
     *                             SP tcp-type-ext
     *                             *(SP extension-att-name SP
     *                                  extension-att-value)
     *
     *     tcp-type-ext          = "tcptype" SP tcp-type
     *     tcp-type              = "active" / "passive" / "so"
     */
    int af, cnt;
    char foundation[32], transport[12], ipaddr[80], type[32], tcp_type[32];
    pj_str_t tmpaddr;
    int comp_id, prio, port;
    pj_status_t status;
    pj_bool_t is_tcp = PJ_FALSE;

    cnt = sscanf(line.c_str(), "%s %d %s %d %s %d typ %s tcptype %s\n",
                 foundation, &comp_id, transport, &prio, ipaddr, &port, type,
                 tcp_type);
    if (cnt != 7 && cnt != 8) {
      JAMI_ERR("[ice:%p] Invalid ICE candidate line", this);
      return false;
    }

    if (strcmp(transport, "TCP") == 0) {
      is_tcp = PJ_TRUE;
    }

    pj_bzero(&cand, sizeof(IceCandidate));

    if (strcmp(type, "host") == 0)
      cand.type = PJ_ICE_CAND_TYPE_HOST;
    else if (strcmp(type, "srflx") == 0)
      cand.type = PJ_ICE_CAND_TYPE_SRFLX;
    else if (strcmp(type, "prflx") == 0)
      cand.type = PJ_ICE_CAND_TYPE_PRFLX;
    else if (strcmp(type, "relay") == 0)
      cand.type = PJ_ICE_CAND_TYPE_RELAYED;
    else {
      JAMI_WARN("[ice:%p] invalid remote candidate type '%s'", this, type);
      return false;
    }

    if (is_tcp) {
      if (strcmp(tcp_type, "active") == 0)
        cand.transport = PJ_CAND_TCP_ACTIVE;
      else if (strcmp(tcp_type, "passive") == 0)
        cand.transport = PJ_CAND_TCP_PASSIVE;
      else if (strcmp(tcp_type, "so") == 0)
        cand.transport = PJ_CAND_TCP_SO;
      else {
        JAMI_WARN("[ice:%p] invalid transport type type '%s'", this, tcp_type);
        return false;
      }
    } else {
      cand.transport = PJ_CAND_UDP;
    }

    cand.comp_id = (pj_uint8_t)comp_id;
    cand.prio = prio;

    if (strchr(ipaddr, ':'))
      af = pj_AF_INET6();
    else {
      af = pj_AF_INET();
      pimpl_->onlyIPv4Private_ &= IpAddr(ipaddr).isPrivate();
    }

    tmpaddr = pj_str(ipaddr);
    pj_sockaddr_init(af, &cand.addr, NULL, 0);
    status = pj_sockaddr_set_str_addr(af, &cand.addr, &tmpaddr);
    if (status != PJ_SUCCESS) {
      JAMI_WARN("[ice:%p] invalid IP address '%s'", this, ipaddr);
      return false;
    }

    pj_sockaddr_set_port(&cand.addr, (pj_uint16_t)port);
    pj_strdup2(pimpl_->pool_.get(), &cand.foundation, foundation);

    return true;
}

ssize_t
IceTransport::recv(int comp_id, unsigned char* buf, size_t len, std::error_code& ec)
{
    auto &io = pimpl_->compIO_[comp_id];
    std::lock_guard<std::mutex> lk(io.mutex);

    if (io.queue.empty()) {
        ec = std::make_error_code(std::errc::resource_unavailable_try_again);
        return -1;
    }

    auto& packet = io.queue.front();
    const auto count = std::min(len, packet.data.size());
    std::copy_n(packet.data.begin(), count, buf);
    if (count == packet.data.size()) {
        io.queue.pop_front();
    } else {
        packet.data.erase(packet.data.begin(), packet.data.begin() + count);
    }

    ec.clear();
    return count;
}

ssize_t
IceTransport::recvfrom(int comp_id, char *buf, size_t len, std::error_code& ec) {
  return pimpl_->peerChannels_.at(comp_id).read(buf, len, ec);
}

void
IceTransport::setOnRecv(unsigned comp_id, IceRecvCb cb)
{
    auto& io = pimpl_->compIO_[comp_id];
    std::lock_guard<std::mutex> lk(io.mutex);
    io.cb = cb;

    if (cb) {
        // Flush existing queue using the callback
        for (const auto& packet : io.queue)
            io.cb((uint8_t*)packet.data.data(), packet.data.size());
        io.queue.clear();
    }
}

ssize_t
IceTransport::send(int comp_id, const unsigned char* buf, size_t len)
{
    sip_utils::register_thread();
    auto remote = getRemoteAddress(comp_id);
    if (!remote) {
        JAMI_ERR("[ice:%p] can't find remote address for component %d", this, comp_id);
        errno = EINVAL;
        return -1;
    }
    pj_ssize_t sent_size = 0;
    auto status = pj_ice_strans_sendto2(pimpl_->icest_.get(), comp_id+1, buf, len, remote.pjPtr(), remote.getLength(), &sent_size);
    if (status == PJ_EPENDING && isTCPEnabled()) {
        auto current_size = sent_size;
        // NOTE; because we are in TCP, the sent size will count the header (2
        // bytes length).
        while (comp_id < pimpl_->lastReadLen_.size() && current_size < len) {
          std::unique_lock<std::mutex> lk(pimpl_->iceMutex_);
          pimpl_->waitDataCv_.wait(lk);
          current_size = pimpl_->lastReadLen_[comp_id];
        }
    } else if (status != PJ_SUCCESS) {
        if (status == PJ_EBUSY) {
            errno = EAGAIN;
        } else {
            pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
            JAMI_ERR("[ice:%p] ice send failed: %s", this, pimpl_->last_errmsg_.c_str());
            errno = EIO;
        }
        return -1;
    }

    return len;
}

int
IceTransport::waitForInitialization(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lk(pimpl_->iceMutex_);
    if (!pimpl_->iceCV_.wait_for(lk, timeout,
                                 [this]{ return pimpl_->_isInitialized() or pimpl_->_isFailed(); })) {
        JAMI_WARN("[ice:%p] waitForInitialization: timeout", this);
        return -1;
    }
    return not pimpl_->_isFailed();
}

int
IceTransport::waitForNegotiation(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lk(pimpl_->iceMutex_);
    if (!pimpl_->iceCV_.wait_for(lk, timeout,
                         [this]{ return pimpl_->_isRunning() or pimpl_->_isFailed(); })) {
        JAMI_WARN("[ice:%p] waitForIceNegotiation: timeout", this);
        return -1;
    }
    return not pimpl_->_isFailed();
}

ssize_t
IceTransport::isDataAvailable(int comp_id)
{
    return pimpl_->peerChannels_.at(comp_id).isDataAvailable();
}

ssize_t
IceTransport::waitForData(int comp_id, std::chrono::milliseconds timeout, std::error_code& ec)
{
    return pimpl_->peerChannels_.at(comp_id).wait(timeout, ec);
}

std::vector<SDP>
IceTransport::parseSDPList(const std::vector<uint8_t>& msg)
{
    std::vector<SDP> sdp_list;

    msgpack::unpacker pac;
    pac.reserve_buffer(msg.size());
    memcpy(pac.buffer(), msg.data(), msg.size());
    pac.buffer_consumed(msg.size());
    msgpack::object_handle oh;

    while (auto result = pac.next(oh)) {
        try {
            SDP sdp;
            if (oh.get().type == msgpack::type::POSITIVE_INTEGER) {
                // Version 1
                result = pac.next(oh);
                if (!result) break;
                std::tie(sdp.ufrag, sdp.pwd) = oh.get().as<std::pair<std::string, std::string>>();
                result = pac.next(oh);
                if (!result) break;
                auto comp_cnt = oh.get().as<uint8_t>();
                while (comp_cnt-- > 0) {
                    result = pac.next(oh);
                    if (!result) break;
                    auto candidates = oh.get().as<std::vector<std::string>>();
                    sdp.candidates.reserve(sdp.candidates.size() + candidates.size());
                    sdp.candidates.insert(sdp.candidates.end(), candidates.begin(), candidates.end());
                }
            } else {
                oh.get().convert(sdp);
            }
            sdp_list.emplace_back(sdp);
        } catch (const msgpack::unpack_error &e) {
            break;
        }
    }

    return sdp_list;
}

bool
IceTransport::isTCPEnabled()
{
    return pimpl_->config_.protocol == PJ_ICE_TP_TCP;
}

//==============================================================================

IceTransportFactory::IceTransportFactory()
    : cp_()
    , pool_(nullptr, [](pj_pool_t* pool) { sip_utils::register_thread(); pj_pool_release(pool); })
    , ice_cfg_()
{
    sip_utils::register_thread();
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

    ice_cfg_.opt.aggressive = PJ_TRUE;
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
        JAMI_ERR("%s",e.what());
        return nullptr;
    }
}

//==============================================================================

void
IceSocketTransport::setOnRecv(RecvCb&& cb)
{
    return ice_->setOnRecv(compId_, cb);
}

bool
IceSocketTransport::isInitiator() const
{
    return ice_->isInitiator();
}

void
IceSocketTransport::shutdown()
{
    ice_->cancelOperations();
}

int
IceSocketTransport::maxPayload() const
{
    auto ip_header_size = (ice_->getRemoteAddress(compId_).getFamily() == AF_INET) ?
        IPV4_HEADER_SIZE : IPV6_HEADER_SIZE;
    return STANDARD_MTU_SIZE - ip_header_size - UDP_HEADER_SIZE;
}

int
IceSocketTransport::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (!ice_->isRunning()) return -1;
    return ice_->waitForData(compId_, timeout, ec);
}

std::size_t
IceSocketTransport::write(const ValueType* buf, std::size_t len, std::error_code& ec)
{
    auto res = ice_->send(compId_, buf, len);
    if (res < 0) {
        ec.assign(errno, std::generic_category());
        return 0;
    }
    ec.clear();
    return res;
}

std::size_t
IceSocketTransport::read(ValueType* buf, std::size_t len, std::error_code& ec)
{
    if (!ice_->isRunning()) return 0;
    try {
        auto res = reliable_
                ? ice_->recvfrom(compId_, reinterpret_cast<char *>(buf), len, ec)
                : ice_->recv(compId_, buf, len, ec);
        return (res < 0) ? 0 : res;
    } catch (const std::exception &e) {
        JAMI_ERR("IceSocketTransport::read exception: %s", e.what());
    }
    return 0;
}

IpAddr
IceSocketTransport::localAddr() const
{
    return ice_->getLocalAddress(compId_);
}

IpAddr
IceSocketTransport::remoteAddr() const
{
    return ice_->getRemoteAddress(compId_);
}

//==============================================================================

void
IceSocket::close()
{
    ice_transport_.reset();
}

ssize_t
IceSocket::send(const unsigned char* buf, size_t len)
{
    if (!ice_transport_.get())
        return -1;
    return ice_transport_->send(compId_, buf, len);
}

ssize_t
IceSocket::waitForData(std::chrono::milliseconds timeout)
{
    if (!ice_transport_.get())
        return -1;

    std::error_code ec;
    return ice_transport_->waitForData(compId_, timeout, ec);
}

void
IceSocket::setOnRecv(IceRecvCb cb)
{
    if (!ice_transport_.get())
        return;
    return ice_transport_->setOnRecv(compId_, cb);
}

uint16_t
IceSocket::getTransportOverhead(){
    return (ice_transport_->getRemoteAddress(compId_).getFamily() == AF_INET) ? IPV4_HEADER_SIZE : IPV6_HEADER_SIZE;
}

} // namespace jami
