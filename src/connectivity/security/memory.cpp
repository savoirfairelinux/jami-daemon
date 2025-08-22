/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "memory.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include <algorithm>

namespace jami {
namespace secure {

void
memzero(void* ptr, std::size_t length)
{
#ifdef _WIN32
    SecureZeroMemory(ptr, length);
#else
    volatile auto* p = static_cast<unsigned char*>(ptr);
    std::fill_n(p, length, 0);
#endif
}

} // namespace secure
} // namespace jami

extern "C" void
jami_secure_memzero(void* ptr, size_t length)
{
    jami::secure::memzero(ptr, length);
}
