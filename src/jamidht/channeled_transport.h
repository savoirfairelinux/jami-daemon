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

#pragma once

#include "noncopyable.h"
#include "scheduled_executor.h"
#include "jamidht/abstract_sip_transport.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <list>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

namespace jami {

class ChannelSocket;
using onShutdownCb = std::function<void(void)>;

namespace tls {

/**
 * ChanneledSIPTransport
 *
 * Implements a pjsip_transport on top of a ChannelSocket
 */
class ChanneledSIPTransport : public AbstractSIPTransport
{
public:
    ChanneledSIPTransport(pjsip_endpoint* endpt,
                          const std::shared_ptr<ChannelSocket>& socket,
                          onShutdownCb&& cb);
    ~ChanneledSIPTransport();

    /**
     * Connect callbacks for channeled socket, must be done when the channel is ready to be used
     */
    void start();

    pjsip_transport* getTransportBase() override { return &trData_.base; }

    IpAddr getLocalAddress() const override { return local_; }

private:
    NON_COPYABLE(ChanneledSIPTransport);

    // The SIP transport uses a ChannelSocket to send and receive datas
    std::shared_ptr<ChannelSocket> socket_ {};
    onShutdownCb shutdownCb_ {};
    IpAddr local_ {};
    IpAddr remote_ {};

    // PJSIP transport backend
    TransportData trData_ {}; // uplink to "this" (used by PJSIP called C-callbacks)

    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> pool_;
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)*> rxPool_;
    pjsip_rx_data rdata_ {};

    pj_status_t send(pjsip_tx_data*, const pj_sockaddr_t*, int, void*, pjsip_transport_callback);

    // Handle disconnected event
    std::atomic_bool disconnected_ {false};
};

} // namespace tls
} // namespace jami
