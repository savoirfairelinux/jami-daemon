/*
 *  Copyright (C) 2025 Savoir-faire Linux Inc.
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

// CPPUnit
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

// For Messner-Twister engine
#include <random>
// For std::find
#include <algorithm>

// JamiDHT
#include "jamidht/jamiaccount.h"

// Commons
#include "manager.h"
#include "../../../test_runner.h"
#include "account_const.h"
#include "common.h"

using namespace std::string_literals;
using namespace libjami::Account;

namespace jami {
namespace test {
// Events to select from randomly
enum class ConversationEvent { ADD_MEMBER, SEND_MESSAGE, CONNECT, DISCONNECT };

// Define a map for the event and its string name (for logging purposes)
const std::map<ConversationEvent, std::string> eventNames {{ConversationEvent::ADD_MEMBER,
                                                            "ADD_MEMBER"},
                                                           {ConversationEvent::SEND_MESSAGE,
                                                            "SEND_MESSAGE"},
                                                           {ConversationEvent::CONNECT, "CONNECT"},
                                                           {ConversationEvent::DISCONNECT,
                                                            "DISCONNECT"}};

// Actions that can be performed using ConversationRepository
enum class RepoAction { PULL, JOIN, FETCH, MERGE, CLONE };

// Define a map for the action and its string name (for logging purposes)
const std::map<RepoAction, std::string> actionNames {{RepoAction::PULL, "PULL"},
                                                     {RepoAction::JOIN, "JOIN"},
                                                     {RepoAction::FETCH, "FETCH"},
                                                     {RepoAction::MERGE, "MERGE"},
                                                     {RepoAction::CLONE, "CLONE"}};

// Event structure, each having a type and a timestamp
struct Event
{
    std::string instigatorAccountID;
    std::string receivingAccountID;
    ConversationEvent eventType;
    uint64_t timeOfOccurrence;

    // For construction of an event struct
    Event(const std::string& instigatorAccountID,
          const std::string& receivingAccountID,
          ConversationEvent type,
          uint64_t timestamp)
        : instigatorAccountID(instigatorAccountID)
        , receivingAccountID(receivingAccountID)
        , eventType(type)
        , timeOfOccurrence(timestamp)
    {}
};

class ConversationRepositoryDST : public CppUnit::TestFixture
{
public:
    ConversationRepositoryDST()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConversationRepositoryDST() { libjami::fini(); }
    static std::string name() { return "ConversationRepositoryDST"; }
    void setUp();
    void tearDown();

    // Helper functions
    bool isUserInRepo(const std::string& instigatorAccountID);
    bool isConnected(const std::string& instigatorAccountID);
    void triggerAction(const ConversationEvent event,
                       const std::string& instigatorAccountID,
                       const std::string& receivingAccountID);
    void generateSideEffects(const ConversationEvent event,
                             const std::string& instigatorAccountID,
                             const std::string& receivingAccountID);
    // void actionSideEffect(const ConversationEvent event, const std::string&);
    // Rules
    bool validateActionRule(const ConversationEvent action,
                            const std::string& instigatorAccountID,
                            const std::string& recevingAccountID);

    // Logging
    void logger(const ConversationEvent event,
                const std::string& instigatorAccountID,
                const std::string& receivingAccountID);

    // Unit test functions
    void generateEventSequence();

    // Unit test suite
    CPPUNIT_TEST_SUITE(ConversationRepositoryDST);
    CPPUNIT_TEST(generateEventSequence);
    CPPUNIT_TEST_SUITE_END();

private:
    // The actual repository itself
    std::unique_ptr<jami::ConversationRepository> repository;

    // Actors
    int numAccounts = 4;
    std::vector<std::string> accountIDs {};
    std::uniform_int_distribution<> accountDist {0, numAccounts - 1};

    // Random number generation
    std::random_device rd;
    std::random_device::result_type eventSeed;
    std::random_device::result_type accountsSeed;

    // Events
    int maxEvents = 40;
    std::vector<Event> events;
    std::uniform_int_distribution<> conversationEventDist {0, 3}; // 4 events

    // Define the necessary repo actions to occur based on the event type
    std::map<ConversationEvent, std::vector<RepoAction>>
        eventActionMap {{ConversationEvent::ADD_MEMBER, {RepoAction::CLONE, RepoAction::JOIN}},
                        {ConversationEvent::SEND_MESSAGE, {RepoAction::PULL}}};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryDST, ConversationRepositoryDST::name());

void
ConversationRepositoryDST::setUp()
{
    // Reserve space for account IDs
    accountIDs.reserve(numAccounts);

    // Load the maximum number of actors
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla-davi.yml");

    // Add only the max number of accounts to be simulated
    auto it = actors.begin();
    for (int i = 0; i < numAccounts; ++i) {
        JAMI_LOG("Adding account {} with ID {}", it->first, it->second);
        accountIDs.push_back(it->second);
        ++it;
    }

    // Generate the random seed to be used for event generation
    eventSeed = 301082494; // rd();
    JAMI_LOG("Random seed generated: {}", eventSeed);

    // Reserve space for max number of events
    events.reserve(maxEvents);
}

void
ConversationRepositoryDST::tearDown()
{
    wait_for_removal_of(accountIDs);
}

bool
ConversationRepositoryDST::isUserInRepo(const std::string& instigatorAccountID)
{
    if (!repository) {
        JAMI_WARNING("No repository exists yet");
        return false;
    }

    if (repository->members().empty()) {
        JAMI_WARNING("No members in repiository yet!");
        return false;
    }

    auto account = Manager::instance().getAccount<JamiAccount>(instigatorAccountID);
    if (!account) {
        JAMI_WARNING("Account {} not found", instigatorAccountID);
        return false;
    }

    std::vector<ConversationMember> conversationMembers = repository->members();
    return std::any_of(conversationMembers.begin(),
                       conversationMembers.end(),
                       [&](const jami::ConversationMember& member) {
                           return member.uri == account->getUsername();
                       });
}

bool
ConversationRepositoryDST::isConnected(const std::string& instigatorAccountID)
{
    return Manager::instance().getAccount<JamiAccount>(instigatorAccountID)->isEnabled();
}

bool
ConversationRepositoryDST::validateActionRule(const ConversationEvent action,
                                              const std::string& instigatorAccountID,
                                              const std::string& recevingAccountID)
{
    // Define rules for actions here (i.e. can this action be performed given the current state of
    // the simulation)
    bool instigatorInConv = isUserInRepo(instigatorAccountID);
    bool receiverInConv = isUserInRepo(recevingAccountID);

    switch (action) {
    case ConversationEvent::CONNECT:
        // User can only be connected if already disconnected
        if (!instigatorInConv || isConnected(instigatorAccountID)) {
            JAMI_WARNING("User {} cannot connect", instigatorAccountID);
            return false;
        }
        break;
    case ConversationEvent::DISCONNECT:
        // User can only be disconnected if already connected
        if (!instigatorInConv || !isConnected(instigatorAccountID)) {
            JAMI_WARNING("User {} cannot disconnect", instigatorAccountID);
            return false;
        }
        break;
    case ConversationEvent::ADD_MEMBER:
        // Note: this is within the context of the CURRENT repo
        // The insigator should only be able to add the receiver if:
        // 1. The instigator is not trying to add themselves
        // 2. The instigator is already part of of the conversation
        // 3. The receivier is not already part of the conversation
        if (instigatorAccountID == recevingAccountID || !instigatorInConv || receiverInConv) {
            JAMI_WARNING("User {} cannot add other user {}", instigatorAccountID, recevingAccountID);
            return false;
        }
        break;
    case ConversationEvent::SEND_MESSAGE:
        // User can only send a message if part of conversation
        if (!instigatorInConv) {
            JAMI_WARNING("User {} cannot send message", instigatorAccountID);
            return false;
        }
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
    return true;
}

void
ConversationRepositoryDST::triggerAction(const ConversationEvent event,
                                         const std::string& instigatorAccountID,
                                         const std::string& receivingAccountID)
{
    auto instigatorAccount = Manager::instance().getAccount<JamiAccount>(instigatorAccountID);
    auto receivingAccount = Manager::instance().getAccount<JamiAccount>(receivingAccountID);
    // Perform the necessary action based on the event type
    switch (event) {
    case ConversationEvent::CONNECT:
        // Enable the account
        instigatorAccount->setEnabled(true);
        break;
    case ConversationEvent::DISCONNECT:
        // Disable the account
        instigatorAccount->setEnabled(false);
        break;
    case ConversationEvent::ADD_MEMBER:
        // Add the account as a member
        repository->addMember(receivingAccount->getUsername());
        generateSideEffects(event, instigatorAccountID, receivingAccountID);
        break;
    case ConversationEvent::SEND_MESSAGE:
        JAMI_LOG("Theoretical send message here");
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

void
ConversationRepositoryDST::generateSideEffects(const ConversationEvent event,
                                               const std::string& instigatorAccountID,
                                               const std::string& receivingAccountID)
{
    auto repoActions = eventActionMap.at(event);

    JAMI_LOG("{} will make {} perform actions!",
             Manager::instance().getAccount<JamiAccount>(instigatorAccountID)->getDisplayName(),
             Manager::instance().getAccount<JamiAccount>(receivingAccountID)->getDisplayName());
}

void
ConversationRepositoryDST::logger(const ConversationEvent event,
                                  const std::string& instigatorAccountID,
                                  const std::string& receivingAccountID)
{
    auto instigatorName
        = Manager::instance().getAccount<JamiAccount>(instigatorAccountID)->getDisplayName();
    auto receiverName
        = Manager::instance().getAccount<JamiAccount>(receivingAccountID)->getDisplayName();
    switch (event) {
    case ConversationEvent::ADD_MEMBER:
        JAMI_LOG("EVENT: {} added account {} to the conversation", instigatorName, receiverName);
        break;
    case ConversationEvent::SEND_MESSAGE:
        JAMI_LOG("EVENT: {} sent a message to the conversation", instigatorName);
        break;
    case ConversationEvent::CONNECT:
        JAMI_LOG("EVENT: {} connected to the conversation", instigatorName);
        break;
    case ConversationEvent::DISCONNECT:
        JAMI_LOG("EVENT: {} disconnected from the conversation", instigatorName);
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

void
ConversationRepositoryDST::generateEventSequence()
{
    // Generate the random number based on the seed from eventSeed
    std::mt19937_64 gen(eventSeed);

    // Generate the number of events we want to occur in the simulated conversation
    while (events.size() < maxEvents) {
        // We need an existing repository firstly
        if (!repository) {
            JAMI_LOG("No initial repository, creating one...");
            std::string& instigatorAccountID = accountIDs[accountDist(gen)];
            auto account = Manager::instance().getAccount<JamiAccount>(instigatorAccountID);
            repository = ConversationRepository::createConversation(account);

            // Manually add event rather than calling triggerAction()
            events.push_back(Event(account->getAccountID(),
                                   account->getAccountID(),
                                   ConversationEvent::ADD_MEMBER,
                                   std::chrono::system_clock::now().time_since_epoch().count()));
            logger(ConversationEvent::ADD_MEMBER, instigatorAccountID, instigatorAccountID);
            continue;
        }
        // Select a random event from the pool to attempt to perform
        ConversationEvent generatedEvent = static_cast<ConversationEvent>(
            conversationEventDist(gen));
        // Select a random account to perform the action
        std::string instigatorAccountID = accountIDs[accountDist(gen)];
        // Select a random account that will need to simulate actions based on the instigator's
        // action Validate the rule prior to accepting it as an event. The account may or may not
        // perform actions based on the generatedEvent type.
        std::string receivingAccountID = accountIDs[accountDist(gen)];

        if (validateActionRule(generatedEvent, instigatorAccountID, receivingAccountID)) {
            events.push_back(Event(instigatorAccountID,
                                   receivingAccountID,
                                   static_cast<ConversationEvent>(generatedEvent),
                                   std::chrono::system_clock::now().time_since_epoch().count()));
            logger(generatedEvent, instigatorAccountID, receivingAccountID);
            triggerAction(generatedEvent, instigatorAccountID, receivingAccountID);
        }
    }

    // Temp, for logging
    JAMI_LOG("=========== INFO ============");
    JAMI_LOG("SEED: {}", eventSeed);
    JAMI_LOG("Generated {} events for {} accounts", maxEvents, numAccounts);
    JAMI_LOG("========= SEQUENCE ==========");
    for (const auto& e : events) {
        logger(e.eventType, e.instigatorAccountID, e.receivingAccountID);
    }
    JAMI_LOG("=============================");
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConversationRepositoryDST::name())