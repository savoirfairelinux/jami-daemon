/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "dring/callmanager_interface.h"

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

#include "pj/limits.h"

#define TRY(ret) \
    do { \
        if ((ret) != PJ_SUCCESS) \
            throw std::runtime_error(#ret " failed"); \
    } while (0)

namespace jami {

static constexpr unsigned STUN_MAX_PACKET_SIZE {8192};
static constexpr uint16_t IPV6_HEADER_SIZE = 40; ///< Size in bytes of IPV6 packet header
static constexpr uint16_t IPV4_HEADER_SIZE = 20; ///< Size in bytes of IPV4 packet header
static constexpr int MAX_CANDIDATES {32};
static constexpr int MAX_DESTRUCTION_TIMEOUT {3};

//==============================================================================

using MutexGuard = std::lock_guard<std::mutex>;
using MutexLock = std::unique_lock<std::mutex>;
using namespace upnp;

namespace {

struct IceSTransDeleter
{
    void operator()(pj_ice_strans* ptr)
    {
        pj_ice_strans_stop_ice(ptr);
        pj_ice_strans_destroy(ptr);
    }
};

} // namespace

//==============================================================================

class IceTransport::Impl
{
public:
    Impl(const char* name, int component_count, bool master, const IceTransportOptions& options);
    ~Impl();

    void onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op, pj_status_t status);

    void onReceiveData(unsigned comp_id, void* pkt, pj_size_t size);

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

    void getDefaultCandidates();

    std::string link() const;

    // Non-mutex protected of public versions
    bool _isInitialized() const;
    bool _isStarted() const;
    bool _isRunning() const;
    bool _isFailed() const;

    const pj_ice_sess_cand* getSelectedCandidate(unsigned comp_id, bool remote) const;
    IpAddr getLocalAddress(unsigned comp_id) const;
    IpAddr getRemoteAddress(unsigned comp_id) const;
    static const char* getCandidateType(const pj_ice_sess_cand* cand);
    bool isTcpEnabled() const { return config_.protocol == PJ_ICE_TP_TCP; }
    bool addStunConfig(int af);
    void addDefaultCandidates();
    void requestUpnpMappings();
    bool hasUpnp() const;
    // Take a list of address pairs (local/public) and add them as
    // reflexive candidates using STUN config.
    void addServerReflexiveCandidates(const std::vector<std::pair<IpAddr, IpAddr>>& addrList);
    // Generate server reflexive candidates using the published (DHT/Account) address
    std::vector<std::pair<IpAddr, IpAddr>> setupGenericReflexiveCandidates();
    // Generate server reflexive candidates using UPNP mappings.
    std::vector<std::pair<IpAddr, IpAddr>> setupUpnpReflexiveCandidates();
    void setDefaultRemoteAddress(unsigned comp_id, const IpAddr& addr);
    const IpAddr& getDefaultRemoteAddress(unsigned comp_id) const;
    bool handleEvents(unsigned max_msec);

    std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>> pool_ {};
    IceTransportCompleteCb on_initdone_cb_ {};
    IceTransportCompleteCb on_negodone_cb_ {};
    IceRecvInfo on_recv_cb_ {};
    mutable std::mutex iceMutex_ {};
    std::unique_ptr<pj_ice_strans, IceSTransDeleter> icest_;
    unsigned component_count_ {};
    pj_ice_sess_cand cand_[MAX_CANDIDATES] {};
    std::string local_ufrag_ {};
    std::string local_pwd_ {};
    pj_sockaddr remoteAddr_ {};
    std::condition_variable iceCV_ {};
    pj_ice_strans_cfg config_ {};
    std::string last_errmsg_ {};

    std::atomic_bool is_stopped_ {false};

    struct Packet
    {
        Packet(void* pkt, pj_size_t size)
            : data {reinterpret_cast<char*>(pkt), reinterpret_cast<char*>(pkt) + size}
        {}
        std::vector<char> data {};
    };

    std::vector<PeerChannel> peerChannels_ {};

    struct ComponentIO
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<Packet> queue;
        IceRecvCb cb;
    };

    std::vector<ComponentIO> compIO_ {};

    std::atomic_bool initiatorSession_ {true};

    // Local/Public addresses used by the account owning the ICE instance.
    IpAddr accountLocalAddr_ {};
    IpAddr accountPublicAddr_ {};

    /**
     * Returns the IP of each candidate for a given component in the ICE session
     */
    struct LocalCandidate
    {
        IpAddr addr;
        pj_ice_cand_transport transport;
    };

    std::shared_ptr<upnp::Controller> upnp_ {};
    std::mutex upnpMutex_ {};
    std::map<Mapping::key_t, Mapping> upnpMappings_;
    std::mutex upnpMappingsMutex_ {};

    bool onlyIPv4Private_ {true};

    // IO/Timer events are handled by following thread
    std::thread thread_ {};
    std::atomic_bool threadTerminateFlags_ {false};

    // Wait data on components
    pj_size_t lastSentLen_ {};
    std::condition_variable waitDataCv_ = {};

    std::atomic_bool destroying_ {false};
    onShutdownCb scb {};

    // Default remote addresses
    std::vector<IpAddr> iceDefaultRemoteAddr_;
};

//==============================================================================

/**
 * Add stun/turn configuration or default host as candidates
 */

static void
add_stun_server(pj_pool_t& pool, pj_ice_strans_cfg& cfg, const StunServerInfo& info)
{
    if (cfg.stun_tp_cnt >= PJ_ICE_MAX_STUN)
        throw std::runtime_error("Too many STUN configurations");

    IpAddr ip {info.uri};

    // Given URI cannot be DNS resolved or not IPv4 or IPv6?
    // This prevents a crash into PJSIP when ip.toString() is called.
    if (ip.getFamily() == AF_UNSPEC) {
        JAMI_DBG("[ice (%s)] STUN server '%s' not used, unresolvable address",
                 (cfg.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
                 info.uri.c_str());
        return;
    }

    auto& stun = cfg.stun_tp[cfg.stun_tp_cnt++];
    pj_ice_strans_stun_cfg_default(&stun);
    pj_strdup2_with_null(&pool, &stun.server, ip.toString().c_str());
    stun.af = ip.getFamily();
    if (!(stun.port = ip.getPort()))
        stun.port = PJ_STUN_PORT;
    stun.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;
    stun.conn_type = cfg.stun.conn_type;

    JAMI_DBG("[ice (%s)] added stun server '%s', port %u",
             (cfg.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
             pj_strbuf(&stun.server),
             stun.port);
}

static void
add_turn_server(pj_pool_t& pool, pj_ice_strans_cfg& cfg, const TurnServerInfo& info)
{
    if (cfg.turn_tp_cnt >= PJ_ICE_MAX_TURN)
        throw std::runtime_error("Too many TURN servers");

    IpAddr ip {info.uri};

    // Same comment as add_stun_server()
    if (ip.getFamily() == AF_UNSPEC) {
        JAMI_DBG("[ice (%s)] TURN server '%s' not used, unresolvable address",
                 (cfg.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
                 info.uri.c_str());
        return;
    }

    auto& turn = cfg.turn_tp[cfg.turn_tp_cnt++];
    pj_ice_strans_turn_cfg_default(&turn);
    pj_strdup2_with_null(&pool, &turn.server, ip.toString().c_str());
    turn.af = ip.getFamily();
    if (!(turn.port = ip.getPort()))
        turn.port = PJ_STUN_PORT;
    turn.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;
    turn.conn_type = cfg.turn.conn_type;

    // Authorization (only static plain password supported yet)
    if (not info.password.empty()) {
        turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
        turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
        pj_strset(&turn.auth_cred.data.static_cred.realm,
                  (char*) info.realm.c_str(),
                  info.realm.size());
        pj_strset(&turn.auth_cred.data.static_cred.username,
                  (char*) info.username.c_str(),
                  info.username.size());
        pj_strset(&turn.auth_cred.data.static_cred.data,
                  (char*) info.password.c_str(),
                  info.password.size());
    }

    JAMI_DBG("[ice (%s)] added turn server '%s', port %u",
             (cfg.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
             pj_strbuf(&turn.server),
             turn.port);
}

//==============================================================================

IceTransport::Impl::Impl(const char* name,
                         int component_count,
                         bool master,
                         const IceTransportOptions& options)
    : pool_(nullptr,
            [](pj_pool_t* pool) {
                sip_utils::register_thread();
                pj_pool_release(pool);
            })
    , on_initdone_cb_(options.onInitDone)
    , on_negodone_cb_(options.onNegoDone)
    , component_count_(component_count)
    , compIO_(component_count)
    , initiatorSession_(master)
    , accountLocalAddr_(std::move(options.accountLocalAddr))
    , accountPublicAddr_(std::move(options.accountPublicAddr))
    , thread_()
    , iceDefaultRemoteAddr_(component_count)
{
    JAMI_DBG("[ice:%p] Creating IceTransport session for \"%s\" - comp count %u - as a %s",
             this,
             name,
             component_count,
             master ? "master" : "slave");

    sip_utils::register_thread();
    if (options.upnpEnable)
        upnp_.reset(new upnp::Controller());

    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
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

    addDefaultCandidates();

    addServerReflexiveCandidates(setupGenericReflexiveCandidates());

    if (upnp_) {
        requestUpnpMappings();
        auto const& upnpMaps = setupUpnpReflexiveCandidates();
        if (not upnpMaps.empty())
            addServerReflexiveCandidates(upnpMaps);
    }

    pool_.reset(
        pj_pool_create(iceTransportFactory.getPoolFactory(), "IceTransport.pool", 512, 512, NULL));
    if (not pool_)
        throw std::runtime_error("pj_pool_create() failed");

    pj_ice_strans_cb icecb;
    pj_bzero(&icecb, sizeof(icecb));

    icecb.on_rx_data = [](pj_ice_strans* ice_st,
                          unsigned comp_id,
                          void* pkt,
                          pj_size_t size,
                          const pj_sockaddr_t* /*src_addr*/,
                          unsigned /*src_addr_len*/) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st)))
            tr->onReceiveData(comp_id, pkt, size);
        else
            JAMI_WARN("null IceTransport");
    };

    icecb.on_ice_complete = [](pj_ice_strans* ice_st, pj_ice_strans_op op, pj_status_t status) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st)))
            tr->onComplete(ice_st, op, status);
        else
            JAMI_WARN("null IceTransport");
    };

    icecb.on_data_sent = [](pj_ice_strans* ice_st, pj_ssize_t size) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st))) {
            std::lock_guard<std::mutex> lk(tr->iceMutex_);
            tr->lastSentLen_ += size;
            tr->waitDataCv_.notify_all();
        } else
            JAMI_WARN("null IceTransport");
    };

    icecb.on_destroy = [](pj_ice_strans* ice_st) {
        if (auto* tr = static_cast<Impl*>(pj_ice_strans_get_user_data(ice_st))) {
            tr->destroying_ = true;
            tr->waitDataCv_.notify_all();
            if (tr->scb)
                tr->scb();
        } else {
            JAMI_WARN("null IceTransport");
        }
    };

    // Add STUN servers
    for (auto& server : options.stunServers)
        add_stun_server(*pool_, config_, server);

    // Add TURN servers
    for (auto& server : options.turnServers)
        add_turn_server(*pool_, config_, server);

    static constexpr auto IOQUEUE_MAX_HANDLES = std::min(PJ_IOQUEUE_MAX_HANDLES, 64);
    TRY(pj_timer_heap_create(pool_.get(), 100, &config_.stun_cfg.timer_heap));
    TRY(pj_ioqueue_create(pool_.get(), IOQUEUE_MAX_HANDLES, &config_.stun_cfg.ioqueue));

    pj_ice_strans* icest = nullptr;
    pj_status_t status = pj_ice_strans_create(name, &config_, component_count, this, &icecb, &icest);

    if (status != PJ_SUCCESS || icest == nullptr) {
        throw std::runtime_error("pj_ice_strans_create() failed");
    }

    // Must be created after any potential failure
    thread_ = std::thread([this] {
        sip_utils::register_thread();
        while (not threadTerminateFlags_) {
            // NOTE: handleEvents can return false in this case
            // but here we don't care if there is event or not.
            handleEvents(500); // limit polling to 500ms
        }
        // NOTE: This last handleEvents is necessary to close TURN socket.
        // Because when destroying the TURN session pjproject creates a pj_timer
        // to postpone the TURN destruction. This timer is only called if we poll
        // the event queue.
        auto started_destruction = std::chrono::system_clock::now();
        while (handleEvents(500)) {
            if (std::chrono::system_clock::now() - started_destruction
                > std::chrono::seconds(MAX_DESTRUCTION_TIMEOUT)) {
                // If the transport is not closed after 3 seconds, avoid blocking
                break;
            }
        }
    });

    // Init to invalid addresses
    iceDefaultRemoteAddr_.reserve(component_count);
}

IceTransport::Impl::~Impl()
{
    JAMI_DBG("[ice:%p] destroying", this);
    sip_utils::register_thread();
    threadTerminateFlags_ = true;
    iceCV_.notify_all();

    if (thread_.joinable())
        thread_.join();

    {
        std::lock_guard<std::mutex> lk {iceMutex_};
        icest_.reset(); // must be done before ioqueue/timer destruction
    }

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

bool
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
    auto ret = timeout.msec != PJ_MAXINT32;

    // timeout limitation
    if (timeout.msec >= 1000)
        timeout.msec = 999;
    if (PJ_TIME_VAL_GT(timeout, max_timeout))
        timeout = max_timeout;

    do {
        auto n_events = pj_ioqueue_poll(config_.stun_cfg.ioqueue, &timeout);

        // timeout
        if (not n_events)
            return ret;

        // error
        if (n_events < 0) {
            const auto err = pj_get_os_error();
            // Kept as debug as some errors are "normal" in regular context
            last_errmsg_ = sip_utils::sip_strerror(err);
            JAMI_DBG("[ice:%p] ioqueue error %d: %s", this, err, last_errmsg_.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(PJ_TIME_VAL_MSEC(timeout)));
            return ret;
        }

        net_event_count += n_events;
        timeout.sec = timeout.msec = 0;
    } while (net_event_count < MAX_NET_EVENTS);
    return ret;
}

void
IceTransport::Impl::onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op, pj_status_t status)
{
    const char* opname = op == PJ_ICE_STRANS_OP_INIT          ? "initialization"
                         : op == PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation"
                                                              : "unknown_op";

    const bool done = status == PJ_SUCCESS;
    if (done) {
        JAMI_DBG("[ice:%p] %s %s success",
                 this,
                 (config_.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
                 opname);
    } else {
        last_errmsg_ = sip_utils::sip_strerror(status);
        JAMI_ERR("[ice:%p] %s %s failed: %s",
                 this,
                 (config_.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
                 opname,
                 last_errmsg_.c_str());
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
    }

    if (op == PJ_ICE_STRANS_OP_INIT and on_initdone_cb_)
        on_initdone_cb_(done);
    else if (op == PJ_ICE_STRANS_OP_NEGOTIATION) {
        if (done) {
            // Dump of connection pairs
            std::ostringstream out;
            for (unsigned i = 0; i < component_count_; ++i) {
                auto laddr = getLocalAddress(i);
                auto raddr = getRemoteAddress(i);

                if (laddr and raddr) {
                    out << " [" << i + 1 << "] " << laddr.toString(true, true) << " ["
                        << getCandidateType(getSelectedCandidate(i, false)) << "] "
                        << " <-> " << raddr.toString(true, true) << " ["
                        << getCandidateType(getSelectedCandidate(i, true)) << "] " << '\n';
                } else {
                    out << " [" << i + 1 << "] disabled\n";
                }
            }

            JAMI_DBG("[ice:%p] %s connection pairs ([comp id] local [type] <-> remote [type]):\n%s",
                     this,
                     (config_.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP"),
                     out.str().c_str());
        }
        if (on_negodone_cb_)
            on_negodone_cb_(done);
    }

    // Unlock waitForXXX APIs
    iceCV_.notify_all();
}

std::string
IceTransport::Impl::link() const
{
    std::ostringstream out;
    for (unsigned i = 0; i < component_count_; ++i) {
        auto laddr = getLocalAddress(i);
        auto raddr = getRemoteAddress(i);

        if (laddr and raddr) {
            out << " [" << i + 1 << "] " << laddr.toString(true, true) << " ["
                << getCandidateType(getSelectedCandidate(i, false)) << "] "
                << " <-> " << raddr.toString(true, true) << " ["
                << getCandidateType(getSelectedCandidate(i, true)) << "] ";
        } else {
            out << " [" << i + 1 << "] disabled";
        }
        if (i + 1 != component_count_)
            out << "\n";
    }
    return out.str();
}

bool
IceTransport::Impl::setInitiatorSession()
{
    JAMI_DBG("[ice:%p] as master", this);
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
    JAMI_DBG("[ice:%p] as slave", this);
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

const pj_ice_sess_cand*
IceTransport::Impl::getSelectedCandidate(unsigned comp_id, bool remote) const
{
    // Return the selected candidate pair. Might not be the nominated pair if
    // ICE has not concluded yet, but should be the nominated pair afterwards.
    if (not _isRunning()) {
        JAMI_ERR("[ice:%p] ICE transport is not running", this);
        return nullptr;
    }
    const auto* sess = pj_ice_strans_get_valid_pair(icest_.get(), comp_id + 1);
    if (sess == nullptr) {
        JAMI_ERR("[ice:%p] Component %i has no valid pair", this, comp_id);
        return nullptr;
    }

    if (remote)
        return sess->rcand;
    else
        return sess->lcand;
}

IpAddr
IceTransport::Impl::getLocalAddress(unsigned comp_id) const
{
    if (auto cand = getSelectedCandidate(comp_id, false))
        return cand->addr;

    JAMI_ERR("[ice:%p] No local address for component %i", this, comp_id);
    return {};
}

IpAddr
IceTransport::Impl::getRemoteAddress(unsigned comp_id) const
{
    if (auto cand = getSelectedCandidate(comp_id, true))
        return cand->addr;

    JAMI_ERR("[ice:%p] No remote address for component %i", this, comp_id);
    return {};
}

const char*
IceTransport::Impl::getCandidateType(const pj_ice_sess_cand* cand)
{
    auto name = cand ? pj_ice_get_cand_type_name(cand->type) : nullptr;
    return name ? name : "?";
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
IceTransport::Impl::getDefaultCandidates()
{
    for (unsigned i = 0; i < component_count_; ++i)
        pj_ice_strans_get_def_cand(icest_.get(), i + 1, &cand_[i]);
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
    getDefaultCandidates();
    JAMI_DBG("[ice:%p] (local) ufrag=%s, pwd=%s", this, local_ufrag_.c_str(), local_pwd_.c_str());
    return true;
}

bool
IceTransport::Impl::addStunConfig(int af)
{
    if (config_.stun_tp_cnt >= PJ_ICE_MAX_STUN) {
        JAMI_ERR("Max number of STUN configurations reached (%i)", PJ_ICE_MAX_STUN);
        return false;
    }

    if (af != pj_AF_INET() and af != pj_AF_INET6()) {
        JAMI_ERR("Invalid address familly (%i)", af);
        return false;
    }

    auto& stun = config_.stun_tp[config_.stun_tp_cnt++];

    pj_ice_strans_stun_cfg_default(&stun);
    stun.cfg.max_pkt_size = STUN_MAX_PACKET_SIZE;
    stun.af = af;
    stun.conn_type = config_.stun.conn_type;

    JAMI_DBG("[ice:%p] added host stun config for %s transport",
             this,
             config_.protocol == PJ_ICE_TP_TCP ? "TCP" : "UDP");

    return true;
}

void
IceTransport::Impl::addDefaultCandidates()
{
    JAMI_DBG("[ice:%p]: Setup default candidates", this);

    // STUN configs layout:
    // - index 0 : host IPv4
    // - index 1 : host IPv6
    // - index 2 : upnp/srflx IPv4.

    config_.stun_tp_cnt = 0;
    addStunConfig(pj_AF_INET());
    addStunConfig(pj_AF_INET6());
}

void
IceTransport::Impl::requestUpnpMappings()
{
    // Must be called once !

    std::lock_guard<std::mutex> lock(upnpMutex_);

    if (not upnp_)
        return;

    auto transport = isTcpEnabled() ? PJ_CAND_TCP_PASSIVE : PJ_CAND_UDP;
    auto portType = transport == PJ_CAND_UDP ? PortType::UDP : PortType::TCP;

    // Request upnp mapping for each component.
    for (unsigned compId = 1; compId <= component_count_; compId++) {
        // Set port number to 0 to get any available port.
        Mapping requestedMap(portType);

        // Request the mapping
        Mapping::sharedPtr_t mapPtr = upnp_->reserveMapping(requestedMap);

        // To use a mapping, it must be valid, open and has valid host address.
        if (mapPtr and mapPtr->getMapKey() and (mapPtr->getState() == MappingState::OPEN)
            and mapPtr->hasValidHostAddress()) {
            std::lock_guard<std::mutex> lock(upnpMappingsMutex_);
            auto ret = upnpMappings_.emplace(mapPtr->getMapKey(), *mapPtr);
            if (ret.second) {
                JAMI_DBG("[ice:%p] UPNP mapping %s successfully allocated",
                         this,
                         mapPtr->toString(true).c_str());
            } else {
                JAMI_WARN("[ice:%p] UPNP mapping %s already in the list!",
                          this,
                          mapPtr->toString().c_str());
            }
        } else {
            JAMI_WARN("[ice:%p]: UPNP mapping request failed!", this);
            upnp_->releaseMapping(requestedMap);
        }
    }
}

bool
IceTransport::Impl::hasUpnp() const
{
    return upnp_ and upnpMappings_.size() == component_count_;
}

void
IceTransport::Impl::addServerReflexiveCandidates(
    const std::vector<std::pair<IpAddr, IpAddr>>& addrList)
{
    if (addrList.size() != component_count_) {
        JAMI_WARN("[ice:%p]: Provided addr list size %lu does not match component count %u",
                  this,
                  addrList.size(),
                  component_count_);
        return;
    }

    // Add config for server reflexive candidates (UPNP or from DHT).
    if (not addStunConfig(pj_AF_INET()))
        return;

    assert(config_.stun_tp_cnt > 0 && config_.stun_tp_cnt < PJ_ICE_MAX_STUN);
    auto& stun = config_.stun_tp[config_.stun_tp_cnt - 1];

    for (unsigned compIdx = 0; compIdx < component_count_; compIdx++) {
        auto& localAddr = addrList[compIdx].first;
        auto& publicAddr = addrList[compIdx].second;

        pj_sockaddr_cp(&stun.cfg.user_mapping[compIdx].local_addr, localAddr.pjPtr());
        pj_sockaddr_cp(&stun.cfg.user_mapping[compIdx].mapped_addr, publicAddr.pjPtr());

        if (isTcpEnabled()) {
            if (publicAddr.getPort() == 9) {
                stun.cfg.user_mapping[compIdx].tp_type = PJ_CAND_TCP_ACTIVE;
            } else {
                stun.cfg.user_mapping[compIdx].tp_type = PJ_CAND_TCP_PASSIVE;
            }
        } else {
            stun.cfg.user_mapping[compIdx].tp_type = PJ_CAND_UDP;
        }
    }

    stun.cfg.user_mapping_cnt = component_count_;
    assert(stun.cfg.user_mapping_cnt < PJ_ICE_MAX_COMP);
}

std::vector<std::pair<IpAddr, IpAddr>>
IceTransport::Impl::setupGenericReflexiveCandidates()
{
    std::vector<std::pair<IpAddr, IpAddr>> addrList;
    auto isTcp = isTcpEnabled();

    // First, set default server reflexive candidates using current
    // account public address.
    // Then, check for UPNP mappings and and them if available.
    // For TCP transport, the connection type is set to passive for UPNP
    // candidates and set to active otherwise.

    if (accountLocalAddr_ and accountPublicAddr_) {
        addrList.reserve(component_count_);
        for (unsigned compIdx = 0; compIdx < component_count_; compIdx++) {
            // For TCP, the type is set to active, because most likely the incoming
            // connection will be blocked by the NAT.
            // For UDP use random port number.
            uint16_t port = isTcp ? 9
                                  : upnp::Controller::generateRandomPort(isTcp ? PortType::TCP
                                                                               : PortType::UDP);

            accountLocalAddr_.setPort(port);
            accountPublicAddr_.setPort(port);
            addrList.emplace_back(accountLocalAddr_, accountPublicAddr_);

            JAMI_DBG("[ice:%p]: Add generic local reflexive candidates [%s : %s] for comp %u",
                     this,
                     accountLocalAddr_.toString(true).c_str(),
                     accountPublicAddr_.toString(true).c_str(),
                     compIdx + 1);
        }
    }

    return addrList;
}

std::vector<std::pair<IpAddr, IpAddr>>
IceTransport::Impl::setupUpnpReflexiveCandidates()
{
    // Add UPNP server reflexive candidates if available.
    if (not hasUpnp())
        return {};

    std::lock_guard<std::mutex> lock(upnpMappingsMutex_);

    if (static_cast<unsigned>(upnpMappings_.size()) < component_count_) {
        JAMI_WARN("[ice:%p]: Not enough mappings %lu. Expected %u",
                  this,
                  upnpMappings_.size(),
                  component_count_);
        return {};
    }

    std::vector<std::pair<IpAddr, IpAddr>> addrList;

    unsigned compId = 1;
    addrList.reserve(upnpMappings_.size());
    for (auto const& [_, map] : upnpMappings_) {
        assert(map.getMapKey());
        IpAddr localAddr {map.getInternalAddress()};
        localAddr.setPort(map.getInternalPort());
        IpAddr publicAddr {map.getExternalAddress()};
        publicAddr.setPort(map.getExternalPort());
        addrList.emplace_back(localAddr, publicAddr);

        JAMI_DBG("[ice:%p]: Add UPNP local reflexive candidates [%s : %s] for comp %u",
                 this,
                 localAddr.toString(true).c_str(),
                 publicAddr.toString(true).c_str(),
                 compId);
        compId++;
    }

    return addrList;
}

void
IceTransport::Impl::setDefaultRemoteAddress(unsigned comp_id, const IpAddr& addr)
{
    // Component ID must be valid.
    assert(static_cast<unsigned>(comp_id) < component_count_);

    iceDefaultRemoteAddr_[comp_id] = addr;
    // The port does not matter. Set it 0 to avoid confusion.
    iceDefaultRemoteAddr_[comp_id].setPort(0);
}

const IpAddr&
IceTransport::Impl::getDefaultRemoteAddress(unsigned comp_id) const
{
    // Component ID must be valid.
    assert(static_cast<unsigned>(comp_id) < component_count_);

    return iceDefaultRemoteAddr_[comp_id];
}

void
IceTransport::Impl::onReceiveData(unsigned comp_id, void* pkt, pj_size_t size)
{
    if (!comp_id or comp_id > component_count_) {
        JAMI_ERR("rx: invalid comp_id (%u)", comp_id);
        return;
    }
    if (!size)
        return;
    auto& io = compIO_[comp_id - 1];
    std::unique_lock<std::mutex> lk(io.mutex);
    if (io.cb) {
        io.cb((uint8_t*) pkt, size);
    } else {
        std::error_code ec;
        auto err = peerChannels_.at(comp_id - 1).write((const char*) pkt, size, ec);
        if (err < 0) {
            JAMI_ERR("[ice:%p] rx: channel is closed", this);
        }
    }
}

//==============================================================================

IceTransport::IceTransport(const char* name,
                           int component_count,
                           bool master,
                           const IceTransportOptions& options)
    : pimpl_ {std::make_unique<Impl>(name, component_count, master, options)}
{}

IceTransport::~IceTransport() {}

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

std::string
IceTransport::getLastErrMsg() const
{
    return pimpl_->last_errmsg_;
}

bool
IceTransport::isInitiator() const
{
    if (isInitialized()) {
        return pj_ice_strans_get_role(pimpl_->icest_.get()) == PJ_ICE_SESS_ROLE_CONTROLLING;
    }
    return pimpl_->initiatorSession_;
}

bool
IceTransport::startIce(const Attribute& rem_attrs, std::vector<IceCandidate>&& rem_candidates)
{
    if (not isInitialized()) {
        JAMI_ERR("[ice:%p] not initialized transport", pimpl_.get());
        pimpl_->is_stopped_ = true;
        return false;
    }

    // pj_ice_strans_start_ice crashes if remote candidates array is empty
    if (rem_candidates.empty()) {
        JAMI_ERR("[ice:%p] start failed: no remote candidates", pimpl_.get());
        pimpl_->is_stopped_ = true;
        return false;
    }

    auto comp_cnt = std::max(1u, getComponentCount());
    if (rem_candidates.size() / comp_cnt > PJ_ICE_ST_MAX_CAND - 1) {
        std::vector<IceCandidate> rcands;
        rcands.reserve(PJ_ICE_ST_MAX_CAND - 1);
        JAMI_WARN("[ice:%p] too much candidates detected, trim list.", pimpl_.get());
        // Just trim some candidates. To avoid to only take host candidates, iterate
        // through the whole list and select some host, some turn and peer reflexives
        // It should give at least enough infos to negotiate.
        auto maxHosts = 8;
        auto maxRelays = PJ_ICE_MAX_TURN;
        for (auto& c : rem_candidates) {
            if (c.type == PJ_ICE_CAND_TYPE_HOST) {
                if (maxHosts == 0)
                    continue;
                maxHosts -= 1;
            } else if (c.type == PJ_ICE_CAND_TYPE_RELAYED) {
                if (maxRelays == 0)
                    continue;
                maxRelays -= 1;
            }
            if (rcands.size() == PJ_ICE_ST_MAX_CAND - 1)
                break;
            rcands.emplace_back(std::move(c));
        }
        rem_candidates = std::move(rcands);
    }

    pj_str_t ufrag, pwd;
    JAMI_DBG("[ice:%p] negotiation starting (%zu remote candidates)",
             pimpl_.get(),
             rem_candidates.size());
    auto status = pj_ice_strans_start_ice(pimpl_->icest_.get(),
                                          pj_strset(&ufrag,
                                                    (char*) rem_attrs.ufrag.c_str(),
                                                    rem_attrs.ufrag.size()),
                                          pj_strset(&pwd,
                                                    (char*) rem_attrs.pwd.c_str(),
                                                    rem_attrs.pwd.size()),
                                          rem_candidates.size(),
                                          rem_candidates.data());
    if (status != PJ_SUCCESS) {
        pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
        JAMI_ERR("[ice:%p] start failed: %s", pimpl_.get(), pimpl_->last_errmsg_.c_str());
        pimpl_->is_stopped_ = true;
        return false;
    }

    return true;
}

bool
IceTransport::startIce(const SDP& sdp)
{
    if (not isInitialized()) {
        JAMI_ERR("[ice:%p] not initialized transport", pimpl_.get());
        pimpl_->is_stopped_ = true;
        return false;
    }

    for (unsigned i = 0; i < pimpl_->component_count_; i++) {
        auto candVec = getLocalCandidates(i);
        for (auto const& cand : candVec) {
            JAMI_DBG("[ice:%p] Using local candidate %s for comp %u", pimpl_.get(), cand.c_str(), i);
        }
    }

    JAMI_DBG("[ice:%p] negotiation starting (%zu remote candidates)",
             pimpl_.get(),
             sdp.candidates.size());
    pj_str_t ufrag, pwd;

    std::vector<IceCandidate> rem_candidates;
    rem_candidates.reserve(sdp.candidates.size());
    IceCandidate cand;
    for (const auto& line : sdp.candidates) {
        if (getCandidateFromSDP(line, cand))
            rem_candidates.emplace_back(cand);
    }
    std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
    if (!pimpl_->icest_)
        return false;
    auto status = pj_ice_strans_start_ice(pimpl_->icest_.get(),
                                          pj_strset(&ufrag,
                                                    (char*) sdp.ufrag.c_str(),
                                                    sdp.ufrag.size()),
                                          pj_strset(&pwd, (char*) sdp.pwd.c_str(), sdp.pwd.size()),
                                          rem_candidates.size(),
                                          rem_candidates.data());
    if (status != PJ_SUCCESS) {
        pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
        JAMI_ERR("[ice:%p] start failed: %s", pimpl_.get(), pimpl_->last_errmsg_.c_str());
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
        std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
        if (!pimpl_->icest_)
            return false;
        auto status = pj_ice_strans_stop_ice(pimpl_->icest_.get());
        if (status != PJ_SUCCESS) {
            pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
            JAMI_ERR("[ice:%p] ICE stop failed: %s", pimpl_.get(), pimpl_->last_errmsg_.c_str());
            return false;
        }
    }
    return true;
}

void
IceTransport::cancelOperations()
{
    for (auto& c : pimpl_->peerChannels_) {
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
    // Return the default remote address if set.
    // Note that the default remote addresses are the addresses
    // set in the 'c=' and 'a=rtcp' lines of the received SDP.
    // See pj_ice_strans_sendto2() for more details.

    if (pimpl_->getDefaultRemoteAddress(comp_id)) {
        return pimpl_->getDefaultRemoteAddress(comp_id);
    }

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

    {
        std::lock_guard<std::mutex> lk {pimpl_->iceMutex_};
        if (!pimpl_->icest_)
            return res;
        if (pj_ice_strans_enum_cands(pimpl_->icest_.get(), comp_id, &cand_cnt, cand) != PJ_SUCCESS) {
            JAMI_ERR("[ice:%p] pj_ice_strans_enum_cands() failed", pimpl_.get());
            return res;
        }
    }

    res.reserve(cand_cnt);
    for (unsigned i = 0; i < cand_cnt; ++i) {
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
        char ipaddr[PJ_INET6_ADDRSTRLEN];
        std::string tcp_type;
        if (cand[i].transport != PJ_CAND_UDP) {
            tcp_type += " tcptype";
            switch (cand[i].transport) {
            case PJ_CAND_TCP_ACTIVE:
                tcp_type += " active";
                break;
            case PJ_CAND_TCP_PASSIVE:
                tcp_type += " passive";
                break;
            case PJ_CAND_TCP_SO:
            default:
                tcp_type += " so";
                break;
            }
        }
        res.emplace_back(
            fmt::format("{} {} {} {} {} {} typ {}{}",
                        std::string_view(cand[i].foundation.ptr, cand[i].foundation.slen),
                        cand[i].comp_id,
                        (cand[i].transport == PJ_CAND_UDP ? "UDP" : "TCP"),
                        cand[i].prio,
                        pj_sockaddr_print(&cand[i].addr, ipaddr, sizeof(ipaddr), 0),
                        pj_sockaddr_get_port(&cand[i].addr),
                        pj_ice_get_cand_type_name(cand[i].type),
                        tcp_type));
    }

    return res;
}

std::vector<uint8_t>
IceTransport::packIceMsg(uint8_t version) const
{
    if (not isInitialized())
        return {};

    msgpack::sbuffer buffer;
    if (version == 1) {
        msgpack::pack(buffer, version);
        msgpack::pack(buffer, std::make_pair(pimpl_->local_ufrag_, pimpl_->local_pwd_));
        msgpack::pack(buffer, static_cast<uint8_t>(pimpl_->component_count_));
        for (unsigned i = 1; i <= pimpl_->component_count_; i++)
            msgpack::pack(buffer, getLocalCandidates(i));
    } else {
        SDP sdp;
        sdp.ufrag = pimpl_->local_ufrag_;
        sdp.pwd = pimpl_->local_pwd_;
        for (unsigned i = 1; i <= pimpl_->component_count_; i++) {
            auto candidates = getLocalCandidates(i);
            sdp.candidates.reserve(sdp.candidates.size() + candidates.size());
            sdp.candidates.insert(sdp.candidates.end(), candidates.begin(), candidates.end());
        }
        msgpack::pack(buffer, sdp);
    }
    return std::vector<uint8_t>(buffer.data(), buffer.data() + buffer.size());
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

    cnt = sscanf(line.c_str(),
                 "%31s %d %11s %d %79s %d typ %31s tcptype %31s\n",
                 foundation,
                 &comp_id,
                 transport,
                 &prio,
                 ipaddr,
                 &port,
                 type,
                 tcp_type);
    if (cnt != 7 && cnt != 8) {
        JAMI_ERR("[ice:%p] Invalid ICE candidate line", pimpl_.get());
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
        JAMI_WARN("[ice:%p] invalid remote candidate type '%s'", pimpl_.get(), type);
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
            JAMI_WARN("[ice:%p] invalid transport type type '%s'", pimpl_.get(), tcp_type);
            return false;
        }
    } else {
        cand.transport = PJ_CAND_UDP;
    }

    cand.comp_id = (pj_uint8_t) comp_id;
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
        JAMI_WARN("[ice:%p] invalid IP address '%s'", pimpl_.get(), ipaddr);
        return false;
    }

    pj_sockaddr_set_port(&cand.addr, (pj_uint16_t) port);
    pj_strdup2(pimpl_->pool_.get(), &cand.foundation, foundation);

    return true;
}

ssize_t
IceTransport::recv(int comp_id, unsigned char* buf, size_t len, std::error_code& ec)
{
    auto& io = pimpl_->compIO_[comp_id];
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
IceTransport::recvfrom(int comp_id, char* buf, size_t len, std::error_code& ec)
{
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
            io.cb((uint8_t*) packet.data.data(), packet.data.size());
        io.queue.clear();
    }
}

void
IceTransport::setOnShutdown(onShutdownCb&& cb)
{
    pimpl_->scb = cb;
}

ssize_t
IceTransport::send(int comp_id, const unsigned char* buf, size_t len)
{
    sip_utils::register_thread();
    auto remote = getRemoteAddress(comp_id);

    if (!remote) {
        JAMI_ERR("[ice:%p] can't find remote address for component %d", pimpl_.get(), comp_id);
        errno = EINVAL;
        return -1;
    }
    auto status = pj_ice_strans_sendto2(pimpl_->icest_.get(),
                                        comp_id + 1,
                                        buf,
                                        len,
                                        remote.pjPtr(),
                                        remote.getLength());
    if (status == PJ_EPENDING && isTCPEnabled()) {
        // NOTE; because we are in TCP, the sent size will count the header (2
        // bytes length).
        std::unique_lock<std::mutex> lk(pimpl_->iceMutex_);
        pimpl_->waitDataCv_.wait(lk, [&] {
            return pimpl_->lastSentLen_ >= static_cast<pj_size_t>(len)
                   or pimpl_->destroying_.load();
        });
        pimpl_->lastSentLen_ = 0;
    } else if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        if (status == PJ_EBUSY) {
            errno = EAGAIN;
        } else {
            pimpl_->last_errmsg_ = sip_utils::sip_strerror(status);
            JAMI_ERR("[ice:%p] ice send failed: %s", pimpl_.get(), pimpl_->last_errmsg_.c_str());
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
    if (!pimpl_->iceCV_.wait_for(lk, timeout, [this] {
            return pimpl_->threadTerminateFlags_ or pimpl_->_isInitialized() or pimpl_->_isFailed();
        })) {
        JAMI_WARN("[ice:%p] waitForInitialization: timeout", pimpl_.get());
        return -1;
    }
    return not(pimpl_->threadTerminateFlags_ or pimpl_->_isFailed());
}

int
IceTransport::waitForNegotiation(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lk(pimpl_->iceMutex_);
    if (!pimpl_->iceCV_.wait_for(lk, timeout, [this] {
            return pimpl_->threadTerminateFlags_ or pimpl_->_isRunning() or pimpl_->_isFailed();
        })) {
        JAMI_WARN("[ice:%p] waitForIceNegotiation: timeout", pimpl_.get());
        return -1;
    }
    return not(pimpl_->threadTerminateFlags_ or pimpl_->_isFailed());
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

    try {
        size_t off = 0;
        while (off != msg.size()) {
            msgpack::unpacked result;
            msgpack::unpack(result, (const char*) msg.data(), msg.size(), off);
            SDP sdp;
            if (result.get().type == msgpack::type::POSITIVE_INTEGER) {
                // Version 1
                msgpack::unpack(result, (const char*) msg.data(), msg.size(), off);
                std::tie(sdp.ufrag, sdp.pwd) = result.get().as<std::pair<std::string, std::string>>();
                msgpack::unpack(result, (const char*) msg.data(), msg.size(), off);
                auto comp_cnt = result.get().as<uint8_t>();
                while (comp_cnt-- > 0) {
                    msgpack::unpack(result, (const char*) msg.data(), msg.size(), off);
                    auto candidates = result.get().as<std::vector<std::string>>();
                    sdp.candidates.reserve(sdp.candidates.size() + candidates.size());
                    sdp.candidates.insert(sdp.candidates.end(),
                                          candidates.begin(),
                                          candidates.end());
                }
            } else {
                result.get().convert(sdp);
            }
            sdp_list.emplace_back(std::move(sdp));
        }
    } catch (const msgpack::unpack_error& e) {
        JAMI_WARN("Error parsing sdp: %s", e.what());
    }

    return sdp_list;
}

bool
IceTransport::isTCPEnabled()
{
    return pimpl_->isTcpEnabled();
}

ICESDP
IceTransport::parse_SDP(std::string_view sdp_msg, const IceTransport& ice)
{
    ICESDP res;
    int nr = 0;
    for (std::string_view line; jami::getline(sdp_msg, line); nr++) {
        if (nr == 0) {
            res.rem_ufrag = line;
        } else if (nr == 1) {
            res.rem_pwd = line;
        } else {
            IceCandidate cand;
            if (ice.getCandidateFromSDP(std::string(line), cand)) {
                JAMI_DBG("Add remote ICE candidate: %.*s", (int) line.size(), line.data());
                res.rem_candidates.emplace_back(cand);
            }
        }
    }
    return res;
}

void
IceTransport::setDefaultRemoteAddress(int comp_id, const IpAddr& addr)
{
    pimpl_->setDefaultRemoteAddress(comp_id, addr);
}

std::string
IceTransport::link() const
{
    return pimpl_->link();
}

//==============================================================================

IceTransportFactory::IceTransportFactory()
    : cp_()
    , pool_(nullptr,
            [](pj_pool_t* pool) {
                sip_utils::register_thread();
                pj_pool_release(pool);
            })
    , ice_cfg_()
{
    sip_utils::register_thread();
    pj_caching_pool_init(&cp_, NULL, 0);
    pool_.reset(pj_pool_create(&cp_.factory, "IceTransportFactory.pool", 512, 512, NULL));
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
IceTransportFactory::createTransport(const char* name,
                                     int component_count,
                                     bool master,
                                     const IceTransportOptions& options)
{
    try {
        return std::make_shared<IceTransport>(name, component_count, master, options);
    } catch (const std::exception& e) {
        JAMI_ERR("%s", e.what());
        return nullptr;
    }
}

std::unique_ptr<IceTransport>
IceTransportFactory::createUTransport(const char* name,
                                      int component_count,
                                      bool master,
                                      const IceTransportOptions& options)
{
    try {
        return std::make_unique<IceTransport>(name, component_count, master, options);
    } catch (const std::exception& e) {
        JAMI_ERR("%s", e.what());
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
    auto ip_header_size = (ice_->getRemoteAddress(compId_).getFamily() == AF_INET)
                              ? IPV4_HEADER_SIZE
                              : IPV6_HEADER_SIZE;
    return STANDARD_MTU_SIZE - ip_header_size - UDP_HEADER_SIZE;
}

int
IceSocketTransport::waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const
{
    if (!ice_->isRunning())
        return -1;
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
    if (!ice_->isRunning())
        return 0;
    try {
        auto res = reliable_ ? ice_->recvfrom(compId_, reinterpret_cast<char*>(buf), len, ec)
                             : ice_->recv(compId_, buf, len, ec);
        return (res < 0) ? 0 : res;
    } catch (const std::exception& e) {
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
    if (ice_transport_)
        ice_transport_->setOnRecv(compId_, cb);
}

uint16_t
IceSocket::getTransportOverhead()
{
    return (ice_transport_->getRemoteAddress(compId_).getFamily() == AF_INET) ? IPV4_HEADER_SIZE
                                                                              : IPV6_HEADER_SIZE;
}

void
IceSocket::setDefaultRemoteAddress(const IpAddr& addr)
{
    if (ice_transport_)
        ice_transport_->setDefaultRemoteAddress(compId_, addr);
}

} // namespace jami
