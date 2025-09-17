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

#include <cppunit/TestAssert.h>

namespace jami {
namespace test {

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

void
ConversationDST::connectSignals()
{
    JAMI_LOG("=== CONNECTING SIGNALS ===");
    confHandlers.clear();

    // For account announcement in setUp()
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [&](const std::string& accountId, const std::map<std::string, std::string>&) {
            for (auto& repoAcc : repositoryAccounts) {
                auto repositoryAccountID = repoAcc.account->getAccountID();
                if (accountId == repositoryAccountID) {
                    auto details = repoAcc.account->getVolatileAccountDetails();
                    auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                    repoAcc.deviceAnnounced = deviceAnnounced == "true";
                    break;
                }
            }
            cv.notify_one();
        }));

    // For checking individual message reception on pull's
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId, const std::string& /* conversationId */, const libjami::SwarmMessage& message) {
            for (RepositoryAccount& repositoryAccount : repositoryAccounts) {
                if (accountId == repositoryAccount.account->getAccountID()) {
                    repositoryAccount.addConversationCommit(message.id);
                    repositoryAccount.swarmLoaded = true;
                }
            }
            cv.notify_one();
        }));

    // For checking full conversation loading from scratch
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmLoaded>(
        [&](uint32_t requestId,
            const std::string& accountId,
            const std::string& conversationId,
            const std::vector<libjami::SwarmMessage>& messages) {
            for (RepositoryAccount& repositoryAccount : repositoryAccounts) {
                if (repositoryAccount.account->getAccountID() == accountId) {
                    for (const auto& msg : messages) {
                        repositoryAccount.addConversationCommit(msg.id);
                    }
                    repositoryAccount.swarmLoaded = true;
                    break;
                }
            }
            cv.notify_one();
        }));

    JAMI_LOG("Registering {} signal handlers...", confHandlers.size());
    libjami::registerSignalHandlers(confHandlers);
}

// TODO Combine with setUp?
bool
ConversationDST::waitForDeviceAnnouncements()
{
    return cv.wait_for(lk, 30s, [&] {
        for (const auto& repoAcc : repositoryAccounts) {
            if (!repoAcc.deviceAnnounced)
                return false;
        }
        return true;
    });
}

// TODO Most of this only needs to be done once, not at the beginning of every test
void
ConversationDST::setUp(int numAccountsToSimulate)
{
    repositoryEventDist = std::discrete_distribution<>(repositoryEventWeights.begin(),
                                                       repositoryEventWeights.end()); // 4 events

    // Initiate account creations for random repo generation
    messageGen.seed(eventSeed);

    // Reserve space for account IDs
    repositoryAccounts.reserve(numAccountsToSimulate);
    // Get the number of existing accounts (in HOME/.local/share/jami/)
    auto existingAccounts = Manager::instance().getAllAccounts<JamiAccount>();
    const int numberOfExistingAccounts = static_cast<int>(existingAccounts.size());

    // Add the existing accounts to the repository accounts list
    for (auto existingAccount : existingAccounts) {
        repositoryAccounts.emplace_back(RepositoryAccount(existingAccount, nullptr, true));
        // Check if the account is already announced (won't get a signal since it's already registered)
        auto details = existingAccount->getVolatileAccountDetails();
        auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
        repositoryAccounts.back().deviceAnnounced = deviceAnnounced == "true";
        // TODO Print full path to account?
        JAMI_LOG("Existing account added: {} [{}]", existingAccount->getAccountID(), existingAccount->getDisplayName());
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
        accountDetails[ConfProperties::UPNP_ENABLED] = "true";
        accountDetails[ConfProperties::ARCHIVE_PASSWORD] = "";

        // Add the account and store the returned account ID
        auto accountId = Manager::instance().addAccount(accountDetails);

        // Initialize the account details for later indexing
        auto account = Manager::instance().getAccount<JamiAccount>(accountId);
        repositoryAccounts.emplace_back(RepositoryAccount(account, nullptr, true));
        JAMI_LOG("New account added: {} [{}]", account->getAccountID(), account->getDisplayName());
    }

    // Reserve space for max number of events
    unvalidatedEvents.reserve(numEventsToGenerate);
}

void
ConversationDST::tearDown()
{
    resetRepositories();
    JAMI_LOG("Tearing down ConversationDST...");
    // TODO Call libjami::fini()? Or move libjami init/start out of setUp()?
}

/**
 * Displays info about an event or git operation
 * @param event The event to be logged
 */
void
ConversationDST::eventLogger(const Event& event)
{
    if (dstConfig.enableEventLogging) {
        auto instigatorName = repositoryAccounts[event.instigatorAccountIndex].account->getDisplayName();
        auto receiverName = repositoryAccounts[event.receivingAccountIndex].account->getDisplayName();
        auto eventTime = event.timeOfOccurrence.count();

        switch (event.type) {
        case ConversationEvent::ADD_MEMBER:
            JAMI_LOG("EVENT: {} added account {} to the conversation at {}", instigatorName, receiverName, eventTime);
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
            JAMI_LOG("GIT OPERATION: {} cloned the conversation from {} at {}", receiverName, instigatorName, eventTime);
            break;
        case ConversationEvent::PULL:
            JAMI_LOG("GIT OPERATION: {} pulled the conversation from {} at {}", receiverName, instigatorName, eventTime);
            break;
        default:
            JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                         "triggered.");
            break;
        }
    }
}

/**
 * Displays a summary of the events sequence and the sequence of events that took place. Information
 * includes:
 * - The seed used
 * - The number of accounts simulated
 * - The number of events generated
 * - The number of valid/invalid events
 * - The rejection rate
 */
// TODO Inline?
void
ConversationDST::displaySummary(int numSimulatedAccounts)
{
    float rejectionRate = (static_cast<float>(invalidEventsCount) / unvalidatedEvents.size());
    sumOfRejectionRates += rejectionRate;
    JAMI_LOG("=========================== Summary ==========================");
    JAMI_LOG("Random seed used: {}", eventSeed);
    JAMI_LOG("Total number of accounts simulated: {}", numSimulatedAccounts);
    JAMI_LOG("Total events generated: {}", unvalidatedEvents.size());
    JAMI_LOG("Total valid events: {}", validatedEvents.size());
    JAMI_LOG("Total invalid events: {}", invalidEventsCount);
    JAMI_LOG("Rejection rate: {:.1f}%", rejectionRate * 100);
    JAMI_LOG("====================== Validated Events ======================");
    for (const auto& e : validatedEvents) {
        eventLogger(e);
    }
}

/**
 * Display the git repository logs of each account that contains a repository. This is the
 * equivalent of running `git log` for each account that has a repository associated with it.
 */
void
ConversationDST::displayGitLog()
{
    if (dstConfig.enableGitLogging) {
        JAMI_LOG("======================= Git Logs ============================");
        for (const RepositoryAccount& repositoryAccount : repositoryAccounts) {
            if (repositoryAccount.repository) {
                JAMI_LOG("Git log for account {}:", repositoryAccount.account->getDisplayName());
                auto log = repositoryAccount.repository->log(LogOptions {});
                for (const auto& entry : log) {
                    JAMI_LOG("Message: {}, Author: {}, ID: {}", entry.commit_msg, entry.author.name, entry.id);
                }
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
ConversationDST::scheduleGitEvent(const ConversationEvent& gitOperation,
                                  int instigatorAccountIndex,
                                  int receivingAccountIndex,
                                  std::chrono::nanoseconds eventTimeOfOccurrence)
{
    // Seed
    std::mt19937_64 gen(eventSeed);

    std::uniform_int_distribution<> adaptiveDist(5, 50);
    std::uniform_int_distribution<> parallelProbabilityDist(0, 100);

    auto scheduledTime = eventTimeOfOccurrence + std::chrono::nanoseconds(adaptiveDist(gen));

    // Check if this is a one-to-one or one-to-all operation
    if (instigatorAccountIndex != receivingAccountIndex) {
        // Just push_back, do not insert in order
        unvalidatedEvents.push_back(Event(instigatorAccountIndex, receivingAccountIndex, gitOperation, scheduledTime));
    } else {
        // Schedule the Git event for each account to "receive"
        for (int i = 0; i < static_cast<int>(repositoryAccounts.size()); ++i) {
            if (repositoryAccounts[i].repository != nullptr && i != instigatorAccountIndex) {
                auto accountSpecificTime = scheduledTime + (std::chrono::nanoseconds(adaptiveDist(gen)) * (i + 1));
                unvalidatedEvents.push_back(Event(instigatorAccountIndex, i, gitOperation, accountSpecificTime));
                // Decide whether or not to make a parallel event based on probability
                if (gitOperation == ConversationEvent::PULL && parallelProbabilityDist(gen) >= 80) {
                    unvalidatedEvents.back().expectIncomingParallelEvent = true;
                    unvalidatedEvents.push_back(Event(i, instigatorAccountIndex, gitOperation, accountSpecificTime));
                }
            }
        }
    }
}

/**
 * Schedule an event to take place given an event that just occurred
 * @param event The event that just took place
 */
void
ConversationDST::scheduleSideEffects(const Event& event)
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
        scheduleGitEvent(ConversationEvent::PULL,
                         event.receivingAccountIndex, // Same indicies so that everyone pulls from the joiner
                         event.receivingAccountIndex,
                         event.timeOfOccurrence);
        break;
    default:
        break;
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
        // The instigator should only be able to add the receiver if:
        // 1. The instigator is not trying to add themselves (this occurs in the very first event)
        // 2. The instigator is already part of of the conversation
        // 3. The receiver is not already part of the conversation
        if (event.instigatorAccountIndex == event.receivingAccountIndex || !instigatorRepoAcc.repository
            || receiverRepoAcc.repository || isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex)) {
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
        if (instigatorRepoAcc.repository && !receiverRepoAcc.repository && instigatorRepoAcc.connected
            && receiverRepoAcc.connected) {
            return isUserInRepo(event.instigatorAccountIndex, event.receivingAccountIndex);
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
        if (instigatorRepoAcc.repository && receiverRepoAcc.repository && instigatorRepoAcc.connected
            && receiverRepoAcc.connected) {
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

// TODO Remove this, "pull" events should be split into "fetch" + "merge" events, which will
// also allow us to get rid of the hack to execute multiple events at the same time.
static std::vector<std::map<std::string, std::string>>
pull_temp(ConversationRepository* repo, const std::string& deviceId, const std::string& commitId)
{
    // If recently fetched, the commit can already be there, so no need to do complex operations
    if (commitId != "" && repo->getCommit(commitId, false) != std::nullopt) {
        return {};
    }
    // Pull from remote
    auto fetched = repo->fetch(deviceId);
    if (!fetched) {
        return {};
    }
    auto commits = repo->mergeHistory(deviceId, [](const std::string&) {});
    return commits;
}

/**
 * Triggers ConversationRepository actions based on the given event.
 * @param event The event that dictates which actions are to be taken
 * @note For the action of pulling from a repository, messages are checked for whether or not they
 * had been sent the appropriate signal for displaying themselves on a client.
 */
void
ConversationDST::triggerEvent(const Event& event)
{
    auto instigatorAccount = repositoryAccounts[event.instigatorAccountIndex].account;
    auto receivingAccount = repositoryAccounts[event.receivingAccountIndex].account;

    // Perform the necessary action based on the event type
    switch (event.type) {
    case ConversationEvent::CONNECT: {
        // Enable the account
        repositoryAccounts[event.instigatorAccountIndex].connected = true;

        // When running unit tests, additional events should NOT be scheduled and the unit tests events should be
        // followed strictly
        if (!dstConfig.runUnitTest) {
            // We can make an assumption that the instigator will pull from a user once they've
            // connected to the repository and that the user they are pulling from is online
            scheduleGitEvent(ConversationEvent::PULL,
                             event.instigatorAccountIndex,
                             event.receivingAccountIndex,
                             event.timeOfOccurrence);
        }
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
        const std::string commitID = repositoryAccounts[event.instigatorAccountIndex].repository->addMember(
            h.toString());
        repositoryAccounts[event.instigatorAccountIndex].addConversationCommit(commitID);
        break;
    }
    case ConversationEvent::SEND_MESSAGE: {
        msgCount++;
        const std::string& messageContent = "{\"body\":\"" + std::to_string(msgCount) + "\",\"type\":\"text/plain\"}";
        //  Note for logging: commitMessage()
        //  returns the ID
        const std::string& commitID = repositoryAccounts[event.instigatorAccountIndex]
                                          .repository->commitMessage(messageContent, true);

        // Insert the newly commited message
        repositoryAccounts[event.instigatorAccountIndex].addConversationCommit(commitID);
        break;
    }
    case ConversationEvent::CLONE: {
        repositoryAccounts[event.receivingAccountIndex].repository
            = ConversationRepository::cloneConversation(
                  repositoryAccounts[event.receivingAccountIndex].account,
                  std::string(repositoryAccounts[event.instigatorAccountIndex].account->getAccountID()),
                  repositoryAccounts[event.instigatorAccountIndex].repository->id())
                  .first;

        // Copy the messages found
        repositoryAccounts[event.receivingAccountIndex].messages = repositoryAccounts[event.instigatorAccountIndex]
                                                                       .messages;

        // Join right after clone
        const std::string commitID = repositoryAccounts[event.receivingAccountIndex].repository->join();
        // Add the ID of the join commit to exsiting commits
        repositoryAccounts[event.receivingAccountIndex].addConversationCommit(commitID);
        break;
    }
    case ConversationEvent::PULL: {
        // CONSIDER FOR FURTHER IMPL: Pull from any random member who has already received the
        // message (might need more state tracking possibly increasing complexity)

        // Create a Conversation object for the receiving account
        std::string convID = repositoryAccounts[event.receivingAccountIndex].repository->id();
        std::shared_ptr<Conversation> conversation
            = std::make_shared<Conversation>(repositoryAccounts[event.receivingAccountIndex].account, convID);

        std::vector<libjami::MediaMap> commits
            = pull_temp(repositoryAccounts[event.receivingAccountIndex].repository.get(),
                        std::string(repositoryAccounts[event.instigatorAccountIndex].account->getAccountID()),
                        repositoryAccounts[event.instigatorAccountIndex].repository->getHead());

        if (!commits.empty()) {
            // Messages have been found. This announce function should add the messages into the resepctive RepositoryAccount
            conversation->announce(commits, false);
        }
        break;
    }
    default:
        JAMI_WARNING("Unknown ConversationEvent type received, this is a bug! No action has been "
                     "triggered.");
        break;
    }
}

/**
 * @brief Trigger a pair of parallel events
 *
 * @warning Supported pairs only include two PULL events at the moment
 */
void
ConversationDST::triggerEvents(const std::vector<Event>& parallelEvents)
{
    if (parallelEvents.size() < 2) {
        JAMI_WARNING(
            "Only a single event was found as part of a parallel grouping! Please check your config.json file!");
        JAMI_WARNING("Running as blocking...");
        triggerEvent(parallelEvents[0]);
        return;
    }

    switch (parallelEvents.size()) {
    case 2:
        // Parallel pulling
        if (parallelEvents[0].type == ConversationEvent::PULL && parallelEvents[1].type == ConversationEvent::PULL) {
            // Verify that the pulling is occuring for mirrored users
            if (parallelEvents[0].instigatorAccountIndex == parallelEvents[1].receivingAccountIndex
                && parallelEvents[1].instigatorAccountIndex == parallelEvents[0].receivingAccountIndex) {
                // The current implementation of ConversationRepository::pull() makes use of the deviceId paramter
                // which is subsequently used to get the remote head of the repository of the device it is fetching
                // from. This means there is no "true" way to execute a git pull in "parallel". To fetch from
                // specific commits we create temporary copies of the repositories before their pulls.

                // We keep the old commit IDs for later reference

                // Get the account IDs
                const std::string firstAccountID = repositoryAccounts[parallelEvents[0].instigatorAccountIndex]
                                                       .account->getAccountID();
                const std::string secondAccountID = repositoryAccounts[parallelEvents[1].instigatorAccountIndex]
                                                        .account->getAccountID();

                // Get the repo ID (remains the same regardless of whom we take it from)
                const std::string repoID = repositoryAccounts[parallelEvents[0].instigatorAccountIndex].repository->id();

                // Create a directory in get_data_dir() with accountID_temp as the name and with the subdirectory
                // conversations
                auto firstAccountTemp = fileutils::get_data_dir() / (firstAccountID + "_temp") / "conversations"
                                        / repoID;
                if (std::filesystem::exists(firstAccountTemp)) {
                    std::filesystem::remove_all(firstAccountTemp);
                }
                std::filesystem::create_directories(firstAccountTemp);
                // Copy the contents of the exsiting conversation to the newly created one
                std::filesystem::copy(fileutils::get_data_dir() / firstAccountID / "conversations" / repoID,
                                      firstAccountTemp,
                                      std::filesystem::copy_options::recursive);

                // Create a directory in get_data_dir() with accountID_temp as the name and with the subdirectory
                // conversations
                auto secondAccountTemp = fileutils::get_data_dir() / (secondAccountID + "_temp") / "conversations"
                                         / repoID;
                if (std::filesystem::exists(secondAccountTemp)) {
                    std::filesystem::remove_all(secondAccountTemp);
                }
                std::filesystem::create_directories(secondAccountTemp);
                // Copy the contents of the exsiting conversation to the newly created one
                std::filesystem::copy(fileutils::get_data_dir() / secondAccountID / "conversations" / repoID,
                                      secondAccountTemp,
                                      std::filesystem::copy_options::recursive);

                // Create a Conversation object for the receiving account
                std::shared_ptr<Conversation> conversationOne
                    = std::make_shared<Conversation>(repositoryAccounts[parallelEvents[0].receivingAccountIndex].account,
                                                     repoID);
                std::shared_ptr<Conversation> conversationTwo
                    = std::make_shared<Conversation>(repositoryAccounts[parallelEvents[1].receivingAccountIndex].account,
                                                     repoID);

                // Perform the parallel pulls
                std::vector<libjami::MediaMap> commitsOne
                    = pull_temp(repositoryAccounts[parallelEvents[0].receivingAccountIndex].repository.get(),
                                firstAccountID + "_temp",
                                "");

                if (!commitsOne.empty()) {
                    // New messages were pulled
                    conversationOne->announce(commitsOne, false);
                }

                std::vector<libjami::MediaMap> commitsTwo
                    = pull_temp(repositoryAccounts[parallelEvents[1].receivingAccountIndex].repository.get(),
                                secondAccountID + "_temp",
                                "");

                if (!commitsTwo.empty()) {
                    // New messages were pulled
                    conversationTwo->announce(commitsTwo, false);
                }
            }
        }
        break;
    default:
        JAMI_WARNING("Unsupported number of parallel events provided. These events will not be ran!");
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
            // Reset the flag for message loading
            repositoryAccount.swarmLoaded = false;
            // Clear messages that may have been added from previous signal handling
            repositoryAccount.messages.clear();
            // Get the id of the conversation (i.e. repo id)
            std::string conversationId = repositoryAccount.repository->id();

            LogOptions options;
            options.from = "";
            // options.nbOfCommits = 20;
            std::shared_ptr<Conversation> conversation = std::make_shared<Conversation>(repositoryAccount.account,
                                                                                        conversationId);
            // Load the 20 messages at a time
            conversation->loadMessages2(
                [accountId = repositoryAccount.account->getAccountID(),
                 conversationId,
                 randomId = std::uniform_int_distribution<uint32_t> {1}(repositoryAccount.account->rand)](
                    auto&& messages) {
                    emitSignal<libjami::ConversationSignal::SwarmLoaded>(randomId, accountId, conversationId, messages);
                },
                options);

            // Wait up to 30 seconds for the signal to be received
            if (!cv.wait_for(lk, 30s, [&] { return repositoryAccount.swarmLoaded; })) {
                JAMI_LOG("ConversationSignal::SwarmLoaded not received for account {}!",
                         repositoryAccount.account->getAccountID());
                return false;
            }

            // Now we compare the number of messages received via the SwarmLoaded signal to that of
            // the ones logged in the repository
            LogOptions repoLogOptions;
            repoLogOptions.skipMerge = true; // Merge commits dont get SwarmLoaded signals, so we
                                             // disabled their logging here
            repoLogOptions.fastLog = true;
            std::vector<jami::ConversationCommit> loggedMessages = repositoryAccount.repository->log(repoLogOptions);
            return repositoryAccount.messages.size() == loggedMessages.size();
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
ConversationDST::generateEventSequence(int numAccountsToSimulate)
{
    CPPUNIT_ASSERT(numAccountsToSimulate > 0);
    CPPUNIT_ASSERT(numAccountsToSimulate <= repositoryAccounts.size());

    // Generate the random number based on the seed from eventSeed
    std::mt19937_64 gen(eventSeed);

    std::uniform_int_distribution<> accountDist {0, numAccountsToSimulate - 1};

    // Indices to be used throughout iteration
    int instigatorAccountIndex, receivingAccountIndex;

    // TODO Should this really be a class member?
    msgCount = 0;

    // Initialize a repository randomly
    JAMI_LOG("Creating initial repository...");

    // Pick a random account to initialize
    instigatorAccountIndex = accountDist(gen);
    // Get the instigator's account
    auto& instigatorAccount = repositoryAccounts[instigatorAccountIndex].account;
    // Create the initial conversation
    repositoryAccounts[instigatorAccountIndex].repository = ConversationRepository::createConversation(
        instigatorAccount);
    // The initial commit should have the same id as the repo, so we just add that
    auto repoID = repositoryAccounts[instigatorAccountIndex].repository->id();
    repositoryAccounts[instigatorAccountIndex].addConversationCommit(repoID);
    // Add the initial event
    startTime = std::chrono::nanoseconds(0);

    unvalidatedEvents.emplace_back(
        Event(instigatorAccountIndex, instigatorAccountIndex, ConversationEvent::ADD_MEMBER, startTime));

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
        unvalidatedEvents.emplace_back(Event(instigatorAccountIndex, receivingAccountIndex, generatedEvent, startTime));
        // Schedule side effects if applicable
        scheduleSideEffects(unvalidatedEvents.back());

        // Log the event
        eventLogger(unvalidatedEvents.back());
    }

    // Sort all events by time before validation
    std::sort(unvalidatedEvents.begin(), unvalidatedEvents.end(), [](const Event& a, const Event& b) {
        return a.timeOfOccurrence < b.timeOfOccurrence;
    });

    // We want to skip the first event, we already know it's the initial repositoryAccount being added
    validatedEvents.emplace_back(unvalidatedEvents.front());
    JAMI_LOG("===================== Validating Events... =====================");
    eventLogger(validatedEvents.front());

    //  Iterate through all the events and not just i-many
    for (size_t i = 1; i < unvalidatedEvents.size(); i++) {
        if (validateEvent(unvalidatedEvents[i])) {
            if (unvalidatedEvents[i].expectIncomingParallelEvent) {
                // Start with the first event parallelized
                std::vector<Event> parallelEvents = {unvalidatedEvents[i]};
                // Insert all the remaining events to parallelize
                // A parallel block will contain a minimum of two events, so we can assume the second event as well
                // as any subsequent ones will be added
                do {
                    i++;
                    if (validateEvent(unvalidatedEvents[i])) {
                        parallelEvents.emplace_back(unvalidatedEvents[i]);
                    }
                } while (unvalidatedEvents[i].expectIncomingParallelEvent && i < unvalidatedEvents.size());
                validatedEvents.insert(validatedEvents.end(), parallelEvents.begin(), parallelEvents.end());
                triggerEvents(parallelEvents);
            } else {
                validatedEvents.emplace_back(unvalidatedEvents[i]);
                eventLogger(validatedEvents.back());
                triggerEvent(validatedEvents.back());
            }
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
    std::vector<std::pair<std::string, std::string>> conversationPaths;

    // Collect conversation paths first and remove accountID_temp folders
    for (RepositoryAccount& repoAcc : repositoryAccounts) {
        if (repoAcc.repository) {
            auto accountId = repoAcc.account->getAccountID();
            auto convId = repoAcc.repository->id();
            conversationPaths.push_back({accountId, convId});

            // Remove accountID_temp folder if it exists
            auto tempDir = fileutils::get_data_dir() / (accountId + "_temp");
            if (std::filesystem::exists(tempDir) && std::filesystem::is_directory(tempDir)) {
                JAMI_LOG("Force deleting temp directory: {}", tempDir.string());
                std::filesystem::remove_all(tempDir);
            }
        }
    }

    // Force delete conversation directories
    for (const auto& [accountId, convId] : conversationPaths) {
        auto convPath = fileutils::get_data_dir() / accountId / "conversations" / convId;
        auto convDataPath = fileutils::get_data_dir() / accountId / "conversation_data" / convId;
        if (std::filesystem::exists(convPath)) {
            JAMI_LOG("Force deleting conversation directory: {}", convPath.string());
            std::filesystem::remove_all(convPath);
        }
        if (std::filesystem::exists(convDataPath)) {
            JAMI_LOG("Force deleting conversation directory: {}", convDataPath.string());
            std::filesystem::remove_all(convDataPath);
        }
    }

    for (RepositoryAccount& repoAcc : repositoryAccounts) {
        repoAcc.repository.reset();
        repoAcc.messages.clear();
    }

    // Clear all events for next cycle
    unvalidatedEvents.clear();
    validatedEvents.clear();
    invalidEventsCount = 0;
}

/**
 * @brief Gets the order of the messages based on their linearized parents in the "client"
 * @param messages The messages to be interpreted
 * @return Messages in the "client" ordered based on their linearized parents
 */
std::vector<jami::ConversationCommit>
ConversationDST::getHistoricalOrder(std::vector<jami::ConversationCommit> messages)
{
    if (messages.empty())
        return {};

    std::vector<jami::ConversationCommit> historicalOrder;
    // Find the initial commit (empty parent)
    auto it = std::find_if(messages.begin(), messages.end(), [](const jami::ConversationCommit& c) {
        return c.linearized_parent.empty();
    });
    if (it == messages.end())
        return {};

    historicalOrder.push_back(*it);

    while (true) {
        auto next = std::find_if(messages.begin(), messages.end(), [&](const jami::ConversationCommit& c) {
            return c.linearized_parent == historicalOrder.back().id;
        });
        if (next == messages.end())
            break;
        historicalOrder.push_back(*next);
    }

    // TODO This should be true, but isn't.
    // CPPUNIT_ASSERT(messages.size() == historicalOrder.size());
    return historicalOrder;
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
    if (!repoAcc.repository) {
        if (!repoAcc.messages.empty()) {
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
    if (repoCommits.size() != repoAcc.messages.size()) {
        JAMI_ERROR("[{}] Repo and history don't have the same number of commits ({} vs {})",
                   repoAcc.account->getDisplayName(),
                   repoCommits.size(),
                   repoAcc.messages.size());
        JAMI_LOG("Repo commits:");
        for (const auto& commit : repoCommits) {
            bool inHistory = false;
            for (const auto& historyCommit : repoAcc.messages) {
                if (commit.id == historyCommit.id) {
                    inHistory = true;
                    break;
                }
            }
            if (inHistory) {
                JAMI_LOG("Message: {}, Author: {}, ID: {}", commit.commit_msg, commit.author.name, commit.id);
            } else {
                JAMI_ERROR("Message: {}, Author: {}, ID: {}", commit.commit_msg, commit.author.name, commit.id);
            }
        }
        return false;
    }

    // Get the commits as they are seen in the history
    std::vector<jami::ConversationCommit> historic = getHistoricalOrder(repoAcc.messages);
    // Reverse the commits so that the appear from newest-to-oldest (to match ordering of the log)
    std::reverse(historic.begin(), historic.end());
    if (repoCommits.size() != historic.size()) {
        JAMI_WARNING("[{}] Repo and sorted history don't have the same number of commits ({} vs {})",
                     repoAcc.account->getDisplayName(),
                     repoCommits.size(),
                     historic.size());
        // TODO Change this to 'false' once getHistoricalOrder() is fixed
        return true;
    }
    // We can safely access both vectors' content using the same index
    JAMI_LOG("Verifying linearized parents for account {}:", repoAcc.account->getDisplayName());
    bool allMessagesFound = true;
    for (size_t i = 0; i < historic.size(); i++) {
        if (historic[i].id == repoCommits[i].id) {
            JAMI_LOG("Same commit IDs: {}", historic[i].id);
        } else {
            JAMI_LOG("Differing commit IDS! History ({}) != Log ({})", historic[i].id, repoCommits[i].id);
            allMessagesFound = false;
        }
    }

    return allMessagesFound;
}

/**
 * @brief Load the configuration for an existing, previously-ran test. This will configure the class with with
 * accounts, a seed, and the validated event sequence.
 * @param unitTestName The file name of the unit test configuration (relative to simulation/data/)
 */
UnitTest
ConversationDST::loadUnitTestConfig(const std::string& unitTestName)
{
    JAMI_LOG("Loading unit test {}", unitTestName);
    std::filesystem::path configFile("simulation/data/" + unitTestName + ".json");
    std::string configContent = fileutils::loadTextFile(configFile);

    // Parse the content into a json object
    Json::Value unitTest;
    if (!json::parse(configContent, unitTest)) {
        JAMI_ERROR("Failed to parse unit test {}!", unitTestName);
        return UnitTest();
    }

    // Store the event seed (for logging, this will not be used anywhere else)
    eventSeed = unitTest[unitTestKeys[UTKEY::SEED]].asUInt();

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
        // Sets of parallel events are encased in array objects, we should check whether the next object
        // blocking or parllel
        const Json::Value validatedEventType = validatedEvent.type();
        if (validatedEventType == Json::ValueType::objectValue) {
            ConversationEvent convEvent = invertedEventNames[validatedEvent[unitTestKeys[UTKEY::EVENT_TYPE]].asString()];
            std::string instigatorAccountID = validatedEvent[unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]].asString();
            std::string receiverAccountID = validatedEvent[unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]].asString();
            ret.events.emplace_back(
                Event(accountIDs[instigatorAccountID], accountIDs[receiverAccountID], convEvent, 0s));
        } else if (validatedEventType == Json::ValueType::arrayValue) {
            // Get the size of the array of parallel events and iterate through each event
            int numberOfParallelEvents = static_cast<int>(validatedEvent.size());
            for (int i = 0; i < numberOfParallelEvents; i++) {
                ConversationEvent parallelConvEvent
                    = invertedEventNames[validatedEvent[i][unitTestKeys[UTKEY::EVENT_TYPE]].asString()];
                std::string instigatorAccountID = validatedEvent[i][unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]]
                                                      .asString();
                std::string receiverAccountID = validatedEvent[i][unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]].asString();
                bool expectIncomingParallel = i < numberOfParallelEvents - 1;
                ret.events.emplace_back(Event(accountIDs[instigatorAccountID],
                                              accountIDs[receiverAccountID],
                                              parallelConvEvent,
                                              0s,
                                              expectIncomingParallel));
            }
        } else {
            JAMI_ERROR("Invalid JSON object type found, please check your configuration!");
            return UnitTest();
        }
    }

    return ret;
}

/**
 * When called, this function will save the DST current configuration (including account folders).
 * Configurations get saved as a subdirectory of the configurations directory. A configuration includes:
 * - Date and time of generation
 * - Seed used
 * - Account IDs used
 * - Validated events sequence
 * @note A key with the name "desc" with an empty string as its value. If pushing the configuration to Jami
 * repository, one should replace the value with a description of what the configuration tests.
 */
void
ConversationDST::saveAsUnitTestConfig()
{
    JAMI_LOG("Saving DST configuration for seed {} as unit test...", eventSeed);

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
    for (const RepositoryAccount& repoAcc : repositoryAccounts) {
        unitTest[unitTestKeys[UTKEY::ACCOUNT_IDS]].append(repoAcc.account->getDisplayName());
    }

    // Get the validated events and store them as individual JSON objects
    unitTest[unitTestKeys[UTKEY::VALIDATED_EVENTS]] = Json::arrayValue;

    for (size_t i = 0; i < validatedEvents.size(); i++) {
        if (validatedEvents[i].expectIncomingParallelEvent) {
            // Start a parallel group
            Json::Value parallelEventArray = Json::arrayValue;
            // Add all consecutive events with expectIncomingParallelEvent == true
            while (i < validatedEvents.size() && validatedEvents[i].expectIncomingParallelEvent) {
                Json::Value eventObj;
                eventObj[unitTestKeys[UTKEY::EVENT_TYPE]] = eventNames[validatedEvents[i].type];
                eventObj[unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]]
                    = repositoryAccounts[validatedEvents[i].instigatorAccountIndex].account->getDisplayName();
                eventObj[unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]]
                    = repositoryAccounts[validatedEvents[i].receivingAccountIndex].account->getDisplayName();
                parallelEventArray.append(eventObj);
                ++i;
            }
            // Now add the first event with expectIncomingParallelEvent == false (end of group)
            if (i < validatedEvents.size()) {
                Json::Value eventObj;
                eventObj[unitTestKeys[UTKEY::EVENT_TYPE]] = eventNames[validatedEvents[i].type];
                eventObj[unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]]
                    = repositoryAccounts[validatedEvents[i].instigatorAccountIndex].account->getDisplayName();
                eventObj[unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]]
                    = repositoryAccounts[validatedEvents[i].receivingAccountIndex].account->getDisplayName();
                parallelEventArray.append(eventObj);
                ++i;
            }
            unitTest[unitTestKeys[UTKEY::VALIDATED_EVENTS]].append(parallelEventArray);
        } else {
            Json::Value validatedEventObject;
            validatedEventObject[unitTestKeys[UTKEY::EVENT_TYPE]] = eventNames[validatedEvents[i].type];
            validatedEventObject[unitTestKeys[UTKEY::INSTIGATOR_ACCOUNT_ID]]
                = repositoryAccounts[validatedEvents[i].instigatorAccountIndex].account->getDisplayName();
            validatedEventObject[unitTestKeys[UTKEY::RECEIVING_ACCOUNT_ID]]
                = repositoryAccounts[validatedEvents[i].receivingAccountIndex].account->getDisplayName();

            unitTest[unitTestKeys[UTKEY::VALIDATED_EVENTS]].append(validatedEventObject);
        }
    }

    // Convert to a stylized string (make it more legible)
    const std::string dstConfig = unitTest.toStyledString();

    const std::filesystem::path filePath(fmt::format("simulation/data/{}-{}-{}.json", dateStr, timeStr, eventSeed));

    // Create the directory if it doesn't exist
    std::filesystem::create_directories(filePath.parent_path());
    fileutils::saveFile(filePath, (const uint8_t*) dstConfig.data(), dstConfig.size());
}

/**
 * Run a given number of randomized tests and display seeds that ran with/without issues
 */
void
ConversationDST::runCycles(unsigned numCycles)
{
    std::vector<std::pair<std::random_device::result_type, bool>> seedsTested;

    // TODO Make this configurable
    int numAccountsToSimulate = 3;

    for (unsigned i = 0; i < numCycles; ++i) {
        // Generate the event sequence
        JAMI_LOG("Starting cycle {} of {}", i + 1, numCycles);
        // Generate the random seed to be used for event generation
        eventSeed = rd();
        JAMI_LOG("Random seed generated: {}", eventSeed);
        generateEventSequence(numAccountsToSimulate);
        // Do verifications
        bool ok = true;
        for (const RepositoryAccount& repoAcc : repositoryAccounts) {
            ok &= checkAppearances(repoAcc);
        }
        // Load each conversation as if it were being opened for the first time
        ok &= verifyLoadConversationFromScratch();
        // Log the summary for all the generated events
        displaySummary(numAccountsToSimulate);
        // Log all the commits in each respective repository
        displayGitLog();
        // Add the tested seed
        seedsTested.emplace_back(eventSeed, ok);
        // Save the iteration of the unit test as a configuration
        if (dstConfig.saveGenerationsAsUnitTests) {
            saveAsUnitTestConfig();
        }
        // Clear out the repositories for reuse (if enabled)
        resetRepositories();
    }

    JAMI_LOG("===================== Seeds Tested ==========================");
    for (const auto& [seed, ok] : seedsTested) {
        if (ok) {
            JAMI_LOG("PASS: {}", seed);
        } else {
            JAMI_ERROR("FAIL: {}", seed);
        }
    }

    JAMI_LOG("=================== Invalid Events Average: {:.1f}% ==========================",
             (static_cast<double>(sumOfRejectionRates) / static_cast<double>(numCycles)) * 100);
}

/**
 * @brief Runs the unit test by triggering all the events in order, specified by the loaded configuration.
 */
void
ConversationDST::runUnitTest(const UnitTest& unitTest)
{
    // Get the first event (representative of who holds the first repository)
    const Event& firstEvent = unitTest.events.front();
    // Get the account associated with the first event
    auto& firstEventAccount = repositoryAccounts[firstEvent.instigatorAccountIndex].account;
    // Create the initial conversation
    repositoryAccounts[firstEvent.instigatorAccountIndex].repository = ConversationRepository::createConversation(
        firstEventAccount);
    const std::string repoID = repositoryAccounts[firstEvent.instigatorAccountIndex].repository->id();
    auto convCommit = repositoryAccounts[firstEvent.instigatorAccountIndex].repository->getCommit(repoID);
    if (convCommit != std::nullopt) {
        repositoryAccounts[firstEvent.instigatorAccountIndex].messages.emplace_back(*convCommit);
    }

    // We skip the first event since it represents who gets their repository first
    for (size_t i = 1; i < unitTest.events.size(); i++) {
        if (unitTest.events[i].expectIncomingParallelEvent) {
            // Start with the first event parallelized
            std::vector<Event> parallelEvents = {unitTest.events[i]};
            // Insert all the remaining events to parallelize
            // A parallel block will contain a minimum of two events, so we can assume the second event as well
            // as any subsequent ones will be added
            do {
                i++;
                parallelEvents.emplace_back(unitTest.events[i]);
            } while (unitTest.events[i].expectIncomingParallelEvent && i < unitTest.events.size());
            triggerEvents(parallelEvents);
        } else {
            eventLogger(unitTest.events[i]);
            triggerEvent(unitTest.events[i]);
        }
    }

    JAMI_LOG("=========================== Summary ==========================");
    JAMI_LOG("Random seed used: {}", eventSeed);
    JAMI_LOG("Number of accounts simulated: {}", unitTest.numAccounts);
    JAMI_LOG("Number of events : {}", unitTest.events.size());
    JAMI_LOG("====================== Events ======================");
    for (const auto& e : unitTest.events) {
        eventLogger(e);
    }

    displayGitLog();
}

/**
 * @brief Runs either a unit test or a randomized cycle generation based on the configuration of the DST
 */
// void
// ConversationDST::run()
//{
//     connectSignals();
//
//     // Need to wait for devices to be registered
//     CPPUNIT_ASSERT(waitForDeviceAnnouncements());
//
//     UnitTest unitTest;
//     if (dstConfig.runUnitTest) {
//         // Load the unit test configuration and accounts prior to starting the manager
//         unitTest = loadUnitTestConfig(dstConfig.unitTestName);
//     }
//
//     // Run either a unit test or a randomized cycle generation
//     dstConfig.runUnitTest ? runUnitTest(unitTest) : runCycles();
//     for (const RepositoryAccount& repoAcc : repositoryAccounts) {
//         if (repoAcc.repository) {
//             JAMI_DEBUG("CONVERSATION_ID_EXPORT:{}", repoAcc.repository->id());
//             break;
//         }
//     }
// }

} // namespace test
} // namespace jami
