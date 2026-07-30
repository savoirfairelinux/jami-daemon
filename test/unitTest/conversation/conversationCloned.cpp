/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include <mutex>
#include <string>

#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "jami.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

struct UserData
{
    std::string conversationId;
    bool requestReceived {false};
    bool deviceAnnounced {false};
    unsigned clonedCount {0};
    unsigned messagesReceived {0};
};

class ConversationClonedTest : public CppUnit::TestFixture
{
public:
    ~ConversationClonedTest() { libjami::fini(); }
    static std::string name() { return "ConversationCloned"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    UserData aliceData;
    std::string bobId;
    UserData bobData;

    std::mutex mtx;
    std::condition_variable cv;

    template<typename Rep, typename Period, typename Predicate>
    bool waitFor(const std::chrono::duration<Rep, Period>& timeout, Predicate&& predicate)
    {
        std::unique_lock lk {mtx};
        return cv.wait_for(lk, timeout, std::forward<Predicate>(predicate));
    }

    void connectSignals();

private:
    void testClonedStillEmittedAfterAFollowUpFetch();

    CPPUNIT_TEST_SUITE(ConversationClonedTest);
    CPPUNIT_TEST(testClonedStillEmittedAfterAFollowUpFetch);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationClonedTest, ConversationClonedTest::name());

void
ConversationClonedTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
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
ConversationClonedTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
ConversationClonedTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [&](const std::string& accountId, const std::map<std::string, std::string>&) {
            std::lock_guard guard {mtx};
            auto account = Manager::instance().getAccount<JamiAccount>(accountId);
            if (!account)
                return;
            auto announced = account->getVolatileAccountDetails()[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED]
                             == "true";
            if (accountId == aliceId)
                aliceData.deviceAnnounced = announced;
            else if (accountId == bobId)
                bobData.deviceAnnounced = announced;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            std::lock_guard guard {mtx};
            if (accountId == aliceId)
                aliceData.conversationId = conversationId;
            else if (accountId == bobId)
                bobData.conversationId = conversationId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
        [&](const std::string& accountId, const std::string&, std::map<std::string, std::string>) {
            std::lock_guard guard {mtx};
            if (accountId == aliceId)
                aliceData.requestReceived = true;
            else if (accountId == bobId)
                bobData.requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId, const std::string&, libjami::SwarmMessage) {
            std::lock_guard guard {mtx};
            if (accountId == aliceId)
                aliceData.messagesReceived++;
            else if (accountId == bobId)
                bobData.messagesReceived++;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationCloned>(
        [&](const std::string& accountId) {
            std::lock_guard guard {mtx};
            if (accountId == aliceId)
                aliceData.clonedCount++;
            else if (accountId == bobId)
                bobData.clonedCount++;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

void
ConversationClonedTest::testClonedStillEmittedAfterAFollowUpFetch()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    // Bob clones a first conversation from Alice. Alice serves it, so she
    // reports that she is done cloning.
    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(waitFor(60s, [&] { return bobData.requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(waitFor(60s, [&] { return !bobData.conversationId.empty(); }));
    CPPUNIT_ASSERT(waitFor(60s, [&] { return aliceData.clonedCount >= 1; }));

    // Bob keeps fetching over the very same git channel, so Alice's git server
    // serves several more packfiles for a single connection.
    auto bobMessages = bobData.messagesReceived;
    libjami::sendMessage(aliceId, aliceData.conversationId, "hello"s, "");
    CPPUNIT_ASSERT(waitFor(60s, [&] { return bobData.messagesReceived > bobMessages; }));
    bobMessages = bobData.messagesReceived;
    libjami::sendMessage(aliceId, aliceData.conversationId, "hello again"s, "");
    CPPUNIT_ASSERT(waitFor(60s, [&] { return bobData.messagesReceived > bobMessages; }));

    // Those extra fetches must not have disturbed Alice's accounting: a second
    // conversation cloned by Bob still has to be reported.
    auto clonedCount = aliceData.clonedCount;
    bobData.conversationId.clear();
    bobData.requestReceived = false;
    auto convId = libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(waitFor(60s, [&] { return aliceData.conversationId == convId; }));
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(waitFor(60s, [&] { return bobData.requestReceived; }));
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(waitFor(60s, [&] { return bobData.conversationId == convId; }));

    CPPUNIT_ASSERT(waitFor(60s, [&] { return aliceData.clonedCount > clonedCount; }));
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::ConversationClonedTest::name())
