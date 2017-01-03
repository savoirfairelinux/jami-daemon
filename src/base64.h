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

#pragma once

#include <stdint.h>
#include <stddef.h>

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

#include <string>
#include <vector>

namespace ring {
namespace base64 {

std::string encode(const std::vector<uint8_t>::const_iterator begin, const std::vector<uint8_t>::const_iterator end);
std::string encode(const std::vector<uint8_t>& dat);
std::vector<uint8_t> decode(const std::string& str);

}} // namespace ring::base64
