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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"

#include <git2.h>

using namespace std::string_literals;
using namespace DRing::Account;

namespace jami { namespace test {

class ConversationRepositoryTest : public CppUnit::TestFixture {
public:
    ~ConversationRepositoryTest() {
        DRing::fini();
    }
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

    std::string addCommit(git_repository* repo, const std::shared_ptr<JamiAccount> account, const std::string& branch, const std::string& commit_msg);
    bool merge_in_master(const std::shared_ptr<JamiAccount> account, git_repository* repo, const std::string& commit_ref);

    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
    CPPUNIT_TEST(testCreateRepository);
    CPPUNIT_TEST(testCloneViaChannelSocket);
    CPPUNIT_TEST(testAddSomeMessages);
    CPPUNIT_TEST(testLogMessages);
    CPPUNIT_TEST(testFetch);
    CPPUNIT_TEST(testMerge);
    CPPUNIT_TEST(testFFMerge);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryTest, ConversationRepositoryTest::name());

void
ConversationRepositoryTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    bool ready = false;
    bool idx = 0;
    while(!ready && idx < 100) {
        auto details = aliceAccount->getVolatileAccountDetails();
        auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready = (daemonStatus == "REGISTERED");
        details = bobAccount->getVolatileAccountDetails();
        daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready &= (daemonStatus == "REGISTERED");
        if (!ready) {
            idx += 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void
ConversationRepositoryTest::tearDown()
{
    auto currentAccSize = Manager::instance().getAccountList().size();
    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);
    // Because cppunit is not linked with dbus, just poll if removed
    for (int i = 0; i < 40; ++i) {
        if (Manager::instance().getAccountList().size() <= currentAccSize - 2) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void
ConversationRepositoryTest::testCreateRepository()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];
    auto uri = aliceAccount->getAccountDetails()[ConfProperties::USERNAME];
    if (uri.find("ring:") == 0)
        uri = uri.substr(std::string("ring:").size());

    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
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
    auto CRLsPath = repoPath + DIR_SEPARATOR_STR + "CRLs" + DIR_SEPARATOR_STR + aliceDeviceId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto adminCrt = repoPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(adminCrt));

    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);

    CPPUNIT_ASSERT(adminCrtStr == parentCert);

    auto deviceCrt = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + aliceDeviceId + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));

    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationRepositoryTest::testCloneViaChannelSocket()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceDeviceId = aliceAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];
    auto uri = aliceAccount->getAccountDetails()[ConfProperties::USERNAME];
    if (uri.find("ring:") == 0)
        uri = uri.substr(std::string("ring:").size());
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    bobAccount->connectionManager().onICERequest([](const std::string&) {return true;});
    aliceAccount->connectionManager().onICERequest([](const std::string&) {return true;});
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
    auto clonedPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+bobAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable rcv, scv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
    std::shared_ptr<ChannelSocket> sendSocket = nullptr;

    bobAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        successfullyReceive = name == "git://*";
        return true;
    });

    aliceAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        receiverConnected = socket && (name == "git://*");
        channelSocket = socket;
        rcv.notify_one();
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&](std::shared_ptr<ChannelSocket> socket) {
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
    std::thread sendT = std::thread([&]() {
        gs.run();
    });

    auto cloned = ConversationRepository::cloneConversation(bobAccount->weak(), aliceDeviceId, repository->id());
    gs.stop();
    sendT.join();

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
    auto CRLsPath = clonedPath + DIR_SEPARATOR_STR + "CRLs" + DIR_SEPARATOR_STR + aliceDeviceId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));

    auto adminCrt = clonedPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(adminCrt));

    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);

    CPPUNIT_ASSERT(adminCrtStr == parentCert);

    auto deviceCrt = clonedPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + aliceDeviceId + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));

    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);

    // Check cloned messages
    auto messages = cloned->log();
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content, messages[0].signature));
}

void
ConversationRepositoryTest::testAddSomeMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    auto id1 = repository->sendMessage("Commit 1");
    auto id2 = repository->sendMessage("Commit 2");
    auto id3 = repository->sendMessage("Commit 3");

    auto messages = repository->log();
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0].id == id3);
    CPPUNIT_ASSERT(messages[0].parent == id2);
    CPPUNIT_ASSERT(messages[0].commit_msg == "Commit 3");
    CPPUNIT_ASSERT(messages[0].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[0].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[1].id == id2);
    CPPUNIT_ASSERT(messages[1].parent == id1);
    CPPUNIT_ASSERT(messages[1].commit_msg == "Commit 2");
    CPPUNIT_ASSERT(messages[1].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[1].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].id == id1);
    CPPUNIT_ASSERT(messages[2].commit_msg == "Commit 1");
    CPPUNIT_ASSERT(messages[2].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[2].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].parent == repository->id());
    // Check sig
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content, messages[0].signature));
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[1].signed_content, messages[1].signature));
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[2].signed_content, messages[2].signature));
}

void
ConversationRepositoryTest::testLogMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    auto id1 = repository->sendMessage("Commit 1");
    auto id2 = repository->sendMessage("Commit 2");
    auto id3 = repository->sendMessage("Commit 3");

    auto messages = repository->log(repository->id(), 1);
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
    messages = repository->log(id2, 2);
    CPPUNIT_ASSERT(messages.size() == 2);
    CPPUNIT_ASSERT(messages[0].id == id2);
    CPPUNIT_ASSERT(messages[1].id == id1);
    messages = repository->log(repository->id(), 3);
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
}

void
ConversationRepositoryTest::testFetch()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceDeviceId = aliceAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    bobAccount->connectionManager().onICERequest([](const std::string&) {return true;});
    aliceAccount->connectionManager().onICERequest([](const std::string&) {return true;});
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
    auto clonedPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+bobAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable rcv, scv, ccv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> channelSocket = nullptr;
    std::shared_ptr<ChannelSocket> sendSocket = nullptr;

    bobAccount->connectionManager().onChannelRequest(
    [&](const std::string&, const std::string& name) {
        successfullyReceive = name == "git://*";
        ccv.notify_one();
        return true;
    });

    aliceAccount->connectionManager().onChannelRequest(
    [&](const std::string&, const std::string& name) {
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        receiverConnected = socket && (name == "git://*");
        channelSocket = socket;
        rcv.notify_one();
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&](std::shared_ptr<ChannelSocket> socket) {
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
    std::thread sendT = std::thread([&]() {
        gs.run();
    });

    // Clone repository
    auto id1 = repository->sendMessage("Commit 1");
    auto cloned = ConversationRepository::cloneConversation(bobAccount->weak(), aliceDeviceId, repository->id());
    gs.stop();
    sendT.join();
    bobAccount->removeGitSocket(aliceDeviceId, repository->id());

    // Add some new messages to fetch
    auto id2 = repository->sendMessage("Commit 2");
    auto id3 = repository->sendMessage("Commit 3");

    // Open a new channel to simulate the fact that we are later
    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&](std::shared_ptr<ChannelSocket> socket) {
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
    std::thread sendT2 = std::thread([&]() {
        gs2.run();
    });

    CPPUNIT_ASSERT(cloned->fetch(aliceDeviceId));
    CPPUNIT_ASSERT(id3 == cloned->remoteHead(aliceDeviceId));

    gs2.stop();
    bobAccount->removeGitSocket(aliceDeviceId, repository->id());
    sendT2.join();

    auto messages = cloned->log(id3);
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0].id == id3);
    CPPUNIT_ASSERT(messages[0].parent == id2);
    CPPUNIT_ASSERT(messages[0].commit_msg == "Commit 3");
    CPPUNIT_ASSERT(messages[0].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[0].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[1].id == id2);
    CPPUNIT_ASSERT(messages[1].parent == id1);
    CPPUNIT_ASSERT(messages[1].commit_msg == "Commit 2");
    CPPUNIT_ASSERT(messages[1].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[1].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].id == id1);
    CPPUNIT_ASSERT(messages[2].commit_msg == "Commit 1");
    CPPUNIT_ASSERT(messages[2].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[2].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].parent == repository->id());
    // Check sig
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content, messages[0].signature));
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[1].signed_content, messages[1].signature));
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[2].signed_content, messages[2].signature));
}

std::string
ConversationRepositoryTest::addCommit(git_repository* repo, const std::shared_ptr<JamiAccount> account, const std::string& branch, const std::string& commit_msg)
{
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto name = account->getAccountDetails()[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
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

    git_tree* tree_ptr = nullptr;
	if (git_commit_tree(&tree_ptr, head_commit.get()) < 0) {
		JAMI_ERR("Could not look up initial tree");
        return {};
    }
    GitTree tree {tree_ptr, git_tree_free};

    git_buf to_sign = {};
    const git_commit* head_ref[1] = { head_commit.get() };
    if (git_commit_create_buffer(&to_sign, repo,
            sig.get(), sig.get(), nullptr, commit_msg.c_str(), tree.get(), 1, &head_ref[0]) < 0) {
        JAMI_ERR("Could not create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());
    if (git_commit_create_with_signature(&commit_id, repo, to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign commit");
        return {};
    }

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_INFO("New commit added with id: %s", commit_str);
        // Move commit to master branch
        git_reference* ref_ptr = nullptr;
        std::string branch_name = "refs/heads/" + branch;
        if (git_reference_create(&ref_ptr, repo, branch_name.c_str(), &commit_id, true, nullptr) < 0) {
            JAMI_WARN("Could not move commit to master");
        }
        git_reference_free(ref_ptr);
    }
    return commit_str ? commit_str : "";
}

void
ConversationRepositoryTest::testMerge()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "master", "Commit 1");

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
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "master", "Commit 1");

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

}} // namespace test

RING_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
