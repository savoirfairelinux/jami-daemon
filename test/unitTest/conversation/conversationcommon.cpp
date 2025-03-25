/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <yaml-cpp/yaml.h>
#include <filesystem>

#include "common.h"

/* Jami */
#include "account_const.h"
#include "base64.h"
#include "jami.h"
#include "fileutils.h"
#include "manager.h"
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "conversation/conversationcommon.h"
#include "json_utils.h"

using namespace std::string_literals;

/* Make GCC quiet about unused functions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

namespace jami {

void
addVote(std::shared_ptr<JamiAccount> account,
        const std::string& convId,
        const std::string& votedUri,
        const std::string& content)
{
    ConversationRepository::DISABLE_RESET = true;
    auto repoPath = fileutils::get_data_dir() / account->getAccountID()
                    / "conversations" / convId;
    auto voteDirectory = repoPath / "votes" / "members";
    auto voteFile = voteDirectory / votedUri;
    if (!dhtnet::fileutils::recursive_mkdir(voteDirectory, 0700)) {
        return;
    }

    std::ofstream file(voteFile);
    if (file.is_open()) {
        file << content;
        file.close();
    }

    Json::Value json;
    json["uri"] = votedUri;
    json["type"] = "vote";
    ConversationRepository cr(account, convId);
    cr.commitMessage(json::toString(json), false);
}

void
simulateRemoval(std::shared_ptr<JamiAccount> account,
                const std::string& convId,
                const std::string& votedUri)
{
    ConversationRepository::DISABLE_RESET = true;
    auto repoPath = fileutils::get_data_dir() / account->getAccountID()
                    / "conversations" / convId;
    auto memberFile = repoPath / "members" / (votedUri + ".crt");
    auto bannedFile = repoPath / "banned" / "members"
                      / (votedUri + ".crt");
    std::rename(memberFile.c_str(), bannedFile.c_str());

    git_repository* repo = nullptr;
    if (git_repository_open(&repo, repoPath.c_str()) != 0)
        return;
    GitRepository rep = {std::move(repo), git_repository_free};

    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr, git_index_free};
    git_strarray array = {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_dispose(&array);

    ConversationRepository cr(account, convId);

    Json::Value json;
    json["action"] = "ban";
    json["uri"] = votedUri;
    json["type"] = "member";
    cr.commitMessage(json::toString(json));

    libjami::sendMessage(account->getAccountID(),
                       convId,
                       "trigger the fake history to be pulled"s,
                       "");
}

void
addFile(std::shared_ptr<JamiAccount> account,
        const std::string& convId,
        const std::string& relativePath,
        const std::string& content)
{
    ConversationRepository::DISABLE_RESET = true;
    auto repoPath = fileutils::get_data_dir() / account->getAccountID()
                    / "conversations" / convId;
    // Add file
    auto p = std::filesystem::path(fileutils::getFullPath(repoPath, relativePath));
    dhtnet::fileutils::recursive_mkdir(p.parent_path());
    std::ofstream file(p);
    if (file.is_open()) {
        file << content;
        file.close();
    }

    git_repository* repo = nullptr;
    if (git_repository_open(&repo, repoPath.c_str()) != 0)
        return;
    GitRepository rep = {std::move(repo), git_repository_free};

    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr, git_index_free};
    git_strarray array = {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_dispose(&array);
}

void
addAll(std::shared_ptr<JamiAccount> account, const std::string& convId)
{
    ConversationRepository::DISABLE_RESET = true;
    auto repoPath = fileutils::get_data_dir() / account->getAccountID()
                    / "conversations" / convId;

    git_repository* repo = nullptr;
    if (git_repository_open(&repo, repoPath.c_str()) != 0)
        return;
    GitRepository rep = {std::move(repo), git_repository_free};

    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr, git_index_free};
    git_strarray array = {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_dispose(&array);
}

void
commit(std::shared_ptr<JamiAccount> account, const std::string& convId, Json::Value& message)
{
    ConversationRepository::DISABLE_RESET = true;
    ConversationRepository cr(account, convId);
    cr.commitMessage(json::toString(message));
}

std::string
commitInRepo(const std::string& path, std::shared_ptr<JamiAccount> account, const std::string& msg)
{
    ConversationRepository::DISABLE_RESET = true;
    auto deviceId = std::string(account->currentDeviceId());
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERROR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr, git_signature_free};

    // Retrieve current index
    git_index* index_ptr = nullptr;
    git_repository* repo = nullptr;
    // TODO share this repo with GitServer
    if (git_repository_open(&repo, path.c_str()) != 0) {
        JAMI_ERROR("Unable to open repository");
        return {};
    }

    if (git_repository_index(&index_ptr, repo) < 0) {
        JAMI_ERROR("Unable to open repository index");
        return {};
    }
    GitIndex index {index_ptr, git_index_free};

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERROR("Unable to write initial tree from index");
        return {};
    }

    git_tree* tree_ptr = nullptr;
    if (git_tree_lookup(&tree_ptr, repo, &tree_id) < 0) {
        JAMI_ERROR("Unable to look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo, "HEAD") < 0) {
        JAMI_ERROR("Unable to get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo, &commit_id) < 0) {
        JAMI_ERROR("Unable to look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_buf to_sign = {};
#if( LIBGIT2_VER_MAJOR > 1 ) || ( LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR >= 8 )
    // For libgit2 version 1.8.0 and above
    git_commit* const head_ref[1] = {head_commit.get()};
#else
    // For libgit2 versions older than 1.8.0
    const git_commit* head_ref[1] = {head_commit.get()};
#endif
    if (git_commit_create_buffer(
            &to_sign, repo, sig.get(), sig.get(), nullptr, msg.c_str(), tree.get(), 1, &head_ref[0])
        < 0) {
        JAMI_ERROR("Unable to create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    if (git_commit_create_with_signature(&commit_id,
                                         repo,
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Unable to sign commit: {}", err->message);
        return {};
    }

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, repo, "refs/heads/main", &commit_id, true, nullptr) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Unable to move commit to main: {}", err->message);
        return {};
    }
    git_reference_free(ref_ptr);
    git_repository_free(repo);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_LOG("New message added with id: {}", commit_str);
        return commit_str;
    }

    return {};
}

} // namespace jami

#pragma GCC diagnostic pop
