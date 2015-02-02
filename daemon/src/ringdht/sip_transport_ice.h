/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#pragma once

#include "ip_utils.h"

#include <pjsip.h>
#include <pj/pool.h>

#include <functional>
#include <memory>

namespace ring {

class IceTransport;

struct SipIceTransport
{
        SipIceTransport(pjsip_endpoint* endpt, pj_pool_t& pool, long t_type,
                        const std::shared_ptr<IceTransport>& ice,
                        int comp_id, std::function<int()> destroy_cb);
        ~SipIceTransport();

        /**
         * To be called on a regular basis to receive packets
         */
        void loop();

        IpAddr getLocalAddress() const;

        std::shared_ptr<IceTransport> getIceTransport() const {
            return ice_;
        }

        // This structure SHOULD be standard-layout,
        // implies std::is_standard_layout<SipIceTransportTranpoline>::value
        // SHOULD return true!
        struct SipIceTransportTranpoline {
                pjsip_transport base; // do not move, SHOULD be the fist member
                SipIceTransport* self {nullptr};
        } trInfo;

    private:
        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> rxPool_;

        pjsip_rx_data rdata;
        bool is_registered_ {false};
        const std::shared_ptr<IceTransport> ice_;
        const int comp_id_;

        std::function<int()> destroy_cb_ {};

        pj_status_t send(pjsip_tx_data *tdata, const pj_sockaddr_t *rem_addr,
                         int addr_len, void *token,
                         pjsip_transport_callback callback);

        ssize_t onRecv();

        pj_status_t shutdown();

        pj_status_t destroy();
};

} // namespace ring
