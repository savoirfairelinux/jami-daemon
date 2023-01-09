/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Common.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * Very common stuff (i.e. that every other header needs except vector_ref.h).
 */

#pragma once

// way too many unsigned to size_t warnings in 32 bit build
#ifdef _M_IX86
#pragma warning(disable : 4244)
#endif

#if _MSC_VER && _MSC_VER < 1900
#define _ALLOW_KEYWORD_MACROS
#define noexcept throw()
#endif

#ifdef __INTEL_COMPILER
#pragma warning(disable : 3682) // call through incomplete class
#endif

#include <map>
#include <unordered_map>
#include <vector>
#include <set>
#include <unordered_set>
#include <functional>
#include <string>
#include <chrono>
#include "vector_ref.h"

// Quote a given token stream to turn it into a string.
#define DEV_QUOTED_HELPER(s) #s
#define DEV_QUOTED(s)        DEV_QUOTED_HELPER(s)

#define DEV_IGNORE_EXCEPTIONS(X) \
    try { \
        X; \
    } catch (...) { \
    }

#define DEV_IF_THROWS(X) \
    try { \
        X; \
    } catch (...)

namespace dev {

extern char const* Version;

extern std::string const EmptyString;

// Binary data types.
using bytes = std::vector<uint8_t>;
using bytesRef = vector_ref<uint8_t>;
using bytesConstRef = vector_ref<uint8_t const>;

// Map types.
using StringMap = std::map<std::string, std::string>;
using BytesMap = std::map<bytes, bytes>;
using HexMap = std::map<bytes, bytes>;

// Hash types.
using StringHashMap = std::unordered_map<std::string, std::string>;

// String types.
using strings = std::vector<std::string>;

// Fixed-length string types.
using string32 = std::array<char, 32>;

// Null/Invalid values for convenience.
extern bytes const NullBytes;

/// @returns the absolute distance between _a and _b.
template<class N>
inline N
diff(N const& _a, N const& _b)
{
    return std::max(_a, _b) - std::min(_a, _b);
}

/// RAII utility class whose destructor calls a given function.
class ScopeGuard
{
public:
    ScopeGuard(std::function<void(void)> _f)
        : m_f(_f)
    {}
    ~ScopeGuard() { m_f(); }

private:
    std::function<void(void)> m_f;
};

#ifdef _MSC_VER
// TODO.
#define DEV_UNUSED
#else
#define DEV_UNUSED __attribute__((unused))
#endif

enum class WithExisting : int { Trust = 0, Verify, Rescue, Kill };

/// Get the current time in seconds since the epoch in UTC
uint64_t utcTime();

} // namespace dev
