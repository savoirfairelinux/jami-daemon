/*
 *  Copyright (C) 2014-2016 Savoir-faire Linux Inc.
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

constexpr static const char*
bool_to_str(bool b) noexcept
{
    return b ? TRUE_STR : FALSE_STR;
}

std::string to_string(double value);

#ifdef _WIN32

std::wstring to_wstring(const std::string& s);

#endif

#ifdef __ANDROID__

// Rationale:
// Some strings functions are not available on Android NDK as explained here:
// http://stackoverflow.com/questions/17950814/how-to-use-stdstoul-and-stdstoull-in-android/18124627#18124627
// We implement them by ourself as well as possible here.

template <typename T>
inline std::string
to_string(T &&value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

static inline int
stoi(const std::string& str)
{
    int v;
    std::istringstream os(str);
    os >> v;
    return v;
}

static inline double
stod(const std::string& str)
{
    double v;
    std::istringstream os(str);
    os >> v;
    return v;
}

#else

template <typename T>
inline std::string
to_string(T &&value)
{
    return std::to_string(std::forward<T>(value));
}

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

#endif

std::string trim(const std::string &s);

std::vector<std::string>
split_string(const std::string& s, char sep);

std::vector<unsigned>
split_string_to_unsigned(const std::string& s, char sep);

} // namespace ring

#endif
