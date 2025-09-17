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
#include <filesystem>

// JamiDHT
#include "jamidht/jamiaccount.h"
#include "jamidht/conversation.h"

// Commons
#include "manager.h"
#include "../../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "fileutils.h"

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
enum class ConversationEvent {
    ADD_MEMBER = 0,
    SEND_MESSAGE = 1,
    CONNECT = 2,
    DISCONNECT = 3,
    PULL = 4,
    CLONE = 5
};

// Define a map for the event and its string name (for logging purposes)
std::map<ConversationEvent, std::string> eventNames {{ConversationEvent::ADD_MEMBER, "ADD_MEMBER"},
                                                     {ConversationEvent::SEND_MESSAGE,
                                                      "SEND_MESSAGE"},
                                                     {ConversationEvent::CONNECT, "CONNECT"},
                                                     {ConversationEvent::DISCONNECT, "DISCONNECT"},
                                                     {ConversationEvent::PULL, "PULL"},
                                                     {ConversationEvent::CLONE, "CLONE"}};

// Account structure, for easy access of data. Meant to avoid calls to the instance manager
struct RepositoryAccount
{
    std::shared_ptr<JamiAccount> account;
    std::unique_ptr<ConversationRepository> repository;
    bool connected {false};
    bool deviceAnnounced {false};
    std::vector<libjami::SwarmMessage> messages;

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

    // Setup and teardown functions
    void connectSignals();
    void setUp();
    void tearDown();

    // Logging
    void eventLogger(const Event& event);
    void displaySummary();
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

    // Test runner dependencies
    libjami::Account::MessageStates getMessageStatus(
        const std::vector<libjami::SwarmMessage>& messages,
        const std::string& id,
        const std::string& peer);
    bool verifyLoadConversationFromScratch();
    void generateEventSequence();
    void resetRepositories();

    // Test runner
    void runCycles();

    // Unit test suite
    CPPUNIT_TEST_SUITE(ConversationRepositoryDST);
    CPPUNIT_TEST(runCycles);
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

    // Signal handlers - must be persistent
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool swarmLoaded = false;
    // Random number generation
    std::random_device rd;
    std::random_device::result_type eventSeed;

    // Events
    int numEventsToGenerate = 200;
    std::vector<Event> unvalidatedEvents;
    std::vector<Event> validatedEvents;
    std::uniform_int_distribution<> repositoryEventDist {0, 3}; // 4 events
    int invalidEventsCount = 0;
    std::chrono::nanoseconds startTime {0};
    std::mt19937_64 messageGen;

    std::vector<std::random_device::result_type> badSeeds {};
    std::vector<std::random_device::result_type> seedsTested {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryDST, ConversationRepositoryDST::name());

void
ConversationRepositoryDST::connectSignals()
{
    JAMI_LOG("=== CONNECTING SIGNALS ===");
    confHandlers.clear();

    // For account announcement in setUp()
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>&) {
                for (auto& repoAcc : repositoryAccounts) {
                    auto repositoryAccountID = repoAcc.account->getAccountID();
                    if (accountId == repositoryAccountID) {
                        auto details = repoAcc.account->getVolatileAccountDetails();
                        auto deviceAnnounced
                            = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                        repoAcc.deviceAnnounced = deviceAnnounced == "true";
                        break;
                    }
                }
                cv.notify_one();
            }));

    // For loadConversation() in verifyLoadConversationFromScratch()
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmLoaded>(
        [&](uint32_t requestId,
            const std::string& accountId,
            const std::string& conversationId,
            std::vector<libjami::SwarmMessage> messages) {
            bool accountFound = false;
            for (RepositoryAccount& repositoryAccount : repositoryAccounts) {
                auto accountIDToCheck = repositoryAccount.account->getAccountID();
                if (accountIDToCheck == accountId) {
                    accountFound = true;
                    repositoryAccount.messages.insert(repositoryAccount.messages.end(),
                                                      messages.begin(),
                                                      messages.end());
                    swarmLoaded = true;
                    break;
                }
            }
            if (!accountFound) {
                JAMI_LOG("WARNING: Account {} not found in repositoryAccounts!", accountId);
            }
            cv.notify_one();
        }));
    JAMI_LOG("Registering {} signal handlers...", confHandlers.size());
    libjami::registerSignalHandlers(confHandlers);
    JAMI_LOG("=== SIGNAL HANDLERS REGISTERED ===");
}

void
ConversationRepositoryDST::setUp()
{
    connectSignals();

    messageGen.seed(eventSeed);

    // Reserve space for account IDs
    repositoryAccounts.reserve(displayNames.size());

    // Add the max number of simulatable accounts
    for (int i = 0; i < displayNames.size(); ++i) {
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

    // Reserve space for max number of events
    unvalidatedEvents.reserve(numEventsToGenerate);
}

void
ConversationRepositoryDST::tearDown()
{
    resetRepositories();
    JAMI_LOG("Tearing down ConversationRepositoryDST...");
}

void
ConversationRepositoryDST::eventLogger(const Event& event)
{
    auto instigatorName = repositoryAccounts[event.instigatorAccountIndex].account->getDisplayName();
    auto receiverName = repositoryAccounts[event.receivingAccountIndex].account->getDisplayName();
    auto eventTime = event.timeOfOccurrence.count();

    switch (event.type) {
    case ConversationEvent::ADD_MEMBER:
        JAMI_LOG("EVENT: {} added account {} to the conversation at {}",
                 instigatorName,
                 receiverName,
                 eventTime);
        break;
    case ConversationEvent::SEND_MESSAGE:
        JAMI_LOG("EVENT: {} sent a message to the conversation at {}", instigatorName, eventTime);
        break;
    case ConversationEvent::CONNECT:
        JAMI_LOG("EVENT: {} connected to the conversation at {}", instigatorName, eventTime);
        break;
    case ConversationEvent::DISCONNECT:
        JAMI_LOG("EVENT: {} disconnected from the conversation at {}", instigatorName, eventTime);
        break;
    case ConversationEvent::CLONE:
        JAMI_LOG("GIT OPERATION: {} cloned the conversation from {} at {}",
                 receiverName,
                 instigatorName,
                 eventTime);
        break;
    case ConversationEvent::PULL:
        JAMI_LOG("GIT OPERATION: {} pulled the conversation from {} at {}",
                 receiverName,
                 instigatorName,
                 eventTime);
        break;
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

void
ConversationRepositoryDST::displaySummary()
{
    JAMI_LOG("=========================== Summary ==========================");
    JAMI_LOG("Random seed used: {}", eventSeed);
    JAMI_LOG("Total number of accounts simulated: {}", numAccounts);
    JAMI_LOG("Total events generated: {}", unvalidatedEvents.size());
    JAMI_LOG("Total valid events: {}", validatedEvents.size());
    JAMI_LOG("Total invalid events: {}", invalidEventsCount);
    JAMI_LOG("Rejection rate: {}%",
             (static_cast<float>(invalidEventsCount) / unvalidatedEvents.size()) * 100);
    JAMI_LOG("====================== Validated Events ======================");
    for (const auto& e : validatedEvents) {
        eventLogger(e);
    }
}

void
ConversationRepositoryDST::displayGitLog()
{
    JAMI_LOG("======================= Git Logs ============================");
    for (const RepositoryAccount& repositoryAccount : repositoryAccounts) {
        if (repositoryAccount.repository) {
            JAMI_LOG("Git log for account {}:", repositoryAccount.account->getDisplayName());
            auto log = repositoryAccount.repository->log(LogOptions {});
            for (const auto& entry : log) {
                JAMI_LOG("Message: {}, Author: {}, ID: {}",
                         entry.commit_msg,
                         entry.author.name,
                         entry.id);
            }
        }
    }
}

void
ConversationRepositoryDST::scheduleGitEvent(const ConversationEvent& gitOperation,
                                            int instigatorAccountIndex,
                                            int receivingAccountIndex,
                                            std::chrono::nanoseconds eventTimeOfOccurrence)
{
    // Seed
    std::mt19937_64 gen(eventSeed);

    std::chrono::nanoseconds adaptiveDelay(0);

    if (static_cast<int>(unvalidatedEvents.size()) >= 2) {
        auto recentTimeDiff = unvalidatedEvents.back().timeOfOccurrence
                              - unvalidatedEvents[unvalidatedEvents.size() - 2].timeOfOccurrence;
        if (recentTimeDiff > std::chrono::nanoseconds(0)) {
            std::uniform_int_distribution<> multiplierDist(1, 50);
            adaptiveDelay = recentTimeDiff * multiplierDist(gen);
        } else {
            adaptiveDelay = std::chrono::nanoseconds(
                unvalidatedEvents.back().timeOfOccurrence.count() / 1000);
        }
    } else {
        adaptiveDelay = std::chrono::nanoseconds(eventTimeOfOccurrence.count() / 100);
    }

    auto scheduledTime = eventTimeOfOccurrence + adaptiveDelay;

    if (!unvalidatedEvents.empty() && scheduledTime <= unvalidatedEvents.back().timeOfOccurrence) {
        auto minIncrement = std::chrono::nanoseconds(
            unvalidatedEvents.back().timeOfOccurrence.count() / 10000);
        scheduledTime = unvalidatedEvents.back().timeOfOccurrence + minIncrement;
    }

    if (instigatorAccountIndex != receivingAccountIndex) {
        // Just push_back, do not insert in order
        unvalidatedEvents.push_back(
            Event(instigatorAccountIndex, receivingAccountIndex, gitOperation, scheduledTime));
    } else {
        for (int i = 0; i < static_cast<int>(repositoryAccounts.size()); ++i) {
            if (repositoryAccounts[i].repository != nullptr && i != instigatorAccountIndex) {
                std::uniform_int_distribution<> offsetPercentDist(1, 20);
                auto accountSpecificTime = scheduledTime
                                           + (adaptiveDelay * offsetPercentDist(gen) / 100);
                unvalidatedEvents.push_back(
                    Event(instigatorAccountIndex, i, gitOperation, accountSpecificTime));
            }
        }
    }
}

void
ConversationRepositoryDST::scheduleSideEffects(const Event& event)
{
    switch (event.type) {
    case ConversationEvent::ADD_MEMBER:
        scheduleGitEvent(ConversationEvent::CLONE,
                         event.instigatorAccountIndex,
                         event.receivingAccountIndex,
                         event.timeOfOccurrence);
        break;
    case ConversationEvent::SEND_MESSAGE:
        scheduleGitEvent(ConversationEvent::PULL,
                         event.instigatorAccountIndex,
                         event.receivingAccountIndex,
                         event.timeOfOccurrence);
        break;
    case ConversationEvent::CLONE:
        // Now we notify the others about the join
        scheduleGitEvent(
            ConversationEvent::PULL,
            event.receivingAccountIndex, // Same indicies so that everyone pulls from the joiner
            event.receivingAccountIndex,
            event.timeOfOccurrence);
        break;
    default:
        break;
    }
}

bool
ConversationRepositoryDST::isUserInRepo(int accountIndexToSearch, int accountIndexToFind)
{
    // Check that the repository exists
    if (!repositoryAccounts[accountIndexToSearch].repository)
        return false;

    // Check that the repository to search has members
    if (repositoryAccounts[accountIndexToSearch].repository->members().empty())
        return false;

    dht::InfoHash h(repositoryAccounts[accountIndexToFind].account->getUsername());
    const std::string& targetUri = h.toString();
    for (const auto& member : repositoryAccounts[accountIndexToSearch].repository->members()) {
        if (member.uri == targetUri) {
            return true;
        }
    }
    return false;
}

bool
ConversationRepositoryDST::validateEvent(const Event& event)
{
    // Define rules for actions here (i.e. can this action be performed given the current state of
    // the simulation)

    // Get the accounts (mainly to improve legibility)
    // References since we can't copy unique ptrs
    RepositoryAccount& instigatorRepoAcc = repositoryAccounts[event.instigatorAccountIndex];
    RepositoryAccount& receiverRepoAcc = repositoryAccounts[event.receivingAccountIndex];

    switch (event.type) {
    case ConversationEvent::CONNECT:
        // Instigator can only be connected if already disconnected
        if (!instigatorRepoAcc.repository || instigatorRepoAcc.connected) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::DISCONNECT:
        // Instigator can only be disconnected if already connected
        if (!instigatorRepoAcc.repository || !instigatorRepoAcc.connected) {
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
            || !instigatorRepoAcc.repository || receiverRepoAcc.repository
            || isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex)) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::SEND_MESSAGE:
        // Instigator can only send a message if part of the conversation
        if (!instigatorRepoAcc.repository) {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::CLONE:
        // The instigator should only be able to have the receiver clone from them if:
        // 1. The instigator and receiver are online
        // 2. The instigator is part of the conversation
        // 3. The receiver is not already part of the conversation
        // 4. The receiver is a member of the instigator's repository
        if (instigatorRepoAcc.repository && !receiverRepoAcc.repository
            && instigatorRepoAcc.connected && receiverRepoAcc.connected) {
            if (isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex))
                return true;
        } else {
            invalidEventsCount++;
            return false;
        }
        break;
    case ConversationEvent::PULL:
        // The instigator should only be able to have the receiver pull from them if:
        // 1. The instigator and receiver are online
        // 2. The instigator and receiver are part of the conversation
        // 3. The receiver is already a member of the instigator's repository
        if (instigatorRepoAcc.repository && receiverRepoAcc.repository
            && instigatorRepoAcc.connected && receiverRepoAcc.connected) {
            if (isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex))
                return true;
        } else {
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
    switch (event.type) {
    case ConversationEvent::CONNECT: {
        // Enable the account
        repositoryAccounts[event.instigatorAccountIndex].connected = true;

        // We can make an assumption that the instigator will pull from a user once they've
        // connected to the repository and that the user they are pulling from is online
        scheduleGitEvent(ConversationEvent::PULL,
                         event.instigatorAccountIndex,
                         event.receivingAccountIndex,
                         event.timeOfOccurrence);
        break;
    }
    case ConversationEvent::DISCONNECT: {
        // Disable the account
        repositoryAccounts[event.instigatorAccountIndex].connected = false;
        break;
    }
    case ConversationEvent::ADD_MEMBER: {
        // Add the account as a member within the context of the instigator's repository
        dht::InfoHash h(receivingAccount->getUsername());
        JAMI_LOG("{} adding device ID {} to conversation...",
                 instigatorAccount->getDisplayName(),
                 h.toString());
        repositoryAccounts[event.instigatorAccountIndex].repository->addMember(h.toString());
        break;
    }
    case ConversationEvent::SEND_MESSAGE: {
        // Pick a random message to send (message content does not affect determinsmism, using the
        // eventSeed is unecessary  )
        std::uniform_int_distribution<> messageDist(0, messages.size() - 1);
        repositoryAccounts[event.instigatorAccountIndex]
            .repository->commitMessage(messages[messageDist(messageGen)], true);
        break;
    }
    case ConversationEvent::CLONE: {
        repositoryAccounts[event.receivingAccountIndex].repository
            = ConversationRepository::cloneConversation(
                repositoryAccounts[event.receivingAccountIndex].account,
                std::string(
                    repositoryAccounts[event.instigatorAccountIndex].account->getAccountID()),
                repositoryAccounts[event.instigatorAccountIndex].repository->id());
        // Join right after clone (might want to separate this from CLONE later)
        repositoryAccounts[event.receivingAccountIndex].repository->join();
        break;
    }
    case ConversationEvent::PULL: {
        // CONSIDER FOR FURTHER IMPL: Pull from any random member who has already received the
        // message (might need more state tracking possibly increasing complexity)

        // Create a Conversation object for the receiving account
        std::string convID = repositoryAccounts[event.receivingAccountIndex].repository->id();
        std::shared_ptr<Conversation> conversation
            = std::make_shared<Conversation>(repositoryAccounts[event.receivingAccountIndex].account, convID);

        // Use Conversation's pull method instead of repository's pull
        conversation->pull(std::string(repositoryAccounts[event.instigatorAccountIndex].account->getAccountID()));
        break;
    }
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

libjami::Account::MessageStates
ConversationRepositoryDST::getMessageStatus(const std::vector<libjami::SwarmMessage>& messages,
                                            const std::string& id,
                                            const std::string& peer)
{
    bool found = false;
    for (const auto& message : messages) {
        if (message.id == id) {
            found = true;
            dht::InfoHash h(peer);
            return static_cast<libjami::Account::MessageStates>(message.status.at(h.toString()));
        }
    }
    if (!found)
        JAMI_LOG("Failed to find message with id {}", id);
    return libjami::Account::MessageStates::UNKNOWN;
}

bool
ConversationRepositoryDST::verifyLoadConversationFromScratch()
{
    // Check the message status of messages in the repository as seen from the users own perspective
    for (RepositoryAccount& repositoryAccount : repositoryAccounts) {
        // Check that the user was part of the conversation (i.e. did they ever have a repository
        // initialized)
        if (repositoryAccount.repository) {
            // Get the conversation ID
            std::string convID = repositoryAccount.repository->id();

            // Create a Conversation object to access addToHistory
            std::shared_ptr<Conversation> conversation
                = std::make_shared<Conversation>(repositoryAccount.account, convID);

            // Reset state for signal handling
            swarmLoaded = false;
            LogOptions options {};
            conversation->loadMessages2(
                [accountId = repositoryAccount.account->getAccountID(),
                 convID,
                 randomId = std::uniform_int_distribution<uint32_t> {1}(
                     repositoryAccount.account->rand)](auto&& messages) {
                    emitSignal<libjami::ConversationSignal::SwarmLoaded>(randomId,
                                                                         accountId,
                                                                         convID,
                                                                         messages);
                },
                options);

            // Wait for the swarmLoaded signal to be received
            if (!cv.wait_for(lk, 30s, [&] { return swarmLoaded; })) {
                JAMI_LOG("Timeout waiting for swarmLoaded signal for account {}'s repository {}!",
                         repositoryAccount.account->getAccountID(),
                         convID);
                return false;
            }


            if (repositoryAccount.messages.empty()) {
                JAMI_LOG("No messages loaded for repository account {}!",
                         repositoryAccount.account->getAccountID());
                return false;
            }

            // Reverse iterate through the messages as they are emplaced in a backwards manner
            for (auto it = std::rbegin(repositoryAccount.messages);
                 it != std::rend(repositoryAccount.messages);
                 ++it) {
                bool isDisplayed = getMessageStatus(repositoryAccount.messages,
                                                    it->id,
                                                    repositoryAccount.account->getUsername())
                                   == libjami::Account::MessageStates::DISPLAYED;
                if (isDisplayed) {
                    JAMI_LOG("Message with id {} is marked as DISPLAYED in account {}'s repository",
                             it->id,
                             repositoryAccount.account->getDisplayName());

                } else {
                    JAMI_LOG("Message with id {} is NOT marked as DISPLAYED in account {}'s "
                             "repository, this is a bug!",
                             it->id,
                             repositoryAccount.account->getDisplayName());
                }
            }
        }
    }
    return true;
}

void
ConversationRepositoryDST::generateEventSequence()
{
    // Generate the random number based on the seed from eventSeed
    std::mt19937_64 gen(eventSeed);

    // Indices to be used throughout iteration
    int instigatorAccountIndex, receivingAccountIndex;

    // Initialize a repository randomly
    JAMI_LOG("Creating initial repository...");

    // Pick a random account to initialize
    instigatorAccountIndex = accountDist(gen);
    // Get the instigator's account
    auto& instigatorAccount = repositoryAccounts[instigatorAccountIndex].account;
    // Create the initial conversation
    repositoryAccounts[instigatorAccountIndex].repository
        = jami::ConversationRepository::createConversation(instigatorAccount);

    // Add the initial event
    startTime = std::chrono::nanoseconds(0);

    unvalidatedEvents.emplace_back(Event(instigatorAccountIndex,
                                         instigatorAccountIndex,
                                         ConversationEvent::ADD_MEMBER,
                                         startTime));

    JAMI_LOG("===================== Unvalidated Events =====================");
    eventLogger(unvalidatedEvents.back());
    // Generate the number of events we want to occur in the simulated conversation
    while (static_cast<int>(unvalidatedEvents.size()) < numEventsToGenerate) {
        // Select a random event from the pool of events
        ConversationEvent generatedEvent = static_cast<ConversationEvent>(repositoryEventDist(gen));

        // Select an account (instigator) to perform an event on another account (receiver)
        instigatorAccountIndex = accountDist(gen);
        receivingAccountIndex = accountDist(gen);

        startTime += std::chrono::milliseconds(1000);

        // Add the event
        unvalidatedEvents.emplace_back(
            Event(instigatorAccountIndex, receivingAccountIndex, generatedEvent, startTime));
        // Schedule side effects if applicable
        scheduleSideEffects(unvalidatedEvents.back());

        // Log the event
        eventLogger(unvalidatedEvents.back());
    }

    // Sort all events by time before validation
    std::sort(unvalidatedEvents.begin(),
              unvalidatedEvents.end(),
              [](const Event& a, const Event& b) {
                  return a.timeOfOccurrence < b.timeOfOccurrence;
              });

    // We want to skip the first event, we already know it's the initial repositoryAccount being added
    validatedEvents.emplace_back(unvalidatedEvents.front());
    JAMI_LOG("===================== Validating Events... =====================");
    eventLogger(validatedEvents.front());
    // Iterate through all the events and not just i-many
    for (int i = 1; i < static_cast<int>(unvalidatedEvents.size()); i++) {
        // JAMI_LOG("Validating event {}", i);
        if (validateEvent(unvalidatedEvents[i])) {
            validatedEvents.emplace_back(unvalidatedEvents[i]);
            eventLogger(validatedEvents.back());
            triggerEvent(validatedEvents.back());
        }
    }
}

void
ConversationRepositoryDST::resetRepositories()
{
    std::vector<std::pair<std::string, std::string>> conversationPaths;

    // Collect conversation paths first
    for (RepositoryAccount& repoAcc : repositoryAccounts) {
        if (repoAcc.repository) {
            auto accountId = repoAcc.account->getAccountID();
            auto convId = repoAcc.repository->id();
            conversationPaths.push_back({accountId, convId});
        }
    }

    // Delete each existing repository for each account
    for (RepositoryAccount& repoAcc : repositoryAccounts) {
        if (repoAcc.repository) {
            auto convModule = repoAcc.account->convModule(false);
            auto convId = repoAcc.repository->id();

            // Reset the repository object first to release any locks
            repoAcc.repository.reset();

            // Remove the conversation
            if (convModule) {
                convModule->removeConversation(convId);
            }

            // Clear any leftover messages
            repoAcc.messages.clear();
        }
    }

    // Reset state variables for next cycle
    swarmLoaded = false;

    // Give a moment for normal cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Force delete any remaining conversation directories
    for (const auto& [accountId, convId] : conversationPaths) {
        auto convPath = fileutils::get_data_dir() / accountId / "conversations" / convId;
        if (std::filesystem::exists(convPath)) {
            JAMI_LOG("Force deleting conversation directory: {}", convPath.string());
            std::filesystem::remove_all(convPath);
        }
    }

    // Clear all events for next cycle
    unvalidatedEvents.clear();
    validatedEvents.clear();
    invalidEventsCount = 0;
}

void
ConversationRepositoryDST::runCycles()
{
    int numCycles = 1;
    for (int i = 0; i < numCycles; ++i) {
        // Generate the event sequence
        JAMI_LOG("Starting cycle {} of {}", i + 1, numCycles);
        // Generate the random seed to be used for event generation
        eventSeed = rd();
        JAMI_LOG("Random seed generated: {}", eventSeed);
        generateEventSequence();
        // Log the summary for all the generated events
        displaySummary();
        // Log all the commits in each respective repository
        displayGitLog();
        // Load all the messages from each repo from scratch and verify that they are marked as displayed
        if (!verifyLoadConversationFromScratch())
            badSeeds.push_back(eventSeed); // Failed signal loading, add to bad seed list
        seedsTested.push_back(eventSeed);
        // Clear out the repositories for reuse
        resetRepositories();
    }

    JAMI_LOG(" ===================== Seeds Tested ==========================");
    for (const auto& seed : seedsTested) {
        JAMI_LOG("{}", seed);
    }

    if (!badSeeds.empty()) {
        JAMI_LOG("=================== Bad Seeds ==========================");
        for (const auto& seed : badSeeds) {
            JAMI_LOG("{}", seed);
        }
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConversationRepositoryDST::name())