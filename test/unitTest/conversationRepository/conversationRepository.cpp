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

    std::string addCommit(git_repository* repo, const std::shared_ptr<JamiAccount> account, const std::string& branch, const std::string& commit_msg);
    bool merge_in_master(const std::shared_ptr<JamiAccount> account, git_repository* repo, const std::string& commit_ref);

    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
    //CPPUNIT_TEST(testCreateRepository);
    //CPPUNIT_TEST(testCloneViaChannelSocket);
    //CPPUNIT_TEST(testAddSomeMessages);
    //CPPUNIT_TEST(testLogMessages);
    //CPPUNIT_TEST(testFetch);
    CPPUNIT_TEST(testMerge);
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
    Manager::instance().removeAccount(aliceId, false);
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

    auto adminCrt = repoPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + aliceDeviceId + ".crt";
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

    auto adminCrt = clonedPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + aliceDeviceId + ".crt";
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

static int perform_fastforward(git_repository *repo, const git_oid *target_oid, int is_unborn)
{
	git_checkout_options ff_checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
	git_reference *target_ref;
	git_reference *new_target_ref;
	git_object *target = NULL;
	int err = 0;

	if (is_unborn) {
		const char *symbolic_ref;
		git_reference *head_ref;

		/* HEAD reference is unborn, lookup manually so we don't try to resolve it */
		err = git_reference_lookup(&head_ref, repo, "HEAD");
		if (err != 0) {
			fprintf(stderr, "failed to lookup HEAD ref\n");
			return -1;
		}

		/* Grab the reference HEAD should be pointing to */
		symbolic_ref = git_reference_symbolic_target(head_ref);

		/* Create our master reference on the target OID */
		err = git_reference_create(&target_ref, repo, symbolic_ref, target_oid, 0, NULL);
		if (err != 0) {
			fprintf(stderr, "failed to create master reference\n");
			return -1;
		}

		git_reference_free(head_ref);
	} else {
		/* HEAD exists, just lookup and resolve */
		err = git_repository_head(&target_ref, repo);
		if (err != 0) {
			fprintf(stderr, "failed to get HEAD reference\n");
			return -1;
		}
	}

	/* Lookup the target object */
	err = git_object_lookup(&target, repo, target_oid, GIT_OBJ_COMMIT);
	if (err != 0) {
		fprintf(stderr, "failed to lookup OID %s\n", git_oid_tostr_s(target_oid));
		return -1;
	}

	/* Checkout the result so the workdir is in the expected state */
	ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
	err = git_checkout_tree(repo, target, &ff_checkout_options);
	if (err != 0) {
		fprintf(stderr, "failed to checkout HEAD reference\n");
		return -1;
	}

	/* Move the target reference to the target OID */
	err = git_reference_set_target(&new_target_ref, target_ref, target_oid, NULL);
	if (err != 0) {
		fprintf(stderr, "failed to move HEAD reference\n");
		return -1;
	}

	git_reference_free(target_ref);
	git_reference_free(new_target_ref);
	git_object_free(target);

	return 0;
}

static int
create_merge_commit(git_repository *repo, const std::shared_ptr<JamiAccount> account, git_index *index, const std::string& wanted_ref)
{
    git_oid tree_oid, commit_oid;
	git_tree *tree;
	git_reference *merge_ref = NULL;
	git_annotated_commit *merge_commit;
	git_reference *head_ref;
	git_commit **parents = (git_commit **)calloc(1 + 1, sizeof(git_commit *));
	const char *msg_target = NULL;
	size_t msglen = 0;
	char *msg;
	size_t i;
	int err;

	/* Grab our needed references */
	git_repository_head(&head_ref, repo);
	/* Maybe that's a ref, so DWIM it */
	err = git_reference_dwim(&merge_ref, repo, wanted_ref.c_str());

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

#define MERGE_COMMIT_MSG "Merge %s '%s'"
	/* Prepare a standard merge commit message */
	if (merge_ref != NULL) {
        git_branch_name(&msg_target, merge_ref);
	} else {
		msg_target = wanted_ref.c_str();
	}

	msglen = snprintf(NULL, 0, MERGE_COMMIT_MSG, (merge_ref ? "branch" : "commit"), msg_target);
	if (msglen > 0) msglen++;
	msg = (char*)malloc(msglen);
	err = snprintf(msg, msglen, MERGE_COMMIT_MSG, (merge_ref ? "branch" : "commit"), msg_target);

	/* This is only to silence the compiler */
	if (err < 0) return false;

	/* Setup our parent commits */
	err = git_reference_peel((git_object **)&parents[0], head_ref, GIT_OBJ_COMMIT);
	for (i = 0; i < 1; i++) {
        git_oid commit_id;
        if (git_oid_fromstr(&commit_id, wanted_ref.c_str()) < 0) {
            return false;
        }
        git_annotated_commit* annotated = nullptr;
        if (git_annotated_commit_lookup(&annotated, repo, &commit_id) < 0) {
            JAMI_ERR("Couldn't lookup commit %s", wanted_ref.c_str());
            return false;
        }
		git_commit_lookup(&parents[i + 1], repo, git_annotated_commit_id(annotated));
	}

	/* Prepare our commit tree */
    git_index_write_tree(&tree_oid, index);
    git_tree_lookup(&tree, repo, &tree_oid);

	/* Commit time ! */

    git_buf to_sign = {};
    if (git_commit_create_buffer(&to_sign, repo,
            sig.get(), sig.get(), nullptr, msg, tree, 2, (const git_commit **)parents) < 0) {
        JAMI_ERR("Could not create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());
    if (git_commit_create_with_signature(&commit_oid, repo, to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign commit");
        return false;
    }


    auto commit_str = git_oid_tostr_s(&commit_oid);
    if (commit_str) {
        JAMI_INFO("New merge commit added with id: %s", commit_str);
        // Move commit to master branch
        git_reference* ref_ptr = nullptr;
        if (git_reference_create(&ref_ptr, repo, "refs/heads/master", &commit_oid, true, nullptr) < 0) {
            JAMI_WARN("Could not move commit to master");
        }
        git_reference_free(ref_ptr);
    }

	/* We're done merging, cleanup the repository state */
	git_repository_state_cleanup(repo);

	return err;
}

bool
ConversationRepositoryTest::merge_in_master(const std::shared_ptr<JamiAccount> account, git_repository* repo, const std::string& commit_ref)
{
    int state = git_repository_state(repo);
	if (state != GIT_REPOSITORY_STATE_NONE) {
        JAMI_ERR("Repository is in unexpected state %d", state);
        return false;
    }

    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, commit_ref.c_str()) < 0) {
        return false;
    }
	git_annotated_commit* annotated = nullptr;
    if (git_annotated_commit_lookup(&annotated, repo, &commit_id) < 0) {
        JAMI_ERR("Couldn't lookup commit %s", commit_ref.c_str());
        return false;
    }
    const git_annotated_commit* annotated_ptr = annotated;

    if (git_repository_set_head(repo, "refs/heads/master") < 0) {
        JAMI_ERR("Couldn't checkout master branch");
        return false;
    }

	git_merge_analysis_t analysis;
	git_merge_preference_t preference;
    if (git_merge_analysis(&analysis, &preference, repo, &annotated_ptr, 1) < 0) {
        JAMI_ERR("Repository analysis failed");
        return false;
    }

    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
        JAMI_INFO("Already up-to-date");
        return true;
    } else if (analysis & GIT_MERGE_ANALYSIS_UNBORN ||
	          (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD &&
	          !(preference & GIT_MERGE_PREFERENCE_NO_FASTFORWARD))) {
        const git_oid *target_oid;
		if (analysis & GIT_MERGE_ANALYSIS_UNBORN) {
			JAMI_INFO("Unborn");
		} else {
			JAMI_INFO("Fast-forward");
		}
        // Since this is a fast-forward, there can be only one merge head
		target_oid = git_annotated_commit_id(annotated);

		return perform_fastforward(repo, target_oid, (analysis & GIT_MERGE_ANALYSIS_UNBORN)) == 0;
    } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
		git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
		git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

		merge_opts.file_flags = GIT_MERGE_FILE_STYLE_DIFF3;

		checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE|GIT_CHECKOUT_ALLOW_CONFLICTS;

		if (preference & GIT_MERGE_PREFERENCE_FASTFORWARD_ONLY) {
			JAMI_ERR("Fast-forward is preferred, but only a merge is possible\n");
			return false;
		}

		if (git_merge(repo, &annotated_ptr, 1, &merge_opts, &checkout_opts) < 0) {
            JAMI_ERR("Git merge failed");
            return false;
        }
	}

	git_index *index;
    if (git_repository_index(&index, repo) < 0) {
        JAMI_ERR("Git Index failed");
        return false;
    }

    if (git_index_has_conflicts(index)) {
		JAMI_WARN("The merge operation resulted in some conflicts");
	} else {
		create_merge_commit(repo, account, index, commit_ref);
	}
    JAMI_INFO("Merge done between %s and master", commit_ref.c_str());
    return true;
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
    auto id3 = addCommit(repo, aliceAccount, "to_merge", "Commit 3");
    auto id4 = addCommit(repo, aliceAccount, "to_merge", "Commit 4");

    merge_in_master(aliceAccount, repo, id4);
    git_repository_free(repo);

    auto messages = repository->log();
    JAMI_ERR("@@@ GIT LOG %u", messages.size());
}

}} // namespace test

RING_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
