/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include "mapping.h"

namespace jami { namespace upnp {

Mapping::Mapping(uint16_t portExternal, uint16_t portInternal,
                 PortType type, bool unique, const std::string& description):
                 portExternal_(portExternal),
                 portInternal_(portInternal),
                 type_(type),
                 unique_(unique),
                 description_(description)
{
};

Mapping::Mapping(Mapping&& other) noexcept:
    portExternal_(other.portExternal_),
    portInternal_(other.portInternal_),
    type_(other.type_),
    unique_(other.unique_),
    description_(std::move(other.description_))
{
    other.portExternal_ = 0;
    other.portInternal_ = 0;
}

Mapping& Mapping::operator=(Mapping&& other) noexcept
{
    if (this != &other) {
        portExternal_ = other.portExternal_;
        other.portExternal_ = 0;
        portInternal_ = other.portInternal_;
        other.portInternal_ = 0;
        type_ = other.type_;
        description_ = std::move(other.description_);
    }
    return *this;
}

bool operator==(const Mapping& cMap1, const Mapping& cMap2)
{
    return (cMap1.portExternal_ == cMap2.portExternal_ &&
            cMap1.portInternal_ == cMap2.portInternal_ &&
            cMap1.type_ == cMap2.type_);
}

bool operator!= (const Mapping& cMap1, const Mapping& cMap2)
{
    return !(cMap1 == cMap2);
}

bool Mapping::operator<(const Mapping& other) const noexcept {
    if (type_ != other.type_)
        return (int)type_ < (int)other.type_;
    if (portExternal_ != other.portExternal_)
        return portExternal_ < other.portExternal_;
    if (portInternal_ != other.portInternal_)
        return portInternal_ < other.portInternal_;
}

std::string
Mapping::toString() const
{
    return getPortExternalStr() + ":" + getPortInternalStr() + " " + getTypeStr();
}

bool
Mapping::isValid() const
{
    return portExternal_ == 0 or portInternal_ == 0 ? false : true;
};

}} // namespace jami::upnp