/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include <set>
#include <algorithm>
#include <regex>
#include <iterator>
#ifdef _WIN32
#include <WTypes.h>
#endif

// Add string operators crucially missing from standard
// see https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/1RcShRhrmRc
namespace std {
inline string
operator+(const string& s, const string_view& sv)
{
    string ret;
    ret.reserve(s.size() + sv.size());
    ret.append(s);
    ret.append(sv);
    return ret;
}
inline string
operator+(const string_view& sv, const string& s)
{
    string ret;
    ret.reserve(s.size() + sv.size());
    ret.append(sv);
    ret.append(s);
    return ret;
}
using svmatch = match_results<string_view::const_iterator>;
using svsub_match = sub_match<string_view::const_iterator>;
inline bool
regex_match(string_view sv,
            svmatch& m,
            const regex& e,
            regex_constants::match_flag_type flags = regex_constants::match_default)
{
    return regex_match(sv.begin(), sv.end(), m, e, flags);
}
inline bool
regex_match(string_view sv,
            const regex& e,
            regex_constants::match_flag_type flags = regex_constants::match_default)
{
    return regex_match(sv.begin(), sv.end(), e, flags);
}
inline bool
regex_search(string_view sv,
            svmatch& m,
            const regex& e,
            regex_constants::match_flag_type flags = regex_constants::match_default)
{
    return regex_search(sv.begin(), sv.end(), m, e, flags);
}
} // namespace std

namespace jami {

constexpr static const char TRUE_STR[] = "true";
constexpr static const char FALSE_STR[] = "false";

constexpr static const char*
bool_to_str(bool b) noexcept
{
    return b ? TRUE_STR : FALSE_STR;
}

std::string to_string(double value);

#ifdef _WIN32
std::wstring to_wstring(const std::string& str, int codePage = CP_UTF8);
std::string to_string(const std::wstring& wstr, int codePage = CP_UTF8);
#endif

std::string to_hex_string(uint64_t id);
uint64_t from_hex_string(const std::string& str);

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

std::string_view trim(std::string_view s);

/**
 * Split a string_view with an API similar to std::getline.
 * @param str The input string to iterate on.
 * @param line The output substring, also used as an iterator.
 *             It must be default-initialised when this function is used 
 *             for the first time with a given string,
 *             and should not be modified by the caller during iteration.
 * @param delim The delimiter.
 * @return True if line was set, false if the end of the input was reached.
 */
inline
bool getline(const std::string_view str, std::string_view& line, char delim = '\n')
{
    if (str.empty())
        return false;
    if (line.data() == nullptr) {
        // first iteration
        line = str.substr(0, str.find(delim));
    } else {
        size_t prevEnd = line.data() + line.size() - str.data();
        if (prevEnd >= str.size())
            return false;
        auto nextStr = str.substr(prevEnd + 1);
        line = nextStr.substr(0, nextStr.find(delim));
    }
    return  true;
}

inline
std::vector<std::string_view> split_string(std::string_view str, char delim)
{
    std::vector<std::string_view> output;
    for (auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + 1) {
        second = std::find(first, last, delim);
        if (first != second)
            output.emplace_back(first, second - first);
    }
    return output;
}

inline
std::vector<std::string_view> split_string(std::string_view str, std::string_view delims = " ")
{
    std::vector<std::string_view> output;
    for (auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last; first = second + 1) {
        second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));
        if (first != second)
            output.emplace_back(first, second - first);
    }
    return output;
}

std::vector<unsigned> split_string_to_unsigned(const std::string& s, char sep);

void string_replace(std::string& str, const std::string& from, const std::string& to);

std::string_view string_remove_suffix(std::string_view str, char separator);

std::string string_join(std::set<std::string> set, std::string_view separator = "/");

std::set<std::string> string_split_set(std::string& str, std::string_view separator = "/");

} // namespace jami
