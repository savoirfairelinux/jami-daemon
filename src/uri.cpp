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

using namespace std::string_view_literals;

namespace jami {

Uri::Uri(std::string_view uri)
{
    // TODO better handling of Uri, for now it's only used for
    // setMessageDisplayed to differentiate swarm:xxx
    scheme_ = Uri::Scheme::JAMI;
    auto posSep = uri.find(':');
    if (posSep != std::string_view::npos) {
        auto scheme_str = uri.substr(0, posSep);
        if (scheme_str == "sip"sv)
            scheme_ = Uri::Scheme::SIP;
        else if (scheme_str == "swarm"sv)
            scheme_ = Uri::Scheme::SWARM;
        else if (scheme_str == "jami"sv)
            scheme_ = Uri::Scheme::JAMI;
        else if (scheme_str == "data-transfer"sv)
            scheme_ = Uri::Scheme::DATA_TRANSFER;
        else if (scheme_str == "git"sv)
            scheme_ = Uri::Scheme::GIT;
        else if (scheme_str == "sync"sv)
            scheme_ = Uri::Scheme::SYNC;
        else
            scheme_ = Uri::Scheme::UNRECOGNIZED;
        authority_ = uri.substr(posSep + 1);
    } else {
        authority_ = uri;
    }
}

const std::string&
Uri::authority() const
{
    return authority_;
}

Uri::Scheme
Uri::scheme() const
{
    return scheme_;
}

std::string
Uri::toString() const
{
    return schemeToString() + ":" + authority_;
}

std::string
Uri::schemeToString() const
{
    switch (scheme_) {
    case Uri::Scheme::SIP:
        return "sip";
    case Uri::Scheme::SWARM:
        return "swarm";
    case Uri::Scheme::GIT:
        return "git";
    case Uri::Scheme::SYNC:
        return "sync";
    case Uri::Scheme::JAMI:
    case Uri::Scheme::UNRECOGNIZED:
    default:
        return "jami";
    }
}

} // namespace jami
