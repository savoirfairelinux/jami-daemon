/*
 *  Copyright (C) 2024 Savoir-faire Linux Inc.
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

namespace jami {
namespace test {

struct UserData {
    std::string conversationId;
    bool requestReceived {false};
    bool registered {false};
    bool stopped {false};
    bool deviceAnnounced {false};
    std::vector<libjami::SwarmMessage> messages;
    std::map<std::string, int> status;
};

class ConversationFetchSentTest : public CppUnit::TestFixture
{
public:
    ~ConversationFetchSentTest() { libjami::fini(); }
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
    void testSyncFetch();

    CPPUNIT_TEST_SUITE(ConversationFetchSentTest);
    CPPUNIT_TEST(testSyncFetch);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationFetchSentTest, ConversationFetchSentTest::name());

void
ConversationFetchSentTest::setUp()
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
ConversationFetchSentTest::connectSignals()
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
            } else if (accountId == bob2Id) {
                bob2Data.messages.emplace_back(message);
            } else if (accountId == carlaId) {
                carlaData.messages.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            [&](const std::string& accountId,
                const std::string& /*conversationId*/,
                const std::string& /*peer*/,
                const std::string& msgId,
                int status) {
                if (accountId == aliceId) {
                    aliceData.status[msgId] = status;
                } else if (accountId == bobId) {
                    bobData.status[msgId] = status;
                } else if (accountId == bob2Id) {
                    bob2Data.status[msgId] = status;
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
}

void
ConversationFetchSentTest::tearDown()
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
ConversationFetchSentTest::testSyncFetch()
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.deviceAnnounced; }));

    // Create conversation between alice and bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && !bob2Data.conversationId.empty(); }));

    std::this_thread::sleep_for(5s); // Wait for all join messages to be received

    // bob send 4 messages
    auto aliceMsgSize = aliceData.messages.size(), bob2MsgSize = bob2Data.messages.size(), bobMsgSize = bobData.messages.size();
    JAMI_ERROR("@@@@@@@@@@@@@@@@@@@@@@@@@");
    libjami::sendMessage(bobId, bobData.conversationId, "1"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData.messages.size() == aliceMsgSize + 1
                && bobData.messages.size() == bobMsgSize + 1
                && bob2Data.messages.size() == bob2MsgSize + 1; }));
    auto msgId1 = aliceData.messages.rbegin()->id;
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { JAMI_ERROR("@@@ msgId1 {} {} {}", msgId1, bob2Data.status[msgId1], bobData.status[msgId1]);return bob2Data.status[msgId1] == 2 && bobData.status[msgId1] == 2; }));
    JAMI_ERROR("@@@@@@@@@@@@@@@@@@@@@@@@@");
    libjami::sendMessage(bobId, bobData.conversationId, "2"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 2
                                                    && bobData.messages.size() == bobMsgSize + 2
                                                    && bob2Data.messages.size() == bob2MsgSize + 2;}));
    auto msgId2 = aliceData.messages.rbegin()->id;
    // TODO status should take messageReceive if not present!
    // Because bob2Data.status is here only on update, but msgReceived can be good directly at first
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        JAMI_ERROR("@@@ msgId2 {} {} {}", msgId2, bob2Data.status[msgId2], bobData.status[msgId2]);return bob2Data.status[msgId2] == 2 && bobData.status[msgId2] == 2; }));
    JAMI_ERROR("@@@@@@@@@@@@@@@@@@@@@@@@@");
    libjami::sendMessage(bobId, bobData.conversationId, "3"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 3
                                                    && bobData.messages.size() == bobMsgSize + 3
                                                    && bob2Data.messages.size() == bob2MsgSize + 3; }));
    auto msgId3 = aliceData.messages.rbegin()->id;
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { JAMI_ERROR("@@@ msgId3 {} {} {}", msgId3, bob2Data.status[msgId3], bobData.status[msgId3]);return bob2Data.status[msgId3] == 2 && bobData.status[msgId3] == 2; }));
    JAMI_ERROR("@@@@@@@@@@@@@@@@@@@@@@@@@");
    libjami::sendMessage(bobId, bobData.conversationId, "4"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 4
                                                    && bobData.messages.size() == bobMsgSize + 4
                                                    && bob2Data.messages.size() == bob2MsgSize + 4; }));
    auto msgId4 = aliceData.messages.rbegin()->id;
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { JAMI_ERROR("@@@ msgId4 {} {} {}", msgId4, bob2Data.status[msgId4], bobData.status[msgId4]);return bob2Data.status[msgId4] == 2 && bobData.status[msgId4] == 2; }));

    // Second message is set to displayed by alice
    aliceAccount->setMessageDisplayed("swarm:" + aliceData.conversationId, msgId2, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.status[msgId1] == 3 && bobData.status[msgId2] == 3
               && bob2Data.status[msgId1] == 3 && bob2Data.status[msgId2] == 3; }));
    CPPUNIT_ASSERT(bobData.status[msgId3] == 2 && bobData.status[msgId4] == 2
                  && bob2Data.status[msgId3] == 2 && bob2Data.status[msgId4] == 2);

    // Alice is disabled
    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.stopped; }));

    // Bob send 2 more messages
    bob2MsgSize = bob2Data.messages.size();
    libjami::sendMessage(bobId, bobData.conversationId, "5"s, "");
    libjami::sendMessage(bobId, bobData.conversationId, "6"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.messages.size() == bob2MsgSize + 2; }));
    auto msgId5 = (bobData.messages.rbegin() + 1)->id;
    auto msgId6 = bobData.messages.rbegin()->id;
    // No update for bob2data
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.status[msgId5] == 1 && bobData.status[msgId6] == 1
                                                && bob2Data.status.find(msgId5) == bob2Data.status.end()
                                                && bob2Data.status.find(msgId6) == bob2Data.status.end(); }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationFetchSentTest::name())
