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
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;

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

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    std::string convId = "";
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

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 0);
}

void
ConversationRequestTest::acceptConvReqAlsoAddContact()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false, memberMessageGenerated = false;
    int conversationReady = 0;
    std::string convId = "";
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
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady += 1;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    auto convId2 = libjami::startConversation(aliceId);
    requestReceived = false;
    libjami::addConversationMember(aliceId, convId2, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::acceptConversationRequest(bobId, convId2);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady == 2; }));
    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 0);
}

void
ConversationRequestTest::testGetRequests()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
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

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    CPPUNIT_ASSERT(requests.front()["id"] == convId);
    libjami::unregisterSignalHandlers();
}

void
ConversationRequestTest::testDeclineRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
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

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    libjami::declineConversationRequest(bobId, convId);
    // Decline request
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
    libjami::unregisterSignalHandlers();
}

void
ConversationRequestTest::testAddContact()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
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
    ConversationRepository repo(aliceAccount, convId);
    // Mode must be one to one
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::ONE_TO_ONE);
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));
    auto clonedPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                      + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(clonedPath));
    auto bobMember = clonedPath + DIR_SEPARATOR_STR + "members" + DIR_SEPARATOR_STR + bobUri
                     + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(bobMember));
}

void
ConversationRequestTest::testDeclineConversationRequestRemoveTrustRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));

    // Decline request
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    auto trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 1);
    libjami::declineConversationRequest(bobId, convId);
    requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
    trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 0);
}

void
ConversationRequestTest::testMalformedTrustRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         requestDeclined = false;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));

    // Decline request
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1);
    auto trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 1);
    // This will let the trust request (not libjami::declineConversationRequest)
    bobAccount->convModule()->declineConversationRequest(convId);
    requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
    trustRequests = libjami::getTrustRequests(bobId);
    CPPUNIT_ASSERT(trustRequests.size() == 1);
    // Reload conversation will fix the state (the trustRequest is removed in another thread)
    bobAccount->convModule()->loadConversations();
    requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);

    auto start = std::chrono::steady_clock::now();

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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
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
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    // removeContact
    aliceAccount->removeContact(bobUri, false);
    std::this_thread::sleep_for(5s); // wait a bit that connections are closed

    // re-add
    CPPUNIT_ASSERT(convId != "");
    auto oldConvId = convId;
    convId = "";
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Should retrieve previous conversation
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return oldConvId == convId; }));
}

void
ConversationRequestTest::testInviteFromMessageAfterRemoved()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string&,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == bobId)
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
    requestReceived = false;
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    // removeContact
    bobAccount->removeContact(aliceUri, false);
    std::this_thread::sleep_for(10s); // wait a bit that connections are closed

    // bob sends a message, this should generate a new request for Alice
    requestReceived = false;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    conversationReady = false;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
}

void
ConversationRequestTest::testRemoveContact()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
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
    bool conversationRemovedAlice = false, conversationRemovedBob = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    conversationRemovedAlice = true;
                else if (accountId == bobId)
                    conversationRemovedBob = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    conversationRemovedBob = false;
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemovedBob; }));

    auto details = bobAccount->getContactDetails(aliceUri);
    CPPUNIT_ASSERT(details.size() == 0);

    conversationRemovedAlice = false;
    aliceAccount->removeContact(bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return conversationRemovedAlice; }));

    std::this_thread::sleep_for(
        10s); // There is no signal, but daemon should then erase the repository

    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));

    repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
               + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(!fileutils::isDirectory(repoPath));
}

void
ConversationRequestTest::testRemoveSelfDoesntRemoveConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         conversationRemoved = false;
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
                if (accountId == bobId)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    conversationRemoved = false;
    aliceAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return conversationRemoved; }));
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
}

void
ConversationRequestTest::testRemoveConversationUpdateContactDetails()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false,
         conversationRemoved = false;
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
                if (accountId == bobId)
                    conversationRemoved = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId && message["type"] == "member") {
                memberMessageGenerated = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestReceived; }));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));

    conversationRemoved = false;
    libjami::removeConversation(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemoved; }));

    auto details = bobAccount->getContactDetails(aliceUri);
    CPPUNIT_ASSERT(details[libjami::Account::TrustRequest::CONVERSATIONID] == "");
}

void
ConversationRequestTest::testBanContact()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
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
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !convId.empty(); }));
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));

    memberMessageGenerated = false;
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return memberMessageGenerated; }));
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + bobAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
}

void
ConversationRequestTest::testBanContactRemoveTrustRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, requestDeclined = true;
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestDeclined>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId)
                    requestDeclined = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    // Check created files
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    bobAccount->removeContact(aliceUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return requestDeclined; }));
    auto requests = libjami::getConversationRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 0);
}

void
ConversationRequestTest::testAddOfflineContactThenConnect()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
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
                if (account_id == carlaId)
                    requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == carlaId) {
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
    aliceAccount->addContact(carlaUri);
    aliceAccount->sendTrustRequest(carlaUri, {});
    cv.wait_for(lk, 5s); // Wait 5 secs for the put to happen
    CPPUNIT_ASSERT(!convId.empty());
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() { return requestReceived; }));
    memberMessageGenerated = false;
    CPPUNIT_ASSERT(carlaAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationReady && memberMessageGenerated; }));
}

void
ConversationRequestTest::testDeclineTrustRequestDoNotGenerateAnother()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    aliceAccount->trackBuddyPresence(bobUri, true);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false, memberMessageGenerated = false;
    std::string convId = "";
    auto bobConnected = false;
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
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = bobAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    bobConnected = true;
                    cv.notify_one();
                } else if (daemonStatus == "UNREGISTERED") {
                    bobConnected = false;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->discardTrustRequest(aliceUri));
    cv.wait_for(lk, 10s); // Wait a bit
    bobConnected = true;
    Manager::instance().sendRegister(bobId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobConnected; }));
    // Trigger on peer online
    requestReceived = false;
    Manager::instance().sendRegister(bobId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobConnected; }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
}

void
ConversationRequestTest::testRemoveContactRemoveSyncing()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, contactAdded = false, requestReceived = false;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& convId,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId && !convId.empty())
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri) {
                contactAdded = true;
            }
            cv.notify_one();
        }));
    bool contactRemoved = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactRemoved>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri) {
                contactRemoved = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactAdded; }));

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 1);
    bobAccount->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactRemoved; }));

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 0);
}

void
ConversationRequestTest::testRemoveConversationRemoveSyncing()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, contactAdded = false, requestReceived = false,
         conversationRemoved = false;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& convId,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId && !convId.empty())
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
                if (accountId == bobId) {
                    conversationRemoved = true;
                }
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri) {
                contactAdded = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactAdded; }));
    // At this point the conversation should be there and syncing.

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 1);
    libjami::removeConversation(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationRemoved; }));

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 0);
}

void
ConversationRequestTest::testCacheRequestFromClient()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false, requestReceived = false;
    std::string convId = "";
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
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationReady = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    aliceAccount->addContact(bobUri);
    std::vector<uint8_t> payload = {0x64, 0x64, 0x64};
    aliceAccount->sendTrustRequest(bobUri,
                                   payload); // Random payload, just care with the file
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));
    auto cachedPath = fileutils::get_cache_dir() + DIR_SEPARATOR_CH + aliceAccount->getAccountID()
                      + DIR_SEPARATOR_CH + "requests" + DIR_SEPARATOR_CH + bobUri;
    CPPUNIT_ASSERT(fileutils::isFile(cachedPath));
    CPPUNIT_ASSERT(fileutils::loadFile(cachedPath) == payload);

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return conversationReady; }));
    CPPUNIT_ASSERT(!fileutils::isFile(cachedPath));
}

void
ConversationRequestTest::testNeedsSyncingWithForCloning()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool contactAdded = false, requestReceived = false;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& convId,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId && !convId.empty())
                    requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string& uri, bool) {
            if (accountId == bobId && uri == aliceUri) {
                contactAdded = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    CPPUNIT_ASSERT(!bobAccount->convModule()->needsSyncingWith(aliceUri, aliceDevice));
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return requestReceived; }));

    Manager::instance().sendRegister(aliceId, false); // This avoid to sync immediately
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return contactAdded; }));
    // At this point the conversation should be there and syncing.

    CPPUNIT_ASSERT(libjami::getConversations(bobId).size() == 1);
    CPPUNIT_ASSERT(bobAccount->convModule()->needsSyncingWith(aliceUri, aliceDevice));
}

void
ConversationRequestTest::testRemoveContactRemoveTrustRequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

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

    wait_for_announcement_of(bob2Id);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);

    bool conversationB1Ready = false, conversationB2Ready = false, conversationB1Removed = false,
         conversationB2Removed = false, requestB1Received = false, requestB2Received = false,
         memberMessageGenerated = false;
    std::string convId = "";
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
            [&](const std::string& account_id,
                const std::string& /*from*/,
                const std::string& /*conversationId*/,
                const std::vector<uint8_t>& /*payload*/,
                time_t /*received*/) {
                if (account_id == bobId)
                    requestB1Received = true;
                else if (account_id == bob2Id)
                    requestB2Received = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            } else if (accountId == bobId) {
                conversationB1Ready = true;
            } else if (accountId == bob2Id) {
                conversationB2Ready = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == bobId) {
                    conversationB1Removed = true;
                } else if (accountId == bob2Id) {
                    conversationB2Removed = true;
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

    // First, Alice adds Bob
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !convId.empty() && requestB1Received && requestB2Received; }));

    // Bob1 accepts, both device should get it
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return conversationB1Ready && conversationB2Ready && memberMessageGenerated;
    }));

    // Bob2 remove Alice ; Bob1 should not have any trust requests
    bob2Account->removeContact(aliceUri, false);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return conversationB1Removed && conversationB2Removed; }));
    std::this_thread::sleep_for(10s); // Wait a bit to ensure that everything is update (via synced)
    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 0);
    CPPUNIT_ASSERT(bob2Account->getTrustRequests().size() == 0);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationRequestTest::name())
