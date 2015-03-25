/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include <iostream>
#include <thread>
#include <cstring>
#include <signal.h>
#include <getopt.h>
#include <string>

#include "dring.h"
#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"
#ifdef RING_VIDEO
#include "videomanager_interface.h"
#endif
#include "fileutils.h"

static int ringFlags = 0;

static void
print_title()
{
    std::cout << "Ring Daemon " << DRing::version()
              << ", by Savoir-Faire Linux 2004-2015" << std::endl
              << "http://www.ring.cx/" << std::endl;
}

static void
print_usage()
{
    std::cout << std::endl <<
    "-c, --console \t- Log in console (instead of syslog)" << std::endl <<
    "-d, --debug \t- Debug mode (more verbose)" << std::endl <<
    "-p, --persistent \t- Stay alive after client quits" << std::endl <<
    "-h, --help \t- Print help" << std::endl;
}

// Parse command line arguments, setting debug options or printing a help
// message accordingly.
// returns true if we should quit (i.e. help was printed), false otherwise
static bool
parse_args(int argc, char *argv[], bool& persistent)
{
    static const struct option long_options[] = {
        /* These options set a flag. */
        {"debug", no_argument, NULL, 'd'},
        {"console", no_argument, NULL, 'c'},
        {"persistent", no_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {0, 0, 0, 0} /* Sentinel */
    };

    int consoleFlag = false;
    int debugFlag = false;
    int helpFlag = false;
    int versionFlag = false;

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
        ringFlags |= DRing::DRING_FLAG_CONSOLE_LOG;

    if (debugFlag)
        ringFlags |= DRing::DRING_FLAG_DEBUG;

    return false;
}

static int
osxTests()
{
    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

    DRing::init(static_cast<DRing::InitFlag>(ringFlags));

    registerCallHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
    registerConfHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
    registerPresHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
#ifdef RING_VIDEO
    registerVideoHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
#endif
    if (!DRing::start())
            return -1;

    while (true) {
        DRing::pollEvents();
        sleep(1);
    }

    DRing::fini();
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

    ring::fileutils::set_program_dir(writable.data());

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

#ifdef RING_VIDEO
    std::cerr << "Warning: built with video support" << std::endl;
#endif

    return run();
}
