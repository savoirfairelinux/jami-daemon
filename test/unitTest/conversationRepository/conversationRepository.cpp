/*
 *  Copyright (C) 2017-2023 Savoir-faire Linux Inc.
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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "connectivity/connectionmanager.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"

#include <git2.h>
#include <filesystem>

using namespace std::string_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class ConversationRepositoryTest : public CppUnit::TestFixture
{
public:
    ConversationRepositoryTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConversationRepositoryTest() { libjami::fini(); }
    static std::string name() { return "ConversationRepository"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testCreateRepository();
    void testCloneViaChannelSocket();
    void testAddSomeMessages();
    void testLogMessages();
    void testFetch();
    void testMerge();
    void testFFMerge();
    void testDiff();
    // NOTE: Just for debug. the test is a bit complex to write
    // due to the clone/fetch verifications (initial commits, size).
    // void testCloneHugeRepo();

    void testMergeProfileWithConflict();

    std::string addCommit(git_repository* repo,
                          const std::shared_ptr<JamiAccount> account,
                          const std::string& branch,
                          const std::string& commit_msg);
    void addAll(git_repository* repo);
    bool merge_in_main(const std::shared_ptr<JamiAccount> account,
                       git_repository* repo,
                       const std::string& commit_ref);

    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
    CPPUNIT_TEST(testCreateRepository);
    CPPUNIT_TEST(testCloneViaChannelSocket);
    CPPUNIT_TEST(testAddSomeMessages);
    CPPUNIT_TEST(testLogMessages);
    CPPUNIT_TEST(testFetch);
    CPPUNIT_TEST(testMerge);
    CPPUNIT_TEST(testFFMerge);
    CPPUNIT_TEST(testDiff);
    CPPUNIT_TEST(testMergeProfileWithConflict);
    // CPPUNIT_TEST(testCloneHugeRepo);

    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryTest,
                                      ConversationRepositoryTest::name());

void
ConversationRepositoryTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
ConversationRepositoryTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
ConversationRepositoryTest::testCreateRepository()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();

    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // 1. Verify that last commit is correctly signed by alice
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

    git_buf signature = {}, signed_data = {};
    git_commit_extract_signature(&signature, &signed_data, repo, &commit_id, "signature");
    auto pk = base64::decode(std::string(signature.ptr, signature.ptr + signature.size));
    auto data = std::vector<uint8_t>(signed_data.ptr, signed_data.ptr + signed_data.size);
    git_repository_free(repo);

    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(data, pk));

    // 2. Check created files
    auto CRLsPath = repoPath + DIR_SEPARATOR_STR + "CRLs" + DIR_SEPARATOR_STR
                    + aliceDeviceId.toString();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto adminCrt = repoPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(adminCrt));

    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);

    CPPUNIT_ASSERT(adminCrtStr == parentCert);

    auto deviceCrt = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR
                     + aliceDeviceId.toString() + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));

    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)),
                             std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationRepositoryTest::testCloneViaChannelSocket()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, scv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
    std::shared_ptr<ChannelSocket> sendSocket = nullptr;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string& name) {
            successfullyReceive = name == "git://*";
            return true;
        });

    aliceAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string&) { return true; });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
            channelSocket = socket;
            rcv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            sendSocket = socket;
                                                        }
                                                        scv.notify_one();
                                                    });

    rcv.wait_for(lk, std::chrono::seconds(10));
    scv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);

    bobAccount->addGitSocket(aliceDeviceId, repository->id(), channelSocket);
    GitServer gs(aliceId, repository->id(), sendSocket);

    auto cloned = ConversationRepository::cloneConversation(bobAccount->weak(),
                                                            aliceDeviceId.toString(),
                                                            repository->id());
    gs.stop();

    CPPUNIT_ASSERT(cloned != nullptr);
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, clonedPath.c_str()) == 0);

    // 1. Verify that last commit is correctly signed by alice
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

    git_buf signature = {}, signed_data = {};
    git_commit_extract_signature(&signature, &signed_data, repo, &commit_id, "signature");
    auto pk = base64::decode(std::string(signature.ptr, signature.ptr + signature.size));
    auto data = std::vector<uint8_t>(signed_data.ptr, signed_data.ptr + signed_data.size);
    git_repository_free(repo);

    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(data, pk));

    // 2. Check created files
    auto CRLsPath = clonedPath + DIR_SEPARATOR_STR + "CRLs" + DIR_SEPARATOR_STR
                    + aliceDeviceId.toString();
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));

    auto adminCrt = clonedPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(adminCrt));

    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);

    CPPUNIT_ASSERT(adminCrtStr == parentCert);

    auto deviceCrt = clonedPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR
                     + aliceDeviceId.toString() + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));

    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)),
                             std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);

    // Check cloned messages
    auto messages = cloned->log();
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content,
                                                                       messages[0].signature));
}

void
ConversationRepositoryTest::testAddSomeMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    auto id1 = repository->commitMessage("Commit 1");
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    auto messages = repository->log();
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0].id == id3);
    CPPUNIT_ASSERT(messages[0].parents.front() == id2);
    CPPUNIT_ASSERT(messages[0].commit_msg == "Commit 3");
    CPPUNIT_ASSERT(messages[0].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[0].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[1].id == id2);
    CPPUNIT_ASSERT(messages[1].parents.front() == id1);
    CPPUNIT_ASSERT(messages[1].commit_msg == "Commit 2");
    CPPUNIT_ASSERT(messages[1].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[1].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].id == id1);
    CPPUNIT_ASSERT(messages[2].commit_msg == "Commit 1");
    CPPUNIT_ASSERT(messages[2].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[2].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].parents.front() == repository->id());
    // Check sig
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content,
                                                                       messages[0].signature));
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[1].signed_content,
                                                                       messages[1].signature));
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[2].signed_content,
                                                                       messages[2].signature));
}

void
ConversationRepositoryTest::testLogMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    auto id1 = repository->commitMessage("Commit 1");
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    LogOptions options;
    options.from = repository->id();
    options.nbOfCommits = 1;
    auto messages = repository->log(options);
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
    options.from = id2;
    options.nbOfCommits = 2;
    messages = repository->log(options);
    CPPUNIT_ASSERT(messages.size() == 2);
    CPPUNIT_ASSERT(messages[0].id == id2);
    CPPUNIT_ASSERT(messages[1].id == id1);
    options.from = repository->id();
    options.nbOfCommits = 3;
    messages = repository->log(options);
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
}

void
ConversationRepositoryTest::testFetch()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, scv, ccv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
    std::shared_ptr<ChannelSocket> sendSocket = nullptr;

    bobAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string& name) {
            successfullyReceive = name == "git://*";
            ccv.notify_one();
            return true;
        });

    aliceAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
            channelSocket = socket;
            rcv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            sendSocket = socket;
                                                        }
                                                        scv.notify_one();
                                                    });

    rcv.wait_for(lk, std::chrono::seconds(10));
    scv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
    CPPUNIT_ASSERT(repository != nullptr);

    bobAccount->addGitSocket(aliceDeviceId, repository->id(), channelSocket);
    GitServer gs(aliceId, repository->id(), sendSocket);

    // Clone repository
    auto id1 = repository->commitMessage("Commit 1");

    auto cloned = ConversationRepository::cloneConversation(bobAccount->weak(),
                                                            aliceDeviceId.toString(),
                                                            repository->id());
    gs.stop();
    bobAccount->removeGitSocket(aliceDeviceId, repository->id());

    // Add some new messages to fetch
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    // Open a new channel to simulate the fact that we are later
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            sendSocket = socket;
                                                        }
                                                        scv.notify_one();
                                                    });

    rcv.wait_for(lk, std::chrono::seconds(10));
    scv.wait_for(lk, std::chrono::seconds(10));
    ccv.wait_for(lk, std::chrono::seconds(10));
    bobAccount->addGitSocket(aliceDeviceId, repository->id(), channelSocket);
    GitServer gs2(aliceId, repository->id(), sendSocket);

    CPPUNIT_ASSERT(cloned->fetch(aliceDeviceId.toString()));
    CPPUNIT_ASSERT(id3 == cloned->remoteHead(aliceDeviceId.toString()));

    gs2.stop();
    bobAccount->removeGitSocket(aliceDeviceId, repository->id());

    auto messages = cloned->log({id3});
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0].id == id3);
    CPPUNIT_ASSERT(messages[0].parents.front() == id2);
    CPPUNIT_ASSERT(messages[0].commit_msg == "Commit 3");
    CPPUNIT_ASSERT(messages[0].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[0].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[1].id == id2);
    CPPUNIT_ASSERT(messages[1].parents.front() == id1);
    CPPUNIT_ASSERT(messages[1].commit_msg == "Commit 2");
    CPPUNIT_ASSERT(messages[1].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[1].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].id == id1);
    CPPUNIT_ASSERT(messages[2].commit_msg == "Commit 1");
    CPPUNIT_ASSERT(messages[2].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[2].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].parents.front() == repository->id());
    // Check sig
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content,
                                                                       messages[0].signature));
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[1].signed_content,
                                                                       messages[1].signature));
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[2].signed_content,
                                                                       messages[2].signature));
}

std::string
ConversationRepositoryTest::addCommit(git_repository* repo,
                                      const std::shared_ptr<JamiAccount> account,
                                      const std::string& branch,
                                      const std::string& commit_msg)
{
    auto deviceId = DeviceId(std::string(account->currentDeviceId()));
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
        JAMI_ERR("Cannot get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo, &commit_id) < 0) {
        JAMI_ERR("Could not look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    // Retrieve current index
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0) {
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
    if (git_tree_lookup(&tree_ptr, repo, &tree_id) < 0) {
        JAMI_ERR("Could not look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    git_buf to_sign = {};
    const git_commit* head_ref[1] = {head_commit.get()};
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
        JAMI_ERR("Could not create commit buffer");
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
        JAMI_ERR("Could not sign commit");
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
            JAMI_WARN("Could not move commit to main");
        }
        git_reference_free(ref_ptr);
    }
    return commit_str ? commit_str : "";
}

void
ConversationRepositoryTest::addAll(git_repository* repo)
{
    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr, git_index_free};
    git_strarray array = {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_free(&array);
}

void
ConversationRepositoryTest::testMerge()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "main", "Commit 1");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, repository->id().c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    auto id2 = addCommit(repo, aliceAccount, "to_merge", "Commit 2");
    git_repository_free(repo);

    // This will create a merge commit
    repository->merge(id2);

    CPPUNIT_ASSERT(repository->log().size() == 4 /* Initial, commit 1, 2, merge */);
}

void
ConversationRepositoryTest::testFFMerge()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "main", "Commit 1");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, id1.c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    auto id2 = addCommit(repo, aliceAccount, "to_merge", "Commit 2");
    git_repository_free(repo);

    // This will use a fast forward merge
    repository->merge(id2);

    CPPUNIT_ASSERT(repository->log().size() == 3 /* Initial, commit 1, 2 */);
}

void
ConversationRepositoryTest::testDiff()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    auto id1 = repository->commitMessage("Commit 1");
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    auto diff = repository->diffStats(id2, id1);
    CPPUNIT_ASSERT(ConversationRepository::changedFiles(diff).empty());
    diff = repository->diffStats(id1);
    auto changedFiles = ConversationRepository::changedFiles(diff);
    CPPUNIT_ASSERT(!changedFiles.empty());
    CPPUNIT_ASSERT(changedFiles[0] == "admins/" + uri + ".crt");
    CPPUNIT_ASSERT(changedFiles[1] == "devices/" + aliceDeviceId.toString() + ".crt");
}

void
ConversationRepositoryTest::testMergeProfileWithConflict()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    auto profile = std::ofstream(repoPath + DIR_SEPARATOR_STR + "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: Some description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id1 = addCommit(repo, aliceAccount, "main", "add profile");
    profile = std::ofstream(repoPath + DIR_SEPARATOR_STR + "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: New description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id2 = addCommit(repo, aliceAccount, "main", "modify profile");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, id1.c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    profile = std::ofstream(repoPath + DIR_SEPARATOR_STR + "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: Another description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id3 = addCommit(repo, aliceAccount, "to_merge", "modify profile merge");

    // This will create a merge commit
    repository->merge(id3);
    CPPUNIT_ASSERT(repository->log().size() == 5 /* Initial, add, modify 1, modify 2, merge */);
}

/*
void
ConversationRepositoryTest::testCloneHugeRepo()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    auto convPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations";
    fileutils::recursive_mkdir(convPath);
    const auto copyOptions = std::filesystem::copy_options::overwrite_existing |
std::filesystem::copy_options::recursive; auto repoPath = convPath + DIR_SEPARATOR_STR +
"8d3be095ebff73be1c43f193d02407b946d7895d"; std::filesystem::copy("/home/amarok/daemon/", repoPath,
copyOptions);

    auto repository = ConversationRepository(aliceAccount->weak(),
"8d3be095ebff73be1c43f193d02407b946d7895d"); auto clonedPath = fileutils::get_data_dir() +
DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository.id();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, scv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
    std::shared_ptr<ChannelSocket> sendSocket = nullptr;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&
name) { successfullyReceive = name == "git://*"; return true;
        });

    aliceAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&
name) { return true; });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
            channelSocket = socket;
            rcv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            sendSocket = socket;
                                                        }
                                                        scv.notify_one();
                                                    });

    ;
    scv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(rcv.wait_for(lk, std::chrono::seconds(10), [&] { return receiverConnected; }));
    CPPUNIT_ASSERT(scv.wait_for(lk, std::chrono::seconds(10), [&] { return successfullyConnected;
})); CPPUNIT_ASSERT(successfullyReceive);

    bobAccount->addGitSocket(aliceDeviceId, repository.id(), channelSocket);
    GitServer gs(aliceId, repository.id(), sendSocket);

    JAMI_ERR("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    auto cloned = ConversationRepository::cloneConversation(bobAccount->weak(),
                                                            aliceDeviceId,
                                                            repository.id());
    gs.stop();
    JAMI_ERR("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ END CLONE");

    CPPUNIT_ASSERT(cloned != nullptr);
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
}
*/

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
