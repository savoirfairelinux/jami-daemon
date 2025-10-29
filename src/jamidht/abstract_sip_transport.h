/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <dhtnet/ip_utils.h>
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
    static_assert(std::is_standard_layout<TransportData>::value, "TransportData requires standard-layout");

    virtual ~AbstractSIPTransport() {};

    virtual pjsip_transport* getTransportBase() = 0;

    virtual dhtnet::IpAddr getLocalAddress() const = 0;
};

} // namespace tls
} // namespace jami
