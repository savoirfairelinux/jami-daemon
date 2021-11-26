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

/* Agent */
#include "bindings/bindings.h"
#include "utils.h"

/* Jami */
#include "jami.h"

/* Third parties */
#include <libguile.h>

struct args {
    int argc;
    char** argv;
};

void*
main_in_guile(void* args_raw)
{
    struct args* args = static_cast<struct args*>(args_raw);

    install_scheme_primitives();

    atexit(DRing::fini);

    scm_shell(args->argc, args->argv);

    /* unreachable */
    return nullptr;
}

#include <sys/resource.h>

int
main(int argc, char* argv[])
{
    struct rlimit rlim;

    getrlimit(RLIMIT_NOFILE, &rlim);
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rlim);

    printf("RLIMIT_NOFILE: %lu\n", rlim.rlim_max);

    setenv("GUILE_LOAD_PATH", ".", 1);

    /* NOTE!  It's very important to initialize the daemon before entering Guile!!! */
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG));

    AGENT_ASSERT(DRing::start(""), "Failed to start daemon");

    struct args args = { argc, argv };

    /* Entering guile context - This never returns */
    scm_with_guile(main_in_guile, (void*)&args);
}
