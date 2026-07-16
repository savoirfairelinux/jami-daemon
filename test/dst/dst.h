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
    // Primary events (generated randomly by the simulator)
    ADD_MEMBER = 0,
    SEND_MESSAGE = 1,
    CONNECT = 2,
    DISCONNECT = 3,
    SEND_FILE = 4,
    ADD_REACTION = 5,
    HOST_CONFERENCE = 6,
    UPDATE_PROFILE = 7,

    // Secondary events (only generated in response to other events)
    FETCH = 8,
    MERGE = 9,
    CLONE = 10,
    DELETE_FILE = 11,
    EDIT_MESSAGE = 12,
    REMOVE_REACTION = 13,
    DELETE_MESSAGE = 14,
    END_CONFERENCE = 15
};
static constexpr uint8_t NUM_PRIMARY_EVENTS = 8;

/**
 * A structure containing the relevant data for account and repo simulation.
 */
struct RepositoryAccount
{
    std::shared_ptr<JamiAccount> account;
    std::unique_ptr<ConversationRepository> repository;
    std::unique_ptr<Conversation> conversation;
    /*!< Indicator for whether or not the account is capable of performing connection-dependent actions*/
    bool connected {true};
    std::unordered_set<std::string> devicesWithPendingFetch;
    /*!< Marker for initializing the account, only used in setUp()*/
    bool identityLoaded {false};

    SimClient client;

    RepositoryAccount(std::shared_ptr<JamiAccount> acc)
        : account(std::move(acc))
    {}

    void createConversation(std::unique_ptr<ConversationRepository>&& repo,
                            std::vector<ConversationCommit>&& commits = {})
    {
        repository = std::move(repo);

        auto accountId = account->getAccountID();
        auto conversationId = repository->id();
        emitSignal<libjami::ConversationSignal::ConversationReady>(accountId, conversationId);

        // The RepositoryAccount keeps its own repository handle for direct use by the simulator, so
        // we hand the Conversation a separate handle along with its commits. Passing the commits
        // lets the Conversation initialize its active-calls list (and emit ActiveCallsChanged),
        // mirroring what the daemon does when opening a conversation cloned from a peer.
        auto convRepo = std::make_unique<ConversationRepository>(account, conversationId);
        conversation = std::make_unique<Conversation>(std::move(convRepo), account, std::move(commits));
        auto members = conversation->getMembers(true, true, true);
        client.setMemberRoles(members);

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
    // Index of the message targeted by a reaction, edition or deletion (-1 if not applicable).
    int targetMessageIndex {-1};
    // Index of the message this one replies to, for SEND_MESSAGE/SEND_FILE (-1 if not a reply).
    int replyToIndex {-1};

    // For construction of an event struct
    Event(int instigatorAccountIndex,
          int receivingAccountIndex,
          ConversationEvent type,
          std::chrono::nanoseconds timeOfOccurrence,
          int targetMessageIndex = -1,
          int replyToIndex = -1)
        : instigatorAccountIndex(instigatorAccountIndex)
        , receivingAccountIndex(receivingAccountIndex)
        , type(type)
        , timeOfOccurrence(timeOfOccurrence)
        , targetMessageIndex(targetMessageIndex)
        , replyToIndex(replyToIndex)
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
    {
        ConversationRepository::FETCH_FROM_LOCAL_REPOS = true;

        // Disabling fsync can make simulations more than twice as fast.
        git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR, 0);
    }
    ~ConversationDST() {}

    bool setUp(int numAccountsToSimulate = MAX_ACCOUNTS);
    void resetRepositories();

    // Checkers
    bool verifyLoadConversationFromScratch();
    std::vector<libjami::SwarmMessage> computeExpectedMessages(const RepositoryAccount& repoAcc) const;
    std::vector<std::map<std::string, std::string>> computeExpectedActiveCalls(const RepositoryAccount& repoAcc) const;
    bool checkActiveCalls(const RepositoryAccount& repoAcc);
    bool checkMessagesMatch(const RepositoryAccount& repoAcc,
                            const std::vector<libjami::SwarmMessage>& expected,
                            const std::vector<libjami::SwarmMessage>& actual);
    bool checkAppearances(const RepositoryAccount& repoAcc);
    bool checkConversationMembers(const RepositoryAccount& repoAcc);
    bool checkAllAccounts();

    // Unit testing
    UnitTest loadUnitTestConfig(const std::string& unitTestPath);
    void saveAsUnitTestConfig(const std::string& saveFilePath);

    // Test runner
    bool run(uint64_t seed, unsigned maxEvents, const std::string& saveFilePath = "");
    bool runCycles(unsigned numCycles, unsigned maxEvents);
    void runUnitTest(const UnitTest& unitTest);

private:
    // Logging
    void eventLogger(const Event& event);
    void displayGitLog();

    // Event sequence generation dependencies
    void generateEventSequence(unsigned maxEvents);
    Event generatePrimaryEvent(std::chrono::nanoseconds time, std::discrete_distribution<>& eventDist);
    void scheduleGitEvent(EventQueue& queue,
                          const ConversationEvent& gitOperation,
                          int instigatorAccountIndex,
                          int receivingAccountIndex,
                          std::chrono::nanoseconds eventTimeOfOccurrence);
    bool isUserInRepo(int accountIndexToSearch, int accountIndexToFind);
    bool validateEvent(const Event& event);
    void triggerEvent(const Event& event, EventQueue* queue = nullptr);

    float rand01() { return std::uniform_real_distribution<float>(0.f, 1.f)(gen_); }

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
    std::chrono::nanoseconds startTime {0};
    std::mt19937_64 gen_;

    float sumOfRejectionRates = 0;
    double sumOfJoinRates = 0;

    // = Logging Preferences =
    // Enable these for verbose logging. You may wish to disable these if generating a large number of cycles
    bool enableEventLogging_;
    bool enableGitLogging_;

    int msgCount = 0;
    int fileCount = 0;
    int conferenceCount = 0;
    int profileUpdateCount = 0;
};

} // namespace test
} // namespace jami
