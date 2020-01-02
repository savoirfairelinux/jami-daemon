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

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <git2.h>

#include <ctime>
#include <fstream>

using GitRepository = std::unique_ptr<git_repository, decltype(&git_repository_free)>;
using GitRevWalker = std::unique_ptr<git_revwalk, decltype(&git_revwalk_free)>;
using GitCommit = std::unique_ptr<git_commit, decltype(&git_commit_free)>;
using GitIndex = std::unique_ptr<git_index, decltype(&git_index_free)>;
using GitTree = std::unique_ptr<git_tree, decltype(&git_tree_free)>;
using GitRemote = std::unique_ptr<git_remote, decltype(&git_remote_free)>;
using GitSignature = std::unique_ptr<git_signature, decltype(&git_signature_free)>;

namespace jami {

class ConversationRepository::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& id) : account_(account), id_(id) {
        auto shared = account.lock();
        if (!shared) return;
        auto path = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+id_;
        git_repository *repo = nullptr;
        if (git_repository_open(&repo, path.c_str()) != 0) {
            JAMI_WARN("Couldn't open %s", path.c_str());
            return;
        }
        repository_ = {std::move(repo), git_repository_free};
    }
    ~Impl() = default;

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
create_empty_repository(const std::string path) {
    git_repository *repo = nullptr;
    if (git_repository_init(&repo, path.c_str(), false /* we want a non-bare repo to work on it */) < 0) {
        JAMI_ERR("Couldn't create a git repository in %s", path.c_str());
    }
    return {std::move(repo), git_repository_free};
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
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
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
    std::string adminPath = adminsPath + DIR_SEPARATOR_STR + parentCert->getId().toString() + ".crt";
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
        if (!crl) continue;
        auto v = crl->getNumber();
        std::stringstream ss;
        ss << std::hex;
        for (const auto& b : v)
            ss << (unsigned)b;
        std::string crlPath = crlsPath + DIR_SEPARATOR_STR + deviceId + DIR_SEPARATOR_STR + ss.str() + ".crl";
        file = fileutils::ofstream(crlPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERR("Could not write data to %s", crlPath.c_str());
            return false;
        }
        file << crl->toString();
        file.close();
    }

    // git add -A
    git_index* index_ptr = nullptr;
    git_strarray array = {0};

    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
        return false;
    }

    GitIndex index {index_ptr, git_index_free};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());

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
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto name = account->getAccountDetails()[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
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

    if (git_commit_create_buffer(&to_sign, repo.get(), sig.get(), sig.get(), nullptr, "Initial commit", tree.get(), 0, nullptr) < 0) {
        JAMI_ERR("Could not create initial buffer");
        return {};
    }

    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());

    // git commit -S
    if (git_commit_create_with_signature(&commit_id, repo.get(), to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign initial commit");
        return {};
    }

    // Move commit to master branch
    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, repo.get(), &commit_id) == 0) {
        git_reference* ref = nullptr;
        git_branch_create(&ref, repo.get(), "master", commit, true);
        git_commit_free(commit);
        git_reference_free(ref);
    }

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) return commit_str;
    return {};
}

std::unique_ptr<ConversationRepository>
ConversationRepository::createConversation(const std::weak_ptr<JamiAccount>& account)
{
    auto shared = account.lock();
    if (!shared) return {};
    // Create temporary directory because we can't know the first hash for now
    std::uniform_int_distribution<uint64_t> dist{ 0, std::numeric_limits<uint64_t>::max() };
    random_device rdev;
    auto tmpPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+std::to_string(dist(rdev));
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
    auto newPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+id;
    if (std::rename(tmpPath.c_str(), newPath.c_str())) {
        JAMI_ERR("Couldn't move %s in %s", tmpPath.c_str(), newPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    JAMI_INFO("New conversation initialized in %s", newPath.c_str());

    return std::make_unique<ConversationRepository>(account, id);
}

std::unique_ptr<ConversationRepository>
ConversationRepository::cloneConversation(
    const std::weak_ptr<JamiAccount>& account,
    const std::string& deviceId,
    const std::string& conversationId
)
{
    auto shared = account.lock();
    if (!shared) return {};
    auto path = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+conversationId;
    git_repository* rep = nullptr;
    std::stringstream url;
    url << "git://" << deviceId << '/' << conversationId;
    if (git_clone(&rep, url.str().c_str(), path.c_str(), nullptr) < 0) {
        const git_error *err = giterr_last();
        if (err) JAMI_ERR("Error when retrieving remote conversation: %s", err->message);
        return nullptr;
    }
    git_repository_free(rep);
    JAMI_INFO("New conversation cloned in %s", path.c_str());
    return std::make_unique<ConversationRepository>(account, conversationId);
}

/////////////////////////////////////////////////////////////////////////////////

ConversationRepository::ConversationRepository(const std::weak_ptr<JamiAccount>& account, const std::string& id)
: pimpl_ { new Impl { account, id } }
{}

ConversationRepository::~ConversationRepository() = default;

std::string
ConversationRepository::id() const
{
    return pimpl_->id_;
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
        if (git_remote_create(&remote_ptr, pimpl_->repository_.get(),
            remoteDeviceId.c_str(), channelName.c_str()) < 0) {
            JAMI_ERR("Could not create remote for repository for conversation %s", pimpl_->id_.c_str());
            return false;
        }
    }
    GitRemote remote {remote_ptr, git_remote_free};

    if (git_remote_fetch(remote.get(), nullptr, &fetch_opts, "fetch") < 0) {
        JAMI_ERR("Could not fetch remote repository for conversation %s", pimpl_->id_.c_str());
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

    git_reference *head_ref_ptr = nullptr;
    std::string remoteHead = "refs/remotes/" + remoteDeviceId + "/" + branch;
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, pimpl_->repository_.get(), remoteHead.c_str()) < 0) {
        JAMI_ERR("failed to lookup %s ref", remoteHead.c_str());
        return {};
    }
    GitReference head_ref {head_ref_ptr, git_reference_free};

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (!commit_str) return {};
    return commit_str;
}


std::string
ConversationRepository::sendMessage(const std::string& msg)
{
    auto account = pimpl_->account_.lock();
    if (!account) return {};
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto name = account->getAccountDetails()[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    // First, we need to add device file to the repository if not present
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());
    std::string devicePath = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + deviceId + ".crt";
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

        // git add
        git_index* index_ptr = nullptr;
        if (git_repository_index(&index_ptr, pimpl_->repository_.get()) < 0) {
            JAMI_ERR("Could not open repository index");
            return {};
        }
        GitIndex index {index_ptr, git_index_free};

        git_index_add_bypath(index.get(), devicePath.c_str());
        git_index_write(index.get());
    }

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
		JAMI_ERR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr, git_signature_free};

    // Retrieve current HEAD
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, pimpl_->repository_.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, pimpl_->repository_.get(), &commit_id) < 0) {
		JAMI_ERR("Could not look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_tree* tree_ptr = nullptr;
	if (git_commit_tree(&tree_ptr, head_commit.get()) < 0) {
		JAMI_ERR("Could not look up initial tree");
        return {};
    }
    GitTree tree {tree_ptr, git_tree_free};

    git_buf to_sign = {};
    const git_commit* head_ref[1] = { head_commit.get() };
    if (git_commit_create_buffer(&to_sign, pimpl_->repository_.get(),
            sig.get(), sig.get(), nullptr, msg.c_str(), tree.get(), 1, &head_ref[0]) < 0) {
        JAMI_ERR("Could not create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());
    if (git_commit_create_with_signature(&commit_id, pimpl_->repository_.get(), to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign commit");
        return {};
    }

    // Move commit to master branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, pimpl_->repository_.get(), "refs/heads/master", &commit_id, true, nullptr) < 0) {
        JAMI_WARN("Could not move commit to master");
    }
    git_reference_free(ref_ptr);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_INFO("New message added with id: %s", commit_str);
    }
    return commit_str ? commit_str : "";
}

std::vector<ConversationCommit>
ConversationRepository::log(const std::string& last, unsigned n)
{
    std::vector<ConversationCommit> commits {};

    git_oid oid;
    if (last.empty()) {
        if (git_reference_name_to_id(&oid, pimpl_->repository_.get(), "HEAD") < 0) {
            JAMI_ERR("Cannot get reference for HEAD");
            return commits;
        }
    } else {
        git_commit* commit = nullptr;
        if (git_oid_fromstr(&oid, last.c_str()) < 0
            || git_commit_lookup(&commit, pimpl_->repository_.get(), &oid) < 0) {
            JAMI_WARN("Failed to look up commit %s", last.c_str());
        } else {
            git_commit_free(commit);
        }
    }

    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, pimpl_->repository_.get()) < 0
        || git_revwalk_push(walker_ptr, &oid) < 0) {
        JAMI_ERR("Couldn't init revwalker for conversation %s", pimpl_->id_);
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
        if (git_commit_lookup(&commit_ptr, pimpl_->repository_.get(), &oid) < 0) {
            JAMI_WARN("Failed to look up commit %s", id);
            break;
        }
        GitCommit commit {commit_ptr, git_commit_free};

        const git_signature* sig = git_commit_author(commit.get());
        GitAuthor author;
        author.name = sig->name;
        author.email = sig->email;
        std::string parent {};
        const git_oid* pid = git_commit_parent_id(commit.get(), 0);
        if (pid) {
            parent = git_oid_tostr_s(pid);
        }

        auto cc = commits.emplace(commits.end(), ConversationCommit {});
        cc->id = id;
        cc->commit_msg = git_commit_message(commit.get());
        cc->author = author;
        cc->parent = parent;
        git_buf signature = {}, signed_data = {};
        if (git_commit_extract_signature(&signature, &signed_data, pimpl_->repository_.get(), &oid, "signature") < 0) {
            JAMI_WARN("Could not extract signature for commit %s", id);
        } else {
            cc->signature = base64::decode(std::string(signature.ptr, signature.ptr + signature.size));
            cc->signed_content  = std::vector<uint8_t>(signed_data.ptr, signed_data.ptr + signed_data.size);
        }
        cc->timestamp = git_commit_time(commit.get());
    }

    return commits;
}


}