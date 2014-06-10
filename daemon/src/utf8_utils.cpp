/*
 *  Copyright (C) 1999 Tom Tromey
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Pascal Potvin <pascal.potvin@extenway.com>
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


#include <string.h>
#include <assert.h>
#include "utf8_utils.h"

/*
 * The LIKELY and UNLIKELY macros let the programmer give hints to
 * the compiler about the expected result of an expression. Some compilers
 * can use this information for optimizations.
 */
#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define LIKELY(expr) (__builtin_expect (expr, 1))
#define UNLIKELY(expr) (__builtin_expect (expr, 0))
#else
#define LIKELY(expr) (expr)
#define UNLIKELY(expr) (expr)
#endif


/*
 * Check whether a Unicode (5.2) char is in a valid range.
 *
 * The first check comes from the Unicode guarantee to never encode
 * a point above 0x0010ffff, since UTF-16 couldn't represent it.
 *
 * The second check covers surrogate pairs (category Cs).
 *
 * @param Char the character
 */
#define UNICODE_VALID(Char)                   \
    ((Char) < 0x110000 &&                     \
     (((Char) & 0xFFFFF800) != 0xD800))

#define CONTINUATION_CHAR                           \
  if ((*(unsigned char *)p & 0xc0) != 0x80) /* 10xxxxxx */ \
    goto error;                                     \
  val <<= 6;                                        \
  val |= (*(unsigned char *)p) & 0x3f;

static const char *
fast_validate(const char *str)
{
    char32_t val = 0;
    char32_t min = 0;
    const char *p;

    for (p = str; *p; p++) {
        if (*(unsigned char *)p < 128)
            /* done */;
        else {
            const char *last;

            last = p;

            if ((*(unsigned char *)p & 0xe0) == 0xc0) { /* 110xxxxx */
                if (UNLIKELY((*(unsigned char *)p & 0x1e) == 0))
                    goto error;

                p++;

                if (UNLIKELY((*(unsigned char *)p & 0xc0) != 0x80))  /* 10xxxxxx */
                    goto error;
            } else {
                if ((*(unsigned char *)p & 0xf0) == 0xe0) { /* 1110xxxx */
                    min = (1 << 11);
                    val = *(unsigned char *)p & 0x0f;
                    goto TWO_REMAINING;
                } else if ((*(unsigned char *)p & 0xf8) == 0xf0) { /* 11110xxx */
                    min = (1 << 16);
                    val = *(unsigned char *)p & 0x07;
                } else
                    goto error;

                p++;
                CONTINUATION_CHAR;
TWO_REMAINING:
                p++;
                CONTINUATION_CHAR;
                p++;
                CONTINUATION_CHAR;

                if (UNLIKELY(val < min))
                    goto error;

                if (UNLIKELY(!UNICODE_VALID(val)))
                    goto error;
            }

            continue;

error:
            return last;
        }
    }

    return p;
}

static const char *
fast_validate_len(const char *str, ssize_t      max_len)
{
    char32_t val = 0;
    char32_t min = 0;
    const char *p;

    assert(max_len >= 0);

    for (p = str; ((p - str) < max_len) && *p; p++) {
        if (*(unsigned char *)p < 128)
            /* done */;
        else {
            const char *last;

            last = p;

            if ((*(unsigned char *)p & 0xe0) == 0xc0) { /* 110xxxxx */
                if (UNLIKELY(max_len - (p - str) < 2))
                    goto error;

                if (UNLIKELY((*(unsigned char *)p & 0x1e) == 0))
                    goto error;

                p++;

                if (UNLIKELY((*(unsigned char *)p & 0xc0) != 0x80))  /* 10xxxxxx */
                    goto error;
            } else {
                if ((*(unsigned char *)p & 0xf0) == 0xe0) { /* 1110xxxx */
                    if (UNLIKELY(max_len - (p - str) < 3))
                        goto error;

                    min = (1 << 11);
                    val = *(unsigned char *)p & 0x0f;
                    goto TWO_REMAINING;
                } else if ((*(unsigned char *)p & 0xf8) == 0xf0) { /* 11110xxx */
                    if (UNLIKELY(max_len - (p - str) < 4))
                        goto error;

                    min = (1 << 16);
                    val = *(unsigned char *)p & 0x07;
                } else
                    goto error;

                p++;
                CONTINUATION_CHAR;
TWO_REMAINING:
                p++;
                CONTINUATION_CHAR;
                p++;
                CONTINUATION_CHAR;

                if (UNLIKELY(val < min))
                    goto error;

                if (UNLIKELY(!UNICODE_VALID(val)))
                    goto error;
            }

            continue;

error:
            return last;
        }
    }

    return p;
}

bool
utf8_validate(const char *str, ssize_t max_len, const char **end)
{
    const char *p;

    if (max_len < 0)
        p = fast_validate(str);
    else
        p = fast_validate_len(str, max_len);

    if (end)
        *end = p;

    if ((max_len >= 0 && p != str + max_len) ||
            (max_len < 0 && *p != '\0'))
        return false;
    else
        return true;
}


std::string
utf8_make_valid(const char *name)
{
    char *string, *pos;
    const char *remainder, *invalid;
    ssize_t remaining_bytes, valid_bytes;

    string = NULL;

    assert(name != NULL);

    remainder = name;
    remaining_bytes = strlen(name);

    while (remaining_bytes != 0) {
        if (utf8_validate(remainder, remaining_bytes, &invalid))
            break;

        valid_bytes = invalid - remainder;

        if (string == NULL)
            // If every byte is replaced by U+FFFD, max(strlen(string)) == 3 * strlen(name)
            string = (char*) malloc(sizeof(char) * 3 * remaining_bytes);

        pos = string;

        strncpy(pos, remainder, valid_bytes);
        pos += valid_bytes;

        /* append U+FFFD REPLACEMENT CHARACTER */
        pos[0] = '\357';
        pos[1] = '\277';
        pos[2] = '\275';

        pos += 3;

        remaining_bytes -= valid_bytes + 1;
        remainder = invalid + 1;
    }

    if (string == NULL)
        return std::string(name);

    strncpy(pos, remainder, remaining_bytes);
    pos += remaining_bytes;

    std::string answer(string, pos - string);
    assert(utf8_validate(answer.c_str(), -1, NULL));

    free(string);

    return answer;
}
