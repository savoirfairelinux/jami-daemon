/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

/* agent */
#include "agent/agent.h"
#include "agent/bindings.h"

/* Jami */
#include "jami.h"

/* Third parties */
#include <gnutls/gnutls.h>
#include <libguile.h>

/* std */
#include <fstream>

/* Guile helpers */

static SCM
main_body(void* path_raw)
{
    const char* path = (const char*)path_raw;

    if (0 == strcmp("-", path)) {
        scm_c_primitive_load("/dev/stdin");
    } else {
        scm_c_primitive_load(path);
    }

    return SCM_UNDEFINED;
}

static SCM
main_catch(void* nil, SCM key_sym, SCM rest_lst)
{
    (void) nil;

    SCM fmt_str = scm_from_utf8_string("Guile exception `~a`: ~a");
    SCM args_lst = scm_list_2(key_sym, rest_lst);
    SCM to_print_str = scm_simple_format(SCM_BOOL_F, fmt_str, args_lst);

    char* to_print_raw = scm_to_locale_string(to_print_str);

    AGENT_ERR("%s\n", to_print_raw);

    free(to_print_raw);

    return SCM_UNDEFINED;
}

void*
main_inner(void* agent_config_raw) /* In Guile context */
{
    install_scheme_primitives();

    Agent::instance().init();

    scm_internal_catch(SCM_BOOL_T, main_body, agent_config_raw, main_catch, nullptr);

    Agent::instance().fini();

    return nullptr;
}

int
main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: agent CONFIG\n");
        exit(EXIT_FAILURE);
    }

    setenv("GUILE_LOAD_PATH", ".", 1);

    /* NOTE!  It's very important to initialize the daemon before entering Guile!!! */
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));

    AGENT_ASSERT(DRing::start(""), "Failed to start daemon");

    /* Entering guile context */
    scm_with_guile(main_inner, argv[1]);

    DRing::fini();
}
