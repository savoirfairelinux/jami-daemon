/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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

#ifdef ENABLE_TRACEPOINTS
/*
 * GCC Only.  We use these instead of classic __FILE__ and __LINE__ because
 * these are evaluated where invoked and not at expansion time.  See GCC manual.
 */
#  define CURRENT_FILENAME() __builtin_FILE()
#  define CURRENT_LINE()     __builtin_LINE()
#else
#  define CURRENT_FILENAME() ""
#  define CURRENT_LINE()     0
#endif

#ifdef HAVE_CXXABI_H
#include <cxxabi.h>

template<typename T>
std::string demangle()
{
    int err;
    char *raw;
    std::string ret;

    raw = abi::__cxa_demangle(typeid(T).name(), 0, 0, &err);

    if (0 == err) {
        ret = raw;
    } else {
        ret = typeid(T).name();
    }

    std::free(raw);

    return ret;
}

#else
template<typename T>
std::string demangle()
{
    return typeid(T).name();
}
#endif
