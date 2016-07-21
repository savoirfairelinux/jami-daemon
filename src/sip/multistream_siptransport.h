/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#pragma once

#include "ip_utils.h"
#include "noncopyable.h"
#include "threadloop.h"

#include <pjsip.h>
#include <pj/pool.h>

#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>
#include <queue>
#include <utility>
#include <vector>
#include <condition_variable>

namespace ring {

namespace ReliableSocket {
class DataStream;
}

/**
 * SipsIceTransport
 *
 * Implements a SipTransport for ReliableSocket::DataStream
 */
class MultiStreamSipTransport
{
private:
    NON_COPYABLE(MultiStreamSipTransport);

public:
    using Clock = std::chrono::steady_clock;
    using TransportData = struct {
        pjsip_transport base; // do not move, SHOULD be the fist member
        MultiStreamSipTransport* self {nullptr};
    };
    static_assert(std::is_standard_layout<TransportData>::value,
                  "TranportData requires standard-layout");

    MultiStreamSipTransport(pjsip_endpoint* endpt,
                            std::shared_ptr<ReliableSocket::DataStream> stream);
    ~MultiStreamSipTransport();

    void shutdown();

public: // Getters
    pjsip_transport* getTransportBase() { return &trData_.base; }

private: // PJSIP transport backend
    TransportData trData_; // uplink to "this" (used by PJSIP called C-callbacks)

    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> pool_;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> rxPool_;

    pjsip_rx_data rdata_;

    pj_ssl_cert_info localCertInfo_;
    pj_ssl_cert_info remoteCertInfo_;

    pj_status_t verifyStatus_ {PJ_EUNKNOWN};

private: // DataStream backend
    const std::shared_ptr<ReliableSocket::DataStream> stream_;

private: // IO / events
    struct ChangeStateEventData {
        pjsip_transport_state_info state_info;
        decltype(PJSIP_TP_STATE_DISCONNECTED) state;
    };

    std::size_t readBufferSize_ {0};

    std::mutex stateChangeEventsMutex_ {};
    std::list<ChangeStateEventData> stateChangeEvents_ {};

    pj_status_t send(pjsip_tx_data*, const pj_sockaddr_t*, int, void*, pjsip_transport_callback);
    void handleEvents();
    void pushChangeStateEvent(ChangeStateEventData&&);
    void updateTransportState(pjsip_transport_state);

private: // Transmission layer (async by thread)
    void flushTxQueue();

    std::mutex txMutex_ {};
    std::condition_variable txCv_ {};
    bool canWrite_ {false}; // true if we can send data (cnx established)
    std::list<pjsip_tx_data*> txQueue_ {}; // Used for asynchronous transmissions
    ThreadLoop txThreadloop_;
};

} // namespace ring
