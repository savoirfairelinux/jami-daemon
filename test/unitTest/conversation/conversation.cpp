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
#include <git2.h>
#include <filesystem>
#include <msgpack.hpp>

#include "../../test_runner.h"
#include "account_const.h"
#include "archiver.h"
#include "base64.h"
#include "common.h"
#include "conversation/conversationcommon.h"
#include "fileutils.h"
#include "jami.h"
#include "manager.h"
#include <dhtnet/certstore.h>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

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
    ~ConversationTest() { libjami::fini(); }
    static std::string name() { return "Conversation"; }
    void setUp();
    void tearDown();
    std::string createFakeConversation(std::shared_ptr<JamiAccount> account,
                                       const std::string& fakeCert = "");

    std::string aliceId;
    std::string alice2Id;
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

private:
    void testCreateConversation();
    void testCreateConversationInvalidDisplayName();
    void testGetConversation();
    void testGetConversationsAfterRm();
    void testRemoveInvalidConversation();
    void testSendMessage();
    void testSendMessageWithBadDisplayName();
    void testReplaceWithBadCertificate();
    void testSendMessageTriggerMessageReceived();
    void testMergeTwoDifferentHeads();
    void testSendMessageToMultipleParticipants();
    void testPingPongMessages();
    void testIsComposing();
    void testMessageStatus();
    void testSetMessageDisplayed();
    void testSetMessageDisplayedTwice();
    void testSetMessageDisplayedPreference();
    void testSetMessageDisplayedAfterClone();
    void testVoteNonEmpty();
    void testNoBadFileInInitialCommit();
    void testNoBadCertInInitialCommit();
    void testPlainTextNoBadFile();
    void testVoteNoBadFile();
    void testETooBigClone();
    void testETooBigFetch();
    void testUnknownModeDetected();
    void testUpdateProfile();
    void testGetProfileRequest();
    void testCheckProfileInConversationRequest();
    void testCheckProfileInTrustRequest();
    void testMemberCannotUpdateProfile();
    void testUpdateProfileWithBadFile();
    void testFetchProfileUnauthorized();
    void testSyncingWhileAccepting();
    void testCountInteractions();
    void testReplayConversation();
    void testSyncWithoutPinnedCert();
    void testImportMalformedContacts();
    void testRemoveReaddMultipleDevice();
    void testCloneFromMultipleDevice();
    void testSendReply();
    void testSearchInConv();
    void testConversationPreferences();
    void testConversationPreferencesBeforeClone();
    void testConversationPreferencesMultiDevices();
    void testFixContactDetails();
    void testRemoveOneToOneNotInDetails();
    void testMessageEdition();
    void testMessageReaction();
    void testLoadPartiallyRemovedConversation();

    CPPUNIT_TEST_SUITE(ConversationTest);
    CPPUNIT_TEST(testCreateConversation);
    CPPUNIT_TEST(testCreateConversationInvalidDisplayName);
    CPPUNIT_TEST(testGetConversation);
    CPPUNIT_TEST(testGetConversationsAfterRm);
    CPPUNIT_TEST(testRemoveInvalidConversation);
    CPPUNIT_TEST(testSendMessage);
    CPPUNIT_TEST(testSendMessageWithBadDisplayName);
    CPPUNIT_TEST(testReplaceWithBadCertificate);
    CPPUNIT_TEST(testSendMessageTriggerMessageReceived);
    CPPUNIT_TEST(testMergeTwoDifferentHeads);
    CPPUNIT_TEST(testSendMessageToMultipleParticipants);
    CPPUNIT_TEST(testPingPongMessages);
    CPPUNIT_TEST(testIsComposing);
    CPPUNIT_TEST(testMessageStatus);
    CPPUNIT_TEST(testSetMessageDisplayed);
    CPPUNIT_TEST(testSetMessageDisplayedTwice);
    CPPUNIT_TEST(testSetMessageDisplayedPreference);
    CPPUNIT_TEST(testSetMessageDisplayedAfterClone);
    CPPUNIT_TEST(testVoteNonEmpty);
    CPPUNIT_TEST(testNoBadFileInInitialCommit);
    CPPUNIT_TEST(testNoBadCertInInitialCommit);
    CPPUNIT_TEST(testPlainTextNoBadFile);
    CPPUNIT_TEST(testVoteNoBadFile);
    CPPUNIT_TEST(testETooBigClone);
    CPPUNIT_TEST(testETooBigFetch);
    CPPUNIT_TEST(testUnknownModeDetected);
    CPPUNIT_TEST(testUpdateProfile);
    CPPUNIT_TEST(testGetProfileRequest);
    CPPUNIT_TEST(testCheckProfileInConversationRequest);
    CPPUNIT_TEST(testCheckProfileInTrustRequest);
    CPPUNIT_TEST(testMemberCannotUpdateProfile);
    CPPUNIT_TEST(testUpdateProfileWithBadFile);
    CPPUNIT_TEST(testFetchProfileUnauthorized);
    CPPUNIT_TEST(testSyncingWhileAccepting);
    CPPUNIT_TEST(testCountInteractions);
    CPPUNIT_TEST(testReplayConversation);
    CPPUNIT_TEST(testSyncWithoutPinnedCert);
    CPPUNIT_TEST(testImportMalformedContacts);
    CPPUNIT_TEST(testRemoveReaddMultipleDevice);
    CPPUNIT_TEST(testCloneFromMultipleDevice);
    CPPUNIT_TEST(testSendReply);
    CPPUNIT_TEST(testSearchInConv);
    CPPUNIT_TEST(testConversationPreferences);
    CPPUNIT_TEST(testConversationPreferencesBeforeClone);
    CPPUNIT_TEST(testConversationPreferencesMultiDevices);
    CPPUNIT_TEST(testFixContactDetails);
    CPPUNIT_TEST(testRemoveOneToOneNotInDetails);
    CPPUNIT_TEST(testMessageEdition);
    CPPUNIT_TEST(testMessageReaction);
    CPPUNIT_TEST(testLoadPartiallyRemovedConversation);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationTest, ConversationTest::name());

void
ConversationTest::setUp()
{
    // Init daemon
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

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
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    if (!alice2Id.empty()) {
        wait_for_removal_of(alice2Id);
    }

    if (bob2Id.empty()) {
        wait_for_removal_of({aliceId, bobId, carlaId});
    } else {
        wait_for_removal_of({aliceId, bobId, carlaId, bob2Id});
    }
}

void
ConversationTest::testCreateConversation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return conversationReady; });
    CPPUNIT_ASSERT(conversationReady);
    ConversationRepository repo(aliceAccount, convId);
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::INVITES_ONLY);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto adminCrt = repoPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(adminCrt));
    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());
    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);
    CPPUNIT_ASSERT(adminCrtStr == parentCert);
    auto deviceCrt = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + aliceDeviceId
                     + ".crt";
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(deviceCrt));
    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)),
                             std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationTest::testCreateConversationInvalidDisplayName()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool aliceRegistered = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    aliceRegistered = true;
                    cv.notify_one();
                }
            }));
    auto messageAliceReceived = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId) {
                messageAliceReceived += 1;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);


    std::map<std::string, std::string> details;
    details[ConfProperties::DISPLAYNAME] = " ";
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceRegistered; }));

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    messageAliceReceived = 0;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 1; }));
}

void
ConversationTest::testGetConversation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    auto conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(conversations.front() == convId);
}

void
ConversationTest::testGetConversationsAfterRm()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    auto conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(libjami::removeConversation(aliceId, convId));
    conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 0);
}

void
ConversationTest::testRemoveInvalidConversation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    auto conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    CPPUNIT_ASSERT(!libjami::removeConversation(aliceId, "foo"));
    conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
}

void
ConversationTest::testSendMessage()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 2; });

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    cv.wait_for(lk, 30s, [&]() { return messageBobReceived == 1; });
}

void
ConversationTest::testSendMessageWithBadDisplayName()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool aliceRegistered = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    aliceRegistered = true;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);

    std::map<std::string, std::string> details;
    details[ConfProperties::DISPLAYNAME] = "<o>";
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceRegistered; }));

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 2; });

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    cv.wait_for(lk, 30s, [&]() { return messageBobReceived == 1; });
}

void
ConversationTest::testReplaceWithBadCertificate()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
    auto requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    auto conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    auto errorDetected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 2; });

    // Replace alice's certificate with a bad one.
    auto repoPath = fmt::format("{}/{}/conversations/{}",
                                fileutils::get_data_dir(),
                                aliceAccount->getAccountID(),
                                convId);
    auto aliceDevicePath = fmt::format("{}/devices/{}.crt",
                                       repoPath,
                                       aliceAccount->currentDeviceId());
    auto bobDevicePath = fmt::format("{}/devices/{}.crt", repoPath, bobAccount->currentDeviceId());
    std::filesystem::copy(bobDevicePath,
                          aliceDevicePath,
                          std::filesystem::copy_options::overwrite_existing);
    addAll(aliceAccount, convId);

    // Note: Do not use libjami::sendMessage as it will replace the invalid certificate by a valid one
    Json::Value root;
    root["type"] = "text/plain";
    root["body"] = "hi";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    auto message = Json::writeString(wbuilder, root);
    messageBobReceived = 0;
    commitInRepo(repoPath, aliceAccount, message);
    // now we need to sync!
    libjami::sendMessage(aliceId, convId, "trigger sync!"s, "");
    // We should detect the incorrect commit!
    cv.wait_for(lk, 30s, [&]() { return errorDetected; });
}

void
ConversationTest::testSendMessageTriggerMessageReceived()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageReceived = 0;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& /* accountId */,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            messageReceived += 1;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& /* accountId */, const std::string& /* conversationId */) {
            conversationReady = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&] { return conversationReady; });

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    cv.wait_for(lk, 30s, [&] { return messageReceived == 1; });
    CPPUNIT_ASSERT(messageReceived == 1);
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testMergeTwoDifferentHeads()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    carlaAccount->trackBuddyPresence(aliceUri, true);
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, carlaGotMessage = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> /* message */) {
            if (accountId == carlaId && conversationId == convId) {
                carlaGotMessage = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

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
    std::filesystem::remove_all(ciPathCarla);
    std::filesystem::copy(ciPathAlice, ciPathCarla);
    carlaAccount->convModule()->loadConversations(); // necessary to load conversation

    // Accept for alice and makes different heads
    ConversationRepository repo(carlaAccount, convId);
    repo.join();

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    libjami::sendMessage(aliceId, convId, "sup"s, "");
    libjami::sendMessage(aliceId, convId, "jami"s, "");

    // Start Carla, should merge and all messages should be there
    Manager::instance().sendRegister(carlaId, true);
    carlaGotMessage = false;
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaGotMessage; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testSendMessageToMultipleParticipants()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    // Enable carla
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool carlaConnected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaConnected; }));
    confHandlers.clear();
    libjami::unregisterSignalHandlers();

    auto messageReceivedAlice = 0;
    auto messageReceivedBob = 0;
    auto messageReceivedCarla = 0;
    auto requestReceived = 0;
    auto conversationReady = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived += 1;
                cv.notify_one();
            }));

    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& /*accountId*/, const std::string& /* conversationId */) {
            conversationReady += 1;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return requestReceived == 2; }));

    messageReceivedAlice = 0;
    libjami::acceptConversationRequest(bobId, convId);
    libjami::acceptConversationRequest(carlaId, convId);
    // >= because we can have merges cause the accept commits
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return conversationReady == 3 && messageReceivedAlice >= 2;
    }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
               + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return messageReceivedBob >= 1 && messageReceivedCarla >= 1;
    }));
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testPingPongMessages()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    messageAliceReceived = 0;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 60s, [&]() { return conversationReady && messageAliceReceived == 1; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    messageBobReceived = 0;
    messageAliceReceived = 0;
    libjami::sendMessage(aliceId, convId, "ping"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return messageBobReceived == 1 && messageAliceReceived == 1;
    }));
    libjami::sendMessage(bobId, convId, "pong"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return messageBobReceived == 2 && messageAliceReceived == 2;
    }));
    libjami::sendMessage(bobId, convId, "ping"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return messageBobReceived == 3 && messageAliceReceived == 3;
    }));
    libjami::sendMessage(aliceId, convId, "pong"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return messageBobReceived == 4 && messageAliceReceived == 4;
    }));
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testIsComposing()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         aliceComposing = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::ComposingStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                bool state) {
                if (accountId == bobId && conversationId == convId && peer == aliceUri) {
                    aliceComposing = state;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    aliceAccount->setIsComposing("swarm:" + convId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceComposing; }));

    aliceAccount->setIsComposing("swarm:" + convId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceComposing; }));
}

void
ConversationTest::testMessageStatus()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    bool sending = false, sent = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (accountId == aliceId && convId == conversationId) {
                    if (status == 2)
                        sending = true;
                    if (status == 3)
                        sent = true;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    sending = false;
    sent = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return sending && sent; }));

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testSetMessageDisplayed()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         msgDisplayed = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    std::string aliceLastMsg;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (conversationId == convId && peer == aliceUri && status == 3) {
                    if (accountId == bobId && msgId == conversationId)
                        msgDisplayed = true;
                    else if (accountId == aliceId)
                        aliceLastMsg = msgId;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);
    aliceLastMsg = "";
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && !aliceLastMsg.empty(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Last displayed messages should not be set yet
    auto membersInfos = libjami::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri && infos["lastDisplayed"] == "";
                                })
                   != membersInfos.end());
    membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    // Last read for alice is when bob is added to the members
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == aliceLastMsg;
                                })
                   != membersInfos.end());

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return msgDisplayed; }));

    // Now, the last displayed message should be updated in member's infos (both sides)
    membersInfos = libjami::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());
    membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testSetMessageDisplayedTwice()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         msgDisplayed = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    std::string aliceLastMsg;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (conversationId == convId && peer == aliceUri && status == 3) {
                    if (accountId == bobId && msgId == conversationId)
                        msgDisplayed = true;
                    else if (accountId == aliceId)
                        aliceLastMsg = msgId;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);
    aliceLastMsg = "";
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && !aliceLastMsg.empty(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return msgDisplayed; }));

    msgDisplayed = false;
    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return msgDisplayed; }));

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testSetMessageDisplayedPreference()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         msgDisplayed = false, aliceRegistered = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    std::string aliceLastMsg;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (conversationId == convId && peer == aliceUri && status == 3) {
                    if (accountId == bobId && msgId == conversationId)
                        msgDisplayed = true;
                    else if (accountId == aliceId)
                        aliceLastMsg = msgId;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    aliceRegistered = true;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);

    auto details = aliceAccount->getAccountDetails();
    CPPUNIT_ASSERT(details[ConfProperties::SENDREADRECEIPT] == "true");
    details[ConfProperties::SENDREADRECEIPT] = "false";
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceRegistered; }));

    aliceLastMsg = "";
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return requestReceived && memberMessageGenerated && !aliceLastMsg.empty();
    }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Last displayed messages should not be set yet
    auto membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    // Last read for alice is when bob is added to the members
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == aliceLastMsg;
                                })
                   != membersInfos.end());

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    // Bob should not receive anything here, as sendMessageDisplayed is disabled for Alice
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return msgDisplayed; }));

    // Assert that message is set as displayed for self (for the read status)
    membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testSetMessageDisplayedAfterClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false, memberMessageGenerated = false, msgDisplayed = false,
         aliceRegistered = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    bool conversationReady = false, conversationAlice2Ready = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
            } else if (accountId == alice2Id) {
                conversationAlice2Ready = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    std::string aliceLastMsg;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                const std::string& msgId,
                int status) {
                if (conversationId == convId && peer == aliceUri && status == 3) {
                    if (accountId == bobId && msgId == conversationId)
                        msgDisplayed = true;
                    else if (accountId == aliceId)
                        aliceLastMsg = msgId;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    aliceRegistered = true;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);

    aliceLastMsg = "";
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return requestReceived && memberMessageGenerated && !aliceLastMsg.empty();
    }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);

    // Alice creates a second device
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "alice2";
    details[ConfProperties::ALIAS] = "alice2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    // Disconnect alice2, to create a valid conv betwen Alice and alice1
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationAlice2Ready; }));

    // Assert that message is set as displayed for self (for the read status)
    auto membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());

    libjami::unregisterSignalHandlers();
}

std::string
ConversationTest::createFakeConversation(std::shared_ptr<JamiAccount> account,
                                         const std::string& fakeCert)
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

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

    if (!dhtnet::fileutils::recursive_mkdir(adminsPath, 0700)) {
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

    if (!dhtnet::fileutils::recursive_mkdir(devicesPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", devicesPath.c_str());
    }

    // /devices
    std::string devicePath = fmt::format("{}/{}.crt", devicesPath, cert->getLongId().toString());
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", devicePath.c_str());
    }
    file << (fakeCert.empty() ? deviceCert : fakeCert);
    file.close();

    if (!dhtnet::fileutils::recursive_mkdir(crlsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", crlsPath.c_str());
    }

    if (fakeCert.empty()) {
        // Add a unwanted file
        std::string badFile = repoPath + DIR_SEPARATOR_STR + "BAD";
        file = fileutils::ofstream(badFile, std::ios::trunc | std::ios::binary);
    }

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
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, messageBobReceived = false, errorDetected = false,
         carlaConnected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaConnected; }));
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    // Now Alice removes Carla with a non empty file
    errorDetected = false;
    addVote(aliceAccount, convId, carlaUri, "CONTENT");
    simulateRemoval(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationTest::testNoBadFileInInitialCommit()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    auto convId = createFakeConversation(carlaAccount);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool carlaConnected = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));
    libjami::addConversationMember(carlaId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationTest::testNoBadCertInInitialCommit()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto fakeCert = aliceAccount->certStore().getCertificate(
        std::string(aliceAccount->currentDeviceId()));
    auto carlaCert = carlaAccount->certStore().getCertificate(
        std::string(carlaAccount->currentDeviceId()));

    CPPUNIT_ASSERT(fakeCert);
    // Create a conversation from Carla with Alice's device
    auto convId = createFakeConversation(carlaAccount, fakeCert->toString(false));

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool carlaConnected = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));
    libjami::addConversationMember(carlaId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationTest::testPlainTextNoBadFile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0;
    bool memberMessageGenerated = false;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    std::string convId = libjami::startConversation(aliceId);
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;

    libjami::acceptConversationRequest(bobId, convId);
    cv.wait_for(lk, 30s, [&] { return conversationReady && memberMessageGenerated; });

    addFile(aliceAccount, convId, "BADFILE");
    Json::Value root;
    root["type"] = "text/plain";
    root["body"] = "hi";
    commit(aliceAccount, convId, root);
    errorDetected = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    // Check not received due to the unwanted file
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testVoteNoBadFile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, messageBobReceived = false, messageCarlaReceived = false,
         carlaConnected = false;
    ;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
    libjami::registerSignalHandlers(confHandlers);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaConnected; }));

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    // Now Alice remove Carla without a vote. Bob will not receive the message
    messageBobReceived = false;
    addFile(aliceAccount, convId, "BADFILE");
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    messageCarlaReceived = false;
    libjami::sendMessage(bobId, convId, "final"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageCarlaReceived; }));
}

void
ConversationTest::testETooBigClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    std::ofstream bad(repoPath + DIR_SEPARATOR_STR + "BADFILE");
    CPPUNIT_ASSERT(bad.is_open());
    for (int i = 0; i < 300 * 1024 * 1024; ++i)
        bad << "A";
    bad.close();

    addAll(aliceAccount, convId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    errorDetected = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testETooBigFetch()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    cv.wait_for(lk, 30s, [&]() { return requestReceived; });

    libjami::acceptConversationRequest(bobId, convId);
    cv.wait_for(lk, 30s, [&]() { return conversationReady; });

    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 2; });

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

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testUnknownModeDetected()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    ConversationRepository repo(aliceAccount, convId);
    Json::Value json;
    json["mode"] = 1412;
    json["type"] = "initial";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    repo.amend(convId, Json::writeString(wbuilder, json));
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         errorDetected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 2)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    errorDetected = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationTest::testUpdateProfile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    std::map<std::string, std::string> profileAlice, profileBob;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationProfileUpdated>(
            [&](const auto& accountId, const auto& /* conversationId */, const auto& profile) {
                if (accountId == aliceId) {
                    profileAlice = profile;
                } else if (accountId == bobId) {
                    profileBob = profile;
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && messageAliceReceived == 1; }));

    messageBobReceived = 0;
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return messageBobReceived == 1 && !profileAlice.empty() && !profileBob.empty();
    }));

    auto infos = libjami::conversationInfos(bobId, convId);
    // Verify that we have the same profile everywhere
    CPPUNIT_ASSERT(infos["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(profileAlice["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(profileBob["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(infos["description"].empty());
    CPPUNIT_ASSERT(profileAlice["description"].empty());
    CPPUNIT_ASSERT(profileBob["description"].empty());

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testGetProfileRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageAliceReceived = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId)
                messageAliceReceived += 1;
            cv.notify_one();
        }));
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    messageAliceReceived = 0;
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 1; }));

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    auto infos = libjami::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(infos["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(infos["description"].empty());

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testCheckProfileInConversationRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    CPPUNIT_ASSERT(requests.front()["title"] == "My awesome swarm");

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testCheckProfileInTrustRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    std::string convId = "";
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    std::vector<uint8_t> payload(vcard.begin(), vcard.end());
    aliceAccount->sendTrustRequest(bobUri, payload);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !convId.empty() && requestReceived; }));
}

void
ConversationTest::testMemberCannotUpdateProfile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                int code,
                const std::string& /* what */) {
                if (accountId == bobId && conversationId == convId && code == 4)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && messageAliceReceived == 1; }));

    messageBobReceived = 0;
    bobAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return errorDetected; }));

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testUpdateProfileWithBadFile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                int code,
                const std::string& /* what */) {
                if (accountId == bobId && conversationId == convId && code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && messageAliceReceived == 1; }));

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
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testFetchProfileUnauthorized()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    bool errorDetected = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                int code,
                const std::string& /* what */) {
                if (accountId == aliceId && conversationId == convId && code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && messageAliceReceived == 1; }));

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
    libjami::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));

    libjami::unregisterSignalHandlers();
}

void
ConversationTest::testSyncingWhileAccepting()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));

    auto convInfos = libjami::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(convInfos["syncing"] == "true");
    CPPUNIT_ASSERT(convInfos.find("created") != convInfos.end());

    Manager::instance().sendRegister(aliceId, true); // This avoid to sync immediately
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    convInfos = libjami::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(convInfos.find("syncing") == convInfos.end());
}

void
ConversationTest::testCountInteractions()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);

    std::string msgId1 = "", msgId2 = "", msgId3 = "";
    aliceAccount->convModule()
        ->sendMessage(convId, "1"s, "", "text/plain", true, {}, [&](bool, std::string commitId) {
            msgId1 = commitId;
            cv.notify_one();
        });
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !msgId1.empty(); }));
    aliceAccount->convModule()
        ->sendMessage(convId, "2"s, "", "text/plain", true, {}, [&](bool, std::string commitId) {
            msgId2 = commitId;
            cv.notify_one();
        });
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !msgId2.empty(); }));
    aliceAccount->convModule()
        ->sendMessage(convId, "3"s, "", "text/plain", true, {}, [&](bool, std::string commitId) {
            msgId3 = commitId;
            cv.notify_one();
        });
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !msgId3.empty(); }));

    CPPUNIT_ASSERT(libjami::countInteractions(aliceId, convId, "", "", "") == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(libjami::countInteractions(aliceId, convId, "", "", aliceAccount->getUsername())
                   == 0);
    CPPUNIT_ASSERT(libjami::countInteractions(aliceId, convId, msgId3, "", "") == 0);
    CPPUNIT_ASSERT(libjami::countInteractions(aliceId, convId, msgId2, "", "") == 1);
}

void
ConversationTest::testReplayConversation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         conversationRemoved = false, messageReceived = false;
    std::vector<std::string> bobMessages;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
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
    libjami::registerSignalHandlers(confHandlers);
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));
    // removeContact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemoved; }));
    // re-add
    CPPUNIT_ASSERT(convId != "");
    auto oldConvId = convId;
    convId = "";
    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty(); }));
    messageReceived = false;
    libjami::sendMessage(aliceId, convId, "foo"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived; }));
    messageReceived = false;
    libjami::sendMessage(aliceId, convId, "bar"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived; }));
    convId = "";
    bobMessages.clear();
    aliceAccount->sendTrustRequest(bobUri, {});
    // Should retrieve previous conversation
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobMessages.size() == 2 && bobMessages[0] == "foo" && bobMessages[1] == "bar";
    }));
}

void
ConversationTest::testSyncWithoutPinnedCert()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string convId = "";
    auto requestReceived = false, conversationReady = false, memberMessageGenerated = false,
         aliceMessageReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
                else if (message["type"] == "text/plain")
                    aliceMessageReceived = true;
            }
            cv.notify_one();
        }));
    auto bob2Started = false, aliceStopped = false, bob2Stopped = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);
                if (!bob2Account)
                    return;
                auto details = bob2Account->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED")
                    bob2Started = true;
                if (daemonStatus == "UNREGISTERED")
                    bob2Stopped = true;
                details = aliceAccount->getVolatileAccountDetails();
                daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "UNREGISTERED")
                    aliceStopped = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // Bob creates a second device
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);

    // Disconnect bob2, to create a valid conv betwen Alice and Bob1
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));
    bob2Stopped = false;
    Manager::instance().sendRegister(bob2Id, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Stopped; }));

    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    // Bob send a message
    libjami::sendMessage(bobId, convId, "hi"s, "");
    cv.wait_for(lk, 30s, [&]() { return aliceMessageReceived; });

    // Alice off, bob2 On
    conversationReady = false;
    aliceStopped = false;
    Manager::instance().sendRegister(aliceId, false);
    cv.wait_for(lk, 10s, [&]() { return aliceStopped; });
    Manager::instance().sendRegister(bob2Id, true);

    // Sync + validate
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
}

void
ConversationTest::testImportMalformedContacts()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto malformedContacts = fileutils::loadFile(std::filesystem::current_path().string()
                                                 + "/conversation/rsc/incorrectContacts");
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    archiver::compressGzip(malformedContacts, bobArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);
    wait_for_announcement_of({bob2Id});
    auto contacts = libjami::getContacts(bob2Id);
    CPPUNIT_ASSERT(contacts.size() == 1);
    CPPUNIT_ASSERT(contacts[0][libjami::Account::TrustRequest::CONVERSATIONID] == "");
}

void
ConversationTest::testRemoveReaddMultipleDevice()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:ALICE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    auto vCardPath = fmt::format("{}/{}/profile.vcf", fileutils::get_data_dir(), aliceId);
    // Add file
    auto p = std::filesystem::path(vCardPath);
    dhtnet::fileutils::recursive_mkdir(p.parent_path());
    std::ofstream file(p);
    if (file.is_open()) {
        file << vcard;
        file.close();
    }

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                else if (accountId == bob2Id)
                    requestReceivedBob2 = true;
                cv.notify_one();
            }));
    std::string convId = "";
    auto conversationReadyBob = false, conversationReadyBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id) {
                conversationReadyBob2 = true;
            }
            cv.notify_one();
        }));
    auto memberMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
            }
            cv.notify_one();
        }));
    auto bob2Started = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    auto conversationRmBob = false, conversationRmBob2 = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId)
                    conversationRmBob = true;
                else if (accountId == bob2Id)
                    conversationRmBob2 = true;
                cv.notify_one();
            }));
    auto aliceProfileReceivedBob = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ProfileReceived>(
        [&](const std::string& accountId, const std::string& peerId, const std::string& path) {
            if (accountId == bobId && peerId == aliceUri)
                aliceProfileReceivedBob = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Bob creates a second device
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));

    // Alice adds bob
    requestReceived = false, requestReceivedBob2 = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationReadyBob && conversationReadyBob2 && memberMessageGenerated;
    }));

    // Remove contact
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRmBob && conversationRmBob2; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(10s);

    // Alice send a message
    requestReceived = false, requestReceivedBob2 = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));

    // Re-Add contact should accept and clone the conversation on all devices
    conversationReadyBob = false;
    conversationReadyBob2 = false;
    aliceProfileReceivedBob = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationReadyBob && conversationReadyBob2 && aliceProfileReceivedBob;
    }));
}

void
ConversationTest::testCloneFromMultipleDevice()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                else if (accountId == bob2Id)
                    requestReceivedBob2 = true;
                cv.notify_one();
            }));
    std::string convId = "";
    auto conversationReadyBob = false, conversationReadyBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id) {
                conversationReadyBob2 = true;
            }
            cv.notify_one();
        }));
    auto memberMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
            }
            cv.notify_one();
        }));
    auto bob2Started = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    auto conversationRmAlice = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    conversationRmAlice = true;
                cv.notify_one();
            }));
    auto errorDetected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 1)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // Bob creates a second device
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));

    // Alice adds bob
    requestReceived = false, requestReceivedBob2 = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationReadyBob && conversationReadyBob2 && memberMessageGenerated;
    }));

    // Remove contact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRmAlice; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(10s);

    // Alice re-adds Bob
    auto oldConv = convId;
    conversationRmAlice = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // This should retrieve the conversation from Bob and don't show any error
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return errorDetected; }));
    CPPUNIT_ASSERT(conversationRmAlice);
    CPPUNIT_ASSERT(oldConv == convId); // Check that convId didn't change and conversation is ready.
}

void
ConversationTest::testSendReply()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::vector<std::map<std::string, std::string>> messageBobReceived = {},
                                                    messageAliceReceived = {};
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                messageBobReceived.emplace_back(message);
            } else {
                messageAliceReceived.emplace_back(message);
            }
            cv.notify_one();
        }));
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return messageAliceReceived.size() == 2; });

    messageBobReceived.clear();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageBobReceived.size() == 1; }));

    auto validId = messageBobReceived.at(0).at("id");
    libjami::sendMessage(aliceId, convId, "foo"s, validId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return messageBobReceived.size() == 2; }));
    CPPUNIT_ASSERT(messageBobReceived.rbegin()->at("reply-to") == validId);

    // Check if parent doesn't exists, no message is generated
    libjami::sendMessage(aliceId, convId, "foo"s, "invalid");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return messageBobReceived.size() == 3; }));
}

void
ConversationTest::testSearchInConv()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         messageReceived = false;
    std::vector<std::string> bobMessages;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "member")
                    memberMessageGenerated = true;
            } else if (accountId == bobId) {
                messageReceived = true;
            }
            cv.notify_one();
        }));
    std::vector<std::map<std::string, std::string>> messages;
    bool finished = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessagesFound>(
        [&](uint32_t,
            const std::string&,
            const std::string& conversationId,
            std::vector<std::map<std::string, std::string>> msg) {
            if (conversationId == convId)
                messages = msg;
            finished = conversationId.empty();
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));
    // Add some messages
    messageReceived = false;
    libjami::sendMessage(aliceId, convId, "message 1"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived; }));
    messageReceived = false;
    libjami::sendMessage(aliceId, convId, "message 2"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived; }));
    messageReceived = false;
    libjami::sendMessage(aliceId, convId, "Message 3"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived; }));

    libjami::searchConversation(aliceId, convId, "", "", "message", "", 0, 0, 0, 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messages.size() == 3 && finished; }));
    messages.clear();
    finished = false;
    libjami::searchConversation(aliceId, convId, "", "", "Message", "", 0, 0, 0, 1);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messages.size() == 1 && finished; }));
    messages.clear();
    finished = false;
    libjami::searchConversation(aliceId, convId, "", "", "message 2", "", 0, 0, 0, 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messages.size() == 1 && finished; }));
    messages.clear();
    finished = false;
    libjami::searchConversation(aliceId, convId, "", "", "foo", "", 0, 0, 0, 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messages.size() == 0 && finished; }));
}

void
ConversationTest::testConversationPreferences()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, conversationRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    // Start conversation and set preferences
    auto convId = libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return conversationReady; });
    CPPUNIT_ASSERT(libjami::getConversationPreferences(aliceId, convId).size() == 0);
    libjami::setConversationPreferences(aliceId, convId, {{"foo", "bar"}});
    auto preferences = libjami::getConversationPreferences(aliceId, convId);
    CPPUNIT_ASSERT(preferences.size() == 1);
    CPPUNIT_ASSERT(preferences["foo"] == "bar");
    // Update
    libjami::setConversationPreferences(aliceId, convId, {{"foo", "bar2"}, {"bar", "foo"}});
    preferences = libjami::getConversationPreferences(aliceId, convId);
    CPPUNIT_ASSERT(preferences.size() == 2);
    CPPUNIT_ASSERT(preferences["foo"] == "bar2");
    CPPUNIT_ASSERT(preferences["bar"] == "foo");
    // Remove conversations removes its preferences.
    CPPUNIT_ASSERT(libjami::removeConversation(aliceId, convId));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemoved; }));
    CPPUNIT_ASSERT(libjami::getConversationPreferences(aliceId, convId).size() == 0);
}

void
ConversationTest::testConversationPreferencesBeforeClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                else if (accountId == bob2Id)
                    requestReceivedBob2 = true;
                cv.notify_one();
            }));
    std::string convId = "";
    auto conversationReadyBob = false, conversationReadyBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id) {
                conversationReadyBob2 = true;
            }
            cv.notify_one();
        }));
    auto bob2Started = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    std::map<std::string, std::string> preferencesBob, preferencesBob2;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationPreferencesUpdated>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> preferences) {
                if (accountId == bobId)
                    preferencesBob = preferences;
                else if (accountId == bob2Id)
                    preferencesBob2 = preferences;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    // Bob creates a second device
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReadyBob; }));

    // Set preferences
    Manager::instance().sendRegister(aliceId, false);
    libjami::setConversationPreferences(bobId, convId, {{"foo", "bar"}, {"bar", "foo"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return preferencesBob.size() == 2; }));
    CPPUNIT_ASSERT(preferencesBob["foo"] == "bar" && preferencesBob["bar"] == "foo");

    // Bob2 should sync preferences
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bob2Started && conversationReadyBob2 && !preferencesBob2.empty();
    }));
    CPPUNIT_ASSERT(preferencesBob2["foo"] == "bar" && preferencesBob2["bar"] == "foo");
}

void
ConversationTest::testConversationPreferencesMultiDevices()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                else if (accountId == bob2Id)
                    requestReceivedBob2 = true;
                cv.notify_one();
            }));
    std::string convId = "";
    auto conversationReadyBob = false, conversationReadyBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id) {
                conversationReadyBob2 = true;
            }
            cv.notify_one();
        }));
    auto bob2Started = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    std::map<std::string, std::string> preferencesBob, preferencesBob2;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationPreferencesUpdated>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> preferences) {
                if (accountId == bobId)
                    preferencesBob = preferences;
                else if (accountId == bob2Id)
                    preferencesBob2 = preferences;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    // Bob creates a second device
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));
    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReadyBob && conversationReadyBob2; }));
    libjami::setConversationPreferences(bobId, convId, {{"foo", "bar"}, {"bar", "foo"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return preferencesBob.size() == 2 && preferencesBob2.size() == 2;
    }));
    CPPUNIT_ASSERT(preferencesBob["foo"] == "bar" && preferencesBob["bar"] == "foo");
    CPPUNIT_ASSERT(preferencesBob2["foo"] == "bar" && preferencesBob2["bar"] == "foo");
}

void
ConversationTest::testFixContactDetails()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string convId = "";
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !convId.empty(); }));

    auto details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"] == convId);
    // Erase convId from contact details, this should be fixed by next reload.
    CPPUNIT_ASSERT(aliceAccount->updateConvForContact(bobUri, convId, ""));
    details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"].empty());

    aliceAccount->convModule()->loadConversations();

    details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"] == convId);
}

void
ConversationTest::testRemoveOneToOneNotInDetails()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string convId = "", secondConv;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                if (convId.empty())
                    convId = conversationId;
                else
                    secondConv = conversationId;
            }
            cv.notify_one();
        }));
    bool conversationRemoved = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string& cid) {
                if (accountId == aliceId && cid == secondConv)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !convId.empty(); }));

    auto details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"] == convId);
    // Create a duplicate
    std::this_thread::sleep_for(2s); // Avoid to get same id
    aliceAccount->convModule()->startConversation(ConversationMode::ONE_TO_ONE, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !secondConv.empty(); }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + secondConv;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    aliceAccount->convModule()->loadConversations();

    // Check that conv is removed
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemoved; }));
}

void
ConversationTest::testMessageEdition()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::vector<std::map<std::string, std::string>> messageBobReceived;
    bool conversationReady = false, memberMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                messageBobReceived.emplace_back(message);
            } else if (accountId == aliceId && message["type"] == "member") {
                memberMessageGenerated = true;
            }
            cv.notify_one();
        }));
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    auto errorDetected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& /* accountId */,
                const std::string& /* conversationId */,
                int code,
                const std::string& /* what */) {
                if (code == 3)
                    errorDetected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));
    auto msgSize = messageBobReceived.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageBobReceived.size() == msgSize + 1; }));
    msgSize = messageBobReceived.size();
    auto editedId = messageBobReceived.rbegin()->at("id");
    libjami::sendMessage(aliceId, convId, "New body"s, editedId, 1);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return messageBobReceived.size() == msgSize + 1; }));
    CPPUNIT_ASSERT(messageBobReceived.rbegin()->at("edit") == editedId);
    CPPUNIT_ASSERT(messageBobReceived.rbegin()->at("body") == "New body");
    // Not an existing message
    msgSize = messageBobReceived.size();
    libjami::sendMessage(aliceId, convId, "New body"s, "invalidId", 1);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 10s, [&]() { return messageBobReceived.size() == msgSize + 1; }));
    // Invalid author
    libjami::sendMessage(aliceId, convId, "New body"s, convId, 1);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 10s, [&]() { return messageBobReceived.size() == msgSize + 1; }));
    // Add invalid edition
    Json::Value root;
    root["type"] = "application/edited-message";
    root["edit"] = convId;
    root["body"] = "new";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    auto message = Json::writeString(wbuilder, root);
    commitInRepo(repoPath, aliceAccount, message);
    errorDetected = false;
    libjami::sendMessage(aliceId, convId, "trigger"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationTest::testMessageReaction()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::vector<std::map<std::string, std::string>> messageAliceReceived;
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId)
                messageAliceReceived.emplace_back(message);
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == aliceId)
                conversationReady = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    auto msgSize = messageAliceReceived.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return messageAliceReceived.size() == msgSize + 1; }));
    msgSize = messageAliceReceived.size();

    auto reactId = messageAliceReceived.rbegin()->at("id");

    libjami::sendMessage(aliceId, convId, "👋"s, reactId, 2);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 10s, [&]() { return messageAliceReceived.size() == msgSize + 1; }));
    CPPUNIT_ASSERT(messageAliceReceived.rbegin()->at("react-to") == reactId);
    CPPUNIT_ASSERT(messageAliceReceived.rbegin()->at("body") == "👋");
}

void
ConversationTest::testLoadPartiallyRemovedConversation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    std::string convId = "";
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            }
            cv.notify_one();
        }));
    bool conversationRemoved = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    // Copy alice's conversation temporary
    auto repoPathAlice = fmt::format("{}/{}/conversations/{}", fileutils::get_data_dir(),
                                     aliceAccount->getAccountID(), convId);
    std::filesystem::copy(repoPathAlice, fmt::format("./{}", convId), std::filesystem::copy_options::recursive);

    // removeContact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemoved; }));
    std::this_thread::sleep_for(10s); // Wait for connection to close and async tasks to finish

    // Copy back alice's conversation
    std::filesystem::copy(fmt::format("./{}", convId), repoPathAlice, std::filesystem::copy_options::recursive);
    std::filesystem::remove_all(fmt::format("./{}", convId));

    // Reloading conversation should remove directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPathAlice));
    aliceAccount->convModule()->loadConversations();
    CPPUNIT_ASSERT(!std::filesystem::is_directory(repoPathAlice));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationTest::name())
