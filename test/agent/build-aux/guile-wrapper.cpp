/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
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

#include <libguile.h>

void inner_main(void *, int argc, char **argv)
{
    scm_shell(argc, argv);
}

int main(int argc, char *argv[])
{
    (void) scm_boot_guile(argc, argv, inner_main, NULL);
    __builtin_unreachable();
}
