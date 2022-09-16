/*
 *  Copyright (C) 2021-2022 Savoir-faire Linux Inc.
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

static bool streq(const char* A, const char* B)
{
    return 0 == strcmp(A, B);
}

void*
main_in_guile(void* args_raw)
{
    struct args* args = static_cast<struct args*>(args_raw);

    install_scheme_primitives();

    scm_shell(args->argc, args->argv);

    /* unreachable */
    return nullptr;
}

void*
compile_in_guile(void* args_raw)
{
    struct args* args = static_cast<struct args*>(args_raw);
    char buf[4096];

    if (args->argc < 4) {
        fprintf(stderr, "Usage: agent.exe compile FILE OUT\n");
        exit(EXIT_FAILURE);
    }

    install_scheme_primitives();

    snprintf(buf, sizeof(buf),
             "(use-modules (system base compile)) (compile-file \"%s\" #:output-file \"%s\")",
             args->argv[2], args->argv[3]);

    scm_c_eval_string(buf);
    scm_gc();

    return nullptr;
}

int
main(int argc, char* argv[])
{
    struct args args = { argc, argv };

    if (argc > 1 && streq(argv[1], "compile")) {
        scm_with_guile(compile_in_guile, (void*)&args);
    } else {

        /* NOTE!  It's very important to initialize the daemon before entering Guile!!! */
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG));

        if (not DRing::start("")) {
            scm_misc_error("Dring::start", NULL, 0);
        }

        /* Entering guile context - This never returns */
        scm_with_guile(main_in_guile, (void*)&args);
    }

    exit(EXIT_SUCCESS);
}
