/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
    bool requestRemoved {false};
    bool errorDetected {false};
    bool registered {false};
    bool stopped {false};
    bool deviceAnnounced {false};
    bool contactRemoved {false};
    bool contactAdded {false};
    Conversation::BootstrapStatus bootstrap {Conversation::BootstrapStatus::FAILED};
    std::string payloadTrustRequest;
    std::vector<libjami::SwarmMessage> messages;
    std::vector<libjami::SwarmMessage> messagesUpdated;
    std::map<std::string, int> members;
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

    void testAddInvalidUri();
    void testRemoveConversationNoMember();
    void testRemoveConversationWithMember();
    void testAddMember();
    void testMemberAddedNoBadFile();
    void testAddOfflineMemberThenConnects();
    void testAddAcceptOfflineThenConnects();
    void testGetMembers();
    void testRemoveMember();
    void testRemovedMemberDoesNotReceiveMessageFromAdmin();
    void testRemovedMemberDoesNotReceiveMessageFromPeer();
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
    void testAddContactTwice();
    void testBanFromNewDevice();

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
    CPPUNIT_TEST(testAddInvalidUri);
    CPPUNIT_TEST(testRemoveConversationNoMember);
    CPPUNIT_TEST(testRemoveConversationWithMember);
    CPPUNIT_TEST(testAddMember);
    CPPUNIT_TEST(testMemberAddedNoBadFile);
    CPPUNIT_TEST(testAddOfflineMemberThenConnects);
    CPPUNIT_TEST(testAddAcceptOfflineThenConnects);
    CPPUNIT_TEST(testGetMembers);
    CPPUNIT_TEST(testRemoveMember);
    CPPUNIT_TEST(testRemovedMemberDoesNotReceiveMessageFromAdmin);
    CPPUNIT_TEST(testRemovedMemberDoesNotReceiveMessageFromPeer);
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
    CPPUNIT_TEST(testAddContactTwice);
    CPPUNIT_TEST(testBanFromNewDevice);
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationMemberEvent>(
        [&](const std::string& accountId, const std::string& conversationId, const auto& member, auto status) {
            if (accountId == aliceId) {
                aliceData.members[member] = status;
            } else if (accountId == bobId) {
                bobData.members[member] = status;
            } else if (accountId == bob2Id) {
                bob2Data.members[member] = status;
            } else if (accountId == carlaId) {
                carlaData.members[member] = status;
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
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestDeclined>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId) {
                    bobData.requestRemoved = true;
                } else if (accountId == bob2Id) {
                    bob2Data.requestRemoved = true;
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string&, bool) {
            if (accountId == bobId) {
                bobData.contactRemoved = true;
            } else if (accountId == bob2Id) {
                bob2Data.contactRemoved = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string&, bool) {
            if (accountId == aliceId) {
                aliceData.contactAdded = true;
            } else if (accountId == bobId) {
                bobData.contactAdded = true;
            } else if (accountId == bob2Id) {
                bob2Data.contactAdded = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

void
ConversationMembersEventTest::generateFakeInvite(std::shared_ptr<JamiAccount> account,
                                                 const std::string& convId,
                                                 const std::string& uri)
{
    auto repoPath = fileutils::get_data_dir() / account->getAccountID()
                    / "conversations" / convId;
    // remove from member & add into banned without voting for the ban
    auto memberFile = repoPath / "invited" / uri;
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

    ConversationRepository cr(account, convId);

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
ConversationMembersEventTest::testAddInvalidUri()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    aliceAccount->addContact("Shitty/Uri");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&]() { return aliceData.contactAdded; }));
    CPPUNIT_ASSERT(aliceData.conversationId.empty());
}

void
ConversationMembersEventTest::testRemoveConversationNoMember()
{
    connectSignals();

    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() / aliceId
                    / "conversations" / convId;
    auto dataPath = fileutils::get_data_dir() / aliceId
                    / "conversation_data" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    CPPUNIT_ASSERT(std::filesystem::is_directory(dataPath));

    auto conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 1);
    // Removing the conversation will erase all related files
    CPPUNIT_ASSERT(libjami::removeConversation(aliceId, convId));
    conversations = libjami::getConversations(aliceId);
    CPPUNIT_ASSERT(conversations.size() == 0);
    CPPUNIT_ASSERT(!std::filesystem::is_directory(repoPath));
    CPPUNIT_ASSERT(!std::filesystem::is_directory(dataPath));
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
    auto repoPath = fileutils::get_data_dir() / aliceId
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvitedFile = repoPath / "invited" / bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvitedFile));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    auto clonedPath = fileutils::get_data_dir() / bobId
                      / "conversations" / convId;
    bobInvitedFile = clonedPath / "invited" / bobUri;
    CPPUNIT_ASSERT(!std::filesystem::is_regular_file(bobInvitedFile));
    // Remove conversation from alice once member confirmed

    auto bobMsgSize = bobData.messages.size();
    libjami::removeConversation(aliceId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size() && bobData.messages.rbegin()->type == "member"; }));
    std::this_thread::sleep_for(3s);
    CPPUNIT_ASSERT(!std::filesystem::is_directory(repoPath));
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
    auto repoPath = fileutils::get_data_dir() / aliceId
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath / "invited" / bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    auto clonedPath = fileutils::get_data_dir() / bobId
                      / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(clonedPath));
    bobInvited = clonedPath / "invited" / bobUri;
    CPPUNIT_ASSERT(!std::filesystem::is_regular_file(bobInvited));
    auto bobMember = clonedPath / "members" / (bobUri + ".crt");
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobMember));
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return !aliceData.conversationId.empty(); }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&] { return carlaData.requestReceived; }));

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaData.requestReceived; }));

    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !carlaData.conversationId.empty(); }));
    auto clonedPath = fileutils::get_data_dir() / carlaId
                      / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(clonedPath));
}

void
ConversationMembersEventTest::testAddAcceptOfflineThenConnects()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return !aliceData.conversationId.empty(); }));

    libjami::addConversationMember(aliceId, aliceData.conversationId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return bobData.requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return aliceData.stopped; }));

    // Accepts
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);

    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    Manager::instance().sendRegister(aliceId, true); // This avoid to sync immediately
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return !bobData.conversationId.empty(); }));
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
    auto repoPath = fileutils::get_data_dir() / aliceId
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(members[0]["uri"] == aliceAccount->getUsername());
    CPPUNIT_ASSERT(members[0]["role"] == "admin");
    CPPUNIT_ASSERT(members[1]["uri"] == bobUri);
    CPPUNIT_ASSERT(members[1]["role"] == "invited");

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    members = libjami::getConversationMembers(bobId, convId);
    CPPUNIT_ASSERT(members.size() == 2);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
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
ConversationMembersEventTest::testRemovedMemberDoesNotReceiveMessageFromAdmin()
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

    // Now check that alice, as the only admin, can remove bob
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 3 /* vote + ban */; }));

    // Now, bob is banned so they shoud not receive any message
    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
}

void
ConversationMembersEventTest::testRemovedMemberDoesNotReceiveMessageFromPeer()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.deviceAnnounced; }));

    // Make Carla send a contact request to Bob and wait until she receives a message from
    // him confirming that he has accepted the request. The point of this is to make sure
    // that Bob and Carla are connected; otherwise the test below where we check that Bob
    // hasn't received a message from Carla could pass for the wrong reason.
    carlaAccount->addContact(bobUri);
    carlaAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto carlaMsgSize = carlaData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(carlaUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaMsgSize + 1 == carlaData.messages.size(); }));

    // Alice creates a swarm and adds Bob and Carla to it
    bobData.requestReceived = false;
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && carlaData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId);
    libjami::acceptConversationRequest(carlaId, convId);
    auto messageReceived = [](UserData& userData, const std::string& action, const std::string& uri) {
        for (auto& message : userData.messages) {
            if (message.type == "member" && message.body["action"] == action && message.body["uri"] == uri)
                return true;
        }
        return false;
    };
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived(aliceData, "join", bobUri); }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived(carlaData, "join", bobUri); }));

    // Alice bans Bob
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived(aliceData, "ban", bobUri); }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageReceived(carlaData, "ban", bobUri); }));

    // Carla's messages should now be received by Alice, but not Bob
    auto aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(carlaId, convId, "hello"s, "");
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobMsgSize < bobData.messages.size(); }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
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

    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.requestReceived && aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !carlaData.conversationId.empty() && aliceMsgSize + 2 == aliceData.messages.size(); }));

    // Invite Alice
    auto carlaMsgSize = carlaData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 3 == aliceData.messages.size()
                                                    && carlaMsgSize + 1 == carlaData.messages.size()
                                                    && bobData.requestReceived; }));
    auto members = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(members.size() == 3);
    members = libjami::getConversationMembers(carlaId, convId);
    CPPUNIT_ASSERT(members.size() == 3);

    // Now check that alice, has the only admin, can remove bob
    aliceMsgSize = aliceData.messages.size();
    carlaMsgSize = carlaData.messages.size();
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size() && carlaMsgSize + 2 == carlaData.messages.size(); }));
    members = libjami::getConversationMembers(aliceId, convId);
    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);
    members = libjami::getConversationMembers(carlaId, convId);
    bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);

    // Check that Carla is still able to sync
    carlaMsgSize = carlaData.messages.size();
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
    auto bobMsgSize = bobData.messages.size();
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));

    addFile(aliceAccount, convId, "BADFILE");
    libjami::removeConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
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
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && aliceData.members[bobUri] == 1; }));
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() {
            return aliceData.members.find(carlaUri) != aliceData.members.end() && bobData.members.find(carlaUri) != bobData.members.end() && carlaData.requestReceived; }));
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return !carlaData.conversationId.empty() && aliceData.members[carlaUri] == 1 && bobData.members[carlaUri] == 1; }));

    // Now Carla remove Bob as a member
    // remove from member & add into banned without voting for the ban
    simulateRemoval(carlaAccount, convId, bobUri);
    // Note: it may be possible that alice doesn't get the error if they got messages from bob (and bob rejects due to an error)
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData.errorDetected || bobData.errorDetected; }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
}

void
ConversationMembersEventTest::testMemberCannotUnBanOther()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
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
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return !carlaData.conversationId.empty()
                                        && aliceData.members[carlaUri] == 1
                                        && bobData.members[carlaUri] == 1; }));
    std::this_thread::sleep_for(3s); // Wait that carla finish the clone
    // Now check that alice, has the only admin, can remove bob
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.members[bobUri] == 3
                                        && carlaData.members[bobUri] == 3; }));

    libjami::addConversationMember(carlaId, convId, bobUri);
    CPPUNIT_ASSERT(
        !cv.wait_for(lk, 10s, [&]() { return aliceData.members[bobUri] == 1
                                        && carlaData.members[bobUri] == 1; }));
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
    auto bobMsgSize = bobData.messages.size();
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));

    // Now Alice remove Carla without a vote. Bob will not receive the message
    simulateRemoval(aliceAccount, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationMembersEventTest::testAdminCannotKickTheirself()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
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

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size();; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() / bobId
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Add commit from invalid user
    Json::Value root;
    root["type"] = "text/plain";
    root["body"] = "hi";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    auto message = Json::writeString(wbuilder, root);
    commitInRepo(repoPath, carlaAccount, message);

    libjami::sendMessage(bobId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.errorDetected; }));
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

    auto aliceMsgSize = aliceData.messages.size();
    aliceAccount->convModule()->addConversationMember(convId, dht::InfoHash(carlaUri), false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() / aliceId / "conversations";
    auto repoPathCarla = fileutils::get_data_dir() / carlaId / "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() / aliceId
                       / "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() / carlaId
                       / "convInfo";
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Accept for alice and makes different heads
    addFile(carlaAccount, convId, "BADFILE");
    // add /members + /devices
    auto cert = carlaAccount->identity().second;
    auto parentCert = cert->issuer;
    auto uri = parentCert->getId().toString();
    auto membersPath = repoPathCarla / convId / "members";
    auto devicesPath = repoPathCarla / convId / "devices";
    auto memberFile = membersPath / fmt::format("{}.crt", carlaUri);
    // Add members/uri.crt
    dhtnet::fileutils::recursive_mkdir(membersPath, 0700);
    dhtnet::fileutils::recursive_mkdir(devicesPath, 0700);
    std::ofstream file(memberFile, std::ios::trunc | std::ios::binary);
    file << parentCert->toString(true);
    file.close();
    auto invitedPath = repoPathCarla / convId / "invited" / carlaUri;
    dhtnet::fileutils::remove(invitedPath);
    auto devicePath = devicesPath / fmt::format("{}.crt", carlaAccount->currentDeviceId());
    file = std::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    file << cert->toString(false);
    addAll(carlaAccount, convId);
    ConversationRepository repo(carlaAccount, convId);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaData.deviceAnnounced; }));

    libjami::sendMessage(carlaId, convId, "hi"s, "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return aliceData.errorDetected; }));
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

    auto aliceMsgSize = aliceData.messages.size();
    aliceAccount->convModule()->addConversationMember(convId, dht::InfoHash(carlaUri), false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() / aliceId / "conversations";
    auto repoPathCarla = fileutils::get_data_dir() / carlaId / "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() / aliceId
                       / "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() / carlaId
                       / "convInfo";
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // Remove invite but do not add member certificate
    std::string invitedPath = repoPathCarla / "invited";
    dhtnet::fileutils::remove(fileutils::getFullPath(invitedPath, carlaUri));

    Json::Value json;
    json["action"] = "join";
    json["uri"] = carlaUri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    ConversationRepository cr(carlaAccount, convId);
    cr.commitMessage(Json::writeString(wbuilder, json), false);

    // Start Carla, should merge and all messages should be there
    carlaAccount->convModule()->loadConversations(); // Because of the copy
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaData.deviceAnnounced; }));

    libjami::sendMessage(carlaId, convId, "hi"s, "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return aliceData.errorDetected; }));
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

    auto aliceMsgSize = aliceData.messages.size();
    aliceAccount->convModule()->addConversationMember(convId, dht::InfoHash(carlaUri), false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Cp conversations & convInfo
    auto repoPathAlice = fileutils::get_data_dir() / aliceId / "conversations";
    auto repoPathCarla = fileutils::get_data_dir() / carlaId / "conversations";
    std::filesystem::copy(repoPathAlice, repoPathCarla, std::filesystem::copy_options::recursive);
    auto ciPathAlice = fileutils::get_data_dir() / aliceId / "convInfo";
    auto ciPathCarla = fileutils::get_data_dir() / carlaId / "convInfo";
    std::remove(ciPathCarla.c_str());
    std::filesystem::copy(ciPathAlice, ciPathCarla);

    // add /members + /devices
    auto cert = carlaAccount->identity().second;
    auto parentCert = cert->issuer;
    auto uri = parentCert->getId().toString();
    auto membersPath = repoPathCarla / convId / "members";
    auto devicesPath = repoPathCarla / convId / "devices";
    auto memberFile = membersPath / fmt::format("{}.crt", carlaUri);
    // Add members/uri.crt
    dhtnet::fileutils::recursive_mkdir(membersPath, 0700);
    dhtnet::fileutils::recursive_mkdir(devicesPath, 0700);
    std::ofstream file(memberFile, std::ios::trunc | std::ios::binary);
    file << parentCert->toString(true);
    file.close();
    auto devicePath = devicesPath / fmt::format("{}.crt", carlaAccount->currentDeviceId());
    file = std::ofstream(devicePath, std::ios::trunc | std::ios::binary);
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaData.deviceAnnounced; }));

    libjami::sendMessage(carlaId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return aliceData.errorDetected; }));
}

void
ConversationMembersEventTest::testFailAddMemberInOneToOne()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, aliceData.conversationId, carlaUri);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
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

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size();; }));

    // NOTE: Add certificate because no DHT lookup
    aliceAccount->certStore().pinCertificate(carlaAccount->identity().second);
    generateFakeInvite(aliceAccount, aliceData.conversationId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.errorDetected; }));
}

void
ConversationMembersEventTest::testConversationMemberEvent()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() / aliceId
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Check created files
    auto bobInvited = repoPath / "invited" / bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobInvited));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    auto clonedPath = fileutils::get_data_dir() / bobId
                      / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(clonedPath));
    bobInvited = clonedPath / "invited" / bobUri;
    CPPUNIT_ASSERT(!std::filesystem::is_regular_file(bobInvited));
    auto bobMember = clonedPath / "members" / (bobUri + ".crt");
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobMember));
}

void
ConversationMembersEventTest::testGetConversationsMembersWhileSyncing()
{
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

    auto members = libjami::getConversationMembers(bobId, aliceData.conversationId);
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

    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 1; }));

    // Remove contact
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.removed; }));

    // wait that connections are closed.
    std::this_thread::sleep_for(10s);

    // Bob add Alice, this should re-add old conversation
    bobAccount->addContact(aliceUri);
    bobAccount->sendTrustRequest(aliceUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.conversationId == aliceData.conversationId; }));
}

void
ConversationMembersEventTest::testAvoidTwoOneToOneMultiDevices()
{
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
    std::this_thread::sleep_for(10s);

    // Bob add Alice, this should re-add old conversation
    bobAccount->addContact(aliceUri);
    bobAccount->sendTrustRequest(aliceUri, {});
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() {
            return bobData.conversationId == aliceData.conversationId && bob2Data.conversationId == aliceData.conversationId; }));
}

void
ConversationMembersEventTest::testRemoveRequestBannedMultiDevices()
{
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.deviceAnnounced; }));

    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));
    CPPUNIT_ASSERT(libjami::getConversationRequests(bob2Id).size() == 1);

    // Bob bans alice, should update bob2
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.contactRemoved; }));
    CPPUNIT_ASSERT(libjami::getConversationRequests(bob2Id).size() == 0);
}

void
ConversationMembersEventTest::testBanUnbanMultiDevice()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = libjami::startConversation(aliceId);

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
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));

    // Alice kick Bob while invited
    auto aliceMsgSize = aliceData.messages.size();
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    // Alice re-add Bob while invited
    aliceMsgSize = aliceData.messages.size();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));

    // bob accepts
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.conversationId == aliceData.conversationId && bob2Data.conversationId == aliceData.conversationId; }));
}

void
ConversationMembersEventTest::testBanUnbanGotFirstConv()
{
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Data.deviceAnnounced; }));

    // Alice adds bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && bob2Data.requestReceived; }));
    CPPUNIT_ASSERT(libjami::getConversationRequests(bob2Id).size() == 1);

    // Accepts requests
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData.conversationId.empty() && !bob2Data.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size();
    }));
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.contactRemoved && bob2Data.contactRemoved; }));

    // Alice sends messages, bob & bob2 should not get it!
    aliceMsgSize = aliceData.messages.size();
    libjami::sendMessage(aliceId, aliceData.conversationId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize != aliceData.messages.size(); }));
    auto msgId = aliceData.messages.rbegin()->id;
    auto getMessage = [](const auto& data, const auto& mid) -> bool {
        return std::find_if(data.messages.begin(), data.messages.end(), [&](auto& msg) { return msg.id == mid; }) != data.messages.end();
    };
    // Connection MUST fail
    CPPUNIT_ASSERT(!cv.wait_for(lk, 20s, [&]() { return getMessage(bobData, msgId) && getMessage(bob2Data, msgId); }));

    // Bobs re-add Alice
    bobData.contactAdded = false; bob2Data.contactAdded = false;
    bobAccount->addContact(aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.contactAdded
                                                    && bob2Data.contactAdded
                                                    && bobData.conversationId == bob2Data.conversationId
                                                    && bobData.conversationId == aliceData.conversationId; }));
}

void
ConversationMembersEventTest::testBanHostWhileHosting()
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

    // Now, Bob starts a call
    aliceMsgSize = aliceData.messages.size();
    auto callId = libjami::placeCallWithMedia(bobId, "swarm:" + convId, {});
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize != aliceData.messages.size(); }));

    // get active calls = 1
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, convId).size() == 1);

    // Now check that alice, has the only admin, can remove bob
    aliceMsgSize = aliceData.messages.size();
    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));
    auto members = libjami::getConversationMembers(aliceId, convId);
    auto bobBanned = false;
    for (auto& member : members) {
        if (member["uri"] == bobUri)
            bobBanned = member["role"] == "banned";
    }
    CPPUNIT_ASSERT(bobBanned);
}

void
ConversationMembersEventTest::testAddContactTwice()
{
    connectSignals();
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Add contact
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return bobData.requestReceived; }));
    auto oldConversationId = aliceData.conversationId;
    CPPUNIT_ASSERT(!oldConversationId.empty());

    // Check that the trust request's data is correct
    auto bobTrustRequests = bobAccount->getTrustRequests();
    CPPUNIT_ASSERT(bobTrustRequests.size() == 1);
    auto request = bobTrustRequests[0];
    CPPUNIT_ASSERT(request["from"] == aliceUri);
    CPPUNIT_ASSERT(request["conversationId"] == oldConversationId);

    // Remove and re-add contact
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return aliceData.removed; }));
    // wait that connections are closed.
    std::this_thread::sleep_for(10s);
    bobData.requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return bobData.requestRemoved && bobData.requestReceived; }));
    auto newConversationId = aliceData.conversationId;
    CPPUNIT_ASSERT(!newConversationId.empty());
    CPPUNIT_ASSERT(newConversationId != oldConversationId);

    // Check that the trust request's data was correctly
    // updated when we received the second request
    bobTrustRequests = bobAccount->getTrustRequests();
    CPPUNIT_ASSERT(bobTrustRequests.size() == 1);
    request = bobTrustRequests[0];
    CPPUNIT_ASSERT(request["from"] == aliceUri);
    CPPUNIT_ASSERT(request["conversationId"] == newConversationId);
}

void
ConversationMembersEventTest::testBanFromNewDevice()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaData.deviceAnnounced; }));

    auto convId = libjami::startConversation(bobId);

    libjami::addConversationMember(bobId, convId, aliceUri);
    libjami::addConversationMember(bobId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return carlaData.requestReceived; }));

    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !carlaData.conversationId.empty(); }));

    Manager::instance().sendRegister(carlaId, false);

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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bob2Data.conversationId.empty(); }));

    auto bobMsgSize = bobData.messages.size();
    libjami::removeConversationMember(bob2Id, convId, aliceUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.messages.size() == bobMsgSize + 2 /* vote + ban */; }));

    Manager::instance().sendRegister(bob2Id, false);
    auto carlaMsgSize = carlaData.messages.size();
    Manager::instance().sendRegister(carlaId, true);
    // Should sync!
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return carlaData.messages.size() > carlaMsgSize; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationMembersEventTest::name())
