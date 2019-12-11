/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <git2.h>

#include <ctime>
#include <fstream>

using GitRepository = std::unique_ptr<git_repository, decltype(&git_repository_free)>;

namespace jami {

/**
 * For now, this does nothing
 */
class ConversationRepository::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& id) : account_(account), id_(id) {}
    ~Impl() = default;

    std::weak_ptr<JamiAccount> account_;
    const std::string id_;
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
    git_index *index;
    git_strarray array = {0};

    if (git_repository_index(&index, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
        return false;
    }

    git_index_add_all(index, &array, 0, nullptr, nullptr);
    git_index_write(index);
    git_index_free(index);

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

    git_signature *sig;
	git_index *index;
	git_oid tree_id, commit_id;
	git_tree *tree;
    git_buf to_sign = {};

    // Sign commit's buffer
    if (git_signature_new(&sig, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
		JAMI_ERR("Unable to create a commit signature.");
        return {};
    }

    if (git_repository_index(&index, repo.get()) < 0) {
		JAMI_ERR("Could not open repository index");
        return {};
    }

	if (git_index_write_tree(&tree_id, index) < 0) {
		JAMI_ERR("Unable to write initial tree from index");
        return {};
    }

	git_index_free(index);

	if (git_tree_lookup(&tree, repo.get(), &tree_id) < 0) {
		JAMI_ERR("Could not look up initial tree");
        git_tree_free(tree);
        return {};
    }

    if (git_commit_create_buffer(&to_sign, repo.get(), sig, sig, nullptr, "Initial commit", tree, 0, nullptr) < 0) {
        JAMI_ERR("Could not create initial buffer");
        git_tree_free(tree);
        return {};
    }

    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());

    // git commit -S
    if (git_commit_create_with_signature(&commit_id, repo.get(), to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign initial commit");
        git_tree_free(tree);
        return {};
    }

    // Move commit to master branch
    git_commit* commit;
    if (git_commit_lookup(&commit, repo.get(), &commit_id) == 0) {
        git_reference* ref;
        git_branch_create(&ref, repo.get(), "master", commit, true);
        git_reference_free(ref);
        git_commit_free(commit);
    }

	git_tree_free(tree);
	git_signature_free(sig);

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

}