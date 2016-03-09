/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#ifndef H_BASE64
#define H_BASE64

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/**
 * Encode a buffer in base64.
 *
 * @param data          the input buffer
 * @param input_length  the input length
 * @param output_length the resulting output length
 * @return              a base64-encoded buffer
 *
 * @note callers should free the returned memory
 */
char *ring_base64_encode(const uint8_t *input, size_t input_length,
                         char *output, size_t *output_length);

/**
 * Decode a base64 buffer.
 *
 * @param data          the input buffer
 * @param input_length  the input length
 * @param output_length the resulting output length
 * @return              a buffer
 *
 * @note callers should free the returned memory
 */
uint8_t *ring_base64_decode(const char *input, size_t input_length,
                            uint8_t *output, size_t *output_length);

#ifdef __cplusplus
}
#endif

#include <string>
#include <vector>

namespace ring {
namespace base64 {

std::string
encode(const std::vector<uint8_t>::const_iterator begin,
       const std::vector<uint8_t>::const_iterator end)
{
    size_t output_length = 4 * ((std::distance(begin, end) + 2) / 3);
    std::string out;
    out.resize(output_length);
    ring_base64_encode(&(*begin), std::distance(begin, end),
                       &(*out.begin()), &output_length);
    out.resize(output_length);
    return out;
}

std::string
encode(const std::vector<uint8_t>& dat)
{
    return encode(dat.cbegin(), dat.cend());
}

std::vector<uint8_t>
decode(const std::string& str)
{
    size_t output_length = str.length() / 4 * 3 + 2;
    std::vector<uint8_t> output;
    output.resize(output_length);
    ring_base64_decode(str.data(), str.size(), output.data(), &output_length);
    output.resize(output_length);
    return output;
}

}} // namespace ring::base64

#endif // H_BASE64
