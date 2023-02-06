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

#include "jami/jami.h"

#include "utils.h"

static SCM init_binding(SCM flags)
{
        LOG_BINDING();

        unsigned int flags_cast = from_guile(flags);

        return to_guile(libjami::init(static_cast<libjami::InitFlag>(flags_cast)));
}

static SCM fini_binding()
{
        LOG_BINDING();

        libjami::fini();

        return SCM_UNDEFINED;
}

static SCM initialized_binding()
{
        LOG_BINDING();

        return to_guile(libjami::initialized());
}

static SCM logging_binding(SCM whom, SCM action)
{
        LOG_BINDING();

        libjami::logging(from_guile(whom), from_guile(action));

        return SCM_UNDEFINED;
}

static SCM platform_binding()
{
        LOG_BINDING();

        return to_guile(libjami::platform());
}

static SCM start_binding(SCM config_file)
{
        LOG_BINDING();

        return to_guile(libjami::start(from_guile(config_file)));
}

static SCM version_binding()
{
        LOG_BINDING();

        return to_guile(libjami::version());
}

static void
install_jami_primitives(void *)
{
    define_primitive("init", 1, 0, 0, (void*) init_binding);
    define_primitive("initialized", 0, 0, 0, (void*) initialized_binding);
    define_primitive("fini", 0, 0, 0, (void*) fini_binding);
    define_primitive("logging", 2, 0, 0, (void*) logging_binding);
    define_primitive("platform", 0, 0, 0, (void*) platform_binding);
    define_primitive("start", 1, 0, 0, (void*) start_binding);
    define_primitive("version", 0, 0, 0, (void*) version_binding);
}
