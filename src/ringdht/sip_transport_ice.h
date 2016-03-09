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

#pragma once

#include "ip_utils.h"

#include <pjsip.h>
#include <pj/pool.h>

#include <functional>
#include <memory>
#include <type_traits> // std::is_standard_layout

namespace ring {

class IceTransport;

struct SipIceTransport
{
        // This structure SHOULD be standard-layout,
        // implies std::is_standard_layout<TransportData>::value
        // SHOULD return true!
        using TransportData = struct {
                pjsip_transport base; // do not move, SHOULD be the fist member
                SipIceTransport* self {nullptr};
        };
        static_assert(std::is_standard_layout<TransportData>::value,
                      "TranportData requires standard-layout");


        SipIceTransport(pjsip_endpoint* endpt, pj_pool_t& pool, long t_type,
                        const std::shared_ptr<IceTransport>& ice,
                        int comp_id);
        ~SipIceTransport();

        /**
         * To be called on a regular basis to receive packets
         */
        void loop();

        IpAddr getLocalAddress() const;

        std::shared_ptr<IceTransport> getIceTransport() const {
            return ice_;
        }

        pjsip_transport* getTransportBase() {
            return &trData_.base;
        }

    private:
        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> rxPool_;

        TransportData trData_;

        pjsip_rx_data rdata_;
        bool is_registered_ {false};
        const std::shared_ptr<IceTransport> ice_;
        const int comp_id_;

        pj_status_t send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                         int addr_len, void *token,
                         pjsip_transport_callback callback);

        void onRecv();
};

} // namespace ring
