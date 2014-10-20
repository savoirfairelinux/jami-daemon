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
#include "logger.h"

#include <pjlib.h>
#include <utility>
#include <sstream>

#define TRY(ret) do {      \
        if ((ret) != PJ_SUCCESS)                             \
            throw std::runtime_error(#ret " failed");      \
    } while (0)

namespace sfl {

// GLOBALS (blame PJSIP)

static pj_caching_pool g_cp_;
static pj_pool_t*      g_pool_ = nullptr;

void
ICETransport::cb_on_rx_data(pj_ice_strans *ice_st,
                            unsigned comp_id,
                            void *pkt, pj_size_t size,
                            const pj_sockaddr_t *src_addr,
                            unsigned src_addr_len)
{
    ICETransport* tr = static_cast<ICETransport*>(pj_ice_strans_get_user_data(ice_st));

    if (not tr) {
        SFL_WARN("NULL ICETransport");
        return;
    }

    tr->onReceiveData(comp_id, pkt, size);
}

void
ICETransport::cb_on_ice_complete(pj_ice_strans *ice_st,
                                 pj_ice_strans_op op,
                                 pj_status_t status)
{
    auto tr = static_cast<ICETransport*>(pj_ice_strans_get_user_data(ice_st));

    if (not tr) {
        SFL_WARN("NULL ICETransport");
        return;
    }

    tr->onComplete(ice_st, op, status);
}

ICETransport::ICETransport(const char* name, int component_count,
                           ICETransportCompleteCb on_complete_cb,
                           ICETransportPool& tp)
    : on_complete_cb_(on_complete_cb), component_count_(component_count)
{
    pj_ice_strans_cb icecb;

    pj_bzero(&icecb, sizeof(icecb));
    icecb.on_rx_data = cb_on_rx_data;
    icecb.on_ice_complete = cb_on_ice_complete;

    pj_ice_strans *icest = nullptr;
    pj_status_t status = pj_ice_strans_create(name, tp.getICECfg(),
                                              component_count, this, &icecb,
                                              &icest);
    if (status != PJ_SUCCESS || icest == nullptr)
        throw std::runtime_error("pj_ice_strans_create() failed");
}

void
ICETransport::onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                         pj_status_t status)
{
    const char *opname =
        (op==PJ_ICE_STRANS_OP_INIT? "initialization" :
         (op==PJ_ICE_STRANS_OP_NEGOTIATION ? "negotiation" : "unknown_op"));

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));

        SFL_ERR("ICE %s failed: %s", opname, errmsg);
        return;
    }

    icest_.reset(ice_st);
    complete_ = true;

    SFL_DBG("ICE %s done", opname);

    if (on_complete_cb_)
        on_complete_cb_(*this);
}

void
ICETransport::getUFragPwd()
{
    pj_str_t local_ufrag, local_pwd;
    pj_ice_strans_get_ufrag_pwd(icest_.get(), &local_ufrag, &local_pwd,
                                NULL, NULL);
    local_ufrag_.assign(local_ufrag.ptr, local_ufrag.slen);
    local_pwd_.assign(local_pwd.ptr, local_pwd.slen);
    SFL_DBG("ICE: ufrag:%s, pwd:%s", local_ufrag_.c_str(), local_pwd_.c_str());
}

void
ICETransport::getDefaultCanditates()
{
    for (unsigned i=0; i < component_count_; ++i)
        pj_ice_strans_get_def_cand(icest_.get(), i+1, &cand_[i]);
}

void
ICETransport::createICESession(pj_ice_sess_role role)
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
ICETransport::setInitiatorSession()
{
    createICESession(PJ_ICE_SESS_ROLE_CONTROLLING);
    SFL_DBG("ICE master session set");
}

void
ICETransport::setSlaveSession()
{
    createICESession(PJ_ICE_SESS_ROLE_CONTROLLED);
    SFL_DBG("ICE slave session set");
}

void
ICETransport::unsetSession()
{
    if (not pj_ice_strans_has_sess(icest_.get())) {
        SFL_ERR("Session not created yet");
        return;
    }

    if (pj_ice_strans_stop_ice(icest_.get())  != PJ_SUCCESS) {
        SFL_ERR("pj_ice_strans_init_ice() failed");
        return;
    }
}

bool
ICETransport::start(const Attribute& rem_attrs,
                    const std::vector<Candidate>& rem_candidates)
{
    pj_str_t ufrag;
    pj_str_t pwd;

    pj_strset(&ufrag, const_cast<char*>(rem_attrs.ufrag.c_str()), rem_attrs.ufrag.size());
    pj_strset(&pwd, const_cast<char*>(rem_attrs.pwd.c_str()), rem_attrs.pwd.size());

    return pj_ice_strans_start_ice(icest_.get(), &ufrag, &pwd,
                                   rem_candidates.size(),
                                   rem_candidates.data()) == PJ_SUCCESS;
}

pj_sockaddr
ICETransport::getLocalAddress() const
{
    return cand_[0].addr;
}

std::vector<pj_sockaddr>
ICETransport::getLocalPorts() const
{
    std::vector<pj_sockaddr> v;

    for (unsigned i=0; i < component_count_; ++i) {
        // duplicate addresses, as data and control sockets are the same
        v.push_back(cand_[i].addr);
        v.push_back(cand_[i].addr);
    }

    return v;
}

const ICETransport::Attribute
ICETransport::getICEAttributes() const
{
    return {local_ufrag_, local_pwd_};
}

std::vector<std::string>
ICETransport::getICECandidates(unsigned comp_id) const
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
ICETransport::onReceiveData(unsigned comp_id, void *pkt, pj_size_t size)
{}

static int
tp_worker_thread(void* udata)
{
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

    return static_cast<ICETransportPool*>(udata)->processThread();
}

ICETransportPool::ICETransportPool() : ice_cfg_()
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
    TRY( pj_thread_create(g_pool_, "icetransportpool", &tp_worker_thread,
                          this, 0, 0, &thread) );
    thread_.reset(thread);

    ice_cfg_.af = pj_AF_INET();

    //ice_cfg_.stun.max_host_cands = icedemo.opt.max_host;
    //ice_cfg_.opt.aggressive = PJ_FALSE;

    // TODO: STUN server candidate

    // TODO: TURN server candidate
}

ICETransportPool::~ICETransportPool()
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
ICETransportPool::processThread()
{
    while (!thread_quit_flag_) {
        handleEvents(500, NULL);
    }

    return 0;
}

int
ICETransportPool::handleEvents(unsigned max_msec, unsigned *p_count)
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


std::shared_ptr<ICETransport>
ICETransportPool::createTransport(const char* name,
                                  int component_count,
                                  ICETransportCompleteCb&& on_complete_cb)
{
    auto transport = std::make_shared<ICETransport>(name, component_count,
                                                    std::forward<ICETransportCompleteCb>(on_complete_cb),
                                                    *this);
    transports_.push_back(transport);
    return transport;
}

}
