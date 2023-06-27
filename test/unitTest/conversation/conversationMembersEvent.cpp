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

#include "manager.h"
#include "../../test_runner.h"
#include "jami.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"
#include "conversation/conversationcommon.h"

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

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
    std::string payloadTrustRequest;
    std::vector<libjami::SwarmMessage> messages;
    std::vector<libjami::SwarmMessage> messagesUpdated;
};

class ConversationMembersEventTest : public CppUnit::TestFixture
{
public:
    ~ConversationMembersEventTest() { libjami::fini(); }
    static std::string name() { return "ConversationMembersEventTest"; }
    void setUp();
    void tearDown();
    void generateFakeInvite(std::shared_ptr<JamiAccount> account,
                            const std::string& convId,
                            const std::string& uri);

    void testRemoveConversationNoMember();
    void testRemoveConversationWithMember();
    void testAddMember();
    void testMemberAddedNoBadFile();
    void testAddOfflineMemberThenConnects();
    void testGetMembers();
    void testRemoveMember();
    void testRemovedMemberDoesNotReceiveMessage();
    void testRemoveInvitedMember();
    void testMemberBanNoBadFile();
    void testMemberTryToRemoveAdmin();
    void testBannedMemberCannotSendMessage();
    void testAdminCanReAddMember();
    void testMemberCannotBanOther();
    void testMemberCannotUnBanOther();
    void testCheckAdminFakeAVoteIsDetected();
    void testAdminCannotKickTheirself();
    void testCommitUnauthorizedUser();
    void testMemberJoinsNoBadFile();
    void testMemberAddedNoCertificate();
    void testMemberJoinsInviteRemoved();
    void testFailAddMemberInOneToOne();
    void testOneToOneFetchWithNewMemberRefused();
    void testConversationMemberEvent();
    void testGetConversationsMembersWhileSyncing();
    void testGetConversationMembersWithSelfOneOne();
    void testAvoidTwoOneToOne();
    void testAvoidTwoOneToOneMultiDevices();
    void testRemoveRequestBannedMultiDevices();
    void testBanUnbanMultiDevice();
    void testBanUnbanGotFirstConv();
    void testBanHostWhileHosting();
    void testRemoveContactTwice();
    void testAddContactTwice();

    std::string aliceId;
    UserData aliceData;
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
    CPPUNIT_TEST_SUITE(ConversationMembersEventTest);
    ////CPPUNIT_TEST(testRemoveConversationNoMember);
    ////CPPUNIT_TEST(testRemoveConversationWithMember);
    ////CPPUNIT_TEST(testAddMember);
    ////CPPUNIT_TEST(testMemberAddedNoBadFile);
    ////CPPUNIT_TEST(testAddOfflineMemberThenConnects);
    CPPUNIT_TEST(testGetMembers);
    ////CPPUNIT_TEST(testRemoveMember);
    ////CPPUNIT_TEST(testRemovedMemberDoesNotReceiveMessage);
    ////CPPUNIT_TEST(testRemoveInvitedMember);
    CPPUNIT_TEST(testMemberBanNoBadFile);
    ////CPPUNIT_TEST(testMemberTryToRemoveAdmin);
    ////CPPUNIT_TEST(testBannedMemberCannotSendMessage);
    CPPUNIT_TEST(testAdminCanReAddMember);
    //CPPUNIT_TEST(testMemberCannotBanOther);
    //CPPUNIT_TEST(testMemberCannotUnBanOther);
    //CPPUNIT_TEST(testCheckAdminFakeAVoteIsDetected);
    //CPPUNIT_TEST(testAdminCannotKickTheirself);
    //CPPUNIT_TEST(testCommitUnauthorizedUser);
    //CPPUNIT_TEST(testMemberJoinsNoBadFile);
    //CPPUNIT_TEST(testMemberAddedNoCertificate);
    //CPPUNIT_TEST(testMemberJoinsInviteRemoved);
    //CPPUNIT_TEST(testFailAddMemberInOneToOne);
    //CPPUNIT_TEST(testOneToOneFetchWithNewMemberRefused);
    //CPPUNIT_TEST(testConversationMemberEvent);
    //CPPUNIT_TEST(testGetConversationsMembersWhileSyncing);
    //CPPUNIT_TEST(testGetConversationMembersWithSelfOneOne);
    //CPPUNIT_TEST(testAvoidTwoOneToOne);
    //CPPUNIT_TEST(testAvoidTwoOneToOneMultiDevices);
    //CPPUNIT_TEST(testRemoveRequestBannedMultiDevices);
    //CPPUNIT_TEST(testBanUnbanMultiDevice);
    //CPPUNIT_TEST(testBanUnbanGotFirstConv);
    //CPPUNIT_TEST(testBanHostWhileHosting);
    //CPPUNIT_TEST(testRemoveContactTwice);
    //CPPUNIT_TEST(testAddContactTwice);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationMembersEventTest,
                                      ConversationMembersEventTest::name());

void
ConversationMembersEventTest::setUp()
{
    connectSignals();

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
    bobData = {};
    bob2Data = {};
    carlaData = {};

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
ConversationMembersEventTest::tearDown()
{
    connectSignals();

    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    if (bob2Id.empty()) {
        wait_for_removal_of({aliceId, bobId, carlaId});
    } else {
        wait_for_removal_of({aliceId, bobId, carlaId, bob2Id});
    }
}

void
ConversationMembersEventTest::connectSignals()
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
    libjami::registerSignalHandlers(confHandlers);
}


void
ConversationMembersEventTest::generateFakeInvite(std::shared_ptr<JamiAccount> account,
                                                 const std::string& convId,
                                                 const std::string& uri)
{
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + account->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    // remove from member & add into banned without voting for the ban
    auto memberFile = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + uri;
    std::ofstream file(memberFile);
    if (file.is_open()) {
        file.close();
    }

    git_repository* repo = nullptr;
    if (git_repository_open(&repo, repoPath.c_str()) != 0)
        return;
    GitRepository rep = {std::move(repo), git_repository_free};

    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr, git_index_free};
    git_strarray array = {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_dispose(&array);

    ConversationRepository cr(account->weak(), convId);

    Json::Value json;
    json["action"] = "add";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    cr.commitMessage(Json::writeString(wbuilder, json));

    libjami::sendMessage(account->getAccountID(),
                         convId,
                         "trigger the fake history to be pulled"s,
                         "");
}

void
ConversationMembersEventTest::testRemoveConversationNoMember()
{
    connectSignals();

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    auto dataPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(fileutils::isDirectory(dataPath));

    auto conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    // Removing the conversation will erase all related files
    CPPUNIT_ASSERT(libjami::removeConversation(aliceId, convId));
    conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 0);
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(!fileutils::isDirectory(dataPath));
}

void
ConversationMembersEventTest::testRemoveConversationWithMember()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvitedFile = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvitedFile));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    bobInvitedFile = clonedPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(!fileutils::isFile(bobInvitedFile));
    // Remove conversation from alice once member confirmed

    auto bobMsgSize = bobData.messages.size();
    libjami::removeConversation(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size() && bobData.messages.rbegin()->type == "member"; }));
    std::this_thread::sleep_for(3s);
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
}

void
ConversationMembersEventTest::testAddMember()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    bobInvited = clonedPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(!fileutils::isFile(bobInvited));
    auto bobMember = clonedPath + DIR_SEPARATOR_STR + "members" + DIR_SEPARATOR_STR + bobUri
                     + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bobMember));
}

void
ConversationMembersEventTest::testMemberAddedNoBadFile()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    addFile(aliceAccount, convId, "BADFILE");
    // NOTE: Add certificate because no DHT lookup
    aliceAccount->certStore().pinCertificate(bobAccount->identity().second);
    generateFakeInvite(aliceAccount, convId, bobUri);
    // Generate conv request
    aliceAccount->sendTextMessage(bobUri,
                                  std::string(bobAccount->currentDeviceId()),
                                  {{"application/invite+json",
                                    "{\"conversationId\":\"" + convId + "\"}"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationMembersEventTest::testAddOfflineMemberThenConnects()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, carlaUri);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaData.requestReceived; }));

    libjami::acceptConversationRequest(carlaId, convId);
    cv.wait_for(lk, 30s, [&]() { return !carlaData.conversationId.empty(); });
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaId
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
}

void
ConversationMembersEventTest::testGetMembers()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start a conversation and add member
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return bobData.requestReceived; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "invited");

    libjami::acceptConversationRequest(bobId, convId);
    cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); });
    members = libjami::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return !bobData.messages.empty(); }));
    members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "member");
}

void
ConversationMembersEventTest::testRemoveMember()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Now check that alice, has the only admin, can remove bob
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 3 /* vote + ban */; }));
    auto members = libjami::getConversationMembers(aliceId, convId);
    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);
}

void
ConversationMembersEventTest::testRemovedMemberDoesNotReceiveMessage()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Now check that alice, has the only admin, can remove bob
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 3 /* vote + ban */; }));

    // Now, bob is banned so they shoud not receive any message
    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
}

void
ConversationMembersEventTest::testRemoveInvitedMember()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    // Add carla
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.deviceAnnounced; }));
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Invite Alice
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 3);

    // Now check that alice, has the only admin, can remove bob
    aliceMsgSize = aliceData.messages.size();
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));
    members = libjami::getConversationMembers(aliceId, convId);
    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);

    // Check that Carla is still able to sync
    auto carlaMsgSize = carlaData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaMsgSize + 1 == carlaData.messages.size(); }));
}

void
ConversationMembersEventTest::testMemberBanNoBadFile()
{
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
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return carlaData.requestReceived; }));
    aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));

    addFile(aliceAccount, convId, "BADFILE");
    libjami::removeConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.errorDetected; }));
}

void
ConversationMembersEventTest::testMemberTryToRemoveAdmin()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Now check that alice, has the only admin, can remove bob
    libjami::removeConversationMember(bobId, convId, aliceUri);
    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2 && aliceMsgSize + 2 != aliceData.messages.size());
}

void
ConversationMembersEventTest::testBannedMemberCannotSendMessage()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 3 == aliceData.messages.size(); }));
    auto members = libjami::getConversationMembers(aliceId, convId);

    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);

    // Now check that alice doesn't receive a message from Bob
    libjami::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 4 == aliceData.messages.size(); }));
    libjami::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testAdminCanReAddMember()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Now check that alice, has the only admin, can remove bob
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 3 == aliceData.messages.size(); }));

    auto members = libjami::getConversationMembers(aliceId, convId);

    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);

    // Then check that bobUri can be re-added
    aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
}

void
ConversationMembersEventTest::testMemberCannotBanOther()
{
    connectSignals();

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
         deviceAnnounced = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string&, const std::string&, std::map<std::string, std::string>) {
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
                    deviceAnnounced = true;
                    cv.notify_one();
                }
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaData.deviceAnnounced; }));
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    memberMessageGenerated = false;
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    // Now Carla remove Bob as a member
    errorDetected = false;
    messageBobReceived = false;
    // remove from member & add into banned without voting for the ban
    simulateRemoval(carlaAccount, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));

    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageBobReceived; }));
}

void
ConversationMembersEventTest::testMemberCannotUnBanOther()
{
    connectSignals();

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
         deviceAnnounced = false, messageCarlaReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string&, const std::string&, std::map<std::string, std::string>) {
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
                    deviceAnnounced = true;
                    cv.notify_one();
                }
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
            } else if (accountId == bobId && conversationId == convId) {
                messageBobReceived = true;
            } else if (accountId == carlaId && conversationId == convId) {
                messageCarlaReceived = true;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaData.deviceAnnounced; }));
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    memberMessageGenerated = false;
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    messageCarlaReceived = false;
    voteMessageGenerated = false;
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return memberMessageGenerated && voteMessageGenerated && messageCarlaReceived;
    }));

    memberMessageGenerated = false;
    voteMessageGenerated = false;
    libjami::addConversationMember(carlaId, convId, bobUri);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    auto members = libjami::getConversationMembers(aliceId, convId);
    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);
}

void
ConversationMembersEventTest::testCheckAdminFakeAVoteIsDetected()
{
    connectSignals();

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
         deviceAnnounced = false;
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
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    deviceAnnounced = true;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return deviceAnnounced; }));
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
    errorDetected = false;
    simulateRemoval(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationMembersEventTest::testAdminCannotKickTheirself()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, aliceMessageReceived = false;
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "text/plain") {
                aliceMessageReceived = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
    libjami::removeConversationMember(aliceId, convId, aliceUri);
    members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
}

void
ConversationMembersEventTest::testCommitUnauthorizedUser()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Wait that alice sees Bob
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageAliceReceived == 2; }));

    // Add commit from invalid user
    Json::Value root;
    root["type"] = "text/plain";
    root["body"] = "hi";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    auto message = Json::writeString(wbuilder, root);
    commitInRepo(repoPath, carlaAccount, message);

    errorDetected = false;
    libjami::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberJoinsNoBadFile()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, errorDetected = false, deviceAnnounced = false,
         memberMessageGenerated = false;
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
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
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
                    deviceAnnounced = true;
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

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return memberMessageGenerated; }));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + aliceId + DIR_SEPARATOR_STR + "conversations";
    auto repoPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + carlaId + DIR_SEPARATOR_STR + "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                       + DIR_SEPARATOR_STR + "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaId
                       + DIR_SEPARATOR_STR + "convInfo";
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Accept for alice and makes different heads
    addFile(carlaAccount, convId, "BADFILE");
    // add /members + /devices
    auto cert = carlaAccount->identity().second;
    auto parentCert = cert->issuer;
    auto uri = parentCert->getId().toString();
    std::string membersPath = fmt::format("{}/{}/members/", repoPathCarla, convId);
    std::string devicesPath = fmt::format("{}/{}/devices/", repoPathCarla, convId);
    std::string memberFile = fmt::format("{}{}.crt", membersPath, carlaUri);
    // Add members/uri.crt
    fileutils::recursive_mkdir(membersPath, 0700);
    fileutils::recursive_mkdir(devicesPath, 0700);
    auto file = fileutils::ofstream(memberFile, std::ios::trunc | std::ios::binary);
    file << parentCert->toString(true);
    file.close();
    std::string invitedPath = fmt::format("{}/{}/invited/{}", repoPathCarla, convId, carlaUri);
    fileutils::remove(invitedPath);
    std::string devicePath = fmt::format("{}{}.crt", devicesPath, carlaAccount->currentDeviceId());
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    file << cert->toString(false);
    addAll(carlaAccount, convId);
    ConversationRepository repo(carlaAccount, convId);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return deviceAnnounced; }));

    errorDetected = false;
    libjami::sendMessage(carlaId, convId, "hi"s, "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberAddedNoCertificate()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, errorDetected = false, deviceAnnounced = false,
         memberMessageGenerated = false;
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
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
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
                    deviceAnnounced = true;
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

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return memberMessageGenerated; }));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + aliceId + DIR_SEPARATOR_STR + "conversations";
    auto repoPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + carlaId + DIR_SEPARATOR_STR + "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                       + DIR_SEPARATOR_STR + "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaId
                       + DIR_SEPARATOR_STR + "convInfo";
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Remove invite but do not add member certificate
    std::string invitedPath = repoPathCarla + "invited";
    fileutils::remove(fileutils::getFullPath(invitedPath, carlaUri));

    Json::Value json;
    json["action"] = "join";
    json["uri"] = carlaUri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    ConversationRepository cr(carlaAccount->weak(), convId);
    cr.commitMessage(Json::writeString(wbuilder, json), false);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return deviceAnnounced; }));

    libjami::sendMessage(carlaId, convId, "hi"s, "");
    errorDetected = false;

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberJoinsInviteRemoved()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, errorDetected = false, deviceAnnounced = false,
         memberMessageGenerated = false;
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
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
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
                    deviceAnnounced = true;
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

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return memberMessageGenerated; }));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + aliceId + DIR_SEPARATOR_STR + "conversations";
    auto repoPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                         + carlaId + DIR_SEPARATOR_STR + "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                       + DIR_SEPARATOR_STR + "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaId
                       + DIR_SEPARATOR_STR + "convInfo";
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // add /members + /devices
    auto cert = carlaAccount->identity().second;
    auto parentCert = cert->issuer;
    auto uri = parentCert->getId().toString();
    std::string membersPath = fmt::format("{}/{}/members/", repoPathCarla, convId);
    std::string devicesPath = fmt::format("{}/{}/devices/", repoPathCarla, convId);
    std::string memberFile = fmt::format("{}{}.crt", membersPath, carlaUri);
    // Add members/uri.crt
    fileutils::recursive_mkdir(membersPath, 0700);
    fileutils::recursive_mkdir(devicesPath, 0700);
    auto file = fileutils::ofstream(memberFile, std::ios::trunc | std::ios::binary);
    file << parentCert->toString(true);
    file.close();
    std::string devicePath = fmt::format("{}{}.crt", devicesPath, carlaAccount->currentDeviceId());
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    file << cert->toString(false);
    addAll(carlaAccount, convId);
    Json::Value json;
    json["action"] = "join";
    json["uri"] = carlaUri;
    json["type"] = "member";
    commit(carlaAccount, convId, json);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return deviceAnnounced; }));

    libjami::sendMessage(carlaId, convId, "hi"s, "");
    errorDetected = false;

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return errorDetected; }));
    libjami::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testFailAddMemberInOneToOne()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !convId.empty(); }));
    memberMessageGenerated = false;
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&]() { return memberMessageGenerated; }));
}

void
ConversationMembersEventTest::testOneToOneFetchWithNewMemberRefused()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         messageBob = false, errorDetected = false;
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
            } else if (accountId == bobId && conversationId == convId
                       && message["type"] == "member") {
                messageBob = true;
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
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    errorDetected = false;
    // NOTE: Add certificate because no DHT lookup
    aliceAccount->certStore().pinCertificate(carlaAccount->identity().second);
    generateFakeInvite(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationMembersEventTest::testConversationMemberEvent()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberAddGenerated = false;
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationMemberEvent>(
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
    libjami::registerSignalHandlers(confHandlers);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberAddGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    bobInvited = clonedPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(!fileutils::isFile(bobInvited));
    auto bobMember = clonedPath + DIR_SEPARATOR_STR + "members" + DIR_SEPARATOR_STR + bobUri
                     + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bobMember));
}

void
ConversationMembersEventTest::testGetConversationsMembersWhileSyncing()
{
    connectSignals();

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

    auto members = libjami::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(std::find_if(members.begin(),
                                members.end(),
                                [&](auto memberInfo) { return memberInfo["uri"] == aliceUri; })
                   != members.end());
    CPPUNIT_ASSERT(std::find_if(members.begin(),
                                members.end(),
                                [&](auto memberInfo) { return memberInfo["uri"] == bobUri; })
                   != members.end());
}

void
ConversationMembersEventTest::testGetConversationMembersWithSelfOneOne()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string convId = "";
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                convId = conversationId;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !convId.empty(); }));

    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceUri);
}

void
ConversationMembersEventTest::testAvoidTwoOneToOne()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    std::string convId = "";
    auto conversationReadyBob = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId && conversationId == convId) {
                conversationReadyBob = true;
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
    auto conversationRmBob = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId)
                    conversationRmBob = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReadyBob && memberMessageGenerated; }));

    // Remove contact
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRmBob; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(10s);

    // Bob add Alice, this should re-add old conversation
    bobAccount->addContact(aliceUri);
    bobAccount->sendTrustRequest(aliceUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReadyBob; }));
}

void
ConversationMembersEventTest::testAvoidTwoOneToOneMultiDevices()
{
    connectSignals();

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
            } else if (accountId == bobId && conversationId == convId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id && conversationId == convId) {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationReadyBob && conversationReadyBob2 && memberMessageGenerated;
    }));

    // Remove contact
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRmBob && conversationRmBob2; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(10s);

    // Bob add Alice, this should re-add old conversation
    bobAccount->addContact(aliceUri);
    bobAccount->sendTrustRequest(aliceUri, {});
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReadyBob && conversationReadyBob2; }));
}

void
ConversationMembersEventTest::testRemoveRequestBannedMultiDevices()
{
    connectSignals();

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
    auto bob2ContactRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool banned) {
            if (accountId == bob2Id && uri == aliceUri && banned) {
                bob2ContactRemoved = true;
            }
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
    CPPUNIT_ASSERT(libjami::getConversationRequests(bob2Id).size() == 1);

    // Bob bans alice, should update bob2
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2ContactRemoved; }));
    CPPUNIT_ASSERT(libjami::getConversationRequests(bob2Id).size() == 0);
}

void
ConversationMembersEventTest::testBanUnbanMultiDevice()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

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
    auto bob2ContactRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool banned) {
            if (accountId == bob2Id && uri == aliceUri && banned) {
                bob2ContactRemoved = true;
            }
            cv.notify_one();
        }));
    auto memberMessageGenerated = false, voteMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            auto itFind = message.find("type");
            if (itFind == message.end())
                return;
            if (accountId == aliceId && conversationId == convId) {
                if (itFind->second == "member")
                    memberMessageGenerated = true;
                if (itFind->second == "vote")
                    voteMessageGenerated = true;
            }
            cv.notify_one();
        }));
    auto conversationReadyBob = false, conversationReadyBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id && conversationId == convId) {
                conversationReadyBob2 = true;
            }
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
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return memberMessageGenerated && requestReceived && requestReceivedBob2;
    }));

    // Alice kick Bob while invited
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    // Alice re-add Bob while invited
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    // bob accepts
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReadyBob && conversationReadyBob2; }));
}

void
ConversationMembersEventTest::testBanUnbanGotFirstConv()
{
    connectSignals();

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
    std::string convId;
    auto conversationReadyBob = false, conversationReadyBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId && conversationId == convId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id && conversationId == convId) {
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
    auto bob2ContactRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool banned) {
            if (accountId == bob2Id && uri == aliceUri && banned) {
                bob2ContactRemoved = true;
            }
            cv.notify_one();
        }));
    auto bobMsgReceived = false, bob2MsgReceived = false, memberMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId) {
                auto itFind = message.find("type");
                if (itFind != message.end() && itFind->second == "member")
                    memberMessageGenerated = true;
            } else if (accountId == bobId && conversationId == convId)
                bobMsgReceived = true;
            else if (accountId == bob2Id && conversationId == convId)
                bob2MsgReceived = true;
            cv.notify_one();
        }));
    auto contactAddedBob = false, contactAddedBob2 = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri) {
                contactAddedBob = true;
            } else if (accountId == bob2Id && uri == aliceUri) {
                contactAddedBob2 = true;
            }
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
    CPPUNIT_ASSERT(libjami::getConversationRequests(bob2Id).size() == 1);

    // Accepts requests
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationReadyBob && conversationReadyBob2 && memberMessageGenerated;
    }));

    // Bob bans alice, should update bob2
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2ContactRemoved; }));

    // Alice sends messages, bob & bob2 should not get it!
    bobMsgReceived = false, bob2MsgReceived = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return bobMsgReceived && bob2MsgReceived; }));

    // Bobs re-add Alice
    contactAddedBob = false, contactAddedBob2 = false;
    bobAccount->addContact(aliceUri);
    bobAccount->sendTrustRequest(aliceUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactAddedBob && contactAddedBob2; }));

    // Alice can sends some messages now
    bobMsgReceived = false, bob2MsgReceived = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgReceived && bob2MsgReceived; }));
}

void
ConversationMembersEventTest::testBanHostWhileHosting()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
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
    bool memberMessageGenerated = false, callMessageGenerated = false, voteMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
                cv.notify_one();
            } else if (accountId == aliceId && conversationId == convId) {
                if (message["type"] == "application/call-history+json") {
                    callMessageGenerated = true;
                } else if (message["type"] == "member") {
                    memberMessageGenerated = true;
                }
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Now, Bob starts a call
    auto callId = libjami::placeCallWithMedia(bobId, "swarm:" + convId, {});
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return callMessageGenerated; }));

    // get active calls = 1
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, convId).size() == 1);

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    auto members = libjami::getConversationMembers(aliceId, convId);
    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);

    libjami::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testRemoveContactTwice()
{
    connectSignals();

    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string&,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    std::string convId = ""; auto conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                convId = conversationId;
            else if (accountId == bobId)
                conversationReady = true;
            cv.notify_one();
        }));
    auto conversationRemoved = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    auto contactRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri)
                contactRemoved = true;
            cv.notify_one();
        }));
    auto memberMessageGenerated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && message["type"] == "member")
                memberMessageGenerated = true;
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
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactRemoved; }));
    // wait that connections are closed.
    std::this_thread::sleep_for(10s);
    // re-add via a new message. Trigger a new request
    requestReceived = false;
    libjami::sendMessage(aliceId, convId, "foo"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    // removeContact again (should remove the trust request/conversation)
    contactRemoved = false;
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactRemoved; }));
}

void
ConversationMembersEventTest::testAddContactTwice()
{
    connectSignals();

    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string&,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    std::string convId = "";
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                convId = conversationId;
            cv.notify_one();
        }));
    auto requestDeclined = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestDeclined>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId)
                    requestDeclined = true;
                cv.notify_one();
            }));
    auto contactRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == aliceId && uri == bobUri)
                contactRemoved = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return requestReceived; }));
    requestReceived = false;
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return contactRemoved; }));
    // wait that connections are closed.
    std::this_thread::sleep_for(10s);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return requestDeclined && requestReceived; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationMembersEventTest::name())
