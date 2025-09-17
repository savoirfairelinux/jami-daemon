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
#pragma once

#include "simclient.h"

#include "jamidht/jamiaccount.h"
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/conversation_module.h"

#include <map>
#include <memory>
#include <string>

namespace jami {
namespace test {

// Events to select from randomly
enum class ConversationEvent : std::uint8_t {
    ADD_MEMBER = 0,
    SEND_MESSAGE = 1,
    CONNECT = 2,
    DISCONNECT = 3,
    PULL = 4,
    CLONE = 5
};

/**
 * A structure containing the relevant data for account and repo simulation. Meant to avoid calls to the
 * instance manager.
 */
struct RepositoryAccount
{
    /*!< Pointer to the JamiAccount associated with the repository */
    std::shared_ptr<JamiAccount> account;
    /*!< Pointer to the repository object of the account*/
    std::unique_ptr<ConversationRepository> repository;
    /*!< Indicator for whether or not the account is capable of performing connection-dependent actions*/
    bool connected {true};
    /*!< Marker for initializing the account, only used in setUp()*/
    bool identityLoaded {false};
    /*!< List of messages that the account has on the client-side (i.e. "on their chatview")*/
    std::vector<jami::ConversationCommit> messages;
    /*!< Marker for whether or not this particular account successfully received the SwarmLoaded signal*/
    bool swarmLoaded {false};

    // TODO comment
    SimClient client;

    RepositoryAccount(std::shared_ptr<JamiAccount> acc, std::unique_ptr<ConversationRepository> repo)
        : account(std::move(acc))
        , repository(std::move(repo))
    {}

    // TODO Figure out when this should be called and how it should behave based
    // on how clients manage their commit histories.
    void addConversationCommit(const std::string& commitID)
    {
        auto convCommit = repository->getCommit(commitID);
        if (convCommit != std::nullopt) {
            // TODO Why would the commit already be there?
            auto it = std::find_if(messages.begin(), messages.end(), [&](const jami::ConversationCommit& c) {
                return c.id == commitID;
            });
            if (it != messages.end()) {
                messages.erase(it);
            }
            messages.emplace_back(*convCommit);
        }
    }
};

// Event structure, each having a type and a timestamp
struct Event
{
    int instigatorAccountIndex;
    int receivingAccountIndex;
    ConversationEvent type;
    std::chrono::nanoseconds timeOfOccurrence;
    // Enable this IFF the proceeding event is to be ran in parallel
    // For example:
    // { A.expectIncomingParallelEvent=false, B.expectIncomingParallelEvent=true, C.expectIncomingParallelEvent=true,
    // D.expectIncomingParallelEvent=false }
    // implies that events B and C are to be simulated in a parallel fashion
    bool expectIncomingParallelEvent;

    // For construction of an event struct
    Event(int instigatorAccountIndex,
          int receivingAccountIndex,
          ConversationEvent type,
          std::chrono::nanoseconds timeOfOccurrence,
          bool expectIncomingParallelEvent = false)
        : instigatorAccountIndex(instigatorAccountIndex)
        , receivingAccountIndex(receivingAccountIndex)
        , type(type)
        , timeOfOccurrence(timeOfOccurrence)
        , expectIncomingParallelEvent(expectIncomingParallelEvent)
    {}
};

struct UnitTest
{
    int numAccounts = 0;
    std::vector<Event> events;
};

class ConversationDST
{
public:
    ConversationDST(bool enableEventLogging = false, bool enableGitLogging = false)
        : enableEventLogging_(enableEventLogging)
        , enableGitLogging_(enableGitLogging)
    {}
    ~ConversationDST() {}

    bool setUp(int numAccountsToSimulate = MAX_ACCOUNTS);

    // Logging
    void eventLogger(const Event& event);
    void displayGitLog();

    // Event sequence generation dependencies
    void scheduleGitEvent(const ConversationEvent& gitOperation,
                          int instigatorAccountIndex,
                          int receivingAccountIndex,
                          std::chrono::nanoseconds eventTimeOfOccurrence);
    void scheduleSideEffects(const Event& event);
    bool isUserInRepo(int accountIndexToSearch, int accountIndexToFind);
    bool validateEvent(const Event& event);
    void triggerEvent(const Event& event);
    void triggerEvents(const std::vector<Event>& parallelEvents);

    // Test runner dependencies
    bool verifyLoadConversationFromScratch();
    void generateEventSequence(unsigned maxEvents);
    void resetRepositories();

    // Checkers
    std::vector<jami::ConversationCommit> getHistoricalOrder(std::vector<jami::ConversationCommit> messages);
    bool checkAppearances(const RepositoryAccount& repoAcc);
    bool checkAppearancesForAllAccounts();

    // Unit testing
    UnitTest loadUnitTestConfig(const std::string& unitTestPath);
    void saveAsUnitTestConfig(std::string saveFilePath);

    // Test runner
    bool run(uint64_t seed, unsigned maxEvents, std::string saveFilePath = "");
    void runCycles(unsigned numCycles, unsigned maxEvents);
    void runUnitTest(const UnitTest& unitTest);

private:
    static constexpr int MAX_ACCOUNTS = 6;

    // Accounts
    std::vector<RepositoryAccount> repositoryAccounts;
    const std::vector<std::string> displayNames = {"ALICE",  "BOB",   "CHARLIE", "DAVE",   "EMILY", "FRANK", "GREG",
                                                   "HOLLY",  "IAN",   "JENNA",   "KEVIN",  "LUCY",  "MIKE",  "NORA",
                                                   "OLIVIA", "PETE",  "QUINN",   "RACHEL", "SAM",   "TOM",   "UMA",
                                                   "VICTOR", "WENDY", "XANDER",  "YVONNE", "ZOE"};
    int numAccountsToSimulate_ = 0;

    // Signal handlers - must be persistent
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    // Random number generation
    std::random_device rd;
    uint64_t eventSeed;

    // Events
    std::vector<Event> unvalidatedEvents;
    std::vector<Event> validatedEvents;
    // Weightings for the event distribution (note that each weight
    // corresponds to the exact ordering of ther ConversationEvent enum)
    std::vector<double> repositoryEventWeights {3, 5, 1, 1};
    int invalidEventsCount = 0;
    std::chrono::nanoseconds startTime {0};
    std::mt19937_64 gen_;

    float sumOfRejectionRates = 0;

    bool runningUnitTest_ {false};

    // = Logging Preferences =
    // Enable these for verbose logging. You may wish to disable these if generating a large number of cycles
    bool enableEventLogging_;
    bool enableGitLogging_;

    int msgCount = 0;
};

} // namespace test
} // namespace jami
