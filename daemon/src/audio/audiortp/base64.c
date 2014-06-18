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

#include <stdint.h>
#include <stdlib.h>

/* Mainly based on the following stackoverflow question:
 * http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
 */
static const uint8_t encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
    's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2',
    '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static const int mod_table[] = { 0, 2, 1 };

uint8_t *sfl_base64_encode(const uint8_t *data,
                           size_t input_length, size_t *output_length)
{
    int i, j;
    uint8_t *encoded_data;
    *output_length = 4 * ((input_length + 2) / 3);

    encoded_data = (uint8_t *)malloc(*output_length);
    if (encoded_data == NULL)
        return NULL;

    for (i = 0, j = 0; i < input_length; ) {
        uint8_t octet_a = i < input_length ? data[i++] : 0;
        uint8_t octet_b = i < input_length ? data[i++] : 0;
        uint8_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}

uint8_t *sfl_base64_decode(const uint8_t *data,
                           size_t input_length, size_t *output_length)
{
    int i, j;
    uint8_t decoding_table[256];
    unsigned char *decoded_data;

    for (i = 0; i < 64; i++)
        decoding_table[(uint8_t) encoding_table[i]] = i;

    if (input_length % 4 != 0)
        return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=')
        (*output_length)--;
    if (data[input_length - 2] == '=')
        (*output_length)--;

    decoded_data = (unsigned char *)malloc(*output_length);
    if (decoded_data == NULL)
        return NULL;

    for (i = 0, j = 0; i < input_length;) {
        uint8_t sextet_a = data[i] == '=' ? 0 & i++
                                          : decoding_table[data[i++]];
        uint8_t sextet_b = data[i] == '=' ? 0 & i++
                                          : decoding_table[data[i++]];
        uint8_t sextet_c = data[i] == '=' ? 0 & i++
                                          : decoding_table[data[i++]];
        uint8_t sextet_d = data[i] == '=' ? 0 & i++
                                          : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6) +
                          (sextet_b << 2 * 6) +
                          (sextet_c << 1 * 6) +
                          (sextet_d << 0 * 6);

        if (j < *output_length)
            decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length)
            decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length)
            decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return decoded_data;
}
