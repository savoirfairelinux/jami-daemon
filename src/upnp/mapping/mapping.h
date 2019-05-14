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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <map>
#include <functional>
#include <chrono>

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

namespace jami { namespace upnp {

enum class PortType { UDP, TCP };

class Mapping 
{
public:
    constexpr static const char * UPNP_DEFAULT_MAPPING_DESCRIPTION = "RING";

    constexpr static uint16_t UPNP_PORT_MIN = 1024;
    constexpr static uint16_t UPNP_PORT_MAX = 65535;

    Mapping(uint16_t port_external = 0,
            uint16_t port_internal = 0,
            PortType type = PortType::UDP,
            const std::string& description = UPNP_DEFAULT_MAPPING_DESCRIPTION): 
            port_external_(port_external), 
            port_internal_(port_internal), 
            type_(type), 
            description_(description)
    {};
    Mapping(Mapping&&) noexcept;    /* Move constructor. */
    ~Mapping() = default;

    /* Operators. */
    Mapping& operator=(Mapping&&) noexcept;
    friend bool operator== (const Mapping& cRedir1, const Mapping& cRedir2);
    friend bool operator!= (const Mapping& cRedir1, const Mapping& cRedir2);

    uint16_t     getPortExternal()    const { return port_external_;                         }
    std::string  getPortExternalStr() const { return jami::to_string(port_external_);        }
    uint16_t     getPortInternal()    const { return port_internal_;                         }
    std::string  getPortInternalStr() const { return jami::to_string(port_internal_);        }
    PortType     getType()            const { return type_;                                  }
    std::string  getTypeStr()         const { return type_ == PortType::UDP ? "UDP" : "TCP"; }
    std::string  getDescription()     const { return description_;                           }

    std::string toString() const;
    bool isValid() const;

    inline explicit operator bool() const { return isValid(); }

#if HAVE_LIBNATPMP
    std::chrono::system_clock::time_point renewal_ {std::chrono::system_clock::time_point::min()};
    bool remove {false};
#endif

private:
    NON_COPYABLE(Mapping);

protected:
    uint16_t port_external_;
    uint16_t port_internal_;
    PortType type_;                 /* UPD or TCP */
    std::string description_;
};

}} // namespace jami::upnp