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
#pragma once

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

// C-callable versions of C++ APIs.
#ifdef __cplusplus
namespace {
extern "C" {
#endif

void jami_secure_memzero(void* ptr, size_t length);

#ifdef __cplusplus
};
}

namespace jami {
namespace secure {

/// Erase with \a size '0' the given memory starting at \a ptr pointer.
void memzero(void* ptr, std::size_t length);

} // namespace secure
} // namespace jami

#endif // __cplusplus
