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
#include "uri.h"

#include <fmt/format.h>
#include <fmt/compile.h>

namespace jami {

Uri::Uri(std::string_view uri)
{
    // TODO better handling of Uri, for now it's only used for
    // setMessageDisplayed to differentiate swarm:xxx
    scheme_ = Uri::Scheme::JAMI;
    auto posSep = uri.find(':');
    if (posSep != std::string::npos) {
        auto scheme_str = uri.substr(0, posSep);
        if (scheme_str == "sip")
            scheme_ = Uri::Scheme::SIP;
        else if (scheme_str == "swarm")
            scheme_ = Uri::Scheme::SWARM;
        else if (scheme_str == "jami")
            scheme_ = Uri::Scheme::JAMI;
        else if (scheme_str == "data-transfer")
            scheme_ = Uri::Scheme::DATA_TRANSFER;
        else if (scheme_str == "git")
            scheme_ = Uri::Scheme::GIT;
        else if (scheme_str == "rdv")
            scheme_ = Uri::Scheme::RENDEZVOUS;
        else if (scheme_str == "sync")
            scheme_ = Uri::Scheme::SYNC;
        else if (scheme_str == "msg")
            scheme_ = Uri::Scheme::MESSAGE;
        else if (scheme_str == "auth")
            scheme_ = Uri::Scheme::AUTH;
        else
            scheme_ = Uri::Scheme::UNRECOGNIZED;
        authority_ = uri.substr(posSep + 1);
    } else {
        authority_ = uri;
    }
    auto posParams = authority_.find(';');
    if (posParams != std::string::npos) {
        authority_ = authority_.substr(0, posParams);
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
    return fmt::format(FMT_COMPILE("{}:{}"), schemeToString(), authority_);
}

constexpr std::string_view
Uri::schemeToString() const
{
    switch (scheme_) {
    case Uri::Scheme::SIP:
        return "sip"sv;
    case Uri::Scheme::SWARM:
        return "swarm"sv;
    case Uri::Scheme::RENDEZVOUS:
        return "rdv"sv;
    case Uri::Scheme::GIT:
        return "git"sv;
    case Uri::Scheme::SYNC:
        return "sync"sv;
    case Uri::Scheme::MESSAGE:
        return "msg"sv;
    case Uri::Scheme::AUTH:
        return "auth"sv;
    case Uri::Scheme::JAMI:
    case Uri::Scheme::UNRECOGNIZED:
    default:
        return "jami"sv;
    }
}

} // namespace jami
