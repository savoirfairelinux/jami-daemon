/*
 *  Copyright (C) 2014-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>

#ifdef __ANDROID__
#include <sstream>
#endif

namespace ring {

constexpr static const char* TRUE_STR = "true";
constexpr static const char* FALSE_STR = "false";

#ifdef __ANDROID__

template <typename T>
std::string to_string(T &&value)
{
    std::ostringstream os;

    os << value;
    return os.str();
}

#else

template <typename T>
std::string to_string(T &&value)
{
    return std::to_string(std::forward<T>(value));
}

#endif

std::string trim(const std::string &s);

std::vector<std::string>
split_string(const std::string& s, char sep);

std::vector<unsigned>
split_string_to_unsigned(const std::string& s, char sep);

} // namespace ring

#endif
