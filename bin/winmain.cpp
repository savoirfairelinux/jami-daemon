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

#include <windows.h>

static int ringFlags = 0;
bool loop = true;

static void
print_title()
{
    std::cout
        << "Ring Daemon " << DRing::version()
        << ", by Savoir-Faire Linux 2004-2015" << std::endl
        << "http://www.sflphone.org/" << std::endl;
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

using namespace std::placeholders;

bool isActive = false;

void IncomingCall(const std::string& accountId, const std::string& callId, const std::string& message) {
    if (isActive == false) {
        DRing::accept(callId);
        isActive = true;
    }
    else
        DRing::refuse(callId);
}

static int
run()
{
    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

    DRing::init(static_cast<DRing::InitFlag>(ringFlags));

    std::map<std::string, SharedCallback> callHandlers;
    callHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(std::bind(&IncomingCall, _1, _2, _3)));

    registerCallHandlers(callHandlers);
    registerConfHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
    registerPresHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());

    if (!DRing::start())
            return -1;

    while (loop) {
        DRing::pollEvents();
        Sleep(1);
    }

    DRing::fini();
}

static void
interrupt()
{
	loop = false;
}

static void
signal_handler(int code)
{
    std::cerr << "Caught signal " << code
              << ", terminating..." << std::endl;
    // Unset signal handlers
    #ifndef _WIN32
    signal(SIGHUP, SIG_DFL);
    #endif
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

    print_title();

    bool persistent = false;
    if (parse_args(argc, argv, persistent))
        return 0;

    // TODO: Block signals for all threads but the main thread, decide how/if we should
    // handle other signals
    signal(SIGINT, signal_handler);
    #ifndef _WIN32
    signal(SIGHUP, signal_handler);
    #endif
    signal(SIGTERM, signal_handler);

#ifdef RING_VIDEO
    std::cerr << "Warning: built with video support" << std::endl;
#endif

    return run();
}