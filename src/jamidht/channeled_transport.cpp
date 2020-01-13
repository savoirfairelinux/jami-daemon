/*
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
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

#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/compat/socket.h>
#include <pj/lock.h>

#include "logger.h"
#include "multiplexed_socket.h"
#include "sip/sip_utils.h"

namespace jami { namespace tls {

ChanneledSIPTransport::ChanneledSIPTransport(pjsip_endpoint* endpt, int tp_type,
                                             const std::shared_ptr<ChannelSocket>& socket,
                                             const IpAddr& local, const IpAddr& remote,
                                             onShutdownCb&& cb)
    : socket_ (socket)
    , trData_ ()
    , pool_  {nullptr, pj_pool_release}
    , rxPool_ (nullptr, pj_pool_release)
    , local_ {local}
    , remote_ {remote}
{
    JAMI_DBG("ChanneledSIPTransport@%p {tr=%p}", this, &trData_.base);

    trData_.self = this; // up-link for PJSIP callbacks

    pool_ = sip_utils::smart_alloc_pool(endpt, "channeled.pool",
                                        sip_utils::POOL_TP_INIT, sip_utils::POOL_TP_INC);

    auto& base = trData_.base;
    std::memset(&base, 0, sizeof(base));

    pj_ansi_snprintf(base.obj_name, PJ_MAX_OBJ_NAME, "chan%p", &base);
    base.endpt = endpt;
    base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    base.pool = pool_.get();

    if (pj_atomic_create(pool_.get(), 0, &base.ref_cnt) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP atomic.");

    if (pj_lock_create_recursive_mutex(pool_.get(), "chan",
                                       &base.lock) != PJ_SUCCESS)
        throw std::runtime_error("Can't create PJSIP mutex.");

    pj_sockaddr_cp(&base.key.rem_addr, remote_.pjPtr());
    base.key.type = tp_type;
    auto reg_type = static_cast<pjsip_transport_type_e>(tp_type);
    base.type_name = const_cast<char*>(pjsip_transport_get_type_name(reg_type));
    base.flag = pjsip_transport_get_flag_from_type(reg_type);
    base.info = static_cast<char*>(pj_pool_alloc(pool_.get(), sip_utils::TRANSPORT_INFO_LENGTH));

    auto remote_addr = remote_.toString();
    pj_ansi_snprintf(base.info, sip_utils::TRANSPORT_INFO_LENGTH, "%s to %s", base.type_name,
                     remote_addr.c_str());
    base.addr_len = remote_.getLength();
    base.dir = PJSIP_TP_DIR_NONE;

    /* Set initial local address */
    pj_sockaddr_cp(&base.local_addr, local_.pjPtr());

    sip_utils::sockaddr_to_host_port(pool_.get(), &base.local_name, &base.local_addr);
    sip_utils::sockaddr_to_host_port(pool_.get(), &base.remote_name, remote_.pjPtr());

    base.send_msg = [](pjsip_transport *transport,
                       pjsip_tx_data *tdata,
                       const pj_sockaddr_t *rem_addr, int addr_len,
                       void *token, pjsip_transport_callback callback) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        return this_->send(tdata, rem_addr, addr_len, token, callback);
    };
    base.do_shutdown = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        JAMI_DBG("ChanneledSIPTransport@%p {tr=%p {rc=%ld}}: shutdown", this_,
                 transport, pj_atomic_get(transport->ref_cnt));
        if (this_->socket_) this_->socket_->shutdown();
        return PJ_SUCCESS;
    };
    base.destroy = [](pjsip_transport *transport) -> pj_status_t {
        auto& this_ = reinterpret_cast<TransportData*>(transport)->self;
        JAMI_DBG("ChanneledSIPTransport@%p: destroying", this_);
        delete this_;
        return PJ_SUCCESS;
    };

    /* Init rdata_ */
    std::memset(&rdata_, 0, sizeof(pjsip_rx_data));
    rxPool_ = sip_utils::smart_alloc_pool(endpt, "channeled.rxPool",
                                          PJSIP_POOL_RDATA_LEN, PJSIP_POOL_RDATA_LEN);
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

    if (pjsip_transport_register(base.tpmgr, &base) != PJ_SUCCESS)
        throw std::runtime_error("Can't register PJSIP transport.");
    
    socket->setOnRecv([this](const uint8_t* buf, size_t len) {
        std::lock_guard<std::mutex> l(rxMtx_);
        std::vector<uint8_t> rx {buf, buf+len};
        rxPending_.emplace_back(std::move(rx));
        scheduler_.run([this]{ handleEvents(); });
        return len;
    });
    socket->onShutdown(std::move(cb));
}

ChanneledSIPTransport::~ChanneledSIPTransport()
{
    JAMI_DBG("~ChanneledSIPTransport@%p {tr=%p}", this, &trData_.base);
    stopLoop_ = true;

    // Flush send queue with ENOTCONN error
    for (auto tdata : txQueue_) {
        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(&trData_.base, tdata->op_key.token,
                                   -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN));
    }

    auto base = getTransportBase();

    // Stop low-level transport first
    socket_->shutdown();
    if (eventLoop_.joinable()) eventLoop_.join();
    socket_.reset();

    // If delete not trigged by pjsip_transport_destroy (happen if objet not given to pjsip)
    if (not base->is_shutdown and not base->is_destroying)
        pjsip_transport_shutdown(base);

    pj_lock_destroy(base->lock);
    pj_atomic_destroy(base->ref_cnt);
    JAMI_DBG("~ChanneledSIPTransport@%p {tr=%p} bye", this, &trData_.base);
}

void
ChanneledSIPTransport::handleEvents()
{
    // Notify transport manager about state changes first
    // Note: stop when disconnected event is encountered
    // and differ its notification AFTER pending rx msg to let
    // them a chance to be delivered to application before closing
    // the transport.
    JAMI_WARN("@@@ handleEvents");
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
            evdata.tls_info.ssl_sock_info = &evdata.ssl_info;
            evdata.state_info.ext_info = &evdata.tls_info;
            if (evdata.state == PJSIP_TP_STATE_CONNECTED) {
                if (PJSIP_TRANSPORT_IS_RELIABLE(&trData_.base)) {
                    eventLoop_ = std::thread([this] {
                        try {
                            eventLoop();
                        } catch (const std::exception& e) {
                            JAMI_ERR() << "SipIceTransport: eventLoop() failure: " << e.what();
                        }
                    });
                }
            }
            if (evdata.state != PJSIP_TP_STATE_DISCONNECTED) {
                (*state_cb)(&trData_.base, evdata.state, &evdata.state_info);
            } else {
                JAMI_WARN("[SIPS] got disconnected event!");
                disconnectedEvent = std::move(evdata);
                disconnected = true;
                stopLoop_ = true;
                break;
            }
        }
    }

    // Handle SIP transport -> TLS
    JAMI_WARN("@@@ --- %u", txQueue_.size());
    decltype(txQueue_) tx_queue;
    {
        std::lock_guard<std::mutex> l(txMutex_);
        // TODO we are already connected, but handle disconnected: if (syncTx_) {
            tx_queue = std::move(txQueue_);
            txQueue_.clear();
       //}
    }

    bool fatal = false;
    JAMI_WARN("@@@ SEND 4 -- %u", tx_queue.size());
    for (auto tdata : tx_queue) {
        JAMI_WARN("@@@ SEND 5");
        pj_status_t status;
        if (!fatal) {
            const std::size_t size = tdata->buf.cur - tdata->buf.start;
            std::error_code ec;
            JAMI_WARN("@@@ SEND 6");
            status = socket_->write(reinterpret_cast<const uint8_t*>(tdata->buf.start), size, ec);
            if (ec) {
                fatal = true;
                socket_->shutdown();
            }
        } else {
            status = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);
        }

        tdata->op_key.tdata = nullptr;
        if (tdata->op_key.callback)
            tdata->op_key.callback(&trData_.base, tdata->op_key.token, status);
    }

    // Handle TLS -> SIP transport
    decltype(rxPending_) rx;
    {
        std::lock_guard<std::mutex> l(rxMtx_);
        rx = std::move(rxPending_);
        rxPending_.clear();
    }

    sip_utils::register_thread();
    for (auto it = rx.begin(); it != rx.end(); ++it) {
        auto& pck = *it;
        pj_pool_reset(rdata_.tp_info.pool);
        pj_gettimeofday(&rdata_.pkt_info.timestamp);
        rdata_.pkt_info.len = std::min(pck.size(), (size_t) PJSIP_MAX_PKT_LEN);
        std::copy_n(pck.data(), rdata_.pkt_info.len, rdata_.pkt_info.packet);
        auto eaten = pjsip_tpmgr_receive_packet(trData_.base.tpmgr, &rdata_);

        // Uncomplet parsing? (may be a partial sip packet received)
        if (eaten != (pj_ssize_t)pck.size()) {
            auto npck_it = std::next(it);
            if (npck_it != rx.end()) {
                // drop current packet, merge reminder with next one
                auto& npck = *npck_it;
                npck.insert(npck.begin(), pck.begin()+eaten, pck.end());
            } else {
                // erase eaten part, keep remainder
                pck.erase(pck.begin(), pck.begin()+eaten);
                {
                    std::lock_guard<std::mutex> l(rxMtx_);
                    rxPending_.splice(rxPending_.begin(), rx, it);
                }
                break;
            }
        }
    }

    // Time to deliver disconnected event if exists
    //TODO!
    //if (disconnected and state_cb) {
    //    JAMI_WARN("[SIPS] process disconnect event");
    //    (*state_cb)(&trData_.base, disconnectedEvent.state, &disconnectedEvent.state_info);
    //}
}


pj_status_t
ChanneledSIPTransport::send(pjsip_tx_data* tdata, const pj_sockaddr_t* rem_addr,
                       int addr_len, void* token,
                       pjsip_transport_callback callback)
{
    JAMI_WARN("@@@ SEND");
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
    JAMI_WARN("@@@ SEND 2");
    const std::size_t size = tdata->buf.cur - tdata->buf.start;
    std::unique_lock<std::mutex> lk {txMutex_};
    if (/*TODO handle disconned: syncTx_ and*/ txQueue_.empty()) {
        std::error_code ec;
        socket_->write(reinterpret_cast<const uint8_t*>(tdata->buf.start), size, ec);
        lk.unlock();
        if (ec) {
            return PJ_EINVAL;
        }
        return PJ_SUCCESS;
    }
    JAMI_WARN("@@@ SEND 3");

    // Asynchronous sending
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;
    txQueue_.push_back(tdata);
    JAMI_WARN("@@@ SEND 3 txQueue len: %u", txQueue_.size());
    scheduler_.run([this]{ handleEvents(); });
    return PJ_EPENDING;
}

void
ChanneledSIPTransport::eventLoop()
{
    while (!stopLoop_) {
        std::error_code err;
        if (socket_ && socket_->waitForData(std::chrono::seconds(10), err)) {
            if (stopLoop_)
                break;
            std::vector<uint8_t> pkt;
            pkt.resize(PJSIP_MAX_PKT_LEN);
            auto read = socket_->read(pkt.data(), PJSIP_MAX_PKT_LEN, err);
            if (err == std::errc::broken_pipe || read == 0) {
                JAMI_DBG("[SIPS] eof");
                // TODO TLS CHange state?
                break;
            }
            if (read > 0) {
                pkt.resize(read);
                std::lock_guard<std::mutex> l(rxMtx_);
                rxPending_.emplace_back(std::move(pkt));
                scheduler_.run([this]{ handleEvents(); });
            }
        }
    }
}

}} // namespace jami::tls
