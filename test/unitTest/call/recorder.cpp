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
#include "fileutils.h"
#include "jami.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "media_const.h"

#include "common.h"

using namespace DRing::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct CallData
{
    std::string callId {};
    std::string state {};
    std::string mediaStatus {};
    std::string device {};
    std::string hostState {};

    void reset()
    {
        callId = "";
        state = "";
        mediaStatus = "";
        device = "";
        hostState = "";
    }
};

class RecorderTest : public CppUnit::TestFixture
{
public:
    RecorderTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
    }
    ~RecorderTest() { DRing::fini(); }
    static std::string name() { return "Recorder"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    CallData bobCall {};

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

private:
    void registerSignalHandlers();
    void testRecordCall();
    void testRecordAudioOnlyCall();
    void testStopCallWhileRecording();
    void testDaemonPreference();

    CPPUNIT_TEST_SUITE(RecorderTest);
    CPPUNIT_TEST(testRecordCall);
    CPPUNIT_TEST(testRecordAudioOnlyCall);
    CPPUNIT_TEST(testStopCallWhileRecording);
    CPPUNIT_TEST(testDaemonPreference);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RecorderTest, RecorderTest::name());

void
RecorderTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    bobCall.reset();

    fileutils::recursive_mkdir("./records");
    DRing::setRecordPath("./records");
}

void
RecorderTest::tearDown()
{
    DRing::setIsAlwaysRecording(false);
    fileutils::removeAll("./records");

    wait_for_removal_of({aliceId, bobId});
}

void
RecorderTest::registerSignalHandlers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId) {
                bobCall.callId = callId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::CallSignal::StateChange>([=](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            if (accountId == aliceId) {
                auto details = DRing::getCallDetails(aliceId, callId);
                if (details["PEER_NUMBER"].find(bobUri) != std::string::npos)
                    bobCall.hostState = state;
            } else if (bobCall.callId == callId)
                bobCall.state = state;
            cv.notify_one();
        }));

    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>&) {
            if (callId == bobCall.callId)
                bobCall.mediaStatus = event;
            cv.notify_one();
        }));

    DRing::registerSignalHandlers(confHandlers);
}

void
RecorderTest::testRecordCall()
{
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    auto callId = DRing::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // Start recorder
    CPPUNIT_ASSERT(!DRing::getIsRecording(aliceId, callId));
    auto files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 0);
    DRing::toggleRecording(aliceId, callId);

    // Stop recorder after a few seconds
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(DRing::getIsRecording(aliceId, callId));
    DRing::toggleRecording(aliceId, callId);
    CPPUNIT_ASSERT(!DRing::getIsRecording(aliceId, callId));

    files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 1);

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));
}

void
RecorderTest::testRecordAudioOnlyCall()
{
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{DRing::Media::MediaAttributeKey::MEDIA_TYPE, DRing::Media::MediaAttributeValue::AUDIO},
           {DRing::Media::MediaAttributeKey::ENABLED, TRUE_STR},
           {DRing::Media::MediaAttributeKey::MUTED, FALSE_STR},
           {DRing::Media::MediaAttributeKey::SOURCE, ""},
           {DRing::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);
    auto callId = DRing::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // Start recorder
    auto files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 0);
    DRing::toggleRecording(aliceId, callId);

    // Stop recorder
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(DRing::getIsRecording(aliceId, callId));
    DRing::toggleRecording(aliceId, callId);
    CPPUNIT_ASSERT(!DRing::getIsRecording(aliceId, callId));

    files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 1);

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));
}

void
RecorderTest::testStopCallWhileRecording()
{
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    auto callId = DRing::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // Start recorder
    auto files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 0);
    DRing::toggleRecording(aliceId, callId);

    // Hangup call
    std::this_thread::sleep_for(5s);
    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));

    files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 1);
}

void
RecorderTest::testDaemonPreference()
{
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    DRing::setIsAlwaysRecording(true);

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    auto callId = DRing::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // Let record some seconds
    std::this_thread::sleep_for(5s);

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));
    auto files = fileutils::readDirectory("./records");
    CPPUNIT_ASSERT(files.size() == 1);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::RecorderTest::name())
