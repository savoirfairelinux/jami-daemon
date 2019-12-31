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

    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
    CPPUNIT_TEST(testCreateRepository);
    CPPUNIT_TEST(testCloneViaChannelSocket);
    CPPUNIT_TEST(testAddSomeMessages);
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
    std::thread sendT = std::thread([&]() {
        repository->serve(sendSocket);
    });

    auto cloned = ConversationRepository::cloneConversation(bobAccount->weak(), aliceDeviceId, repository->id());
    channelSocket->shutdown();
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
}

void
ConversationRepositoryTest::testAddSomeMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    repository->sendMessage("Commit 1");
    repository->sendMessage("Commit 2");
    repository->sendMessage("Commit 3");
    // TODO check commits
}

}} // namespace test

RING_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
