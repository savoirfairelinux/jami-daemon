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

#include "fileutils.h"
#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/multiplexed_socket.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"

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
            CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
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
    void testCreateMultipleConversationThenAddDevice();
    void testReceivesInviteThenAddDevice();
    void testRemoveConversationOnAllDevices();
    void testSyncCreateAccountExportDeleteReimportOldBackup();
    void testSyncCreateAccountExportDeleteReimportWithConvId();
    void testSyncCreateAccountExportDeleteReimportWithConvReq();

    CPPUNIT_TEST_SUITE(SyncHistoryTest);
    CPPUNIT_TEST(testCreateConversationThenSync);
    CPPUNIT_TEST(testCreateConversationWithOnlineDevice);
    CPPUNIT_TEST(testCreateConversationWithMessagesThenAddDevice);
    CPPUNIT_TEST(testCreateMultipleConversationThenAddDevice);
    CPPUNIT_TEST(testReceivesInviteThenAddDevice);
    CPPUNIT_TEST(testRemoveConversationOnAllDevices);
    CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportOldBackup);
    CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportWithConvId);
    CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportWithConvReq);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SyncHistoryTest, SyncHistoryTest::name());

void
SyncHistoryTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    alice2Id = "";
}

void
SyncHistoryTest::tearDown()
{
    if (alice2Id.empty()) {
        wait_for_removal_of({aliceId, bobId});
    } else {
        wait_for_removal_of({aliceId, bobId, alice2Id});
    }
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

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto conversationReady = false, alice2Ready = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (alice2Id != accountId) {
                    return;
                }
                alice2Ready = details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)
                              == "true";
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return alice2Ready && conversationReady;
    }));
    std::remove(aliceArchive.c_str());
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

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;

    // Start conversation now
    auto convId = aliceAccount->startConversation();
    auto conversationReady = false, alice2Ready = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (alice2Id != accountId) {
                    return;
                }
                alice2Ready = details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)
                              == "true";
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return alice2Ready && conversationReady;
    }));
    DRing::unregisterSignalHandlers();
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testCreateConversationWithMessagesThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto conversationReady = false;
    auto messageReceived = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            messageReceived = true;
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
    confHandlers.clear();

    // Start conversation
    messageReceived = false;
    aliceAccount->sendMessage(convId, std::string("Message 1"));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return messageReceived; }));
    aliceAccount->sendMessage(convId, std::string("Message 2"));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return messageReceived; }));
    aliceAccount->sendMessage(convId, std::string("Message 3"));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return messageReceived; }));

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
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testCreateMultipleConversationThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Start conversation
    auto convId = aliceAccount->startConversation();
    aliceAccount->sendMessage(convId, std::string("Message 1"));
    aliceAccount->sendMessage(convId, std::string("Message 2"));
    aliceAccount->sendMessage(convId, std::string("Message 3"));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto convId2 = aliceAccount->startConversation();
    aliceAccount->sendMessage(convId2, std::string("Message 1"));
    aliceAccount->sendMessage(convId2, std::string("Message 2"));
    aliceAccount->sendMessage(convId2, std::string("Message 3"));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto convId3 = aliceAccount->startConversation();
    aliceAccount->sendMessage(convId3, std::string("Message 1"));
    aliceAccount->sendMessage(convId3, std::string("Message 2"));
    aliceAccount->sendMessage(convId3, std::string("Message 3"));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto convId4 = aliceAccount->startConversation();
    aliceAccount->sendMessage(convId4, std::string("Message 1"));
    aliceAccount->sendMessage(convId4, std::string("Message 2"));
    aliceAccount->sendMessage(convId4, std::string("Message 3"));

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
    std::atomic_int conversationReady = 0;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == alice2Id) {
                conversationReady += 1;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    confHandlers.clear();

    // Check if conversation is ready
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&]() { return conversationReady == 4; }));
    DRing::unregisterSignalHandlers();
    std::remove(aliceArchive.c_str());
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

    // Check that alice receives the request
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, memberEvent = false;
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
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationMemberEvent>(
            [&](const std::string& /*accountId*/,
                const std::string& /*conversationId*/,
                const std::string& /*memberUri*/,
                int /*event*/) {
                memberEvent = true;
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    memberEvent = false;
    bobAccount->addConversationMember(convId, uri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return memberEvent && requestReceived; }));
    DRing::unregisterSignalHandlers();
    confHandlers.clear();

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

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return requestReceived; }));
    DRing::unregisterSignalHandlers();
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testRemoveConversationOnAllDevices()
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

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;

    // Start conversation now
    auto convId = aliceAccount->startConversation();
    bool alice2Ready = false;
    auto conversationReady = false, conversationRemoved = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == alice2Id && conversationId == convId) {
                conversationRemoved = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (alice2Id != accountId) {
                    return;
                }
                alice2Ready = details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)
                              == "true";
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return alice2Ready && conversationReady;
    }));
    aliceAccount->removeConversation(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return conversationRemoved; }));

    DRing::unregisterSignalHandlers();
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportOldBackup()
{
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Backup alice before start conversation, worst scenario for invites
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);

    // Start conversation
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool alice2Ready = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            } else {
                messageAliceReceived += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }

            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (alice2Id != accountId) {
                    return;
                }
                alice2Ready = details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)
                              == "true";
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    // Wait that alice sees Bob
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageAliceReceived == 1; });

    // disable account (same as removed)
    Manager::instance().sendRegister(aliceId, false);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    requestReceived = false;
    conversationReady = false;
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return alice2Ready; }));
    aliceAccount = Manager::instance().getAccount<JamiAccount>(alice2Id);

    // This will trigger a conversation request. Cause alice2 can't know first conversation
    bobAccount->sendMessage(convId, std::string("hi"));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return requestReceived; }));

    aliceAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return conversationReady; }));

    messageBobReceived = 0;
    aliceAccount->sendMessage(convId, std::string("hi"));
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; });
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvId()
{
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    // Start conversation
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool alice2Ready = false;
    bool memberAddGenerated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationMemberEvent>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& uri,
                int event) {
                if (accountId == aliceId && conversationId == convId && uri == bobUri
                    && event == 0) {
                    memberAddGenerated = true;
                }
                cv.notify_one();
            }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (conversationId != convId)
                return;
            if (accountId == bobId || accountId == alice2Id)
                conversationReady = true;
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (alice2Id != accountId) {
                    return;
                }
                alice2Ready = details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)
                              == "true";
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    // We need to track presence to know when to sync
    bobAccount->trackBuddyPresence(aliceUri, true);

    // Wait that alice sees Bob
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberAddGenerated; });

    // Backup alice after startConversation with member
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);

    // disable account (same as removed)
    Manager::instance().sendRegister(aliceId, false);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    requestReceived = false;
    conversationReady = false;
    alice2Id = Manager::instance().addAccount(details);
    // Should retrieve conversation, no need for action as the convInfos is in the archive
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return alice2Ready; }));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return conversationReady; }));

    messageBobReceived = 0;
    aliceAccount = Manager::instance().getAccount<JamiAccount>(alice2Id);
    aliceAccount->sendMessage(convId, std::string("hi"));
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; });
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvReq()
{
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    // Start conversation
    auto convId = bobAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool alice2Ready = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            } else {
                messageAliceReceived += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }

            if (accountId == alice2Id && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (alice2Id != accountId) {
                    return;
                }
                alice2Ready = details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)
                              == "true";
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    bobAccount->addConversationMember(convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    // Backup alice after startConversation with member
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);

    // disable account (same as removed)
    Manager::instance().sendRegister(aliceId, false);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    conversationReady = false;
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return alice2Ready; }));
    aliceAccount = Manager::instance().getAccount<JamiAccount>(alice2Id);

    // Should get the same request as before.
    aliceAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageBobReceived == 1;
    }));
    std::remove(aliceArchive.c_str());
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SyncHistoryTest::name())
