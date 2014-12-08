/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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

#include "sip_transport_ice.h"

#include "ice_transport.h"

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/lock.h>

#include <algorithm>

SipIceTransport::SipIceTransport(pjsip_endpoint *endpt, pj_pool_t& pool, long t_type, const std::shared_ptr<sfl::IceTransport>& ice, int comp_id)
 : ice_(ice), comp_id_(comp_id)
{
    if (not ice->isRunning())
        throw std::logic_error("ice transport must be running");

    SFL_DBG("Creating SipIceTransport");

    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);

    base.pool = &pool; //pjsip_endpt_create_pool(endpt, "ice", POOL_TP_INIT, POOL_TP_INC);
    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "ice");
    auto status = pj_atomic_create(base.pool, 0, &base.ref_cnt);
    if (status != PJ_SUCCESS) {
        throw std::runtime_error("Can't create PJSIP atomic.");
    }
    status = pj_lock_create_recursive_mutex(base.pool, "ice", &base.lock);
    if (status != PJ_SUCCESS) {
        throw std::runtime_error("Can't create PJSIP mutex.");
    }

    base.key.type = t_type;

    auto remote = ice->getRemote(comp_id);
    SFL_DBG("SipIceTransport: remote is %s", remote.toString(true).c_str());

    pj_sockaddr_cp(&base.key.rem_addr, remote.pjPtr());
    base.type_name = (char*)pjsip_transport_get_type_name((pjsip_transport_type_e)base.key.type);
    base.flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)base.key.type);
    base.info = (char*) pj_pool_alloc(base.pool, 64);

    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_ansi_snprintf(base.info, 64, "%s to %s",
                     base.type_name,
                     pj_sockaddr_print(remote.pjPtr(), print_addr,
                                       sizeof(print_addr), 3));
    base.addr_len = remote.getLength();
    base.dir = PJSIP_TP_DIR_NONE;//is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;

    /* Set initial local address */
    auto local = ice->getLocalAddress();
    pj_sockaddr_cp(&base.local_addr, local.pjPtr());

    base.send_msg = [](pjsip_transport *transport,
                    pjsip_tx_data *tdata,
                    const pj_sockaddr_t *rem_addr, int addr_len,
                    void *token, pjsip_transport_callback callback) {
        auto this_ = reinterpret_cast<SipIceTransport*>(transport);
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport *transport){
        auto this_ = reinterpret_cast<SipIceTransport*>(transport);
        return this_->shutdown();
    };
    base.destroy = [](pjsip_transport *transport){
        auto this_ = reinterpret_cast<SipIceTransport*>(transport);
        return this_->destroy();
    };

    /* Init rdata */
    auto rx_pool = pjsip_endpt_create_pool(base.endpt,
                   "rtd%p",
                   PJSIP_POOL_RDATA_LEN,
                   PJSIP_POOL_RDATA_INC);
    if (!rx_pool)
        throw std::bad_alloc();

    rdata.tp_info.pool = rx_pool;
    rdata.tp_info.transport = &base;
    rdata.tp_info.tp_data = this;
    rdata.tp_info.op_key.rdata = &rdata;
    pj_ioqueue_op_key_init(&rdata.tp_info.op_key.op_key, sizeof(pj_ioqueue_op_key_t));
    rdata.pkt_info.src_addr = base.key.rem_addr;
    rdata.pkt_info.src_addr_len = sizeof(rdata.pkt_info.src_addr);
    auto rem_addr = &base.key.rem_addr;
    pj_sockaddr_print(rem_addr, rdata.pkt_info.src_name, sizeof(rdata.pkt_info.src_name), 0);
    rdata.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);


    pjsip_transport_register(base.tpmgr, &base);
    is_registered_ = true;

    using namespace std::placeholders;
    ice->setOnRecv(comp_id_, std::bind(&SipIceTransport::onRecv, this, _1, _2));
}

SipIceTransport::~SipIceTransport()
{
    if (rdata.tp_info.pool) {
        pj_pool_release(rdata.tp_info.pool);
        rdata.tp_info.pool = nullptr;
    }

    if (base.lock) {
        pj_lock_destroy(base.lock);
        base.lock = nullptr;
    }

    if (base.ref_cnt) {
        pj_atomic_destroy(base.ref_cnt);
        base.ref_cnt = nullptr;
    }
}

pj_status_t
SipIceTransport::send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                int addr_len,
                void *token,
                pjsip_transport_callback callback)
{
    SFL_WARN("SipIceTransport::send");
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

    auto size = ice_->send(comp_id_, (uint8_t*)tdata->buf.start, tdata->buf.cur - tdata->buf.start);
    if (size > 0) {
        // TODO
    }
    return PJ_SUCCESS;
}

ssize_t
SipIceTransport::onRecv(uint8_t* buf, size_t len)
{
    SFL_WARN("SipIceTransport::onRecv");
    auto max_size = std::min(sizeof(rdata.pkt_info.packet) - rdata.pkt_info.len, len);
    std::copy_n(buf, max_size, (uint8_t*)rdata.pkt_info.packet + rdata.pkt_info.len);
    rdata.pkt_info.len += max_size;
    rdata.pkt_info.zero = 0;
    pj_gettimeofday(&rdata.pkt_info.timestamp);

    auto eaten = pjsip_tpmgr_receive_packet(rdata.tp_info.transport->tpmgr, &rdata);

    /* Move unprocessed data to the front of the buffer */
    auto rem = rdata.pkt_info.len - eaten;
    if (rem > 0 && rem != rdata.pkt_info.len) {
        std::move(rdata.pkt_info.packet + eaten, rdata.pkt_info.packet + eaten + rem, rdata.pkt_info.packet);
        //pj_memmove(rdata.pkt_info.packet, rdata.pkt_info.packet + eaten, rem);
    }
    rdata.pkt_info.len = rem;

    /* Reset pool. */
    pj_pool_reset(rdata.tp_info.pool);
}

pj_status_t
SipIceTransport::shutdown()
{
    SFL_WARN("SIP transport ICE: shutdown");
}

pj_status_t
SipIceTransport::destroy()
{
    SFL_WARN("SIP transport ICE: destroy");
    is_registered_ = false;
    pjsip_transport_destroy(&base);
}
