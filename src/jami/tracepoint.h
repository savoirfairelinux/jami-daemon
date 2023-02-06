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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "tracepoint-def.h"
#pragma GCC diagnostic pop

#ifdef ENABLE_TRACEPOINTS

#  ifndef lttng_ust_tracepoint
#    define lttng_ust_tracepoint(...) tracepoint(__VA_ARGS__)
#  endif

#  ifndef lttng_ust_do_tracepoint
#    define lttng_ust_do_tracepoint(...) do_tracepoint(__VA_ARGS__)
#  endif

#  ifndef lttng_ust_tracepoint_enabled
#    define lttng_ust_tracepoint_enabled(...) tracepoint_enabled(__VA_ARGS__)
#  endif

#  define jami_tracepoint(tp_name, ...)                                 \
        lttng_ust_tracepoint(jami, tp_name __VA_OPT__(,) __VA_ARGS__)

#  define jami_tracepoint_if_enabled(tp_name, ...)                      \
        do {                                                            \
                if (lttng_ust_tracepoint_enabled(jami, tp_name)) {      \
                        lttng_ust_do_tracepoint(jami,                   \
                                                tp_name                 \
                                                __VA_OPT__(,)           \
                                                __VA_ARGS__);           \
                }                                                       \
        }                                                               \
        while (0)

#else

#  define jami_tracepoint(...)            static_assert(true)
#  define jami_tracepoint_if_enabled(...) static_assert(true)

#endif
