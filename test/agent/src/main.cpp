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

#include "bindings/bindings.h"

#include "jami.h"

#include <libguile.h>

extern "C" {
LIBJAMI_PUBLIC void init();
LIBJAMI_PUBLIC void fini();
}

void
init()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG));

    if (not libjami::start("")) {
        scm_misc_error("Dring::start", NULL, 0);
    }

    install_scheme_primitives();
}

void
fini()
{
    libjami::fini();
}
