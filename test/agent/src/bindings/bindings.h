/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

/* Guile */
#include <libguile.h>

/* Agent */
#include "utils.h"

#define DEFINE_AND_EXPORT(NAME, VALUE, TYPE)     \
        do {                                     \
                scm_c_define(NAME, TYPE(VALUE)); \
                scm_c_export(NAME, NULL);        \
        } while (0)

#define DEFINE_INT(NAME)    DEFINE_AND_EXPORT(#NAME, NAME, scm_from_int)
#define DEFINE_UINT(NAME)   DEFINE_AND_EXPORT(#NAME, NAME, scm_from_uint)
#define DEFINE_UINT32(NAME) DEFINE_AND_EXPORT(#NAME, NAME, scm_from_uint32)

#define LOG_BINDING() JAMI_INFO("[GUILE] In binding %s()", __func__)

extern void define_primitive(const char* name, int req, int opt, int rst, void* func);
extern void install_scheme_primitives();
