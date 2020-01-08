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
#include "jamidht/conversation.h"
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

class ConversationTest : public CppUnit::TestFixture {
public:
    ~ConversationTest() {
        DRing::fini();
    }
    static std::string name() { return "Conversation"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testCreateConversation();
    void testAddMember();
    void testGetMembers();
    void testSendMessage();

    CPPUNIT_TEST_SUITE(ConversationTest);
    CPPUNIT_TEST(testCreateConversation);
    CPPUNIT_TEST(testAddMember);
    CPPUNIT_TEST(testGetMembers);
    CPPUNIT_TEST(testSendMessage);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationTest, ConversationTest::name());

void
ConversationTest::setUp()
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
ConversationTest::tearDown()
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
ConversationTest::testCreateConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto uri = aliceAccount->getAccountDetails()[DRing::Account::ConfProperties::USERNAME];
    if (uri.find("ring:") == 0)
        uri = uri.substr(std::string("ring:").size());
    auto convId = aliceAccount->startConversation();

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
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
ConversationTest::testAddMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getAccountDetails()[ConfProperties::USERNAME];
    if (bobUri.find("ring:") == 0)
        bobUri = bobUri.substr(std::string("ring:").size());
    auto convId = aliceAccount->startConversation();

    aliceAccount->addConversationMember(convId, bobUri);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobMemberFile = repoPath + DIR_SEPARATOR_STR + "members" + DIR_SEPARATOR_STR + bobUri + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bobMemberFile));

    // TODO test received invite
    std::this_thread::sleep_for(std::chrono::seconds(10));
    bobAccount->acceptConversationRequest(convId);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto clonedPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+bobAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
}

void
ConversationTest::testGetMembers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getAccountDetails()[ConfProperties::USERNAME];
    if (bobUri.find("ring:") == 0)
        bobUri = bobUri.substr(std::string("ring:").size());
    auto convId = aliceAccount->startConversation();

    aliceAccount->addConversationMember(convId, bobUri);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto members = aliceAccount->getConversationMembers(convId);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getAccountDetails()[ConfProperties::USERNAME].substr(std::string("ring:").size()));
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "member");

    std::this_thread::sleep_for(std::chrono::seconds(10));
    aliceAccount->loadConversationMessages(convId);
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

void
ConversationTest::testSendMessage()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getAccountDetails()[ConfProperties::USERNAME];
    if (bobUri.find("ring:") == 0)
        bobUri = bobUri.substr(std::string("ring:").size());
    auto convId = aliceAccount->startConversation();

    aliceAccount->addConversationMember(convId, bobUri);
    std::this_thread::sleep_for(std::chrono::seconds(10));

    bobAccount->acceptConversationRequest(convId);
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    aliceAccount->sendMessage(convId, "hi");
    std::this_thread::sleep_for(std::chrono::seconds(10));
}


}} // namespace test

RING_TEST_RUNNER(jami::test::ConversationTest::name())
