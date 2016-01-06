/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "secure_ice.h"

namespace ring { namespace ice {

SecureIceTransport::SecureIceTransport(const char* name, int component_count, bool master,
                                       const TlsParams& tls_params,
                                       const IceTransportOptions& options = {})
    : IceTransport(name, component_count, master, options)
    , tlsParams(tls_params)
    , tlsThread_([this]{ return setup(); }, [this]{ loop(); }, [this]{ clean(); })
{
    gnutls_priority_init(&priority_cache_,
                         "SECURE192:-VERS-TLS-ALL:+VERS-DTLS-ALL:%SERVER_PRECEDENCE",
                         nullptr);

    tlsThread_.start();
    Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
}


SecureIceTransport::~SecureIceTransport()
{
    const auto was_established = state_ == TlsConnectionState::ESTABLISHED;
    tlsThread_.join();
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    handleEvents(); // process latest incoming packets
}

SecureIceTransport::stop()
{
}

}} // namespace ring::ice
