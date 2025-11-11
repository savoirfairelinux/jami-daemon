/*
 *  Copyright (C) 1999 Tom Tromey
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  Author: Pascal Potvin <pascal.potvin@extenway.com>
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

#include "connectivity/utf8_utils.h"

#include <simdutf.h>

#include <cstring>
#include <cassert>

namespace jami {

bool
utf8_validate(std::string_view str)
{
    return simdutf::validate_utf8(str.data(), str.size());
}

std::string
utf8_make_valid(std::string_view name)
{
    ssize_t remaining_bytes = name.size();
    const char* remainder = name.data();
    char* str = nullptr;
    char* pos;

    while (remaining_bytes != 0) {
        auto [err, valid_bytes] = simdutf::validate_utf8_with_errors(remainder, remaining_bytes);
        if (!err)
            break;

        if (str == nullptr)
            // If every byte is replaced by U+FFFD, max(strlen(string)) == 3 * name.size()
            str = new char[3 * remaining_bytes];

        pos = str;

        strncpy(pos, remainder, valid_bytes);
        pos += valid_bytes;

        /* append U+FFFD REPLACEMENT CHARACTER */
        pos[0] = '\357';
        pos[1] = '\277';
        pos[2] = '\275';

        pos += 3;

        remaining_bytes -= valid_bytes + 1;
        remainder = name.data() + valid_bytes + 1;
    }

    if (str == NULL)
        return std::string(name);

    strncpy(pos, remainder, remaining_bytes);
    pos += remaining_bytes;

    std::string answer(str, pos - str);
    assert(utf8_validate(answer));

    delete[] str;

    return answer;
}

} // namespace jami
