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
#include <git2.h>
#include <filesystem>
#include <msgpack.hpp>

#include "manager.h"
#include "../../test_runner.h"
#include "jami.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"
#include "conversation/conversationcommon.h"
#include "common.h"

using namespace std::string_literals;
using namespace DRing::Account;

struct ConvInfoTest
{
    std::string id {};
    time_t created {0};
    time_t removed {0};
    time_t erased {0};

    MSGPACK_DEFINE_MAP(id, created, removed, erased)
};

namespace jami {
namespace test {

class ConversationTest : public CppUnit::TestFixture
{
public:
    ~ConversationTest() { DRing::fini(); }
    static std::string name() { return "Conversation"; }
    void setUp();
    void tearDown();
    std::string createFakeConversation(std::shared_ptr<JamiAccount> account);

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;

private:
    void testCreateConversation();
    void testGetConversation();
    void testGetConversationsAfterRm();
    void testRemoveInvalidConversation();
    void testSendMessage();
    void testSendMessageTriggerMessageReceived();
    void testMergeTwoDifferentHeads();
    void testSendMessageToMultipleParticipants();
    void testPingPongMessages();
    void testIsComposing();
    void testSetMessageDisplayed();
    void testSetMessageDisplayedPreference();
    void testVoteNonEmpty();

    // LATER void testBanDevice();
    // LATER void testBannedDeviceCannotSendMessageButMemberCan();
    // LATER void testRevokedDeviceCannotSendMessage();
    // LATER void test2AdminsCannotBanEachOthers();
    // LATER void test2AdminsBanMembers();
    // LATER void test2AdminsBanOtherAdmin();
    // LATER void testAdminRemoveConversationShouldPromoteOther();

    void testNoBadFileInInitialCommit();
    void testPlainTextNoBadFile();
    void testVoteNoBadFile();
    void testETooBigClone();
    void testETooBigFetch();
    void testUnknownModeDetected();
    void testUpdateProfile();
    void testCheckProfileInConversationRequest();
    void testCheckProfileInTrustRequest();
    void testMemberCannotUpdateProfile();
    void testUpdateProfileWithBadFile();
    void testFetchProfileUnauthorized();
    void testDoNotLoadIncorrectConversation();
    void testSyncingWhileAccepting();
    void testCountInteractions();
    void testReplayConversation();

    CPPUNIT_TEST_SUITE(ConversationTest);
    CPPUNIT_TEST(testCreateConversation);
    CPPUNIT_TEST(testGetConversation);
    CPPUNIT_TEST(testGetConversationsAfterRm);
    CPPUNIT_TEST(testRemoveInvalidConversation);
    CPPUNIT_TEST(testSendMessage);
    CPPUNIT_TEST(testSendMessageTriggerMessageReceived);
    CPPUNIT_TEST(testSendMessageToMultipleParticipants);
    CPPUNIT_TEST(testPingPongMessages);
    CPPUNIT_TEST(testIsComposing);
    CPPUNIT_TEST(testSetMessageDisplayed);
    CPPUNIT_TEST(testSetMessageDisplayedPreference);
    CPPUNIT_TEST(testVoteNonEmpty);
    CPPUNIT_TEST(testNoBadFileInInitialCommit);
    CPPUNIT_TEST(testPlainTextNoBadFile);
    CPPUNIT_TEST(testVoteNoBadFile);
    CPPUNIT_TEST(testETooBigClone);
    CPPUNIT_TEST(testETooBigFetch);
    CPPUNIT_TEST(testUnknownModeDetected);
    CPPUNIT_TEST(testUpdateProfile);
    CPPUNIT_TEST(testCheckProfileInConversationRequest);
    CPPUNIT_TEST(testCheckProfileInTrustRequest);
    CPPUNIT_TEST(testMemberCannotUpdateProfile);
    CPPUNIT_TEST(testUpdateProfileWithBadFile);
    CPPUNIT_TEST(testFetchProfileUnauthorized);
    CPPUNIT_TEST(testDoNotLoadIncorrectConversation);
    CPPUNIT_TEST(testSyncingWhileAccepting);
    CPPUNIT_TEST(testCountInteractions);
    CPPUNIT_TEST(testReplayConversation);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationTest, ConversationTest::name());

void
ConversationTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));

    auto actors = load_actors("actors/alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
ConversationTest::tearDown()
{
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    if (bob2Id.empty()) {
        wait_for_removal_of({aliceId, bobId, carlaId});
    } else {
        wait_for_removal_of({aliceId, bobId, carlaId, bob2Id});
    }
}

void
ConversationTest::testCreateConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = DRing::startConversation(aliceId);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; });
    CPPUNIT_ASSERT(conversationReady);
    ConversationRepository repo(aliceAccount, convId);
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::INVITES_ONLY);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
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
    auto deviceCrt = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + aliceDeviceId
                     + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));
    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)),
                             std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationTest::testGetConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    auto conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(conversations.front() == convId);
}

void
ConversationTest::testGetConversationsAfterRm()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = DRing::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    auto conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(DRing::removeConversation(aliceId, convId));
    conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 0);
}

void
ConversationTest::testRemoveInvalidConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = DRing::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    auto conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(!DRing::removeConversation(aliceId, "foo"));
    conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
}

void
ConversationTest::testSendMessage()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageAliceReceived == 2; });

    DRing::sendMessage(aliceId, convId, "hi"s, "");
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; });
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testSendMessageTriggerMessageReceived()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageReceived = 0;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            messageReceived += 1;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& /* accountId */, const std::string& /* conversationId */) {
            conversationReady = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);
    cv.wait_for(lk, std::chrono::seconds(30), [&] { return conversationReady; });

    DRing::sendMessage(aliceId, convId, "hi"s, "");
    cv.wait_for(lk, std::chrono::seconds(30), [&] { return messageReceived == 1; });
    CPPUNIT_ASSERT(messageReceived == 1);
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testMergeTwoDifferentHeads()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, carlaGotMessage = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> /* message */) {
            if (accountId == carlaId && conversationId == convId) {
                carlaGotMessage = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + aliceAccount->getAccountID() + DIR_SEPARATOR_STR + "conversations";
    auto repoPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + carlaAccount->getAccountID() + DIR_SEPARATOR_STR + "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                       + DIR_SEPARATOR_STR + "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
                       + DIR_SEPARATOR_STR + "convInfo";
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Accept for alice and makes different heads
    ConversationRepository repo(carlaAccount, convId);
    repo.join();

    DRing::sendMessage(aliceId, convId, "hi"s, "");
    DRing::sendMessage(aliceId, convId, "sup"s, "");
    DRing::sendMessage(aliceId, convId, "jami"s, "");

    // Start Carla, should merge and all messages should be there
    Manager::instance().sendRegister(carlaId, true);
    carlaGotMessage = false;
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return carlaGotMessage; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testSendMessageToMultipleParticipants()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    // Enable carla
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool carlaConnected = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced = details[DRing::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return carlaConnected; }));
    confHandlers.clear();
    DRing::unregisterSignalHandlers();

    auto messageReceivedAlice = 0;
    auto messageReceivedBob = 0;
    auto messageReceivedCarla = 0;
    auto requestReceived = 0;
    auto conversationReady = 0;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId)
                messageReceivedAlice += 1;
            if (accountId == bobId)
                messageReceivedBob += 1;
            if (accountId == carlaId)
                messageReceivedCarla += 1;
            cv.notify_one();
        }));

    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived += 1;
                cv.notify_one();
            }));

    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& /*accountId*/, const std::string& /* conversationId */) {
            conversationReady += 1;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);

    DRing::addConversationMember(aliceId, convId, bobUri);
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&]() { return requestReceived == 2; }));

    messageReceivedAlice = 0;
    DRing::acceptConversationRequest(bobId, convId);
    DRing::acceptConversationRequest(carlaId, convId);
    // >= because we can have merges cause the accept commits
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady == 3 && messageReceivedAlice >= 2;
    }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
               + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return messageReceivedBob >= 1 && messageReceivedCarla >= 1;
    }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testPingPongMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId)
                messageBobReceived += 1;
            if (accountId == aliceId)
                messageAliceReceived += 1;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    auto convId = DRing::startConversation(aliceId);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    messageAliceReceived = 0;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady && messageAliceReceived == 1;
    }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    messageBobReceived = 0;
    messageAliceReceived = 0;
    DRing::sendMessage(aliceId, convId, "ping"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 1 && messageAliceReceived == 1;
    }));
    DRing::sendMessage(bobId, convId, "pong"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 2 && messageAliceReceived == 2;
    }));
    DRing::sendMessage(bobId, convId, "ping"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 3 && messageAliceReceived == 3;
    }));
    DRing::sendMessage(aliceId, convId, "pong"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return messageBobReceived == 4 && messageAliceReceived == 4;
    }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testIsComposing()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         aliceComposing = false;
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
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::ComposingStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                bool state) {
                if (accountId == bobId && conversationId == convId && peer == aliceUri) {
                    aliceComposing = state;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));

    aliceAccount->setIsComposing("swarm:" + convId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return aliceComposing; }));

    aliceAccount->setIsComposing("swarm:" + convId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !aliceComposing; }));
}

void
ConversationTest::testSetMessageDisplayed()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         msgDisplayed = false;
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
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (accountId == bobId && conversationId == convId && msgId == conversationId
                    && peer == aliceUri && status == 3) {
                    msgDisplayed = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));

    // Last displayed messages should not be set yet
    auto membersInfos = DRing::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri && infos["lastDisplayed"] == "";
                                })
                   != membersInfos.end());
    membersInfos = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri && infos["lastDisplayed"] == "";
                                })
                   != membersInfos.end());

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return msgDisplayed; }));

    // Now, the last displayed message should be updated in member's infos (both sides)
    membersInfos = DRing::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());
    membersInfos = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());

    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testSetMessageDisplayedPreference()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         msgDisplayed = false, aliceRegistered = false;
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
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (accountId == bobId && conversationId == convId && msgId == conversationId
                    && peer == aliceUri && status == 3) {
                    msgDisplayed = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    aliceRegistered = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    auto details = aliceAccount->getAccountDetails();
    CPPUNIT_ASSERT(details[ConfProperties::SENDREADRECEIPT] == "true");
    details[ConfProperties::SENDREADRECEIPT] = "false";
    DRing::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return aliceRegistered; }));

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));

    // Last displayed messages should not be set yet
    auto membersInfos = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri && infos["lastDisplayed"] == "";
                                })
                   != membersInfos.end());

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    // Bob should not receive anything here, as sendMessageDisplayed is disabled for Alice
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(10), [&]() { return msgDisplayed; }));

    // Assert that message is set as displayed for self (for the read status)
    membersInfos = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());
    DRing::unregisterSignalHandlers();
}

/*void
ConversationTest::testBanDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto bobDeviceId = std::string(bobAccount->currentDeviceId());
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, bob2GetMessage = false, bobGetMessage = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string&,
                const std::string&,
                std::map<std::string, std::string>) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if ((accountId == bobId || accountId == bob2Id) && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
            } else if (accountId == bobId) {
                bobGetMessage = true;
            } else if (accountId == bob2Id) {
                bob2GetMessage = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));

    // Add second device for Bob
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);

    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);
                if (!bob2Account)
                    return;
                auto details = bob2Account->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED")
                    cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    conversationReady = false;
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() { return conversationReady; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    bobGetMessage = false;
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    DRing::removeConversationMember(aliceId, convId, bobDeviceId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return memberMessageGenerated && voteMessageGenerated;
    }));

    auto bannedFile = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId
                      + DIR_SEPARATOR_STR + "banned" + DIR_SEPARATOR_STR + "devices"
                      + DIR_SEPARATOR_STR + bobDeviceId + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bannedFile));
    members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);

    // Assert that bob2 get the message, not Bob
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(10), [&]() { return bobGetMessage; }));
    CPPUNIT_ASSERT(bob2GetMessage && !bobGetMessage);
    DRing::unregisterSignalHandlers();
}*/

std::string
ConversationTest::createFakeConversation(std::shared_ptr<JamiAccount> account)
{
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + account->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + "tmp";

    git_repository* repo_ptr = nullptr;
    git_repository_init_options opts;
    git_repository_init_options_init(&opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
    opts.flags |= GIT_REPOSITORY_INIT_MKPATH;
    opts.initial_head = "main";
    if (git_repository_init_ext(&repo_ptr, repoPath.c_str(), &opts) < 0) {
        JAMI_ERR("Couldn't create a git repository in %s", repoPath.c_str());
    }
    GitRepository repo {std::move(repo_ptr), git_repository_free};

    // Add files
    auto deviceId = std::string(account->currentDeviceId());

    repoPath = git_repository_workdir(repo.get());
    std::string adminsPath = repoPath + "admins";
    std::string devicesPath = repoPath + "devices";
    std::string crlsPath = repoPath + "CRLs" + DIR_SEPARATOR_STR + deviceId;

    if (!fileutils::recursive_mkdir(adminsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", adminsPath.c_str());
    }

    auto cert = account->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERR("Parent cert is null!");
    }

    // /admins
    std::string adminPath = adminsPath + DIR_SEPARATOR_STR + parentCert->getId().toString()
                            + ".crt";
    auto file = fileutils::ofstream(adminPath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", adminPath.c_str());
    }
    file << parentCert->toString(true);
    file.close();

    if (!fileutils::recursive_mkdir(devicesPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", devicesPath.c_str());
    }

    // /devices
    std::string devicePath = devicesPath + DIR_SEPARATOR_STR + cert->getId().toString() + ".crt";
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", devicePath.c_str());
    }
    file << deviceCert;
    file.close();

    if (!fileutils::recursive_mkdir(crlsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", crlsPath.c_str());
    }

    std::string badFile = repoPath + DIR_SEPARATOR_STR + "BAD";
    file = fileutils::ofstream(badFile, std::ios::trunc | std::ios::binary);

    addAll(account, "tmp");

    JAMI_INFO("Initial files added in %s", repoPath.c_str());

    std::string name = account->getDisplayName();
    if (name.empty())
        name = deviceId;

    git_signature* sig_ptr = nullptr;
    git_index* index_ptr = nullptr;
    git_oid tree_id, commit_id;
    git_tree* tree_ptr = nullptr;
    git_buf to_sign = {};

    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERR("Unable to create a commit signature.");
    }
    GitSignature sig {sig_ptr, git_signature_free};

    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
    }
    GitIndex index {index_ptr, git_index_free};

    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERR("Unable to write initial tree from index");
    }

    if (git_tree_lookup(&tree_ptr, repo.get(), &tree_id) < 0) {
        JAMI_ERR("Could not look up initial tree");
    }
    GitTree tree = {tree_ptr, git_tree_free};

    Json::Value json;
    json["mode"] = 1;
    json["type"] = "initial";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 Json::writeString(wbuilder, json).c_str(),
                                 tree.get(),
                                 0,
                                 nullptr)
        < 0) {
        JAMI_ERR("Could not create initial buffer");
        return {};
    }

    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);

    // git commit -S
    if (git_commit_create_with_signature(&commit_id,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERR("Could not sign initial commit");
        return {};
    }

    // Move commit to main branch
    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, repo.get(), &commit_id) == 0) {
        git_reference* ref = nullptr;
        git_branch_create(&ref, repo.get(), "main", commit, true);
        git_commit_free(commit);
        git_reference_free(ref);
    }

    auto commit_str = git_oid_tostr_s(&commit_id);

    auto finalRepo = fileutils::get_data_dir() + DIR_SEPARATOR_STR + account->getAccountID()
                     + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + commit_str;
    std::rename(repoPath.c_str(), finalRepo.c_str());

    file = std::ofstream(fileutils::get_data_dir() + DIR_SEPARATOR_STR + account->getAccountID()
                             + DIR_SEPARATOR_STR + "convInfo",
                         std::ios::trunc | std::ios::binary);

    std::vector<ConvInfoTest> test;
    test.emplace_back(ConvInfoTest {commit_str, std::time(nullptr), 0, 0});
    msgpack::pack(file, test);

    account->convModule()->loadConversations(); // necessary to load fake conv

    return commit_str;
}

void
ConversationTest::testVoteNonEmpty()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, messageBobReceived = false, errorDetected = false,
         carlaConnected = false;
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
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced = details[DRing::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
            } else if (accountId == bobId && conversationId == convId) {
                messageBobReceived = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            int code,
            const std::string& /* what */) {
            if (code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return carlaConnected; }));
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return memberMessageGenerated && messageBobReceived;
    }));

    // Now Alice removes Carla with a non empty file
    errorDetected = false;
    addVote(aliceAccount, convId, carlaUri, "CONTENT");
    simulateRemoval(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));
}

void
ConversationTest::testNoBadFileInInitialCommit()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    auto convId = createFakeConversation(carlaAccount);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool carlaConnected = false;
    bool errorDetected = false;
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
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced = details[DRing::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            int code,
            const std::string& /* what */) {
            if (code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return carlaConnected; }));
    DRing::addConversationMember(carlaId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));
}

void
ConversationTest::testPlainTextNoBadFile()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0;
    bool memberMessageGenerated = false;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    std::string convId = DRing::startConversation(aliceId);
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            int code,
            const std::string& /* what */) {
            if (code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;

    DRing::acceptConversationRequest(bobId, convId);
    cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return conversationReady && memberMessageGenerated;
    });

    addFile(aliceAccount, convId, "BADFILE");
    Json::Value root;
    root["type"] = "text/plain";
    root["body"] = "hi";
    commit(aliceAccount, convId, root);
    errorDetected = false;
    DRing::sendMessage(aliceId, convId, "hi"s, "");
    // Check not received due to the unwanted file
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testVoteNoBadFile()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, messageBobReceived = false, messageCarlaReceived = false,
         carlaConnected = true;
    ;
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
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced = details[DRing::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            std::string body = "";
            auto it = message.find("body");
            if (it != message.end()) {
                body = it->second;
            }
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
            } else if (accountId == bobId && conversationId == convId) {
                messageBobReceived = true;
            } else if (accountId == carlaId && conversationId == convId && body == "final") {
                messageCarlaReceived = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return carlaConnected; }));

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return requestReceived && memberMessageGenerated;
    }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return memberMessageGenerated && messageBobReceived;
    }));

    // Now Alice remove Carla without a vote. Bob will not receive the message
    messageBobReceived = false;
    addFile(aliceAccount, convId, "BADFILE");
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return memberMessageGenerated && voteMessageGenerated;
    }));

    messageCarlaReceived = false;
    DRing::sendMessage(bobId, convId, "final"s, "");
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageCarlaReceived; }));
}

void
ConversationTest::testETooBigClone()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            int code,
            const std::string& /* what */) {
            if (code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    std::ofstream bad(repoPath + DIR_SEPARATOR_STR + "BADFILE");
    CPPUNIT_ASSERT(bad.is_open());
    for (int i = 0; i < 300 * 1024 * 1024; ++i)
        bad << "A";
    bad.close();

    addAll(aliceAccount, convId);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    errorDetected = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testETooBigFetch()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            int code,
            const std::string& /* what */) {
            if (code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);

    DRing::addConversationMember(aliceId, convId, bobUri);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; });

    DRing::acceptConversationRequest(bobId, convId);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; });

    // Wait that alice sees Bob
    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageAliceReceived == 2; });

    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    std::ofstream bad(repoPath + DIR_SEPARATOR_STR + "BADFILE");
    CPPUNIT_ASSERT(bad.is_open());
    errorDetected = false;
    for (int i = 0; i < 300 * 1024 * 1024; ++i)
        bad << "A";
    bad.close();

    addAll(aliceAccount, convId);
    Json::Value json;
    json["body"] = "o/";
    json["type"] = "text/plain";
    commit(aliceAccount, convId, json);

    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testUnknownModeDetected()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    ConversationRepository repo(aliceAccount, convId);
    Json::Value json;
    json["mode"] = 1412;
    json["type"] = "initial";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    repo.amend(convId, Json::writeString(wbuilder, json));
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         errorDetected = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            int code,
            const std::string& /* what */) {
            if (code == 1)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    errorDetected = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));
}

void
ConversationTest::testUpdateProfile()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageAliceReceived == 1;
    }));

    messageBobReceived = 0;
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageBobReceived == 1; }));

    auto infos = DRing::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(infos["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(infos["description"].empty());

    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testCheckProfileInConversationRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    auto requests = DRing::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    CPPUNIT_ASSERT(requests.front()["title"] == "My awesome swarm");

    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testCheckProfileInTrustRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
    std::string convId = "";
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& payload,
            time_t /*received*/) {
            auto pstr = std::string(payload.begin(), payload.begin() + payload.size());
            if (account_id == bobId
                && std::string(payload.data(), payload.data() + payload.size()) == vcard)
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    std::vector<uint8_t> payload(vcard.begin(), vcard.end());
    aliceAccount->sendTrustRequest(bobUri, payload);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&]() {
        return !convId.empty() && requestReceived;
    }));
}

void
ConversationTest::testMemberCannotUpdateProfile()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            int code,
            const std::string& /* what */) {
            if (accountId == bobId && conversationId == convId && code == 4)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageAliceReceived == 1;
    }));

    messageBobReceived = 0;
    bobAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&]() { return errorDetected; }));

    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testUpdateProfileWithBadFile()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            int code,
            const std::string& /* what */) {
            if (accountId == bobId && conversationId == convId && code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageAliceReceived == 1;
    }));

    // Update profile but with bad file
    addFile(aliceAccount, convId, "BADFILE");
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    addFile(aliceAccount, convId, "profile.vcf", vcard);
    Json::Value root;
    root["type"] = "application/update-profile";
    commit(aliceAccount, convId, root);
    errorDetected = false;
    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));

    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testFetchProfileUnauthorized()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
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
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::OnConversationError>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            int code,
            const std::string& /* what */) {
            if (accountId == aliceId && conversationId == convId && code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageAliceReceived == 1;
    }));

    // Fake realist profile update
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    addFile(bobAccount, convId, "profile.vcf", vcard);
    Json::Value root;
    root["type"] = "application/update-profile";
    commit(bobAccount, convId, root);
    errorDetected = false;
    DRing::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return errorDetected; }));

    DRing::unregisterSignalHandlers();
}

void
ConversationTest::testDoNotLoadIncorrectConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    auto convInfos = DRing::conversationInfos(aliceId, convId);
    CPPUNIT_ASSERT(convInfos.find("mode") != convInfos.end());
    CPPUNIT_ASSERT(convInfos.find("syncing") == convInfos.end());

    Manager::instance().sendRegister(aliceId, false);
    auto repoGitPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                       + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId
                       + DIR_SEPARATOR_STR + ".git";
    fileutils::removeAll(repoGitPath, true); // This make the repository not usable

    aliceAccount->convModule()
        ->loadConversations(); // Refresh. This should detect the incorrect conversations.

    // the conv should be detected as invalid and added as syncing
    convInfos = DRing::conversationInfos(aliceId, convId);
    CPPUNIT_ASSERT(convInfos.find("mode") == convInfos.end());
    CPPUNIT_ASSERT(convInfos["syncing"] == "true");
}

void
ConversationTest::testSyncingWhileAccepting()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    std::string convId = "";
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
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));

    auto convInfos = DRing::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(convInfos["syncing"] == "true");

    Manager::instance().sendRegister(aliceId, true); // This avoid to sync immediately
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));

    convInfos = DRing::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(convInfos.find("syncing") == convInfos.end());
}

void
ConversationTest::testCountInteractions()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    std::string msgId1 = "", msgId2 = "", msgId3 = "";
    aliceAccount->convModule()
        ->sendMessage(convId, "1"s, "", "text/plain", true, [&](bool, std::string commitId) {
            msgId1 = commitId;
            cv.notify_one();
        });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return !msgId1.empty(); }));
    aliceAccount->convModule()
        ->sendMessage(convId, "2"s, "", "text/plain", true, [&](bool, std::string commitId) {
            msgId2 = commitId;
            cv.notify_one();
        });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return !msgId2.empty(); }));
    aliceAccount->convModule()
        ->sendMessage(convId, "3"s, "", "text/plain", true, [&](bool, std::string commitId) {
            msgId3 = commitId;
            cv.notify_one();
        });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return !msgId3.empty(); }));

    CPPUNIT_ASSERT(DRing::countInteractions(aliceId, convId, "", "", "") == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(DRing::countInteractions(aliceId, convId, "", "", aliceAccount->getUsername())
                   == 0);
    CPPUNIT_ASSERT(DRing::countInteractions(aliceId, convId, msgId3, "", "") == 0);
    CPPUNIT_ASSERT(DRing::countInteractions(aliceId, convId, msgId2, "", "") == 1);
}

void
ConversationTest::testReplayConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         conversationRemoved = false, messageReceived = false;
    std::vector<std::string> bobMessages;
    std::string convId = "";
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId)
                conversationRemoved = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                messageReceived = true;
                if (message["type"] == "member")
                    memberMessageGenerated = true;
            } else if (accountId == bobId && message["type"] == "text/plain") {
                bobMessages.emplace_back(message["body"]);
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && memberMessageGenerated;
    }));
    // removeContact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationRemoved; }));
    // re-add
    CPPUNIT_ASSERT(convId != "");
    auto oldConvId = convId;
    convId = "";
    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !convId.empty(); }));
    messageReceived = false;
    DRing::sendMessage(aliceId, convId, "foo"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageReceived; }));
    messageReceived = false;
    DRing::sendMessage(aliceId, convId, "bar"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return messageReceived; }));
    convId = "";
    bobMessages.clear();
    aliceAccount->sendTrustRequest(bobUri, {});
    // Should retrieve previous conversation
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return bobMessages.size() == 2 && bobMessages[0] == "foo" && bobMessages[1] == "bar";
    }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationTest::name())
