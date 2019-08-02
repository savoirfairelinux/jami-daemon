/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
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
#include <sstream>
#include <cctype>
#include <algorithm>
#include <ostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif

#include <ciso646> // fix windows compiler bug

namespace jami {

#ifdef _WIN32

std::wstring
to_wstring(const std::string& s)
{
    int slength = (int)s.length();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, nullptr, 0);
    if (not len)
        throw std::runtime_error("Can't convert string to wchar");
    std::wstring r((size_t)len, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, &(*r.begin()), len))
        throw std::runtime_error("Can't convert string to wchar");
    return r;
}

std::string
decodeMultibyteString(const std::string& s)
{
    if (not s.length())
        return {};
    auto wstr = to_wstring(s);
    return std::string(wstr.begin(), wstr.end());
}

std::string
bstrToStdString(BSTR bstr)
{
    int wslen = ::SysStringLen(bstr);
    if (wslen != 0) {
        std::wstring wstr(bstr, wslen);
        return std::string(wstr.begin(), wstr.end());
    }
    return {};
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

} // namespace jami
