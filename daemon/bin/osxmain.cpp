/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "sflphone.h"
#include "fileutils.h"

static int sflphFlags = 0;

static void print_title()
{
    std::cout << "SFLphone Daemon " << sflph_version() <<
        ", by Savoir-Faire Linux 2004-2014" << std::endl <<
        "http://www.sflphone.org/" << std::endl;
}

static void print_usage()
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
static bool parse_args(int argc, char *argv[], bool &persistent)
{
    int consoleFlag = false;
    int debugFlag = false;
    int helpFlag = false;
    int versionFlag = false;
    static const struct option long_options[] = {
        /* These options set a flag. */
        {"debug", no_argument, NULL, 'd'},
        {"console", no_argument, NULL, 'c'},
        {"persistent", no_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {0, 0, 0, 0} /* Sentinel */
    };

    while (true) {
        /* getopt_long stores the option index here. */
        int option_index = 0;
        int c = getopt_long(argc, argv, "dcphv", long_options, &option_index);

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

    bool quit = false;
    if (helpFlag) {
        print_usage();
        quit = true;
    } else if (versionFlag) {
        // We've always print the title/version, so we can just exit
        quit = true;
    } else {
        if (consoleFlag) {
            sflphFlags |= SFLPH_FLAG_CONSOLE_LOG;
        }
        if (debugFlag) {
            sflphFlags |= SFLPH_FLAG_DEBUG;
        }
    }
    return quit;
}

static int osxTests()
{
    sflph_ev_handlers evHandlers = {
        .call_ev_handlers = {},
        .config_ev_handlers = {},
#ifdef SFL_PRESENCE
        .pres_ev_handlers = {}
#endif
#ifdef SFL_VIDEO
        .video_ev_handlers = {}
#endif
    };

    sflph_init(&evHandlers, static_cast<sflph_init_flag>(sflphFlags));


    sflph_call_play_dtmf("0");
    sflph_config_get_audio_output_device_list();

}
static int run()
{
    osxTests();
    return 1;
}

static void interrupt()
{}

static void signal_handler(int code)
{
    std::cerr << "Caught signal " << strsignal(code)
                  << ", terminating..." << std::endl;

    // Unset signal handlers
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    interrupt();
}

int main(int argc, char *argv [])
{
    int ret;

    // make a copy as we don't want to modify argv[0], copy it to a vector to
    // guarantee that memory is correctly managed/exception safe
    std::string programName(argv[0]);
    std::vector<char> writable(programName.size() + 1);
    std::copy(programName.begin(), programName.end(), writable.begin());

    fileutils::set_program_dir(writable.data());

#ifdef TOP_BUILDDIR
    if (!getenv("CODECS_PATH"))
        setenv("CODECS_PATH", TOP_BUILDDIR "/src/audio/codecs", 1);
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

#ifdef SFL_VIDEO
    std::cerr << "Warning: built with video support" << std::endl;
#endif

    ret = run();

    return ret;
}
