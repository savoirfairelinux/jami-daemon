/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <libintl.h>
#include <cstring>
#include <iostream>
#include <memory> // for auto_ptr
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <cc++/common.h>
#include "global.h"
#include "fileutils.h"

#include "dbus/dbusmanager.h"
#include "manager.h"

#include "audio/audiolayer.h"

ost::CommandOptionNoArg	console(
    "console", "c", "Log in console (instead of syslog)"
);

ost::CommandOptionNoArg	debug(
    "debug", "d", "Debug mode (more verbose)"
);

ost::CommandOptionNoArg	help(
    "help", "h", "Print help"
);

// returns true if directory exists
static bool check_dir(const char *path)
{
    DIR *dir = opendir(path);

    if (!dir) {	// doesn't exist
        if (mkdir(path, 0755) != 0) {   // couldn't create the dir
            perror(path);
            return false;
        }
    } else {
        closedir(dir);
    }

    return true;
}

int
main(int argc, char **argv)
{
    set_program_dir(argv[0]);
    // makeCommandOptionParse allocates the object with operator new, so
    // auto_ptr is fine in this context.
    // TODO: This should eventually be replaced with std::unique_ptr for C++0x
    std::auto_ptr<ost::CommandOptionParse> args(ost::makeCommandOptionParse(argc, argv, ""));

    printf("SFLphone Daemon "VERSION", by Savoir-Faire Linux 2004-2011\n" \
           "http://www.sflphone.org/\n");

    if (help.numSet) {
        std::cerr << args->printUsage();
        return 0;
    } else if (args->argsHaveError()) {
        std::cerr << args->printErrors();
        std::cerr << args->printUsage();
        return 1;
    }

    Logger::setConsoleLog(console.numSet);
    Logger::setDebugMode(debug.numSet);

    const char *xdg_env = XDG_CACHE_HOME;
    std::string path = xdg_env ? xdg_env : std::string(HOMEDIR) + DIR_SEPARATOR_STR ".cache/";

    if (!check_dir(path.c_str()))
        return 1;

    path = path + "sflphone";

    if (!check_dir(path.c_str()))
        return 1;

    std::string pidfile = path + "/" PIDFILE;
    FILE *fp = fopen(pidfile.c_str(),"r");

    if (fp) { // PID file exists. Check the former process still alive or not. If alive, give user a hint.
        int oldPid;

        if (fscanf(fp, "%d", &oldPid) != 1) {
            std::cerr << "Couldn't read pidfile " << pidfile << std::endl;
            return 1;
        }

        fclose(fp);

        if (kill(oldPid, 0) == 0) {
            std::cerr << "There is already a sflphoned daemon running in the system. Starting Failed." << std::endl;
            return 1;
        }
    }

    // write pid file
    fp = fopen(pidfile.c_str(),"w");

    if (!fp) {
        perror(pidfile.c_str());
        return 1;
    } else {
        std::ostringstream pidstr;
        pidstr << getpid();

        fputs(pidstr.str().c_str() , fp);
        fclose(fp);
    }

    try {
        Manager::instance().init();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An exception occured when initializing the system." << std::endl;
        return 1;
    }

    _debug("Starting DBus event loop");
    Manager::instance().getDbusManager()->exec();

    return 0;
}
