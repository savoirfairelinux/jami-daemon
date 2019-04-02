/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include <stdint.h>
#include <stddef.h>

#include <string>
#include <vector>
#include <exception>

namespace jami {
namespace base64 {

class base64_exception : public std::exception { };

std::string encode(const std::vector<uint8_t>::const_iterator begin, const std::vector<uint8_t>::const_iterator end);
std::string encode(const std::vector<uint8_t>& dat);
std::vector<uint8_t> decode(const std::string& str);

}} // namespace jami::base64
