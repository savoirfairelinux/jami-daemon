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

#include "connectivity/utf8_utils.h"

#include <simdutf.h>

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
    std::string sanitized;
    while (!name.empty()) {
        const auto [err, valid_bytes] = simdutf::validate_utf8_with_errors(name.data(), name.size());
        if (!err) {
            sanitized.append(name.data(), name.size());
            break;
        }
        if (sanitized.empty())
            sanitized.reserve(name.size());
        sanitized.append(name.data(), valid_bytes);
        sanitized.append("\xEF\xBF\xBD", 3); // append U+FFFD replacement character
        name.remove_prefix(valid_bytes + 1);
    }

    assert(utf8_validate(sanitized));
    return sanitized;
}

} // namespace jami
