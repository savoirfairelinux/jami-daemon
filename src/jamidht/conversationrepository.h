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

constexpr auto EFETCH = 1;
constexpr auto EINVALIDMODE = 2;
constexpr auto EVALIDFETCH = 3;
constexpr auto EUNAUTHORIZED = 4;

class JamiAccount;
class ChannelSocket;

struct GitAuthor
{
    std::string name {};
    std::string email {};
};

enum class ConversationMode : int { ONE_TO_ONE = 0, ADMIN_INVITES_ONLY, INVITES_ONLY, PUBLIC };

struct ConversationCommit
{
    std::string id {};
    std::vector<std::string> parents {};
    GitAuthor author {};
    std::vector<uint8_t> signed_content {};
    std::vector<uint8_t> signature {};
    std::string commit_msg {};
    std::string linearized_parent {};
    int64_t timestamp {0};
};

enum class MemberRole { ADMIN = 0, MEMBER, INVITED, BANNED, LEFT };

struct ConversationMember
{
    std::string uri;
    MemberRole role;

    std::map<std::string, std::string> map() const
    {
        std::string rolestr;
        if (role == MemberRole::ADMIN) {
            rolestr = "admin";
        } else if (role == MemberRole::MEMBER) {
            rolestr = "member";
        } else if (role == MemberRole::INVITED) {
            rolestr = "invited";
        } else if (role == MemberRole::BANNED) {
            rolestr = "banned";
        } else if (role == MemberRole::LEFT) {
            rolestr = "left"; // For one to one
        }

        return {{"uri", uri}, {"role", rolestr}};
    }
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
     * @param mode          The wanted mode
     * @param otherMember   The other uri
     * @return  the conversation repository object
     */
    static DRING_TESTABLE std::unique_ptr<ConversationRepository> createConversation(
        const std::weak_ptr<JamiAccount>& account,
        ConversationMode mode = ConversationMode::INVITES_ONLY,
        const std::string& otherMember = "");

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
    std::string remoteHead(const std::string& remoteDeviceId,
                           const std::string& branch = "main") const;

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

    std::vector<std::string> commitMessages(const std::vector<std::string>& msgs);

    /**
     * Amend a commit message
     * @param id      The commit to amend
     * @param msg     The commit message of the commit
     * @return <empty> on failure, else the message id
     */
    std::string amend(const std::string& id, const std::string& msg);

    /**
     * Get commits from [last-n, last]
     * @param last              last commit (default empty)
     * @param n                 Max commits number to get (default: 0)
     * @param logIfNotFound     Log if commit is found (false for detect commit before pulling)
     * @param fastLog           Used to count commits. This will just generate empty ConversationCommit
     * @param authorUri         used to stop counting if detected with fast log
     * @return a list of commits
     */
    std::vector<ConversationCommit> logN(const std::string& last = "",
                                         unsigned n = 0,
                                         bool logIfNotFound = true) const;
    std::vector<ConversationCommit> log(const std::string& from = "",
                                        const std::string& to = "",
                                        bool logIfNotFound = true,
                                        bool fastLog = false,
                                        const std::string& authorUri = "") const;
    std::optional<ConversationCommit> getCommit(const std::string& commitId,
                                                bool logIfNotFound = true) const;

    /**
     * Get parent via topological + date sort in branch main of a commit
     * @param commitId      id to choice
     */
    std::optional<std::string> linearizedParent(const std::string& commitId) const;

    /**
     * Merge another branch into the main branch
     * @param merge_id      The reference to merge
     * @return a pair containing if the merge was successful and the merge commit id
     * generated if one (can be a fast forward merge without commit)
     */
    std::pair<bool, std::string> merge(const std::string& merge_id);

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

    /**
     * Get conversation's mode
     * @return the mode
     */
    ConversationMode mode() const;

    std::string voteKick(const std::string& uri, bool isDevice);
    std::string resolveVote(const std::string& uri, bool isDevice);

    /**
     * Validate a fetch with remote device
     * @param remotedevice
     * @return the validated commits and if an error occurs
     */
    std::pair<std::vector<ConversationCommit>, bool> validFetch(
        const std::string& remoteDevice) const;
    bool validClone() const;

    /**
     * Delete branch with remote
     * @param remoteDevice
     */
    void removeBranchWith(const std::string& remoteDevice);

    /**
     * One to one util, get initial members
     * @return initial members
     */
    std::vector<std::string> getInitialMembers() const;

    /**
     * Get conversation's members
     * @return members
     */
    std::vector<ConversationMember> members() const;

    /**
     * @param filter           If we want to remove one member
     * @param filteredRoles    If we want to ignore some roles
     * @return members' uris
     */
    std::vector<std::string> memberUris(std::string_view filter,
                                        const std::set<MemberRole>& filteredRoles) const;

    /**
     * To use after a merge with member's events, refresh members knowledge
     */
    void refreshMembers() const;

    /**
     * Because conversations can contains non contacts certificates, this methods
     * loads certificates in conversations into the cert store
     * @param blocking      if we need to wait that certificates are pinned
     */
    void pinCertificates(bool blocking = false);

    /**
     * Change repository's infos
     * @param map       New infos (supported keys: title, description, avatar)
     * @return the commit id
     */
    std::string updateInfos(const std::map<std::string, std::string>& map);

    /**
     * Retrieve current infos (title, description, avatar, mode)
     * @return infos
     */
    std::map<std::string, std::string> infos() const;
    static std::map<std::string, std::string> infosFromVCard(
        const std::map<std::string, std::string>& details);

private:
    ConversationRepository() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami