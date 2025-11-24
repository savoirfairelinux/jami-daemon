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

#include "base64.h"
#include "connectivity/sip_utils.h"

#include <simdutf.h>

namespace jami {
namespace base64 {

std::string
encode(std::string_view str)
{
    std::string buffer(simdutf::base64_length_from_binary(str.size()), '\0');
    simdutf::binary_to_base64((const char*) str.data(), str.size(), buffer.data());
    return buffer;
}

std::vector<unsigned char>
decode(std::string_view str)
{
    std::vector<unsigned char> buffer(simdutf::maximal_binary_length_from_base64(str.data(), str.size()));
    const simdutf::result r = simdutf::base64_to_binary(str.data(), str.size(), (char*) buffer.data());
    if (r.error)
        throw base64_exception();
    buffer.resize(r.count); // resize the buffer according to actual number of bytes
    return buffer;
}

} // namespace base64
} // namespace jami
