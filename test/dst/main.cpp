/*
 *  Copyright (C) 2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "dst.h"
#include "manager.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <getopt.h>

/*
constexpr int NUM_EVENTS = 500; // TODO This is currently unused...
constexpr bool SAVE_AS_UNIT_TESTS = false;
constexpr bool ENABLE_EVENT_LOGGER = true;
constexpr bool ENABLE_GIT_LOGGER = true;
constexpr bool RESET_REPOS = false;
constexpr bool IS_UNIT_TEST = false;
constexpr const char* UNIT_TEST_FOLDER_NAME = "";
*/
// TODO Add option to keep repos when using --input or --seed
/*
jamidst [--cycles=<NUM> | --input=<PATH> | --seed=<SEED>] [--max-events=<NUM>] [--event-log] [--git-log] [--daemon-log]
        [--output=<PATH>]
jamidst accounts
*/

struct CLIArgs
{
    std::string command = "";

    std::string numCycles = "";
    std::string inputPath = "";
    std::string seed = "";

    std::string outputPath = "";
    std::string maxEvents = "";
    bool enableEventLog = false;
    bool enableGitLog = false;
    bool enableDaemonLog = false;
};

static void
fatalError(const std::string& message)
{
    if (!message.empty()) {
        fmt::print(stderr, "{}", message);
    }
    exit(1);
}

static CLIArgs
parseArguments(int argc, char* argv[])
{
    static const struct option longOptions[] = {
        {"cycles", required_argument, nullptr, 'c'},
        {"input", required_argument, nullptr, 'i'},
        {"seed", required_argument, nullptr, 's'},
        {"output", required_argument, nullptr, 'o'},
        {"max-events", required_argument, nullptr, 'm'},
        {"event-log", no_argument, nullptr, 'e'},
        {"git-log", no_argument, nullptr, 'g'},
        {"daemon-log", no_argument, nullptr, 'd'},
        {nullptr, 0, nullptr, 0},
    };
    CLIArgs args;
    while (true) {
        int optionIndex = 0;
        int c = getopt_long(argc, argv, "", longOptions, &optionIndex);
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            args.numCycles = optarg;
            break;
        case 'i':
            args.inputPath = optarg;
            break;
        case 's':
            args.seed = optarg;
            break;
        case 'o':
            args.outputPath = optarg;
            break;
        case 'm':
            args.maxEvents = optarg;
            break;
        case 'e':
            args.enableEventLog = true;
            break;
        case 'g':
            args.enableGitLog = true;
            break;
        case 'd':
            args.enableDaemonLog = true;
            break;
        default:
            fatalError("");
        }
    }

    if (optind < argc) {
        int positionalArgCount = argc - optind;
        if (positionalArgCount > 1) {
            fatalError("Too many positional arguments provided.\n");
        }
        args.command = argv[optind];
    }
    return args;
}

int
main(int argc, char* argv[])
{
    CLIArgs args = parseArguments(argc, argv);
    fmt::print("command: {}\n", args.command);
    fmt::print("numCycles: {}\n", args.numCycles);
    fmt::print("inputPath: {}\n", args.inputPath);
    fmt::print("seed: {}\n", args.seed);
    fmt::print("outputPath: {}\n", args.outputPath);
    fmt::print("maxEvents: {}\n", args.maxEvents);
    fmt::print("enableEventLog: {}\n", args.enableEventLog);
    fmt::print("enableGitLog: {}\n", args.enableGitLog);
    fmt::print("enableDaemonLog: {}\n", args.enableDaemonLog);

    libjami::InitFlag initFlags {};
    if (args.enableDaemonLog) {
        initFlags = libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG);
    }
    libjami::init(initFlags);
    if (!libjami::start("dst-config.yml")) {
        fatalError("Failed to start libjami\n");
    }

    jami::test::ConversationDST dst;

    int ret = 0;
    if (!args.inputPath.empty()) {
        jami::test::UnitTest unitTest = dst.loadUnitTestConfig(args.inputPath);
        if (unitTest.numAccounts == 0 || unitTest.events.empty()) {
            fatalError("");
        }

        dst.setUp(unitTest.numAccounts);
        dst.connectSignals();
        if (!dst.waitForDeviceAnnouncements()) {
            fatalError("Device announcements timed out\n");
        }
        dst.runUnitTest(unitTest);
        if (!dst.checkAppearancesForAllAccounts()) {
            ret = 1;
        }
    } else if (!args.seed.empty()) {
        uint64_t seed;
        try {
            seed = std::stoull(args.seed);
        } catch (const std::exception& e) {
            fatalError(fmt::format("Failed to parse seed: {}\n", e.what()));
        }
        dst.setUp();
        dst.connectSignals();
        if (!dst.waitForDeviceAnnouncements()) {
            fatalError("Device announcements timed out\n");
        }
        if (!dst.run(seed)) {
            ret = 1;
        }
    } else if (!args.numCycles.empty()) {
        unsigned numCycles;
        try {
            numCycles = std::stoul(args.numCycles);
        } catch (const std::exception& e) {
            fatalError(fmt::format("Failed to parse number of cycles: {}\n", e.what()));
        }
        dst.setUp();
        dst.connectSignals();
        if (!dst.waitForDeviceAnnouncements()) {
            fatalError("Device announcements timed out\n");
        }
        dst.runCycles(numCycles);
    }

    dst.tearDown();
    return ret;
}
