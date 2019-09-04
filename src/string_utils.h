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

#pragma once

#include <string>
#include <vector>
#ifdef _WIN32
#include <WTypes.h>
#endif

namespace jami {

constexpr static const char* TRUE_STR = "true";
constexpr static const char* FALSE_STR = "false";

constexpr static const char*
bool_to_str(bool b) noexcept
{
    return b ? TRUE_STR : FALSE_STR;
}

std::string to_string(double value);

#ifdef _WIN32
std::wstring to_wstring(const std::string& str, int codePage = CP_UTF8);
std::string to_string(const std::wstring& wstr, int codePage = CP_UTF8);
std::string bstrToStdString(BSTR bstr);
#endif

static inline int
stoi(const std::string& str)
{
    return std::stoi(str);
}

static inline double
stod(const std::string& str)
{
    return std::stod(str);
}

std::string trim(const std::string &s);

std::vector<std::string>
split_string(const std::string& s, char sep);

std::vector<unsigned>
split_string_to_unsigned(const std::string& s, char sep);

void string_replace(std::string& str, const std::string& from, const std::string& to);

} // namespace jami
