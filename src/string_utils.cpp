/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Vsevolod Ivanov <vsevolod.ivanov@savoirfairelinux.com>
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
#include <sstream>
#include <cctype>
#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <iomanip>
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
    int srcLength = (int)str.length();
    int requiredSize = MultiByteToWideChar(codePage, 0, str.c_str(), srcLength, nullptr, 0);
    if (!requiredSize) {
        throw std::runtime_error("Can't convert string to wstring");
    }
    std::wstring result((size_t)requiredSize, 0);
    if (!MultiByteToWideChar(codePage, 0, str.c_str(), srcLength, &(*result.begin()), requiredSize)) {
        throw std::runtime_error("Can't convert string to wstring");
    }
    return result;
}

std::string
to_string(const std::wstring& wstr, int codePage)
{
    int srcLength = (int)wstr.length();
    int requiredSize = WideCharToMultiByte(codePage, 0, wstr.c_str(), srcLength, nullptr, 0, 0, 0);
    if (!requiredSize) {
        throw std::runtime_error("Can't convert wstring to string");
    }
    std::string result((size_t)requiredSize, 0);
    if (!WideCharToMultiByte(codePage, 0, wstr.c_str(), srcLength, &(*result.begin()), requiredSize, 0, 0)) {
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
        throw std::invalid_argument{"can't parse double"};
    return {buf, (size_t)len};
}

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
            result.emplace_back(jami::stoi(token));
    return result;
}

void string_replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

std::string
bytes_to_hex_string(const unsigned char* data, const int size)
{
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < size; i++)
        ss << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}

} // namespace jami
