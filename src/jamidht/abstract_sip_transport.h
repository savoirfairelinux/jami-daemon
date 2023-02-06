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

#include "connectivity/ip_utils.h"
#include "connectivity/sip_utils.h"

#include <pjsip.h>
#include <pj/pool.h>

namespace jami {

namespace tls {

/**
 * AbstractSIPTransport
 *
 * Implements a pjsip_transport on top
 */
class AbstractSIPTransport
{
public:
    struct TransportData
    {
        pjsip_transport base; // do not move, SHOULD be the fist member
        AbstractSIPTransport* self {nullptr};
    };
    static_assert(std::is_standard_layout<TransportData>::value,
                  "TransportData requires standard-layout");

    virtual ~AbstractSIPTransport() {};

    virtual pjsip_transport* getTransportBase() = 0;

    virtual IpAddr getLocalAddress() const = 0;
};

} // namespace tls
} // namespace jami
