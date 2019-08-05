/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>

namespace jami { namespace upnp {

enum class PortType {
    UDP,
    TCP
};

class Mapping
{
public:
    constexpr static const char* UPNP_DEFAULT_MAPPING_DESCRIPTION = "DRING";
    constexpr static uint16_t UPNP_PORT_MIN = 1024;
    constexpr static uint16_t UPNP_PORT_MAX = 65535;

    Mapping(uint16_t portExternal = 0, uint16_t portInternal = 0,
            PortType type = PortType::UDP, bool unique = true,
            const std::string& description = UPNP_DEFAULT_MAPPING_DESCRIPTION);
    Mapping(Mapping&& other) noexcept;
    Mapping(const Mapping& other) noexcept;
    ~Mapping() = default;

    Mapping& operator=(Mapping&&) noexcept;
    bool operator==(const Mapping& other) const noexcept;
    bool operator!=(const Mapping& other) const noexcept;
    bool operator<(const Mapping& other) const noexcept;
    bool operator>(const Mapping& other) const noexcept;
    bool operator<=(const Mapping& other) const noexcept;
    bool operator>=(const Mapping& other) const noexcept;

    uint16_t     getPortExternal()    const { return portExternal_;                          }
    std::string  getPortExternalStr() const { return std::to_string(portExternal_);          }
    uint16_t     getPortInternal()    const { return portInternal_;                          }
    std::string  getPortInternalStr() const { return std::to_string(portInternal_);          }
    PortType     getType()            const { return type_;                                  }
    std::string  getTypeStr()         const { return type_ == PortType::UDP ? "UDP" : "TCP"; }
    bool         isUnique()           const { return unique_;                                }
    std::string  getDescription()     const { return description_;                           }

    std::string toString() const;
    bool isValid() const;

public:
    std::chrono::system_clock::time_point renewal_ {std::chrono::system_clock::time_point::min()};

protected:
    uint16_t portExternal_;
    uint16_t portInternal_;
    PortType type_;
    bool unique_;
    std::string description_;
};

}} // namespace jami::upnp