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

struct UserData {
    std::string conversationId;
    bool removed {false};
    bool requestReceived {false};
    bool errorDetected {false};
    bool registered {false};
    bool stopped {false};
    bool deviceAnnounced {false};
    bool composing {false};
    bool sending {false};
    bool sent {false};
    bool searchFinished {false};
    std::string profilePath;
    std::string payloadTrustRequest;
    std::map<std::string, std::string> profile;
    std::vector<libjami::SwarmMessage> messages;
    std::vector<libjami::SwarmMessage> messagesUpdated;
    std::vector<std::map<std::string, std::string>> reactions;
    std::vector<std::map<std::string, std::string>> messagesFound;
    std::vector<std::string> reactionRemoved;
    std::map<std::string, std::string> preferences;
};

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
    UserData aliceData;
    std::string alice2Id;
    UserData alice2Data;
    std::string bobId;
    UserData bobData;
    std::string bob2Id;
    UserData bob2Data;
    std::string carlaId;
    UserData carlaData;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    void connectSignals();

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

    aliceData = {};
    alice2Data = {};
    bobData = {};
    bob2Data = {};
    carlaData = {};

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
ConversationTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>&) {
                if (accountId == aliceId) {
                    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
                    auto details = aliceAccount->getVolatileAccountDetails();
                    auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                    if (daemonStatus == "REGISTERED") {
                        aliceData.registered = true;
                    } else if (daemonStatus == "UNREGISTERED") {
                        aliceData.stopped = true;
                    }
                    auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                    aliceData.deviceAnnounced = deviceAnnounced == "true";
                } else if (accountId == bobId) {
                    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
                    auto details = bobAccount->getVolatileAccountDetails();
                    auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                    if (daemonStatus == "REGISTERED") {
                        bobData.registered = true;
                    } else if (daemonStatus == "UNREGISTERED") {
                        bobData.stopped = true;
                    }
                    auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                    bobData.deviceAnnounced = deviceAnnounced == "true";
                } else if (accountId == bob2Id) {
                    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);
                    auto details = bob2Account->getVolatileAccountDetails();
                    auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                    if (daemonStatus == "REGISTERED") {
                        bob2Data.registered = true;
                    } else if (daemonStatus == "UNREGISTERED") {
                        bob2Data.stopped = true;
                    }
                    auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                    bob2Data.deviceAnnounced = deviceAnnounced == "true";
                } else if (accountId == carlaId) {
                    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
                    auto details = carlaAccount->getVolatileAccountDetails();
                    auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                    if (daemonStatus == "REGISTERED") {
                        carlaData.registered = true;
                    } else if (daemonStatus == "UNREGISTERED") {
                        carlaData.stopped = true;
                    }
                    auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                    carlaData.deviceAnnounced = deviceAnnounced == "true";
                }
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                aliceData.conversationId = conversationId;
            } else if (accountId == alice2Id) {
                alice2Data.conversationId = conversationId;
            } else if (accountId == bobId) {
                bobData.conversationId = conversationId;
            } else if (accountId == bob2Id) {
                bob2Data.conversationId = conversationId;
            } else if (accountId == carlaId) {
                carlaData.conversationId = conversationId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& payload,
                time_t /*received*/) {
                auto payloadStr = std::string(payload.data(), payload.data() + payload.size());
                if (account_id == aliceId)
                    aliceData.payloadTrustRequest = payloadStr;
                else if (account_id == bobId)
                    bobData.payloadTrustRequest = payloadStr;
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == aliceId) {
                    aliceData.requestReceived = true;
                } else if (accountId == bobId) {
                    bobData.requestReceived = true;
                } else if (accountId == bob2Id) {
                    bob2Data.requestReceived = true;
                } else if (accountId == carlaId) {
                    carlaData.requestReceived = true;
                }
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            libjami::SwarmMessage message) {
            if (accountId == aliceId) {
                aliceData.messages.emplace_back(message);
            } else if (accountId == bobId) {
                bobData.messages.emplace_back(message);
            } else if (accountId == carlaId) {
                carlaData.messages.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageUpdated>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            libjami::SwarmMessage message) {
            if (accountId == aliceId) {
                aliceData.messagesUpdated.emplace_back(message);
            } else if (accountId == bobId) {
                bobData.messagesUpdated.emplace_back(message);
            } else if (accountId == carlaId) {
                carlaData.messagesUpdated.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ReactionAdded>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            const std::string& /* messageId */,
            std::map<std::string, std::string> reaction) {
            if (accountId == aliceId) {
                aliceData.reactions.emplace_back(reaction);
            } else if (accountId == bobId) {
                bobData.reactions.emplace_back(reaction);
            } else if (accountId == carlaId) {
                carlaData.reactions.emplace_back(reaction);
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ReactionRemoved>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            const std::string& /* messageId */,
            const std::string& reactionId) {
            if (accountId == aliceId) {
                aliceData.reactionRemoved.emplace_back(reactionId);
            } else if (accountId == bobId) {
                bobData.reactionRemoved.emplace_back(reactionId);
            } else if (accountId == carlaId) {
                carlaData.reactionRemoved.emplace_back(reactionId);
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& /* conversationId */,
                int /*code*/,
                const std::string& /* what */) {
                if (accountId == aliceId)
                    aliceData.errorDetected = true;
                else if (accountId == bobId)
                    bobData.errorDetected = true;
                else if (accountId == carlaId)
                    carlaData.errorDetected = true;
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::ComposingStatusChanged>(
            [&](const std::string& accountId,
                const std::string& /*conversationId*/,
                const std::string& /*peer*/,
                bool state) {
                if (accountId == bobId) {
                    bobData.composing = state;
                }
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& /*conversationId*/,
                const std::string& /*peer*/,
                const std::string& /*msgId*/,
                int status) {
                if (accountId == aliceId) {
                    if (status == 2)
                        aliceData.sending = true;
                    if (status == 3)
                        aliceData.sent = true;
                } else if (accountId == bobId) {
                    if (status == 2)
                        bobData.sending = true;
                    if (status == 3)
                        bobData.sent = true;
                }
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationProfileUpdated>(
            [&](const auto& accountId, const auto& /* conversationId */, const auto& profile) {
                if (accountId == aliceId) {
                    aliceData.profile = profile;
                } else if (accountId == bobId) {
                    bobData.profile = profile;
                }
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    aliceData.removed = true;
                else if (accountId == bobId)
                    bobData.removed = true;
                else if (accountId == bob2Id)
                    bob2Data.removed = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ProfileReceived>(
        [&](const std::string& accountId, const std::string& peerId, const std::string& path) {
            if (accountId == bobId)
                bobData.profilePath = path;
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationPreferencesUpdated>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> preferences) {
                if (accountId == bobId)
                    bobData.preferences = preferences;
                else if (accountId == bob2Id)
                    bob2Data.preferences = preferences;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessagesFound>(
        [&](uint32_t,
            const std::string& accountId,
            const std::string& conversationId,
            std::vector<std::map<std::string, std::string>> msg) {
            if (accountId == aliceId) {
                aliceData.messagesFound.insert(aliceData.messagesFound.end(), msg.begin(), msg.end());
                aliceData.searchFinished = conversationId.empty();
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
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
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));
    ConversationRepository repo(aliceAccount, convId);
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::INVITES_ONLY);

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
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
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));

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
    connectSignals();

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));

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
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == 2; });

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    cv.wait_for(lk, 30s, [&]() { return bobData.messages.size() == bobMsgSize + 1; });
}

void
ConversationTest::testSendMessageWithBadDisplayName()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::string> details;
    details[ConfProperties::DISPLAYNAME] = "<o>";
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.registered; }));

    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == 2; }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.messages.size() == bobMsgSize + 1; }));
}

void
ConversationTest::testReplaceWithBadCertificate()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    // Wait that alice sees Bob
    cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == 2; });

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
    commitInRepo(repoPath, aliceAccount, message);
    // now we need to sync!
    bobData.errorDetected = false;
    libjami::sendMessage(aliceId, convId, "trigger sync!"s, "");
    // We should detect the incorrect commit!
    cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; });
}

void
ConversationTest::testSendMessageTriggerMessageReceived()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&] { return !aliceData.conversationId.empty(); });

    auto msgSize = aliceData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceData.messages.size() == msgSize + 1; }));
}

void
ConversationTest::testMergeTwoDifferentHeads()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    carlaAccount->trackBuddyPresence(aliceUri, true);
    auto convId = libjami::startConversation(aliceId);

    auto msgSize = aliceData.messages.size();
    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == msgSize + 1; }));

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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return !carlaData.messages.empty(); }));
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
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 60s, [&]() { return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "ping"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobMsgSize + 1 == bobData.messages.size() && aliceMsgSize + 1 == aliceData.messages.size();
    }));
    libjami::sendMessage(bobId, convId, "pong"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobMsgSize + 2 == bobData.messages.size() && aliceMsgSize + 2 == aliceData.messages.size();
    }));
    libjami::sendMessage(bobId, convId, "ping"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobMsgSize + 3 == bobData.messages.size() && aliceMsgSize + 3 == aliceData.messages.size();
    }));
    libjami::sendMessage(aliceId, convId, "pong"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobMsgSize + 4 == bobData.messages.size() && aliceMsgSize + 4 == aliceData.messages.size();
    }));
}

void
ConversationTest::testIsComposing()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    aliceAccount->setIsComposing("swarm:" + convId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.composing; }));
    aliceAccount->setIsComposing("swarm:" + convId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.composing; }));
}

void
ConversationTest::testMessageStatus()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    aliceData.sending = false;
    aliceData.sent = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.sending && aliceData.sent; }));
}

void
ConversationTest::testSetMessageDisplayed()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

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
                                           && infos["lastDisplayed"] == aliceData.messages[0].id;
                                })
                   != membersInfos.end());
    bobData.sent = false;
    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.sent; }));

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
}

void
ConversationTest::testSetMessageDisplayedTwice()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    bobData.sent = false;
    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.sent; }));

    bobData.sent = false;
    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobData.sent; }));
}

void
ConversationTest::testSetMessageDisplayedPreference()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    auto details = aliceAccount->getAccountDetails();
    CPPUNIT_ASSERT(details[ConfProperties::SENDREADRECEIPT] == "true");
    details[ConfProperties::SENDREADRECEIPT] = "false";
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.registered; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size(); }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    // Last displayed messages should not be set yet
    auto membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    // Last read for alice is when bob is added to the members
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == aliceData.messages[0].id;
                                })
                   != membersInfos.end());

    aliceAccount->setMessageDisplayed("swarm:" + convId, convId, 3);
    // Bob should not receive anything here, as sendMessageDisplayed is disabled for Alice
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobData.sent; }));

    // Assert that message is set as displayed for self (for the read status)
    membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());
}

void
ConversationTest::testSetMessageDisplayedAfterClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !alice2Data.conversationId.empty(); }));

    // Assert that message is set as displayed for self (for the read status)
    auto membersInfos = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) {
                                    return infos["uri"] == aliceUri
                                           && infos["lastDisplayed"] == convId;
                                })
                   != membersInfos.end());
}

std::string
ConversationTest::createFakeConversation(std::shared_ptr<JamiAccount> account,
                                         const std::string& fakeCert)
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
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaData.deviceAnnounced; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = bobData.messages.size();
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && bobMsgSize + 2 == bobData.messages.size(); }));

    // Now Alice removes Carla with a non empty file
    addVote(aliceAccount, convId, carlaUri, "CONTENT");
    simulateRemoval(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.errorDetected; }));
}

void
ConversationTest::testNoBadFileInInitialCommit()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();

    auto convId = createFakeConversation(carlaAccount);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaData.deviceAnnounced; }));
    libjami::addConversationMember(carlaId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.requestReceived; }));

    libjami::acceptConversationRequest(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.errorDetected; }));
}

void
ConversationTest::testNoBadCertInInitialCommit()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

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

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaData.deviceAnnounced; }));
    libjami::addConversationMember(carlaId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.requestReceived; }));

    libjami::acceptConversationRequest(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.errorDetected; }));
}

void
ConversationTest::testPlainTextNoBadFile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::string convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    addFile(aliceAccount, convId, "BADFILE");
    Json::Value root;
    root["type"] = "text/plain";
    root["body"] = "hi";
    commit(aliceAccount, convId, root);
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    // Check not received due to the unwanted file
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationTest::testVoteNoBadFile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.deviceAnnounced; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = bobData.messages.size();
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && bobMsgSize + 2 == bobData.messages.size(); }));

    // Now Alice remove Carla without a vote. Bob will not receive the message
    addFile(aliceAccount, convId, "BADFILE");
    aliceMsgSize = aliceData.messages.size();
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    auto carlaMsgSize = carlaData.messages.size();
    libjami::sendMessage(bobId, convId, "final"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaMsgSize + 1 == carlaData.messages.size(); }));
}

void
ConversationTest::testETooBigClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationTest::testETooBigFetch()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    // Wait that alice sees Bob
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    std::ofstream bad(repoPath + DIR_SEPARATOR_STR + "BADFILE");
    CPPUNIT_ASSERT(bad.is_open());
    for (int i = 0; i < 300 * 1024 * 1024; ++i)
        bad << "A";
    bad.close();

    addAll(aliceAccount, convId);
    Json::Value json;
    json["body"] = "o/";
    json["type"] = "text/plain";
    commit(aliceAccount, convId, json);

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationTest::testUnknownModeDetected()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

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
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationTest::testUpdateProfile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && !bobData.conversationId.empty(); }));

    auto bobMsgSize = bobData.messages.size();
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobMsgSize + 1 == bobData.messages.size() && !aliceData.profile.empty() && !bobData.profile.empty();
    }));

    auto infos = libjami::conversationInfos(bobId, convId);
    // Verify that we have the same profile everywhere
    CPPUNIT_ASSERT(infos["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(aliceData.profile["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(bobData.profile["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(infos["description"].empty());
    CPPUNIT_ASSERT(aliceData.profile["description"].empty());
    CPPUNIT_ASSERT(bobData.profile["description"].empty());
}

void
ConversationTest::testGetProfileRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto infos = libjami::conversationInfos(bobId, convId);
    CPPUNIT_ASSERT(infos["title"] == "My awesome swarm");
    CPPUNIT_ASSERT(infos["description"].empty());
}

void
ConversationTest::testCheckProfileInConversationRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    aliceAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    CPPUNIT_ASSERT(requests.front()["title"] == "My awesome swarm");
}

void
ConversationTest::testCheckProfileInTrustRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    aliceAccount->addContact(bobUri);
    std::vector<uint8_t> payload(vcard.begin(), vcard.end());
    aliceAccount->sendTrustRequest(bobUri, payload);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return bobData.payloadTrustRequest == vcard; }));
}

void
ConversationTest::testMemberCannotUpdateProfile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && !bobData.conversationId.empty(); }));

    bobAccount->convModule()->updateConversationInfos(convId, {{"title", "My awesome swarm"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return bobData.errorDetected; }));
}

void
ConversationTest::testUpdateProfileWithBadFile()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && !bobData.conversationId.empty(); }));

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
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationTest::testFetchProfileUnauthorized()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && !bobData.conversationId.empty(); }));

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
    libjami::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.errorDetected; }));
}

void
ConversationTest::testSyncingWhileAccepting()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));

    auto convInfos = libjami::conversationInfos(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(convInfos["syncing"] == "true");
    CPPUNIT_ASSERT(convInfos.find("created") != convInfos.end());

    Manager::instance().sendRegister(aliceId, true); // This avoid to sync immediately
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    convInfos = libjami::conversationInfos(bobId, bobData.conversationId);
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
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() {
            return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size(); }));
    // removeContact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.removed; }));
    std::this_thread::sleep_for(5s);
    // re-add
    CPPUNIT_ASSERT(bobData.conversationId != "");
    auto oldConvId = bobData.conversationId;
    aliceData.conversationId = "";
    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));
    aliceMsgSize = aliceData.messages.size();
    libjami::sendMessage(aliceId, aliceData.conversationId, "foo"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::sendMessage(aliceId, aliceData.conversationId, "bar"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));
    bobData.messages.clear();
    aliceAccount->sendTrustRequest(bobUri, {});
    // Should retrieve previous conversation
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        JAMI_ERROR("@@@ {}", bobData.messages.size());
        if (bobData.messages.size() > 0)
            JAMI_ERROR("@@@ {}", bobData.messages[0].body["body"]);
        return bobData.messages.size() == 2 && bobData.messages[0].body["body"] == "foo" && bobData.messages[1].body["body"] == "bar";
    }));
}

void
ConversationTest::testSyncWithoutPinnedCert()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.registered; }));
    Manager::instance().sendRegister(bob2Id, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.stopped; }));

    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() {
            return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Bob send a message
    libjami::sendMessage(bobId, bobData.conversationId, "hi"s, "");
    cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); });

    // Alice off, bob2 On
    Manager::instance().sendRegister(aliceId, false);
    cv.wait_for(lk, 10s, [&]() { return aliceData.stopped; });
    Manager::instance().sendRegister(bob2Id, true);

    // Sync + validate
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bob2Data.conversationId.empty(); }));
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
    connectSignals();

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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.deviceAnnounced; }));

    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData.conversationId.empty() && !bob2Data.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size();
    }));

    // Remove contact
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.removed && bob2Data.removed; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(5s);

    // Alice send a message
    bobData.requestReceived = false; bob2Data.requestReceived = false;
    libjami::sendMessage(aliceId, aliceData.conversationId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));

    // Re-Add contact should accept and clone the conversation on all devices
    bobData.conversationId = ""; bob2Data.conversationId = "";
    bobData.profile.clear();
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return!bobData.conversationId.empty() && !bob2Data.conversationId.empty() && !bobData.profilePath.empty();
    }));
}

void
ConversationTest::testCloneFromMultipleDevice()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.deviceAnnounced; }));

    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData.conversationId.empty() && !bob2Data.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size();
    }));

    // Remove contact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.removed; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(10s);

    // Alice re-adds Bob
    auto oldConv = bobData.conversationId;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // This should retrieve the conversation from Bob and don't show any error
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return aliceData.errorDetected; }));
    CPPUNIT_ASSERT(oldConv == aliceData.conversationId); // Check that convId didn't change and conversation is ready.
}

void
ConversationTest::testSendReply()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size(); }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.messages.size() == bobMsgSize + 1; }));

    auto validId = bobData.messages.at(0).id;
    libjami::sendMessage(aliceId, convId, "foo"s, validId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return bobData.messages.size() == bobMsgSize + 2; }));
    CPPUNIT_ASSERT(bobData.messages.rbegin()->body.at("reply-to") == validId);

    // Check if parent doesn't exists, no message is generated
    libjami::sendMessage(aliceId, convId, "foo"s, "invalid");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobData.messages.size() == bobMsgSize + 3; }));
}

void
ConversationTest::testSearchInConv()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    auto aliceMsgSize = aliceData.messages.size();
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size(); }));
    // Add some messages
    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, aliceData.conversationId, "message 1"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
    libjami::sendMessage(aliceId, aliceData.conversationId, "message 2"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 2 == bobData.messages.size(); }));
    libjami::sendMessage(aliceId, aliceData.conversationId, "Message 3"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 3 == bobData.messages.size(); }));

    libjami::searchConversation(aliceId, aliceData.conversationId, "", "", "message", "", 0, 0, 0, 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messagesFound.size() == 3 && aliceData.searchFinished; }));
    aliceData.messagesFound.clear();
    aliceData.searchFinished = false;
    libjami::searchConversation(aliceId, aliceData.conversationId, "", "", "Message", "", 0, 0, 0, 1);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messagesFound.size() == 1 && aliceData.searchFinished; }));
    aliceData.messagesFound.clear();
    aliceData.searchFinished = false;
    libjami::searchConversation(aliceId, aliceData.conversationId, "", "", "message 2", "", 0, 0, 0, 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messagesFound.size() == 1 && aliceData.searchFinished; }));
    aliceData.messagesFound.clear();
    aliceData.searchFinished = false;
    libjami::searchConversation(aliceId, aliceData.conversationId, "", "", "foo", "", 0, 0, 0, 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messagesFound.size() == 0 && aliceData.searchFinished; }));
}

void
ConversationTest::testConversationPreferences()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    // Start conversation and set preferences
    auto convId = libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); });
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.removed; }));
    CPPUNIT_ASSERT(libjami::getConversationPreferences(aliceId, convId).size() == 0);
}

void
ConversationTest::testConversationPreferencesBeforeClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Bob creates a second device
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    // Set preferences
    Manager::instance().sendRegister(aliceId, false);
    libjami::setConversationPreferences(bobId, bobData.conversationId, {{"foo", "bar"}, {"bar", "foo"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.preferences.size() == 2; }));
    CPPUNIT_ASSERT(bobData.preferences["foo"] == "bar" && bobData.preferences["bar"] == "foo");

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
        return bob2Data.deviceAnnounced && !bob2Data.conversationId.empty() && !bob2Data.preferences.empty();
    }));
    CPPUNIT_ASSERT(bob2Data.preferences["foo"] == "bar" && bob2Data.preferences["bar"] == "foo");
}

void
ConversationTest::testConversationPreferencesMultiDevices()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.deviceAnnounced; }));
    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && !bob2Data.conversationId.empty(); }));
    libjami::setConversationPreferences(bobId, bobData.conversationId, {{"foo", "bar"}, {"bar", "foo"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.preferences.size() == 2 && bob2Data.preferences.size() == 2;
    }));
    CPPUNIT_ASSERT(bobData.preferences["foo"] == "bar" && bobData.preferences["bar"] == "foo");
    CPPUNIT_ASSERT(bob2Data.preferences["foo"] == "bar" && bob2Data.preferences["bar"] == "foo");
}

void
ConversationTest::testFixContactDetails()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !aliceData.conversationId.empty(); }));

    auto details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"] == aliceData.conversationId);
    // Erase convId from contact details, this should be fixed by next reload.
    CPPUNIT_ASSERT(aliceAccount->updateConvForContact(bobUri, aliceData.conversationId, ""));
    details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"].empty());

    aliceAccount->convModule()->loadConversations();

    details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"] == aliceData.conversationId);
}

void
ConversationTest::testRemoveOneToOneNotInDetails()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !aliceData.conversationId.empty(); }));

    auto details = aliceAccount->getContactDetails(bobUri);
    CPPUNIT_ASSERT(details["conversationId"] == aliceData.conversationId);
    auto firstConv = aliceData.conversationId;
    // Create a duplicate
    std::this_thread::sleep_for(2s); // Avoid to get same id
    aliceAccount->convModule()->startConversation(ConversationMode::ONE_TO_ONE, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return firstConv != aliceData.conversationId; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + aliceData.conversationId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    aliceAccount->convModule()->loadConversations();

    // Check that conv is removed
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.removed; }));
}

void
ConversationTest::testMessageEdition()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && aliceData.messages.size() == aliceMsgSize + 1; }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.messages.size() == bobMsgSize + 1; }));
    auto editedId = bobData.messages.rbegin()->id;
    // Should trigger MessageUpdated
    bobMsgSize = bobData.messagesUpdated.size();
    libjami::sendMessage(aliceId, convId, "New body"s, editedId, 1);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return bobData.messagesUpdated.size() == bobMsgSize + 1; }));
    CPPUNIT_ASSERT(bobData.messagesUpdated.rbegin()->body.at("body") == "New body");
    // Not an existing message
    bobMsgSize = bobData.messagesUpdated.size();
    libjami::sendMessage(aliceId, convId, "New body"s, "invalidId", 1);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 10s, [&]() { return bobData.messagesUpdated.size() == bobMsgSize + 1; }));
    // Invalid author
    libjami::sendMessage(aliceId, convId, "New body"s, convId, 1);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 10s, [&]() { return bobData.messagesUpdated.size() == bobMsgSize + 1; }));
    // Add invalid edition
    Json::Value root;
    root["type"] = "application/edited-message";
    root["edit"] = convId;
    root["body"] = "new";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    auto message = Json::writeString(wbuilder, root);
    commitInRepo(repoPath, aliceAccount, message);
    bobData.errorDetected = false;
    libjami::sendMessage(aliceId, convId, "trigger"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));

}

void
ConversationTest::testMessageReaction()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));
    auto msgSize = aliceData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == msgSize + 1; }));
    msgSize = aliceData.messages.size();

    // Add reaction
    auto reactId = aliceData.messages.rbegin()->id;
    libjami::sendMessage(aliceId, convId, "👋"s, reactId, 2);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 10s, [&]() { return aliceData.reactions.size() == 1; }));
    CPPUNIT_ASSERT(aliceData.reactions.rbegin()->at("react-to") == reactId);
    CPPUNIT_ASSERT(aliceData.reactions.rbegin()->at("body") == "👋");
    auto emojiId = aliceData.reactions.rbegin()->at("id");

    // Remove reaction
    libjami::sendMessage(aliceId, convId, ""s, emojiId, 1);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 10s, [&]() { return aliceData.reactionRemoved.size() == 1; }));
    CPPUNIT_ASSERT(emojiId == aliceData.reactionRemoved[0]);
}

void
ConversationTest::testLoadPartiallyRemovedConversation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    // Copy alice's conversation temporary
    auto repoPathAlice = fmt::format("{}/{}/conversations/{}", fileutils::get_data_dir(),
                                     aliceId, aliceData.conversationId);
    std::filesystem::copy(repoPathAlice, fmt::format("./{}", aliceData.conversationId), std::filesystem::copy_options::recursive);

    // removeContact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.removed; }));
    std::this_thread::sleep_for(10s); // Wait for connection to close and async tasks to finish

    // Copy back alice's conversation
    std::filesystem::copy(fmt::format("./{}", aliceData.conversationId), repoPathAlice, std::filesystem::copy_options::recursive);
    std::filesystem::remove_all(fmt::format("./{}", aliceData.conversationId));

    // Reloading conversation should remove directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPathAlice));
    aliceAccount->convModule()->loadConversations();
    CPPUNIT_ASSERT(!std::filesystem::is_directory(repoPathAlice));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationTest::name())
