/*
 *  Copyright (C) 2021-2024 Savoir-faire Linux Inc.
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
#pragma once

#include <string>
#include <string_view>

namespace jami {

using namespace std::string_view_literals;

static constexpr std::string_view DATA_TRANSFER_SCHEME = "data-transfer://"sv;

class Uri
{
public:
    enum class Scheme {
        JAMI,          // Start with "jami:" and 45 ASCII chars OR 40 ASCII chars
        SIP,           // Start with "sip:"
        SWARM,         // Start with "swarm:" and 40 ASCII chars
        RENDEZVOUS,    // Start wutg "rdv" and used for call in swarms
        GIT,           // Start with "git:"
        DATA_TRANSFER, // Start with "data-transfer://"
        SYNC,          // Start with "sync:"
        AUTH,          // Start with "auth:"
        UNRECOGNIZED   // Anything that doesn't fit in other categories
    };

    Uri(std::string_view uri);

    const std::string& authority() const;
    Scheme scheme() const;
    std::string toString() const;
    // TODO hostname, transport, handle sip:

private:
    std::string schemeToString() const;
    Scheme scheme_;
    std::string authority_;
};
} // namespace jami
