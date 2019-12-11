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
#include "jamiaccount.h"
#include "fileutils.h"

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <git2.h>

#include <ctime>

namespace jami {

class ConversationRepository::Impl
{
public:
    Impl(const std::string& id) : id_(id) {}
    ~Impl() = default;

    const std::string id_;
};

/////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<git_repository, decltype(&git_repository_free)>
create_empty_repository(const std::string path) {
    git_repository *repo = nullptr;
    if (git_repository_init(&repo, path.c_str(), false /* we want a non-bare repo to work on it */) < 0) {
        JAMI_ERR("Couldn't create a git repository in %s", path.c_str());
    }
    return {std::move(repo), git_repository_free};
}

std::string
initial_commit(std::unique_ptr<git_repository, decltype(&git_repository_free)>& repo, const std::string& name)
{
    git_signature *sig;
	git_index *index;
	git_oid tree_id, commit_id;
	git_tree *tree;

    auto identifier = name + "@jami";
    if (git_signature_new(&sig, name.c_str(), identifier.c_str(), std::time(nullptr), 0) < 0)
		JAMI_ERR("Unable to create a commit signature.");

    if (git_repository_index(&index, repo.get()) < 0)
		JAMI_ERR("Could not open repository index");

	if (git_index_write_tree(&tree_id, index) < 0)
		JAMI_ERR("Unable to write initial tree from index");

	git_index_free(index);

	if (git_tree_lookup(&tree, repo.get(), &tree_id) < 0)
		JAMI_ERR("Could not look up initial tree");

	if (git_commit_create_v(
			&commit_id, repo.get(), "HEAD", sig, sig,
			NULL, "Initial commit", tree, 0) < 0)
		JAMI_ERR("Could not create the initial commit");

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
    std::uniform_int_distribution<uint64_t> dist{ 0, std::numeric_limits<uint64_t>::max() };
    random_device rdev;
    auto tmpPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+std::to_string(dist(rdev));
    if (fileutils::isDirectory(tmpPath)) {
        JAMI_ERR("%s already exists. Abort create conversations", tmpPath.c_str());
        return {};
    }
    if (!fileutils::recursive_mkdir(tmpPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations");
        return {};
    }
    auto repo = create_empty_repository(tmpPath);
    if (!repo) {
        return {};
    }
    auto deviceId = shared->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto id = initial_commit(repo, deviceId);
    if (id.empty()) {
        JAMI_ERR("Couldn't create initial commit in %s", tmpPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }
    auto newPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+id;
    if (std::rename(tmpPath.c_str(), newPath.c_str())) {
        JAMI_ERR("Couldn't move %s in %s", tmpPath.c_str(), newPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    JAMI_INFO("New conversation initialized in %s", newPath.c_str());

    return std::make_unique<ConversationRepository>(id);
}

/////////////////////////////////////////////////////////////////////////////////

ConversationRepository::ConversationRepository(const std::string& id)
: pimpl_ { new Impl { id } }
{}

ConversationRepository::~ConversationRepository() = default;

std::string
ConversationRepository::id() const
{
    return pimpl_->id_;
}

}