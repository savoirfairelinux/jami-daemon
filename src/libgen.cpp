/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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
 
#include "libgen.h"

char *basename(char * path)
{
    int i;

    if (path == NULL || path[0] == '\0')
        return "";
    for (i = strlen(path) - 1; i >= 0 && path[i] == '/'; i--);
    if (i == -1)
        return "/";
    for (path[i + 1] = '\0'; i >= 0 && path[i] != '/'; i--);
    return &path[i + 1];
}

char *dirname(char * path)
{
    int i;

    if (path == NULL || path[0] == '\0')
        return ".";
    for (i = strlen(path) - 1; i >= 0 && path[i] == '/'; i--);
    if (i == -1)
        return "/";
    for (i--; i >= 0 && path[i] != '/'; i--);
    if (i == -1)
        return ".";
    path[i] = '\0';
    for (i--; i >= 0 && path[i] == '/'; i--);
    if (i == -1)
        return "/";
    path[i + 1] = '\0';
    return path;
}