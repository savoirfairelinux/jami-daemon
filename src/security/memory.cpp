/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "memory.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include <algorithm>

namespace jami { namespace secure {

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

}}

extern "C" void
ring_secure_memzero(void* ptr, size_t length)
{
    jami::secure::memzero(ptr, length);
}
