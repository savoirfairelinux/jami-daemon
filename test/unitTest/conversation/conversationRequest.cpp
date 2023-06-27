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
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"

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
    bool registered {false};
    bool stopped {false};
    bool deviceAnnounced {false};
    bool contactAdded {false};
    bool contactRemoved {false};
    std::string profilePath;
    std::string payloadTrustRequest;
    std::vector<libjami::SwarmMessage> messages;
    std::vector<libjami::SwarmMessage> messagesUpdated;
};

class ConversationRequestTest : public CppUnit::TestFixture
{
public:
    ~ConversationRequestTest() { libjami::fini(); }
    static std::string name() { return "ConversationRequest"; }
    void setUp();
    void tearDown();

    void testAcceptTrustRemoveConvReq();
    void acceptConvReqAlsoAddContact();
    void testGetRequests();
    void testDeclineRequest();
    void testAddContact();
    void testDeclineConversationRequestRemoveTrustRequest();
    void testMalformedTrustRequest();
    void testAddContactDeleteAndReAdd();
    void testInviteFromMessageAfterRemoved();
    void testRemoveContact();
    void testRemoveContactMultiDevice();
    void testRemoveSelfDoesntRemoveConversation();
    void testRemoveConversationUpdateContactDetails();
    void testBanContact();
    void testBanContactRemoveTrustRequest();
    void testAddOfflineContactThenConnect();
    void testDeclineTrustRequestDoNotGenerateAnother();
    void testRemoveContactRemoveSyncing();
    void testRemoveConversationRemoveSyncing();
    void testCacheRequestFromClient();
    void testNeedsSyncingWithForCloning();
    void testRemoveContactRemoveTrustRequest();
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
    CPPUNIT_TEST_SUITE(ConversationRequestTest);
    CPPUNIT_TEST(testAcceptTrustRemoveConvReq);
    CPPUNIT_TEST(acceptConvReqAlsoAddContact);
    CPPUNIT_TEST(testGetRequests);
    CPPUNIT_TEST(testDeclineRequest);
    CPPUNIT_TEST(testAddContact);
    CPPUNIT_TEST(testDeclineConversationRequestRemoveTrustRequest);
    CPPUNIT_TEST(testMalformedTrustRequest);
    CPPUNIT_TEST(testAddContactDeleteAndReAdd);
    CPPUNIT_TEST(testInviteFromMessageAfterRemoved);
    CPPUNIT_TEST(testRemoveContact);
    CPPUNIT_TEST(testRemoveContactMultiDevice);
    CPPUNIT_TEST(testRemoveSelfDoesntRemoveConversation);
    CPPUNIT_TEST(testRemoveConversationUpdateContactDetails);
    CPPUNIT_TEST(testBanContact);
    CPPUNIT_TEST(testBanContactRemoveTrustRequest);
    CPPUNIT_TEST(testAddOfflineContactThenConnect);
    CPPUNIT_TEST(testDeclineTrustRequestDoNotGenerateAnother);
    CPPUNIT_TEST(testRemoveContactRemoveSyncing);
    CPPUNIT_TEST(testRemoveConversationRemoveSyncing);
    CPPUNIT_TEST(testCacheRequestFromClient);
    CPPUNIT_TEST(testNeedsSyncingWithForCloning);
    CPPUNIT_TEST(testRemoveContactRemoveTrustRequest);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRequestTest, ConversationRequestTest::name());

void
ConversationRequestTest::setUp()
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
    bobData = {};
    bob2Data = {};
    carlaData = {};

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}
void
ConversationRequestTest::connectSignals()
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string&, bool) {
            if (accountId == bobId) {
                bobData.contactAdded = true;
            }
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

    libjami::registerSignalHandlers(confHandlers);
}


void
ConversationRequestTest::tearDown()
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
ConversationRequestTest::testAcceptTrustRemoveConvReq()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 0);
}

void
ConversationRequestTest::acceptConvReqAlsoAddContact()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    bobData.requestReceived = false;
    auto convId2 = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId2, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId2);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 0);
}

void
ConversationRequestTest::testGetRequests()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    CPPUNIT_ASSERT(requests.front()["id"] == convId);
}

void
ConversationRequestTest::testDeclineRequest()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::declineConversationRequest(bobId, convId);
    // Decline request
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
}

void
ConversationRequestTest::testAddContact()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !aliceData.conversationId.empty(); }));
    ConversationRepository repo(aliceAccount, aliceData.conversationId);
    // Mode must be one to one
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::ONE_TO_ONE);
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + aliceData.conversationId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size();; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + aliceData.conversationId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(clonedPath));
    auto bobMember = clonedPath + DIR_SEPARATOR_STR + "members" + DIR_SEPARATOR_STR + bobUri
                     + ".crt";
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobMember));
}

void
ConversationRequestTest::testDeclineConversationRequestRemoveTrustRequest()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    // Decline request
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    auto trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 1);
    libjami::declineConversationRequest(bobId, aliceData.conversationId);
    requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
    trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 0);
}

void
ConversationRequestTest::testMalformedTrustRequest()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    // Decline request
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    auto trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 1);
    // This will let the trust request (not libjami::declineConversationRequest)
    bobAccount->convModule()->declineConversationRequest(aliceData.conversationId);
    requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
    trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 1);
    // Reload conversation will fix the state (the trustRequest is removed in another thread)
    bobAccount->convModule()->loadConversations();
    requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);

    auto start = std::chrono::steady_clock::now();

    auto requestDeclined = false;
    do {
        trustRequests = libjami::getTrustRequests(bobId);
        requestDeclined = trustRequests.size() == 0;
        if (!requestDeclined)
            std::this_thread::sleep_for(1s);
    } while (not requestDeclined and std::chrono::steady_clock::now() - start < 2s);

    CPPUNIT_ASSERT(requestDeclined);
}

void
ConversationRequestTest::testAddContactDeleteAndReAdd()
{
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
        cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size(); }));

    // removeContact
    aliceAccount->removeContact(bobUri, false);
    std::this_thread::sleep_for(5s); // wait a bit that connections are closed

    // re-add
    CPPUNIT_ASSERT(aliceData.conversationId != "");
    auto oldConvId = aliceData.conversationId;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Should retrieve previous conversation
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return oldConvId == aliceData.conversationId; }));
}

void
ConversationRequestTest::testInviteFromMessageAfterRemoved()
{
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
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // removeContact
    bobAccount->removeContact(aliceUri, false);
    std::this_thread::sleep_for(10s); // wait a bit that connections are closed

    // bob sends a message, this should generate a new request for Alice
    bobData.requestReceived = false;
    libjami::sendMessage(aliceId, aliceData.conversationId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    bobData.conversationId = "";
    CPPUNIT_ASSERT(bobAccount->getContacts().size() == 0);
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    CPPUNIT_ASSERT(bobAccount->getContacts().size() == 1);
}

void
ConversationRequestTest::testRemoveContact()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.removed; }));

    auto details = bobAccount->getContactDetails(aliceUri);
    CPPUNIT_ASSERT(details.size() == 0);

    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return aliceData.removed; }));

    std::this_thread::sleep_for(
        10s); // There is no signal, but daemon should then erase the repository

    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + aliceData.conversationId;
    CPPUNIT_ASSERT(!std::filesystem::is_directory(repoPath));

    repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
               + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + bobData.conversationId;
    CPPUNIT_ASSERT(!std::filesystem::is_directory(repoPath));
}

void
ConversationRequestTest::testRemoveContactMultiDevice()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    // Add second device for Bob
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bob2Data.deviceAnnounced;
    }));
    // First, Alice adds Bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.requestReceived && bob2Data.requestReceived;
    }));

    // Bob1 decline via removeContact, both device should remove the request
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.requestRemoved && bob2Data.requestRemoved; }));
}

void
ConversationRequestTest::testRemoveSelfDoesntRemoveConversation()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    aliceAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return aliceData.removed; }));
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + aliceData.conversationId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
}

void
ConversationRequestTest::testRemoveConversationUpdateContactDetails()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    libjami::removeConversation(bobId, bobData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.removed; }));

    auto details = bobAccount->getContactDetails(aliceUri);
    CPPUNIT_ASSERT(details[libjami::Account::TrustRequest::CONVERSATIONID] == "");
}

void
ConversationRequestTest::testBanContact()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 2 == aliceData.messages.size(); }));
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobId
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + bobData.conversationId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
}

void
ConversationRequestTest::testBanContactRemoveTrustRequest()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return bobData.requestRemoved; }));
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
}

void
ConversationRequestTest::testAddOfflineContactThenConnect()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    aliceAccount->addContact(carlaUri);
    aliceAccount->sendTrustRequest(carlaUri, {});
    cv.wait_for(lk, 5s); // Wait 5 secs for the put to happen
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() { return carlaData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(carlaAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() == aliceMsgSize + 1; }));
}

void
ConversationRequestTest::testDeclineTrustRequestDoNotGenerateAnother()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    aliceAccount->trackBuddyPresence(bobUri, true);

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->discardTrustRequest(aliceUri));
    cv.wait_for(lk, 10s); // Wait a bit
    Manager::instance().sendRegister(bobId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.stopped; }));
    // Trigger on peer online
    bobData.deviceAnnounced = false; bobData.requestReceived = false;
    Manager::instance().sendRegister(bobId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.deviceAnnounced; }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
}

void
ConversationRequestTest::testRemoveContactRemoveSyncing()
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.contactAdded; }));

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 1);
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.contactRemoved; }));

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 0);
}

void
ConversationRequestTest::testRemoveConversationRemoveSyncing()
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.contactAdded; }));
    // At this point the conversation should be there and syncing.

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 1);
    libjami::removeConversation(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.removed; }));

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 0);
}

void
ConversationRequestTest::testCacheRequestFromClient()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    std::vector<uint8_t> payload = {0x64, 0x64, 0x64};
    aliceAccount->sendTrustRequest(bobUri,
                                   payload); // Random payload, just care with the file
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto cachedPath = fileutils::get_cache_dir() + DIR_SEPARATOR_CH + aliceId
                      + DIR_SEPARATOR_CH + "requests" + DIR_SEPARATOR_CH + bobUri;
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(cachedPath));
    CPPUNIT_ASSERT(fileutils::loadFile(cachedPath) == payload);

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    CPPUNIT_ASSERT(!std::filesystem::is_regular_file(cachedPath));
}

void
ConversationRequestTest::testNeedsSyncingWithForCloning()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());

    CPPUNIT_ASSERT(!bobAccount->convModule()->needsSyncingWith(aliceUri, aliceDevice));
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.contactAdded; }));
    // At this point the conversation should be there and syncing.

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 1);
    CPPUNIT_ASSERT(bobAccount->convModule()->needsSyncingWith(aliceUri, aliceDevice));
}

void
ConversationRequestTest::testRemoveContactRemoveTrustRequest()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    // Add second device for Bob
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bob2Data.deviceAnnounced;
    }));
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);

    // First, Alice adds Bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.requestReceived && bob2Data.requestReceived;
    }));

    // Bob1 accepts, both device should get it
    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData.conversationId.empty() && !bob2Data.conversationId.empty() && aliceMsgSize + 1 == aliceData.messages.size();
    }));

    // Bob2 remove Alice ; Bob1 should not have any trust requests
    bob2Account->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.contactRemoved && bob2Data.contactRemoved; }));
    std::this_thread::sleep_for(10s); // Wait a bit to ensure that everything is update (via synced)
    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 0);
    CPPUNIT_ASSERT(bob2Account->getTrustRequests().size() == 0);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationRequestTest::name())
