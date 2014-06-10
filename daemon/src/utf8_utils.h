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

#ifndef H_UTF8_UTILS
#define H_UTF8_UTILS

#include <cstdlib>
#include <string>

/**
 * utf8_validate:
 *
 * Validates UTF-8 encoded text. @str is the text to validate;
 *
 * Returns true if all of @str was valid. Dbus requires valid UTF-8 as input;
 * sip packets should also be encoded in utf8; so data read from a file or the
 * network should be checked with utf8_validate() before doing anything else
 * with it.
 *
 * Returns: true if the text was valid UTF-8
 */

bool
utf8_validate(const std::string & str);

/**
 * utf8_make_valid:
 * @name: a pointer to a nul delimited string.
 *
 * Transforms a unknown c_string into a pretty utf8 encoded std::string.
 * Every unreadable or invalid byte will be transformed into U+FFFD
 * (REPLACEMENT CHARACTER).
 *
 * Returns: a valid utf8 string.
 */
std::string
utf8_make_valid(const std::string & name);

#endif // H_UTF8_UTILS
