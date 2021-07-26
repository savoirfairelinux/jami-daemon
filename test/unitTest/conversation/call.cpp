/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
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
#include <filesystem>
#include <string>

#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "conversation/conversationcommon.h"
#include "manager.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

class ConversationCallTest : public CppUnit::TestFixture
{
public:
    ~ConversationCallTest() { DRing::fini(); }
    static std::string name() { return "ConversationCallTest"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;

private:
    void testActiveCalls();

    CPPUNIT_TEST_SUITE(ConversationCallTest);
    CPPUNIT_TEST(testActiveCalls);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationCallTest, ConversationCallTest::name());

void
ConversationCallTest::setUp()
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
ConversationCallTest::tearDown()
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
ConversationCallTest::testActiveCalls()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->currentDeviceId();
    auto uri = aliceAccount->getUsername();

    std::string convId;
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
    std::vector<std::map<std::string, std::string>> messages;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && conversationId == convId)
                messages.emplace_back(message);
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Start conversation
    convId = DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return conversationReady; });

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, convId).size() == 0);

    // start call
    messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + convId, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !messages.empty() && messages[0]["type"] == "application/call-history+json";
    });

    // get active calls = 1
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, convId).size() == 1);

    // hangup
    messages.clear();
    Manager::instance().hangupCall(aliceId, callId);

    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !messages.empty() && messages[0].find("duration") != messages[0].end();
    });

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, convId).size() == 0);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationCallTest::name())
