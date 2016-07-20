/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "multistream_siptransport.h"

#include "manager.h"
#include "sip/sip_utils.h"
#include "logger.h"
#include "intrin.h"
#include "data_transfer.h"

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/compat/socket.h>
#include <pj/lock.h>

#include <algorithm>
#include <cstring> // std::memset

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

MultiStreamSipTransport::MultiStreamSipTransport(pjsip_endpoint* endpt,
                                                 std::shared_ptr<ReliableSocket::DataStream> stream)
    : trData_ ()
    , pool_  {nullptr, pj_pool_release}
    , rxPool_ (nullptr, pj_pool_release)
    , stream_ (stream)
{
    RING_DBG("MultiStreamSipTransport@%p {PjTr=%p}", this, &trData_.base);

    trData_.self = this; // up-link for PJSIP C callbacks

    pool_ = std::move(sip_utils::smart_alloc_pool(endpt, "SipsIceTransport.pool",
                                                  POOL_TP_INIT, POOL_TP_INC));

    auto& base = trData_.base;
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "MultiStreamSipTransport");
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool_.get();

    if (pj_atomic_create(pool_.get(), 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool_.get(), "SipsIceTransport.mutex",
                                       &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    IpAddr remote_addr {"10.0.0.2:1"}; // TODO: remote address?
    pj_sockaddr_cp(&base.key.rem_addr, remote_addr.pjPtr());
    base.key.type = PJSIP_TRANSPORT_TLS;
    base.type_name = (char*)pjsip_transport_get_type_name((pjsip_transport_type_e)base.key.type);
    base.flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)base.key.type);
    base.info = (char*) pj_pool_alloc(pool_.get(), TRANSPORT_INFO_LENGTH);

    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_ansi_snprintf(base.info, TRANSPORT_INFO_LENGTH, "%s to %s", base.type_name,
                     pj_sockaddr_print(remote_addr.pjPtr(), print_addr, sizeof(print_addr), 3));
    base.addr_len = sizeof(pj_sockaddr_in); // TODO: ???
    base.dir = PJSIP_TP_DIR_NONE;
    base.data = nullptr;

    IpAddr local_addr {"10.0.0.1:1"}; // TODO: local address?
    pj_sockaddr_cp(&base.local_addr, local_addr.pjPtr());

    sockaddr_to_host_port(pool_.get(), &base.local_name, &base.local_addr);
    sockaddr_to_host_port(pool_.get(), &base.remote_name, remote_addr.pjPtr());

    base.send_msg = [](pjsip_transport *transport,
                       pjsip_tx_data *tdata,
                       const pj_sockaddr_t *rem_addr, int addr_len,
                       void *token, pjsip_transport_callback callback) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_DBG("MultiStreamSipTransport@%p: shutdown", this_);
        {
            // Flush pending state changes and rx packet before shutdown
            // or pjsip callbacks will crash

            std::unique_lock<std::mutex> lk{this_->stateChangeEventsMutex_};
            this_->stateChangeEvents_.clear();
            this_->stream_->close();
        }
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        RING_DBG("MultiStreamSipTransport@%p: destroying", this_);
        delete this_; // we're owned by PJSIP
        return PJ_SUCCESS;
    };

    /* Init rdata_ */
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));
    rxPool_ = std::move(sip_utils::smart_alloc_pool(endpt, "MultiStreamSipTransport.rxPool",
                                                    PJSIP_POOL_RDATA_LEN, PJSIP_POOL_RDATA_LEN));
    rdata_.tp_info.pool = rxPool_.get();
    rdata_.tp_info.transport = &base;
    rdata_.tp_info.tp_data = this;
    rdata_.tp_info.op_key.rdata = &rdata_;
    pj_ioqueue_op_key_init(&rdata_.tp_info.op_key.op_key,
                           sizeof(pj_ioqueue_op_key_t));
    rdata_.pkt_info.src_addr = base.key.rem_addr;
    rdata_.pkt_info.src_addr_len = sizeof(rdata_.pkt_info.src_addr);
    auto rem_addr = &base.key.rem_addr;
    pj_sockaddr_print(rem_addr, rdata_.pkt_info.src_name,
                      sizeof(rdata_.pkt_info.src_name), 0);
    rdata_.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);
    readBufferPtr_ = rdata_.pkt_info.packet;

    std::memset(&localCertInfo_, 0, sizeof(pj_ssl_cert_info));
    std::memset(&remoteCertInfo_, 0, sizeof(pj_ssl_cert_info));

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register MultiStreamSipTransport on PJSIP");

    Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });

    updateTransportState(PJSIP_TP_STATE_CONNECTED);
}

MultiStreamSipTransport::~MultiStreamSipTransport()
{
    RING_DBG("~MultiStreamSipTransport@%p {PjTr=%p}", this, &trData_.base);

    // Flush send queue with ENOTCONN error
    for (auto tdata : txQueue_) {
        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(&trData_.base, tdata->op_key.token,
                                   -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN));
    }

    auto base = getTransportBase();
    Manager::instance().unregisterEventHandler((uintptr_t)this);

    // Stop low-level transport first
    stream_->close();

    // If delete not trigged by pjsip_transport_destroy (happen if objet not given to pjsip)
    if (not base->is_shutdown and not base->is_destroying)
        pjsip_transport_shutdown(base);

    pj_lock_destroy(base->lock);
    pj_atomic_destroy(base->ref_cnt);
}

void
MultiStreamSipTransport::handleEvents()
{
    // Notify transport manager about state changes first
    // Note: stop when disconnected event is encountered
    // and differ its notification AFTER pending rx msg to let
    // them a chance to be delivered to application before closing
    // the transport.
    decltype(stateChangeEvents_) eventDataQueue;
    {
        std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
        eventDataQueue = std::move(stateChangeEvents_);
        stateChangeEvents_.clear();
    }

    ChangeStateEventData disconnectedEvent;
    bool disconnected = false;
    auto state_cb = pjsip_tpmgr_get_state_cb(trData_.base.tpmgr);
    if (state_cb) {
        for (auto& evdata : eventDataQueue) {
            evdata.state_info.ext_info = nullptr;
            if (evdata.state != PJSIP_TP_STATE_DISCONNECTED) {
                (*state_cb)(&trData_.base, evdata.state, &evdata.state_info);
            } else {
                disconnectedEvent = std::move(evdata);
                disconnected = true;
                break;
            }
        }
    }

    // Handle SIP transport -> Stream
    decltype(txQueue_) tx_queue;
    {
        std::lock_guard<std::mutex> l(txMutex_);
        if (syncTx_) {
            tx_queue = std::move(txQueue_);
            txQueue_.clear();
        }
    }

    bool fatal = false;
    for (auto tdata : tx_queue) {
        pj_status_t status;
        if (!fatal) {
            const std::size_t size = tdata->buf.cur - tdata->buf.start;
            auto ret = stream_->sendData(tdata->buf.start, size);
            if (ret < 0) {
                RING_ERR("[SIP] fatal error during sending: %s", strerror(ret));
                stream_->close();
                fatal = true;
            }
            if (ret < 0)
                status = -PJ_RETURN_OS_ERROR(errno);
            else
                status = ret;
        } else
            status = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(&trData_.base, tdata->op_key.token, status);
    }

    // Handle Stream -> SIP transport
    if (readBufferSize_ < sizeof(rdata_.pkt_info.packet) and stream_->canRead()) {
        auto ret = stream_->recvData(readBufferPtr_, readBufferSize_ - sizeof(rdata_.pkt_info.packet));
        if (ret < 0) {
            RING_WARN("[MSST] readData failed, %s", strerror(ret));
        } else if (ret > 0) {
            readBufferPtr_ += ret;
            readBufferSize_ += ret;
            rdata_.pkt_info.len = readBufferSize_;
            rdata_.pkt_info.zero = 0;
        }
    }

    if (readBufferSize_ > 0) {
        pj_gettimeofday(&rdata_.pkt_info.timestamp);
        //RING_ERR("avail %zu\n%s\n-", rdata_.pkt_info.len, std::string(rdata_.pkt_info.packet, rdata_.pkt_info.len).c_str());
        auto eaten = pjsip_tpmgr_receive_packet(trData_.base.tpmgr, &rdata_);
        pj_pool_reset(rdata_.tp_info.pool);
        if (auto remain = readBufferSize_ - eaten) {
            pj_memmove(rdata_.pkt_info.packet, rdata_.pkt_info.packet + eaten, remain);
            readBufferPtr_ = rdata_.pkt_info.packet + remain;
        } else
            readBufferPtr_ = rdata_.pkt_info.packet;
        readBufferSize_ -= eaten;
        //RING_ERR("eaten %zu\n%s\n-", eaten, std::string(rdata_.pkt_info.packet, readBufferSize_).c_str());
    }

    // Time to deliver disconnected event if exists
    if (disconnected and state_cb)
        (*state_cb)(&trData_.base, disconnectedEvent.state, &disconnectedEvent.state_info);
}

void
MultiStreamSipTransport::pushChangeStateEvent(ChangeStateEventData&& ev)
{
    std::lock_guard<std::mutex> lk{stateChangeEventsMutex_};
    stateChangeEvents_.emplace_back(std::move(ev));
}

void
MultiStreamSipTransport::updateTransportState(pjsip_transport_state state)
{
    ChangeStateEventData ev;

    std::memset(&ev.state_info, 0, sizeof(ev.state_info));

    ev.state = state;
    {
        std::lock_guard<std::mutex> lk {txMutex_};
        syncTx_ = true;
    }
    ev.state_info.status = PJ_SUCCESS;

    pushChangeStateEvent(std::move(ev));
}

pj_status_t
MultiStreamSipTransport::send(pjsip_tx_data* tdata, const pj_sockaddr_t* rem_addr,
                       int addr_len, void* token,
                       pjsip_transport_callback callback)
{
    // Sanity check
    PJ_ASSERT_RETURN(tdata, PJ_EINVAL);

    // Check that there's no pending operation associated with the tdata
    PJ_ASSERT_RETURN(tdata->op_key.tdata == nullptr, PJSIP_EPENDINGTX);

    // Check the address is supported
    PJ_ASSERT_RETURN(rem_addr and
                     (addr_len==sizeof(pj_sockaddr_in) or
                      addr_len==sizeof(pj_sockaddr_in6)),
                     PJ_EINVAL);

    // Check in we are able to send it in synchronous way first
    const std::size_t size = tdata->buf.cur - tdata->buf.start;
    std::unique_lock<std::mutex> lk {txMutex_};
    if (syncTx_ and txQueue_.empty()) {
        RING_WARN("[MSST] send %zu", size);
        auto ret = stream_->sendData(tdata->buf.start, size);
        lk.unlock();

        // Shutdown on fatal error, else ignore it
        if (ret < 0) {
            RING_ERR("[SIP] error during sending: %s", strerror(ret));
            stream_->close();
            return -PJ_RETURN_OS_ERROR(errno);
        }

        return PJ_SUCCESS;
    }

    // Asynchronous sending
    RING_WARN("[MSST] send async %zu", size);
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;
    txQueue_.push_back(tdata);
    return PJ_EPENDING;
}

} // namespace ring
