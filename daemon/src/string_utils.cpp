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

#include "string_utils.h"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace ring {

std::string
trim(const std::string &s)
{
   auto wsfront = std::find_if_not(s.cbegin(),s.cend(), [](int c){return std::isspace(c);});
   return std::string(wsfront, std::find_if_not(s.rbegin(),std::string::const_reverse_iterator(wsfront), [](int c){return std::isspace(c);}).base());
}

std::vector<std::string>
split_string(const std::string &s, char delim)
{
    std::vector<std::string> result;
    std::string token;
    std::istringstream ss(s);

    while (std::getline(ss, token, delim))
        if (not token.empty())
            result.emplace_back(token);
    return result;
}

std::vector<unsigned>
split_string_to_unsigned(const std::string &s, char delim)
{
    std::vector<unsigned> result;
    std::string token;
    std::istringstream ss(s);

    while (std::getline(ss, token, delim))
        if (not token.empty())
            result.emplace_back(std::stoi(token));
    return result;
}

} // namespace ring
