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

#pragma once

#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <regex>
#include <iterator>
#include <charconv>

#ifdef _WIN32
#include <WTypes.h>
#endif

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

template<typename T>
T
to_int(std::string_view str, T defaultValue)
{
    T result;
    auto [p, ec] = std::from_chars(str.data(), str.data()+str.size(), result);
    if (ec == std::errc())
        return result;
    else
        return defaultValue;
}

template<typename T>
T
to_int(std::string_view str)
{
    T result;
    auto [p, ec] = std::from_chars(str.data(), str.data()+str.size(), result);
    if (ec == std::errc())
        return result;
    if (ec == std::errc::invalid_argument)
        throw std::invalid_argument("Can't parse integer: invalid_argument");
    else if (ec == std::errc::result_out_of_range)
        throw std::out_of_range("Can't parse integer: out of range");
    throw std::system_error(std::make_error_code(ec));
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

template<typename... Args>
std::string concat(Args &&... args){
    static_assert((std::is_constructible_v<std::string_view, Args&&> && ...));
    std::string s;
    s.reserve((std::string_view{ args }.size() + ...));
    (s.append(std::forward<Args>(args)), ...);
    return s;
}

std::string_view trim(std::string_view s);

/**
 * Split a string_view with an API similar to std::getline.
 * @param str The input string stream to iterate on, trimed of line during iteration.
 * @param line The output substring.
 * @param delim The delimiter.
 * @return True if line was set, false if the end of the input was reached.
 */
inline bool
getline_full(std::string_view& str, std::string_view& line, char delim = '\n')
{
    if (str.empty())
        return false;
    auto pos = str.find(delim);
    line = str.substr(0, pos);
    str.remove_prefix(pos < str.size() ? pos + 1 : str.size());
    return true;
}

/**
 * Similar to @getline_full but skips empty results.
 */
inline bool
getline(std::string_view& str, std::string_view& line, char delim = '\n')
{
    do {
        if (!getline_full(str, line, delim))
            return false;
    } while (line.empty());
    return true;
}

inline std::vector<std::string_view>
split_string(std::string_view str, char delim)
{
    std::vector<std::string_view> output;
    for (auto first = str.data(), second = str.data(), last = first + str.size();
         second != last && first != last;
         first = second + 1) {
        second = std::find(first, last, delim);
        if (first != second)
            output.emplace_back(first, second - first);
    }
    return output;
}

inline std::vector<std::string_view>
split_string(std::string_view str, std::string_view delims = " ")
{
    std::vector<std::string_view> output;
    for (auto first = str.data(), second = str.data(), last = first + str.size();
         second != last && first != last;
         first = second + 1) {
        second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));
        if (first != second)
            output.emplace_back(first, second - first);
    }
    return output;
}

std::vector<unsigned> split_string_to_unsigned(std::string_view s, char sep);

void string_replace(std::string& str, const std::string& from, const std::string& to);

std::string_view string_remove_suffix(std::string_view str, char separator);

std::string string_join(const std::set<std::string>& set, std::string_view separator = "/");

std::set<std::string> string_split_set(std::string& str, std::string_view separator = "/");

} // namespace jami

// Add string operators crucially missing from standard
// see https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/1RcShRhrmRc
namespace std {
inline string
operator+(const string& s, const string_view& sv)
{
    return jami::concat(s, sv);
}
inline string
operator+(const string_view& sv, const string& s)
{
    return jami::concat(sv, s);
}
using svmatch = match_results<string_view::const_iterator>;
using svsub_match = sub_match<string_view::const_iterator>;
constexpr string_view svsub_match_view(const svsub_match& submatch) noexcept {
    return string_view(&*submatch.first, submatch.second - submatch.first);
}
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
