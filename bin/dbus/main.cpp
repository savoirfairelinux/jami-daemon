/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dbuscallmanager.hpp"
#include "dbusconfigurationmanager.hpp"
#include "dbusinstance.hpp"
#include "dbuspresencemanager.hpp"
#ifdef ENABLE_VIDEO
#include "dbusvideomanager.hpp"
#endif
#ifdef ENABLE_PLUGIN
#include "dbuspluginmanagerinterface.hpp"
#endif

#include <sdbus-c++/sdbus-c++.h>
#include <jami.h>
#include <string.h>

#include <csignal>
#include <getopt.h>
#include <iostream>
#include <memory>

bool persistent = false;
std::unique_ptr<sdbus::IConnection> connection;

static int ringFlags = 0;

static void
print_title()
{
    std::cout
        << "Jami Daemon " << libjami::version()
        << ", by Savoir-faire Linux Inc. 2004-2023" << std::endl
        << "https://jami.net/" << std::endl
#ifdef ENABLE_VIDEO
        << "[Video support enabled]" << std::endl
#endif
#ifdef ENABLE_PLUGIN
        << "[Plugins support enabled]" << std::endl
#endif
        << std::endl;
}

static void
print_usage()
{
    std::cout << std::endl <<
    "-c, --console \t- Log in console (instead of syslog)" << std::endl <<
    "-d, --debug \t- Debug mode (more verbose)" << std::endl <<
    "-p, --persistent \t- Stay alive after client quits" << std::endl <<
    "--auto-answer \t- Force automatic answer to incoming calls" << std::endl <<
    "-h, --help \t- Print help" << std::endl;
}

// Parse command line arguments, setting debug options or printing a help
// message accordingly.
// returns true if we should quit (i.e. help was printed), false otherwise
static bool
parse_args(int argc, char *argv[])
{
    int consoleFlag = false;
    int debugFlag = false;
    int helpFlag = false;
    int versionFlag = false;
    int autoAnswer = false;

    const struct option long_options[] = {
        /* These options set a flag. */
        {"debug",       no_argument,        nullptr,    'd'},
        {"console",     no_argument,        nullptr,    'c'},
        {"persistent",  no_argument,        nullptr,    'p'},
        {"help",        no_argument,        nullptr,    'h'},
        {"version",     no_argument,        nullptr,    'v'},
        {"auto-answer", no_argument,        &autoAnswer, true},
        {nullptr,       0,                  nullptr,     0} /* Sentinel */
    };

    while (true) {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        auto c = getopt_long(argc, argv, "dcphv:", long_options, &option_index);

        // end of the options
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                debugFlag = true;
                break;

            case 'c':
                consoleFlag = true;
                break;

            case 'p':
                persistent = true;
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

    if (helpFlag) {
        print_usage();
        return true;
    }

    if (versionFlag) {
        // We've always print the title/version, so we can just exit
        return true;
    }

    if (consoleFlag)
        ringFlags |= libjami::LIBJAMI_FLAG_CONSOLE_LOG;

    if (debugFlag)
        ringFlags |= libjami::LIBJAMI_FLAG_DEBUG;

    if (autoAnswer)
        ringFlags |= libjami::LIBJAMI_FLAG_AUTOANSWER;

    return false;
}

static void
signal_handler(int code)
{
    // Unset signal handlers
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    std::cerr << "Caught signal " << strsignal(code)
              << ", terminating..." << std::endl;

    connection->leaveEventLoop();
}

int
main(int argc, char *argv [])
{
    print_title();

    if (parse_args(argc, argv))
        return 0;

    if (!libjami::init(static_cast<libjami::InitFlag>(ringFlags))) {
        std::cerr << "libjami::init() failed" << std::endl;
        return 1;
    }

    try {
        connection = sdbus::createSessionBusConnection("cx.ring.Ring");
        DBusCallManager callManager(*connection);
        DBusConfigurationManager configurationManager(*connection);
        DBusInstance instanceManager(*connection);
        DBusPresenceManager presenceManager(*connection);
#ifdef ENABLE_VIDEO
        DBusVideoManager videoManager(*connection);
#endif
#ifdef ENABLE_PLUGIN
        DBusPluginManagerInterface pluginManager(*connection);
#endif

        if (!libjami::start()) {
            std::cerr << "libjami::start() failed" << std::endl;
            libjami::unregisterSignalHandlers();
            libjami::fini();
            return 2;
        }

        // TODO: Block signals for all threads but the main thread, decide how/if we should
        // handle other signals
        std::signal(SIGINT, signal_handler);
        std::signal(SIGHUP, signal_handler);
        std::signal(SIGTERM, signal_handler);
        std::signal(SIGPIPE, SIG_IGN);

        connection->enterEventLoop();

        libjami::unregisterSignalHandlers();

    } catch (const sdbus::Error& ex) {
        std::cerr << "sdbus exception: " << ex.what() << std::endl;
    }

    libjami::fini();
    return 0;
}
