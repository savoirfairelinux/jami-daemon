/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <cstdlib>
#include <signal.h>
#include <string>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include "fileutils.h"

namespace fileutils {
// returns true if directory exists
bool check_dir(const char *path)
{
    DIR *dir = opendir(path);

    if (!dir) { // doesn't exist
        if (mkdir(path, 0755) != 0) {   // couldn't create the dir
            perror(path);
            return false;
        }
    } else
        closedir(dir);

    return true;
}

static char *program_dir = NULL;

void set_program_dir(char *program_path)
{
    program_dir = dirname(program_path);
}

const char *get_program_dir()
{
    return program_dir;
}

//TODO it is faking this, implement proper system 
const char *get_data_dir()
{
    std::string path = std::string(get_program_dir()) + "/../../share/sflphone/ringtones/";
    return path.c_str();
}

bool create_pidfile()
{
    std::string xdg_env(XDG_CACHE_HOME);
    std::string path = (not xdg_env.empty()) ? xdg_env : std::string(HOMEDIR) + DIR_SEPARATOR_STR ".cache/";

    if (!check_dir(path.c_str()))
        return false;

    path += "sflphone";

    if (!check_dir(path.c_str()))
        return false;

    std::string pidfile = path + "/" PIDFILE;
    std::ifstream is(pidfile.c_str());

    if (is) {
        // PID file exists. Check if the former process is still alive or
        // not. If alive, give user a hint.
        int oldPid;
        is >> oldPid;

        if (kill(oldPid, 0) == 0) {
            // Use cerr because logging has not been initialized
            std::cerr << "There is already a sflphoned daemon running in " <<
                "the system. Starting Failed." << std::endl;
            return false;
        }
    }

    // write pid file
    std::ofstream os(pidfile.c_str());

    if (!os) {
        perror(pidfile.c_str());
        return false;
    } else {
        os << getpid();
    }

    return true;
}

bool isDirectoryWritable(const std::string &directory)
{
    return access(directory.c_str(), W_OK) == 0;
}
}
