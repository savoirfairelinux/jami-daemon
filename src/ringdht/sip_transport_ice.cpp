/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "sip_transport_ice.h"
#include "ice_transport.h"
#include "manager.h"
#include "logger.h"

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/lock.h>

#include <algorithm>

namespace ring {

static constexpr int POOL_TP_INIT {512};
static constexpr int POOL_TP_INC {512};
static constexpr int TRANSPORT_INFO_LENGTH {64};

static void
sockaddr_to_host_port(pj_pool_t* pool,
                      pjsip_host_port* host_port,
                      const pj_sockaddr* addr)
{
    host_port->host.ptr = (char*) pj_pool_alloc(pool, PJ_INET6_ADDRSTRLEN+4);
    pj_sockaddr_print(addr, host_port->host.ptr, PJ_INET6_ADDRSTRLEN+4, 0);
    host_port->host.slen = pj_ansi_strlen(host_port->host.ptr);
    host_port->port = pj_sockaddr_get_port(addr);
}

SipIceTransport::SipIceTransport(pjsip_endpoint* endpt, pj_pool_t& /* pool */,
                                 long /* t_type */,
                                 const std::shared_ptr<IceTransport>& ice,
                                 int comp_id)
    : pool_(nullptr, pj_pool_release)
    , rxPool_(nullptr, pj_pool_release)
    , trData_()
    , rdata_()
    , ice_(ice)
    , comp_id_(comp_id)
{
    trData_.self = this;

    if (not ice or not ice->isRunning())
        throw std::logic_error("ice transport must exist and negotiation completed");

    RING_DBG("SipIceTransport@%p {tr=%p}", this, &trData_.base);
    auto& base = trData_.base;

    pool_.reset(pjsip_endpt_create_pool(endpt, "SipIceTransport.pool", POOL_TP_INIT, POOL_TP_INC));
    if (not pool_)
        throw std::bad_alloc();
    auto pool = pool_.get();

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "SipIceTransport");
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool;

    rdata_.tp_info.pool = pool;

    // FIXME: not destroyed in case of exception
    if (pj_atomic_create(pool, 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    // FIXME: not destroyed in case of exception
    if (pj_lock_create_recursive_mutex(pool, "SipIceTransport.mutex", &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    auto remote = ice->getRemoteAddress(comp_id);
    RING_DBG("SipIceTransport: remote is %s", remote.toString(true).c_str());
    pj_sockaddr_cp(&base.key.rem_addr, remote.pjPtr());
    base.key.type = PJSIP_TRANSPORT_UDP;//t_type;
    base.type_name = (char*)pjsip_transport_get_type_name((pjsip_transport_type_e)base.key.type);
    base.flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)base.key.type);
    base.info = (char*) pj_pool_alloc(pool, TRANSPORT_INFO_LENGTH);

    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_ansi_snprintf(base.info, TRANSPORT_INFO_LENGTH, "%s to %s",
                     base.type_name,
                     pj_sockaddr_print(remote.pjPtr(), print_addr,
                                       sizeof(print_addr), 3));
    base.addr_len = remote.getLength();
    base.dir = PJSIP_TP_DIR_NONE;//is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;
    base.data = nullptr;

    /* Set initial local address */
    auto local = ice->getDefaultLocalAddress();
    pj_sockaddr_cp(&base.local_addr, local.pjPtr());

    sockaddr_to_host_port(pool, &base.local_name, &base.local_addr);
    sockaddr_to_host_port(pool, &base.remote_name, remote.pjPtr());

    base.send_msg = [](pjsip_transport *transport,
                       pjsip_tx_data *tdata,
                       const pj_sockaddr_t *rem_addr, int addr_len,
                       void *token, pjsip_transport_callback callback) {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_WARN("SipIceTransport@%p: shutdown", this_);
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_WARN("SipIceTransport@%p: destroy", this_);
        delete this_;
        return PJ_SUCCESS;
    };

    /* Init rdata */
    rxPool_.reset(pjsip_endpt_create_pool(base.endpt,
                                          "SipIceTransport.rtd%p",
                                          PJSIP_POOL_RDATA_LEN,
                                          PJSIP_POOL_RDATA_INC));
    if (not rxPool_)
        throw std::bad_alloc();
    auto rx_pool = rxPool_.get();

    rdata_.tp_info.pool = rx_pool;
    rdata_.tp_info.transport = &base;
    rdata_.tp_info.tp_data = this;
    rdata_.tp_info.op_key.rdata = &rdata_;
    pj_ioqueue_op_key_init(&rdata_.tp_info.op_key.op_key, sizeof(pj_ioqueue_op_key_t));
    rdata_.pkt_info.src_addr = base.key.rem_addr;
    rdata_.pkt_info.src_addr_len = sizeof(rdata_.pkt_info.src_addr);
    auto rem_addr = &base.key.rem_addr;
    pj_sockaddr_print(rem_addr, rdata_.pkt_info.src_name, sizeof(rdata_.pkt_info.src_name), 0);
    rdata_.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);
    rdata_.pkt_info.len  = 0;
    rdata_.pkt_info.zero = 0;

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");
    is_registered_ = true;

    Manager::instance().registerEventHandler((uintptr_t)this,
                                             [this]{ loop(); });
}

SipIceTransport::~SipIceTransport()
{
    RING_DBG("~SipIceTransport@%p", this);
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    pj_lock_destroy(trData_.base.lock);
    pj_atomic_destroy(trData_.base.ref_cnt);
    RING_DBG("destroying SipIceTransport@%p", this);
}

void
SipIceTransport::loop()
{
    while (ice_->getNextPacketSize(comp_id_) > 0)
        onRecv();
}

IpAddr
SipIceTransport::getLocalAddress() const
{
    return ice_->getLocalAddress(comp_id_);
}

pj_status_t
SipIceTransport::send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                      int addr_len, void *token,
                      pjsip_transport_callback callback)
{
    /* Sanity check */
    PJ_ASSERT_RETURN(tdata, PJ_EINVAL);

    /* Check that there's no pending operation associated with the tdata */
    PJ_ASSERT_RETURN(tdata->op_key.tdata == nullptr, PJSIP_EPENDINGTX);

    /* Check the address is supported */
    PJ_ASSERT_RETURN(rem_addr && (addr_len==sizeof(pj_sockaddr_in) ||
                                  addr_len==sizeof(pj_sockaddr_in6)),
                     PJ_EINVAL);

    /* Init op key. */
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;

    auto buf_sz = tdata->buf.cur - tdata->buf.start;
    auto size = ice_->send(comp_id_, (uint8_t*)tdata->buf.start, buf_sz);
    if (size > 0) {
        if (size < buf_sz) {
            std::move(tdata->buf.start + size,
                      tdata->buf.start + buf_sz,
                      tdata->buf.start);
            tdata->buf.cur -= size;
        }
        tdata->op_key.tdata = nullptr;
    } else
        return PJ_EUNKNOWN;

    return PJ_SUCCESS;
}

void
SipIceTransport::onRecv()
{
    rdata_.pkt_info.len += ice_->recv(comp_id_,
        (uint8_t*)rdata_.pkt_info.packet+rdata_.pkt_info.len,
        sizeof(rdata_.pkt_info.packet)-rdata_.pkt_info.len);
    rdata_.pkt_info.zero = 0;
    pj_gettimeofday(&rdata_.pkt_info.timestamp);

    auto eaten = pjsip_tpmgr_receive_packet(rdata_.tp_info.transport->tpmgr, &rdata_);

    /* Move unprocessed data to the front of the buffer */
    auto rem = rdata_.pkt_info.len - eaten;
    if (rem > 0 && rem != rdata_.pkt_info.len) {
        std::move(rdata_.pkt_info.packet + eaten,
                  rdata_.pkt_info.packet + eaten + rem,
                  rdata_.pkt_info.packet);
    }
    rdata_.pkt_info.len = rem;

    /* Reset pool */
    pj_pool_reset(rdata_.tp_info.pool);
}

} // namespace ring
