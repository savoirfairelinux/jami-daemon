/*
 *  Copyright (C) 2004-2017 Savoir-Faire Linux Inc.
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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
 // Get the current path
 #pragma once

 #include <stdio.h>
 #ifdef WINDOWS
     #include <direct.h>
     #define GetCurrentDir _getcwd
 #else
     #include <unistd.h>
     #define GetCurrentDir getcwd
  #endif

 std::string pathTest()
 {
     // retrieve the current path
     char cCurrentPath[FILENAME_MAX];
     GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
     cCurrentPath[sizeof(cCurrentPath) - 1] = '\0';
     return std::string(cCurrentPath);
 }
