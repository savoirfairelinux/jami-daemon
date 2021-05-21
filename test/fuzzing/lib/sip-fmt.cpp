/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
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

#include <cassert>

#include "lib/sip-fmt.h"

SIPFmt::SIPFmt(const std::vector<uint8_t>& data)
    : isValid_(false)

{
    parse(data);
}

void
SIPFmt::pushBody(char byte)
{
    body_.emplace_back(byte);
}

void
SIPFmt::setField(const std::string& field)
{
    size_t at = field.find_first_of(':');

    assert(at != std::string::npos);

    setFieldValue(field.substr(0, at), field.substr(at + 1));
}

void
SIPFmt::setFieldValue(const std::string& field, const std::string& value)
{
    std::string fieldLow;

    fieldLow.reserve(field.size());

    for (auto it = field.cbegin(); it != field.cend(); ++it) {
        fieldLow.push_back(tolower(*it));
    }

    fields_[fieldLow] = value;
}

void
SIPFmt::swap(std::vector<uint8_t>& with)
{
    if (not isValid_) {
        return;
    }

    std::vector<uint8_t> data;

    auto push_str = [&](const std::string& str) {
        for (auto it = str.cbegin(); it != str.cend(); ++it) {
            data.emplace_back((uint8_t) *it);
        }
    };

    auto push_CRLN = [&] {
        data.emplace_back((uint8_t) '\r');
        data.emplace_back((uint8_t) '\n');
    };

    if (isResponse()) {
        push_str(version_);
        data.emplace_back((uint8_t) ' ');
        push_str(status_);
        data.emplace_back((uint8_t) ' ');
        push_str(msg_);
        push_CRLN();
    } else {
        push_str(method_);
        data.emplace_back(' ');
        push_str(URI_);
        data.emplace_back(' ');
        push_str(version_);
        push_CRLN();
    }

    for (auto it = fields_.cbegin(); it != fields_.cend(); ++it) {
        push_str(it->first);
        data.emplace_back((uint8_t) ':');
        data.emplace_back((uint8_t) ' ');

        push_str(it->second);
        push_CRLN();
    }

    for (auto it = body_.begin(); it != body_.end(); ++it) {
        data.emplace_back((uint8_t) *it);
    }

    data.shrink_to_fit();

    data.swap(with);
}
