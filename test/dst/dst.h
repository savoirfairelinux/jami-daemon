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
#include <queue>
#include <string>

namespace jami {
namespace test {

// Events to select from randomly
enum class ConversationEvent : std::uint8_t {
    ADD_MEMBER = 0,
    SEND_MESSAGE = 1,
    CONNECT = 2,
    DISCONNECT = 3,
    FETCH = 4,
    MERGE = 5,
    CLONE = 6
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
    std::unique_ptr<Conversation> conversation;
    /*!< Indicator for whether or not the account is capable of performing connection-dependent actions*/
    bool connected {true};
    // TODO comment
    std::unordered_set<std::string> devicesWithPendingFetch;
    /*!< Marker for initializing the account, only used in setUp()*/
    bool identityLoaded {false};
    /*!< Marker for whether or not this particular account successfully received the SwarmLoaded signal*/
    bool swarmLoaded {false};

    // TODO comment
    SimClient client;

    RepositoryAccount(std::shared_ptr<JamiAccount> acc, std::unique_ptr<ConversationRepository> repo)
        : account(std::move(acc))
        , repository(std::move(repo))
    {}

    void createConversation(std::unique_ptr<ConversationRepository>&& repo)
    {
        repository = std::move(repo);

        auto accountId = account->getAccountID();
        auto conversationId = repository->id();

        // TODO Emit a signal instead?
        client.onConversationReady(accountId, conversationId);

        conversation = std::make_unique<Conversation>(account, conversationId);
        auto messages = conversation->loadMessagesSync({});
        emitSignal<libjami::ConversationSignal::SwarmLoaded>(0, accountId, conversationId, messages);
    }
};

// Event structure, each having a type and a timestamp
struct Event
{
    int instigatorAccountIndex;
    int receivingAccountIndex;
    ConversationEvent type;
    std::chrono::nanoseconds timeOfOccurrence;

    // For construction of an event struct
    Event(int instigatorAccountIndex,
          int receivingAccountIndex,
          ConversationEvent type,
          std::chrono::nanoseconds timeOfOccurrence)
        : instigatorAccountIndex(instigatorAccountIndex)
        , receivingAccountIndex(receivingAccountIndex)
        , type(type)
        , timeOfOccurrence(timeOfOccurrence)
    {}
};
struct EventComparator
{
    bool operator()(const Event& a, const Event& b) const { return a.timeOfOccurrence > b.timeOfOccurrence; }
};
using EventQueue = std::priority_queue<Event, std::vector<Event>, EventComparator>;

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
    void scheduleGitEvent(EventQueue& queue,
                          const ConversationEvent& gitOperation,
                          int instigatorAccountIndex,
                          int receivingAccountIndex,
                          std::chrono::nanoseconds eventTimeOfOccurrence);
    void scheduleSideEffects(EventQueue& queue, const Event& event);
    bool isUserInRepo(int accountIndexToSearch, int accountIndexToFind);
    bool validateEvent(const Event& event);
    void triggerEvent(const Event& event, EventQueue* queue = nullptr);

    // Test runner dependencies
    bool verifyLoadConversationFromScratch();
    void generateEventSequence(unsigned maxEvents);
    void resetRepositories();

    // Checkers
    bool checkAppearances(const RepositoryAccount& repoAcc);
    bool checkAppearancesForAllAccounts();

    // Unit testing
    UnitTest loadUnitTestConfig(const std::string& unitTestPath);
    void saveAsUnitTestConfig(std::string saveFilePath);

    // Test runner
    bool run(uint64_t seed, unsigned maxEvents, std::string saveFilePath = "");
    bool runCycles(unsigned numCycles, unsigned maxEvents);
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
    std::vector<Event> validatedEvents;
    // Weightings for the event distribution (note that each weight
    // corresponds to the exact ordering of ther ConversationEvent enum)
    std::vector<double> repositoryEventWeights {3, 5, 1, 1};
    int invalidEventsCount = 0;
    std::chrono::nanoseconds startTime {0};
    std::mt19937_64 gen_;

    float sumOfRejectionRates = 0;

    // = Logging Preferences =
    // Enable these for verbose logging. You may wish to disable these if generating a large number of cycles
    bool enableEventLogging_;
    bool enableGitLogging_;

    int msgCount = 0;
};

} // namespace test
} // namespace jami
