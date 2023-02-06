/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
#include <iostream>
#include <thread>
#include <cstring>
#include <signal.h>
#include <getopt.h>
#include <string>
#include <chrono>

#include "jami.h"
#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"
#ifdef ENABLE_VIDEO
#include "videomanager_interface.h"
#endif
#include "fileutils.h"

static int ringFlags = 0;

static void
print_title()
{
    std::cout
        << "Jami Daemon " << libjami::version()
        << ", by Savoir-faire Linux 2004-2023" << std::endl
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
        {"debug", no_argument, NULL, 'd'},
        {"console", no_argument, NULL, 'c'},
        {"persistent", no_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"auto-answer", no_argument, &autoAnswer, true},
        {0, 0, 0, 0} /* Sentinel */
    };

    while (true) {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        auto c = getopt_long(argc, argv, "dcphv", long_options, &option_index);

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

static int
osxTests()
{
    using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

    libjami::init(static_cast<libjami::InitFlag>(ringFlags));

    registerSignalHandlers(std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>>());

    if (!libjami::start())
            return -1;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    libjami::fini();
}

static int
run()
{
    osxTests();
    return 0;
}

static void
interrupt()
{}

static void
signal_handler(int code)
{
    std::cerr << "Caught signal " << strsignal(code)
              << ", terminating..." << std::endl;

    // Unset signal handlers
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    interrupt();
}

int
main(int argc, char *argv [])
{
    // make a copy as we don't want to modify argv[0], copy it to a vector to
    // guarantee that memory is correctly managed/exception safe
    std::string programName {argv[0]};
    std::vector<char> writable(programName.size() + 1);
    std::copy(programName.begin(), programName.end(), writable.begin());

    jami::fileutils::set_program_dir(writable.data());

#ifdef TOP_BUILDDIR
    if (!getenv("CODECS_PATH"))
        setenv("CODECS_PATH", TOP_BUILDDIR "/src/media/audio/codecs", 1);
#endif

    print_title();

    bool persistent = false;
    if (parse_args(argc, argv, persistent))
        return 0;

    // TODO: Block signals for all threads but the main thread, decide how/if we should
    // handle other signals
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    return run();
}
