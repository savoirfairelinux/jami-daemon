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

#include <map>
#include <memory>
#include <string>

#include "jamidht/jamiaccount.h"
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/conversation_module.h"

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

// Define a map for the event and its string name (for logging purposes)
std::map<ConversationEvent, std::string> eventNames {{ConversationEvent::ADD_MEMBER, "ADD_MEMBER"},
                                                     {ConversationEvent::SEND_MESSAGE, "SEND_MESSAGE"},
                                                     {ConversationEvent::CONNECT, "CONNECT"},
                                                     {ConversationEvent::DISCONNECT, "DISCONNECT"},
                                                     {ConversationEvent::PULL, "PULL"},
                                                     {ConversationEvent::CLONE, "CLONE"}};
std::map<std::string, ConversationEvent> invertedEventNames {{"ADD_MEMBER", ConversationEvent::ADD_MEMBER},
                                                             {"SEND_MESSAGE", ConversationEvent::SEND_MESSAGE},
                                                             {"CONNECT", ConversationEvent::CONNECT},
                                                             {"DISCONNECT", ConversationEvent::DISCONNECT},
                                                             {"PULL", ConversationEvent::PULL},
                                                             {"CLONE", ConversationEvent::CLONE}};

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
    bool connected {false};
    /*!< Marker for initializing the account, only use in connectSignals()*/
    bool deviceAnnounced {false};
    /*!< List of messages that the account has on the client-side (i.e. "on their chatview")*/
    std::vector<jami::ConversationCommit> messages;
    /*!< Marker for whether or not this particular account successfully received the SwarmLoaded signal*/
    bool swarmLoaded {false};

    RepositoryAccount(std::shared_ptr<JamiAccount> acc, std::unique_ptr<ConversationRepository> repo, bool connected)
        : account(std::move(acc))
        , repository(std::move(repo))
        , connected(connected)
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
/**
 * @brief The configuration of the DST representing how it should behave
 */
struct DSTConfiguration
{
    // = DST Preference =
    int numCycles;
    // This should always be true when running more than one cycle of random repo generation,
    // otherwise the tests will segfault. When re-simulating a specific seed, set this to false so
    // that the repo contents can be examined post-test if needed. However, do not forget to delete
    // the created accounts manually, otherwise you will accumulate junk accounts.
    bool enableResetRepoBetweenIterations;
    // TODO The above is not used; fix or delete

    // = Logging Preferences =
    // Enable these for verbose logging. You may wish to disable these if generating a large number of cycles
    bool enableEventLogging;
    bool enableGitLogging;

    // = Unit test Preferences =
    bool saveGenerationsAsUnitTests;
    bool runUnitTest;
    std::string unitTestName;

    DSTConfiguration(int cycles,
                     bool resetRepositories,
                     bool eventLogging,
                     bool gitLogging,
                     bool saveTests,
                     bool runTests,
                     const std::string& testFolderName)
        : numCycles(cycles)
        , enableResetRepoBetweenIterations(resetRepositories)
        , enableEventLogging(eventLogging)
        , enableGitLogging(gitLogging)
        , saveGenerationsAsUnitTests(saveTests)
        , runUnitTest(runTests)
        , unitTestName(testFolderName)
    {}
};

// Keys used in config.json
enum class UTKEY {
    SEED,
    DATE,
    TIME,
    DESC,
    ACCOUNT_IDS,
    VALIDATED_EVENTS,
    EVENT_TYPE,
    INSTIGATOR_ACCOUNT_ID,
    RECEIVING_ACCOUNT_ID
};
std::map<UTKEY, std::string> unitTestKeys {{UTKEY::SEED, "seed"},
                                           {UTKEY::DATE, "date"},
                                           {UTKEY::TIME, "time"},
                                           {UTKEY::DESC, "desc"},
                                           {UTKEY::ACCOUNT_IDS, "accountIDs"},
                                           {UTKEY::VALIDATED_EVENTS, "validatedEvents"},
                                           {UTKEY::EVENT_TYPE, "eventType"},
                                           {UTKEY::INSTIGATOR_ACCOUNT_ID, "instigatorAccountID"},
                                           {UTKEY::RECEIVING_ACCOUNT_ID, "receivingAccountID"}};

class ConversationDST
{};

} // namespace test
} // namespace jami
