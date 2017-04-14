/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <pjlib.h>
#include <pjlib-util/base64.h>

namespace ring {
namespace base64 {

std::string
encode(const std::vector<uint8_t>::const_iterator begin,
       const std::vector<uint8_t>::const_iterator end)
{
    int input_length = std::distance(begin, end);
    int output_length = 4 * ((input_length + 2) / 3);

    char output_buffer[output_length];
    if( pj_base64_encode(&(*begin), input_length, output_buffer, &output_length) != PJ_SUCCESS) {
        throw new base64_exception();
    }

    return std::string(output_buffer, output_length);
}

std::string
encode(const std::vector<uint8_t>& dat)
{
    return encode(dat.cbegin(), dat.cend());
}

std::vector<uint8_t>
decode(const std::string& str)
{
    int output_length = str.length() / 4 * 3 + 2;
    pj_str_t input;
    char* tempstr = new char [str.length()+1];
    strcpy (tempstr, str.c_str());
    pj_strset(&input, tempstr, str.length());
    uint8_t output[output_length];

    if(pj_base64_decode(&input, output, &output_length) != PJ_SUCCESS) {
        throw new base64_exception();
    }

    return std::vector<uint8_t>(output, output + output_length);
}

}} // namespace ring::base64
