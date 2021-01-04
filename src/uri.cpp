/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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
#include "uri.h"

namespace jami {

URI::URI(const std::string& uri)
{
    // TODO better handling of URI, for now it's only used for
    // setMessageDisplayed to differentiate swarm:xxx
    scheme_ = URI::SchemeType::JAMI;
    auto posSep = uri.find(":");
    if (posSep != std::string::npos) {
        auto scheme_str = uri.substr(0, posSep);
        if (scheme_str == "sip")
            scheme_ = URI::SchemeType::SIP_HOST;
        else if (scheme_str == "swarm")
            scheme_ = URI::SchemeType::SWARM;
        else if (scheme_str == "jami")
            scheme_ = URI::SchemeType::JAMI;
        else
            scheme_ = URI::SchemeType::UNRECOGNIZED;
        userinfo_ = uri.substr(posSep+1);
    } else {
        userinfo_ = uri;
    }

}

std::string
URI::userinfo() const
{
    return userinfo_;
}

URI::SchemeType
URI::scheme() const
{
    return scheme_;
}

std::string
URI::full() const
{
    return schemeToString() + ":" + userinfo_;
}

std::string
URI::schemeToString() const
{
    switch (scheme_)
    {
    case URI::SchemeType::SIP_HOST:
    case URI::SchemeType::SIP_OTHER:
        return "sip";
    case URI::SchemeType::SWARM:
        return "swarm";
    case URI::SchemeType::JAMI:
    case URI::SchemeType::UNRECOGNIZED:
    default:
        return "jami";
    }
}

} // namespace jami
