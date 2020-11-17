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

using random_device = dht::crypto::random_device;

#include <ctime>
#include <fstream>
#include <json/json.h>
#include <regex>
#include <exception>

using namespace std::string_view_literals;
constexpr auto DIFF_REGEX = " +\\| +[0-9]+.*"sv;

namespace jami {

class ConversationRepository::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& id)
        : account_(account)
        , id_(id)
    {
        auto shared = account.lock();
        if (!shared)
            throw std::logic_error("No account detected when loading conversation");
        auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + id_;
        git_repository* repo = nullptr;
        // TODO share this repo with GitServer
        if (git_repository_open(&repo, path.c_str()) != 0)
            throw std::logic_error("Couldn't open " + path);
        repository_ = {std::move(repo), git_repository_free};
    }
    ~Impl()
    {
        if (repository_)
            repository_.reset();
    }

    GitSignature signature();
    bool mergeFastforward(const git_oid* target_oid, int is_unborn);
    bool createMergeCommit(git_index* index, const std::string& wanted_ref);

    bool add(const std::string& path);
    std::string commit(const std::string& msg);

    GitDiff diff(const std::string& idNew, const std::string& idOld) const;
    std::string diffStats(const GitDiff& diff) const;

    std::vector<ConversationCommit> log(const std::string& from, const std::string& to, unsigned n);

    std::weak_ptr<JamiAccount> account_;
    const std::string id_;
    GitRepository repository_ {nullptr, git_repository_free};
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
    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
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
    git_strarray array = {0};
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
 * @param repo      The git repository
 * @param account   The account who signs
 * @return          The first commit hash or empty if failed
 */
std::string
initial_commit(GitRepository& repo, const std::shared_ptr<JamiAccount>& account)
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

    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 "Initial commit",
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

bool
ConversationRepository::Impl::createMergeCommit(git_index* index, const std::string& wanted_ref)
{
    // The merge will occur between current HEAD and wanted_ref
    git_reference* head_ref_ptr = nullptr;
    if (git_repository_head(&head_ref_ptr, repository_.get()) < 0) {
        JAMI_ERR("Could not get HEAD reference");
        return false;
    }
    GitReference head_ref {head_ref_ptr, git_reference_free};

    // Maybe that's a ref, so DWIM it
    git_reference* merge_ref_ptr = nullptr;
    git_reference_dwim(&merge_ref_ptr, repository_.get(), wanted_ref.c_str());
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
        return false;
    }
    parents[0] = {parent, git_commit_free};
    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, wanted_ref.c_str()) < 0) {
        return false;
    }
    git_annotated_commit* annotated_ptr = nullptr;
    if (git_annotated_commit_lookup(&annotated_ptr, repository_.get(), &commit_id) < 0) {
        JAMI_ERR("Couldn't lookup commit %s", wanted_ref.c_str());
        return false;
    }
    GitAnnotatedCommit annotated {annotated_ptr, git_annotated_commit_free};
    if (git_commit_lookup(&parent, repository_.get(), git_annotated_commit_id(annotated.get()))
        < 0) {
        JAMI_ERR("Couldn't lookup commit %s", wanted_ref.c_str());
        return false;
    }
    parents[1] = {parent, git_commit_free};

    // Prepare our commit tree
    git_oid tree_oid;
    git_tree* tree = nullptr;
    if (git_index_write_tree(&tree_oid, index) < 0) {
        JAMI_ERR("Couldn't write index");
        return false;
    }
    if (git_tree_lookup(&tree, repository_.get(), &tree_oid) < 0) {
        JAMI_ERR("Couldn't lookup tree");
        return false;
    }

    // Commit
    git_buf to_sign = {};
    const git_commit* parents_ptr[2] {parents[0].get(), parents[1].get()};
    if (git_commit_create_buffer(&to_sign,
                                 repository_.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 stream.str().c_str(),
                                 tree,
                                 2,
                                 &parents_ptr[0])
        < 0) {
        JAMI_ERR("Could not create commit buffer");
        return false;
    }

    auto account = account_.lock();
    if (!account)
        false;
    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    git_oid commit_oid;
    if (git_commit_create_with_signature(&commit_oid,
                                         repository_.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Could not sign commit");
        return false;
    }

    auto commit_str = git_oid_tostr_s(&commit_oid);
    if (commit_str) {
        JAMI_INFO("New merge commit added with id: %s", commit_str);
        // Move commit to main branch
        git_reference* ref_ptr = nullptr;
        if (git_reference_create(&ref_ptr,
                                 repository_.get(),
                                 "refs/heads/main",
                                 &commit_oid,
                                 true,
                                 nullptr)
            < 0) {
            JAMI_WARN("Could not move commit to main");
        }
        git_reference_free(ref_ptr);
    }

    // We're done merging, cleanup the repository state
    git_repository_state_cleanup(repository_.get());

    return true;
}

bool
ConversationRepository::Impl::mergeFastforward(const git_oid* target_oid, int is_unborn)
{
    // Initialize target
    git_reference* target_ref_ptr = nullptr;
    if (is_unborn) {
        git_reference* head_ref_ptr = nullptr;
        // HEAD reference is unborn, lookup manually so we don't try to resolve it
        if (git_reference_lookup(&head_ref_ptr, repository_.get(), "HEAD") < 0) {
            JAMI_ERR("failed to lookup HEAD ref");
            return false;
        }
        GitReference head_ref {head_ref_ptr, git_reference_free};

        // Grab the reference HEAD should be pointing to
        const auto* symbolic_ref = git_reference_symbolic_target(head_ref.get());

        // Create our main reference on the target OID
        if (git_reference_create(&target_ref_ptr,
                                 repository_.get(),
                                 symbolic_ref,
                                 target_oid,
                                 0,
                                 nullptr)
            < 0) {
            JAMI_ERR("failed to create main reference");
            return false;
        }

    } else if (git_repository_head(&target_ref_ptr, repository_.get()) < 0) {
        // HEAD exists, just lookup and resolve
        JAMI_ERR("failed to get HEAD reference");
        return false;
    }
    GitReference target_ref {target_ref_ptr, git_reference_free};

    // Lookup the target object
    git_object* target_ptr = nullptr;
    if (git_object_lookup(&target_ptr, repository_.get(), target_oid, GIT_OBJ_COMMIT) != 0) {
        JAMI_ERR("failed to lookup OID %s", git_oid_tostr_s(target_oid));
        return false;
    }
    GitObject target {target_ptr, git_object_free};

    // Checkout the result so the workdir is in the expected state
    git_checkout_options ff_checkout_options;
    git_checkout_init_options(&ff_checkout_options, GIT_CHECKOUT_OPTIONS_VERSION);
    ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
    if (git_checkout_tree(repository_.get(), target.get(), &ff_checkout_options) != 0) {
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

    return 0;
}

bool
ConversationRepository::Impl::add(const std::string& path)
{
    if (!repository_)
        return false;
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repository_.get()) < 0) {
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

std::string
ConversationRepository::Impl::commit(const std::string& msg)
{
    if (!repository_)
        return {};

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
    if (git_repository_index(&index_ptr, repository_.get()) < 0) {
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
    if (git_tree_lookup(&tree_ptr, repository_.get(), &tree_id) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repository_.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repository_.get(), &commit_id) < 0) {
        JAMI_ERR("Could not look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_buf to_sign = {};
    const git_commit* head_ref[1] = {head_commit.get()};
    if (git_commit_create_buffer(&to_sign,
                                 repository_.get(),
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
                                         repository_.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Could not sign commit");
        return {};
    }

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, repository_.get(), "refs/heads/main", &commit_id, true, nullptr)
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

GitDiff
ConversationRepository::Impl::diff(const std::string& idNew, const std::string& idOld) const
{
    if (!repository_)
        return {nullptr, git_diff_free};

    // Retrieve tree for commit new
    git_oid oid;
    git_commit* commitNew = nullptr;
    if (idNew == "HEAD") {
        if (git_reference_name_to_id(&oid, repository_.get(), "HEAD") < 0) {
            JAMI_ERR("Cannot get reference for HEAD");
            return {nullptr, git_diff_free};
        }

        if (git_commit_lookup(&commitNew, repository_.get(), &oid) < 0) {
            JAMI_ERR("Could not look up HEAD commit");
            return {nullptr, git_diff_free};
        }
    } else {
        if (git_oid_fromstr(&oid, idNew.c_str()) < 0
            || git_commit_lookup(&commitNew, repository_.get(), &oid) < 0) {
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
        if (git_diff_tree_to_tree(&diff_ptr, repository_.get(), nullptr, treeNew.get(), {}) < 0) {
            JAMI_ERR("Could not get diff to empty repository");
            return {nullptr, git_diff_free};
        }
        return {diff_ptr, git_diff_free};
    }

    // Retrieve tree for commit old
    git_commit* commitOld = nullptr;
    if (git_oid_fromstr(&oid, idOld.c_str()) < 0
        || git_commit_lookup(&commitOld, repository_.get(), &oid) < 0) {
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
    if (git_diff_tree_to_tree(&diff_ptr, repository_.get(), treeOld.get(), treeNew.get(), {}) < 0) {
        JAMI_ERR("Could not get diff between %s and %s", idOld.c_str(), idNew.c_str());
        return {nullptr, git_diff_free};
    }
    return {diff_ptr, git_diff_free};
}

std::vector<ConversationCommit>
ConversationRepository::Impl::log(const std::string& from, const std::string& to, unsigned n)
{
    std::vector<ConversationCommit> commits {};

    git_oid oid;
    if (from.empty()) {
        if (git_reference_name_to_id(&oid, repository_.get(), "HEAD") < 0) {
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
    if (git_revwalk_new(&walker_ptr, repository_.get()) < 0
        || git_revwalk_push(walker_ptr, &oid) < 0) {
        JAMI_DBG("Couldn't init revwalker for conversation %s", id_.c_str());
        return commits;
    }
    GitRevWalker walker {walker_ptr, git_revwalk_free};
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL);

    auto x = git_oid_tostr_s(&oid);
    for (auto idx = 0; !git_revwalk_next(&oid, walker.get()); ++idx) {
        if (n != 0 && idx == n) {
            break;
        }
        git_commit* commit_ptr = nullptr;
        std::string id = git_oid_tostr_s(&oid);
        if (git_commit_lookup(&commit_ptr, repository_.get(), &oid) < 0) {
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
        if (git_commit_extract_signature(&signature,
                                         &signed_data,
                                         repository_.get(),
                                         &oid,
                                         "signature")
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
ConversationRepository::createConversation(const std::weak_ptr<JamiAccount>& account)
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
    auto id = initial_commit(repo, shared);
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
    if (git_clone(&rep, url.str().c_str(), path.c_str(), nullptr) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("Error when retrieving remote conversation: %s", err->message);
        return nullptr;
    }
    git_repository_free(rep);
    JAMI_INFO("New conversation cloned in %s", path.c_str());
    return std::make_unique<ConversationRepository>(account, conversationId);
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
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());

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
    Json::Value json;
    json["action"] = "add";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return pimpl_->commit(Json::writeString(wbuilder, json));
}

bool
ConversationRepository::fetch(const std::string& remoteDeviceId)
{
    // Fetch distant repository
    git_remote* remote_ptr = nullptr;
    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;

    // Assert that repository exists
    std::string channelName = "git://" + remoteDeviceId + '/' + pimpl_->id_;
    auto res = git_remote_lookup(&remote_ptr, pimpl_->repository_.get(), remoteDeviceId.c_str());
    if (res != 0) {
        if (res != GIT_ENOTFOUND) {
            JAMI_ERR("Couldn't lookup for remote %s", remoteDeviceId.c_str());
            return false;
        }
        if (git_remote_create(&remote_ptr,
                              pimpl_->repository_.get(),
                              remoteDeviceId.c_str(),
                              channelName.c_str())
            < 0) {
            JAMI_ERR("Could not create remote for repository for conversation %s",
                     pimpl_->id_.c_str());
            return false;
        }
    }
    GitRemote remote {remote_ptr, git_remote_free};

    if (git_remote_fetch(remote.get(), nullptr, &fetch_opts, "fetch") < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERR("Could not fetch remote repository for conversation %s: %s",
                     pimpl_->id_.c_str(),
                     err->message);
        return false;
    }

    return true;
}

std::string
ConversationRepository::remoteHead(const std::string& remoteDeviceId, const std::string& branch)
{
    git_remote* remote_ptr = nullptr;
    if (git_remote_lookup(&remote_ptr, pimpl_->repository_.get(), remoteDeviceId.c_str()) < 0) {
        JAMI_WARN("No remote found with id: %s", remoteDeviceId.c_str());
        return {};
    }
    GitRemote remote {remote_ptr, git_remote_free};

    git_reference* head_ref_ptr = nullptr;
    std::string remoteHead = "refs/remotes/" + remoteDeviceId + "/" + branch;
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, pimpl_->repository_.get(), remoteHead.c_str()) < 0) {
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
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());
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
ConversationRepository::logN(const std::string& last, unsigned n) const
{
    return pimpl_->log(last, "", n);
}

std::vector<ConversationCommit>
ConversationRepository::log(const std::string& from, const std::string& to) const
{
    return pimpl_->log(from, to, 0);
}

std::optional<ConversationCommit>
ConversationRepository::getCommit(const std::string& commitId) const
{
    auto commits = logN(commitId, 1);
    if (commits.empty())
        return std::nullopt;
    return std::move(commits[0]);
}

bool
ConversationRepository::merge(const std::string& merge_id)
{
    // First, the repository must be in a clean state
    int state = git_repository_state(pimpl_->repository_.get());
    if (state != GIT_REPOSITORY_STATE_NONE) {
        JAMI_ERR("Merge operation aborted: repository is in unexpected state %d", state);
        return false;
    }
    // Checkout main (to do a `git_merge branch`)
    if (git_repository_set_head(pimpl_->repository_.get(), "refs/heads/main") < 0) {
        JAMI_ERR("Merge operation aborted: couldn't checkout main branch");
        return false;
    }

    // Then check that merge_id exists
    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, merge_id.c_str()) < 0) {
        JAMI_ERR("Merge operation aborted: couldn't lookup commit %s", merge_id.c_str());
        return false;
    }
    git_annotated_commit* annotated_ptr = nullptr;
    if (git_annotated_commit_lookup(&annotated_ptr, pimpl_->repository_.get(), &commit_id) < 0) {
        JAMI_ERR("Merge operation aborted: couldn't lookup commit %s", merge_id.c_str());
        return false;
    }
    GitAnnotatedCommit annotated {annotated_ptr, git_annotated_commit_free};

    // Now, we can analyze which type of merge do we need
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    const git_annotated_commit* const_annotated = annotated.get();
    if (git_merge_analysis(&analysis, &preference, pimpl_->repository_.get(), &const_annotated, 1)
        < 0) {
        JAMI_ERR("Merge operation aborted: repository analysis failed");
        return false;
    }

    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
        JAMI_INFO("Already up-to-date");
        return true;
    } else if (analysis & GIT_MERGE_ANALYSIS_UNBORN
               || (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD
                   && !(preference & GIT_MERGE_PREFERENCE_NO_FASTFORWARD))) {
        if (analysis & GIT_MERGE_ANALYSIS_UNBORN)
            JAMI_INFO("Merge analysis result: Unborn");
        else
            JAMI_INFO("Merge analysis result: Fast-forward");
        const auto* target_oid = git_annotated_commit_id(annotated.get());

        if (pimpl_->mergeFastforward(target_oid, (analysis & GIT_MERGE_ANALYSIS_UNBORN)) < 0) {
            const git_error* err = giterr_last();
            if (err)
                JAMI_ERR("Fast forward merge failed: %s", err->message);
            return false;
        }
        return true;
    } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
        git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
        merge_opts.file_flags = GIT_MERGE_FILE_STYLE_DIFF3;
        git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
        checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_ALLOW_CONFLICTS;

        if (preference & GIT_MERGE_PREFERENCE_FASTFORWARD_ONLY) {
            JAMI_ERR("Fast-forward is preferred, but only a merge is possible");
            return false;
        }

        if (git_merge(pimpl_->repository_.get(), &const_annotated, 1, &merge_opts, &checkout_opts)
            < 0) {
            const git_error* err = giterr_last();
            if (err)
                JAMI_ERR("Git merge failed: %s", err->message);
            return false;
        }
    }

    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, pimpl_->repository_.get()) < 0) {
        JAMI_ERR("Merge operation aborted: could not open repository index");
        return false;
    }
    GitIndex index {index_ptr, git_index_free};
    if (git_index_has_conflicts(index.get())) {
        JAMI_WARN("Merge operation aborted: the merge operation resulted in some conflicts");
        return false;
    }
    auto result = pimpl_->createMergeCommit(index.get(), merge_id);
    JAMI_INFO("Merge done between %s and main", merge_id.c_str());
    return result;
}

std::string
ConversationRepository::diffStats(const std::string& newId, const std::string& oldId) const
{
    auto diff = pimpl_->diff(newId, oldId);
    if (!diff)
        return {};
    return pimpl_->diffStats(diff);
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
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());
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
    std::string membersPath = repoPath + "members" + DIR_SEPARATOR_STR + uri + ".crt";
    std::string memberFile = membersPath + DIR_SEPARATOR_STR + uri + ".crt";
    std::string adminsPath = repoPath + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    if (fileutils::isFile(memberFile) or fileutils::isFile(adminsPath)) {
        // Already member, nothing to commit
        return {};
    }
    // Remove invited/uri.crt
    std::string invitedPath = repoPath + "invited";
    fileutils::remove(fileutils::getFullPath(invitedPath, uri + ".crt"));
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
    if (!git_add_all(pimpl_->repository_.get())) {
        return {};
    }
    Json::Value json;
    json["action"] = "join";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
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
    auto deviceId = details[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto uri = details[DRing::Account::ConfProperties::USERNAME];
    auto name = details[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account
                   ->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    // Remove related files
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());

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

    if (!git_add_all(pimpl_->repository_.get())) {
        return {};
    }

    Json::Value json;
    json["action"] = "remove";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return commitMessage(Json::writeString(wbuilder, json));
}

void
ConversationRepository::erase()
{
    // First, we need to add the member file to the repository if not present
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());

    JAMI_DBG() << "Erasing " << repoPath;
    fileutils::removeAll(repoPath, true);
}

std::string
ConversationRepository::voteKick(const std::string& uri, bool isDevice)
{
    // Add vote + commit
    // TODO simplify
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto cert = account->identity().second;
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERR("Parent cert is null!");
        return {};
    }
    auto adminUri = parentCert->getId().toString();
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
    auto nbAdmins = 0, nbVote = 0;
    // For each admin, check if voted
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());
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
            nbVote += 1;
    }

    if (nbAdmins > 0 && (static_cast<double>(nbVote) / static_cast<double>(nbAdmins)) > .5) {
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
        if (!git_add_all(pimpl_->repository_.get()))
            return {};

        Json::Value json;
        json["action"] = "ban";
        json["uri"] = uri;
        json["type"] = "member";
        Json::StreamWriterBuilder wbuilder;
        wbuilder["commentStyle"] = "None";
        wbuilder["indentation"] = "";
        return commitMessage(Json::writeString(wbuilder, json));
    }

    // If vote nok
    return {};
}

} // namespace jami
