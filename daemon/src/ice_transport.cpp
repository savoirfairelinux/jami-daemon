/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "ice_transport.h"
#include "ice_socket.h"
#include "logger.h"
#include "sip/sip_utils.h"
#include "manager.h"

#include <pjlib.h>
#include <utility>
#include <algorithm>
#include <sstream>

#define TRY(ret) do {      \
        if ((ret) != PJ_SUCCESS)                             \
            throw std::runtime_error(#ret " failed");      \
    } while (0)

namespace sfl {

// GLOBALS (blame PJSIP)

static pj_caching_pool g_cp_;
static pj_pool_t*      g_pool_ = nullptr;

static void
register_thread() {
    // We have to register the external thread so it could access the pjsip frameworks
    if (!pj_thread_is_registered()) {
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
        static thread_local pj_thread_desc desc;
        static thread_local pj_thread_t *this_thread;
#else
        static __thread pj_thread_desc desc;
        static __thread pj_thread_t *this_thread;
#endif
        SFL_DBG("Registering thread");
        pj_thread_register(NULL, desc, &this_thread);
    }
}

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
        SFL_WARN("NULL IceTransport");
}

void
IceTransport::cb_on_ice_complete(pj_ice_strans* ice_st,
                                 pj_ice_strans_op op,
                                 pj_status_t status)
{
    if (auto tr = static_cast<IceTransport*>(pj_ice_strans_get_user_data(ice_st)))
        tr->onComplete(ice_st, op, status);
    else
        SFL_WARN("NULL IceTransport");
}

IceTransport::IceTransport(const char* name, int component_count,
                           IceTransportCompleteCb on_initdone_cb,
                           IceTransportCompleteCb on_negodone_cb)
    : on_initdone_cb_(on_initdone_cb)
    , on_negodone_cb_(on_negodone_cb)
    , component_count_(component_count)
    , compIO_(component_count)
{
    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    pj_ice_strans_cb icecb;

    pj_bzero(&icecb, sizeof(icecb));
    icecb.on_rx_data = cb_on_rx_data;
    icecb.on_ice_complete = cb_on_ice_complete;

    pj_ice_strans *icest = nullptr;
    pj_status_t status = pj_ice_strans_create(name,
                                              iceTransportFactory.getIceCfg(),
                                              component_count, this, &icecb,
                                              &icest);
    if (status != PJ_SUCCESS || icest == nullptr)
        throw std::runtime_error("pj_ice_strans_create() failed");
}

void
IceTransport::onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                         pj_status_t status)
{
    const char *opname =
        (op==PJ_ICE_STRANS_OP_INIT? "initialization" :
         (op==PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op"));

    if (!icest_.get())
        icest_.reset(ice_st);

    const bool done = status == PJ_SUCCESS;

    if (!done) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        SFL_ERR("ICE %s failed: %s", opname, errmsg);
    }
    SFL_DBG("ICE %s with %s", opname, done?"success":"error");
    if (op == PJ_ICE_STRANS_OP_INIT and on_initdone_cb_) {
        on_initdone_cb_(*this, done);
    } else if (op == PJ_ICE_STRANS_OP_NEGOTIATION and on_negodone_cb_) {
        on_negodone_cb_(*this, done);
        running = done;
    }
}

void
IceTransport::getUFragPwd()
{
    pj_str_t local_ufrag, local_pwd;
    pj_ice_strans_get_ufrag_pwd(icest_.get(), &local_ufrag, &local_pwd,
                                NULL, NULL);
    local_ufrag_.assign(local_ufrag.ptr, local_ufrag.slen);
    local_pwd_.assign(local_pwd.ptr, local_pwd.slen);
    SFL_DBG("ICE: local {ufrag:%s, pwd:%s}", local_ufrag_.c_str(), local_pwd_.c_str());
}

void
IceTransport::getDefaultCanditates()
{
    for (unsigned i=0; i < component_count_; ++i)
        pj_ice_strans_get_def_cand(icest_.get(), i+1, &cand_[i]);
}

void
IceTransport::createIceSession(pj_ice_sess_role role)
{
    if (pj_ice_strans_has_sess(icest_.get())) {
        SFL_ERR("Session already created");
        return;
    }

    if (pj_ice_strans_init_ice(icest_.get(), role, NULL, NULL) != PJ_SUCCESS) {
        SFL_ERR("pj_ice_strans_init_ice() failed");
        return;
    }

    // Fetch some information on local configuration
    getUFragPwd();
    getDefaultCanditates();
}

void
IceTransport::setInitiatorSession()
{
    createIceSession(PJ_ICE_SESS_ROLE_CONTROLLING);
    SFL_DBG("ICE master session set");
}

void
IceTransport::setSlaveSession()
{
    createIceSession(PJ_ICE_SESS_ROLE_CONTROLLED);
    SFL_DBG("ICE slave session set");
}

void
IceTransport::unsetSession()
{
    running = false;

    if (not pj_ice_strans_has_sess(icest_.get())) {
        SFL_ERR("Session not created yet");
        return;
    }

    if (pj_ice_strans_stop_ice(icest_.get()) != PJ_SUCCESS) {
        SFL_ERR("pj_ice_strans_init_ice() failed");
        return;
    }
}

bool
IceTransport::start(const Attribute& rem_attrs,
                    const std::vector<IceCandidate>& rem_candidates)
{
    pj_str_t ufrag, pwd;
    SFL_DBG("ICE: starting negotiation (%u candidates)", rem_candidates.size());
    auto status = pj_ice_strans_start_ice(icest_.get(),
                                          pj_cstr(&ufrag, rem_attrs.ufrag.c_str()),
                                          pj_cstr(&pwd, rem_attrs.pwd.c_str()),
                                          rem_candidates.size(),
                                          rem_candidates.data());
    if (status != PJ_SUCCESS) {
        SFL_ERR("ICE: start failed");
        sip_utils::sip_strerror(status);
        return false;
    }
    return true;
}

IpAddr
IceTransport::getLocalAddress() const
{
    return cand_[0].addr;
}

IpAddr
IceTransport::getRemote(unsigned comp_id) const
{
    auto sess = pj_ice_strans_get_valid_pair(icest_.get(), comp_id);
    if (!sess)
        return {};
    return sess->rcand->addr;
}

void
IceTransport::setRemoteDestination(const std::string& ip_addr, uint16_t port)
{
    int af;
    if (ip_addr.find(':') != ip_addr.npos)
        af = pj_AF_INET6();
    else
        af = pj_AF_INET();

    pj_str_t ip;
    if (pj_sockaddr_init(af, &remoteAddr_, pj_cstr(&ip, ip_addr.c_str()), port))
        SFL_ERR("invalid IP address: %s", ip_addr.c_str());
}

std::vector<IpAddr>
IceTransport::getLocalPorts() const
{
    std::vector<IpAddr> v;

    for (unsigned i=0; i < component_count_; ++i) {
        // duplicate addresses, as data and control sockets are the same
        v.push_back(cand_[i].addr);
        v.push_back(cand_[i].addr);
    }

    return v;
}

const IceTransport::Attribute
IceTransport::getIceAttributes() const
{
    return {local_ufrag_, local_pwd_};
}

std::vector<std::string>
IceTransport::getIceCandidates(unsigned comp_id) const
{
    std::vector<std::string> res;
    pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
    unsigned cand_cnt = PJ_ARRAY_SIZE(cand);

    if (pj_ice_strans_enum_cands(icest_.get(), comp_id+1, &cand_cnt, cand) != PJ_SUCCESS) {
        SFL_ERR("pj_ice_strans_enum_cands() failed");
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

void
IceTransport::onReceiveData(unsigned comp_id, void *pkt, pj_size_t size)
{
    if (!comp_id or comp_id > component_count_) {
        SFL_ERR("rx: invalid comp_id (%u)", comp_id);
        return;
    }
    if (!size)
        return;
    auto& io = compIO_[comp_id-1];
    std::unique_lock<std::mutex> lk(io.mutex);
    if (io.cb) {
        io.cb((uint8_t*)pkt, size);
    } else {
        io.queue.emplace_back(Packet(pkt, size));
        io.cv.notify_one();
    }
}

bool
IceTransport::getCandidateFromSDP(const std::string line, IceCandidate& cand)
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
        SFL_WARN("ICE: invalid remote candidate line");
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
        SFL_WARN("ICE: invalid remote candidate type '%s'", type);
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
        SFL_ERR("ICE: invalid remote IP address '%s'", ipaddr);
        return false;
    }

    pj_sockaddr_set_port(&cand.addr, (pj_uint16_t)port);
    pj_strdup2(g_pool_, &cand.foundation, foundation);
    return true;
}

ssize_t
IceTransport::recv(int comp_id, unsigned char* buf, size_t len)
{
    register_thread();
    auto& io = compIO_[comp_id];
    std::unique_lock<std::mutex> lk(io.mutex);

    if (io.queue.empty())
        return 0;

    auto& packet = io.queue.front();
    const auto count = std::min(len, packet.datalen);
    std::copy_n(buf, count, packet.data.get());
    io.queue.pop_front();

    return count;
}

void
IceTransport::setOnRecv(unsigned comp_id, IceRecvCb cb)
{
    auto& io = compIO_[comp_id];
    std::unique_lock<std::mutex> lk(io.mutex);
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
    auto status = pj_ice_strans_sendto(icest_.get(), comp_id+1, buf, len,
                                       &remoteAddr_,
                                       pj_sockaddr_get_len(&remoteAddr_));
    if (status != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        SFL_ERR("send failed");
        return -1;
    }
    return len;
}

ssize_t
IceTransport::getNextPacketSize(int comp_id)
{
    auto& io = compIO_[comp_id];
    std::unique_lock<std::mutex> lk(io.mutex);
    if (io.queue.empty()) {
        return 0;
    }
    return io.queue.front().datalen;
}

ssize_t
IceTransport::waitForData(int comp_id, unsigned int timeout)
{
    auto& io = compIO_[comp_id];
    std::unique_lock<std::mutex> lk(io.mutex);
    if (!io.cv.wait_for(lk, std::chrono::milliseconds(timeout),
                        [&io]{ return !io.queue.empty(); })) {
        return 0;
    }
    return io.queue.front().datalen;
}

IceTransportFactory::IceTransportFactory() : ice_cfg_()
{
    pj_caching_pool_init(&g_cp_, NULL, 0);
    g_pool_ = pj_pool_create(&g_cp_.factory, "icetransportpool",
                              512, 512, NULL);
    if (not g_pool_)
        throw std::runtime_error("pj_pool_create() failed");

    pj_ice_strans_cfg_default(&ice_cfg_);
    ice_cfg_.stun_cfg.pf = &g_cp_.factory;

    TRY( pj_timer_heap_create(g_pool_, 100, &ice_cfg_.stun_cfg.timer_heap) );
    TRY( pj_ioqueue_create(g_pool_, 16, &ice_cfg_.stun_cfg.ioqueue) );

    pj_thread_t* thread = nullptr;
    const auto& thread_work = [](void* udata) {
        register_thread();
        return static_cast<IceTransportFactory*>(udata)->processThread();
    };
    TRY( pj_thread_create(g_pool_, "icetransportpool",
                          thread_work, this, 0, 0, &thread) );
    thread_.reset(thread);

    ice_cfg_.af = pj_AF_INET();

    //ice_cfg_.stun.max_host_cands = icedemo.opt.max_host;
    ice_cfg_.opt.aggressive = PJ_FALSE;

    // TODO: STUN server candidate

    // TODO: TURN server candidate
}

IceTransportFactory::~IceTransportFactory()
{
    pj_thread_sleep(500);

    thread_quit_flag_ = PJ_TRUE;
    if (thread_) {
        pj_thread_join(thread_.get());
        thread_.reset();
    }

    if (ice_cfg_.stun_cfg.ioqueue)
        pj_ioqueue_destroy(ice_cfg_.stun_cfg.ioqueue);

    if (ice_cfg_.stun_cfg.timer_heap)
        pj_timer_heap_destroy(ice_cfg_.stun_cfg.timer_heap);

    pj_caching_pool_destroy(&g_cp_);
}

int
IceTransportFactory::processThread()
{
    while (!thread_quit_flag_) {
        handleEvents(500, NULL);
    }

    return 0;
}

int
IceTransportFactory::handleEvents(unsigned max_msec, unsigned *p_count)
{
    enum { MAX_NET_EVENTS = 1 };
    pj_time_val max_timeout = {0, 0};
    pj_time_val timeout = {0, 0};
    unsigned count = 0, net_event_count = 0;
    int c;

    max_timeout.msec = max_msec;

    timeout.sec = timeout.msec = 0;
    c = pj_timer_heap_poll(ice_cfg_.stun_cfg.timer_heap, &timeout);
    if (c > 0)
        count += c;

    pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
    if (timeout.msec >= 1000) timeout.msec = 999;

    if (PJ_TIME_VAL_GT(timeout, max_timeout))
        timeout = max_timeout;

    do {
        c = pj_ioqueue_poll(ice_cfg_.stun_cfg.ioqueue, &timeout);
        if (c < 0) {
            pj_status_t err = pj_get_netos_error();
            pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
            if (p_count)
                *p_count = count;
            return err;
        } else if (c == 0) {
            break;
        } else {
            net_event_count += c;
            timeout.sec = timeout.msec = 0;
        }
    } while (c > 0 && net_event_count < MAX_NET_EVENTS);

    count += net_event_count;
    if (p_count)
        *p_count = count;

    return PJ_SUCCESS;
}


std::shared_ptr<IceTransport>
IceTransportFactory::createTransport(const char* name,
                                     int component_count,
                                     IceTransportCompleteCb&& on_initdone_cb,
                                     IceTransportCompleteCb&& on_negodone_cb)
{
    return std::make_shared<IceTransport>(name, component_count,
                                          std::forward<IceTransportCompleteCb>(on_initdone_cb),
                                          std::forward<IceTransportCompleteCb>(on_negodone_cb));
}

struct in_addr
IceSocket::getAddress() const
{
    return ia_.getAddress();
}

int
IceSocket::connect(const ost::InetAddress& ia, int port)
{
    if (ice_transport_.get())
        return 0;

    // STUB
    ia_ = ia;
    port_ = port;
    //ice_transport_ = Manager::instance().iceTransportPool.getIPV4(ia.getAddress(), port);
    //return static_cast<bool>(ice_transport_) ? 0 : -1;
    return -1;
}

int
IceSocket::connect(const std::string& addr, int port)
{
    if (ice_transport_.get()) {
        ice_transport_->setRemoteDestination(addr, port);
        return 0;
    }
    return -1;
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

}
