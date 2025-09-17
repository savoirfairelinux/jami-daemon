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

static void
printUsage()
{
    static const char* usage
        = "Usage: jamidst [--seed=<SEED> | --cycles=<NUM> | --input=<PATH>]\n"
          "               [--max-events=<NUM>] [--keep-repos] [--output=<PATH>]\n"
          "               [--event-log] [--git-log] [--daemon-log]\n"
          "\n"
          "Main options (mutually exclusive):\n"
          " --seed=<SEED>       Perform one simulation using the specified seed\n"
          " --cycles=<NUM>      Perform <NUM> simulations using randomly generated seeds\n"
          " --input=<PATH>      Execute the sequence of events from the specified input file\n"
          "\n"
          "Other options:\n"
          " --max-events=<NUM>  Maximum number of events to generate per simulation (default: 500)\n"
          " --keep-repos        Keep the generated git repositories after execution\n"
          " --output=<PATH>     File path to save the generated sequence of events (when using --seed)\n"
          " --event-log         Enable logging of events\n"
          " --git-log           Enable logging of git operations\n"
          " --daemon-log        Enable daemon logs\n";
    fmt::print("{}", usage);
}

struct CLIArgs
{
    std::string numCycles = "";
    std::string inputPath = "";
    std::string seed = "";

    std::string outputPath = "";
    std::string maxEvents = "";
    bool keepRepos = false;
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
        {"keep-repos", no_argument, nullptr, 'k'},
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
        case 'k':
            args.keepRepos = true;
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
        fatalError(fmt::format("{} positional arguments provided, expected none.\n", positionalArgCount));
    }
    return args;
}

int
main(int argc, char* argv[])
{
    CLIArgs args = parseArguments(argc, argv);
    int mainOptions = !args.numCycles.empty() + !args.inputPath.empty() + !args.seed.empty();
    if (mainOptions > 1) {
        fatalError("Options --cycles, --input, and --seed are mutually exclusive.\n");
    }
    if (mainOptions == 0) {
        printUsage();
        return 0;
    }

    unsigned maxEvents = 500;
    if (!args.maxEvents.empty()) {
        try {
            maxEvents = std::stoul(args.maxEvents);
        } catch (const std::exception& e) {
            fatalError(fmt::format("Failed to parse max-events: {}\n", e.what()));
        }
    }

    libjami::InitFlag initFlags {};
    if (args.enableDaemonLog) {
        initFlags = libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG);
    }
    libjami::init(initFlags);
    if (!libjami::start("dst-config.yml")) {
        fatalError("Failed to start libjami\n");
    }

    jami::test::ConversationDST dst(args.enableEventLog, args.enableGitLog);

    int ret = 0;
    if (!args.inputPath.empty()) {
        jami::test::UnitTest unitTest = dst.loadUnitTestConfig(args.inputPath);
        if (unitTest.numAccounts == 0 || unitTest.events.empty()) {
            fatalError("");
        }

        if (!dst.setUp(unitTest.numAccounts)) {
            fatalError("Failed to set up accounts\n");
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

        if (!dst.setUp()) {
            fatalError("Failed to set up accounts\n");
        }
        if (!dst.run(seed, maxEvents, args.outputPath)) {
            ret = 1;
        }
    } else if (!args.numCycles.empty()) {
        unsigned numCycles;
        try {
            numCycles = std::stoul(args.numCycles);
        } catch (const std::exception& e) {
            fatalError(fmt::format("Failed to parse number of cycles: {}\n", e.what()));
        }

        if (!dst.setUp()) {
            fatalError("Failed to set up accounts\n");
        }
        if (!dst.runCycles(numCycles, maxEvents)) {
            ret = 1;
        }
    }

    if (!args.keepRepos) {
        dst.resetRepositories();
    }
    libjami::fini();

    return ret;
}
