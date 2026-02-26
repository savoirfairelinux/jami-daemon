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

#include "account_const.h"
#include "fileutils.h"
#include "conversation_interface.h"
#include "configurationmanager_interface.h"
#include "jami.h"
#include "json_utils.h"
#include "manager.h"

#undef NDEBUG
#include <cassert>
#include <chrono>
#include <fmt/color.h>
#include <fmt/printf.h>

namespace jami {
namespace test {

// Define a map for the event and its string name (for logging purposes)
std::map<ConversationEvent, std::string> eventNames {{ConversationEvent::ADD_MEMBER, "ADD_MEMBER"},
                                                     {ConversationEvent::SEND_MESSAGE, "SEND_MESSAGE"},
                                                     {ConversationEvent::CONNECT, "CONNECT"},
                                                     {ConversationEvent::DISCONNECT, "DISCONNECT"},
                                                     {ConversationEvent::FETCH, "FETCH"},
                                                     {ConversationEvent::MERGE, "MERGE"},
                                                     {ConversationEvent::CLONE, "CLONE"}};
std::map<std::string, ConversationEvent> invertedEventNames {{"ADD_MEMBER", ConversationEvent::ADD_MEMBER},
                                                             {"SEND_MESSAGE", ConversationEvent::SEND_MESSAGE},
                                                             {"CONNECT", ConversationEvent::CONNECT},
                                                             {"DISCONNECT", ConversationEvent::DISCONNECT},
                                                             {"FETCH", ConversationEvent::FETCH},
                                                             {"MERGE", ConversationEvent::MERGE},
                                                             {"CLONE", ConversationEvent::CLONE}};

// Keys used in config.json
enum class UTKEY : std::uint8_t {
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

bool
ConversationDST::setUp(int numAccountsToSimulate)
{
    // Reserve space for account IDs
    repositoryAccounts.reserve(numAccountsToSimulate);
    // Get the number of existing accounts (in HOME/.local/share/jami/)
    auto existingAccounts = Manager::instance().getAllAccounts<JamiAccount>();
    const int numberOfExistingAccounts = static_cast<int>(existingAccounts.size());

    // Add the existing accounts to the repository accounts list
    for (const auto& existingAccount : existingAccounts) {
        repositoryAccounts.emplace_back(RepositoryAccount(existingAccount));
        repositoryAccounts.back().identityLoaded = true;
    }

    // Add the max number of simulatable accounts
    int numAccountsToAdd = std::max(numAccountsToSimulate - numberOfExistingAccounts, 0);
    for (int i = 0; i < numAccountsToAdd; ++i) {
        using namespace libjami::Account;
        // Configure the account
        auto accountDetails = libjami::getAccountTemplate("RING");
        accountDetails[ConfProperties::TYPE] = "RING";
        accountDetails[ConfProperties::DISPLAYNAME] = displayNames[i];
        accountDetails[ConfProperties::ALIAS] = displayNames[i];
        accountDetails[ConfProperties::UPNP_ENABLED] = "false";
        accountDetails[ConfProperties::ARCHIVE_PASSWORD] = "";

        // Add the account and store the returned account ID
        auto accountId = Manager::instance().addAccount(accountDetails);

        // Initialize the account details for later indexing
        auto account = Manager::instance().getAccount<JamiAccount>(accountId);
        repositoryAccounts.emplace_back(RepositoryAccount(account));
    }

    // Register signal handlers
    confHandlers.clear();
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onConversationReady(accountId, conversationId);
                    break;
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::RegistrationStateChanged>(
        [&](const std::string& accountId, const std::string& state, int, const std::string&) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.identityLoaded = state != "INITIALIZING";
                    break;
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId, const std::string& conversationId, const libjami::SwarmMessage& message) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onSwarmMessageReceived(accountId, conversationId, message);
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageUpdated>(
        [&](const std::string& accountId, const std::string& conversationId, const libjami::SwarmMessage& message) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onSwarmMessageUpdated(accountId, conversationId, message);
                }
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmLoaded>(
        [&](uint32_t requestId,
            const std::string& accountId,
            const std::string& conversationId,
            const std::vector<libjami::SwarmMessage>& messages) {
            for (auto& repoAcc : repositoryAccounts) {
                if (repoAcc.account->getAccountID() == accountId) {
                    repoAcc.client.onSwarmLoaded(requestId, accountId, conversationId, messages);
                    break;
                }
            }
            cv.notify_one();
        }));
    JAMI_LOG("Registering {} signal handlers...", confHandlers.size());
    libjami::registerSignalHandlers(confHandlers);

    // When creating a new account, its identity (key + certificate) is generated asynchronously.
    // We need to wait for the process to complete before attempting to create a new conversation
    // repository (see the add_initial_files function in conversationrepository.cpp).
    bool ok = cv.wait_for(lk, 30s, [&] {
        for (const auto& repoAcc : repositoryAccounts) {
            if (!repoAcc.identityLoaded)
                return false;
        }
        return true;
    });
    if (!ok)
        return false;

    // Sort accounts by display name. This isn't strictly necessary, but it ensures that
    // successive test runs with the same seed will have the same account order, which can
    // make the logs easier to compare during debugging.
    std::sort(repositoryAccounts.begin(),
              repositoryAccounts.end(),
              [](const RepositoryAccount& a, const RepositoryAccount& b) {
                  return a.account->getDisplayName() < b.account->getDisplayName();
              });

    int accountIndex = 0;
    for (RepositoryAccount& repositoryAccount : repositoryAccounts) {
        auto path = fileutils::get_data_dir() / repositoryAccount.account->getAccountID();
        fmt::print("Account #{} [{:<7}]: {}\n", accountIndex++, repositoryAccount.account->getDisplayName(), path);
    }
    return true;
}

/**
 * Displays info about an event or git operation
 * @param event The event to be logged
 */
void
ConversationDST::eventLogger(const Event& event)
{
    if (enableEventLogging_) {
        auto instigatorName = repositoryAccounts[event.instigatorAccountIndex].account->getDisplayName();
        auto receiverName = repositoryAccounts[event.receivingAccountIndex].account->getDisplayName();
        auto eventTime = event.timeOfOccurrence.count();

        switch (event.type) {
        case ConversationEvent::ADD_MEMBER:
            fmt::print("EVENT: {} added account {} to the conversation at {}\n",
                       instigatorName,
                       receiverName,
                       eventTime);
            break;
        case ConversationEvent::SEND_MESSAGE:
            fmt::print("EVENT: {} sent a message to the conversation at {}\n", instigatorName, eventTime);
            break;
        case ConversationEvent::CONNECT:
            fmt::print("EVENT: {} connected to the conversation at {}\n", instigatorName, eventTime);
            break;
        case ConversationEvent::DISCONNECT:
            fmt::print("EVENT: {} disconnected from the conversation at {}\n", instigatorName, eventTime);
            break;
        case ConversationEvent::CLONE:
            fmt::print("GIT OPERATION: {} cloned the conversation from {} at {}\n",
                       receiverName,
                       instigatorName,
                       eventTime);
            break;
        case ConversationEvent::FETCH:
            fmt::print("GIT OPERATION: {} fetched commits from {} at {}\n", receiverName, instigatorName, eventTime);
            break;
        case ConversationEvent::MERGE:
            fmt::print("GIT OPERATION: {} merged commits from {} at {}\n", receiverName, instigatorName, eventTime);
            break;

        default:
            assert(false && "Unknown ConversationEvent type received, this is a bug!");
            break;
        }
    }
}

/**
 * Display the git repository logs of each account that contains a repository. This is the
 * equivalent of running `git log` for each account that has a repository associated with it.
 */
void
ConversationDST::displayGitLog()
{
    if (enableGitLogging_) {
        std::map<std::string, std::string> nameFromURI;
        for (const auto& repoAcc : repositoryAccounts) {
            nameFromURI[repoAcc.account->getUsername()] = repoAcc.account->getDisplayName();
        }

        fmt::print("======================= Git Logs ============================\n");
        for (const RepositoryAccount& repositoryAccount : repositoryAccounts) {
            if (repositoryAccount.repository) {
                fmt::print("Git log for account {}:\n", repositoryAccount.account->getDisplayName());
                auto log = repositoryAccount.repository->log(LogOptions {});
                for (const auto& entry : log) {
                    fmt::print("  Message: {}, Author: {}, ID: {}\n", entry.commit_msg, entry.author.name, entry.id);
                }

                fmt::print("\nClient messages for account {}:\n", repositoryAccount.account->getDisplayName());
                for (const auto& message : repositoryAccount.client.getMessages()) {
                    std::string log = " ";
                    for (const auto& key : {"body", "action", "mode", "type", "uri"}) {
                        if (auto it = message.body.find(key); it != message.body.end()) {
                            log += fmt::format(" {}:{}", key, it->second);
                        }
                    }
                    fmt::print("{} author:{} id:{}\n",
                               log,
                               nameFromURI[message.body.at("author")],
                               message.body.at("id"));
                }
                fmt::print("\n");
            }
        }
    }
}

/**
 *
 * @param gitOperation The git operation to be scheduled
 * @param instigatorAccountIndex The index of the account that performs the git operation
 * @param receivingAccountIndex The index of the account that has the git operation performed on them.
 * @param eventTimeOfOccurrence The time of occurrence of the event that triggered the schedule
 *
 * @note Example: A pulls from B (where A is the receiver/puller, and B is the instigator/source)
 */
void
ConversationDST::scheduleGitEvent(EventQueue& queue,
                                  const ConversationEvent& gitOperation,
                                  int instigatorAccountIndex,
                                  int receivingAccountIndex,
                                  std::chrono::nanoseconds eventTimeOfOccurrence)
{
    assert(instigatorAccountIndex >= -1 && instigatorAccountIndex < numAccountsToSimulate_);
    assert(receivingAccountIndex >= -1 && receivingAccountIndex < numAccountsToSimulate_);

    std::uniform_int_distribution<> adaptiveDist(5, 50);

    if (receivingAccountIndex == -1) {
        assert(instigatorAccountIndex >= 0);
        // Schedule the Git event for each account to "receive"
        for (int i = 0; i < numAccountsToSimulate_; ++i) {
            if (repositoryAccounts[i].repository != nullptr && i != instigatorAccountIndex) {
                auto scheduledTime = eventTimeOfOccurrence + std::chrono::nanoseconds(adaptiveDist(gen_));
                queue.emplace(instigatorAccountIndex, i, gitOperation, scheduledTime);
            }
        }
    } else if (instigatorAccountIndex == -1) {
        assert(receivingAccountIndex >= 0);
        // Schedule the Git event for each account to "initiate"
        for (int i = 0; i < numAccountsToSimulate_; ++i) {
            if (repositoryAccounts[i].repository != nullptr && i != receivingAccountIndex) {
                auto scheduledTime = eventTimeOfOccurrence + std::chrono::nanoseconds(adaptiveDist(gen_));
                queue.emplace(i, receivingAccountIndex, gitOperation, scheduledTime);
            }
        }
    } else {
        // Schedule the Git event for the specific instigator and receiver
        auto scheduledTime = eventTimeOfOccurrence + std::chrono::nanoseconds(adaptiveDist(gen_));
        queue.emplace(instigatorAccountIndex, receivingAccountIndex, gitOperation, scheduledTime);
    }
}

/**
 * Checks whether or not a user is a member of a conversation
 * @param accountIndexToSearch
 * @param accountIndexToFind
 * @return bool Whether or not the user is a member of the conversation
 */
bool
ConversationDST::isUserInRepo(int accountIndexToSearch, int accountIndexToFind)
{
    // Check that the repository exists
    if (!repositoryAccounts[accountIndexToSearch].repository)
        return false;

    // Check whether the repository to search has members
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

/**
 * Validates a given event based on existing events and the current state of all accounts (i.e. can
 * this action be performed given the current state of the simulation)
 * @param event The event to be validated
 * @return bool Whether or not the event is valid
 */
bool
ConversationDST::validateEvent(const Event& event)
{
    // Get the accounts (mainly to improve legibility)
    // References since we can't copy unique ptrs
    RepositoryAccount& instigatorRepoAcc = repositoryAccounts[event.instigatorAccountIndex];
    RepositoryAccount& receiverRepoAcc = repositoryAccounts[event.receivingAccountIndex];

    switch (event.type) {
    case ConversationEvent::CONNECT:
        // Instigator can only be connected if already disconnected
        if (!instigatorRepoAcc.repository || instigatorRepoAcc.connected) {
            return false;
        }
        break;
    case ConversationEvent::DISCONNECT:
        // Instigator can only be disconnected if already connected
        if (!instigatorRepoAcc.repository || !instigatorRepoAcc.connected) {
            return false;
        }
        break;
    case ConversationEvent::ADD_MEMBER:
        // Note: this is within the context of the CURRENT repo
        // The instigator should only be able to add the receiver if:
        // 1. The instigator is not trying to add themselves (this occurs in the very first event)
        // 2. The instigator is already part of of the conversation
        // 3. The receiver is not already part of the conversation
        if (event.instigatorAccountIndex == event.receivingAccountIndex || !instigatorRepoAcc.repository
            || receiverRepoAcc.repository
            || instigatorRepoAcc.conversation->isMember(receiverRepoAcc.account->getUsername(), true)) {
            return false;
        }
        break;
    case ConversationEvent::SEND_MESSAGE:
        // Instigator can only send a message if part of the conversation
        if (!instigatorRepoAcc.repository) {
            return false;
        }
        break;
    case ConversationEvent::CLONE:
        // The instigator should only be able to have the receiver clone from them if:
        // 1. The instigator and receiver are online
        // 2. The instigator is part of the conversation
        // 3. The receiver is not already part of the conversation
        // 4. The receiver is a member of the instigator's repository
        if (instigatorRepoAcc.repository && !receiverRepoAcc.repository && instigatorRepoAcc.connected
            && receiverRepoAcc.connected) {
            return isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex);
        } else {
            return false;
        }
        break;
    case ConversationEvent::FETCH:
        // The instigator should only be able to have the receiver fetch from them if:
        // 1. The instigator and receiver are online
        // 2. The instigator and receiver are part of the conversation
        // 3. The receiver is already a member of the instigator's repository
        if (instigatorRepoAcc.repository && receiverRepoAcc.repository && instigatorRepoAcc.connected
            && receiverRepoAcc.connected) {
            if (!isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex)) {
                return false;
            }
            // We cannot have two pending fetches from the same device at the same time.
            if (receiverRepoAcc.devicesWithPendingFetch.contains(instigatorRepoAcc.account->getAccountID())) {
                return false;
            }
        } else {
            return false;
        }
        break;
    case ConversationEvent::MERGE:
        // Merge events should always be valid since they are only scheduled after a valid
        // fetch event has been executed.
        assert(instigatorRepoAcc.repository);
        assert(receiverRepoAcc.repository);
        assert(receiverRepoAcc.devicesWithPendingFetch.contains(instigatorRepoAcc.account->getAccountID()));
        break;
    default:
        assert(false && "Unknown ConversationEvent type received, this is a bug!");
        return false;
        break;
    }
    return true;
}

/**
 * Triggers ConversationRepository actions based on the given event.
 * @param event The event that dictates which actions are to be taken
 */
void
ConversationDST::triggerEvent(const Event& event, EventQueue* queue)
{
    auto& instigatorAccount = repositoryAccounts[event.instigatorAccountIndex];
    auto& receivingAccount = repositoryAccounts[event.receivingAccountIndex];

    // Perform the necessary action based on the event type
    switch (event.type) {
    case ConversationEvent::CONNECT: {
        // Enable the account
        instigatorAccount.connected = true;

        if (queue) {
            // We should eventually incorporate the DRT and message notifications into the simulation, but for
            // now just have members sync with everyone else in the conversation when they connect.
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);
            scheduleGitEvent(*queue, ConversationEvent::FETCH, -1, event.instigatorAccountIndex, event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::DISCONNECT: {
        // Disable the account
        instigatorAccount.connected = false;
        break;
    }
    case ConversationEvent::ADD_MEMBER: {
        // Add the account as a member within the context of the instigator's repository
        dht::InfoHash h(receivingAccount.account->getUsername());
        const std::string commitID = instigatorAccount.repository->addMember(h.toString());

        assert(!commitID.empty());
        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue,
                             ConversationEvent::CLONE,
                             event.instigatorAccountIndex,
                             event.receivingAccountIndex,
                             event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::SEND_MESSAGE: {
        msgCount++;
        const std::string& messageContent = "{\"body\":\"" + std::to_string(msgCount) + "\",\"type\":\"text/plain\"}";
        //  Note for logging: commitMessage()
        //  returns the ID
        const std::string& commitID = instigatorAccount.repository->commitMessage(messageContent, true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::CLONE: {
        auto repo = ConversationRepository::cloneConversation(receivingAccount.account,
                                                              instigatorAccount.account->getAccountID(),
                                                              instigatorAccount.repository->id())
                        .first;
        // Join right after clone
        const std::string commitID = repo->join();
        assert(!commitID.empty());

        receivingAccount.createConversation(std::move(repo));
        if (queue) {
            // Now we notify the others about the join
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.receivingAccountIndex, -1, event.timeOfOccurrence);
        }
        assert(receivingAccount.client.hasConsistentHistory());
        break;
    }
    case ConversationEvent::FETCH: {
        auto commitId = instigatorAccount.repository->getHead();
        assert(!commitId.empty());
        auto repo = receivingAccount.repository.get();
        assert(repo != nullptr);
        // Don't fetch if the commit is already there.
        if (repo->hasCommit(commitId)) {
            return;
        }
        // Fetch from peer
        auto deviceId = instigatorAccount.account->getAccountID();
        bool fetched = repo->fetch(deviceId);
        assert(fetched);

        assert(!receivingAccount.devicesWithPendingFetch.contains(deviceId));
        receivingAccount.devicesWithPendingFetch.insert(deviceId);
        if (queue) {
            scheduleGitEvent(*queue,
                             ConversationEvent::MERGE,
                             event.instigatorAccountIndex,
                             event.receivingAccountIndex,
                             event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::MERGE: {
        auto deviceId = instigatorAccount.account->getAccountID();
        assert(receivingAccount.devicesWithPendingFetch.contains(deviceId));
        receivingAccount.devicesWithPendingFetch.erase(deviceId);

        auto commits = receivingAccount.repository->mergeHistory(deviceId, [](const std::string&) {});
        if (!commits.empty()) {
            // Messages have been found. This announce function should add the messages into the respective RepositoryAccount
            receivingAccount.conversation->announce(commits, false);
        }
        assert(receivingAccount.client.hasConsistentHistory());
        break;
    }
    default:
        assert(false && "Unknown ConversationEvent type received, this is a bug!");
        break;
    }
}

/**
 * Verifies that all messages are loaded and given the SwarmLoaded signal on a fresh instance of
 * opening a conversation
 * @return bool Whether or not all messages were loaded
 */
bool
ConversationDST::verifyLoadConversationFromScratch()
{
    for (RepositoryAccount& repositoryAccount : repositoryAccounts) {
        if (repositoryAccount.repository) {
            // Clear messages that may have been added from previous signal handling
            repositoryAccount.client.clearMessages();
            // Get the id of the conversation (i.e. repo id)
            std::string conversationId = repositoryAccount.repository->id();

            LogOptions options;
            options.from = "";
            // options.nbOfCommits = 20;
            std::shared_ptr<Conversation> conversation = std::make_shared<Conversation>(repositoryAccount.account,
                                                                                        conversationId);
            auto accountId = repositoryAccount.account->getAccountID();
            auto messages = conversation->loadMessagesSync(options);
            emitSignal<libjami::ConversationSignal::SwarmLoaded>(0, accountId, conversationId, messages);

            // Now we compare the number of messages received via the SwarmLoaded signal to that of
            // the ones logged in the repository
            LogOptions repoLogOptions;
            repoLogOptions.skipMerge = true; // Merge commits dont get SwarmLoaded signals, so we
                                             // disabled their logging here
            repoLogOptions.fastLog = true;
            std::vector<jami::ConversationCommit> loggedMessages = repositoryAccount.repository->log(repoLogOptions);

            auto numRepoMessages = loggedMessages.size();
            auto numClientMessages = repositoryAccount.client.getMessages().size();
            if (numClientMessages != numRepoMessages) {
                fmt::print(
                    fg(fmt::color::red),
                    "Number of messages received via SwarmLoaded signal does not match the number of messages in "
                    "the repository for account {}! Received {}, expected {}.",
                    repositoryAccount.account->getAccountID(),
                    numClientMessages,
                    numRepoMessages);
                return false;
            }
        }
    }

    return true;
}

/**
 * Generates a randomized sequence of events based on the current members of the
 * ConversationDST class. A sequence of events can be re-simulated provided that the same
 * eventSeed is used. Each event will be validated on a case-by-case basis given the already
 * validated events. Valid events are then triggered and logged.
 */
void
ConversationDST::generateEventSequence(unsigned maxEvents)
{
    assert(validatedEvents.empty());

    auto simulationStart = std::chrono::steady_clock::now();

    // Generate the random number based on the seed from eventSeed
    gen_.seed(eventSeed);

    numAccountsToSimulate_ = std::uniform_int_distribution<>(2, MAX_ACCOUNTS)(gen_);
    assert(numAccountsToSimulate_ <= repositoryAccounts.size());

    std::uniform_int_distribution<> accountDist {0, numAccountsToSimulate_ - 1};
    std::discrete_distribution<> repositoryEventDist {repositoryEventWeights.begin(), repositoryEventWeights.end()};

    // Indices to be used throughout iteration
    int instigatorAccountIndex, receivingAccountIndex;

    msgCount = 0;

    // Pick a random account to initialize
    instigatorAccountIndex = accountDist(gen_);
    // Get the instigator's account
    auto& instigatorAccount = repositoryAccounts[instigatorAccountIndex].account;

    // Create the initial conversation
    auto repo = ConversationRepository::createConversation(instigatorAccount);
    assert(repo != nullptr);
    fmt::print("Conversation ID: {}\n", repo->id());
    repositoryAccounts[instigatorAccountIndex].createConversation(std::move(repo));

    // Add the initial event
    startTime = std::chrono::nanoseconds(0);

    EventQueue unvalidatedEvents;
    unvalidatedEvents.emplace(instigatorAccountIndex, instigatorAccountIndex, ConversationEvent::ADD_MEMBER, startTime);

    // Generate the number of events we want to occur in the simulated conversation
    while (unvalidatedEvents.size() < maxEvents) {
        // Select a random event from the pool of events
        ConversationEvent generatedEvent = static_cast<ConversationEvent>(repositoryEventDist(gen_));

        // Select an account (instigator) to perform an event on another account (receiver)
        instigatorAccountIndex = accountDist(gen_);
        receivingAccountIndex = accountDist(gen_);

        startTime += std::chrono::milliseconds(1000);

        // Add the event
        Event event(instigatorAccountIndex, receivingAccountIndex, generatedEvent, startTime);
        unvalidatedEvents.push(event);
    }

    // We want to skip the first event, we already know it's the initial repositoryAccount being added
    validatedEvents.emplace_back(unvalidatedEvents.top());
    unvalidatedEvents.pop();
    if (enableEventLogging_)
        fmt::print("===================== Validating Events... =====================\n");
    eventLogger(validatedEvents.front());

    size_t unvalidatedEventsCount = 1;
    size_t invalidEventsCount = 0;
    while (unvalidatedEventsCount < maxEvents && !unvalidatedEvents.empty()) {
        Event event = unvalidatedEvents.top();
        unvalidatedEvents.pop();
        eventLogger(event);
        unvalidatedEventsCount++;

        if (validateEvent(event)) {
            validatedEvents.emplace_back(event);
            triggerEvent(event, &unvalidatedEvents);
        } else {
            invalidEventsCount++;
        }
    }

    auto simulationEnd = std::chrono::steady_clock::now();
    auto simulationTime = std::chrono::duration_cast<std::chrono::milliseconds>(simulationEnd - simulationStart).count();

    // Display a summary of the  sequence of events that took place.
    double rejectionRate = (static_cast<double>(invalidEventsCount) / unvalidatedEventsCount);
    sumOfRejectionRates += rejectionRate;
    fmt::print("=========================== Summary ==========================\n");
    fmt::print("Random seed used: {}\n", eventSeed);
    fmt::print("Total number of accounts simulated: {}\n", numAccountsToSimulate_);
    fmt::print("Total events generated: {}\n", unvalidatedEventsCount);
    fmt::print("Total valid events: {}\n", validatedEvents.size());
    fmt::print("Total invalid events: {}\n", invalidEventsCount);
    fmt::print("Rejection rate: {:.1f}%\n", rejectionRate * 100);
    fmt::print("Simulation time: {} ms\n", simulationTime);
    if (enableEventLogging_) {
        fmt::print("====================== Validated Events ======================\n");
        for (const auto& e : validatedEvents) {
            eventLogger(e);
        }
    }
}

/**
 * Clear out the contents of the repositories of each account so that the accounts may be used for
 * multiple iterations.
 */
void
ConversationDST::resetRepositories()
{
    for (RepositoryAccount& repoAcc : repositoryAccounts) {
        if (repoAcc.repository) {
            // Force delete conversation directories
            auto accountId = repoAcc.account->getAccountID();
            auto convId = repoAcc.repository->id();
            auto convPath = fileutils::get_data_dir() / accountId / "conversations" / convId;
            auto convDataPath = fileutils::get_data_dir() / accountId / "conversation_data" / convId;
            if (std::filesystem::exists(convPath)) {
                std::filesystem::remove_all(convPath);
            }
            if (std::filesystem::exists(convDataPath)) {
                std::filesystem::remove_all(convDataPath);
            }
        }

        // Reset account state
        repoAcc.repository.reset();
        repoAcc.conversation.reset();
        repoAcc.connected = true;
        repoAcc.devicesWithPendingFetch.clear();
        repoAcc.client = SimClient();
    }

    // Clear all events for next cycle
    validatedEvents.clear();
}

bool
ConversationDST::checkAppearancesForAllAccounts()
{
    for (const auto& repoAcc : repositoryAccounts) {
        if (!checkAppearances(repoAcc)) {
            return false;
        }
    }
    return true;
}

/**
 * @param accountsToCheck The specific repostiory accounts to be checked. Passing in an empty vector will check all accounts
 * @brief Checks whether all the messages for each account were actually displayed
 */
bool
ConversationDST::checkAppearances(const RepositoryAccount& repoAcc)
{
    auto clientMessages = repoAcc.client.getMessages();

    if (!repoAcc.repository) {
        if (!clientMessages.empty()) {
            JAMI_ERROR("[{}] Account has messages in its history but no repository", repoAcc.account->getDisplayName());
            return false;
        }
        return true;
    }
    // Get the commits as they seen in the repository
    LogOptions logOptions;
    logOptions.skipMerge = true;
    std::vector<jami::ConversationCommit> repoCommits = repoAcc.repository->log(logOptions);
    // The number of messages "displayed" on the client should be identical to that of the actual number of messages
    // in the repo for the same account
    if (repoCommits.size() != clientMessages.size()) {
        if (enableGitLogging_) {
            fmt::print(fg(fmt::color::red),
                       "[{}] Repo and client don't have the same number of messages ({} vs {})\n",
                       repoAcc.account->getDisplayName(),
                       repoCommits.size(),
                       clientMessages.size());
            fmt::print("Repo commits:\n");
            for (const auto& commit : repoCommits) {
                bool clientHasCommit = false;
                for (const auto& message : clientMessages) {
                    if (commit.id == message.id) {
                        clientHasCommit = true;
                        break;
                    }
                }
                if (clientHasCommit) {
                    fmt::print("Message: {}, Author: {}, ID: {}\n", commit.commit_msg, commit.author.name, commit.id);
                } else {
                    fmt::print(fg(fmt::color::red),
                               "Message: {}, Author: {}, ID: {}\n",
                               commit.commit_msg,
                               commit.author.name,
                               commit.id);
                }
            }
        }
        return false;
    }

    // We can safely access both vectors' content using the same index
    bool orderingCorrect = true;
    for (size_t i = 0; i < clientMessages.size(); i++) {
        if (clientMessages[i].id != repoCommits[i].id) {
            orderingCorrect = false;
            break;
        }
    }
    if (enableGitLogging_ && !orderingCorrect) {
        fmt::print(fg(fmt::color::red),
                   "[{}] Client messages are not in the correct order\n",
                   repoAcc.account->getDisplayName());
        for (size_t i = 0; i < clientMessages.size(); i++) {
            if (clientMessages[i].id == repoCommits[i].id) {
                fmt::print("Repo commit ID: {}, Client message ID: {}\n", repoCommits[i].id, clientMessages[i].id);
            } else {
                fmt::print(fg(fmt::color::red),
                           "Repo commit ID: {}, Client message ID: {}\n",
                           repoCommits[i].id,
                           clientMessages[i].id);
            }
        }
    }
    return orderingCorrect;
}

/**
 * @brief Load the configuration for an existing, previously-ran test. This will configure the class with with
 * accounts, a seed, and the validated event sequence.
 * @param unitTestPath The file path of the unit test configuration
 */
UnitTest
ConversationDST::loadUnitTestConfig(const std::string& unitTestPath)
{
    JAMI_LOG("Loading unit test {}", unitTestPath);
    std::string configContent = fileutils::loadTextFile(unitTestPath);

    // Parse the content into a json object
    Json::Value unitTest;
    if (!json::parse(configContent, unitTest)) {
        JAMI_ERROR("Failed to parse unit test {}!", unitTestPath);
        return UnitTest();
    }

    // Store the event seed (for logging, this will not be used anywhere else)
    eventSeed = unitTest[unitTestKeys[UTKEY::SEED]].asUInt64();

    // Map account IDs to indices in repositoryAccounts vector
    const Json::Value& accountIDsObj = unitTest[unitTestKeys[UTKEY::ACCOUNT_IDS]];
    std::map<std::string, int> accountIDs;
    for (Json::ArrayIndex i = 0; i < accountIDsObj.size(); ++i) {
        accountIDs[accountIDsObj[i].asString()] = i;
    }

    UnitTest ret;
    ret.numAccounts = accountIDs.size();

    // Get the validated events
    const Json::Value& validatedEventsObj = unitTest[unitTestKeys[UTKEY::VALIDATED_EVENTS]];
    for (const Json::Value& validatedEvent : validatedEventsObj) {
        const Json::Value validatedEventType = validatedEvent.type();
        if (validatedEventType == Json::ValueType::objectValue) {
            ConversationEvent convEvent = invertedEventNames[validatedEvent[unitTestKeys[UTKEY::EVENT_TYPE]].asString()];
            std::string instigatorAccountID = validatedEvent[unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]].asString();
            std::string receiverAccountID = validatedEvent[unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]].asString();
            ret.events.emplace_back(
                Event(accountIDs[instigatorAccountID], accountIDs[receiverAccountID], convEvent, 0s));
        } else {
            JAMI_ERROR("Invalid JSON object type found, please check your configuration!");
            return UnitTest();
        }
    }

    return ret;
}

/**
 * When called, this function will save the DST current configuration.
 * Configurations get saved as a subdirectory of the configurations directory. A configuration includes:
 * - Date and time of generation
 * - Seed used
 * - Account names
 * - Validated events sequence
 * @note A key with the name "desc" with an empty string as its value. If pushing the configuration to Jami
 * repository, one should replace the value with a description of what the configuration tests.
 */
void
ConversationDST::saveAsUnitTestConfig(const std::string& saveFilePath)
{
    // TODO Add error handling in this function
    fmt::print("Saving DST configuration for seed {} as unit test...\n", eventSeed);

    // Get current date in YYYYMMDD format
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&now_c);

    std::ostringstream dateOss, timeOss;
    dateOss << std::put_time(&local_tm, "%Y%m%d");
    timeOss << std::put_time(&local_tm, "%H:%M");

    std::string dateStr = dateOss.str();
    std::string timeStr = timeOss.str();

    // Create a JSON object representative of the configuration
    Json::Value unitTest;
    // Store basic info
    unitTest[unitTestKeys[UTKEY::SEED]] = eventSeed;
    unitTest[unitTestKeys[UTKEY::DATE]] = dateStr;
    unitTest[unitTestKeys[UTKEY::DESC]] = "", unitTest[unitTestKeys[UTKEY::TIME]] = timeStr;
    unitTest[unitTestKeys[UTKEY::ACCOUNT_IDS]] = Json::arrayValue;
    // TODO Ensure all display names are unique
    for (int i = 0; i < numAccountsToSimulate_; i++) {
        unitTest[unitTestKeys[UTKEY::ACCOUNT_IDS]].append(repositoryAccounts[i].account->getDisplayName());
    }

    // Get the validated events and store them as individual JSON objects
    unitTest[unitTestKeys[UTKEY::VALIDATED_EVENTS]] = Json::arrayValue;

    // TODO Store timestamps
    for (size_t i = 0; i < validatedEvents.size(); i++) {
        Json::Value validatedEventObject;
        validatedEventObject[unitTestKeys[UTKEY::EVENT_TYPE]] = eventNames[validatedEvents[i].type];
        validatedEventObject[unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]]
            = repositoryAccounts[validatedEvents[i].instigatorAccountIndex].account->getDisplayName();
        validatedEventObject[unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]]
            = repositoryAccounts[validatedEvents[i].receivingAccountIndex].account->getDisplayName();

        unitTest[unitTestKeys[UTKEY::VALIDATED_EVENTS]].append(validatedEventObject);
    }

    // Convert to a stylized string (make it more legible)
    const std::string dstConfig = unitTest.toStyledString();

    const std::filesystem::path filePath(saveFilePath);

    // Create the directory if it doesn't exist
    std::filesystem::create_directories(filePath.parent_path());
    fileutils::saveFile(filePath, (const uint8_t*) dstConfig.data(), dstConfig.size());
}

bool
ConversationDST::run(uint64_t seed, unsigned maxEvents, const std::string& saveFilePath)
{
    eventSeed = seed;
    generateEventSequence(maxEvents);

    // Log all the commits in each respective repository
    displayGitLog();

    // Do verifications
    bool ok = checkAppearancesForAllAccounts();
    // Load each conversation as if it were being opened for the first time
    ok &= verifyLoadConversationFromScratch();

    // Save the iteration of the unit test as a configuration
    if (!saveFilePath.empty()) {
        saveAsUnitTestConfig(saveFilePath);
    }

    if (ok) {
        fmt::print("PASS: {}\n", seed);
    } else {
        fmt::print(fg(fmt::color::red), "FAIL: {}\n", seed);
    }
    return ok;
}

/**
 * Run a given number of randomized tests and display seeds that ran with/without issues
 */
bool
ConversationDST::runCycles(unsigned numCycles, unsigned maxEvents)
{
    unsigned passCount = 0;
    std::vector<uint64_t> failingSeeds;
    for (unsigned i = 0; i < numCycles; ++i) {
        fmt::print("\nStarting cycle {} of {}\n", i + 1, numCycles);

        uint64_t seed = (static_cast<uint64_t>(rd()) << 32) | rd();
        fmt::print("Random seed generated: {}\n", seed);

        bool ok = run(seed, maxEvents);
        if (ok) {
            passCount++;
        } else {
            failingSeeds.push_back(seed);
        }

        // Clear out the repositories for reuse
        resetRepositories();
    }

    fmt::print("\nInvalid Events Average: {:.1f}%\n",
               (static_cast<double>(sumOfRejectionRates) / static_cast<double>(numCycles)) * 100);
    fmt::print("Passed: {} / {}\n", passCount, numCycles);
    if (!failingSeeds.empty()) {
        fmt::print("Failing seeds:\n");
        for (const auto& seed : failingSeeds) {
            fmt::print("  {}\n", seed);
        }
    }

    return passCount == numCycles;
}

/**
 * @brief Runs the unit test by triggering all the events in order, specified by the loaded configuration.
 */
void
ConversationDST::runUnitTest(const UnitTest& unitTest)
{
    auto simulationStart = std::chrono::steady_clock::now();

    // Get the first event (representative of who holds the first repository)
    const Event& firstEvent = unitTest.events.front();
    // Get the account associated with the first event
    auto& firstEventAccount = repositoryAccounts[firstEvent.instigatorAccountIndex].account;
    // Create the initial conversation
    auto repo = ConversationRepository::createConversation(firstEventAccount);
    repositoryAccounts[firstEvent.instigatorAccountIndex].createConversation(std::move(repo));

    msgCount = 0;
    // We skip the first event since it represents who gets their repository first
    for (size_t i = 1; i < unitTest.events.size(); i++) {
        eventLogger(unitTest.events[i]);
        triggerEvent(unitTest.events[i]);
    }

    auto simulationEnd = std::chrono::steady_clock::now();
    auto simulationTime = std::chrono::duration_cast<std::chrono::milliseconds>(simulationEnd - simulationStart).count();

    fmt::print("=========================== Summary ==========================\n");
    fmt::print("Random seed used: {}\n", eventSeed);
    fmt::print("Number of accounts simulated: {}\n", unitTest.numAccounts);
    fmt::print("Number of events : {}\n", unitTest.events.size());
    fmt::print("Simulation time: {} ms\n", simulationTime);
    if (enableEventLogging_) {
        fmt::print("====================== Events ======================\n");
        for (const auto& e : unitTest.events) {
            eventLogger(e);
        }
    }

    displayGitLog();
}

} // namespace test
} // namespace jami
