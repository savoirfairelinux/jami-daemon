/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

/* Jami */
#include "logger.h"

/* Agent */
#include "utils.h"

static SCM log_binding(SCM log_lvl_int, SCM file_str, SCM line_int, SCM text_str)
{
        const std::string file = from_guile(file_str);
        const std::string text = from_guile(text_str);

        jami::Logger::log(from_guile(log_lvl_int),
                          file.c_str(),
                          from_guile(line_int),
                          false, "[GUILE] %s\n", text.c_str());

        return SCM_UNDEFINED;
}

static void
install_logger_primitives(void *)
{
    define_primitive("log", 4, 0, 0, (void*) log_binding);

    DEFINE_INT(LOG_DEBUG);
    DEFINE_INT(LOG_INFO);
    DEFINE_INT(LOG_WARNING);
    DEFINE_INT(LOG_ERR);
}
