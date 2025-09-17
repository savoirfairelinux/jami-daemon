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

// Account structure, for easy access of data. Meant ot avoid calls to the instance manager
struct RepositoryAccount
{
    std::shared_ptr<JamiAccount> account;
    std::unique_ptr<ConversationRepository> repository;
    bool connected {false};
    bool deviceAnnounced {false};

    RepositoryAccount(std::shared_ptr<JamiAccount> acc,
                      std::unique_ptr<ConversationRepository> repo,
                      bool connected)
        : account(acc)
        , repository(std::move(repo))
        , connected(connected)
    {}
};

// Event structure, each having a type and a timestamp
struct Event
{
    int instigatorAccountIndex;
    int receivingAccountIndex;
    ConversationEvent eventType;
    uint64_t timeOfOccurrence;

    // For construction of an event struct
    Event(int instigatorAccountIndex,
          int receivingAccountIndex,
          ConversationEvent type,
          uint64_t timestamp)
        : instigatorAccountIndex(instigatorAccountIndex)
        , receivingAccountIndex(receivingAccountIndex)
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

    // For account initialization
    void connectSignals();

    void setUp();
    void tearDown();

    // Helper functions
    bool isUserInRepo(int instigatorAccountID);
    void triggerAction(const ConversationEvent event,
                       int instigatorAccountIndex,
                       int receivingAccountIndex);
    void generateSideEffects(const ConversationEvent event,
                             int instigatorAccountID,
                             int receivingAccountID);
    // void actionSideEffect(const ConversationEvent event, const std::string&);
    // Rules
    bool validateActionRule(const ConversationEvent action,
                            int instigatorAccountID,
                            int receivingAccountID);

    // Logging
    void logger(const ConversationEvent event,
                int instigatorAccountIndex,
                int receivingAccountIndex);

    // Unit test functions
    void generateEventSequence();

    // Unit test suite
    CPPUNIT_TEST_SUITE(ConversationRepositoryDST);
    CPPUNIT_TEST(generateEventSequence);
    CPPUNIT_TEST_SUITE_END();

private:
    // Accounts
    std::vector<RepositoryAccount> repositoryAccounts;
    int numAccounts = 4;
    std::uniform_int_distribution<> accountDist {0, numAccounts - 1};
    const std::vector<std::string> displayNames = {"ALICE", "BOB",    "CHARLIE", "DAVE",   "EMILY",
                                                   "FRANK", "GREG",   "HOLLY",   "IAN",    "JENNA",
                                                   "KEVIN", "LUCY",   "MIKE",    "NORA",   "OLIVIA",
                                                   "PETE",  "QUINN",  "RACHEL",  "SAM",    "TOM",
                                                   "UMA",   "VICTOR", "WENDY",   "XANDER", "YVONNE",
                                                   "ZOE"};
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    // Random number generation
    std::random_device rd;
    std::random_device::result_type eventSeed;
    std::random_device::result_type accountsSeed;

    // Events
    int maxEvents = 50;
    std::vector<Event> events;
    std::uniform_int_distribution<> conversationEventDist {0, 3}; // 4 events
    int invalidEventsCount = 0;

    // Define the necessary repo actions to occur based on the event type
    std::map<ConversationEvent, std::vector<RepoAction>>
        eventActionMap {{ConversationEvent::ADD_MEMBER, {RepoAction::CLONE, RepoAction::JOIN}},
                        {ConversationEvent::SEND_MESSAGE, {RepoAction::PULL}}};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryDST, ConversationRepositoryDST::name());

void
ConversationRepositoryDST::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>&) {
                for (auto& repoAcc : repositoryAccounts) {
                    auto repositoryAccountID = repoAcc.account->getAccountID();
                    if (accountId == repositoryAccountID) {
                        auto repositoryAccount = Manager::instance().getAccount<JamiAccount>(
                            repositoryAccountID);
                        auto details = repositoryAccount->getVolatileAccountDetails();
                        auto deviceAnnounced
                            = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                        repoAcc.deviceAnnounced = deviceAnnounced == "true";
                        break;
                    }
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
}

void
ConversationRepositoryDST::setUp()
{
    connectSignals();

    // Reserve space for account IDs
    repositoryAccounts.reserve(numAccounts);

    // Add only the max number of accounts to be simulated
    for (int i = 0; i < numAccounts; ++i) {
        // Configure the account
        auto accountDetails = libjami::getAccountTemplate("RING");
        accountDetails[ConfProperties::TYPE] = "RING";
        accountDetails[ConfProperties::DISPLAYNAME] = displayNames[i];
        accountDetails[ConfProperties::ALIAS] = displayNames[i];
        accountDetails[ConfProperties::UPNP_ENABLED] = "true";
        accountDetails[ConfProperties::ARCHIVE_PASSWORD] = "";

        // Add the account and store the returned account ID
        auto accountId = Manager::instance().addAccount(accountDetails);

        // Initialize the account details for later indexing
        auto account = Manager::instance().getAccount<JamiAccount>(accountId);
        repositoryAccounts.emplace_back(RepositoryAccount(account, nullptr, true));
    }

    // Need to wait for devices to be registered
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
        for (const auto& repoAcc : repositoryAccounts) {
            if (!repoAcc.deviceAnnounced)
                return false;
        }
        return true;
    }));

    // Generate the random seed to be used for event generation
    eventSeed = 301082494; // rd() <- for now reusing seed, but should be rd();
    JAMI_LOG("Random seed generated: {}", eventSeed);

    // Reserve space for max number of events
    events.reserve(maxEvents);
}

void
ConversationRepositoryDST::tearDown()
{
    JAMI_LOG("Tearing down ConversationRepositoryDST...");
}

bool
ConversationRepositoryDST::isUserInRepo(int accountIndex)
{
    // Check that the repository existst
    if (!repositoryAccounts[accountIndex].repository) {
        JAMI_WARNING("Repository not initialized yet!");
        return false;
    }

    // Check that it has members
    if (repositoryAccounts[accountIndex].repository->members().empty()) {
        JAMI_WARNING("No members in repository yet!");
        return false;
    }
    return true;
}

bool
ConversationRepositoryDST::validateActionRule(const ConversationEvent action,
                                              int instigatorAccountIndex,
                                              int receivingAccountIndex)
{
    // Define rules for actions here (i.e. can this action be performed given the current state of
    // the simulation)
    bool instigatorInConv = isUserInRepo(instigatorAccountIndex);
    bool receiverInConv = isUserInRepo(receivingAccountIndex);

    switch (action) {
    case ConversationEvent::CONNECT:
        // User can only be connected if already disconnected
        if (!instigatorInConv || repositoryAccounts[instigatorAccountIndex].connected) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::DISCONNECT:
        // User can only be disconnected if already connected
        if (!instigatorInConv || !repositoryAccounts[instigatorAccountIndex].connected) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::ADD_MEMBER:
        // Note: this is within the context of the CURRENT repo
        // The insigator should only be able to add the receiver if:
        // 1. The instigator is not trying to add themselves
        // 2. The instigator is already part of of the conversation
        // 3. The receiver is not already part of the conversation
        if (repositoryAccounts[instigatorAccountIndex].account->getAccountID()
                == repositoryAccounts[receivingAccountIndex].account->getAccountID()
            || !instigatorInConv || receiverInConv) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::SEND_MESSAGE:
        // User can only send a message if part of conversation
        if (!instigatorInConv) {
            invalidEventsCount++;
            return false;
        }
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        invalidEventsCount++;
        break;
    }
    return true;
}

void
ConversationRepositoryDST::triggerAction(const ConversationEvent event,
                                         int instigatorAccountIndex,
                                         int receivingAccountIndex)
{
    auto instigatorAccount = repositoryAccounts[instigatorAccountIndex].account;
    auto receivingAccount = repositoryAccounts[receivingAccountIndex].account;

    // Perform the necessary action based on the event type
    switch (event) {
    case ConversationEvent::CONNECT:
        // Enable the account
        repositoryAccounts[instigatorAccountIndex].connected = true;
        break;
    case ConversationEvent::DISCONNECT:
        // Disable the account
        repositoryAccounts[instigatorAccountIndex].connected = false;
        break;
    case ConversationEvent::ADD_MEMBER:
        // Add the account as a member within the context of the instigator's repository
        repositoryAccounts[instigatorAccountIndex].repository->addMember(
            receivingAccount->getAccountID());
        generateSideEffects(event, instigatorAccountIndex, receivingAccountIndex);
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
                                               int instigatorAccountIndex,
                                               int receivingAccountIndex)
{
    auto repoActions = eventActionMap.at(event);

    for (const RepoAction repoAction : repoActions) {
        if (repoAction == RepoAction::CLONE) {
            JAMI_LOG("Cloning the conversation!");
            repositoryAccounts[receivingAccountIndex].repository
                = ConversationRepository::cloneConversation(
                    repositoryAccounts[receivingAccountIndex].account,
                    repositoryAccounts[instigatorAccountIndex].account->getAccountID(),
                    repositoryAccounts[instigatorAccountIndex].repository->id());
        }
    }

    JAMI_LOG("{} will make {} perform actions!",
             repositoryAccounts[instigatorAccountIndex].account->getDisplayName(),
             repositoryAccounts[receivingAccountIndex].account->getDisplayName());
}

void
ConversationRepositoryDST::logger(const ConversationEvent event,
                                  int instigatorAccountIndex,
                                  int receivingAccountIndex)
{
    auto instigatorName = repositoryAccounts[instigatorAccountIndex].account->getDisplayName();
    auto receiverName = repositoryAccounts[receivingAccountIndex].account->getDisplayName();

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
        // We need an existing repository firstly, so make sure that no accounts have a repository
        if (std::all_of(repositoryAccounts.begin(),
                        repositoryAccounts.end(),
                        [](const RepositoryAccount& repo) { return !repo.repository; })) {
            JAMI_LOG("No initial repository, creating one...");
            int instigatorAccountIndex = accountDist(gen);
            auto& instigatorAccount = repositoryAccounts[instigatorAccountIndex].account;
            repositoryAccounts[instigatorAccountIndex].repository
                = jami::ConversationRepository::createConversation(instigatorAccount);

            // Manually add event rather than calling triggerAction(), this is an exception
            events.push_back(Event(instigatorAccountIndex,
                                   instigatorAccountIndex,
                                   ConversationEvent::ADD_MEMBER,
                                   std::chrono::system_clock::now().time_since_epoch().count()));
            logger(ConversationEvent::ADD_MEMBER, instigatorAccountIndex, instigatorAccountIndex);
            continue;
        }
        // Select a random event from the pool to attempt to perform
        ConversationEvent generatedEvent = static_cast<ConversationEvent>(
            conversationEventDist(gen));
        // Select a random account to perform the action
        int instigatorAccountIndex = accountDist(gen);
        // Select a random account that will need to simulate actions based on the instigator's
        // action Validate the rule prior to accepting it as an event. The account may or may not
        // perform actions based on the generatedEvent type.
        int receivingAccountIndex = accountDist(gen);

        if (validateActionRule(generatedEvent, instigatorAccountIndex, receivingAccountIndex)) {
            events.push_back(Event(instigatorAccountIndex,
                                   receivingAccountIndex,
                                   static_cast<ConversationEvent>(generatedEvent),
                                   std::chrono::system_clock::now().time_since_epoch().count()));
            logger(generatedEvent, instigatorAccountIndex, receivingAccountIndex);
            triggerAction(generatedEvent, instigatorAccountIndex, receivingAccountIndex);
        }
    }

    // Temp, for logging
    JAMI_LOG("=========== INFO ============");
    JAMI_LOG("SEED: {}", eventSeed);
    JAMI_LOG("Generated {} events for {} accounts", maxEvents, numAccounts);
    JAMI_LOG("Percentage of invalid events attempted: {}%",
             (static_cast<float>(invalidEventsCount)
              / static_cast<float>(maxEvents + invalidEventsCount))
                 * 100.0f);
    JAMI_LOG("========= SEQUENCE ==========");
    for (const auto& e : events) {
        logger(e.eventType, e.instigatorAccountIndex, e.receivingAccountIndex);
    }
    JAMI_LOG("=============================");
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConversationRepositoryDST::name())