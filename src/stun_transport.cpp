/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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

#include "stun_transport.h"

#include "logger.h"
#include "ip_utils.h"
#include "sip/sip_utils.h"

#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdexcept>
#include <future>
#include <atomic>
#include <thread>

namespace ring {

enum class ServerState {
    NONE,
    READY,
    DOWN,
};

class StunTransportPimpl {
public:
    StunTransportPimpl() = default;
    ~StunTransportPimpl();

    void ioJob();

    pj_status_t onSendMsg(void* token, const void* pkt,
                          pj_size_t pkt_size, const pj_sockaddr_t* dst_addr,
                          unsigned addr_len);
    pj_status_t onRxRequest(const pj_uint8_t* pkt,
                            unsigned pkt_len, const pj_stun_rx_data* rdata,
                            void* token, const pj_sockaddr_t* src_addr,
                            unsigned src_addr_len);
    pj_status_t onRxIndication(const pj_uint8_t* pkt,
                               unsigned pkt_len, const pj_stun_msg* msg,
                               void* token, const pj_sockaddr_t* src_addr,
                               unsigned src_addr_len);
    void onRequestComplete(pj_status_t status,
                           void* token, pj_stun_tx_data* tdata,
                           const pj_stun_msg* response,
                           const pj_sockaddr_t* src_addr,
                           unsigned src_addr_len);

    pj_bool_t onActiveSockRead(void* data, pj_size_t size, pj_status_t status,
                               pj_size_t* remainder);
    pj_bool_t onActiveSockSent(pj_ioqueue_op_key_t* send_key, pj_ssize_t sent);

    pj_caching_pool poolCache {};
    pj_pool_t* pool {nullptr};
    pj_stun_config stunConfig {};
    pj_stun_sock_cfg sockConfig {};
    pj_stun_session* session {nullptr};
    pj_sock_t sock_fd {PJ_INVALID_SOCKET};
    pj_activesock_t*  activeSock {nullptr};
    IpAddr serverAddr;
    IpAddr mappedAddr;
    std::atomic<ServerState> state {ServerState::NONE};
    std::atomic_bool ioJobQuit {false};
    std::thread ioWorker;
};

StunTransportPimpl::~StunTransportPimpl()
{
    if (session) {
        pj_stun_session_destroy(session);
        activeSock = nullptr;
    }
    if (activeSock) {
        pj_activesock_close(activeSock);
        activeSock = nullptr;
    }
    ioJobQuit = true;
    if (ioWorker.joinable())
        ioWorker.join();
    if (pool)
        pj_pool_release(pool);
    pj_caching_pool_destroy(&poolCache);
}

void
StunTransportPimpl::ioJob()
{
    sip_utils::register_thread();

    while (!ioJobQuit.load()) {
        const pj_time_val delay = {0, 10};
        pj_ioqueue_poll(stunConfig.ioqueue, &delay);
        pj_timer_heap_poll(stunConfig.timer_heap, nullptr);
  }
}

pj_status_t
StunTransportPimpl::onSendMsg(void* token, const void* pkt,
                              pj_size_t pkt_size, const pj_sockaddr_t* dst_addr,
                              unsigned addr_len)
{
    (void)token;
    (void)dst_addr;
    (void)addr_len;
    pj_ssize_t size = pkt_size;
    RING_WARN("onSendMsg: %zu bytes", pkt_size);
    auto status = pj_sock_send(sock_fd, pkt, &size, 0);
    if (status != PJ_SUCCESS) {
        RING_ERR("pj_sock_send failed: %d", status);
    }
    return status;
}

pj_status_t
StunTransportPimpl::onRxRequest(const pj_uint8_t* pkt,
                                unsigned pkt_len, const pj_stun_rx_data* rdata,
                                void* token, const pj_sockaddr_t* src_addr,
                                unsigned src_addr_len)
{
    (void)pkt;
    (void)pkt_len;
    (void)rdata;
    (void)token;
    (void)src_addr;
    (void)src_addr_len;
    RING_WARN("onRxRequest");
    return PJ_SUCCESS;
}

pj_status_t
StunTransportPimpl::onRxIndication(const pj_uint8_t* pkt,
                                   unsigned pkt_len, const pj_stun_msg* msg,
                                   void* token, const pj_sockaddr_t* src_addr,
                                   unsigned src_addr_len)
{
    (void)pkt;
    (void)pkt_len;
    (void)msg;
    (void)token;
    (void)src_addr;
    (void)src_addr_len;
    RING_WARN("onRxIndication");
    return PJ_SUCCESS;
}

void
StunTransportPimpl::onRequestComplete(pj_status_t status,
                                      void* token, pj_stun_tx_data* tdata,
                                      const pj_stun_msg* response,
                                      const pj_sockaddr_t* src_addr,
                                      unsigned src_addr_len)
{
    (void)status;
    (void)token;
    (void)tdata;
    (void)response;
    (void)src_addr;
    (void)src_addr_len;

    if (status != PJ_SUCCESS) {
        char err_msg[PJ_ERR_MSG_SIZE] {0};
        pj_strerror(status, err_msg, sizeof(err_msg));
        RING_WARN("request failed: %s", err_msg);
        return;
    }

    pj_sockaddr addr;
    pj_stun_sock_op op;
    if (pj_sockaddr_has_addr(&addr)) {
        RING_DBG("Keep alive");
        op = PJ_STUN_SOCK_KEEP_ALIVE_OP; // TODO: implement keep-alive
    } else {
        RING_DBG("Binding complete");
        op = PJ_STUN_SOCK_BINDING_OP;
    }

    const pj_stun_sockaddr_attr *mapped_attr;
    mapped_attr = (const pj_stun_sockaddr_attr*)pj_stun_msg_find_attr(response, PJ_STUN_ATTR_XOR_MAPPED_ADDR, 0);
    if (!mapped_attr) {
        mapped_attr = (const pj_stun_sockaddr_attr*)pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MAPPED_ADDR, 0);
    }

    if (mapped_attr == NULL) {
        RING_WARN("no mapped addresses");
        return;
    }

    auto mapped_changed = !pj_sockaddr_has_addr(&addr) || pj_sockaddr_cmp(&addr, &mapped_attr->sockaddr) != 0;
    if (mapped_changed) {
        mappedAddr = IpAddr {mapped_attr->sockaddr};
        RING_WARN("STUN mapped address found/changed: %s", mappedAddr.toString(true, true).c_str());

        if (op == PJ_STUN_SOCK_KEEP_ALIVE_OP) {
            op = PJ_STUN_SOCK_MAPPED_ADDR_CHANGE;
        }

        if (state.load() == ServerState::NONE)
            state = ServerState::READY;
    }
}

pj_bool_t
StunTransportPimpl::onActiveSockRead(void* data, pj_size_t size, pj_status_t status,
                                     pj_size_t* remainder)
{
    if (status != PJ_SUCCESS) {
        char err_msg[PJ_ERR_MSG_SIZE] {0};
        pj_strerror(status, err_msg, sizeof(err_msg));
        RING_WARN("socket read failed: %s", err_msg);
        pj_activesock_close(activeSock);
        activeSock = nullptr;
        return PJ_FALSE;
    }

    // STUN or application packet?
    status = pj_stun_msg_check((const pj_uint8_t*)data, size, PJ_STUN_CHECK_PACKET);
    if (status != PJ_SUCCESS) {
        // give it to upper layer
        RING_WARN("get user data: %zu bytes", size);
        *remainder = 0;
        return PJ_TRUE;
    }

    RING_WARN("onActiveSockRead: %zu bytes", size);
    status = pj_stun_session_on_rx_pkt(session, data, size, 0, (void*)1, remainder,
                                       serverAddr.pjPtr(), serverAddr.getLength());


    return PJ_TRUE;
}

pj_bool_t
StunTransportPimpl::onActiveSockSent(pj_ioqueue_op_key_t* send_key, pj_ssize_t sent)
{
    (void)send_key;
    RING_WARN("onActiveSockSent: %zu bytes", sent);
    return PJ_TRUE;
}

//==================================================================================================

StunTransport::StunTransport(const StunTransportParams& params)
    : pimpl_ {new StunTransportPimpl}
{
    pj_status_t status;

    // PJSIP memory pool
    pj_caching_pool_init(&pimpl_->poolCache, &pj_pool_factory_default_policy, 0);

    if (params.bound_addr.isUnspecified())
        throw std::invalid_argument("invalid bound address");

    pimpl_->pool = pj_pool_create(&pimpl_->poolCache.factory, "RingStunTr", 512, 512, nullptr);
    if (!pimpl_->pool)
        throw std::runtime_error("pj_pool_create");

    // STUN config
    pj_stun_config_init(&pimpl_->stunConfig, &pimpl_->poolCache.factory, 0, nullptr, nullptr);

    // Create global timer heap
    status = pj_timer_heap_create(pimpl_->pool, 1000, &pimpl_->stunConfig.timer_heap);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_timer_heap_create");

    // Create global ioqueue
    status = pj_ioqueue_create(pimpl_->pool, 16, &pimpl_->stunConfig.ioqueue);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_ioqueue_create");

    // Let ioJob plays with them
    pimpl_->ioWorker = std::thread([this]{ pimpl_->ioJob(); });

    // Setup STUN session callbacks
    pj_stun_session_cb stun_cb;
    pj_bzero(&stun_cb, sizeof(stun_cb));
    stun_cb.on_send_msg = [](pj_stun_session *sess, void *token, const void *pkt,
                             pj_size_t pkt_size, const pj_sockaddr_t *dst_addr,
                             unsigned addr_len) -> pj_status_t {
                              auto tr = static_cast<StunTransport*>(pj_stun_session_get_user_data(sess));
                              return tr->pimpl_->onSendMsg(token, pkt, pkt_size, dst_addr, addr_len);
                          };
    stun_cb.on_rx_request = [](pj_stun_session *sess, const pj_uint8_t *pkt, unsigned pkt_len,
                               const pj_stun_rx_data *rdata, void *token,
                               const pj_sockaddr_t *src_addr,
                               unsigned src_addr_len) -> pj_status_t {
                                auto tr = static_cast<StunTransport*>(pj_stun_session_get_user_data(sess));
                                return tr->pimpl_->onRxRequest(pkt, pkt_len, rdata, token, src_addr, src_addr_len);
                            };
    stun_cb.on_rx_indication = [](pj_stun_session *sess, const pj_uint8_t *pkt, unsigned pkt_len,
                                  const pj_stun_msg *msg, void *token,
                                  const pj_sockaddr_t *src_addr,
                                  unsigned src_addr_len) -> pj_status_t {
                                   auto tr = static_cast<StunTransport*>(pj_stun_session_get_user_data(sess));
                                   return tr->pimpl_->onRxIndication(pkt, pkt_len, msg, token, src_addr, src_addr_len);
                               };
    stun_cb.on_request_complete = [](pj_stun_session* sess, pj_status_t status, void* token,
                                     pj_stun_tx_data* tdata, const pj_stun_msg* response,
                                     const pj_sockaddr_t* src_addr, unsigned src_addr_len) {
                                      auto tr = static_cast<StunTransport*>(pj_stun_session_get_user_data(sess));
                                      tr->pimpl_->onRequestComplete(status, token, tdata, response, src_addr, src_addr_len);
                                  };


    // Create a STUN session with ourself as user data
    status = pj_stun_session_create(&pimpl_->stunConfig, nullptr, &stun_cb, true, nullptr,
                                    &pimpl_->session);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_stun_session_create");
    pj_stun_session_set_user_data(pimpl_->session, this);

    // Create a TCP socket, bound to user given address and a random port
    status = pj_sock_socket(params.bound_addr.getFamily(), pj_SOCK_STREAM(), 0, &pimpl_->sock_fd);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_sock_socket");

    status = pj_sock_bind_random(pimpl_->sock_fd, params.bound_addr.pjPtr(),
                                 pimpl_->sockConfig.port_range, 10);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_sock_bind_random");

    // Make an active sock to handle the STUN sock with our ioqueue
    pj_activesock_cfg activesock_cfg;
    pj_activesock_cb activesock_cb;

    pj_activesock_cfg_default(&activesock_cfg);
    ///activesock_cfg.grp_lock = stun_sock->grp_lock;
    activesock_cfg.async_cnt = 1;
    activesock_cfg.concurrency = 0;

    pj_bzero(&activesock_cb, sizeof(activesock_cb));
    activesock_cb.on_data_read = [](pj_activesock_t* asock, void* data, pj_size_t size,
                                    pj_status_t status, pj_size_t* remainder) -> pj_bool_t {
                                     auto tr = static_cast<StunTransport*>(pj_activesock_get_user_data(asock));
                                     return tr->pimpl_->onActiveSockRead(data, size, status, remainder);
                                 };
    activesock_cb.on_data_sent = [](pj_activesock_t* asock, pj_ioqueue_op_key_t* send_key,
                                    pj_ssize_t sent) -> pj_bool_t {
                                     auto tr = static_cast<StunTransport*>(pj_activesock_get_user_data(asock));
                                     return tr->pimpl_->onActiveSockSent(send_key, sent);
                                 };

    status = pj_activesock_create(pimpl_->pool, pimpl_->sock_fd, pj_SOCK_STREAM(),
                                  &activesock_cfg, pimpl_->stunConfig.ioqueue, &activesock_cb, this,
                                  &pimpl_->activeSock);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_activesock_create");
}

StunTransport::~StunTransport()
{}

void
StunTransport::connect(const IpAddr& server)
{
    if (server.isUnspecified())
        throw std::invalid_argument("invalid server address");

    pimpl_->serverAddr = server;
    if (server.getPort() == 0)
        pimpl_->serverAddr.setPort(PJ_STUN_PORT);

    // TCP connect on STUN server
    auto status = pj_sock_connect(pimpl_->sock_fd, pimpl_->serverAddr.pjPtr(),
                                  pimpl_->serverAddr.getLength());

    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_sock_connect");

    // Run active-socket asynchonous reads
    status = pj_activesock_start_read(pimpl_->activeSock, pimpl_->pool, PJ_STUN_SOCK_PKT_LEN, 0);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_activesock_start_read");

    // Create STUN BINDING request
    pj_stun_tx_data* tdata;
    status = pj_stun_session_create_req(pimpl_->session, PJ_STUN_BINDING_REQUEST, PJ_STUN_MAGIC,
                                        nullptr, &tdata);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_stun_session_create_req");

    // Send the request
    status = pj_stun_session_send_msg(pimpl_->session, (void*)(pj_ssize_t)1, PJ_FALSE, PJ_FALSE,
                                      pimpl_->serverAddr.pjPtr(), pimpl_->serverAddr.getLength(),
                                      tdata);
    if (status != PJ_SUCCESS)
        throw std::runtime_error("pj_stun_session_send_msg");

    waitBinding();
}

void
StunTransport::waitBinding()
{
    while (pimpl_->state.load() != ServerState::READY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

const IpAddr&
StunTransport::getMappedAddr() const
{
    return pimpl_->mappedAddr;
}

bool
StunTransport::sendto(const IpAddr&, const std::vector<uint8_t>&)
{
    return false;
}

void
StunTransport::recvfrom(std::pair<IpAddr, std::vector<uint8_t>>&)
{}

} // namespace ring
