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
    void testSyncOneToOne();
    void testConversationRequestRemoved();
    void testProfileReceivedMultiDevice();

    CPPUNIT_TEST_SUITE(SyncHistoryTest);
    // CPPUNIT_TEST(testCreateConversationThenSync);
    // CPPUNIT_TEST(testCreateConversationWithOnlineDevice);
    // CPPUNIT_TEST(testCreateConversationWithMessagesThenAddDevice);
    CPPUNIT_TEST(testCreateMultipleConversationThenAddDevice);
    // CPPUNIT_TEST(testReceivesInviteThenAddDevice);
    // CPPUNIT_TEST(testRemoveConversationOnAllDevices);
    // CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportOldBackup);
    // CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportWithConvId);
    // CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportWithConvReq);
    CPPUNIT_TEST(testSyncOneToOne);
    // CPPUNIT_TEST(testConversationRequestRemoved);
    // CPPUNIT_TEST(testProfileReceivedMultiDevice);
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
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
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
    auto convId = DRing::startConversation(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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
    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testCreateConversationWithOnlineDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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
    auto convId = DRing::startConversation(aliceId);
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
}

void
SyncHistoryTest::testCreateConversationWithMessagesThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto conversationReady = false;
    auto messageReceived = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& /* accountId */,
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
    DRing::sendMessage(aliceId, convId, std::string("Message 1"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return messageReceived; }));
    messageReceived = false;
    DRing::sendMessage(aliceId, convId, std::string("Message 2"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return messageReceived; }));
    messageReceived = false;
    DRing::sendMessage(aliceId, convId, std::string("Message 3"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return messageReceived; }));

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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
    DRing::loadConversationMessages(alice2Id, convId, "", 0);
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
SyncHistoryTest::testCreateMultipleConversationThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Start conversation
    auto convId = DRing::startConversation(aliceId);
    DRing::sendMessage(aliceId, convId, std::string("Message 1"), "");
    DRing::sendMessage(aliceId, convId, std::string("Message 2"), "");
    DRing::sendMessage(aliceId, convId, std::string("Message 3"), "");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto convId2 = DRing::startConversation(aliceId);
    DRing::sendMessage(aliceId, convId2, std::string("Message 1"), "");
    DRing::sendMessage(aliceId, convId2, std::string("Message 2"), "");
    DRing::sendMessage(aliceId, convId2, std::string("Message 3"), "");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto convId3 = DRing::startConversation(aliceId);
    DRing::sendMessage(aliceId, convId3, std::string("Message 1"), "");
    DRing::sendMessage(aliceId, convId3, std::string("Message 2"), "");
    DRing::sendMessage(aliceId, convId3, std::string("Message 3"), "");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto convId4 = DRing::startConversation(aliceId);
    DRing::sendMessage(aliceId, convId4, std::string("Message 1"), "");
    DRing::sendMessage(aliceId, convId4, std::string("Message 2"), "");
    DRing::sendMessage(aliceId, convId4, std::string("Message 3"), "");

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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

    JAMI_ERR() << "@@@ ALICE 2 " << alice2Id;

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
}

void
SyncHistoryTest::testReceivesInviteThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto uri = aliceAccount->getUsername();

    // Start conversation for Alice
    auto convId = DRing::startConversation(bobId);

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
    DRing::addConversationMember(bobId, convId, uri);
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
}

void
SyncHistoryTest::testRemoveConversationOnAllDevices()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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
    auto convId = DRing::startConversation(aliceId);
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
    DRing::removeConversation(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return conversationRemoved; }));

    DRing::unregisterSignalHandlers();
    std::remove(aliceArchive.c_str());
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportOldBackup()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Backup alice before start conversation, worst scenario for invites
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    // Start conversation
    auto convId = DRing::startConversation(aliceId);

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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
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

    // This will trigger a conversation request. Cause alice2 can't know first conversation
    DRing::sendMessage(bobId, convId, std::string("hi"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return requestReceived; }));

    DRing::acceptConversationRequest(alice2Id, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return conversationReady; }));

    messageBobReceived = 0;
    DRing::sendMessage(alice2Id, convId, std::string("hi"), "");
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; });
    std::remove(aliceArchive.c_str());
    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvId()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    // Start conversation
    auto convId = DRing::startConversation(aliceId);

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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    // We need to track presence to know when to sync
    bobAccount->trackBuddyPresence(aliceUri, true);

    // Wait that alice sees Bob
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberAddGenerated; });

    // Backup alice after startConversation with member
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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
    DRing::sendMessage(alice2Id, convId, std::string("hi"), "");
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; });
    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvReq()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    // Start conversation
    auto convId = DRing::startConversation(bobId);

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

    DRing::addConversationMember(bobId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    // Backup alice after startConversation with member
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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

    // Should get the same request as before.
    DRing::acceptConversationRequest(alice2Id, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageBobReceived == 1;
    }));
    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testSyncOneToOne()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    // Start conversation
    std::string convId;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto conversationReady = false, alice2Ready = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                convId = conversationId;
            else if (accountId == alice2Id && conversationId == convId)
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

    aliceAccount->addContact(bobAccount->getUsername());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return !convId.empty(); }));

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
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
    JAMI_ERR() << "@@@@@ " << aliceId << " " << alice2Id;
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return alice2Ready && conversationReady;
    }));
    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testConversationRequestRemoved()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto uri = aliceAccount->getUsername();

    // Start conversation for Alice
    auto convId = DRing::startConversation(bobId);

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

    DRing::addConversationMember(bobId, convId, uri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return requestReceived; }));
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
    bool requestDeclined = false, requestDeclined2 = false;
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
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestDeclined>(
            [&](const std::string& accountId, const std::string& conversationId) {
                if (conversationId != convId)
                    return;
                if (accountId == aliceId)
                    requestDeclined = true;
                if (accountId == alice2Id)
                    requestDeclined2 = true;
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return requestReceived; }));
    // Now decline trust request, this should trigger ConversationRequestDeclined both sides for Alice
    DRing::declineConversationRequest(aliceId, convId);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return requestDeclined && requestDeclined2;
    }));

    DRing::unregisterSignalHandlers();
}

void
SyncHistoryTest::testProfileReceivedMultiDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    // Set VCards
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    auto alicePath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                     + DIR_SEPARATOR_STR + "profile.vcf";
    auto bobPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                   + DIR_SEPARATOR_STR + "profile.vcf";
    // Save VCard
    auto p = std::filesystem::path(alicePath);
    fileutils::recursive_mkdir(p.parent_path());
    std::ofstream aliceFile(alicePath);
    if (aliceFile.is_open()) {
        aliceFile << vcard;
        aliceFile.close();
    }
    p = std::filesystem::path(bobPath);
    fileutils::recursive_mkdir(p.parent_path());
    std::ofstream bobFile(bobPath);
    if (bobFile.is_open()) {
        bobFile << vcard;
        bobFile.close();
    }

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, bobProfileReceived = false,
         aliceProfileReceived = false;
    std::string convId = "";
    std::string bobDest = aliceAccount->dataTransfer()->profilePath(bobUri);
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& /*payload*/,
            time_t /*received*/) {
            if (account_id == bobId)
                requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ProfileReceived>(
        [&](const std::string& accountId, const std::string& peerId, const std::string& path) {
            if (accountId == aliceId && peerId == bobUri) {
                bobProfileReceived = true;
                auto p = std::filesystem::path(bobDest);
                fileutils::recursive_mkdir(p.parent_path());
                std::rename(path.c_str(), bobDest.c_str());
            } else if (accountId == bobId && peerId == aliceUri) {
                aliceProfileReceived = true;
            } else if (accountId == alice2Id && peerId == bobUri) {
                bobProfileReceived = true;
            } else if (accountId == alice2Id && peerId == aliceUri) {
                aliceProfileReceived = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobProfileReceived && aliceProfileReceived;
    }));
    CPPUNIT_ASSERT(fileutils::isFile(bobDest));

    // Now create alice2
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    bobProfileReceived = false, aliceProfileReceived = false;
    alice2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return aliceProfileReceived && bobProfileReceived;
    }));
    DRing::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SyncHistoryTest::name())
