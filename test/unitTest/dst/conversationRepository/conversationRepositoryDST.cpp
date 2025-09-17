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
#include <chrono>

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
// Vector with random silly messages
const std::vector<std::string> messages
    = {"{\"body\":\"salut\",\"type\":\"text/plain\"}",
       "{\"body\":\"ça va?\",\"type\":\"text/plain\"}",
       "{\"body\":\"quoi de neuf?\",\"type\":\"text/plain\"}",
       "{\"body\":\"je suis là!\",\"type\":\"text/plain\"}",
       "{\"body\":\"à plus!\",\"type\":\"text/plain\"}",
       "{\"body\":\"bye!\",\"type\":\"text/plain\"}",
       "{\"body\":\"à toute!\",\"type\":\"text/plain\"}",
       "{\"body\":\"à demain!\",\"type\":\"text/plain\"}",
       "{\"body\":\"bonne nuit!\",\"type\":\"text/plain\"}",
       "{\"body\":\"bonne journée!\",\"type\":\"text/plain\"}",
       "{\"body\":\"bon week-end!\",\"type\":\"text/plain\"}",
       "{\"body\":\"bon courage!\",\"type\":\"text/plain\"}",
       "{\"body\":\"bon app!\",\"type\":\"text/plain\"}",
       "{\"body\":\"je reviens dans 5 min\",\"type\":\"text/plain\"}",
       "{\"body\":\"tu as vu le dernier film?\",\"type\":\"text/plain\"}",
       "{\"body\":\"on se retrouve où?\",\"type\":\"text/plain\"}",
       "{\"body\":\"j’ai une bonne nouvelle!\",\"type\":\"text/plain\"}",
       "{\"body\":\"félicitations!\",\"type\":\"text/plain\"}",
       "{\"body\":\"désolé pour le retard\",\"type\":\"text/plain\"}",
       "{\"body\":\"c’est parti!\",\"type\":\"text/plain\"}",
       "{\"body\":\"j’ai faim\",\"type\":\"text/plain\"}",
       "{\"body\":\"tu veux un café?\",\"type\":\"text/plain\"}",
       "{\"body\":\"il fait beau aujourd’hui\",\"type\":\"text/plain\"}",
       "{\"body\":\"bonne chance!\",\"type\":\"text/plain\"}",
       "{\"body\":\"on se voit ce soir?\",\"type\":\"text/plain\"}",
       "{\"body\":\"merci beaucoup!\",\"type\":\"text/plain\"}",
       "{\"body\":\"je suis d’accord\",\"type\":\"text/plain\"}",
       "{\"body\":\"pas de souci\",\"type\":\"text/plain\"}",
       "{\"body\":\"à bientôt!\",\"type\":\"text/plain\"}",
       "{\"body\":\"super idée!\",\"type\":\"text/plain\"}"};

// Events to select from randomly
enum class ConversationEvent { ADD_MEMBER = 0, SEND_MESSAGE = 1, CONNECT = 2, DISCONNECT = 3 };

// Define a map for the event and its string name (for logging purposes)
const std::map<ConversationEvent, std::string> eventNames {
    {ConversationEvent::ADD_MEMBER, "ADD_MEMBER"},
    {ConversationEvent::SEND_MESSAGE, "SEND_MESSAGE"},
    {ConversationEvent::CONNECT, "CONNECT"},
    {ConversationEvent::DISCONNECT, "DISCONNECT"},
};

enum class GitOperation { PULL = 4, JOIN = 5, CLONE = 6 };

const std::map<GitOperation, std::string> gitOperationNames {
    {GitOperation::PULL, "PULL"},
    {GitOperation::JOIN, "JOIN"},
    {GitOperation::CLONE, "CLONE"},
};

// Account structure, for easy access of data. Meant ot avoid calls to the instance manager
struct RepositoryAccount
{
    std::shared_ptr<JamiAccount> account;
    std::unique_ptr<ConversationRepository> repository;
    bool connected {false};
    bool deviceAnnounced {false};
    std::string lastOnlineCommit {""};

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
    int type;
    std::chrono::nanoseconds timeOfOccurrence;

    // For construction of an event struct
    Event(int instigatorAccountIndex,
          int receivingAccountIndex,
          int type,
          std::chrono::nanoseconds timeOfOccurrence)
        : instigatorAccountIndex(instigatorAccountIndex)
        , receivingAccountIndex(receivingAccountIndex)
        , type(type)
        , timeOfOccurrence(timeOfOccurrence)
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
    void triggerEvent(const Event& event);
    void scheduleGitEvent(const GitOperation gitOperation,
                          int instigatorAccountIndex,
                          int receivingAccountIndex,
                          std::chrono::nanoseconds eventTimeOfOccurrence);
    void triggerGitEvent(const Event& event);
    // // Rules
    bool validateEvent(const Event& event);
    bool validateGitOperation(const Event& event);

    // Logging
    void eventLogger(const Event& event);
    void gitLogger(const Event& event);

    // Unit test functions
    void generateEventSequence();

    // Unit test suite
    CPPUNIT_TEST_SUITE(ConversationRepositoryDST);
    CPPUNIT_TEST(generateEventSequence);
    CPPUNIT_TEST_SUITE_END();

private:
    // Accounts
    std::vector<RepositoryAccount> repositoryAccounts;
    int numAccounts = 3;
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
    int maxEvents = 200;
    std::vector<Event> unvalidatedEvents;
    std::uniform_int_distribution<> repositoryEventDist {0, 3}; // 4 events
    int invalidEventsCount = 0;
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
    unvalidatedEvents.reserve(maxEvents);
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
        // JAMI_WARNING("Repository not initialized yet!");
        return false;
    }

    // Check that it has members
    if (repositoryAccounts[accountIndex].repository->members().empty()) {
        // JAMI_WARNING("No members in repository yet!");
        return false;
    }
    return true;
}

bool
ConversationRepositoryDST::validateEvent(const Event& event)
{
    // Define rules for actions here (i.e. can this action be performed given the current state of
    // the simulation)
    bool instigatorInConv = isUserInRepo(event.instigatorAccountIndex);
    bool receiverInConv = isUserInRepo(event.receivingAccountIndex);

    switch (static_cast<ConversationEvent>(event.type)) {
    case ConversationEvent::CONNECT:
        // User can only be connected if already disconnected
        if (!instigatorInConv || repositoryAccounts[event.instigatorAccountIndex].connected) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::DISCONNECT:
        // User can only be disconnected if already connected
        if (!instigatorInConv || !repositoryAccounts[event.instigatorAccountIndex].connected) {
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
        if (event.instigatorAccountIndex == event.receivingAccountIndex
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
        return false;
        break;
    }
    return true;
}

void
ConversationRepositoryDST::triggerEvent(const Event& event)
{
    auto instigatorAccount = repositoryAccounts[event.instigatorAccountIndex].account;
    auto receivingAccount = repositoryAccounts[event.receivingAccountIndex].account;

    // Perform the necessary action based on the event type
    switch (static_cast<ConversationEvent>(event.type)) {
    case ConversationEvent::CONNECT:
        // Enable the account
        repositoryAccounts[event.instigatorAccountIndex].connected = true;
        break;
    case ConversationEvent::DISCONNECT:
        // Disable the account
        repositoryAccounts[event.instigatorAccountIndex].connected = false;
        break;
    case ConversationEvent::ADD_MEMBER:
        // Add the account as a member within the context of the instigator's repository
        repositoryAccounts[event.instigatorAccountIndex].repository->addMember(
            std::string(receivingAccount->currentDeviceId()));
        scheduleGitEvent(GitOperation::CLONE,
                         event.instigatorAccountIndex,
                         event.receivingAccountIndex,
                         event.timeOfOccurrence);
        break;
    case ConversationEvent::SEND_MESSAGE:
        // pick a random message to send (dont care to seed this, message content shouldnt matter)
        repositoryAccounts[event.instigatorAccountIndex]
            .repository->commitMessage(messages[std::rand() % messages.size()], true);
        scheduleGitEvent(GitOperation::PULL,
                         event.instigatorAccountIndex,
                         event.receivingAccountIndex,
                         event.timeOfOccurrence);
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

void
ConversationRepositoryDST::scheduleGitEvent(const GitOperation gitOperation,
                                            int instigatorAccountIndex,
                                            int receivingAccountIndex,
                                            std::chrono::nanoseconds eventTimeOfOccurrence)
{
    // Seed
    std::mt19937_64 gen(eventSeed);

    // Get an average time between events
    std::chrono::nanoseconds averageTimeBetweenEvents
        = (unvalidatedEvents.back().timeOfOccurrence - unvalidatedEvents.front().timeOfOccurrence)
          / unvalidatedEvents.size();

    // Schedule the git event to occur after a random time interval
    std::uniform_int_distribution<> timeDist(1, unvalidatedEvents.size() / 2);

    // Different account indices indicate a git operation concerning two different accounts
    std::chrono::nanoseconds randomTimeInterval;
    if (instigatorAccountIndex != receivingAccountIndex) {
        randomTimeInterval = timeDist(gen) * averageTimeBetweenEvents;
        // Insert the event into unvalidatedEvents in a sorted manner
        auto it = std::lower_bound(unvalidatedEvents.begin(),
                                   unvalidatedEvents.end(),
                                   eventTimeOfOccurrence + randomTimeInterval,
                                   [](const Event& a, const std::chrono::nanoseconds& t) {
                                       return a.timeOfOccurrence < t;
                                   });
        unvalidatedEvents.insert(it,
                                 Event(instigatorAccountIndex,
                                       receivingAccountIndex,
                                       static_cast<int>(gitOperation),
                                       eventTimeOfOccurrence + randomTimeInterval));
        JAMI_LOG("Scheduled git event {} from {} to {} at time {}",
                 gitOperationNames.at(gitOperation),
                 repositoryAccounts[instigatorAccountIndex].account->getDisplayName(),
                 repositoryAccounts[receivingAccountIndex].account->getDisplayName(),
                 (eventTimeOfOccurrence + randomTimeInterval).count());
    } else {
        // If the account indices are the same, it implies that every account is to be involved
        for (int i = 0; i < repositoryAccounts.size(); ++i) {
            if (repositoryAccounts[i].repository != nullptr && i != instigatorAccountIndex) {
                randomTimeInterval = timeDist(gen) * averageTimeBetweenEvents;
                // Insert the event into unvalidatedEvents in a sorted manner
                auto it = std::lower_bound(unvalidatedEvents.begin(),
                                           unvalidatedEvents.end(),
                                           eventTimeOfOccurrence + randomTimeInterval,
                                           [](const Event& a, const std::chrono::nanoseconds& t) {
                                               return a.timeOfOccurrence < t;
                                           });
                unvalidatedEvents.insert(it,
                                         Event(instigatorAccountIndex,
                                               i,
                                               static_cast<int>(gitOperation),
                                               eventTimeOfOccurrence + randomTimeInterval));
                JAMI_LOG("Scheduled git event {} from {} to {} at time {}",
                         gitOperationNames.at(gitOperation),
                         repositoryAccounts[instigatorAccountIndex].account->getDisplayName(),
                         repositoryAccounts[i].account->getDisplayName(),
                         (eventTimeOfOccurrence + randomTimeInterval).count());
            }
        }
    }
}

bool
ConversationRepositoryDST::validateGitOperation(const Event& event)
{
    switch(static_cast<GitOperation>(event.type)) {
        case GitOperation::CLONE:
            // The instigator (i.e. the one to clone from) should already have a repository
            if (!repositoryAccounts[event.instigatorAccountIndex].repository)
                return false;
            break;
        case GitOperation::PULL:
            // Both the instigator and receiver should be part of the conversation
            if (!repositoryAccounts[event.instigatorAccountIndex].repository
                || !repositoryAccounts[event.receivingAccountIndex].repository)
                return false;
            break;
        case GitOperation::JOIN:
        default:
            JAMI_WARNING("Unknown GitOperation type received, this is a bug! No action has been "
                         "triggered.");
            return false;
            break;
    }
    return true;
}

void
ConversationRepositoryDST::triggerGitEvent(const Event& event)
{
    switch (static_cast<GitOperation>(event.type)) {
    case GitOperation::CLONE:
        repositoryAccounts[event.receivingAccountIndex].repository
            = ConversationRepository::cloneConversation(
                repositoryAccounts[event.receivingAccountIndex].account,
                std::string(repositoryAccounts[event.instigatorAccountIndex]
                                .account->getAccountID()), // Hack atm, not actually the device ID
                repositoryAccounts[event.instigatorAccountIndex].repository->id());
        // Join right after clone (might want to separate this from CLONE later)
        repositoryAccounts[event.receivingAccountIndex].repository->join();
        break;
    case GitOperation::JOIN:
        // Make the recepient join the conversation
        repositoryAccounts[event.receivingAccountIndex].repository->join();
        break;
    case GitOperation::PULL:
        // CONSIDER FOR FURTHER IMPL: Pull from any random member who has already received the
        // message (might need more state tracking possibly increasing complexity)

        repositoryAccounts[event.receivingAccountIndex].repository->pull(
            std::string(repositoryAccounts[event.instigatorAccountIndex].account->getAccountID()),
            repositoryAccounts[event.instigatorAccountIndex].repository->getHead(),
            repositoryAccounts[event.receivingAccountIndex].repository->getHead(),
            [&](bool) {},
            [&](const std::string&) {});
        break;
    default:
        JAMI_WARNING("Unknown GitOperation type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

void
ConversationRepositoryDST::eventLogger(const Event& event)
{
    auto instigatorName = repositoryAccounts[event.instigatorAccountIndex].account->getDisplayName();
    auto receiverName = repositoryAccounts[event.receivingAccountIndex].account->getDisplayName();

    switch (static_cast<ConversationEvent>(event.type)) {
    case ConversationEvent::ADD_MEMBER:
        JAMI_LOG("EVENT: {} added account {} to the conversation at {}",
                 instigatorName,
                 receiverName,
                 event.timeOfOccurrence.count());
        break;
    case ConversationEvent::SEND_MESSAGE:
        JAMI_LOG("EVENT: {} sent a message to the conversation at {}",
                 instigatorName,
                 event.timeOfOccurrence.count());
        break;
    case ConversationEvent::CONNECT:
        JAMI_LOG("EVENT: {} connected to the conversation at {}",
                 instigatorName,
                 event.timeOfOccurrence.count());
        break;
    case ConversationEvent::DISCONNECT:
        JAMI_LOG("EVENT: {} disconnected from the conversation at {}",
                 instigatorName,
                 event.timeOfOccurrence.count());
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

void
ConversationRepositoryDST::gitLogger(const Event& event)
{
    auto instigatorName = repositoryAccounts[event.instigatorAccountIndex].account->getDisplayName();
    auto receiverName = repositoryAccounts[event.receivingAccountIndex].account->getDisplayName();

    switch (static_cast<GitOperation>(event.type)) {
    case GitOperation::CLONE:
        JAMI_LOG("EVENT: {} cloned the conversation from {} at {}",
                 receiverName,
                 instigatorName,
                 event.timeOfOccurrence.count());
        break;
    case GitOperation::JOIN:
        JAMI_LOG("EVENT: {} joined the conversation at {}",
                 receiverName,
                 event.timeOfOccurrence.count());
        break;
    case GitOperation::PULL:
        JAMI_LOG("EVENT: {} pulled the conversation from {} at {}",
                 receiverName,
                 instigatorName,
                 event.timeOfOccurrence.count());
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

    // Indices to be used throughout interation
    int instigatorAccountIndex, receivingAccountIndex;

    // Intialize a repository randomly
    JAMI_LOG("Creating intiial repsitory...");

    // Pick a random account to intitalize
    instigatorAccountIndex = accountDist(gen);
    // Get the instigator's account
    auto& instigatorAccount = repositoryAccounts[instigatorAccountIndex].account;
    // Create the initial conversation
    repositoryAccounts[instigatorAccountIndex].repository
        = jami::ConversationRepository::createConversation(instigatorAccount);

    // Add the initial event
    unvalidatedEvents.emplace_back(Event(instigatorAccountIndex,
                                         instigatorAccountIndex,
                                         static_cast<int>(ConversationEvent::ADD_MEMBER),
                                         std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())));
    eventLogger(unvalidatedEvents.back());

    // Generate the number of events we want to occur in the simulated conversation
    while (unvalidatedEvents.size() < maxEvents) {
        // Select a random event from the pool of events
        int generatedEvent = repositoryEventDist(gen);

        // Select an account (instigator) to perform an event on another account (receiver)
        instigatorAccountIndex = accountDist(gen);
        receivingAccountIndex = accountDist(gen);

        // Add the event
        unvalidatedEvents.emplace_back(
            Event(instigatorAccountIndex,
                  receivingAccountIndex,
                  generatedEvent,
                  std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now().time_since_epoch())));
        eventLogger(unvalidatedEvents.back());
    }

    // We now validate these events
    std::vector<Event> validatedEvents;
    // We want to skip the first event, we already know it's the initial repositoryAccount being added
    JAMI_LOG("================= Unvalidated Events ================");
    for (int i = 1; i < maxEvents; i++) {
        if (unvalidatedEvents[i].type <= 3 && unvalidatedEvents[i].type >= 0) {
            // Repository-related events are between 0 and 3
            if (validateEvent(unvalidatedEvents[i])) {
                validatedEvents.emplace_back(unvalidatedEvents[i]);
                triggerEvent(validatedEvents.back());
                eventLogger(validatedEvents.back());
            }
        } else if (unvalidatedEvents[i].type >= 4 && unvalidatedEvents[i].type <= 6) {
            // Git operations are between 4 and 6
            if (validateGitOperation(unvalidatedEvents[i])) {
                validatedEvents.emplace_back(unvalidatedEvents[i]);
                triggerGitEvent(validatedEvents.back());
                gitLogger(validatedEvents.back());
            }
        } else {
            JAMI_WARNING(
                "Unknown event type found during validation, this is a bug! Event has been "
                "skipped.");
        }
    }

    JAMI_LOG("================= Validated Events ================");
    for (const auto& e : validatedEvents) {
        if (e.type <= 3 && e.type >= 0) {
            eventLogger(e);
        } else if (e.type >= 4 && e.type <= 6) {
            gitLogger(e);
        }
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConversationRepositoryDST::name())