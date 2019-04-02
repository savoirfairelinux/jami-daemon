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

#include "upnp_igd.h"

namespace jami { namespace upnp {

/* move constructor and operator */
Mapping::Mapping(Mapping&& other) noexcept
    : port_external_(other.port_external_)
    , port_internal_(other.port_internal_)
    , type_(other.type_)
    , description_(std::move(other.description_))
{
    other.port_external_ = 0;
    other.port_internal_ = 0;
}

Mapping& Mapping::operator=(Mapping&& other) noexcept
{
    if (this != &other) {
        port_external_ = other.port_external_;
        other.port_external_ = 0;
        port_internal_ = other.port_internal_;
        other.port_internal_ = 0;
        type_ = other.type_;
        description_ = std::move(other.description_);
    }
    return *this;
}

bool operator== (const Mapping& cMap1, const Mapping& cMap2)
{
    /* we don't compare the description because it doesn't change the function of the
     * mapping; we don't compare the IGD because for now we assume that we always
     * use the same one and that all mappings are active
     */
    return (cMap1.port_external_ == cMap2.port_external_ &&
            cMap1.port_internal_ == cMap2.port_internal_ &&
            cMap1.type_ == cMap2.type_);
}

bool operator!= (const Mapping& cMap1, const Mapping& cMap2)
{
    return !(cMap1 == cMap2);
}

}} // namespace jami::upnp
