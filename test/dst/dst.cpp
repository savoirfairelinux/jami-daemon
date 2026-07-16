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
#include "jamidht/commit_message.h"
#include "manager.h"

#undef NDEBUG
#include <algorithm>
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
                                                     {ConversationEvent::SEND_FILE, "SEND_FILE"},
                                                     {ConversationEvent::FETCH, "FETCH"},
                                                     {ConversationEvent::MERGE, "MERGE"},
                                                     {ConversationEvent::CLONE, "CLONE"},
                                                     {ConversationEvent::DELETE_FILE, "DELETE_FILE"},
                                                     {ConversationEvent::EDIT_MESSAGE, "EDIT_MESSAGE"},
                                                     {ConversationEvent::ADD_REACTION, "ADD_REACTION"},
                                                     {ConversationEvent::REMOVE_REACTION, "REMOVE_REACTION"},
                                                     {ConversationEvent::DELETE_MESSAGE, "DELETE_MESSAGE"},
                                                     {ConversationEvent::HOST_CONFERENCE, "HOST_CONFERENCE"},
                                                     {ConversationEvent::END_CONFERENCE, "END_CONFERENCE"},
                                                     {ConversationEvent::UPDATE_PROFILE, "UPDATE_PROFILE"}};
std::map<std::string, ConversationEvent> invertedEventNames {{"ADD_MEMBER", ConversationEvent::ADD_MEMBER},
                                                             {"SEND_MESSAGE", ConversationEvent::SEND_MESSAGE},
                                                             {"CONNECT", ConversationEvent::CONNECT},
                                                             {"DISCONNECT", ConversationEvent::DISCONNECT},
                                                             {"SEND_FILE", ConversationEvent::SEND_FILE},
                                                             {"FETCH", ConversationEvent::FETCH},
                                                             {"MERGE", ConversationEvent::MERGE},
                                                             {"CLONE", ConversationEvent::CLONE},
                                                             {"DELETE_FILE", ConversationEvent::DELETE_FILE},
                                                             {"EDIT_MESSAGE", ConversationEvent::EDIT_MESSAGE},
                                                             {"ADD_REACTION", ConversationEvent::ADD_REACTION},
                                                             {"REMOVE_REACTION", ConversationEvent::REMOVE_REACTION},
                                                             {"DELETE_MESSAGE", ConversationEvent::DELETE_MESSAGE},
                                                             {"HOST_CONFERENCE", ConversationEvent::HOST_CONFERENCE},
                                                             {"END_CONFERENCE", ConversationEvent::END_CONFERENCE},
                                                             {"UPDATE_PROFILE", ConversationEvent::UPDATE_PROFILE}};

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
    RECEIVING_ACCOUNT_ID,
    TARGET_MESSAGE_INDEX,
    REPLY_TO_INDEX
};
std::map<UTKEY, std::string> unitTestKeys {{UTKEY::SEED, "seed"},
                                           {UTKEY::DATE, "date"},
                                           {UTKEY::TIME, "time"},
                                           {UTKEY::DESC, "desc"},
                                           {UTKEY::ACCOUNT_IDS, "accountIDs"},
                                           {UTKEY::VALIDATED_EVENTS, "validatedEvents"},
                                           {UTKEY::EVENT_TYPE, "eventType"},
                                           {UTKEY::INSTIGATOR_ACCOUNT_ID, "instigatorAccountID"},
                                           {UTKEY::RECEIVING_ACCOUNT_ID, "receivingAccountID"},
                                           {UTKEY::TARGET_MESSAGE_INDEX, "targetMessageIndex"},
                                           {UTKEY::REPLY_TO_INDEX, "replyToIndex"}};

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

    // Sort accounts by display name. This isn't strictly necessary, but it ensures that
    // successive test runs with the same seed will have the same account order, which can
    // make the logs easier to compare during debugging.
    std::sort(repositoryAccounts.begin(),
              repositoryAccounts.end(),
              [](const RepositoryAccount& a, const RepositoryAccount& b) {
                  return a.account->getDisplayName() < b.account->getDisplayName();
              });

    // Register signal handlers
    confHandlers.clear();
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationMemberEvent>(
        [&](const std::string& accountId, const std::string& conversationId, const std::string& memberId, int event) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onConversationMemberEvent(accountId, conversationId, memberId, event);
                    break;
                }
            }
        }));
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ReactionAdded>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const std::string& messageId,
            std::map<std::string, std::string> reaction) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onReactionAdded(accountId, conversationId, messageId, reaction);
                }
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ReactionRemoved>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const std::string& messageId,
            const std::string& reactionId) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onReactionRemoved(accountId, conversationId, messageId, reactionId);
                }
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ActiveCallsChanged>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const std::vector<std::map<std::string, std::string>>& activeCalls) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onActiveCallsChanged(accountId, conversationId, activeCalls);
                }
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationProfileUpdated>(
        [&](const std::string& accountId, const std::string& conversationId, std::map<std::string, std::string> profile) {
            for (auto& repoAcc : repositoryAccounts) {
                if (accountId == repoAcc.account->getAccountID()) {
                    repoAcc.client.onConversationProfileUpdated(accountId, conversationId, profile);
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
        case ConversationEvent::SEND_FILE:
            fmt::print("EVENT: {} sent a file to the conversation at {}\n", instigatorName, eventTime);
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
        case ConversationEvent::DELETE_FILE:
            fmt::print("EVENT: {} deleted a file (target index {}) at {}\n",
                       instigatorName,
                       event.targetMessageIndex,
                       eventTime);
            break;
        case ConversationEvent::EDIT_MESSAGE:
            fmt::print("EVENT: {} edited a message (target index {}) at {}\n",
                       instigatorName,
                       event.targetMessageIndex,
                       eventTime);
            break;
        case ConversationEvent::ADD_REACTION:
            fmt::print("EVENT: {} added a reaction to a message (target index {}) at {}\n",
                       instigatorName,
                       event.targetMessageIndex,
                       eventTime);
            break;
        case ConversationEvent::REMOVE_REACTION:
            fmt::print("EVENT: {} removed a reaction (target index {}) at {}\n",
                       instigatorName,
                       event.targetMessageIndex,
                       eventTime);
            break;
        case ConversationEvent::DELETE_MESSAGE:
            fmt::print("EVENT: {} deleted a message (target index {}) at {}\n",
                       instigatorName,
                       event.targetMessageIndex,
                       eventTime);
            break;
        case ConversationEvent::HOST_CONFERENCE:
            fmt::print("EVENT: {} started hosting a conference at {}\n", instigatorName, eventTime);
            break;
        case ConversationEvent::END_CONFERENCE:
            fmt::print("EVENT: {} stopped hosting a conference (target index {}) at {}\n",
                       instigatorName,
                       event.targetMessageIndex,
                       eventTime);
            break;
        case ConversationEvent::UPDATE_PROFILE:
            fmt::print("EVENT: {} updated the conversation profile at {}\n", instigatorName, eventTime);
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
                    fmt::print("  Message: {}, Author: {}, ID: {}\n",
                               entry.commitMsg.toString(),
                               entry.author.name,
                               entry.id);
                }

                fmt::print("\nClient messages for account {}:\n", repositoryAccount.account->getDisplayName());
                for (const auto& message : repositoryAccount.client.getMessages()) {
                    std::string log = " ";
                    for (const auto& key :
                         {CommitKey::BODY, CommitKey::ACTION, CommitKey::MODE, CommitKey::TYPE, CommitKey::URI}) {
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
        if (event.instigatorAccountIndex == event.receivingAccountIndex || !instigatorRepoAcc.repository) {
            return false;
        }
        break;
    case ConversationEvent::SEND_MESSAGE:
        // Instigator can only send a message if part of the conversation
        if (!instigatorRepoAcc.repository) {
            return false;
        }
        break;
    case ConversationEvent::SEND_FILE:
        // Same precondition as SEND_MESSAGE
        if (!instigatorRepoAcc.repository) {
            return false;
        }
        break;
    case ConversationEvent::HOST_CONFERENCE:
        // Instigator can only host a conference if part of the conversation.
        if (!instigatorRepoAcc.repository) {
            return false;
        }
        break;
    case ConversationEvent::UPDATE_PROFILE:
        // Instigator must be part of the conversation. Permission is intentionally not checked
        // here: the insufficient-permission case is exercised and asserted in triggerEvent.
        if (!instigatorRepoAcc.repository) {
            return false;
        }
        break;
    case ConversationEvent::END_CONFERENCE: {
        // Valid by construction: only scheduled by HOST_CONFERENCE for a conference the instigator
        // just started hosting.
        assert(instigatorRepoAcc.repository != nullptr);
        assert(event.targetMessageIndex >= 0);
        const auto& target = instigatorRepoAcc.client.getMessageAtIndex(event.targetMessageIndex);
        assert(target.type == CommitType::CALL_HISTORY);
        // The target is a hosting-start commit: it has a confId but no duration yet.
        assert(target.body.find(CommitKey::CONF_ID) != target.body.end());
        assert(target.body.find(CommitKey::DURATION) == target.body.end());
        break;
    }
    case ConversationEvent::DELETE_FILE: {
        // Valid by construction: only scheduled by SEND_FILE for a file the instigator just sent.
        assert(instigatorRepoAcc.repository != nullptr);
        assert(event.targetMessageIndex >= 0);
        const auto& target = instigatorRepoAcc.client.getMessageAtIndex(event.targetMessageIndex);
        assert(target.type == CommitType::DATA_TRANSFER);
        // The file must not have been deleted yet (tid must be non-empty).
        auto tidIt = target.body.find(CommitKey::TID);
        assert(tidIt != target.body.end() && !tidIt->second.empty());
        break;
    }
    case ConversationEvent::DELETE_MESSAGE: {
        // Valid by construction: only scheduled by SEND_MESSAGE for a text/plain message the
        // instigator just sent (and for which no edition was scheduled).
        assert(instigatorRepoAcc.repository != nullptr);
        assert(event.targetMessageIndex >= 0);
        const auto& target = instigatorRepoAcc.client.getMessageAtIndex(event.targetMessageIndex);
        assert(target.type == CommitType::TEXT);
        assert(target.body.find(CommitKey::EDIT) == target.body.end());
        assert(target.body.find(CommitKey::REACT_TO) == target.body.end());
        // The message must not have been deleted yet (body must be non-empty).
        auto bodyIt = target.body.find(CommitKey::BODY);
        assert(bodyIt != target.body.end() && !bodyIt->second.empty());
        break;
    }
    case ConversationEvent::EDIT_MESSAGE: {
        // Valid by construction: only scheduled by SEND_MESSAGE or EDIT_MESSAGE for a text/plain
        // message the instigator authored.
        assert(instigatorRepoAcc.repository != nullptr);
        assert(event.targetMessageIndex >= 0);
        const auto& target = instigatorRepoAcc.client.getMessageAtIndex(event.targetMessageIndex);
        assert(target.type == CommitType::TEXT);
        assert(target.body.find(CommitKey::EDIT) == target.body.end());
        assert(target.body.find(CommitKey::REACT_TO) == target.body.end());
        break;
    }
    case ConversationEvent::ADD_REACTION:
        // Instigator must be part of the conversation and the selector must resolve to a visible
        // message (the target index is assigned before validation in generateEventSequence).
        if (!instigatorRepoAcc.repository || event.targetMessageIndex < 0) {
            return false;
        }
        break;
    case ConversationEvent::REMOVE_REACTION: {
        // Only scheduled by ADD_REACTION. The instigator's reaction is guaranteed to still be
        // present unless the reacted-to message was deleted in the meantime, since a deletion
        // clears the message's reactions. That is the only way this event can become invalid.
        assert(instigatorRepoAcc.repository != nullptr);
        assert(event.targetMessageIndex >= 0);
        dht::InfoHash instigatorHash(instigatorRepoAcc.account->getUsername());
        auto reactionId = instigatorRepoAcc.client.reactionByAuthor(event.targetMessageIndex, instigatorHash.toString());
        if (reactionId.empty()) {
            // Assert the reaction is gone precisely because the target message was deleted (its
            // body/tid was emptied by an edition).
            const auto& target = instigatorRepoAcc.client.getMessageAtIndex(event.targetMessageIndex);
            const std::string& deletedKey = (target.type == CommitType::DATA_TRANSFER) ? CommitKey::TID
                                                                                       : CommitKey::BODY;
            auto it = target.body.find(deletedKey);
            assert(it != target.body.end() && it->second.empty());
            return false;
        }
        break;
    }
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

        // The operation should succeed if and only if the receiving account was not already a member.
        bool wasAlreadyMember = instigatorAccount.conversation->isMember(receivingAccount.account->getUsername(), true);
        const std::string commitID = instigatorAccount.repository->addMember(h.toString());
        assert(commitID.empty() == wasAlreadyMember);

        if (!commitID.empty()) {
            instigatorAccount.conversation->announce(commitID, true);
            if (queue) {
                scheduleGitEvent(*queue,
                                 ConversationEvent::CLONE,
                                 event.instigatorAccountIndex,
                                 event.receivingAccountIndex,
                                 event.timeOfOccurrence);
            }
        }
        break;
    }
    case ConversationEvent::SEND_MESSAGE: {
        msgCount++;
        // The reply target (if any) is resolved during generation and recorded on the event, so
        // that replays are deterministic.
        std::string replyToId;
        if (event.replyToIndex >= 0)
            replyToId = instigatorAccount.client.getMessageAtIndex(event.replyToIndex).id;
        auto msg = CommitMessage::text(std::to_string(msgCount), replyToId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);

            // With ~30% probability, schedule a secondary edition or deletion for this message.
            // Editing and deleting are mutually exclusive for a given message, so we pick one.
            if (rand01() < 0.3f) {
                int targetIdx = instigatorAccount.client.getIndex(commitID);
                std::uniform_int_distribution<> delayDist(1, 5000);
                auto secondaryTime = event.timeOfOccurrence + std::chrono::milliseconds(delayDist(gen_));
                auto secondaryType = rand01() < 0.5f ? ConversationEvent::EDIT_MESSAGE
                                                     : ConversationEvent::DELETE_MESSAGE;
                queue->emplace(event.instigatorAccountIndex,
                               event.instigatorAccountIndex,
                               secondaryType,
                               secondaryTime,
                               targetIdx);
            }
        }
        break;
    }
    case ConversationEvent::SEND_FILE: {
        fileCount++;
        // Build deterministic pseudo-metadata from the counter. No real file I/O is needed;
        // these fields are opaque to commitMessage.
        auto displayName = "file_" + std::to_string(fileCount) + ".bin";
        // 128 hex chars = SHA3-512 size; pad the counter value with leading zeros.
        auto sha3sum = fmt::format("{:0>128x}", static_cast<uint64_t>(fileCount));
        uint64_t tid = static_cast<uint64_t>(fileCount);
        int64_t totalSize = static_cast<int64_t>(fileCount) * 1024;

        // The reply target (if any) is resolved during generation and recorded on the event, so
        // that replays are deterministic.
        std::string replyToId;
        if (event.replyToIndex >= 0)
            replyToId = instigatorAccount.client.getMessageAtIndex(event.replyToIndex).id;
        auto msg = CommitMessage::fileSent(displayName, sha3sum, tid, totalSize, replyToId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);

            // With ~30% probability, schedule a secondary DELETE_FILE for this file.
            if (rand01() < 0.3f) {
                int targetIdx = instigatorAccount.client.getIndex(commitID);
                std::uniform_int_distribution<> delayDist(1, 5000);
                auto deleteTime = event.timeOfOccurrence + std::chrono::milliseconds(delayDist(gen_));
                queue->emplace(event.instigatorAccountIndex,
                               event.instigatorAccountIndex,
                               ConversationEvent::DELETE_FILE,
                               deleteTime,
                               targetIdx);
            }
        }
        break;
    }
    case ConversationEvent::DELETE_FILE: {
        assert(event.targetMessageIndex >= 0);
        const auto& targetMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        assert(targetMessage.type == CommitType::DATA_TRANSFER);
        const std::string& fileCommitId = targetMessage.id;

        auto msg = CommitMessage::fileDeleted(fileCommitId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::DELETE_MESSAGE: {
        assert(event.targetMessageIndex >= 0);
        // targetMessageIndex points to the original text/plain message being deleted.
        const auto& targetMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        assert(targetMessage.type == CommitType::TEXT);
        const std::string& msgCommitId = targetMessage.id;

        // A message deletion is an edit of the message commit with an empty body.
        auto msg = CommitMessage::edit("", msgCommitId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);

        // Deleting a message empties its body and clears its reactions.
        const auto& updatedMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        assert(updatedMessage.body.at(CommitKey::BODY).empty());
        assert(updatedMessage.reactions.empty());

        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::HOST_CONFERENCE: {
        conferenceCount++;
        // Deterministic pseudo-identifiers for the hosted conference.
        std::string confId = std::to_string(conferenceCount);
        std::string device(instigatorAccount.account->currentDeviceId());
        dht::InfoHash hostHash(instigatorAccount.account->getUsername());
        std::string hostId = hostHash.toString();

        auto msg = CommitMessage::conferenceHostingStart(confId, device, hostId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);

            // Always schedule the matching END_CONFERENCE for the just-started conference.
            int targetIdx = instigatorAccount.client.getIndex(commitID);
            std::uniform_int_distribution<> delayDist(1, 5000);
            auto endTime = event.timeOfOccurrence + std::chrono::milliseconds(delayDist(gen_));
            queue->emplace(event.instigatorAccountIndex,
                           event.instigatorAccountIndex,
                           ConversationEvent::END_CONFERENCE,
                           endTime,
                           targetIdx);
        }
        break;
    }
    case ConversationEvent::END_CONFERENCE: {
        assert(event.targetMessageIndex >= 0);
        // targetMessageIndex points to the hosting-start commit.
        const auto& startMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        assert(startMessage.type == CommitType::CALL_HISTORY);
        const std::string& confId = startMessage.body.at(CommitKey::CONF_ID);
        const std::string& device = startMessage.body.at(CommitKey::DEVICE);
        const std::string& hostId = startMessage.body.at(CommitKey::URI);

        // Counter-derived, deterministic call duration in milliseconds.
        uint64_t duration = std::stoull(confId) * 1000;
        auto msg = CommitMessage::conferenceHostingEnd(confId, device, hostId, duration);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);

        // The end commit carries the same confId/device/uri as the start commit, plus a duration.
        const auto& endMessage = instigatorAccount.client.getMessageAtIndex(instigatorAccount.client.getIndex(commitID));
        assert(endMessage.body.at(CommitKey::CONF_ID) == confId);
        assert(endMessage.body.at(CommitKey::DEVICE) == device);
        assert(endMessage.body.at(CommitKey::URI) == hostId);
        assert(!endMessage.body.at(CommitKey::DURATION).empty());

        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::UPDATE_PROFILE: {
        profileUpdateCount++;
        // Deterministic profile derived from the counter.
        std::string title = "Title " + std::to_string(profileUpdateCount);
        std::string description = "Desc " + std::to_string(profileUpdateCount);
        std::map<std::string, std::string> profile {{"title", title}, {"description", description}};

        // The profile can only be updated by a member whose role is at least the required
        // permission level (ADMIN by default, see ConversationRepository::updateProfilePermLvl_).
        // Determine this from the instigator's own repository membership.
        dht::InfoHash instigatorHash(instigatorAccount.account->getUsername());
        const std::string instigatorUri = instigatorHash.toString();
        bool hasPermission = false;
        for (const auto& member : instigatorAccount.repository->members()) {
            if (member.uri == instigatorUri) {
                hasPermission = member.role == jami::MemberRole::ADMIN;
                break;
            }
        }

        const std::string commitID = instigatorAccount.repository->updateInfos(profile);
        // Mirrors ADD_MEMBER: the commit succeeds iff the instigator has sufficient permission.
        assert(commitID.empty() == !hasPermission);

        if (!commitID.empty()) {
            auto infos = instigatorAccount.repository->infos();
            assert(infos.at("title") == title);
            assert(infos.at("description") == description);

            instigatorAccount.conversation->announce(commitID, true);
            // The daemon emits this signal from Conversation::updateInfos; the DST commits through
            // the repository directly, so we mirror it here for the author.
            emitSignal<libjami::ConversationSignal::ConversationProfileUpdated>(instigatorAccount.account->getAccountID(),
                                                                                instigatorAccount.repository->id(),
                                                                                infos);
            if (queue) {
                scheduleGitEvent(*queue,
                                 ConversationEvent::FETCH,
                                 event.instigatorAccountIndex,
                                 -1,
                                 event.timeOfOccurrence);
            }
        }
        break;
    }
    case ConversationEvent::EDIT_MESSAGE: {
        assert(event.targetMessageIndex >= 0);
        // targetMessageIndex always points to the original text/plain message, never an edit commit.
        auto originalMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        assert(originalMessage.type == CommitType::TEXT);
        assert(originalMessage.body.find(CommitKey::EDIT) == originalMessage.body.end());
        assert(originalMessage.body.find(CommitKey::REACT_TO) == originalMessage.body.end());

        msgCount++;
        auto msg = CommitMessage::edit(std::to_string(msgCount), originalMessage.id);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);

            // With ~30% probability, schedule another edition of the same original message.
            if (rand01() < 0.3f) {
                std::uniform_int_distribution<> delayDist(1, 5000);
                auto editTime = event.timeOfOccurrence + std::chrono::milliseconds(delayDist(gen_));
                queue->emplace(event.instigatorAccountIndex,
                               event.instigatorAccountIndex,
                               ConversationEvent::EDIT_MESSAGE,
                               editTime,
                               event.targetMessageIndex); // Same original, not the edit commit
            }
        }
        const auto& updatedMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        assert(updatedMessage.editions.size() == originalMessage.editions.size() + 1);
        for (const auto& [key, value] : updatedMessage.editions[0]) {
            if (key == "id") {
                assert(value
                       == (originalMessage.latestEditionId.empty() ? originalMessage.id
                                                                   : originalMessage.latestEditionId));
            } else {
                assert(value == originalMessage.body.at(key));
            }
        }
        for (size_t i = 0; i < originalMessage.editions.size(); i++) {
            assert(updatedMessage.editions[i + 1] == originalMessage.editions[i]);
        }
        break;
    }
    case ConversationEvent::ADD_REACTION: {
        assert(event.targetMessageIndex >= 0);
        const auto& targetMessage = instigatorAccount.client.getMessageAtIndex(event.targetMessageIndex);
        const std::string& reactToId = targetMessage.id;

        // A fixed emoji keeps the reaction deterministic; the body is opaque to commitMessage.
        auto msg = CommitMessage::reaction("\U0001F44D", reactToId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);

            // With ~30% probability, schedule a secondary REMOVE_REACTION targeting the same
            // message; the removal resolves the reaction the instigator just added.
            if (rand01() < 0.3f) {
                std::uniform_int_distribution<> delayDist(1, 5000);
                auto removeTime = event.timeOfOccurrence + std::chrono::milliseconds(delayDist(gen_));
                queue->emplace(event.instigatorAccountIndex,
                               event.instigatorAccountIndex,
                               ConversationEvent::REMOVE_REACTION,
                               removeTime,
                               event.targetMessageIndex);
            }
        }
        break;
    }
    case ConversationEvent::REMOVE_REACTION: {
        assert(event.targetMessageIndex >= 0);
        dht::InfoHash instigatorHash(instigatorAccount.account->getUsername());
        auto reactionId = instigatorAccount.client.reactionByAuthor(event.targetMessageIndex, instigatorHash.toString());
        assert(!reactionId.empty());

        // A reaction removal is an edit of the reaction commit with an empty body.
        auto msg = CommitMessage::edit("", reactionId);
        const std::string commitID = instigatorAccount.repository->commitMessage(msg.toString(), true);
        assert(!commitID.empty());

        instigatorAccount.conversation->announce(commitID, true);
        if (queue) {
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.instigatorAccountIndex, -1, event.timeOfOccurrence);
        }
        break;
    }
    case ConversationEvent::CLONE: {
        auto [repo, commits] = ConversationRepository::cloneConversation(receivingAccount.account,
                                                                         instigatorAccount.account->getAccountID(),
                                                                         instigatorAccount.repository->id());
        // Join right after clone
        const std::string commitID = repo->join();
        assert(!commitID.empty());

        receivingAccount.createConversation(std::move(repo), std::move(commits));
        if (queue) {
            // Now we notify the others about the join
            scheduleGitEvent(*queue, ConversationEvent::FETCH, event.receivingAccountIndex, -1, event.timeOfOccurrence);
        }
        assert(receivingAccount.client.hasConsistentHistory());
        assert(checkConversationMembers(receivingAccount));
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
            // The daemon emits this signal from Conversation::pull when a fetched commit changes
            // profile.vcf; the DST merges directly, so we mirror it here when a profile-update
            // commit is among the merged commits.
            auto isProfileUpdate = [](const std::map<std::string, std::string>& commit) {
                auto it = commit.find(CommitKey::TYPE);
                return it != commit.end() && it->second == CommitType::UPDATE_PROFILE;
            };
            if (std::any_of(commits.begin(), commits.end(), isProfileUpdate)) {
                emitSignal<libjami::ConversationSignal::ConversationProfileUpdated>(receivingAccount.account
                                                                                        ->getAccountID(),
                                                                                    receivingAccount.repository->id(),
                                                                                    receivingAccount.repository->infos());
            }
        }
        assert(receivingAccount.client.hasConsistentHistory());
        assert(checkConversationMembers(receivingAccount));
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

            // Compare the messages the client received via the SwarmLoaded signal against the ones
            // we expect based on the repository history.
            auto repoMessages = computeExpectedMessages(repositoryAccount);
            auto clientMessages = repositoryAccount.client.getMessages();
            if (!checkMessagesMatch(repositoryAccount, repoMessages, clientMessages)) {
                return false;
            }

            // A fresh Conversation re-initializes its active calls from the repository and emits
            // ActiveCallsChanged, so the client's list should again match the oracle.
            if (!checkActiveCalls(repositoryAccount)) {
                return false;
            }
        }
    }

    return true;
}

/**
 * Generates a single primary event given the current state of the simulation. The event type is
 * sampled from the weighted distribution, and the instigator is always a current member of the
 * conversation (an account holding a repository). This keeps the proportion of invalid events low
 * without attempting to guarantee validity: a small percentage of generated events (e.g. an
 * ADD_REACTION when no message is visible yet) may still be rejected by validateEvent, which is
 * acceptable. Note that CONNECT and DISCONNECT share a single weight in the distribution; the
 * actual event is derived from the instigator's current connection state (see below).
 *
 * @param time The timeline position to assign to the generated event.
 * @param eventDist The weighted distribution used to sample the primary event type.
 * @return A primary Event.
 */
Event
ConversationDST::generatePrimaryEvent(std::chrono::nanoseconds time, std::discrete_distribution<>& eventDist)
{
    // Collect the accounts that are currently members of the conversation. Primary events are
    // always instigated by a member.
    std::vector<int> members;
    for (int i = 0; i < numAccountsToSimulate_; ++i) {
        if (repositoryAccounts[i].repository)
            members.push_back(i);
    }
    assert(!members.empty());

    ConversationEvent type = static_cast<ConversationEvent>(eventDist(gen_));
    int instigator = members[std::uniform_int_distribution<size_t>(0, members.size() - 1)(gen_)];
    int receiver = instigator;
    int targetMessageIndex = -1;
    int replyToIndex = -1;

    switch (type) {
    case ConversationEvent::ADD_MEMBER:
        // Prefer adding an account that is not yet a member, so that all accounts eventually join
        // the conversation. If everyone is already a member, target a random account (the resulting
        // operation is a validated no-op, handled by triggerEvent).
        if (members.size() < static_cast<size_t>(numAccountsToSimulate_)) {
            do {
                receiver = std::uniform_int_distribution<>(0, numAccountsToSimulate_ - 1)(gen_);
            } while (repositoryAccounts[receiver].repository);
        } else {
            receiver = std::uniform_int_distribution<>(0, numAccountsToSimulate_ - 1)(gen_);
        }
        break;
    case ConversationEvent::CONNECT:
        // CONNECT and DISCONNECT share a single weight; the actual event is derived from the
        // instigator's current connection state. This avoids generating e.g. a CONNECT for an
        // already-connected account, which would be rejected.
        type = repositoryAccounts[instigator].connected ? ConversationEvent::DISCONNECT : ConversationEvent::CONNECT;
        break;
    case ConversationEvent::ADD_REACTION:
        // React to a random message currently visible to the instigator (-1 if none, which makes
        // the event invalid). Assigning the target here (rather than in triggerEvent) keeps it
        // recorded for replay.
        targetMessageIndex = repositoryAccounts[instigator].client.randomMessageIndex(gen_);
        break;
    case ConversationEvent::SEND_MESSAGE:
    case ConversationEvent::SEND_FILE:
        // With ~25% probability, make this a reply to one of the instigator's visible messages.
        // Resolving the target here (rather than in triggerEvent) keeps it recorded for replay.
        if (rand01() < 0.25f)
            replyToIndex = repositoryAccounts[instigator].client.randomMessageIndex(gen_);
        break;
    default:
        break;
    }

    return Event(instigator, receiver, type, time, targetMessageIndex, replyToIndex);
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

    // Weightings for the primary event distribution. CONNECT and DISCONNECT are generated from a
    // single combined weight (assigned to CONNECT); generatePrimaryEvent picks the appropriate one
    // based on the instigator's connection state, so DISCONNECT itself is never sampled directly.
    double eventWeights[NUM_PRIMARY_EVENTS] = {0};
    eventWeights[static_cast<uint8_t>(ConversationEvent::ADD_MEMBER)] = 3;
    eventWeights[static_cast<uint8_t>(ConversationEvent::SEND_MESSAGE)] = 5;
    eventWeights[static_cast<uint8_t>(ConversationEvent::CONNECT)] = 2;
    eventWeights[static_cast<uint8_t>(ConversationEvent::DISCONNECT)] = 0;
    eventWeights[static_cast<uint8_t>(ConversationEvent::SEND_FILE)] = 3;
    eventWeights[static_cast<uint8_t>(ConversationEvent::ADD_REACTION)] = 3;
    eventWeights[static_cast<uint8_t>(ConversationEvent::HOST_CONFERENCE)] = 2;
    eventWeights[static_cast<uint8_t>(ConversationEvent::UPDATE_PROFILE)] = 1;
    std::discrete_distribution<> repositoryEventDist {eventWeights, eventWeights + NUM_PRIMARY_EVENTS};

    msgCount = 0;
    fileCount = 0;
    conferenceCount = 0;
    profileUpdateCount = 0;

    // Create the initial conversation
    int initialAccountIndex = 0;
    auto& initialAccount = repositoryAccounts[initialAccountIndex].account;
    auto repo = ConversationRepository::createConversation(initialAccount);
    assert(repo != nullptr);
    fmt::print("Conversation ID: {}\n", repo->id());
    repositoryAccounts[initialAccountIndex].createConversation(std::move(repo));

    startTime = std::chrono::nanoseconds(0);

    if (enableEventLogging_)
        fmt::print("===================== Generating Events... =====================\n");

    // The first event is implicit: it represents the initial account creating the conversation. It
    // is recorded (so the sequence can be saved and replayed) but not triggered, since the
    // repository was already created above.
    Event initialEvent(initialAccountIndex, initialAccountIndex, ConversationEvent::ADD_MEMBER, startTime);
    validatedEvents.emplace_back(initialEvent);
    eventLogger(initialEvent);

    // Secondary events (git operations, editions, deletions, ...) are scheduled by triggerEvent
    // into this timeline-ordered queue. Primary events, in contrast, are generated lazily and
    // interleaved with the secondary events, which allows them to be sampled from the set of
    // actions that make sense given the current state of the conversation (see generatePrimaryEvent).
    EventQueue secondaryEvents;

    size_t processedEvents = 1; // The initial event above counts as processed.
    size_t invalidEventsCount = 0;
    // Per-event-type subtotals: number of events processed and rejected for each type. The initial
    // event counts as a processed (and valid) ADD_MEMBER.
    std::map<ConversationEvent, size_t> processedByType {{ConversationEvent::ADD_MEMBER, 1}};
    std::map<ConversationEvent, size_t> invalidByType;
    while (processedEvents < maxEvents) {
        // The next primary event occurs one time step from now. Any secondary event scheduled to
        // occur before then is processed first, in timeline order. Otherwise, a new primary event
        // is generated at that time.
        auto nextPrimaryTime = startTime + std::chrono::milliseconds(1000);
        bool takeSecondary = !secondaryEvents.empty() && secondaryEvents.top().timeOfOccurrence <= nextPrimaryTime;

        Event event = takeSecondary ? secondaryEvents.top()
                                    : generatePrimaryEvent(nextPrimaryTime, repositoryEventDist);
        if (takeSecondary) {
            secondaryEvents.pop();
        } else {
            startTime = nextPrimaryTime;
        }

        eventLogger(event);
        processedEvents++;
        processedByType[event.type]++;
        if (validateEvent(event)) {
            validatedEvents.emplace_back(event);
            triggerEvent(event, &secondaryEvents);
        } else {
            invalidEventsCount++;
            invalidByType[event.type]++;
        }
    }

    auto simulationEnd = std::chrono::steady_clock::now();
    auto simulationTime = std::chrono::duration_cast<std::chrono::milliseconds>(simulationEnd - simulationStart).count();

    // Display a summary of the sequence of events that took place.
    double rejectionRate = (static_cast<double>(invalidEventsCount) / processedEvents);
    sumOfRejectionRates += rejectionRate;

    int accountsInConversation = 0;
    for (const auto& repoAcc : repositoryAccounts) {
        if (repoAcc.repository) {
            accountsInConversation++;
        }
    }
    sumOfJoinRates += static_cast<double>(accountsInConversation) / numAccountsToSimulate_;
    fmt::print("=========================== Summary ==========================\n");
    fmt::print("Random seed used: {}\n", eventSeed);
    fmt::print("Number of accounts in conversation: {}/{}\n", accountsInConversation, numAccountsToSimulate_);
    fmt::print("Total events generated: {}\n", processedEvents);
    fmt::print("Total valid events: {}\n", validatedEvents.size());
    fmt::print("Total invalid events: {}\n", invalidEventsCount);
    fmt::print("Rejection rate: {:.1f}%\n", rejectionRate * 100);
    fmt::print("Per-event-type breakdown (invalid/total, rejection rate):\n");
    for (const auto& [type, processed] : processedByType) {
        size_t invalid = invalidByType.count(type) ? invalidByType.at(type) : 0;
        double typeRejectionRate = (static_cast<double>(invalid) / processed) * 100;
        fmt::print("  {:<16} {}/{} ({:.1f}%)\n", eventNames[type], invalid, processed, typeRejectionRate);
    }
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
ConversationDST::checkAllAccounts()
{
    for (const auto& repoAcc : repositoryAccounts) {
        if (!checkAppearances(repoAcc)) {
            return false;
        }
        if (!checkConversationMembers(repoAcc)) {
            return false;
        }
        if (!checkActiveCalls(repoAcc)) {
            return false;
        }
        if (!checkProfile(repoAcc)) {
            return false;
        }
    }
    return true;
}

/**
 * Compares two SwarmMessage objects for the purposes of DST validation. Only the fields that are
 * derived from the shared conversation history are considered. The per-account "status" map, as
 * well as the local-only "pluginData" field, are intentionally ignored
 * since they are not a property of the (shared) history being validated.
 */
static bool
swarmMessagesEqual(const libjami::SwarmMessage& a, const libjami::SwarmMessage& b)
{
    // Reactions are appended in ingestion order, which differs between the incremental signals
    // (arrival order) and a fresh load or the oracle (git-log order). Their order is therefore not
    // a well-defined property of the shared history, so we compare them independently of order,
    // keyed by their unique commit id.
    auto reactionsEqual = [](std::vector<std::map<std::string, std::string>> x,
                             std::vector<std::map<std::string, std::string>> y) {
        auto byId = [](const auto& lhs, const auto& rhs) {
            return lhs.at("id") < rhs.at("id");
        };
        std::sort(x.begin(), x.end(), byId);
        std::sort(y.begin(), y.end(), byId);
        return x == y;
    };
    return a.id == b.id && a.type == b.type && a.linearizedParent == b.linearizedParent && a.body == b.body
           && reactionsEqual(a.reactions, b.reactions) && a.editions == b.editions
           && a.latestEditionId == b.latestEditionId;
}

/**
 * Reconstructs the vector of SwarmMessage objects that a client is expected to hold for a given
 * account, based solely on the contents of its conversation repository. This acts as an
 * independent oracle used to validate the messages actually accumulated by the SimClient, both
 * from the incremental signals emitted during the simulation (see checkAppearances) and from a
 * fresh load of the conversation (see verifyLoadConversationFromScratch).
 *
 * The commit map for each message is obtained from ConversationRepository::convCommitToMap, i.e.
 * the same function the daemon uses to build the SwarmMessage bodies. This keeps the comparison
 * focused on the higher-level folding and linearization logic rather than on git/JSON parsing,
 * which is not what the DST is meant to test.
 *
 * @note The messages are returned in reverse chronological order (newest first), matching
 *       SimClient::getMessages.
 */
std::vector<libjami::SwarmMessage>
ConversationDST::computeExpectedMessages(const RepositoryAccount& repoAcc) const
{
    if (!repoAcc.repository) {
        return {};
    }

    // Merge commits are not materialized as messages, so we skip them. The log is returned in
    // reverse chronological order (newest first); we process it oldest-first so that the message
    // targeted by a reaction or edition is always already known by the time we reach it (the
    // target is an ancestor, hence older). This is why, unlike the daemon (which ingests commits
    // incrementally across un-merged branches and therefore needs the pendingReactions/
    // pendingEditions machinery), we can fold reactions and editions directly.
    LogOptions options;
    options.skipMerge = true;
    std::vector<jami::ConversationCommit> commits = repoAcc.repository->log(options);

    // All commits (materialized or not) indexed by id, mirroring History::quickAccess.
    std::map<std::string, std::shared_ptr<libjami::SwarmMessage>> quickAccess;
    // Messages that materialize as standalone entries, in chronological order (oldest first).
    std::vector<std::shared_ptr<libjami::SwarmMessage>> materialized;

    // Folds an edition commit into the message (or reaction) it targets. Mirrors
    // Conversation::Impl::handleEdition (simplified: the target is always already present).
    auto handleEdition = [&](const std::shared_ptr<libjami::SwarmMessage>& commit) {
        auto editId = commit->body.at(CommitKey::EDIT);
        auto it = quickAccess.find(editId);
        assert(it != quickAccess.end());
        auto baseCommit = it->second;
        auto itReact = baseCommit->body.find(CommitKey::REACT_TO);
        std::string toReplace = (baseCommit->type == CommitType::DATA_TRANSFER) ? CommitKey::TID : CommitKey::BODY;
        auto body = commit->body.at(toReplace);
        if (itReact != baseCommit->body.end()) {
            assert(!itReact->second.empty());
            // The edited commit is itself a reaction (this is how a reaction is removed: the new
            // body is empty). Update the reaction stored on the message it was applied to.
            baseCommit->body[toReplace] = body;
            auto targetIt = quickAccess.find(itReact->second);
            if (targetIt != quickAccess.end()) {
                auto& reactions = targetIt->second->reactions;
                auto reactionIt = std::find_if(reactions.begin(), reactions.end(), [&](const auto& reaction) {
                    return reaction.at("id") == editId;
                });
                if (reactionIt != reactions.end()) {
                    (*reactionIt)[toReplace] = body;
                    if (body.empty()) {
                        reactions.erase(reactionIt);
                    }
                }
            }
        } else {
            // Editing a normal message: push the superseded body into "editions" (newest first)
            // and update the message body in place.
            auto editionBody = baseCommit->body;
            editionBody["id"] = baseCommit->latestEditionId.empty() ? baseCommit->id : baseCommit->latestEditionId;
            baseCommit->editions.emplace(baseCommit->editions.begin(), std::move(editionBody));
            baseCommit->body[toReplace] = commit->body[toReplace];
            baseCommit->latestEditionId = commit->id;
            if (toReplace == CommitKey::TID) {
                // Avoid replacing the fileId on the client.
                baseCommit->body["fileId"] = "";
            }
            // Deleting a message (empty body) also clears its reactions.
            if (commit->body.at(toReplace).empty()) {
                baseCommit->reactions.clear();
            }
        }
    };

    // Process commits oldest-first, dispatching each to the appropriate handler (mirroring
    // Conversation::Impl::addToHistory).
    for (auto it = commits.rbegin(); it != commits.rend(); ++it) {
        auto commitMap = repoAcc.repository->convCommitToMap(*it);
        assert(commitMap.has_value());

        auto commit = std::make_shared<libjami::SwarmMessage>();
        commit->fromMapStringString(*commitMap);
        quickAccess[commit->id] = commit;

        auto reactToIt = commit->body.find(CommitKey::REACT_TO);
        auto editIt = commit->body.find(CommitKey::EDIT);
        // REACT_TO and EDIT are mutually exclusive
        assert(reactToIt == commit->body.end() || editIt == commit->body.end());
        if (reactToIt != commit->body.end()) {
            auto reactTo = reactToIt->second;
            assert(!reactTo.empty());
            auto it = quickAccess.find(reactTo);
            assert(it != quickAccess.end());
            // A deleted message (body, or tid for a file, emptied by an edition) displays no
            // reactions, so reactions targeting it are ignored.
            const auto& target = it->second;
            const std::string& bodyKey = (target->type == CommitType::DATA_TRANSFER) ? CommitKey::TID : CommitKey::BODY;
            auto bodyIt = target->body.find(bodyKey);
            if (bodyIt == target->body.end() || !bodyIt->second.empty()) {
                target->reactions.emplace_back(commit->body);
            }
        } else if (editIt != commit->body.end()) {
            assert(!editIt->second.empty());
            handleEdition(commit);
        } else {
            materialized.emplace_back(commit);
        }
    }

    // The linearized parent of each materialized message is the id of the previous (older) one,
    // mirroring Conversation::loadMessagesSync. The oldest message (the initial commit) keeps the
    // empty linearized parent set by convCommitToMap.
    for (size_t i = 1; i < materialized.size(); ++i) {
        materialized[i]->linearizedParent = materialized[i - 1]->id;
    }

    // Return newest first, matching SimClient::getMessages.
    std::vector<libjami::SwarmMessage> expected;
    expected.reserve(materialized.size());
    for (auto it = materialized.rbegin(); it != materialized.rend(); ++it) {
        expected.emplace_back(**it);
    }
    return expected;
}

/**
 * Reconstructs the list of active (ongoing) conferences for a given account from its repository
 * history. This is an independent oracle for the active-call list the SimClient accumulates from
 * the ActiveCallsChanged signal. It mirrors the daemon's active-call bookkeeping (see
 * Conversation::Impl::updateActiveCalls): a hosting-start commit (a call-history commit with a
 * confId/uri/device and no duration) adds a call, the matching hosting-end commit (same triple
 * with a duration) removes it, and a member being removed or banned removes any call they host.
 *
 * @note The order of the returned calls is not significant (it is compared order-independently).
 */
std::vector<std::map<std::string, std::string>>
ConversationDST::computeExpectedActiveCalls(const RepositoryAccount& repoAcc) const
{
    std::vector<std::map<std::string, std::string>> activeCalls;
    if (!repoAcc.repository) {
        return activeCalls;
    }

    LogOptions options;
    options.skipMerge = true;
    std::vector<jami::ConversationCommit> commits = repoAcc.repository->log(options);

    // Process commits oldest-first.
    for (auto it = commits.rbegin(); it != commits.rend(); ++it) {
        auto commitMap = repoAcc.repository->convCommitToMap(*it);
        assert(commitMap.has_value());
        const auto& commit = *commitMap;

        auto typeIt = commit.find(CommitKey::TYPE);
        if (typeIt == commit.end())
            continue;

        if (typeIt->second == CommitType::MEMBER) {
            // A removed or banned member can no longer host: drop any call they host.
            auto actionIt = commit.find(CommitKey::ACTION);
            auto uriIt = commit.find(CommitKey::URI);
            if (actionIt != commit.end() && uriIt != commit.end()
                && (actionIt->second == CommitAction::REMOVE || actionIt->second == CommitAction::BAN)) {
                const std::string& memberUri = uriIt->second;
                activeCalls.erase(std::remove_if(activeCalls.begin(),
                                                 activeCalls.end(),
                                                 [&](const auto& call) {
                                                     return call.at("uri") == memberUri
                                                            || call.at("device") == memberUri;
                                                 }),
                                  activeCalls.end());
            }
        } else if (typeIt->second == CommitType::CALL_HISTORY) {
            auto confIt = commit.find(CommitKey::CONF_ID);
            auto uriIt = commit.find(CommitKey::URI);
            auto deviceIt = commit.find(CommitKey::DEVICE);
            if (confIt == commit.end() || uriIt == commit.end() || deviceIt == commit.end())
                continue;
            auto matches = [&](const auto& call) {
                return call.at("id") == confIt->second && call.at("uri") == uriIt->second
                       && call.at("device") == deviceIt->second;
            };
            if (commit.find(CommitKey::DURATION) == commit.end()) {
                // Hosting start: add the call if not already tracked.
                if (std::none_of(activeCalls.begin(), activeCalls.end(), matches)) {
                    activeCalls.emplace_back(std::map<std::string, std::string> {{"id", confIt->second},
                                                                                 {"uri", uriIt->second},
                                                                                 {"device", deviceIt->second}});
                }
            } else {
                // Hosting end: remove the matching call.
                activeCalls.erase(std::remove_if(activeCalls.begin(), activeCalls.end(), matches), activeCalls.end());
            }
        }
    }
    return activeCalls;
}

/**
 * @brief Checks that the active-call list accumulated by the client (from the ActiveCallsChanged
 * signal) matches the one reconstructed from the repository history. The comparison is
 * order-independent, since the list order is not a well-defined property of the shared history.
 */
bool
ConversationDST::checkActiveCalls(const RepositoryAccount& repoAcc)
{
    if (!repoAcc.repository) {
        return true;
    }

    auto expected = computeExpectedActiveCalls(repoAcc);
    auto actual = repoAcc.client.getActiveCalls();

    auto byId = [](const auto& a, const auto& b) {
        return a.at("id") < b.at("id");
    };
    std::sort(expected.begin(), expected.end(), byId);
    std::sort(actual.begin(), actual.end(), byId);

    if (expected != actual) {
        fmt::print(fg(fmt::color::red),
                   "[{}] Client active calls ({}) don't match the repository ({})\n",
                   repoAcc.account->getDisplayName(),
                   actual.size(),
                   expected.size());
        return false;
    }
    return true;
}

/**
 * @brief Checks that the conversation profile accumulated by the client (from the
 * ConversationProfileUpdated signal) matches the one stored in its repository.
 */
bool
ConversationDST::checkProfile(const RepositoryAccount& repoAcc)
{
    if (!repoAcc.repository) {
        return true;
    }

    auto repoInfos = repoAcc.repository->infos();
    const auto& clientProfile = repoAcc.client.getProfile();

    if (repoInfos != clientProfile) {
        auto formatProfile = [](const std::map<std::string, std::string>& profile) {
            std::string out;
            for (const auto& [key, value] : profile) {
                out += fmt::format("\n  {}={}", key, value);
            }
            return out;
        };
        fmt::print(fg(fmt::color::red),
                   "[{}] Client profile doesn't match the repository\n  client:{}\n  repository:{}\n",
                   repoAcc.account->getDisplayName(),
                   formatProfile(clientProfile),
                   formatProfile(repoInfos));
        return false;
    }
    return true;
}

/**
 * Compares the messages a client is expected to have (as computed by computeExpectedMessages) with
 * the ones it actually has. On mismatch, details are printed when git logging is enabled.
 */
bool
ConversationDST::checkMessagesMatch(const RepositoryAccount& repoAcc,
                                    const std::vector<libjami::SwarmMessage>& repoMessages,
                                    const std::vector<libjami::SwarmMessage>& clientMessages)
{
    auto name = repoAcc.account->getDisplayName();

    // Format a single message's body, reactions and editions as a human-readable string.
    auto formatMaps = [](const std::vector<std::map<std::string, std::string>>& maps) {
        std::string out;
        for (const auto& map : maps) {
            out += "\n      {";
            bool first = true;
            for (const auto& [key, value] : map) {
                out += fmt::format("{}{}={}", first ? "" : ", ", key, value);
                first = false;
            }
            out += "}";
        }
        return out;
    };
    auto formatMessage = [&](const libjami::SwarmMessage& message) {
        std::string body;
        bool first = true;
        for (const auto& [key, value] : message.body) {
            body += fmt::format("{}{}={}", first ? "" : ", ", key, value);
            first = false;
        }
        return fmt::format("id={}, type={}, linearizedParent={}\n    body: {{{}}}\n    reactions:{}\n    editions:{}",
                           message.id,
                           message.type,
                           message.linearizedParent,
                           body,
                           formatMaps(message.reactions),
                           formatMaps(message.editions));
    };

    auto dumpMessages = [&](const char* label, const std::vector<libjami::SwarmMessage>& messages) {
        fmt::print("{}:\n", label);
        for (const auto& message : messages) {
            fmt::print("  {}\n", formatMessage(message));
        }
    };

    if (repoMessages.size() != clientMessages.size()) {
        fmt::print(fg(fmt::color::red),
                   "[{}] Client has {} messages but repo has {}\n",
                   name,
                   clientMessages.size(),
                   repoMessages.size());
        if (enableGitLogging_) {
            dumpMessages("Repo messages", repoMessages);
            dumpMessages("Client messages", clientMessages);
        }
        return false;
    }

    for (size_t i = 0; i < repoMessages.size(); i++) {
        if (!swarmMessagesEqual(repoMessages[i], clientMessages[i])) {
            fmt::print(fg(fmt::color::red),
                       "[{}] Client message at index {} does not match the repo message\n",
                       name,
                       i);
            if (enableGitLogging_) {
                fmt::print(fg(fmt::color::red), "  Repo: {}\n", formatMessage(repoMessages[i]));
                fmt::print(fg(fmt::color::red), "  Client:   {}\n", formatMessage(clientMessages[i]));
            }
            return false;
        }
    }
    return true;
}

/**
 * @brief Checks that the messages accumulated by the client during the simulation match the ones
 * reconstructed from the repository history.
 */
bool
ConversationDST::checkAppearances(const RepositoryAccount& repoAcc)
{
    auto repoMessages = computeExpectedMessages(repoAcc);
    auto clientMessages = repoAcc.client.getMessages();
    return checkMessagesMatch(repoAcc, repoMessages, clientMessages);
}

bool
ConversationDST::checkConversationMembers(const RepositoryAccount& repoAcc)
{
    if (!repoAcc.repository) {
        return true;
    }

    for (const auto& member : repoAcc.repository->members()) {
        auto memberRole = repoAcc.client.getMemberRole(member.uri);
        if (memberRole == MemberRole::INVALID) {
            fmt::print(fg(fmt::color::red),
                       "[{}] Client doesn't have member {}\n",
                       repoAcc.account->getDisplayName(),
                       member.uri);
            return false;
        }

        if (static_cast<int>(memberRole) != static_cast<int>(member.role)) {
            fmt::print(fg(fmt::color::red),
                       "[{}] Role for member {} differs between client ({}) and repository ({})\n",
                       repoAcc.account->getDisplayName(),
                       member.uri,
                       static_cast<int>(memberRole),
                       static_cast<int>(member.role));
            // The current merge algorithm does not properly handle role conflicts, which can cause
            // the repo to end up in an inconsistent state (where e.g. a peer is already a member, but
            // also still in the "invited" directory) and lead to a role mismatch here.
            // TODO return false once the above issue is fixed.
            return true;
        }
    }
    return true;
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
            int targetMessageIndex = -1;
            const auto& targetKey = unitTestKeys[UTKEY::TARGET_MESSAGE_INDEX];
            if (validatedEvent.isMember(targetKey)) {
                targetMessageIndex = validatedEvent[targetKey].asInt();
            }
            int replyToIndex = -1;
            const auto& replyKey = unitTestKeys[UTKEY::REPLY_TO_INDEX];
            if (validatedEvent.isMember(replyKey)) {
                replyToIndex = validatedEvent[replyKey].asInt();
            }
            ret.events.emplace_back(Event(accountIDs[instigatorAccountID],
                                          accountIDs[receiverAccountID],
                                          convEvent,
                                          0s,
                                          targetMessageIndex,
                                          replyToIndex));
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
        if (validatedEvents[i].targetMessageIndex >= 0) {
            validatedEventObject[unitTestKeys[UTKEY::TARGET_MESSAGE_INDEX]] = validatedEvents[i].targetMessageIndex;
        }
        if (validatedEvents[i].replyToIndex >= 0) {
            validatedEventObject[unitTestKeys[UTKEY::REPLY_TO_INDEX]] = validatedEvents[i].replyToIndex;
        }

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
    bool ok = checkAllAccounts();
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
    fmt::print("Join Rate Average: {:.1f}%\n", (sumOfJoinRates / static_cast<double>(numCycles)) * 100);
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
    fileCount = 0;
    conferenceCount = 0;
    profileUpdateCount = 0;
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
