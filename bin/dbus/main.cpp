/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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

#include "dbusclient.h"
#include "jami.h"

#include <signal.h>
#include <getopt.h>

#include <iostream>
#include <thread>
#include <memory>
#include <cstring>
#include <cstdlib>

static int ringFlags = 0;
static std::weak_ptr<DBusClient> weakClient;

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
parse_args(int argc, char *argv[], bool& persistent)
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
    std::cerr << "Caught signal " << strsignal(code)
              << ", terminating..." << std::endl;

    // Unset signal handlers
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    // Interrupt the process
    if (auto client = weakClient.lock())
        client->exit();
}

int
main(int argc, char *argv [])
{
    print_title();

    bool persistent = false;
    if (parse_args(argc, argv, persistent))
        return 0;

    // TODO: Block signals for all threads but the main thread, decide how/if we should
    // handle other signals
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    try {
        if (auto client = std::make_shared<DBusClient>(ringFlags, persistent))
        {
            weakClient = client;
            return client->event_loop();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception in the DBusClient: " << ex.what() << std::endl;
    }
    return 1;
}
