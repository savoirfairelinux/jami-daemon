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
#include <getopt.h>
#include "fileutils.h"
#include "dbus/dbusmanager.h"
#include "logger.h"
#include "manager.h"

namespace {
    void print_title()
    {
        std::cout << "SFLphone Daemon " << VERSION <<
            ", by Savoir-Faire Linux 2004-2012" << std::endl <<
            "http://www.sflphone.org/" << std::endl;
    }

    void print_usage()
    {
        std::cout << std::endl <<
        "-c, --console \t- Log in console (instead of syslog)" << std::endl <<
        "-d, --debug \t- Debug mode (more verbose)" << std::endl <<
        "-h, --help \t- Print help" << std::endl;
    }

    // Parse command line arguments, setting debug options or printing a help
    // message accordingly.
    // returns true if we should quit (i.e. help was printed), false otherwise
    bool parse_args(int argc, char *argv[])
    {
        int consoleFlag = false;
        int debugFlag = false;
        int helpFlag = false;
        int versionFlag = false;
        static const struct option long_options[] = {
            /* These options set a flag. */
            {"debug", no_argument, NULL, 'd'},
            {"console", no_argument, NULL, 'c'},
            {"help", no_argument, NULL, 'h'},
            {"version", no_argument, NULL, 'v'},
            {0, 0, 0, 0} /* Sentinel */
        };

        while (true) {
            /* getopt_long stores the option index here. */
            int option_index = 0;
            int c = getopt_long(argc, argv, "dchv", long_options, &option_index);

            /* Detect the end of the options. */
            if (c == -1)
                break;

            switch (c) {
                case 'd':
                    debugFlag = true;
                    break;

                case 'c':
                    consoleFlag = true;
                    break;

                case 'h':
                case '?':
                    helpFlag = true;
                    break;

                case 'v':
                    versionFlag = true;
                    break;

                default:
                    break;
            }
        }

        bool quit = false;
        if (helpFlag) {
            print_usage();
            quit = true;
        } else if (versionFlag) {
            // We've always print the title/version, so we can just exit
            quit = true;
        } else {
            Logger::setConsoleLog(consoleFlag);
            Logger::setDebugMode(debugFlag);
        }
        return quit;
    }
}

int main(int argc, char *argv [])
{
    fileutils::set_program_dir(argv[0]);
    print_title();
    if (parse_args(argc, argv))
        return 0;

    if (!fileutils::create_pidfile())
        return 1;

    try {
        Manager::instance().init("");
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An exception occured when initializing " PACKAGE <<
            std::endl;
        return 1;
    }

    DEBUG("Starting DBus event loop");
    Manager::instance().getDbusManager()->exec();
    Manager::instance().saveHistory();

    return 0;
}
