/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include <string_view>
#include <vector>
#include <exception>

#include <cstdint>

namespace jami {
namespace base64 {

class base64_exception : public std::exception
{};

std::string encode(std::string_view);

inline std::string encode(const std::vector<uint8_t>& data) {
    return encode(std::string_view((const char*)data.data(), data.size()));
}

std::vector<uint8_t> decode(std::string_view);

} // namespace base64
} // namespace jami
