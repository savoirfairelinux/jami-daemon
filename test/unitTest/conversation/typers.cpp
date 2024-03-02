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
    bool registered {false};
    bool stopped {false};
    bool requestReceived {false};
    bool deviceAnnounced {false};
    std::map<std::string, int> composing;
};

class TypersTest : public CppUnit::TestFixture
{
public:
    ~TypersTest() { libjami::fini(); }
    static std::string name() { return "Typers"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    UserData aliceData;
    std::string bobId;
    UserData bobData;

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;

    void connectSignals();

    void testSetIsComposing();
    void testTimeout();
    void testTypingRemovedOnMemberRemoved();
    void testAccountConfig();

private:
    CPPUNIT_TEST_SUITE(TypersTest);
    CPPUNIT_TEST(testSetIsComposing);
    CPPUNIT_TEST(testTimeout);
    CPPUNIT_TEST(testTypingRemovedOnMemberRemoved);
    CPPUNIT_TEST(testAccountConfig);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TypersTest, TypersTest::name());

void
TypersTest::setUp()
{
    // Init daemon
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    auto actors = load_actors("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    aliceData = {};
    bobData = {};

    wait_for_announcement_of({aliceId, bobId});
}
void
TypersTest::connectSignals()
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
                }
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                aliceData.conversationId = conversationId;
            } else if (accountId == bobId) {
                bobData.conversationId = conversationId;
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
                }
                cv.notify_one();
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::ComposingStatusChanged>(
            [&](const std::string& accountId,
                const std::string& /* conversationId */,
                const std::string& contactUri,
                int status) {
                if (accountId == aliceId) {
                    aliceData.composing[contactUri] = status;
                } else if (accountId == bobId) {
                    bobData.composing[contactUri] = status;
                }
                cv.notify_one();
            }));

    libjami::registerSignalHandlers(confHandlers);
}

void
TypersTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
TypersTest::testSetIsComposing()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    std::this_thread::sleep_for(5s); // Wait a bit to ensure that everything is updated

    libjami::setIsComposing(aliceId, "swarm:" + aliceData.conversationId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() {
        return bobData.composing[aliceUri]; }));

    libjami::setIsComposing(aliceId, "swarm:" + aliceData.conversationId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 12s, [&]() { return !bobData.composing[aliceUri]; }));
}

void
TypersTest::testTimeout()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    std::this_thread::sleep_for(5s); // Wait a bit to ensure that everything is updated

    libjami::setIsComposing(aliceId, "swarm:" + aliceData.conversationId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return bobData.composing[aliceUri]; }));

    // After 12s, it should be false
    CPPUNIT_ASSERT(cv.wait_for(lk, 12s, [&]() { return !bobData.composing[aliceUri]; }));
}

void
TypersTest::testTypingRemovedOnMemberRemoved()
{
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));

    std::this_thread::sleep_for(5s); // Wait a bit to ensure that everything is updated

    libjami::setIsComposing(bobId, "swarm:" + bobData.conversationId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return aliceData.composing[bobUri]; }));

    libjami::removeConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return !aliceData.composing[bobUri]; }));
}

void
TypersTest::testAccountConfig()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::string> details;
    details[ConfProperties::SENDCOMPOSING] = "false";
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.registered; }));

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->getTrustRequests().size() == 1);
    libjami::acceptConversationRequest(bobId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
    std::this_thread::sleep_for(5s); // Wait a bit to ensure that everything is updated

    // Should not receive composing status
    libjami::setIsComposing(aliceId, "swarm:" + aliceData.conversationId, true);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&]() {
        return bobData.composing[aliceUri]
        ; }));
}


} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::TypersTest::name())
