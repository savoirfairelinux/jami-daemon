/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "base64.h"
#include "connectivity/sip_utils.h"

#include <pjlib.h>
#include <pjlib-util/base64.h>

namespace jami {
namespace base64 {

std::string
encode(std::string_view dat)
{
    if (dat.empty() || dat.size() > INT_MAX)
        return {};

    int input_length = (int)dat.size();
    int output_length = PJ_BASE256_TO_BASE64_LEN(input_length);
    std::string out;
    out.resize(output_length);

    if (pj_base64_encode((const uint8_t*)dat.data(), input_length, &(*out.begin()), &output_length) != PJ_SUCCESS) {
        throw base64_exception();
    }

    out.resize(output_length);
    return out;
}

std::vector<uint8_t>
decode(std::string_view str)
{
    if (str.empty())
        return {};

    int output_length = PJ_BASE64_TO_BASE256_LEN(str.length());
    auto input = sip_utils::CONST_PJ_STR(str);

    std::vector<uint8_t> out;
    out.resize(output_length);

    if (pj_base64_decode(&input, &(*out.begin()), &output_length) != PJ_SUCCESS) {
        throw base64_exception();
    }

    out.resize(output_length);
    return out;
}

} // namespace base64
} // namespace jami
