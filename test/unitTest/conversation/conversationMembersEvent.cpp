/*
 *  Copyright (C) 2017-2022 Savoir-faire Linux Inc.
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
using namespace DRing::Account;

namespace jami {
namespace test {

class ConversationMembersEventTest : public CppUnit::TestFixture
{
public:
    ~ConversationMembersEventTest() { DRing::fini(); }
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

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;

private:
    CPPUNIT_TEST_SUITE(ConversationMembersEventTest);
    CPPUNIT_TEST(testRemoveConversationNoMember);
    CPPUNIT_TEST(testRemoveConversationWithMember);
    CPPUNIT_TEST(testAddMember);
    CPPUNIT_TEST(testMemberAddedNoBadFile);
    CPPUNIT_TEST(testAddOfflineMemberThenConnects);
    CPPUNIT_TEST(testGetMembers);
    CPPUNIT_TEST(testRemoveMember);
    CPPUNIT_TEST(testRemovedMemberDoesNotReceiveMessage);
    CPPUNIT_TEST(testRemoveInvitedMember);
    CPPUNIT_TEST(testMemberBanNoBadFile);
    CPPUNIT_TEST(testMemberTryToRemoveAdmin);
    CPPUNIT_TEST(testBannedMemberCannotSendMessage);
    CPPUNIT_TEST(testAdminCanReAddMember);
    CPPUNIT_TEST(testMemberCannotBanOther);
    CPPUNIT_TEST(testMemberCannotUnBanOther);
    CPPUNIT_TEST(testCheckAdminFakeAVoteIsDetected);
    CPPUNIT_TEST(testAdminCannotKickTheirself);
    CPPUNIT_TEST(testCommitUnauthorizedUser);
    CPPUNIT_TEST(testMemberJoinsNoBadFile);
    CPPUNIT_TEST(testMemberAddedNoCertificate);
    CPPUNIT_TEST(testMemberJoinsInviteRemoved);
    CPPUNIT_TEST(testFailAddMemberInOneToOne);
    CPPUNIT_TEST(testOneToOneFetchWithNewMemberRefused);
    CPPUNIT_TEST(testConversationMemberEvent);
    CPPUNIT_TEST(testGetConversationsMembersWhileSyncing);
    CPPUNIT_TEST(testGetConversationMembersWithSelfOneOne);
    CPPUNIT_TEST(testAvoidTwoOneToOne);
    CPPUNIT_TEST(testAvoidTwoOneToOneMultiDevices);
    CPPUNIT_TEST(testRemoveRequestBannedMultiDevices);
    CPPUNIT_TEST(testBanUnbanMultiDevice);
    CPPUNIT_TEST(testBanUnbanGotFirstConv);
    CPPUNIT_TEST(testBanHostWhileHosting);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationMembersEventTest,
                                      ConversationMembersEventTest::name());

void
ConversationMembersEventTest::setUp()
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
ConversationMembersEventTest::tearDown()
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

    DRing::sendMessage(account->getAccountID(),
                       convId,
                       "trigger the fake history to be pulled"s,
                       "");
}

void
ConversationMembersEventTest::testRemoveConversationNoMember()
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    auto dataPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversation_data" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(fileutils::isDirectory(dataPath));

    auto conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    // Removing the conversation will erase all related files
    CPPUNIT_ASSERT(DRing::removeConversation(aliceId, convId));
    conversations = DRing::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 0);
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(!fileutils::isDirectory(dataPath));
}

void
ConversationMembersEventTest::testRemoveConversationWithMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         bobSeeAliceRemoved = false;
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
            auto itFind = message.find("type");
            if (itFind == message.end())
                return;
            if (accountId == aliceId && conversationId == convId && itFind->second == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            } else if (accountId == bobId && conversationId == convId
                       && itFind->second == "member") {
                bobSeeAliceRemoved = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvitedFile = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvitedFile));

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    bobInvitedFile = clonedPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(!fileutils::isFile(bobInvitedFile));
    // Remove conversation from alice once member confirmed
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    bobSeeAliceRemoved = false;
    DRing::removeConversation(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobSeeAliceRemoved; }));
    std::this_thread::sleep_for(3s);
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
}

void
ConversationMembersEventTest::testAddMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
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
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, errorDetected = false;
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
            if (accountId == bobId && conversationId == convId && code == 1)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    addFile(aliceAccount, convId, "BADFILE");
    generateFakeInvite(aliceAccount, convId, bobUri);
    // Generate conv request
    aliceAccount->sendTextMessage(bobUri,
                                  {{"application/invite+json",
                                    "{\"conversationId\":\"" + convId + "\"}"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    errorDetected = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testAddOfflineMemberThenConnects()
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
    bool conversationReady = false, requestReceived = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, carlaUri);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return requestReceived; }));

    DRing::acceptConversationRequest(carlaId, convId);
    cv.wait_for(lk, 30s, [&]() { return conversationReady; });
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + carlaAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testGetMembers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto messageReceived = false;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId) {
                messageReceived = true;
                cv.notify_one();
            }
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
    // Start a conversation and add member
    auto convId = DRing::startConversation(aliceId);

    messageReceived = false;
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return messageReceived; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "invited");

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    messageReceived = false;
    DRing::acceptConversationRequest(bobId, convId);
    cv.wait_for(lk, 30s, [&]() { return conversationReady; });
    members = DRing::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return messageReceived; }));
    members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "member");
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testRemoveMember()
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
         voteMessageGenerated = false;
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
                cv.notify_one();
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testRemovedMemberDoesNotReceiveMessage()
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
    bool conversationReady = false, requestReceived = false;
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
    bool memberMessageGenerated = false, voteMessageGenerated = false, messageBobReceived = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote")
                voteMessageGenerated = true;
            else if (accountId == aliceId && conversationId == convId && message["type"] == "member")
                memberMessageGenerated = true;
            else if (accountId == bobId && conversationId == convId)
                messageBobReceived = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    // Now, bob is banned so they shoud not receive any message
    messageBobReceived = false;
    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return messageBobReceived; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testRemoveInvitedMember()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool voteMessageGenerated = false, memberMessageGenerated = false, carlaMessageReceived;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote")
                voteMessageGenerated = true;
            else if (accountId == aliceId && conversationId == convId && message["type"] == "member")
                memberMessageGenerated = true;
            else if (accountId == carlaId && message["type"] == "text/plain"
                     && message["body"] == "hi")
                carlaMessageReceived = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Add carla
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaConnected; }));
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Invite Alice
    requestReceived = false;
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 3);

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);

    // Check that Carla is still able to sync
    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaMessageReceived; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberBanNoBadFile()
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
        [&](const std::string& accountId,
            const std::string& conversationId,
            int code,
            const std::string& /* what */) {
            if (accountId == bobId && conversationId == convId && code == 3)
                errorDetected = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaConnected; }));
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    memberMessageGenerated = false;
    voteMessageGenerated = false;
    addFile(aliceAccount, convId, "BADFILE");
    DRing::removeConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberTryToRemoveAdmin()
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
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "member")
                memberMessageGenerated = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    DRing::removeConversationMember(bobId, convId, aliceUri);
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2 && !memberMessageGenerated);
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testBannedMemberCannotSendMessage()
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
         voteMessageGenerated = false, aliceMessageReceived = false;
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
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);

    // Now check that alice doesn't receive a message from Bob
    aliceMessageReceived = false;
    DRing::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return aliceMessageReceived; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testAdminCanReAddMember()
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
         voteMessageGenerated = false;
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
            if (accountId == aliceId && conversationId == convId && message["type"] == "vote") {
                voteMessageGenerated = true;
                cv.notify_one();
            } else if (accountId == aliceId && conversationId == convId
                       && message["type"] == "member") {
                memberMessageGenerated = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);

    // Then check that bobUri can be re-added
    memberMessageGenerated = false, voteMessageGenerated = false;
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberCannotBanOther()
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
            [&](const std::string&, const std::string&, std::map<std::string, std::string>) {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaConnected; }));
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    memberMessageGenerated = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    // Now Carla remove Bob as a member
    errorDetected = false;
    messageBobReceived = false;
    // remove from member & add into banned without voting for the ban
    simulateRemoval(carlaAccount, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));

    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageBobReceived; }));
}

void
ConversationMembersEventTest::testMemberCannotUnBanOther()
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
         carlaConnected = false, messageCarlaReceived = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string&, const std::string&, std::map<std::string, std::string>) {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaConnected; }));
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    memberMessageGenerated = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && messageBobReceived; }));

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    messageCarlaReceived = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return memberMessageGenerated && voteMessageGenerated && messageCarlaReceived;
    }));

    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::addConversationMember(carlaId, convId, bobUri);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
}

void
ConversationMembersEventTest::testCheckAdminFakeAVoteIsDetected()
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaConnected; }));
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    requestReceived = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return requestReceived && memberMessageGenerated; }));
    memberMessageGenerated = false;
    messageBobReceived = false;
    DRing::acceptConversationRequest(carlaId, convId);
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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         voteMessageGenerated = false, aliceMessageReceived = false;
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
    DRing::registerSignalHandlers(confHandlers);
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
    DRing::removeConversationMember(aliceId, convId, aliceUri);
    members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
}

void
ConversationMembersEventTest::testCommitUnauthorizedUser()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
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
    DRing::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberJoinsNoBadFile()
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
    bool conversationReady = false, errorDetected = false, carlaConnected = false,
         memberMessageGenerated = false;
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
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
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

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return memberMessageGenerated; }));

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
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Accept for alice and makes different heads
    addFile(carlaAccount, convId, "BADFILE");
    ConversationRepository repo(carlaAccount, convId);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));

    errorDetected = false;
    DRing::sendMessage(carlaId, convId, "hi"s, "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberAddedNoCertificate()
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
    bool conversationReady = false, errorDetected = false, carlaConnected = false,
         memberMessageGenerated = false;
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
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
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

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return memberMessageGenerated; }));

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
    cr.commitMessage(Json::writeString(wbuilder, json));

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));

    DRing::sendMessage(carlaId, convId, "hi"s, "");
    errorDetected = false;

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testMemberJoinsInviteRemoved()
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
    bool conversationReady = false, errorDetected = false, carlaConnected = false,
         memberMessageGenerated = false;
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
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
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

    aliceAccount->convModule()->addConversationMember(convId, carlaUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return memberMessageGenerated; }));

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
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Let invited, but add /members + /devices
    auto cert = carlaAccount->identity().second;
    auto parentCert = cert->issuer;
    auto uri = parentCert->getId().toString();
    std::string membersPath = repoPathCarla + "members" + DIR_SEPARATOR_STR;
    std::string memberFile = membersPath + DIR_SEPARATOR_STR + carlaUri + ".crt";
    // Add members/uri.crt
    fileutils::recursive_mkdir(membersPath, 0700);
    auto file = fileutils::ofstream(memberFile, std::ios::trunc | std::ios::binary);
    file << parentCert->toString(true);
    file.close();

    addAll(carlaAccount, convId);
    Json::Value json;
    json["action"] = "join";
    json["uri"] = carlaUri;
    json["type"] = "member";
    commit(carlaAccount, convId, json);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));

    DRing::sendMessage(carlaId, convId, "hi"s, "");
    errorDetected = false;

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return errorDetected; }));
    DRing::unregisterSignalHandlers();
}

void
ConversationMembersEventTest::testFailAddMemberInOneToOne()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
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
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !convId.empty(); }));
    memberMessageGenerated = false;
    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&]() { return memberMessageGenerated; }));
}

void
ConversationMembersEventTest::testOneToOneFetchWithNewMemberRefused()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         messageBob = false, errorDetected = false;
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
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
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    errorDetected = false;
    generateFakeInvite(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return errorDetected; }));
}

void
ConversationMembersEventTest::testConversationMemberEvent()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberAddGenerated = false;
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
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberAddGenerated; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    // Check created files
    auto bobInvited = repoPath + DIR_SEPARATOR_STR + "invited" + DIR_SEPARATOR_STR + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(bobInvited));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));

    auto members = DRing::getConversationMembers(bobId, convId);
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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::string convId = "";
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                convId = conversationId;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !convId.empty(); }));

    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceUri);
}

void
ConversationMembersEventTest::testAvoidTwoOneToOne()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

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
                if (accountId == bobId)
                    requestReceived = true;
                cv.notify_one();
            }));
    std::string convId = "";
    auto conversationReadyBob = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId && conversationId == convId) {
                conversationReadyBob = true;
            }
            cv.notify_one();
        }));
    auto memberMessageGenerated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == bobId)
                conversationRmBob = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    DRing::acceptConversationRequest(bobId, convId);
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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
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
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        DRing::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    auto conversationRmBob = false, conversationRmBob2 = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == bobId)
                conversationRmBob = true;
            else if (accountId == bob2Id)
                conversationRmBob2 = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Bob creates a second device
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));

    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));
    DRing::acceptConversationRequest(bobId, convId);
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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
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
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        DRing::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    auto bob2ContactRemoved = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool banned) {
            if (accountId == bob2Id && uri == aliceUri && banned) {
                bob2ContactRemoved = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Bob creates a second device
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));

    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));
    CPPUNIT_ASSERT(DRing::getConversationRequests(bob2Id).size() == 1);

    // Bob bans alice, should update bob2
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2ContactRemoved; }));
    CPPUNIT_ASSERT(DRing::getConversationRequests(bob2Id).size() == 0);
}

void
ConversationMembersEventTest::testBanUnbanMultiDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
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
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        DRing::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    auto bob2ContactRemoved = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool banned) {
            if (accountId == bob2Id && uri == aliceUri && banned) {
                bob2ContactRemoved = true;
            }
            cv.notify_one();
        }));
    auto memberMessageGenerated = false, voteMessageGenerated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == bobId && conversationId == convId) {
                conversationReadyBob = true;
            } else if (accountId == bob2Id && conversationId == convId) {
                conversationReadyBob2 = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Bob creates a second device
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));

    // Alice adds bob
    requestReceived = false;
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return memberMessageGenerated && requestReceived && requestReceivedBob2;
    }));

    // Alice kick Bob while invited
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    // Alice re-add Bob while invited
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));

    // bob accepts
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReadyBob && conversationReadyBob2; }));
}

void
ConversationMembersEventTest::testBanUnbanGotFirstConv()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto requestReceived = false, requestReceivedBob2 = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
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
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId == bob2Id) {
                    auto daemonStatus = details.at(
                        DRing::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        bob2Started = true;
                }
                cv.notify_one();
            }));
    auto bob2ContactRemoved = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool banned) {
            if (accountId == bob2Id && uri == aliceUri && banned) {
                bob2ContactRemoved = true;
            }
            cv.notify_one();
        }));
    auto bobMsgReceived = false, bob2MsgReceived = false, memberMessageGenerated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri) {
                contactAddedBob = true;
            } else if (accountId == bob2Id && uri == aliceUri) {
                contactAddedBob2 = true;
            }
            cv.notify_one();
        }));

    DRing::registerSignalHandlers(confHandlers);

    // Bob creates a second device
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Started; }));

    // Alice adds bob
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived && requestReceivedBob2; }));
    CPPUNIT_ASSERT(DRing::getConversationRequests(bob2Id).size() == 1);

    // Accepts requests
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationReadyBob && conversationReadyBob2 && memberMessageGenerated;
    }));

    // Bob bans alice, should update bob2
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2ContactRemoved; }));

    // Alice sends messages, bob & bob2 should not get it!
    bobMsgReceived = false, bob2MsgReceived = false;
    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return bobMsgReceived && bob2MsgReceived; }));

    // Bobs re-add Alice
    contactAddedBob = false, contactAddedBob2 = false;
    bobAccount->addContact(aliceUri);
    bobAccount->sendTrustRequest(aliceUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactAddedBob && contactAddedBob2; }));

    // Alice can sends some messages now
    bobMsgReceived = false, bob2MsgReceived = false;
    DRing::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgReceived && bob2MsgReceived; }));
}

void
ConversationMembersEventTest::testBanHostWhileHosting()
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
    bool conversationReady = false, requestReceived = false;
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
    bool memberMessageGenerated = false, callMessageGenerated = false,
         voteMessageGenerated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
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
    DRing::registerSignalHandlers(confHandlers);
    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    // Now, Bob starts a call
    auto callId = DRing::placeCallWithMedia(bobId, "swarm:" + convId, {});
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return callMessageGenerated; }));

    // get active calls = 1
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, convId).size() == 1);

    // Now check that alice, has the only admin, can remove bob
    memberMessageGenerated = false;
    voteMessageGenerated = false;
    DRing::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated && voteMessageGenerated; }));
    auto members = DRing::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 1);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, convId).size() == 0);

    DRing::unregisterSignalHandlers();
}
} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationMembersEventTest::name())
