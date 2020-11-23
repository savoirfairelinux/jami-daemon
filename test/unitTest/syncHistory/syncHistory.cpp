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
#include <filesystem>

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/multiplexed_socket.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "account_const.h"

using namespace DRing::Account;

namespace jami {
namespace test {

class SyncHistoryTest : public CppUnit::TestFixture
{
public:
    SyncHistoryTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~SyncHistoryTest() { DRing::fini(); }
    static std::string name() { return "SyncHistory"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string alice2Id;

private:
    void testCreateConversationThenSync();
    void testCreateConversationWithOnlineDevice();
    void testCreateConversationWithMessagesThenAddDevice();
    void testReceivesInviteThenAddDevice();

    CPPUNIT_TEST_SUITE(SyncHistoryTest);
    CPPUNIT_TEST(testCreateConversationThenSync);
    CPPUNIT_TEST(testCreateConversationWithOnlineDevice);
    CPPUNIT_TEST(testCreateConversationWithMessagesThenAddDevice);
    CPPUNIT_TEST(testReceivesInviteThenAddDevice);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SyncHistoryTest, SyncHistoryTest::name());

void
SyncHistoryTest::setUp()
{
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

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                bool ready = false;
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready = (daemonStatus == "REGISTERED");
                details = bobAccount->getVolatileAccountDetails();
                daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready &= (daemonStatus == "REGISTERED");
                if (ready)
                    cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    alice2Id = "";
}

void
SyncHistoryTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    auto toRemove = alice2Id.empty() ? 2 : 3;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - toRemove) {
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceId, true);
    if (!alice2Id.empty())
        Manager::instance().removeAccount(alice2Id, true);
    Manager::instance().removeAccount(bobId, true);
    // Because cppunit is not linked with dbus, just poll if removed
    cv.wait_for(lk, std::chrono::seconds(30));

    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testCreateConversationThenSync()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Start conversation
    auto convId = aliceAccount->startConversation();

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto alice2Account = Manager::instance().getAccount<JamiAccount>(alice2Id);
                auto details = alice2Account->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED")
                    cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

    auto conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

    // Check if conversation is ready
    CPPUNIT_ASSERT(conversationReady);
}

void
SyncHistoryTest::testCreateConversationWithOnlineDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto alice2Account = Manager::instance().getAccount<JamiAccount>(alice2Id);
                auto details = alice2Account->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED")
                    cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

    // Start conversation now
    auto convId = aliceAccount->startConversation();
    auto conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

    // Check if conversation is ready
    CPPUNIT_ASSERT(conversationReady);
}

void
SyncHistoryTest::testCreateConversationWithMessagesThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Start conversation
    auto convId = aliceAccount->startConversation();
    aliceAccount->sendMessage(convId, "Message 1");
    aliceAccount->sendMessage(convId, "Message 2");
    aliceAccount->sendMessage(convId, "Message 3");

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto conversationReady = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto alice2Account = Manager::instance().getAccount<JamiAccount>(alice2Id);
                auto details = alice2Account->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED")
                    cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

    // Check if conversation is ready
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() { return conversationReady; }));
    auto alice2Account = Manager::instance().getAccount<JamiAccount>(alice2Id);
    std::vector<std::map<std::string, std::string>> messages;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationLoaded>(
        [&](uint32_t,
            const std::string& accountId,
            const std::string& conversationId,
            std::vector<std::map<std::string, std::string>> msg) {
            if (accountId == alice2Id && conversationId == convId) {
                messages = msg;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    alice2Account->loadConversationMessages(convId);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

    // Check messages
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0]["body"] == "Message 3");
    CPPUNIT_ASSERT(messages[1]["body"] == "Message 2");
    CPPUNIT_ASSERT(messages[2]["body"] == "Message 1");
}

void
SyncHistoryTest::testReceivesInviteThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto uri = aliceAccount->getUsername();

    // Start conversation for Alice
    auto convId = bobAccount->startConversation();
    CPPUNIT_ASSERT(bobAccount->addConversationMember(convId, uri));

    // Check that alice receives the request
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == aliceId && conversationId == convId) {
                    requestReceived = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();
    CPPUNIT_ASSERT(requestReceived);

    // Now create alice2
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    requestReceived = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == alice2Id && conversationId == convId) {
                    requestReceived = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();
    CPPUNIT_ASSERT(requestReceived);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SyncHistoryTest::name())
