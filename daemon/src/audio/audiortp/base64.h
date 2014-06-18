/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
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
uint8_t *sfl_base64_encode(const uint8_t *data,
                           size_t input_length, size_t *output_length);

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
uint8_t *sfl_base64_decode(const uint8_t *data,
                           size_t input_length, size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif // H_BASE64
