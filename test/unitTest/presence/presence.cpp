/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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
#include <filesystem>
#include <string>

#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "conversation/conversationcommon.h"
#include "manager.h"
#include "media_const.h"
#include "sip/sipcall.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct UserData
{
    std::map<std::string, int> status;
    std::map<std::string, std::string> statusNote;

    std::string conversationId;
    bool requestReceived {false};
};

class PresenceTest : public CppUnit::TestFixture
{
public:
    ~PresenceTest() { libjami::fini(); }
    static std::string name() { return "PresenceTest"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string carlaId;
    UserData aliceData_;
    UserData bobData_;
    UserData carlaData_;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

private:
    void connectSignals();
    void enableCarla();

    void testGetSetSubscriptions();
    void testPresenceStatus();
    void testPresenceStatusNote();
    void testPresenceInvalidStatusNote();
    void testPresenceStatusNoteBeforeConnection();

    CPPUNIT_TEST_SUITE(PresenceTest);
    CPPUNIT_TEST(testGetSetSubscriptions);
    CPPUNIT_TEST(testPresenceStatus);
    CPPUNIT_TEST(testPresenceStatusNote);
    CPPUNIT_TEST(testPresenceInvalidStatusNote);
    CPPUNIT_TEST(testPresenceStatusNoteBeforeConnection);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PresenceTest, PresenceTest::name());

void
PresenceTest::setUp()
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
    aliceData_ = {};
    bobData_ = {};
    carlaData_ = {};

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
PresenceTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId, carlaId});
}

void
PresenceTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                aliceData_.conversationId = conversationId;
            else if (accountId == bobId)
                bobData_.conversationId = conversationId;
            else if (accountId == carlaId)
                carlaData_.conversationId = conversationId;
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& /*conversationId*/,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == aliceId)
                    aliceData_.requestReceived = true;
                if (accountId == bobId)
                    bobData_.requestReceived = true;
                if (accountId == carlaId)
                    carlaData_.requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::PresenceSignal::NewBuddyNotification>(
            [&](const std::string& accountId,
                const std::string& peerId,
                int status,
                const std::string& note) {
                if (accountId == aliceId) {
                    aliceData_.status[peerId] = status;
                    aliceData_.statusNote[peerId] = note;
                } else if (accountId == bobId) {
                    bobData_.status[peerId] = status;
                    bobData_.statusNote[peerId] = note;
                } else if (accountId == carlaId) {
                    carlaData_.status[peerId] = status;
                    carlaData_.statusNote[peerId] = note;
                }
                cv.notify_one();
            }));

    libjami::registerSignalHandlers(confHandlers);
}

void
PresenceTest::enableCarla()
{
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));
    confHandlers.clear();
    libjami::unregisterSignalHandlers();
}

void
PresenceTest::testGetSetSubscriptions()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    connectSignals();

    CPPUNIT_ASSERT(libjami::getSubscriptions(aliceId).empty());
    libjami::setSubscriptions(aliceId, {bobUri, carlaUri});
    auto subscriptions = libjami::getSubscriptions(aliceId);
    CPPUNIT_ASSERT(subscriptions.size() == 2);
    auto checkSub = [&](const std::string& uri) {
        return std::find_if(subscriptions.begin(), subscriptions.end(), [&](const auto& sub) {
                   return sub.at("Buddy") == uri;
               }) != subscriptions.end();
    };
    CPPUNIT_ASSERT(checkSub(bobUri) && checkSub(carlaUri));
}

void
PresenceTest::testPresenceStatus()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();

    // Track Presence
    aliceAccount->trackBuddyPresence(bobUri, true);
    aliceAccount->trackBuddyPresence(carlaUri, true);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status.find(bobUri) != aliceData_.status.end() && aliceData_.status[bobUri] == 1;
    }));
    CPPUNIT_ASSERT(aliceData_.status.find(carlaUri) == aliceData_.status.end()); // For now, still offline so no change

    // Carla is now online
    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return aliceData_.status.find(carlaUri) != aliceData_.status.end() && aliceData_.status[carlaUri] == 1;
    }));

    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.conversationId.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.conversationId, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.conversationId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));
    libjami::acceptConversationRequest(bobId, aliceData_.conversationId);
    libjami::acceptConversationRequest(carlaId, aliceData_.conversationId);

    // Should connect to peers
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status[bobUri] == 2 && aliceData_.status[carlaUri] == 2;
    }));

    // Carla disconnects, should just let the presence on the DHT for a few minutes
    Manager::instance().sendRegister(carlaId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status[carlaUri] == 1;
    }));
}

void
PresenceTest::testPresenceStatusNote()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();

    // NOTE: Presence status notes are only sent to contacts.
    aliceAccount->addContact(bobUri);
    aliceAccount->addContact(carlaUri);

    // Track Presence
    aliceAccount->trackBuddyPresence(bobUri, true);
    bobAccount->trackBuddyPresence(aliceUri, true);
    aliceAccount->trackBuddyPresence(carlaUri, true);
    carlaAccount->trackBuddyPresence(aliceUri, true);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status.find(bobUri) != aliceData_.status.end() && aliceData_.status[bobUri] == 1
            && aliceData_.status.find(carlaUri) != aliceData_.status.end() && aliceData_.status[carlaUri] == 1;
    }));

    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.conversationId.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.conversationId, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.conversationId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));
    libjami::acceptConversationRequest(bobId, aliceData_.conversationId);
    libjami::acceptConversationRequest(carlaId, aliceData_.conversationId);

    // Should connect to peers
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status[bobUri] == 2 && aliceData_.status[carlaUri] == 2;
    }));

    // Alice sends a status note, should be received by Bob and Carla
    libjami::publish(aliceId, true, "Testing Jami");

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData_.statusNote.find(aliceUri) != bobData_.statusNote.end() && bobData_.statusNote[aliceUri] == "Testing Jami"
            && carlaData_.statusNote.find(aliceUri) != carlaData_.statusNote.end() && carlaData_.statusNote[aliceUri] == "Testing Jami";
    }));
}


void
PresenceTest::testPresenceInvalidStatusNote()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();

    // Track Presence
    aliceAccount->trackBuddyPresence(bobUri, true);
    bobAccount->trackBuddyPresence(aliceUri, true);
    aliceAccount->trackBuddyPresence(carlaUri, true);
    carlaAccount->trackBuddyPresence(aliceUri, true);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status.find(bobUri) != aliceData_.status.end() && aliceData_.status[bobUri] == 1
            && aliceData_.status.find(carlaUri) != aliceData_.status.end() && aliceData_.status[carlaUri] == 1;
    }));

    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.conversationId.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.conversationId, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.conversationId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));
    libjami::acceptConversationRequest(bobId, aliceData_.conversationId);
    libjami::acceptConversationRequest(carlaId, aliceData_.conversationId);

    // Should connect to peers
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status[bobUri] == 2 && aliceData_.status[carlaUri] == 2;
    }));

    // Alice sends a status note that will generate an invalid XML message
    libjami::publish(aliceId, true, "Testing<BAD>");

    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() {
        return bobData_.statusNote.find(aliceUri) != bobData_.statusNote.end() && bobData_.statusNote[aliceUri] == "Testing<BAD>"
            && carlaData_.statusNote.find(aliceUri) != carlaData_.statusNote.end() && carlaData_.statusNote[aliceUri] == "Testing<BAD>";
    }));
}

void
PresenceTest::testPresenceStatusNoteBeforeConnection()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();

    // Track Presence
    aliceAccount->trackBuddyPresence(bobUri, true);
    bobAccount->trackBuddyPresence(aliceUri, true);
    aliceAccount->trackBuddyPresence(carlaUri, true);
    carlaAccount->trackBuddyPresence(aliceUri, true);
    libjami::publish(aliceId, true, "Testing Jami");

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status.find(bobUri) != aliceData_.status.end() && aliceData_.status[bobUri] == 1
            && aliceData_.status.find(carlaUri) != aliceData_.status.end() && aliceData_.status[carlaUri] == 1;
    }));

    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.conversationId.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.conversationId, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.conversationId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));
    libjami::acceptConversationRequest(bobId, aliceData_.conversationId);
    libjami::acceptConversationRequest(carlaId, aliceData_.conversationId);

    // Should connect to peers
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.status[bobUri] == 2 && aliceData_.status[carlaUri] == 2;
    }));

    // Alice sends a status note, should be received by Bob and Carla
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData_.statusNote.find(aliceUri) != bobData_.statusNote.end() && bobData_.statusNote[aliceUri] == "Testing Jami"
            && carlaData_.statusNote.find(aliceUri) != carlaData_.statusNote.end() && carlaData_.statusNote[aliceUri] == "Testing Jami";
    }));

}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::PresenceTest::name())
