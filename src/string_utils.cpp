/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "string_utils.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <sstream>
#include <cctype>
#include <algorithm>
#include <ostream>
#include <iomanip>
#include <stdexcept>
#include <ios>
#include <charconv>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif

#include <ciso646> // fix windows compiler bug

namespace jami {

#ifdef _WIN32
std::wstring
to_wstring(const std::string& str, int codePage)
{
    int srcLength = (int) str.length();
    int requiredSize = MultiByteToWideChar(codePage, 0, str.c_str(), srcLength, nullptr, 0);
    if (!requiredSize) {
        throw std::runtime_error("Can't convert string to wstring");
    }
    std::wstring result((size_t) requiredSize, 0);
    if (!MultiByteToWideChar(codePage, 0, str.c_str(), srcLength, &(*result.begin()), requiredSize)) {
        throw std::runtime_error("Can't convert string to wstring");
    }
    return result;
}

std::string
to_string(const std::wstring& wstr, int codePage)
{
    int srcLength = (int) wstr.length();
    int requiredSize = WideCharToMultiByte(codePage, 0, wstr.c_str(), srcLength, nullptr, 0, 0, 0);
    if (!requiredSize) {
        throw std::runtime_error("Can't convert wstring to string");
    }
    std::string result((size_t) requiredSize, 0);
    if (!WideCharToMultiByte(
            codePage, 0, wstr.c_str(), srcLength, &(*result.begin()), requiredSize, 0, 0)) {
        throw std::runtime_error("Can't convert wstring to string");
    }
    return result;
}
#endif

std::string
to_string(double value)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%-.*G", 16, value);
    if (len <= 0)
        throw std::invalid_argument {"can't parse double"};
    return {buf, (size_t) len};
}

std::string
to_hex_string(uint64_t id)
{
    return fmt::format("{:016x}", id);
}

uint64_t
from_hex_string(const std::string& str)
{
    uint64_t id;
    if (auto [p, ec] = std::from_chars(str.data(), str.data()+str.size(), id, 16); ec != std::errc()) {
        throw std::invalid_argument("Can't parse id: " + str);
    }
    return id;
}

std::string_view
trim(std::string_view s)
{
    auto wsfront = std::find_if_not(s.cbegin(), s.cend(), [](int c) { return std::isspace(c); });
    return std::string_view(&*wsfront, std::find_if_not(s.rbegin(),
                                        std::string_view::const_reverse_iterator(wsfront),
                                        [](int c) { return std::isspace(c); })
                           .base() - wsfront);
}

std::vector<unsigned>
split_string_to_unsigned(std::string_view str, char delim)
{
    std::vector<unsigned> output;
    for (auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + 1) {
        second = std::find(first, last, delim);
        if (first != second) {
            unsigned result;
            auto [p, ec] = std::from_chars(first, second, result);
            if (ec ==  std::errc())
                output.emplace_back(result);
        }
    }
    return output;
}

void
string_replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

std::string_view
string_remove_suffix(std::string_view str, char separator)
{
    auto it = str.find(separator);
    if (it != std::string_view::npos)
        str = str.substr(0, it);
    return str;
}

std::string
string_join(const std::set<std::string>& set, std::string_view separator)
{
    return fmt::format("{}", fmt::join(set, separator));
}

std::set<std::string>
string_split_set(std::string& str, std::string_view separator)
{
    std::set<std::string> output;
    for (auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + 1) {
        second = std::find_first_of(first, last, std::cbegin(separator), std::cend(separator));
        if (first != second)
            output.emplace(first, second - first);
    }
    return output;
}

} // namespace jami
