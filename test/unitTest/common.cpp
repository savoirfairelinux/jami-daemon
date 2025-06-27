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

#include "common.h"

/* Jami */
#include "account_const.h"
#include "jami.h"
#include "fileutils.h"
#include "manager.h"

#include "base64.h"

/* Make GCC quiet about unused functions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

void
wait_for_announcement_of(const std::vector<std::string> accountIDs,
                         std::chrono::seconds timeout)
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::vector<std::atomic_bool> accountsReady(accountIDs.size());

    size_t to_be_announced = accountIDs.size();

    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&,
             accountIDs = std::move(accountIDs)](const std::string& accountID,
                                                 const std::map<std::string, std::string>& details) {
                for (size_t i = 0; i < accountIDs.size(); ++i) {
                    if (accountIDs[i] != accountID) {
                        continue;
                    }

                    if (jami::Manager::instance().getAccount(accountID)->getAccountType() == "SIP") {
                        auto daemonStatus = details.at(libjami::Account::ConfProperties::Registration::STATUS);
                        if (daemonStatus != "REGISTERED") {
                            continue;
                        }
                    } else {
                        try {
                            if ("true"
                                != details.at(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED)) {
                                continue;
                            }
                        } catch (const std::out_of_range&) {
                            continue;
                        }
                    }

                    accountsReady[i] = true;
                    cv.notify_one();
                }
            }));

    JAMI_DBG("Waiting for %zu account to be announced...", to_be_announced);

    libjami::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(cv.wait_for(lk, timeout, [&] {
        for (const auto& rdy : accountsReady) {
            if (not rdy) {
                return false;
            }
        }

        return true;
    }));

    libjami::unregisterSignalHandlers();

    JAMI_DBG("%zu account announced!", to_be_announced);
}

void
wait_for_announcement_of(const std::string& accountId,
                         std::chrono::seconds timeout)
{
    wait_for_announcement_of(std::vector<std::string> {accountId}, timeout);
}

void
wait_for_removal_of(const std::vector<std::string> accounts,
                    std::chrono::seconds timeout)
{
    JAMI_INFO("Removing %zu accounts...", accounts.size());

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::atomic_bool accountsRemoved {false};

    size_t current = jami::Manager::instance().getAccountList().size();

    /* Prevent overflow */
    CPPUNIT_ASSERT(current >= accounts.size());

    size_t target = current - accounts.size();

    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountsChanged>([&]() {
            if (jami::Manager::instance().getAccountList().size() <= target) {
                accountsRemoved = true;
                cv.notify_one();
            }
        }));

    libjami::unregisterSignalHandlers();
    libjami::registerSignalHandlers(confHandlers);

    for (const auto& account : accounts) {
        jami::Manager::instance().removeAccount(account, true);
    }

    CPPUNIT_ASSERT(cv.wait_for(lk, timeout, [&] { return accountsRemoved.load(); }));

    libjami::unregisterSignalHandlers();
}

void
wait_for_removal_of(const std::string& account,
                    std::chrono::seconds timeout)
{
    wait_for_removal_of(std::vector<std::string>{account}, timeout);
}

std::map<std::string, std::string>
load_actors(const std::filesystem::path& from_yaml)
{
    std::map<std::string, std::string> actors {};

    std::ifstream file(from_yaml);

    CPPUNIT_ASSERT(file.is_open());

    YAML::Node node = YAML::Load(file);

    CPPUNIT_ASSERT(node.IsMap());

    auto default_account = node["default-account"];

    std::map<std::string, std::string> default_details = libjami::getAccountTemplate(default_account["type"].as<std::string>());
    if (default_account.IsMap()) {
        for (const auto& kv : default_account) {
            auto key = kv.first.as<std::string>();
            if (default_details.find(key) != default_details.end()) {
                default_details[key] = kv.second.as<std::string>();
            } else {
                default_details["Account." + key] = kv.second.as<std::string>();
            }
        }
    }

    auto accounts = node["accounts"];

    CPPUNIT_ASSERT(accounts.IsMap());

    for (const auto& kv : accounts) {
        auto account_name = kv.first.as<std::string>();
        auto account = kv.second.as<YAML::Node>();
        auto details = std::map<std::string, std::string>(default_details);

        for (const auto& detail : account) {
            auto key = detail.first.as<std::string>();
            if (details.find(key) != details.end()) {
                details[key] = detail.second.as<std::string>();
            } else {
                details["Account." + key] = detail.second.as<std::string>();
            }
        }

        actors[account_name] = jami::Manager::instance().addAccount(details);
    }

    return actors;
}

std::map<std::string, std::string>
load_actors_and_wait_for_announcement(const std::string& from_yaml)
{
    auto actors = load_actors(from_yaml);

    std::vector<std::string> wait_for;

    wait_for.reserve(actors.size());

    for (auto it = actors.cbegin(); it != actors.cend(); ++it) {
        wait_for.emplace_back(it->second);
    }

    wait_for_announcement_of(wait_for);

    return actors;
}

std::string
addCommit(git_repository* repo,
          const std::shared_ptr<jami::JamiAccount> account,
          const std::string& branch,
          const std::string& commit_msg)
{
    auto deviceId = jami::DeviceId(std::string(account->currentDeviceId()));
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId.toString();

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.to_c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr, git_signature_free};

    // Retrieve current HEAD
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo, "HEAD") < 0) {
        JAMI_ERR("Unable to get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo, &commit_id) < 0) {
        JAMI_ERR("Unable to look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    // Retrieve current index
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0) {
        JAMI_ERR("Unable to open repository index");
        return {};
    }
    GitIndex index {index_ptr, git_index_free};

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERR("Unable to write initial tree from index");
        return {};
    }

    git_tree* tree_ptr = nullptr;
    if (git_tree_lookup(&tree_ptr, repo, &tree_id) < 0) {
        JAMI_ERR("Unable to look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    git_buf to_sign = {};
#if LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR == 8 && \
    (LIBGIT2_VER_REVISION == 0 || LIBGIT2_VER_REVISION == 1 || LIBGIT2_VER_REVISION == 3)
    git_commit* const head_ref[1] = {head_commit.get()};
#else
    const git_commit* head_ref[1] = {head_commit.get()};
#endif
    if (git_commit_create_buffer(&to_sign,
                                 repo,
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 commit_msg.c_str(),
                                 tree.get(),
                                 1,
                                 &head_ref[0])
        < 0) {
        JAMI_ERR("Unable to create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = jami::base64::encode(signed_buf);
    if (git_commit_create_with_signature(&commit_id,
                                         repo,
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Unable to sign commit");
        return {};
    }

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_INFO("New commit added with id: %s", commit_str);
        // Move commit to main branch
        git_reference* ref_ptr = nullptr;
        std::string branch_name = "refs/heads/" + branch;
        if (git_reference_create(&ref_ptr, repo, branch_name.c_str(), &commit_id, true, nullptr)
            < 0) {
            JAMI_WARN("Unable to move commit to main");
        }
        git_reference_free(ref_ptr);
    }
    return commit_str ? commit_str : "";
}

#pragma GCC diagnostic pop
