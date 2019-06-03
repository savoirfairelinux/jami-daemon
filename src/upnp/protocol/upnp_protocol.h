/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include <string>
#include <map>
#include <functional>
#include <chrono>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include "../igd/igd.h"
#include "../mapping/mapping.h"

namespace jami { namespace upnp {

// Pure virtual interface class that UPnPContext uses to call protocol functions.
class UPnPProtocol 
{
public:
    UPnPProtocol();
    virtual ~UPnPProtocol();

    // Signals a change in the network.
    virtual void connectivityChanged() = 0;

    // Search for IGD.
    virtual void searchForIGD() = 0;

    // Tries to add mapping. Assumes mutex is already locked.
    virtual Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error) = 0;

    // Removes a mapping.
    virtual void removeMapping(const Mapping& mapping) = 0;  

    // Removes all local mappings of IGD that we're added by the application.
    virtual void removeAllLocalMappings(IGD* igd) = 0;

    // Add IGD listener.
    virtual size_t addIGDListener(IGDFoundCallback&& cb) = 0;
    
    // Remove IGD listener.
    virtual void removeIGDListener(size_t token) = 0;
};

}} // namespace jami::upnp