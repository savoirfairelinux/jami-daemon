/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <memory> // for auto_ptr
#include <string>
// #include <commoncpp/common.h>
#include "fileutils.h"

#include "dbus/dbusmanager.h"
#include "manager.h"
/*
ost::CommandOptionNoArg	console(
    "console", "c", "Log in console (instead of syslog)"
);

ost::CommandOptionNoArg	debug(
    "debug", "d", "Debug mode (more verbose)"
);

ost::CommandOptionNoArg	help(
    "help", "h", "Print help"
);
*/
int main(int /*argc*/, char **argv)
{
    fileutils::set_program_dir(argv[0]);
    // makeCommandOptionParse allocates the object with operator new, so
    // auto_ptr is fine in this context.
    // TODO: This should eventually be replaced with std::unique_ptr for C++0x
    // std::auto_ptr<ost::CommandOptionParse> args(ost::makeCommandOptionParse(argc, argv, ""));

    printf("SFLphone Daemon " VERSION ", by Savoir-Faire Linux 2004-2012\n" \
           "http://www.sflphone.org/\n");
/*
    if (help.numSet) {
        std::cerr << args->printUsage();
        return 0;
    } else if (args->argsHaveError()) {
        std::cerr << args->printErrors();
        std::cerr << args->printUsage();
        return 1;
    }
*/
    // Logger::setConsoleLog(console.numSet);
    // Logger::setDebugMode(debug.numSet);
    Logger::setConsoleLog(1);
    Logger::setDebugMode(1);

    if (!fileutils::create_pidfile())
        return 1;

    try {
        Manager::instance().init("");
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An exception occured when initializing the system." << std::endl;
        return 1;
    }

    DEBUG("Starting DBus event loop");
    Manager::instance().getDbusManager()->exec();
    Manager::instance().saveHistory();

    return 0;
}
