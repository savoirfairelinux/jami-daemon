/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <optional>
#include <git2.h>
#include <memory>
#include <opendht/default_types.h>
#include <string>
#include <vector>

#include "def.h"

using GitPackBuilder = std::unique_ptr<git_packbuilder, decltype(&git_packbuilder_free)>;
using GitRepository = std::unique_ptr<git_repository, decltype(&git_repository_free)>;
using GitRevWalker = std::unique_ptr<git_revwalk, decltype(&git_revwalk_free)>;
using GitCommit = std::unique_ptr<git_commit, decltype(&git_commit_free)>;
using GitAnnotatedCommit
    = std::unique_ptr<git_annotated_commit, decltype(&git_annotated_commit_free)>;
using GitIndex = std::unique_ptr<git_index, decltype(&git_index_free)>;
using GitTree = std::unique_ptr<git_tree, decltype(&git_tree_free)>;
using GitRemote = std::unique_ptr<git_remote, decltype(&git_remote_free)>;
using GitReference = std::unique_ptr<git_reference, decltype(&git_reference_free)>;
using GitSignature = std::unique_ptr<git_signature, decltype(&git_signature_free)>;
using GitObject = std::unique_ptr<git_object, decltype(&git_object_free)>;
using GitDiff = std::unique_ptr<git_diff, decltype(&git_diff_free)>;
using GitDiffStats = std::unique_ptr<git_diff_stats, decltype(&git_diff_stats_free)>;
using GitIndexConflictIterator
    = std::unique_ptr<git_index_conflict_iterator, decltype(&git_index_conflict_iterator_free)>;

namespace jami {

class JamiAccount;
class ChannelSocket;

struct GitAuthor
{
    std::string name {};
    std::string email {};
};

struct ConversationCommit
{
    std::string id {};
    std::vector<std::string> parents {};
    GitAuthor author {};
    std::vector<uint8_t> signed_content {};
    std::vector<uint8_t> signature {};
    std::string commit_msg {};
    int64_t timestamp {0};
};

/**
 * This class gives access to the git repository that represents the conversation
 */
class DRING_TESTABLE ConversationRepository
{
public:
    /**
     * Creates a new repository, with initial files, where the first commit hash is the conversation id
     * @param account       The related account
     * @return  the conversation repository object
     */
    static DRING_TESTABLE std::unique_ptr<ConversationRepository> createConversation(
        const std::weak_ptr<JamiAccount>& account);

    /**
     * Clones a conversation on a remote device
     * @note This will use the socket registered for the conversation with JamiAccount::addGitSocket()
     * @param account           The account getting the conversation
     * @param deviceId          Remote device
     * @param conversationId    Conversation to clone
     * @param socket            Socket used to clone
     */
    static DRING_TESTABLE std::unique_ptr<ConversationRepository> cloneConversation(
        const std::weak_ptr<JamiAccount>& account,
        const std::string& deviceId,
        const std::string& conversationId);

    /**
     * Open a conversation repository for an account and an id
     * @param account       The related account
     * @param id            The conversation id
     */
    ConversationRepository(const std::weak_ptr<JamiAccount>& account, const std::string& id);
    ~ConversationRepository();

    /**
     * Write the certificate in /members and commit the change
     * @param uri    Member to add
     * @return the commit id if successful
     */
    std::string addMember(const std::string& uri);

    /**
     * Fetch a remote repository via the given socket
     * @note This will use the socket registered for the conversation with JamiAccount::addGitSocket()
     * @note will create a remote identified by the deviceId
     * @param remoteDeviceId    Remote device id to fetch
     * @return if the operation was successful
     */
    bool fetch(const std::string& remoteDeviceId);

    /**
     * Retrieve remote head. Can be useful after a fetch operation
     * @param remoteDeviceId        The remote name
     * @param branch                Remote branch to check (default: main)
     * @return the commit id pointed
     */
    std::string remoteHead(const std::string& remoteDeviceId, const std::string& branch = "main");

    /**
     * Return the conversation id
     */
    const std::string& id() const;

    /**
     * Add a new commit to the conversation
     * @param msg     The commit message of the commit
     * @return <empty> on failure, else the message id
     */
    std::string commitMessage(const std::string& msg);

    /**
     * Get commits from [last-n, last]
     * @param last  last commit (default empty)
     * @param n     Max commits number to get (default: 0)
     * @return a list of commits
     */
    std::vector<ConversationCommit> logN(const std::string& last = "", unsigned n = 0) const;
    std::vector<ConversationCommit> log(const std::string& from = "",
                                        const std::string& to = "") const;
    std::optional<ConversationCommit> getCommit(const std::string& commitId) const;

    /**
     * Merge another branch into the main branch
     * @param merge_id      The reference to merge
     * @return if the merge was successful
     */
    bool merge(const std::string& merge_id);

    /**
     * Get current diff stats between two commits
     * @param oldId     Old commit
     * @param newId     Recent commit (empty value will compare to the empty repository)
     * @note "HEAD" is also accepted as parameter for newId
     * @return diff stats
     */
    std::string diffStats(const std::string& newId, const std::string& oldId = "") const;

    /**
     * Get changed files from a git diff
     * @param diffStats     The stats to analyze
     * @return get the changed files from a git diff
     */
    static std::vector<std::string> changedFiles(const std::string_view& diffStats);

    /**
     * Join a repository
     * @return commit Id
     */
    std::string join();

    /**
     * Erase self from repository
     * @return commit Id
     */
    std::string leave();

    /**
     * Erase repository
     */
    void erase();

private:
    ConversationRepository() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami