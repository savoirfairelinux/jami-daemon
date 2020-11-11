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
#include "conversationrepository.h"

#include "account_const.h"
#include "base64.h"
#include "jamiaccount.h"
#include "fileutils.h"
#include "gittransport.h"
#include "string_utils.h"
#include "client/ring_signal.h"
#include "vcard.h"

using random_device = dht::crypto::random_device;

#include <ctime>
#include <fstream>
#include <json/json.h>
#include <regex>
#include <exception>
#include <optional>

using namespace std::string_view_literals;
constexpr auto DIFF_REGEX = " +\\| +[0-9]+.*"sv;
constexpr size_t MAX_FETCH_SIZE {256 * 1024 * 1024}; // 256Mb

namespace jami {

class ConversationRepository::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& id)
        : account_(account)
        , id_(id)
    {
        initMembers();
    }

    // NOTE! We use temporary GitRepository to avoid to keep file opened (TODO check why
    // git_remote_fetch() leaves pack-data opened)
    GitRepository repository() const
    {
        auto shared = account_.lock();
        if (!shared)
            return {nullptr, git_repository_free};
        auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + id_;
        git_repository* repo = nullptr;
        if (git_repository_open(&repo, path.c_str()) != 0)
            return {nullptr, git_repository_free};
        return {std::move(repo), git_repository_free};
    }

    GitSignature signature();
    bool mergeFastforward(const git_oid* target_oid, int is_unborn);
    std::string createMergeCommit(git_index* index, const std::string& wanted_ref);

    bool validCommits(const std::vector<ConversationCommit>& commits) const;
    bool checkOnlyDeviceCertificate(const std::string& userDevice,
                                    const std::string& commitId,
                                    const std::string& parentId) const;
    bool checkVote(const std::string& userDevice,
                   const std::string& commitId,
                   const std::string& parentId) const;
    bool isValidUserAtCommit(const std::string& userDevice, const std::string& commitId) const;
    bool checkInitialCommit(const std::string& userDevice, const std::string& commitId) const;
    bool checkValidAdd(const std::string& userDevice,
                       const std::string& uriMember,
                       const std::string& commitid,
                       const std::string& parentId) const;
    bool checkValidJoins(const std::string& userDevice,
                         const std::string& uriMember,
                         const std::string& commitid,
                         const std::string& parentId) const;
    bool checkValidRemove(const std::string& userDevice,
                          const std::string& uriMember,
                          const std::string& commitid,
                          const std::string& parentId) const;
    bool checkValidProfileUpdate(const std::string& userDevice,
                                 const std::string& commitid,
                                 const std::string& parentId) const;

    bool add(const std::string& path);
    std::string commit(const std::string& msg);
    ConversationMode mode() const;

    // NOTE! GitDiff needs to be deteleted before repo
    GitDiff diff(git_repository* repo, const std::string& idNew, const std::string& idOld) const;
    std::string diffStats(const std::string& newId, const std::string& oldId) const;
    std::string diffStats(const GitDiff& diff) const;

    std::vector<ConversationCommit> behind(const std::string& from) const;
    std::vector<ConversationCommit> log(const std::string& from,
                                        const std::string& to,
                                        unsigned n,
                                        bool logIfNotFound = false) const;
    std::optional<std::string> linearizedParent(const std::string& commitId) const;

    GitObject fileAtTree(const std::string& path, const GitTree& tree) const;
    // NOTE! GitDiff needs to be deteleted before repo
    GitTree treeAtCommit(git_repository* repo, const std::string& commitId) const;
    std::string getCommitType(const std::string& commitMsg) const;

    std::vector<std::string> getInitialMembers() const;

    std::weak_ptr<JamiAccount> account_;
    const std::string id_;
    mutable std::optional<ConversationMode> mode_ {};

    // Members utils
    mutable std::mutex membersMtx_ {};
    std::vector<ConversationMember> members_ {};

    std::vector<ConversationMember> members() const
    {
        std::lock_guard<std::mutex> lk(membersMtx_);
        return members_;
    }

    bool resolveConflicts(git_index* index, const std::string& other_id);

    void initMembers();

    // Permissions
    MemberRole updateProfilePermLvl_ {MemberRole::ADMIN};
};

/////////////////////////////////////////////////////////////////////////////////

/**
 * Creates an empty repository
 * @param path       Path of the new repository
 * @return The libgit2's managed repository
 */
GitRepository
create_empty_repository(const std::string& path)
{
    git_repository* repo = nullptr;
    git_repository_init_options opts;
    git_repository_init_options_init(&opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
    opts.flags |= GIT_REPOSITORY_INIT_MKPATH;
    opts.initial_head = "main";
    if (git_repository_init_ext(&repo, path.c_str(), &opts) < 0) {
        JAMI_ERR("Couldn't create a git repository in %s", path.c_str());
    }
    return {std::move(repo), git_repository_free};
}

/**
 * Add all files to index
 * @param repo
 * @return if operation is successful
 */
bool
git_add_all(git_repository* repo)
{
    // git add -A
    git_index* index_ptr = nullptr;
    git_strarray array {nullptr, 0};
    if (git_repository_index(&index_ptr, repo) < 0) {
        JAMI_ERR("Could not open repository index");
        return false;
    }
    GitIndex index {index_ptr, git_index_free};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    return true;
}

/**
 * Adds initial files. This adds the certificate of the account in the /admins directory
 * the device's key in /devices and the CRLs in /CRLs.
 * @param repo      The repository
 * @return if files were added successfully
 */
bool
add_initial_files(GitRepository& repo, const std::shared_ptr<JamiAccount>& account)
{
    auto deviceId = account->currentDeviceId();
    std::string repoPath = git_repository_workdir(repo.get());
    std::string adminsPath = repoPath + "admins";
    std::string devicesPath = repoPath + "devices";
    std::string crlsPath = repoPath + "CRLs" + DIR_SEPARATOR_STR + deviceId;

    if (!fileutils::recursive_mkdir(adminsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", adminsPath.c_str());
        return false;
    }

    auto cert = account->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERR("Parent cert is null!");
        return false;
    }

    // /admins
    std::string adminPath = adminsPath + DIR_SEPARATOR_STR + parentCert->getId().toString()
                            + ".crt";
    auto file = fileutils::ofstream(adminPath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", adminPath.c_str());
        return false;
    }
    file << parentCert->toString(true);
    file.close();

    if (!fileutils::recursive_mkdir(devicesPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", devicesPath.c_str());
        return false;
    }

    // /devices
    std::string devicePath = devicesPath + DIR_SEPARATOR_STR + cert->getId().toString() + ".crt";
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", devicePath.c_str());
        return false;
    }
    file << deviceCert;
    file.close();

    if (!fileutils::recursive_mkdir(crlsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", crlsPath.c_str());
        return false;
    }

    // /CRLs
    for (const auto& crl : account->identity().second->getRevocationLists()) {
        if (!crl)
            continue;
        std::string crlPath = crlsPath + DIR_SEPARATOR_STR + deviceId + DIR_SEPARATOR_STR + dht::toHex(crl->getNumber())
                              + ".crl";
        file = fileutils::ofstream(crlPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERR("Could not write data to %s", crlPath.c_str());
            return false;
        }
        file << crl->toString();
        file.close();
    }

    if (!git_add_all(repo.get())) {
        return false;
    }

    JAMI_INFO("Initial files added in %s", repoPath.c_str());
    return true;
}

/**
 * Sign and create the initial commit
 * @param repo          The git repository
 * @param account       The account who signs
 * @param mode          The mode
 * @param otherMember   If one to one
 * @return          The first commit hash or empty if failed
 */
std::string
initial_commit(GitRepository& repo,
               const std::shared_ptr<JamiAccount>& account,
               ConversationMode mode,
               const std::string& otherMember = "")
{
    auto deviceId = std::string(account->currentDeviceId());
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    git_index* index_ptr = nullptr;
    git_oid tree_id, commit_id;
    git_tree* tree_ptr = nullptr;
    git_buf to_sign = {};

    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr, git_signature_free};

    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
        return {};
    }
    GitIndex index {index_ptr, git_index_free};

    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERR("Unable to write initial tree from index");
        return {};
    }

    if (git_tree_lookup(&tree_ptr, repo.get(), &tree_id) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    Json::Value json;
    json["mode"] = static_cast<int>(mode);
    if (mode == ConversationMode::ONE_TO_ONE) {
        json["invited"] = otherMember;
    }
    json["type"] = "initial";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 Json::writeString(wbuilder, json).c_str(),
                                 tree.get(),
                                 0,
                                 nullptr)
        < 0) {
        JAMI_ERR("Could not create initial buffer");
        return {};
    }

    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);

    // git commit -S
    if (git_commit_create_with_signature(&commit_id,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Could not sign initial commit");
        return {};
    }

    // Move commit to main branch
    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, repo.get(), &commit_id) == 0) {
        git_reference* ref = nullptr;
        git_branch_create(&ref, repo.get(), "main", commit, true);
        git_commit_free(commit);
        git_reference_free(ref);
    }

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str)
        return commit_str;
    return {};
}

//////////////////////////////////

GitSignature
ConversationRepository::Impl::signature()
{
    auto account = account_.lock();
    if (!account)
        return {nullptr, git_signature_free};
    auto deviceId = std::string(account->currentDeviceId());
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERR("Unable to create a commit signature.");
        return {nullptr, git_signature_free};
    }
    return {sig_ptr, git_signature_free};
}

std::string
ConversationRepository::Impl::createMergeCommit(git_index* index, const std::string& wanted_ref)
{
    // The merge will occur between current HEAD and wanted_ref
    git_reference* head_ref_ptr = nullptr;
    auto repo = repository();
    if (git_repository_head(&head_ref_ptr, repo.get()) < 0) {
        JAMI_ERR("Could not get HEAD reference");
        return {};
    }
    GitReference head_ref {head_ref_ptr, git_reference_free};

    // Maybe that's a ref, so DWIM it
    git_reference* merge_ref_ptr = nullptr;
    git_reference_dwim(&merge_ref_ptr, repo.get(), wanted_ref.c_str());
    GitReference merge_ref {merge_ref_ptr, git_reference_free};

    GitSignature sig {signature()};

    // Prepare a standard merge commit message
    const char* msg_target = nullptr;
    if (merge_ref) {
        git_branch_name(&msg_target, merge_ref.get());
    } else {
        msg_target = wanted_ref.c_str();
    }

    std::stringstream stream;
    stream << "Merge " << (merge_ref ? "branch" : "commit") << " '" << msg_target << "'";

    // Setup our parent commits
    GitCommit parents[2] {{nullptr, git_commit_free}, {nullptr, git_commit_free}};
    git_commit* parent = nullptr;
    if (git_reference_peel((git_object**) &parent, head_ref.get(), GIT_OBJ_COMMIT) < 0) {
        JAMI_ERR("Could not peel HEAD reference");
        return {};
    }
    parents[0] = {parent, git_commit_free};
    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, wanted_ref.c_str()) < 0) {
        return {};
    }
    git_annotated_commit* annotated_ptr = nullptr;
    if (git_annotated_commit_lookup(&annotated_ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERR("Couldn't lookup commit %s", wanted_ref.c_str());
        return {};
    }
    GitAnnotatedCommit annotated {annotated_ptr, git_annotated_commit_free};
    if (git_commit_lookup(&parent, repo.get(), git_annotated_commit_id(annotated.get())) < 0) {
        JAMI_ERR("Couldn't lookup commit %s", wanted_ref.c_str());
        return {};
    }
    parents[1] = {parent, git_commit_free};

    // Prepare our commit tree
    git_oid tree_oid;
    git_tree* tree = nullptr;
    if (git_index_write_tree_to(&tree_oid, index, repo.get()) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("XXX checkout index: %s", err->message);
        JAMI_ERR("Couldn't write index");
        return {};
    }
    if (git_tree_lookup(&tree, repo.get(), &tree_oid) < 0) {
        JAMI_ERR("Couldn't lookup tree");
        return {};
    }

    // Commit
    git_buf to_sign = {};
    const git_commit* parents_ptr[2] {parents[0].get(), parents[1].get()};
    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 stream.str().c_str(),
                                 tree,
                                 2,
                                 &parents_ptr[0])
        < 0) {
        JAMI_ERR("Could not create commit buffer");
        return {};
    }

    auto account = account_.lock();
    if (!account)
        return {};
    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    git_oid commit_oid;
    if (git_commit_create_with_signature(&commit_oid,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Could not sign commit");
        return {};
    }

    auto commit_str = git_oid_tostr_s(&commit_oid);
    if (commit_str) {
        JAMI_INFO("New merge commit added with id: %s", commit_str);
        // Move commit to main branch
        git_reference* ref_ptr = nullptr;
        if (git_reference_create(&ref_ptr, repo.get(), "refs/heads/main", &commit_oid, true, nullptr)
            < 0) {
            JAMI_WARN("Could not move commit to main");
        }
        git_reference_free(ref_ptr);
    }

    // We're done merging, cleanup the repository state & index
    git_repository_state_cleanup(repo.get());

    git_object* target_ptr = nullptr;
    if (git_object_lookup(&target_ptr, repo.get(), &commit_oid, GIT_OBJ_COMMIT) != 0) {
        JAMI_ERR("failed to lookup OID %s", git_oid_tostr_s(&commit_oid));
        return {};
    }
    GitObject target {target_ptr, git_object_free};

    git_reset(repo.get(), target.get(), GIT_RESET_HARD, nullptr);

    return commit_str ? commit_str : "";
}

bool
ConversationRepository::Impl::mergeFastforward(const git_oid* target_oid, int is_unborn)
{
    // Initialize target
    git_reference* target_ref_ptr = nullptr;
    auto repo = repository();
    if (is_unborn) {
        git_reference* head_ref_ptr = nullptr;
        // HEAD reference is unborn, lookup manually so we don't try to resolve it
        if (git_reference_lookup(&head_ref_ptr, repo.get(), "HEAD") < 0) {
            JAMI_ERR("failed to lookup HEAD ref");
            return false;
        }
        GitReference head_ref {head_ref_ptr, git_reference_free};

        // Grab the reference HEAD should be pointing to
        const auto* symbolic_ref = git_reference_symbolic_target(head_ref.get());

        // Create our main reference on the target OID
        if (git_reference_create(&target_ref_ptr, repo.get(), symbolic_ref, target_oid, 0, nullptr)
            < 0) {
            JAMI_ERR("failed to create main reference");
            return false;
        }

    } else if (git_repository_head(&target_ref_ptr, repo.get()) < 0) {
        // HEAD exists, just lookup and resolve
        JAMI_ERR("failed to get HEAD reference");
        return false;
    }
    GitReference target_ref {target_ref_ptr, git_reference_free};

    // Lookup the target object
    git_object* target_ptr = nullptr;
    if (git_object_lookup(&target_ptr, repo.get(), target_oid, GIT_OBJ_COMMIT) != 0) {
        JAMI_ERR("failed to lookup OID %s", git_oid_tostr_s(target_oid));
        return false;
    }
    GitObject target {target_ptr, git_object_free};

    // Checkout the result so the workdir is in the expected state
    git_checkout_options ff_checkout_options;
    git_checkout_init_options(&ff_checkout_options, GIT_CHECKOUT_OPTIONS_VERSION);
    ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
    if (git_checkout_tree(repo.get(), target.get(), &ff_checkout_options) != 0) {
        JAMI_ERR("failed to checkout HEAD reference");
        return false;
    }

    // Move the target reference to the target OID
    git_reference* new_target_ref;
    if (git_reference_set_target(&new_target_ref, target_ref.get(), target_oid, nullptr) < 0) {
        JAMI_ERR("failed to move HEAD reference");
        return false;
    }
    git_reference_free(new_target_ref);

    return true;
}

bool
ConversationRepository::Impl::add(const std::string& path)
{
    auto repo = repository();
    if (!repo)
        return false;
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
        return false;
    }
    GitIndex index {index_ptr, git_index_free};
    if (git_index_add_bypath(index.get(), path.c_str()) != 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("Error when adding file: %s", err->message);
        return false;
    }
    return git_index_write(index.get()) == 0;
}

bool
ConversationRepository::Impl::checkOnlyDeviceCertificate(const std::string& userDevice,
                                                         const std::string& commitId,
                                                         const std::string& parentId) const
{
    // Here, we check that a file device is modified or not.
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    if (changedFiles.size() == 0)
        return true;
    else if (changedFiles.size() > 1)
        return false;
    // If modified, it's the first commit of a device, we check
    // that the file wasn't there previously
    std::string deviceFile = std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt";
    if (changedFiles[0] != deviceFile) {
        return false;
    }

    // Retrieve tree for recent commit
    auto repo = repository();
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;
    if (!fileAtTree(deviceFile, treeNew)) {
        JAMI_ERR("%s announced but not found", deviceFile.c_str());
        return false;
    }
    return !fileAtTree(deviceFile, treeOld);
}

bool
ConversationRepository::Impl::checkVote(const std::string& userDevice,
                                        const std::string& commitId,
                                        const std::string& parentId) const
{
    // Check that maximum deviceFile and a vote is added
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    if (changedFiles.size() == 0) {
        return true;
    } else if (changedFiles.size() > 2) {
        return false;
    }
    // If modified, it's the first commit of a device, we check
    // that the file wasn't there previously. And the vote MUST be added
    std::string deviceFile = "";
    std::string votedFile = "";
    for (const auto& changedFile : changedFiles) {
        if (changedFile == std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt") {
            deviceFile = changedFile;
        } else if (changedFile.find("votes") == 0) {
            votedFile = changedFile;
        } else {
            // Invalid file detected
            JAMI_ERR("Invalid vote file detected: %s", changedFile.c_str());
            return false;
        }
    }

    if (votedFile.empty()) {
        JAMI_WARN("No vote detected for commit %s", commitId.c_str());
        return false;
    }

    auto repo = repository();
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    if (not deviceFile.empty()) {
        if (fileAtTree(deviceFile, treeOld)) {
            JAMI_ERR("Invalid device file modified: %s", deviceFile.c_str());
            return false;
        }
    }

    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert)
        return false;
    auto userUri = cert->getIssuerUID();
    // Check that voter is admin
    auto adminFile = std::string("admins") + DIR_SEPARATOR_STR + userUri + ".crt";

    if (!fileAtTree(adminFile, treeOld)) {
        JAMI_ERR("Vote from non admin: %s", userUri.c_str());
        return false;
    }

    // Check votedFile path
    const std::regex regex_votes("votes.(members|devices).(\\w+).(\\w+)");
    std::smatch base_match;
    if (!std::regex_match(votedFile, base_match, regex_votes) or base_match.size() != 4) {
        JAMI_WARN("Invalid votes path: %s", votedFile.c_str());
        return false;
    }

    std::string matchedUri = base_match[3];
    if (matchedUri != userUri) {
        JAMI_ERR("Admin voted for other user: %s vs %s", userUri.c_str(), matchedUri.c_str());
        return false;
    }
    std::string votedUri = base_match[2];
    std::string type = base_match[1];

    // Check that vote file is empty and wasn't modified
    if (fileAtTree(votedFile, treeOld)) {
        JAMI_ERR("Invalid voted file modified: %s", votedFile.c_str());
        return false;
    }
    auto vote = fileAtTree(votedFile, treeNew);
    if (!vote) {
        JAMI_ERR("No vote file found for: %s", userUri.c_str());
        return false;
    }
    auto* blob = reinterpret_cast<git_blob*>(vote.get());
    auto voteContent = std::string_view(static_cast<const char*>(git_blob_rawcontent(blob)),
                                        git_blob_rawsize(blob));
    if (!voteContent.empty()) {
        JAMI_ERR("Vote file not empty: %s", votedFile.c_str());
        return false;
    }

    // Check that peer voted is only other device or other member
    if (type == "members") {
        // Voted uri = not self
        if (votedUri == userUri) {
            JAMI_ERR("Detected vote for self: %s", votedUri.c_str());
            return false;
        }
        // file in members or admin
        adminFile = std::string("admins") + DIR_SEPARATOR_STR + votedUri + ".crt";
        auto memberFile = std::string("members") + DIR_SEPARATOR_STR + votedUri + ".crt";
        if (!fileAtTree(adminFile, treeOld) && !fileAtTree(memberFile, treeOld)) {
            JAMI_ERR("No member file found for vote: %s", votedUri.c_str());
            return false;
        }
    } else if (type == "devices") {
        // Check not current device
        if (votedUri == userDevice) {
            JAMI_ERR("Detected vote for self: %s", votedUri.c_str());
            return false;
        }
        // File in devices
        deviceFile = std::string("devices") + DIR_SEPARATOR_STR + votedUri + ".crt";
        if (!fileAtTree(deviceFile, treeOld)) {
            JAMI_ERR("No device file found for vote: %s", votedUri.c_str());
            return false;
        }
    } else {
        JAMI_ERR("Unknown vote type: %s", type.c_str());
        return false;
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidAdd(const std::string& userDevice,
                                            const std::string& uriMember,
                                            const std::string& commitId,
                                            const std::string& parentId) const
{
    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert)
        return false;
    auto userUri = cert->getIssuerUID();

    auto repo = repository();
    std::string repoPath = git_repository_workdir(repo.get());
    if (mode() == ConversationMode::ONE_TO_ONE) {
        auto initialMembers = getInitialMembers();
        auto it = std::find(initialMembers.begin(), initialMembers.end(), uriMember);
        if (it == initialMembers.end()) {
            JAMI_ERR("Invalid add in one to one conversation: %s", uriMember.c_str());
            return false;
        }
    }

    // Check that only /invited/uri.crt is added & deviceFile & CRLs
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    if (changedFiles.size() == 0) {
        return false;
    } else if (changedFiles.size() > 3) {
        return false;
    }

    // Check that user added is not sender
    if (userUri == uriMember) {
        JAMI_ERR("Member tried to add self: %s", userUri.c_str());
        return false;
    }

    // If modified, it's the first commit of a device, we check
    // that the file wasn't there previously. And the member MUST be added
    std::string deviceFile = "";
    std::string invitedFile = "";
    std::string crlFile = std::string("CRLs") + DIR_SEPARATOR_STR + userUri;
    for (const auto& changedFile : changedFiles) {
        if (changedFile == std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt") {
            deviceFile = changedFile;
        } else if (changedFile == std::string("invited") + DIR_SEPARATOR_STR + uriMember) {
            invitedFile = changedFile;
        } else if (changedFile == crlFile) {
            // Nothing to do
        } else {
            // Invalid file detected
            JAMI_ERR("Invalid add file detected: %s", changedFile.c_str());
            return false;
        }
    }

    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeOld)
        return false;
    auto treeNew = treeAtCommit(repo.get(), commitId);
    if (not treeNew)
        return false;
    auto blob_invite = fileAtTree(invitedFile, treeNew);
    if (!blob_invite) {
        JAMI_ERR("Invitation not found for commit %s", commitId.c_str());
        return false;
    }

    auto* blob = reinterpret_cast<git_blob*>(blob_invite.get());
    auto invitation = std::string_view(static_cast<const char*>(git_blob_rawcontent(blob)),
                                       git_blob_rawsize(blob));
    if (!invitation.empty()) {
        JAMI_ERR("Invitation not empty for commit %s", commitId.c_str());
        return false;
    }

    // Check that user not in /banned
    std::string bannedFile = std::string("banned") + DIR_SEPARATOR_STR + "members"
                             + DIR_SEPARATOR_STR + uriMember + ".crt";
    if (fileAtTree(bannedFile, treeOld)) {
        JAMI_ERR("Tried to add banned member: %s", bannedFile.c_str());
        return false;
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidJoins(const std::string& userDevice,
                                              const std::string& uriMember,
                                              const std::string& commitId,
                                              const std::string& parentId) const
{
    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert)
        return false;
    auto userUri = cert->getIssuerUID();
    // Check no other files changed
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    auto oneone = mode() == ConversationMode::ONE_TO_ONE;
    std::size_t wantedChanged = oneone ? 2 : 3;
    if (changedFiles.size() != wantedChanged) {
        return false;
    }

    auto invitedFile = std::string("invited") + DIR_SEPARATOR_STR + uriMember;
    auto membersFile = std::string("members") + DIR_SEPARATOR_STR + uriMember + ".crt";
    auto deviceFile = std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt";

    // Retrieve tree for commits
    auto repo = repository();
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    // Check /invited removed
    if (!oneone) {
        if (fileAtTree(invitedFile, treeNew)) {
            JAMI_ERR("%s invited not removed", userUri.c_str());
            return false;
        }
        if (!fileAtTree(invitedFile, treeOld)) {
            JAMI_ERR("%s invited not found", userUri.c_str());
            return false;
        }
    }

    // Check /members added
    if (!fileAtTree(membersFile, treeNew)) {
        JAMI_ERR("%s members not found", userUri.c_str());
        return false;
    }
    if (fileAtTree(membersFile, treeOld)) {
        JAMI_ERR("%s members found too soon", userUri.c_str());
        return false;
    }

    // Check /devices added
    if (!fileAtTree(deviceFile, treeNew)) {
        JAMI_ERR("%s devices not found", userUri.c_str());
        return false;
    }
    if (fileAtTree(deviceFile, treeOld)) {
        JAMI_ERR("%s devices found too soon", userUri.c_str());
        return false;
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidRemove(const std::string& userDevice,
                                               const std::string& uriMember,
                                               const std::string& commitId,
                                               const std::string& parentId) const
{
    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert)
        return false;
    auto userUri = cert->getIssuerUID();
    auto removeSelf = userUri == uriMember;

    // Retrieve tree for recent commit
    auto repo = repository();
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    std::string deviceFile = std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt";
    std::string adminFile = std::string("admins") + DIR_SEPARATOR_STR + uriMember + ".crt";
    std::string memberFile = std::string("members") + DIR_SEPARATOR_STR + uriMember + ".crt";
    std::string crlFile = std::string("CRLs") + DIR_SEPARATOR_STR + uriMember;
    std::vector<std::string> voters;
    std::vector<std::string> devicesRemoved;
    std::vector<std::string> bannedFiles;
    // Check that no weird file is added nor removed

    const std::regex regex_votes("votes.(members|devices).(\\w+).(\\w+)");
    const std::regex regex_devices("devices.(\\w+)\\.crt");
    const std::regex regex_banned("banned.(members|devices).(\\w+)\\.crt");
    std::smatch base_match;
    for (const auto& f : changedFiles) {
        if (f == deviceFile || f == adminFile || f == memberFile || f == crlFile) {
            // Ignore
        } else if (std::regex_match(f, base_match, regex_votes)) {
            if (base_match.size() != 4 or base_match[2] != uriMember) {
                JAMI_ERR("Invalid vote file detected: %s", f.c_str());
                return false;
            }
            voters.emplace_back(base_match[3]);
            // Check that votes were not added here
            if (!fileAtTree(f, treeOld)) {
                JAMI_ERR("invalid vote added (%s)", f.c_str());
                return false;
            }
        } else if (std::regex_match(f, base_match, regex_devices)) {
            if (base_match.size() == 2)
                devicesRemoved.emplace_back(base_match[1]);
        } else if (std::regex_match(f, base_match, regex_banned)) {
            bannedFiles.emplace_back(f);
            if (base_match.size() != 3 or base_match[2] != uriMember) {
                JAMI_ERR("Invalid banned file detected :%s", f.c_str());
                return false;
            }
        } else {
            JAMI_ERR("Unwanted changed file detected: %s", f.c_str());
            return false;
        }
    }

    // Check that removed devices are for removed member (or directly uriMember)
    for (const auto& deviceUri : devicesRemoved) {
        deviceFile = std::string("devices") + DIR_SEPARATOR_STR + deviceUri + ".crt";
        if (!fileAtTree(deviceFile, treeOld)) {
            JAMI_ERR("device not found added (%s)", deviceFile.c_str());
            return false;
        }
        cert = tls::CertificateStore::instance().getCertificate(deviceUri);
        if (!cert)
            return false;
        if (uriMember != cert->getIssuerUID()
            and uriMember != deviceUri /* If device is removed */) {
            JAMI_ERR("device removed but not for removed user (%s)", deviceFile.c_str());
            return false;
        }
    }

    if (removeSelf) {
        return bannedFiles.empty() && voters.empty();
    }

    // If not for self check that user device is admin
    adminFile = std::string("admins") + DIR_SEPARATOR_STR + userUri + ".crt";
    if (!fileAtTree(adminFile, treeOld)) {
        JAMI_ERR("admin file (%s) not found", adminFile.c_str());
        return false;
    }

    // If not for self check that vote is valid and not added
    auto nbAdmins = 0;
    auto nbVotes = 0;
    std::string repoPath = git_repository_workdir(repo.get());
    for (const auto& certificate : fileutils::readDirectory(repoPath + "admins")) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s", certificate.c_str());
            continue;
        }
        nbAdmins += 1;
        auto adminUri = certificate.substr(0, certificate.size() - std::string(".crt").size());
        if (std::find(voters.begin(), voters.end(), adminUri) != voters.end()) {
            nbVotes += 1;
        }
    }

    if (nbAdmins == 0 or (static_cast<double>(nbVotes) / static_cast<double>(nbAdmins)) < .5) {
        JAMI_ERR("Incomplete vote detected (commit: %s)", commitId.c_str());
        return false;
    }

    // If not for self check that member or device certificate is moved to banned/
    return !bannedFiles.empty();
}

bool
ConversationRepository::Impl::checkValidProfileUpdate(const std::string& userDevice,
                                                      const std::string& commitId,
                                                      const std::string& parentId) const
{
    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert)
        return false;
    auto userUri = cert->getIssuerUID();
    auto valid = false;
    {
        std::lock_guard<std::mutex> lk(membersMtx_);
        for (const auto& member : members_) {
            if (member.uri == userUri) {
                valid = member.role <= updateProfilePermLvl_;
                break;
            }
        }
    }
    if (!valid) {
        JAMI_ERR("Profile changed from unauthorized user: %s", userDevice.c_str());
        return false;
    }

    // Retrieve tree for recent commit
    auto repo = repository();
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    // Check that no weird file is added nor removed
    for (const auto& f : changedFiles) {
        if (f == "profile.vcf") {
            // Ignore
        } else {
            JAMI_ERR("Unwanted changed file detected: %s", f.c_str());
            return false;
        }
    }

    return true;
}

bool
ConversationRepository::Impl::isValidUserAtCommit(const std::string& userDevice,
                                                  const std::string& commitId) const
{
    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert || !cert->issuer)
        return false;
    auto userUri = cert->getIssuerUID();

    // Retrieve tree for commit
    auto repo = repository();
    auto tree = treeAtCommit(repo.get(), commitId);
    if (not tree)
        return false;

    // Check that /devices/userDevice.crt exists
    std::string deviceFile = std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt";
    auto blob_device = fileAtTree(deviceFile, tree);
    if (!fileAtTree(deviceFile, tree)) {
        JAMI_ERR("%s announced but not found", deviceFile.c_str());
        return false;
    }

    // Check that /(members|admins)/userUri.crt exists
    std::string membersFile = std::string("members") + DIR_SEPARATOR_STR + userUri + ".crt";
    std::string adminsFile = std::string("admins") + DIR_SEPARATOR_STR + userUri + ".crt";
    auto blob_parent = fileAtTree(membersFile, tree);
    if (not blob_parent)
        blob_parent = fileAtTree(adminsFile, tree);
    if (not blob_parent) {
        JAMI_ERR("Certificate not found for %s", userUri.c_str());
        return false;
    }

    // Check that certificate matches
    auto* blob = reinterpret_cast<git_blob*>(blob_device.get());
    auto deviceCert = std::string_view(static_cast<const char*>(git_blob_rawcontent(blob)),
                                       git_blob_rawsize(blob));
    blob = reinterpret_cast<git_blob*>(blob_parent.get());
    auto parentCert = std::string_view(static_cast<const char*>(git_blob_rawcontent(blob)),
                                       git_blob_rawsize(blob));
    auto deviceCertStr = cert->toString(false);
    auto parentCertStr = cert->issuer->toString(true);

    return deviceCert == deviceCertStr && parentCert == parentCertStr;
}

bool
ConversationRepository::Impl::checkInitialCommit(const std::string& userDevice,
                                                 const std::string& commitId) const
{
    auto cert = tls::CertificateStore::instance().getCertificate(userDevice);
    if (!cert) {
        JAMI_ERR("Cannot find certificate for %s", userDevice.c_str());
        return false;
    }
    auto userUri = cert->getIssuerUID();
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, ""));

    try {
        mode();
    } catch (...) {
        JAMI_ERR("Invalid mode detected for commit: %s", commitId.c_str());
        return false;
    }

    auto hasDevice = false, hasAdmin = false;
    std::string adminsFile = std::string("admins") + DIR_SEPARATOR_STR + userUri + ".crt";
    std::string deviceFile = std::string("devices") + DIR_SEPARATOR_STR + userDevice + ".crt";
    std::string crlFile = std::string("CRLs") + DIR_SEPARATOR_STR + userUri;
    // Check that admin cert is added
    // Check that device cert is added
    // Check CRLs added
    // Check that no other file is added
    for (const auto& changedFile : changedFiles) {
        if (changedFile == adminsFile) {
            hasAdmin = true;
        } else if (changedFile == deviceFile) {
            hasDevice = true;
        } else if (changedFile == crlFile) {
            // Nothing to do
        } else {
            // Invalid file detected
            JAMI_ERR("Invalid add file detected: %s", changedFile.c_str());
            return false;
        }
    }

    return hasDevice && hasAdmin;
}

std::string
ConversationRepository::Impl::commit(const std::string& msg)
{
    auto account = account_.lock();
    if (!account)
        return {};
    auto deviceId = std::string(account->currentDeviceId());
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr, git_signature_free};

    // Retrieve current index
    git_index* index_ptr = nullptr;
    auto repo = repository();
    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
        return {};
    }
    GitIndex index {index_ptr, git_index_free};

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERR("Unable to write initial tree from index");
        return {};
    }

    git_tree* tree_ptr = nullptr;
    if (git_tree_lookup(&tree_ptr, repo.get(), &tree_id) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERR("Could not look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_buf to_sign = {};
    const git_commit* head_ref[1] = {head_commit.get()};
    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 msg.c_str(),
                                 tree.get(),
                                 1,
                                 &head_ref[0])
        < 0) {
        JAMI_ERR("Could not create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    if (git_commit_create_with_signature(&commit_id,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Could not sign commit");
        return {};
    }

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, repo.get(), "refs/heads/main", &commit_id, true, nullptr)
        < 0) {
        JAMI_WARN("Could not move commit to main");
    }
    git_reference_free(ref_ptr);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_INFO("New message added with id: %s", commit_str);
    }
    return commit_str ? commit_str : "";
}

ConversationMode
ConversationRepository::Impl::mode() const
{
    // If already retrieven, return it, else get it from first commit
    if (mode_ != std::nullopt)
        return *mode_;

    auto lastMsg = log(id_, "", 1);
    if (lastMsg.size() == 0) {
        throw std::logic_error("Can't retrieve first commit");
        if (auto shared = account_.lock()) {
            emitSignal<DRing::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                       id_,
                                                                       EINVALIDMODE,
                                                                       "No initial commit");
        }
    }
    auto commitMsg = lastMsg[0].commit_msg;

    std::string err;
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(commitMsg.data(), commitMsg.data() + commitMsg.size(), &root, &err)) {
        throw std::logic_error("Can't retrieve first commit");
        if (auto shared = account_.lock()) {
            emitSignal<DRing::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                       id_,
                                                                       EINVALIDMODE,
                                                                       "No initial commit");
        }
    }
    if (!root.isMember("mode")) {
        throw std::logic_error("No mode detected for initial commit");
        if (auto shared = account_.lock()) {
            emitSignal<DRing::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                       id_,
                                                                       EINVALIDMODE,
                                                                       "No mode detected");
        }
    }
    int mode = root["mode"].asInt();

    switch (mode) {
    case 0:
        mode_ = ConversationMode::ONE_TO_ONE;
        break;
    case 1:
        mode_ = ConversationMode::ADMIN_INVITES_ONLY;
        break;
    case 2:
        mode_ = ConversationMode::INVITES_ONLY;
        break;
    case 3:
        mode_ = ConversationMode::PUBLIC;
        break;
    default:
        if (auto shared = account_.lock()) {
            emitSignal<DRing::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                       id_,
                                                                       EINVALIDMODE,
                                                                       "Incorrect mode detected");
        }
        throw std::logic_error("Incorrect mode detected");
        break;
    }
    return *mode_;
}

std::string
ConversationRepository::Impl::diffStats(const std::string& newId, const std::string& oldId) const
{
    auto repo = repository();
    if (auto d = diff(repo.get(), newId, oldId))
        return diffStats(d);
    return {};
}

GitDiff
ConversationRepository::Impl::diff(git_repository* repo,
                                   const std::string& idNew,
                                   const std::string& idOld) const
{
    if (!repo) {
        JAMI_ERR("Cannot get reference for HEAD");
        return {nullptr, git_diff_free};
    }

    // Retrieve tree for commit new
    git_oid oid;
    git_commit* commitNew = nullptr;
    if (idNew == "HEAD") {
        if (git_reference_name_to_id(&oid, repo, "HEAD") < 0) {
            JAMI_ERR("Cannot get reference for HEAD");
            return {nullptr, git_diff_free};
        }

        if (git_commit_lookup(&commitNew, repo, &oid) < 0) {
            JAMI_ERR("Could not look up HEAD commit");
            return {nullptr, git_diff_free};
        }
    } else {
        if (git_oid_fromstr(&oid, idNew.c_str()) < 0
            || git_commit_lookup(&commitNew, repo, &oid) < 0) {
            JAMI_WARN("Failed to look up commit %s", idNew.c_str());
            return {nullptr, git_diff_free};
        }
    }
    GitCommit new_commit = {commitNew, git_commit_free};

    git_tree* tNew = nullptr;
    if (git_commit_tree(&tNew, new_commit.get()) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return {nullptr, git_diff_free};
    }
    GitTree treeNew = {tNew, git_tree_free};

    git_diff* diff_ptr = nullptr;
    if (idOld.empty()) {
        if (git_diff_tree_to_tree(&diff_ptr, repo, nullptr, treeNew.get(), {}) < 0) {
            JAMI_ERR("Could not get diff to empty repository");
            return {nullptr, git_diff_free};
        }
        return {diff_ptr, git_diff_free};
    }

    // Retrieve tree for commit old
    git_commit* commitOld = nullptr;
    if (git_oid_fromstr(&oid, idOld.c_str()) < 0 || git_commit_lookup(&commitOld, repo, &oid) < 0) {
        JAMI_WARN("Failed to look up commit %s", idOld.c_str());
        return {nullptr, git_diff_free};
    }
    GitCommit old_commit {commitOld, git_commit_free};

    git_tree* tOld = nullptr;
    if (git_commit_tree(&tOld, old_commit.get()) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return {nullptr, git_diff_free};
    }
    GitTree treeOld = {tOld, git_tree_free};

    // Calc diff
    if (git_diff_tree_to_tree(&diff_ptr, repo, treeOld.get(), treeNew.get(), {}) < 0) {
        JAMI_ERR("Could not get diff between %s and %s", idOld.c_str(), idNew.c_str());
        return {nullptr, git_diff_free};
    }
    return {diff_ptr, git_diff_free};
}

std::vector<ConversationCommit>
ConversationRepository::Impl::behind(const std::string& from) const
{
    git_oid oid_local, oid_remote;
    auto repo = repository();
    if (git_reference_name_to_id(&oid_local, repo.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return {};
    }
    if (git_oid_fromstr(&oid_remote, from.c_str()) < 0) {
        JAMI_ERR("Cannot get reference for commit %s", from.c_str());
        return {};
    }
    size_t ahead = 0, behind = 0;
    if (git_graph_ahead_behind(&ahead, &behind, repo.get(), &oid_local, &oid_remote) != 0) {
        JAMI_ERR("Cannot get commits ahead for commit %s", from.c_str());
        return {};
    }

    if (behind == 0) {
        return {}; // Nothing to validate
    }

    return log(from, "", behind);
}

std::vector<ConversationCommit>
ConversationRepository::Impl::log(const std::string& from,
                                  const std::string& to,
                                  unsigned n,
                                  bool logIfNotFound) const
{
    std::vector<ConversationCommit> commits {};

    git_oid oid;
    auto repo = repository();
    if (!repo)
        return commits;
    if (from.empty()) {
        if (git_reference_name_to_id(&oid, repo.get(), "HEAD") < 0) {
            JAMI_ERR("Cannot get reference for HEAD");
            return commits;
        }
    } else {
        if (git_oid_fromstr(&oid, from.c_str()) < 0) {
            JAMI_ERR("Cannot get reference for commit %s", from.c_str());
            return commits;
        }
    }

    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, repo.get()) < 0 || git_revwalk_push(walker_ptr, &oid) < 0) {
        if (walker_ptr)
            git_revwalk_free(walker_ptr);
        // This fail can be ok in the case we check if a commit exists before pulling (so can fail
        // there). only log if the fail is unwanted.
        if (logIfNotFound)
            JAMI_DBG("Couldn't init revwalker for conversation %s", id_.c_str());
        return commits;
    }
    GitRevWalker walker {walker_ptr, git_revwalk_free};
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    for (auto idx = 0u; !git_revwalk_next(&oid, walker.get()); ++idx) {
        if (n != 0 && idx == n) {
            break;
        }
        git_commit* commit_ptr = nullptr;
        std::string id = git_oid_tostr_s(&oid);
        if (git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0) {
            JAMI_WARN("Failed to look up commit %s", id.c_str());
            break;
        }
        GitCommit commit {commit_ptr, git_commit_free};
        if (id == to) {
            break;
        }

        const git_signature* sig = git_commit_author(commit.get());
        GitAuthor author;
        author.name = sig->name;
        author.email = sig->email;
        std::vector<std::string> parents;
        auto parentsCount = git_commit_parentcount(commit.get());
        for (unsigned int p = 0; p < parentsCount; ++p) {
            std::string parent {};
            const git_oid* pid = git_commit_parent_id(commit.get(), p);
            if (pid) {
                parent = git_oid_tostr_s(pid);
                parents.emplace_back(parent);
            }
        }

        auto cc = commits.emplace(commits.end(), ConversationCommit {});
        cc->id = std::move(id);
        cc->commit_msg = git_commit_message(commit.get());
        cc->author = std::move(author);
        cc->parents = std::move(parents);
        git_buf signature = {}, signed_data = {};
        if (git_commit_extract_signature(&signature, &signed_data, repo.get(), &oid, "signature")
            < 0) {
            JAMI_WARN("Could not extract signature for commit %s", id.c_str());
        } else {
            cc->signature = base64::decode(
                std::string(signature.ptr, signature.ptr + signature.size));
            cc->signed_content = std::vector<uint8_t>(signed_data.ptr,
                                                      signed_data.ptr + signed_data.size);
        }
        cc->timestamp = git_commit_time(commit.get());
    }

    return commits;
}

std::optional<std::string>
ConversationRepository::Impl::linearizedParent(const std::string& commitId) const
{
    git_oid oid;
    auto repo = repository();
    if (!repo or git_reference_name_to_id(&oid, repo.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return std::nullopt;
    }

    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, repo.get()) < 0 || git_revwalk_push(walker_ptr, &oid) < 0) {
        if (walker_ptr)
            git_revwalk_free(walker_ptr);
        // This fail can be ok in the case we check if a commit exists before pulling (so can fail
        // there). only log if the fail is unwanted.
        return std::nullopt;
    }
    GitRevWalker walker {walker_ptr, git_revwalk_free};
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    auto ret = false;
    for (auto idx = 0u; !git_revwalk_next(&oid, walker.get()); ++idx) {
        git_commit* commit_ptr = nullptr;
        std::string id = git_oid_tostr_s(&oid);
        if (git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0) {
            JAMI_WARN("Failed to look up commit %s", id.c_str());
            break;
        }

        if (ret)
            return id;
        if (id == commitId)
            ret = true;
    }

    return std::nullopt;
}

GitObject
ConversationRepository::Impl::fileAtTree(const std::string& path, const GitTree& tree) const
{
    git_object* blob_ptr = nullptr;
    if (git_object_lookup_bypath(&blob_ptr,
                                 reinterpret_cast<git_object*>(tree.get()),
                                 path.c_str(),
                                 GIT_OBJECT_BLOB)
        != 0) {
        return GitObject {nullptr, git_object_free};
    }
    return GitObject {blob_ptr, git_object_free};
}

GitTree
ConversationRepository::Impl::treeAtCommit(git_repository* repo, const std::string& commitId) const
{
    git_oid oid;
    git_commit* commit = nullptr;
    if (git_oid_fromstr(&oid, commitId.c_str()) < 0 || git_commit_lookup(&commit, repo, &oid) < 0) {
        JAMI_WARN("Failed to look up commit %s", commitId.c_str());
        return GitTree {nullptr, git_tree_free};
    }
    GitCommit gc = {commit, git_commit_free};
    git_tree* tree = nullptr;
    if (git_commit_tree(&tree, gc.get()) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return GitTree {nullptr, git_tree_free};
    }
    return GitTree {tree, git_tree_free};
}

std::string
ConversationRepository::Impl::getCommitType(const std::string& commitMsg) const
{
    std::string type = {};
    std::string err;
    Json::Value cm;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (reader->parse(commitMsg.data(), commitMsg.data() + commitMsg.size(), &cm, &err)) {
        type = cm["type"].asString();
    } else {
        JAMI_WARN("%s", err.c_str());
    }
    return type;
}

std::vector<std::string>
ConversationRepository::Impl::getInitialMembers() const
{
    auto firstCommit = log(id_, "", 1);
    if (firstCommit.size() == 0) {
        return {};
    }
    auto commit = firstCommit[0];

    auto authorDevice = commit.author.email;
    auto cert = tls::CertificateStore::instance().getCertificate(authorDevice);
    if (!cert)
        return {};
    auto authorId = cert->getIssuerUID();
    if (mode() == ConversationMode::ONE_TO_ONE) {
        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(commit.commit_msg.data(),
                           commit.commit_msg.data() + commit.commit_msg.size(),
                           &root,
                           &err)) {
            return {authorId};
        }
        if (root.isMember("invited") && root["invited"].asString() != authorId)
            return {authorId, root["invited"].asString()};
    }
    return {authorId};
}

bool
ConversationRepository::Impl::resolveConflicts(git_index* index, const std::string& other_id)
{
    git_index_conflict_iterator* conflict_iterator = nullptr;
    const git_index_entry* ancestor_out = nullptr;
    const git_index_entry* our_out = nullptr;
    const git_index_entry* their_out = nullptr;

    git_index_conflict_iterator_new(&conflict_iterator, index);

    git_oid head_commit_id;
    auto repo = repository();
    if (git_reference_name_to_id(&head_commit_id, repo.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return false;
    }
    auto commit_str = git_oid_tostr_s(&head_commit_id);
    if (!commit_str)
        return false;
    auto useRemote = (other_id > commit_str); // Choose by commit version

    // NOTE: for now, only authorize conflicts on "profile.vcf"
    std::vector<git_index_entry> new_entries;
    while (git_index_conflict_next(&ancestor_out, &our_out, &their_out, conflict_iterator)
           != GIT_ITEROVER) {
        if (ancestor_out && ancestor_out->path && our_out && our_out->path && their_out
            && their_out->path) {
            if (std::string(ancestor_out->path) == "profile.vcf") {
                // Checkout wanted version. copy the index_entry
                git_index_entry resolution = useRemote ? *their_out : *our_out;
                resolution.flags &= GIT_INDEX_STAGE_NORMAL;
                if (!(resolution.flags & GIT_IDXENTRY_VALID))
                    resolution.flags |= GIT_IDXENTRY_VALID;
                // NOTE: do no git_index_add yet, wait for after full conflict checks
                new_entries.push_back(resolution);
                continue;
            }
            JAMI_ERR("Conflict detected on a file that is not authorized: %s", ancestor_out->path);
            return false;
        }
        return false;
    }

    for (auto& entry : new_entries)
        git_index_add(index, &entry);
    git_index_conflict_cleanup(index);
    git_index_conflict_iterator_free(conflict_iterator);

    // Checkout and cleanup
    git_checkout_options opt;
    git_checkout_options_init(&opt, GIT_CHECKOUT_OPTIONS_VERSION);
    opt.checkout_strategy |= GIT_CHECKOUT_FORCE;
    opt.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;
    if (other_id > commit_str)
        opt.checkout_strategy |= GIT_CHECKOUT_USE_THEIRS;
    else
        opt.checkout_strategy |= GIT_CHECKOUT_USE_OURS;

    if (git_checkout_index(repo.get(), index, &opt) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("Cannot checkout index: %s", err->message);
        return false;
    }

    return true;
}

void
ConversationRepository::Impl::initMembers()
{
    auto repo = repository();
    if (!repo)
        return;

    std::vector<std::string> uris;
    std::lock_guard<std::mutex> lk(membersMtx_);
    members_.clear();
    std::string repoPath = git_repository_workdir(repo.get());
    std::vector<std::string> paths = {repoPath + DIR_SEPARATOR_STR + "invited",
                                      repoPath + DIR_SEPARATOR_STR + "admins",
                                      repoPath + DIR_SEPARATOR_STR + "members",
                                      repoPath + DIR_SEPARATOR_STR + "banned" + DIR_SEPARATOR_STR
                                          + "members"};
    std::vector<MemberRole> roles = {
        MemberRole::INVITED,
        MemberRole::ADMIN,
        MemberRole::MEMBER,
        MemberRole::BANNED,
    };

    auto i = 0;
    for (const auto& p : paths) {
        for (const auto& f : fileutils::readDirectory(p)) {
            auto pos = f.find(".crt");
            auto uri = f.substr(0, pos);
            uris.emplace_back(uri);
            members_.emplace_back(ConversationMember {uri, roles[i]});
        }
        ++i;
    }

    if (mode() == ConversationMode::ONE_TO_ONE) {
        for (const auto& member : getInitialMembers()) {
            auto it = std::find(uris.begin(), uris.end(), member);
            if (it == uris.end()) {
                members_.emplace_back(ConversationMember {member, MemberRole::INVITED});
            }
        }
    }
}

std::string
ConversationRepository::Impl::diffStats(const GitDiff& diff) const
{
    git_diff_stats* stats_ptr = nullptr;
    if (git_diff_get_stats(&stats_ptr, diff.get()) < 0) {
        JAMI_ERR("Could not get diff stats");
        return {};
    }
    GitDiffStats stats = {stats_ptr, git_diff_stats_free};

    git_diff_stats_format_t format = GIT_DIFF_STATS_FULL;
    git_buf statsBuf = {};
    if (git_diff_stats_to_buf(&statsBuf, stats.get(), format, 80) < 0) {
        JAMI_ERR("Could not format diff stats");
        return {};
    }

    return std::string(statsBuf.ptr, statsBuf.ptr + statsBuf.size);
}

//////////////////////////////////

std::unique_ptr<ConversationRepository>
ConversationRepository::createConversation(const std::weak_ptr<JamiAccount>& account,
                                           ConversationMode mode,
                                           const std::string& otherMember)
{
    auto shared = account.lock();
    if (!shared)
        return {};
    // Create temporary directory because we can't know the first hash for now
    std::uniform_int_distribution<uint64_t> dist {0, std::numeric_limits<uint64_t>::max()};
    random_device rdev;
    auto tmpPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                   + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR
                   + std::to_string(dist(rdev));
    if (fileutils::isDirectory(tmpPath)) {
        JAMI_ERR("%s already exists. Abort create conversations", tmpPath.c_str());
        return {};
    }
    if (!fileutils::recursive_mkdir(tmpPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", tmpPath.c_str());
        return {};
    }
    auto repo = create_empty_repository(tmpPath);
    if (!repo) {
        return {};
    }

    // Add initial files
    if (!add_initial_files(repo, shared)) {
        JAMI_ERR("Error when adding initial files");
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    // Commit changes
    auto id = initial_commit(repo, shared, mode, otherMember);
    if (id.empty()) {
        JAMI_ERR("Couldn't create initial commit in %s", tmpPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    // Move to wanted directory
    auto newPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                   + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + id;
    if (std::rename(tmpPath.c_str(), newPath.c_str())) {
        JAMI_ERR("Couldn't move %s in %s", tmpPath.c_str(), newPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    JAMI_INFO("New conversation initialized in %s", newPath.c_str());

    return std::make_unique<ConversationRepository>(account, id);
}

std::unique_ptr<ConversationRepository>
ConversationRepository::cloneConversation(const std::weak_ptr<JamiAccount>& account,
                                          const std::string& deviceId,
                                          const std::string& conversationId)
{
    auto shared = account.lock();
    if (!shared)
        return {};
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + conversationId;
    git_repository* rep = nullptr;
    std::stringstream url;
    url << "git://" << deviceId << '/' << conversationId;

    git_clone_options clone_options;
    git_clone_options_init(&clone_options, GIT_CLONE_OPTIONS_VERSION);
    git_fetch_options_init(&clone_options.fetch_opts, GIT_FETCH_OPTIONS_VERSION);
    size_t received_bytes = 0;
    clone_options.fetch_opts.callbacks.payload = static_cast<void*>(&received_bytes);
    clone_options.fetch_opts.callbacks.transfer_progress = [](const git_indexer_progress* stats,
                                                              void* payload) {
        *(static_cast<size_t*>(payload)) += stats->received_bytes;
        if (*(static_cast<size_t*>(payload)) > MAX_FETCH_SIZE) {
            JAMI_ERR("Abort fetching repository, the fetch is too big: %lu bytes",
                     *(static_cast<size_t*>(payload)));
            return -1;
        }
        return 0;
    };

    if (git_clone(&rep, url.str().c_str(), path.c_str(), nullptr) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("Error when retrieving remote conversation: %s %s", err->message, path.c_str());
        return nullptr;
    }
    git_repository_free(rep);
    auto repo = std::make_unique<ConversationRepository>(account, conversationId);
    repo->pinCertificates(); // need to load certificates to validate non known members
    if (!repo->validClone()) {
        JAMI_ERR("Error when validating remote conversation");
        return nullptr;
    }
    JAMI_INFO("New conversation cloned in %s", path.c_str());
    return repo;
}

bool
ConversationRepository::Impl::validCommits(
    const std::vector<ConversationCommit>& commitsToValidate) const
{
    for (const auto& commit : commitsToValidate) {
        auto userDevice = commit.author.email;
        auto validUserAtCommit = commit.id;
        if (commit.parents.size() == 0) {
            if (!checkInitialCommit(userDevice, commit.id)) {
                JAMI_WARN("Malformed initial commit %s. Please check you use the latest "
                          "version of Jami, or that your contact is not doing unwanted stuff.",
                          commit.id.c_str());
                if (auto shared = account_.lock()) {
                    emitSignal<DRing::ConversationSignal::OnConversationError>(
                        shared->getAccountID(), id_, EVALIDFETCH, "Malformed initial commit");
                }
                return false;
            }
        } else if (commit.parents.size() == 1) {
            auto type = getCommitType(commit.commit_msg);
            if (type == "vote") {
                // Check that vote is valid
                if (!checkVote(userDevice, commit.id, commit.parents[0])) {
                    JAMI_WARN("Malformed vote commit %s. Please check you use the latest version "
                              "of Jami, or that your contact is not doing unwanted stuff.",
                              commit.id.c_str());
                    if (auto shared = account_.lock()) {
                        emitSignal<DRing::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed vote");
                    }
                    return false;
                }
            } else if (type == "member") {
                std::string err;
                Json::Value root;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(commit.commit_msg.data(),
                                   commit.commit_msg.data() + commit.commit_msg.size(),
                                   &root,
                                   &err)) {
                    JAMI_ERR() << "Failed to parse " << err;
                    if (auto shared = account_.lock()) {
                        emitSignal<DRing::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed member commit");
                    }
                    return false;
                }
                std::string action = root["action"].asString();
                std::string uriMember = root["uri"].asString();
                if (action == "add") {
                    if (!checkValidAdd(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARN(
                            "Malformed add commit %s. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id.c_str());
                        if (auto shared = account_.lock()) {
                            emitSignal<DRing::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed add member commit");
                        }
                        return false;
                    }
                } else if (action == "join") {
                    if (!checkValidJoins(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARN(
                            "Malformed joins commit %s. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id.c_str());
                        if (auto shared = account_.lock()) {
                            emitSignal<DRing::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed join member commit");
                        }
                        return false;
                    }
                } else if (action == "remove") {
                    // In this case, we remove the user. So if self, the user will not be
                    // valid for this commit. Check previous commit
                    validUserAtCommit = commit.parents[0];
                    if (!checkValidRemove(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARN(
                            "Malformed removes commit %s. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id.c_str());
                        if (auto shared = account_.lock()) {
                            emitSignal<DRing::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed remove member commit");
                        }
                        return false;
                    }
                } else if (action == "ban") {
                    // Note device.size() == "member".size()
                    if (!checkValidRemove(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARN(
                            "Malformed removes commit %s. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id.c_str());
                        if (auto shared = account_.lock()) {
                            emitSignal<DRing::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed ban member commit");
                        }
                        return false;
                    }
                } else {
                    JAMI_WARN("Malformed member commit %s with action %s. Please check you use the "
                              "latest "
                              "version of Jami, or that your contact is not doing unwanted stuff.",
                              commit.id.c_str(),
                              action.c_str());
                    if (auto shared = account_.lock()) {
                        emitSignal<DRing::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed member commit");
                    }
                    return false;
                }
            } else if (type == "application/update-profile") {
                if (!checkValidProfileUpdate(userDevice, commit.id, commit.parents[0])) {
                    JAMI_WARN("Malformed profile updates commit %s. Please check you use the "
                              "latest version "
                              "of Jami, or that your contact is not doing unwanted stuff.",
                              commit.id.c_str());
                    if (auto shared = account_.lock()) {
                        emitSignal<DRing::ConversationSignal::OnConversationError>(
                            shared->getAccountID(),
                            id_,
                            EVALIDFETCH,
                            "Malformed profile updates commit");
                    }
                    return false;
                }
            } else {
                // Note: accept all mimetype here, as we can have new mimetypes
                // Just avoid to add weird files
                // Check that no weird file is added outside device cert nor removed
                if (!checkOnlyDeviceCertificate(userDevice, commit.id, commit.parents[0])) {
                    JAMI_WARN("Malformed %s commit %s. Please check you use the latest "
                              "version of Jami, or that your contact is not doing unwanted stuff.",
                              type.c_str(),
                              commit.id.c_str());
                    if (auto shared = account_.lock()) {
                        emitSignal<DRing::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed commit");
                    }
                    return false;
                }
            }

            // For all commit, check that user is valid,
            // So that user certificate MUST be in /members or /admins
            // and device cert MUST be in /devices
            if (!isValidUserAtCommit(userDevice, validUserAtCommit)) {
                JAMI_WARN(
                    "Malformed commit %s. Please check you use the latest version of Jami, or "
                    "that your contact is not doing unwanted stuff. %s",
                    validUserAtCommit.c_str(),
                    commit.commit_msg.c_str());
                if (auto shared = account_.lock()) {
                    emitSignal<DRing::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                               id_,
                                                                               EVALIDFETCH,
                                                                               "Malformed commit");
                }
                return false;
            }
        } else {
            // Merge commit, for now, nothing to validate
        }
        JAMI_DBG("Validate commit %s", commit.id.c_str());
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////

ConversationRepository::ConversationRepository(const std::weak_ptr<JamiAccount>& account,
                                               const std::string& id)
    : pimpl_ {new Impl {account, id}}
{}

ConversationRepository::~ConversationRepository() = default;

const std::string&
ConversationRepository::id() const
{
    return pimpl_->id_;
}

std::string
ConversationRepository::addMember(const std::string& uri)
{
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto deviceId = account->currentDeviceId();
    auto name = account->getUsername();
    if (name.empty())
        name = deviceId;

    // First, we need to add the member file to the repository if not present
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());

    std::string invitedPath = repoPath + "invited";
    if (!fileutils::recursive_mkdir(invitedPath, 0700)) {
        JAMI_ERR("Error when creating %s.", invitedPath.c_str());
        return {};
    }
    std::string devicePath = invitedPath + DIR_SEPARATOR_STR + uri;
    if (fileutils::isFile(devicePath)) {
        JAMI_WARN("Member %s already present!", uri.c_str());
        return {};
    }

    auto file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", devicePath.c_str());
        return {};
    }
    std::string path = "invited/" + uri;
    if (!pimpl_->add(path.c_str()))
        return {};

    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        pimpl_->members_.emplace_back(ConversationMember {uri, MemberRole::INVITED});
    }

    Json::Value json;
    json["action"] = "add";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return pimpl_->commit(Json::writeString(wbuilder, json));
}

std::string
ConversationRepository::amend(const std::string& id, const std::string& msg)
{
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto deviceId = std::string(account->currentDeviceId());
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    git_oid tree_id, commit_id;

    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr, git_signature_free};

    git_commit* commit_ptr = nullptr;
    auto repo = pimpl_->repository();
    if (git_oid_fromstr(&tree_id, id.c_str()) < 0
        || git_commit_lookup(&commit_ptr, repo.get(), &tree_id) < 0) {
        JAMI_WARN("Failed to look up commit %s", id.c_str());
        return {};
    }
    GitCommit commit {commit_ptr, git_commit_free};

    if (git_commit_amend(
            &commit_id, commit.get(), nullptr, sig.get(), sig.get(), nullptr, msg.c_str(), nullptr)
        < 0) {
        JAMI_ERR("Could not amend commit");
        return {};
    }

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, repo.get(), "refs/heads/main", &commit_id, true, nullptr)
        < 0) {
        JAMI_WARN("Could not move commit to main");
    }
    git_reference_free(ref_ptr);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_DBG("Commit %s amended (new id: %s)", id.c_str(), commit_str);
        return commit_str;
    }
    return {};
}

bool
ConversationRepository::fetch(const std::string& remoteDeviceId)
{
    // Fetch distant repository
    git_remote* remote_ptr = nullptr;
    git_fetch_options fetch_opts;
    git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION);

    auto lastMsg = logN("", 1);
    if (lastMsg.size() == 0) {
        return false;
    }
    auto lastCommit = lastMsg[0].id;

    // Assert that repository exists
    auto repo = pimpl_->repository();
    std::string channelName = "git://" + remoteDeviceId + '/' + pimpl_->id_;
    auto res = git_remote_lookup(&remote_ptr, repo.get(), remoteDeviceId.c_str());
    if (res != 0) {
        if (res != GIT_ENOTFOUND) {
            JAMI_ERR("Couldn't lookup for remote %s", remoteDeviceId.c_str());
            return false;
        }
        if (git_remote_create(&remote_ptr, repo.get(), remoteDeviceId.c_str(), channelName.c_str())
            < 0) {
            JAMI_ERR("Could not create remote for repository for conversation %s",
                     pimpl_->id_.c_str());
            return false;
        }
    }
    GitRemote remote {remote_ptr, git_remote_free};

    size_t received_bytes = 0;
    fetch_opts.callbacks.payload = static_cast<void*>(&received_bytes);
    fetch_opts.callbacks.transfer_progress = [](const git_indexer_progress* stats, void* payload) {
        *(static_cast<size_t*>(payload)) += stats->received_bytes;
        if (*(static_cast<size_t*>(payload)) > MAX_FETCH_SIZE) {
            JAMI_ERR("Abort fetching repository, the fetch is too big: %lu bytes",
                     *(static_cast<size_t*>(payload)));
            return -1;
        }
        return 0;
    };
    if (git_remote_fetch(remote.get(), nullptr, &fetch_opts, "fetch") < 0) {
        const git_error* err = giterr_last();
        if (err) {
            JAMI_ERR("Could not fetch remote repository for conversation %s: %s",
                     pimpl_->id_.c_str(),
                     err->message);

            if (auto shared = pimpl_->account_.lock()) {
                emitSignal<DRing::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                           pimpl_->id_,
                                                                           EFETCH,
                                                                           err->message);
            }
        }
        return false;
    }

    return true;
}

std::string
ConversationRepository::remoteHead(const std::string& remoteDeviceId,
                                   const std::string& branch) const
{
    git_remote* remote_ptr = nullptr;
    auto repo = pimpl_->repository();
    if (git_remote_lookup(&remote_ptr, repo.get(), remoteDeviceId.c_str()) < 0) {
        JAMI_WARN("No remote found with id: %s", remoteDeviceId.c_str());
        return {};
    }
    GitRemote remote {remote_ptr, git_remote_free};

    git_reference* head_ref_ptr = nullptr;
    std::string remoteHead = "refs/remotes/" + remoteDeviceId + "/" + branch;
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo.get(), remoteHead.c_str()) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("failed to lookup %s ref: %s", remoteHead.c_str(), err->message);
        return {};
    }
    GitReference head_ref {head_ref_ptr, git_reference_free};

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (!commit_str)
        return {};
    return commit_str;
}

std::string
ConversationRepository::commitMessage(const std::string& msg)
{
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto deviceId = std::string(account->currentDeviceId());

    // First, we need to add device file to the repository if not present
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());
    std::string path = std::string("devices") + DIR_SEPARATOR_STR + deviceId + ".crt";
    std::string devicePath = repoPath + path;
    if (!fileutils::isFile(devicePath)) {
        auto file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERR("Could not write data to %s", devicePath.c_str());
            return {};
        }
        auto cert = account->identity().second;
        auto deviceCert = cert->toString(false);
        file << deviceCert;
        file.close();

        if (!pimpl_->add(path))
            JAMI_WARN("Couldn't add file %s", devicePath.c_str());
    }

    return pimpl_->commit(msg);
}

std::vector<ConversationCommit>
ConversationRepository::logN(const std::string& last, unsigned n, bool logIfNotFound) const
{
    return pimpl_->log(last, "", n, logIfNotFound);
}

std::vector<ConversationCommit>
ConversationRepository::log(const std::string& from, const std::string& to, bool logIfNotFound) const
{
    return pimpl_->log(from, to, 0, logIfNotFound);
}

std::optional<ConversationCommit>
ConversationRepository::getCommit(const std::string& commitId, bool logIfNotFound) const
{
    auto commits = logN(commitId, 1, logIfNotFound);
    if (commits.empty())
        return std::nullopt;
    return std::move(commits[0]);
}

std::pair<bool, std::string>
ConversationRepository::merge(const std::string& merge_id)
{
    // First, the repository must be in a clean state
    auto repo = pimpl_->repository();
    int state = git_repository_state(repo.get());
    if (state != GIT_REPOSITORY_STATE_NONE) {
        JAMI_ERR("Merge operation aborted: repository is in unexpected state %d", state);
        return {false, ""};
    }
    // Checkout main (to do a `git_merge branch`)
    if (git_repository_set_head(repo.get(), "refs/heads/main") < 0) {
        JAMI_ERR("Merge operation aborted: couldn't checkout main branch");
        return {false, ""};
    }

    // Then check that merge_id exists
    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, merge_id.c_str()) < 0) {
        JAMI_ERR("Merge operation aborted: couldn't lookup commit %s", merge_id.c_str());
        return {false, ""};
    }
    git_annotated_commit* annotated_ptr = nullptr;
    if (git_annotated_commit_lookup(&annotated_ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERR("Merge operation aborted: couldn't lookup commit %s", merge_id.c_str());
        return {false, ""};
    }
    GitAnnotatedCommit annotated {annotated_ptr, git_annotated_commit_free};

    // Now, we can analyze which type of merge do we need
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    const git_annotated_commit* const_annotated = annotated.get();
    if (git_merge_analysis(&analysis, &preference, repo.get(), &const_annotated, 1) < 0) {
        JAMI_ERR("Merge operation aborted: repository analysis failed");
        return {false, ""};
    }

    // Handle easy merges
    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
        JAMI_INFO("Already up-to-date");
        return {true, ""};
    } else if (analysis & GIT_MERGE_ANALYSIS_UNBORN
               || (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD
                   && !(preference & GIT_MERGE_PREFERENCE_NO_FASTFORWARD))) {
        if (analysis & GIT_MERGE_ANALYSIS_UNBORN)
            JAMI_INFO("Merge analysis result: Unborn");
        else
            JAMI_INFO("Merge analysis result: Fast-forward");
        const auto* target_oid = git_annotated_commit_id(annotated.get());

        if (!pimpl_->mergeFastforward(target_oid, (analysis & GIT_MERGE_ANALYSIS_UNBORN))) {
            const git_error* err = giterr_last();
            if (err)
                JAMI_ERR("Fast forward merge failed: %s", err->message);
            return {false, ""};
        }
        return {true, ""}; // fast forward so no commit generated;
    }

    // Else we want to check for conflicts
    git_oid head_commit_id;
    if (git_reference_name_to_id(&head_commit_id, repo.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return {false, ""};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo.get(), &head_commit_id) < 0) {
        JAMI_ERR("Could not look up HEAD commit");
        return {false, ""};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_commit* other__ptr = nullptr;
    if (git_commit_lookup(&other__ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERR("Could not look up HEAD commit");
        return {false, ""};
    }
    GitCommit other_commit {other__ptr, git_commit_free};

    git_merge_options merge_opts;
    git_merge_options_init(&merge_opts, GIT_MERGE_OPTIONS_VERSION);
    git_index* index_ptr = nullptr;
    if (git_merge_commits(&index_ptr, repo.get(), head_commit.get(), other_commit.get(), &merge_opts)
        < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("Git merge failed: %s", err->message);
        return {false, ""};
    }
    GitIndex index {index_ptr, git_index_free};
    if (git_index_has_conflicts(index.get())) {
        JAMI_INFO("Some conflicts were detected during the merge operations. Resolution phase.");
        if (!pimpl_->resolveConflicts(index.get(), merge_id) or !git_add_all(repo.get())) {
            JAMI_ERR("Merge operation aborted; Can't automatically resolve conflicts");
            return {false, ""};
        }
    }

    auto result = pimpl_->createMergeCommit(index.get(), merge_id);
    JAMI_INFO("Merge done between %s and main", merge_id.c_str());

    return {!result.empty(), result};
}

std::string
ConversationRepository::diffStats(const std::string& newId, const std::string& oldId) const
{
    return pimpl_->diffStats(newId, oldId);
}

std::vector<std::string>
ConversationRepository::changedFiles(const std::string_view& diffStats)
{
    std::string line;
    std::vector<std::string> changedFiles;
    for (auto line : split_string(diffStats, '\n')) {
        std::regex re(" +\\| +[0-9]+.*");
        std::svmatch match;
        if (!std::regex_search(line, match, re) && match.size() == 0)
            continue;
        changedFiles.emplace_back(std::regex_replace(std::string {line}, re, "").substr(1));
    }
    return changedFiles;
}

std::string
ConversationRepository::join()
{
    // Check that not already member
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto cert = account->identity().second;
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERR("Parent cert is null!");
        return {};
    }
    auto uri = parentCert->getId().toString();
    std::string membersPath = repoPath + "members" + DIR_SEPARATOR_STR;
    std::string memberFile = membersPath + DIR_SEPARATOR_STR + uri + ".crt";
    std::string adminsPath = repoPath + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    if (fileutils::isFile(memberFile) or fileutils::isFile(adminsPath)) {
        // Already member, nothing to commit
        return {};
    }
    // Remove invited/uri.crt
    std::string invitedPath = repoPath + "invited";
    fileutils::remove(fileutils::getFullPath(invitedPath, uri));
    // Add members/uri.crt
    if (!fileutils::recursive_mkdir(membersPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", membersPath.c_str());
        return {};
    }
    auto file = fileutils::ofstream(memberFile, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", memberFile.c_str());
        return {};
    }
    file << parentCert->toString(true);
    file.close();
    // git add -A
    if (!git_add_all(repo.get())) {
        return {};
    }
    Json::Value json;
    json["action"] = "join";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        auto updated = false;

        for (auto& member : pimpl_->members_) {
            if (member.uri == uri) {
                updated = true;
                member.role = MemberRole::MEMBER;
                break;
            }
        }
        if (!updated)
            pimpl_->members_.emplace_back(ConversationMember {uri, MemberRole::MEMBER});
    }

    return commitMessage(Json::writeString(wbuilder, json));
}

std::string
ConversationRepository::leave()
{
    // TODO simplify
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto details = account->getAccountDetails();
    auto deviceId = details[DRing::Account::ConfProperties::DEVICE_ID];
    auto uri = details[DRing::Account::ConfProperties::USERNAME];
    auto name = details[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account
                   ->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    // Remove related files
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());

    std::string adminFile = repoPath + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    std::string memberFile = repoPath + "members" + DIR_SEPARATOR_STR + uri + ".crt";
    std::string crlsPath = repoPath + "CRLs";

    if (fileutils::isFile(adminFile)) {
        fileutils::removeAll(adminFile, true);
    }

    if (fileutils::isFile(memberFile)) {
        fileutils::removeAll(memberFile, true);
    }

    // /CRLs
    for (const auto& crl : account->identity().second->getRevocationLists()) {
        if (!crl)
            continue;
        auto v = crl->getNumber();
        std::stringstream ss;
        ss << std::hex;
        for (const auto& b : v)
            ss << (unsigned) b;
        std::string crlPath = crlsPath + DIR_SEPARATOR_STR + deviceId + DIR_SEPARATOR_STR + ss.str()
                              + ".crl";

        if (fileutils::isFile(crlPath)) {
            fileutils::removeAll(crlPath, true);
        }
    }

    // Devices
    for (const auto& d : account->getKnownDevices()) {
        std::string deviceFile = repoPath + "devices" + DIR_SEPARATOR_STR + d.first + ".crt";
        if (fileutils::isFile(deviceFile)) {
            fileutils::removeAll(deviceFile, true);
        }
    }

    if (!git_add_all(repo.get())) {
        return {};
    }

    Json::Value json;
    json["action"] = "remove";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        std::remove_if(pimpl_->members_.begin(), pimpl_->members_.end(), [&](auto& member) {
            return member.uri == account->getUsername();
        });
    }

    return commitMessage(Json::writeString(wbuilder, json));
}

void
ConversationRepository::erase()
{
    // First, we need to add the member file to the repository if not present
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());

    JAMI_DBG() << "Erasing " << repoPath;
    fileutils::removeAll(repoPath, true);
}

ConversationMode
ConversationRepository::mode() const
{
    return pimpl_->mode();
}

std::string
ConversationRepository::voteKick(const std::string& uri, bool isDevice)
{
    // Add vote + commit
    // TODO simplify
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto cert = account->identity().second;
    auto adminUri = cert->getIssuerUID();
    if (adminUri == uri) {
        JAMI_WARN("Admin tried to ban theirself");
        return {};
    }

    // TODO avoid duplicate
    auto relativeVotePath = std::string("votes") + DIR_SEPARATOR_STR
                            + (isDevice ? "devices" : "members") + DIR_SEPARATOR_STR + uri;
    auto voteDirectory = repoPath + DIR_SEPARATOR_STR + relativeVotePath;
    if (!fileutils::recursive_mkdir(voteDirectory, 0700)) {
        JAMI_ERR("Error when creating %s. Abort vote", voteDirectory.c_str());
        return {};
    }
    auto votePath = fileutils::getFullPath(voteDirectory, adminUri);
    auto voteFile = fileutils::ofstream(votePath, std::ios::trunc | std::ios::binary);
    if (!voteFile.is_open()) {
        JAMI_ERR("Could not write data to %s", votePath.c_str());
        return {};
    }
    voteFile.close();

    auto toAdd = fileutils::getFullPath(relativeVotePath, adminUri);
    if (!pimpl_->add(toAdd.c_str()))
        return {};

    Json::Value json;
    json["uri"] = uri;
    json["type"] = "vote";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return commitMessage(Json::writeString(wbuilder, json));
}

std::string
ConversationRepository::resolveVote(const std::string& uri, bool isDevice)
{
    // Count ratio admin/votes
    auto nbAdmins = 0, nbVotes = 0;
    // For each admin, check if voted
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());
    std::string adminsPath = repoPath + "admins";
    std::string membersPath = repoPath + "members";
    std::string devicesPath = repoPath + "devices";
    std::string bannedPath = repoPath + "banned";
    auto isAdmin = fileutils::isFile(fileutils::getFullPath(adminsPath, uri + ".crt"));
    std::string type = "members";
    if (isDevice)
        type = "devices";
    else if (isAdmin)
        type = "admins";

    auto voteDirectory = repoPath + DIR_SEPARATOR_STR + "votes" + DIR_SEPARATOR_STR
                         + (isDevice ? "devices" : "members") + DIR_SEPARATOR_STR + uri;
    for (const auto& certificate : fileutils::readDirectory(adminsPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", adminsPath.c_str(), certificate.c_str());
            continue;
        }
        auto adminUri = certificate.substr(0, certificate.size() - std::string(".crt").size());
        nbAdmins += 1;
        if (fileutils::isFile(fileutils::getFullPath(voteDirectory, adminUri)))
            nbVotes += 1;
    }

    if (nbAdmins > 0 && (static_cast<double>(nbVotes) / static_cast<double>(nbAdmins)) > .5) {
        JAMI_WARN("More than half of the admins voted to ban %s, apply the ban", uri.c_str());

        // Remove vote directory
        fileutils::removeAll(voteDirectory, true);

        // Move from device or members file into banned
        std::string originFilePath = membersPath;
        if (isDevice)
            originFilePath = devicesPath;
        else if (isAdmin)
            originFilePath = adminsPath;
        originFilePath += DIR_SEPARATOR_STR + uri + ".crt";
        auto destPath = bannedPath + DIR_SEPARATOR_STR + (isDevice ? "devices" : "members");
        auto destFilePath = destPath + DIR_SEPARATOR_STR + uri + ".crt";
        if (!fileutils::recursive_mkdir(destPath, 0700)) {
            JAMI_ERR("Error when creating %s. Abort resolving vote", destPath.c_str());
            return {};
        }

        if (std::rename(originFilePath.c_str(), destFilePath.c_str())) {
            JAMI_ERR("Error when moving %s to %s. Abort resolving vote",
                     originFilePath.c_str(),
                     destFilePath.c_str());
            return {};
        }

        // If members, remove related devices
        if (!isDevice) {
            for (const auto& certificate : fileutils::readDirectory(devicesPath)) {
                auto certPath = fileutils::getFullPath(devicesPath, certificate);
                auto deviceCert = fileutils::loadTextFile(certPath);
                try {
                    crypto::Certificate cert(deviceCert);
                    if (auto issuer = cert.issuer)
                        if (issuer->toString() == uri)
                            fileutils::remove(certPath, true);
                } catch (...) {
                    continue;
                }
            }
        }

        // Commit
        if (!git_add_all(repo.get()))
            return {};

        Json::Value json;
        json["action"] = "ban";
        json["uri"] = uri;
        json["type"] = "member";
        Json::StreamWriterBuilder wbuilder;
        wbuilder["commentStyle"] = "None";
        wbuilder["indentation"] = "";

        if (!isDevice) {
            std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
            auto updated = false;

            for (auto& member : pimpl_->members_) {
                if (member.uri == uri) {
                    updated = true;
                    member.role = MemberRole::BANNED;
                    break;
                }
            }
            if (!updated)
                pimpl_->members_.emplace_back(ConversationMember {uri, MemberRole::BANNED});
        }
        return commitMessage(Json::writeString(wbuilder, json));
    }

    // If vote nok
    return {};
}

std::pair<std::vector<ConversationCommit>, bool>
ConversationRepository::validFetch(const std::string& remoteDevice) const
{
    auto newCommit = remoteHead(remoteDevice);
    if (not pimpl_ or newCommit.empty())
        return {{}, false};
    auto commitsToValidate = pimpl_->behind(newCommit);
    std::reverse(std::begin(commitsToValidate), std::end(commitsToValidate));
    auto isValid = pimpl_->validCommits(commitsToValidate);
    if (isValid)
        return {commitsToValidate, false};
    return {{}, true};
}

bool
ConversationRepository::validClone() const
{
    return pimpl_->validCommits(logN("", 0));
}

std::optional<std::string>
ConversationRepository::linearizedParent(const std::string& commitId) const
{
    return pimpl_->linearizedParent(commitId);
}

void
ConversationRepository::removeBranchWith(const std::string& remoteDevice)
{
    git_remote* remote_ptr = nullptr;
    auto repo = pimpl_->repository();
    if (git_remote_lookup(&remote_ptr, repo.get(), remoteDevice.c_str()) < 0) {
        JAMI_WARN("No remote found with id: %s", remoteDevice.c_str());
        return;
    }
    GitRemote remote {remote_ptr, git_remote_free};

    git_remote_prune(remote.get(), nullptr);
}

std::vector<std::string>
ConversationRepository::getInitialMembers() const
{
    return pimpl_->getInitialMembers();
}

std::vector<ConversationMember>
ConversationRepository::members() const
{
    return pimpl_->members();
}

void
ConversationRepository::refreshMembers() const
{
    return pimpl_->initMembers();
}

void
ConversationRepository::pinCertificates()
{
    auto repo = pimpl_->repository();
    if (!repo)
        return;

    std::string repoPath = git_repository_workdir(repo.get());
    std::vector<std::string> paths = {repoPath + "admins",
                                      repoPath + "members",
                                      repoPath + "devices"};

    for (const auto& path : paths) {
        tls::CertificateStore::instance().pinCertificatePath(path, {});
    }
}

std::string
ConversationRepository::updateInfos(const std::map<std::string, std::string>& profile)
{
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto uri = std::string(account->getUsername());
    auto valid = false;
    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        for (const auto& member : pimpl_->members_) {
            if (member.uri == uri) {
                valid = member.role <= pimpl_->updateProfilePermLvl_;
                break;
            }
        }
    }
    if (!valid) {
        JAMI_ERR("Not enough authorization for updating infos");
        emitSignal<DRing::ConversationSignal::OnConversationError>(
            account->getAccountID(),
            pimpl_->id_,
            EUNAUTHORIZED,
            "Not enough authorization for updating infos");
        return {};
    }

    auto infosMap = infos();
    for (const auto& [k, v] : profile) {
        infosMap[k] = v;
    }
    auto repo = pimpl_->repository();
    std::string repoPath = git_repository_workdir(repo.get());
    auto profilePath = repoPath + "profile.vcf";
    auto file = fileutils::ofstream(profilePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", profilePath.c_str());
        return {};
    }
    file << vCard::Delimiter::BEGIN_TOKEN;
    file << vCard::Delimiter::END_LINE_TOKEN;
    file << vCard::Property::VCARD_VERSION;
    file << ":2.1";
    file << vCard::Delimiter::END_LINE_TOKEN;
    auto titleIt = infosMap.find("title");
    if (titleIt != infosMap.end()) {
        file << vCard::Property::FORMATTED_NAME;
        file << ":";
        file << titleIt->second;
        file << vCard::Delimiter::END_LINE_TOKEN;
    }
    auto descriptionIt = infosMap.find("description");
    if (descriptionIt != infosMap.end()) {
        file << vCard::Property::DESCRIPTION;
        file << ":";
        file << descriptionIt->second;
        file << vCard::Delimiter::END_LINE_TOKEN;
    }
    file << vCard::Property::PHOTO;
    file << vCard::Delimiter::SEPARATOR_TOKEN;
    file << vCard::Property::BASE64;
    auto avatarIt = infosMap.find("avatar");
    if (avatarIt != infosMap.end()) {
        // TODO type=png? store another way?
        file << ":";
        file << avatarIt->second;
    }
    file << vCard::Delimiter::END_LINE_TOKEN;
    file << vCard::Delimiter::END_TOKEN;
    file.close();

    if (!pimpl_->add("profile.vcf"))
        return {};
    Json::Value json;
    json["type"] = "application/update-profile";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    return pimpl_->commit(Json::writeString(wbuilder, json));
}

std::map<std::string, std::string>
ConversationRepository::infos() const
{
    try {
        auto repo = pimpl_->repository();
        std::string repoPath = git_repository_workdir(repo.get());
        auto profilePath = repoPath + "profile.vcf";
        std::map<std::string, std::string> result;
        if (fileutils::isFile(profilePath)) {
            auto content = fileutils::loadTextFile(profilePath);
            result = ConversationRepository::infosFromVCard(vCard::utils::toMap(content));
        }
        result["mode"] = std::to_string(static_cast<int>(mode()));
        return result;
    } catch (...) {
    }
    return {};
}

std::map<std::string, std::string>
ConversationRepository::infosFromVCard(const std::map<std::string, std::string>& details)
{
    std::map<std::string, std::string> result;
    for (const auto& [k, v] : details) {
        if (k == vCard::Property::FORMATTED_NAME) {
            result["title"] = v;
        } else if (k == vCard::Property::DESCRIPTION) {
            result["description"] = v;
        } else if (k.find(vCard::Property::PHOTO) == 0) {
            result["avatar"] = v;
        }
    }
    return result;
}

} // namespace jami
