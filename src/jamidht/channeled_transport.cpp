/*
 *  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
 *
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

#include "channeled_transport.h"

#include "logger.h"
#include "connectivity/multiplexed_socket.h"
#include "connectivity/sip_utils.h"

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/compat/socket.h>
#include <pj/lock.h>

namespace jami {
namespace tls {

ChanneledSIPTransport::ChanneledSIPTransport(pjsip_endpoint* endpt,
                                             const std::shared_ptr<ChannelSocket>& socket,
                                             onShutdownCb&& cb)
    : socket_(socket)
    , shutdownCb_(std::move(cb))
    , trData_()
    , pool_ {nullptr, pj_pool_release}
    , rxPool_(nullptr, pj_pool_release)
{
    local_ = socket->getLocalAddress();
    remote_ = socket->getRemoteAddress();
    int tp_type = local_.isIpv6() ? PJSIP_TRANSPORT_TLS6 : PJSIP_TRANSPORT_TLS;

    JAMI_DBG("ChanneledSIPTransport@%p {tr=%p}", this, &trData_.base);

    // Init memory
    trData_.self = this; // up-link for PJSIP callbacks

    pool_ = sip_utils::smart_alloc_pool(endpt,
                                        "channeled.pool",
                                        sip_utils::POOL_TP_INIT,
                                        sip_utils::POOL_TP_INC);

    auto& base = trData_.base;
    std::memset(&base, 0, sizeof(base));

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "chan%p", &base);
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool_.get();

    if (pj_atomic_create(pool_.get(), 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool_.get(), "chan", &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    if (not local_) {
        JAMI_ERR("Invalid local address");
        throw std::runtime_error("Invalid local address");
    }
    if (not remote_) {
        JAMI_ERR("Invalid remote address");
        throw std::runtime_error("Invalid remote address");
    }

    pj_sockaddr_cp(&base.key.rem_addr, remote_.pjPtr());
    base.key.type = tp_type;
    auto reg_type = static_cast<pjsip_transport_type_e>(tp_type);
    base.type_name = const_cast<char*>(pjsip_transport_get_type_name(reg_type));
    base.flag = pjsip_transport_get_flag_from_type(reg_type);
    base.info = static_cast<char*>(pj_pool_alloc(pool_.get(), sip_utils::TRANSPORT_INFO_LENGTH));

    auto remote_addr = remote_.toString();
    pj_ansi_snprintf(base.info,
                     sip_utils::TRANSPORT_INFO_LENGTH,
                     "%s to %s",
                     base.type_name,
                     remote_addr.c_str());
    base.addr_len = remote_.getLength();
    base.dir = PJSIP_TP_DIR_NONE;

    // Set initial local address
    pj_sockaddr_cp(&base.local_addr, local_.pjPtr());

    sip_utils::sockaddr_to_host_port(pool_.get(), &base.local_name, &base.local_addr);
    sip_utils::sockaddr_to_host_port(pool_.get(), &base.remote_name, remote_.pjPtr());

    // Init transport callbacks
    base.send_msg = [](pjsip_transport* transport,
                       pjsip_tx_data* tdata,
                       const pj_sockaddr_t* rem_addr,
                       int addr_len,
                       void* token,
                       pjsip_transport_callback callback) -> pj_status_t {
        auto* this_ = reinterpret_cast<ChanneledSIPTransport*>(
            reinterpret_cast<TransportData*>(transport)->self);
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport* transport) -> pj_status_t {
        auto* this_ = reinterpret_cast<ChanneledSIPTransport*>(
            reinterpret_cast<TransportData*>(transport)->self);
        JAMI_DEBUG("ChanneledSIPTransport@{} tr={} rc={:d}: shutdown",
                 fmt::ptr(this_),
                 fmt::ptr(transport),
                 pj_atomic_get(transport->ref_cnt));
        if (this_->socket_)
            this_->socket_->shutdown();
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport* transport) -> pj_status_t {
        auto* this_ = reinterpret_cast<ChanneledSIPTransport*>(
            reinterpret_cast<TransportData*>(transport)->self);
        JAMI_DEBUG("ChanneledSIPTransport@{}: destroying", fmt::ptr(this_));
        delete this_;
        return PJ_SUCCESS;
    };

    // Init rdata_
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));
    rxPool_ = sip_utils::smart_alloc_pool(endpt,
                                          "channeled.rxPool",
                                          PJSIP_POOL_RDATA_LEN,
                                          PJSIP_POOL_RDATA_LEN);
    rdata_.tp_info.pool = rxPool_.get();
    rdata_.tp_info.transport = &base;
    rdata_.tp_info.tp_data = this;
    rdata_.tp_info.op_key.rdata = &rdata_;
    pj_ioqueue_op_key_init(&rdata_.tp_info.op_key.op_key, sizeof(pj_ioqueue_op_key_t));
    rdata_.pkt_info.src_addr = base.key.rem_addr;
    rdata_.pkt_info.src_addr_len = sizeof(rdata_.pkt_info.src_addr);
    auto rem_addr = &base.key.rem_addr;
    pj_sockaddr_print(rem_addr, rdata_.pkt_info.src_name, sizeof(rdata_.pkt_info.src_name), 0);
    rdata_.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);

    // Register callbacks
    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");
}

void
ChanneledSIPTransport::start()
{
    // Link to Channel Socket
    socket_->setOnRecv([this](const uint8_t* buf, size_t len) {
        pj_gettimeofday(&rdata_.pkt_info.timestamp);
        size_t remaining {len};
        while (remaining) {
            // Build rdata
            size_t added = std::min(remaining,
                                    (size_t) PJSIP_MAX_PKT_LEN - (size_t) rdata_.pkt_info.len);
            std::copy_n(buf, added, rdata_.pkt_info.packet + rdata_.pkt_info.len);
            rdata_.pkt_info.len += added;
            buf += added;
            remaining -= added;

            // Consume packet
            auto eaten = pjsip_tpmgr_receive_packet(trData_.base.tpmgr, &rdata_);
            if (eaten == rdata_.pkt_info.len) {
                rdata_.pkt_info.len = 0;
            } else if (eaten > 0) {
                memmove(rdata_.pkt_info.packet, rdata_.pkt_info.packet + eaten, eaten);
                rdata_.pkt_info.len -= eaten;
            }
            pj_pool_reset(rdata_.tp_info.pool);
        }
        return len;
    });
    socket_->onShutdown([this] {
        disconnected_ = true;
        if (auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr)) {
            JAMI_WARN("[SIPS] process disconnect event");
            pjsip_transport_state_info state_info;
            std::memset(&state_info, 0, sizeof(state_info));
            state_info.status = PJ_SUCCESS;
            (*state_cb)(&trData_.base, PJSIP_TP_STATE_DISCONNECTED, &state_info);
        }
        shutdownCb_();
    });
}

ChanneledSIPTransport::~ChanneledSIPTransport()
{
    JAMI_DBG("~ChanneledSIPTransport@%p {tr=%p}", this, &trData_.base);
    auto base = getTransportBase();

    // Here, we reset callbacks in ChannelSocket to avoid to call it after destruction
    // ChanneledSIPTransport is managed by pjsip, so we don't have any weak_ptr available
    socket_->setOnRecv([](const uint8_t*, size_t len) { return len; });
    socket_->onShutdown([]() {});
    // Stop low-level transport first
    socket_->shutdown();
    socket_.reset();

    // If delete not trigged by pjsip_transport_destroy (happen if objet not given to pjsip)
    if (not base->is_shutdown and not base->is_destroying)
        pjsip_transport_shutdown(base);

    pj_lock_destroy(base->lock);
    pj_atomic_destroy(base->ref_cnt);
    JAMI_DBG("~ChanneledSIPTransport@%p {tr=%p} bye", this, &trData_.base);
}

pj_status_t
ChanneledSIPTransport::send(pjsip_tx_data* tdata,
                            const pj_sockaddr_t* rem_addr,
                            int addr_len,
                            void*,
                            pjsip_transport_callback)
{
    // Sanity check
    PJ_ASSERT_RETURN(tdata, PJ_EINVAL);

    // Check that there's no pending operation associated with the tdata
    PJ_ASSERT_RETURN(tdata->op_key.tdata == nullptr, PJSIP_EPENDINGTX);

    // Check the address is supported
    PJ_ASSERT_RETURN(rem_addr
                         and (addr_len == sizeof(pj_sockaddr_in)
                              or addr_len == sizeof(pj_sockaddr_in6)),
                     PJ_EINVAL);

    // Check in we are able to send it in synchronous way first
    std::size_t size = tdata->buf.cur - tdata->buf.start;
    if (socket_) {
        std::error_code ec;
        socket_->write(reinterpret_cast<const uint8_t*>(tdata->buf.start), size, ec);
        if (!ec)
            return PJ_SUCCESS;
    }
    return PJ_EINVAL;
}

} // namespace tls
} // namespace jami
