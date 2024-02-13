/*
 *  Copyright (C) 2024 Savoir-faire Linux Inc.
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

#include "manager.h"
#include "sip/sipaccount.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "media_const.h"
#include "call_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <filesystem>
#include <string>

using namespace libjami::Account;
using namespace libjami::Call::Details;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

class SIPCallTest : public CppUnit::TestFixture
{
public:
    SIPCallTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~SIPCallTest() { libjami::fini(); }
    static std::string name() { return "Call"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testCall();

    CPPUNIT_TEST_SUITE(SIPCallTest);
    CPPUNIT_TEST(testCall);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SIPCallTest, SIPCallTest::name());

void
SIPCallTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob_SIP.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    std::this_thread::sleep_for(10s);
}

void
SIPCallTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
SIPCallTest::testCall()
{
    auto aliceAccount = Manager::instance().getAccount<SIPAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<SIPAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    JAMI_ERROR("@@@@@@@@@22 {}", bobUri);
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
            libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
            {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);

    auto call = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callReceived.load(); }));

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(aliceId, call);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped == 2; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SIPCallTest::name())
