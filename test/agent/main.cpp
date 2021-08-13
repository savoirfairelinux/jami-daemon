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

void*
main_inner(void* agent_config_raw) /* In Guile context */
{
    install_scheme_primitives();

    Agent::instance().init();

    atexit(fini);

    char *argv[] = {
        (char*)"guile",
        (char*)"-l",
        (char*)agent_config_raw,
        (char*)"--",
        NULL,
    };

    scm_shell(4, argv);

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
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG));

    AGENT_ASSERT(DRing::start(""), "Failed to start daemon");

    /* Entering guile context */
    scm_with_guile(main_inner, argv[1]);
}
