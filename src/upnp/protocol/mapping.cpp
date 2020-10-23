/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
#include "logger.h"

namespace jami {
namespace upnp {

Mapping::Mapping(uint16_t portExternal,
                 uint16_t portInternal,
                 PortType type,
                 const std::string& description,
                 bool available)
    : portExternal_(portExternal)
    , portInternal_(portInternal)
    , type_(type)
    , description_(description)
    , available_(available)
    , open_(false)
    {};

Mapping::Mapping(Mapping&& other) noexcept
    :
#if HAVE_LIBNATPMP
    renewal_(other.renewal_)
    ,
#endif
    portExternal_(other.portExternal_)
    , portInternal_(other.portInternal_)
    , type_(other.type_)
    , description_(std::move(other.description_))
    , available_(other.available_)
    , open_(other.open_)
{
    other.portExternal_ = 0;
    other.portInternal_ = 0;
}

Mapping::Mapping(const Mapping& other)
    :
#if HAVE_LIBNATPMP
    renewal_(other.renewal_)
    ,
#endif
    portExternal_(other.portExternal_)
    , portInternal_(other.portInternal_)
    , type_(other.type_)
    , description_(std::move(other.description_))
    , available_ (other.available_)
    , open_ (other.open_)
{}

Mapping&
Mapping::operator=(Mapping&& other) noexcept
{
    if (this != &other) {
        portExternal_ = other.portExternal_;
        other.portExternal_ = 0;
        portInternal_ = other.portInternal_;
        other.portInternal_ = 0;
        type_ = other.type_;
        available_ = other.available_;
        other.available_ = false;
        open_ = other.open_;
        description_ = std::move(other.description_);
#if HAVE_LIBNATPMP
        renewal_ = other.renewal_;
#endif
    }
    return *this;
}

bool
Mapping::operator==(const Mapping& other) const noexcept
{
    return (portExternal_ == other.portExternal_ && portInternal_ == other.portInternal_
            && type_ == other.type_);
}

bool
Mapping::operator!=(const Mapping& other) const noexcept
{
    if (type_ != other.type_)
        return true;
    if (portExternal_ != other.portExternal_)
        return true;
    if (portInternal_ != other.portInternal_)
        return true;
    return false;
}

bool
Mapping::operator<(const Mapping& other) const noexcept
{
    if (type_ != other.type_)
        return (int) type_ < (int) other.type_;
    if (portExternal_ != other.portExternal_)
        return portExternal_ < other.portExternal_;
    if (portInternal_ != other.portInternal_)
        return portInternal_ < other.portInternal_;
    return false;
}

bool
Mapping::operator>(const Mapping& other) const noexcept
{
    if (type_ != other.type_)
        return (int) type_ > (int) other.type_;
    if (portExternal_ != other.portExternal_)
        return portExternal_ > other.portExternal_;
    if (portInternal_ != other.portInternal_)
        return portInternal_ > other.portInternal_;
    return false;
}

bool
Mapping::operator<=(const Mapping& other) const noexcept
{
    if (type_ != other.type_)
        return (int) type_ <= (int) other.type_;
    if (portExternal_ != other.portExternal_)
        return portExternal_ <= other.portExternal_;
    if (portInternal_ != other.portInternal_)
        return portInternal_ <= other.portInternal_;
    return false;
}

bool
Mapping::operator>=(const Mapping& other) const noexcept
{
    if (type_ != other.type_)
        return (int) type_ >= (int) other.type_;
    if (portExternal_ != other.portExternal_)
        return portExternal_ >= other.portExternal_;
    if (portInternal_ != other.portInternal_)
        return portInternal_ >= other.portInternal_;
    return false;
}

void
Mapping::setAvailable(bool val)
{
    JAMI_DBG("UPnP: Changing mapping %s state from %s to %s",
        this->toString().c_str(),
        available_ ? "AVAILABLE":"UNAVAILABLE", val ? "AVAILABLE":"UNAVAILABLE");

    available_ = val;
}

void
Mapping::setOpen(bool val)
{
    JAMI_DBG("UPnP: Changing mapping %s state from %s to %s",
        this->toString().c_str(),
        open_ ? "OPEN":"CLOSED", val ? "OPEN":"CLOSED");

    open_ = val;
}

void
Mapping::setDescription(const std::string& descr)
{
    description_ = descr;
}

std::string
Mapping::toString() const
{
    return getPortExternalStr() + ":" + getPortInternalStr() + " [" + getTypeStr() + "]" +
        " \"" + description_.c_str() + "\"";
}

bool
Mapping::isValid() const
{
    return portExternal_ == 0 or portInternal_ == 0 ? false : true;
};

} // namespace upnp
} // namespace jami
