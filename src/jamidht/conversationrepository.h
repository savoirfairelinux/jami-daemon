/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

using DeviceId = dht::PkId;

constexpr auto EFETCH = 1;
constexpr auto EINVALIDMODE = 2;
constexpr auto EVALIDFETCH = 3;
constexpr auto EUNAUTHORIZED = 4;
constexpr auto ECOMMIT = 5;

class JamiAccount;

struct LogOptions
{
    std::string from {};
    std::string to {};
    uint64_t nbOfCommits {0};  // maximum number of commits wanted
    bool skipMerge {false};    // Do not include merge commits in the log. Used by the module to get
                               // last interaction without potential merges
    bool includeTo {false};    // If we want or not the "to" commit [from-to] or [from-to)
    bool fastLog {false};      // Do not parse content, used mostly to count
    bool logIfNotFound {true}; // Add a warning in the log if commit is not found

    std::string authorUri {}; // filter commits from author
};

struct Filter
{
    std::string author;
    std::string lastId;
    std::string regexSearch;
    std::string type;
    int64_t after {0};
    int64_t before {0};
    uint32_t maxResult {0};
    bool caseSensitive {false};
};

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
    MSGPACK_DEFINE(uri, role)
};

enum class CallbackResult { Skip, Break, Ok };

using PreConditionCb
    = std::function<CallbackResult(const std::string&, const GitAuthor&, const GitCommit&)>;
using PostConditionCb
    = std::function<bool(const std::string&, const GitAuthor&, ConversationCommit&)>;
using OnMembersChanged = std::function<void(const std::set<std::string>&)>;

/**
 * This class gives access to the git repository that represents the conversation
 */
class LIBJAMI_TESTABLE ConversationRepository
{
public:
    #ifdef LIBJAMI_TESTABLE
    static bool DISABLE_RESET; // Some tests inject bad files so resetHard() will break the test
    #endif
    /**
     * Creates a new repository, with initial files, where the first commit hash is the conversation id
     * @param account       The related account
     * @param mode          The wanted mode
     * @param otherMember   The other uri
     * @return  the conversation repository object
     */
    static LIBJAMI_TESTABLE std::unique_ptr<ConversationRepository> createConversation(
        const std::shared_ptr<JamiAccount>& account,
        ConversationMode mode = ConversationMode::INVITES_ONLY,
        const std::string& otherMember = "");

    /**
     * Clones a conversation on a remote device
     * @note This will use the socket registered for the conversation with JamiAccount::addGitSocket()
     * @param account           The account getting the conversation
     * @param deviceId          Remote device
     * @param conversationId    Conversation to clone
     * @param checkCommitCb     Used if commits should be treated
     */
    static LIBJAMI_TESTABLE std::unique_ptr<ConversationRepository> cloneConversation(
        const std::shared_ptr<JamiAccount>& account,
        const std::string& deviceId,
        const std::string& conversationId,
        std::function<void(std::vector<ConversationCommit>)>&& checkCommitCb = {});

    /**
     * Open a conversation repository for an account and an id
     * @param account       The related account
     * @param id            The conversation id
     */
    ConversationRepository(const std::shared_ptr<JamiAccount>& account, const std::string& id);
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
     * @param msg           The commit message of the commit
     * @param verifyDevice  If we need to validate that certificates are correct (used for testing)
     * @return <empty> on failure, else the message id
     */
    std::string commitMessage(const std::string& msg, bool verifyDevice = true);

    std::vector<std::string> commitMessages(const std::vector<std::string>& msgs);

    /**
     * Amend a commit message
     * @param id      The commit to amend
     * @param msg     The commit message of the commit
     * @return <empty> on failure, else the message id
     */
    std::string amend(const std::string& id, const std::string& msg);

    /**
     * Get commits depending on the options we pass
     * @return a list of commits
     */
    std::vector<ConversationCommit> log(const LogOptions& options = {}) const;
    void log(PreConditionCb&& preCondition,
            std::function<void(ConversationCommit&&)>&& emplaceCb,
            PostConditionCb&& postCondition,
            const std::string& from = "",
            bool logIfNotFound = true) const;
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
     * @param force         Should be false, skip validateDevice() ; used for test purpose
     * @return a pair containing if the merge was successful and the merge commit id
     * generated if one (can be a fast forward merge without commit)
     */
    std::pair<bool, std::string> merge(const std::string& merge_id, bool force = false);

    /**
     * Get the common parent between two branches
     * @param from  The first branch
     * @param to    The second branch
     * @return the common parent
     */
    std::string mergeBase(const std::string& from, const std::string& to) const;

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
    static std::vector<std::string> changedFiles(std::string_view diffStats);

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

    /**
     * The voting system is divided in two parts. The voting phase where
     * admins can decide an action (such as kicking someone)
     * and the resolving phase, when > 50% of the admins voted, we can
     * considered the vote as finished
     */
    /**
     * Add a vote to kick a device or a user
     * @param uri       identified of the user/device
     * @param type      device, members, admins or invited
     * @return the commit id or empty if failed
     */
    std::string voteKick(const std::string& uri, const std::string& type);
    /**
     * Add a vote to re-add a user
     * @param uri       identified of the user
     * @param type      device, members, admins or invited
     * @return the commit id or empty if failed
     */
    std::string voteUnban(const std::string& uri, const std::string_view type);
    /**
     * Validate if a vote is finished
     * @param uri       identified of the user/device
     * @param type      device, members, admins or invited
     * @param voteType  "ban" or "unban"
     * @return the commit id or empty if failed
     */
    std::string resolveVote(const std::string& uri,
                            const std::string_view type,
                            const std::string& voteType);

    /**
     * Validate a fetch with remote device
     * @param remotedevice
     * @return the validated commits and if an error occurs
     */
    std::pair<std::vector<ConversationCommit>, bool> validFetch(
        const std::string& remoteDevice) const;
    bool validClone(std::function<void(std::vector<ConversationCommit>)>&& checkCommitCb) const;

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
     * Get conversation's devices
     * @param ignoreExpired     If we want to ignore expired devices
     * @return members
     */
    std::map<std::string, std::vector<DeviceId>> devices(bool ignoreExpired = true) const;

    /**
     * @param filter           If we want to remove one member
     * @param filteredRoles    If we want to ignore some roles
     * @return members' uris
     */
    std::set<std::string> memberUris(std::string_view filter,
                                        const std::set<MemberRole>& filteredRoles) const;

    /**
     * To use after a merge with member's events, refresh members knowledge
     */
    void refreshMembers() const;

    void onMembersChanged(OnMembersChanged&& cb);

    /**
     * Because conversations can contains non contacts certificates, this methods
     * loads certificates in conversations into the cert store
     * @param blocking      if we need to wait that certificates are pinned
     */
    void pinCertificates(bool blocking = false);
    /**
     * Retrieve the uri from a deviceId
     * @note used by swarm manager (peersToSyncWith)
     * @param deviceId
     * @return corresponding issuer
     */
    std::string uriFromDevice(const std::string& deviceId) const;

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
        std::map<std::string, std::string>&& details);

    /**
     * Convert ConversationCommit to MapStringString for the client
     */
    std::vector<std::map<std::string, std::string>> convCommitsToMap(
        const std::vector<ConversationCommit>& commits) const;
    std::optional<std::map<std::string, std::string>> convCommitToMap(
        const ConversationCommit& commit) const;

    /**
     * Get current HEAD hash
     */
    std::string getHead() const;

private:
    ConversationRepository() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
MSGPACK_ADD_ENUM(jami::MemberRole);