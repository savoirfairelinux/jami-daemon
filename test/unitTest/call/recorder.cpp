/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
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
#include "client/videomanager.h"

#include "common.h"

using namespace libjami::Account;
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
    bool changeRequested = false;

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
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~RecorderTest() { libjami::fini(); }
    static std::string name() { return "Recorder"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string recordDir;
    std::string recordedFile;
    CallData bobCall {};

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    std::string videoPath = std::filesystem::absolute("media/test_video_file.mp4").string();

private:
    void registerSignalHandlers();
    void testRecordCall();
    void testRecordAudioOnlyCall();
    void testRecordCallOnePersonRdv();
    void testStopCallWhileRecording();
    void testDaemonPreference();

    CPPUNIT_TEST_SUITE(RecorderTest);
    CPPUNIT_TEST(testRecordCall);
    CPPUNIT_TEST(testRecordAudioOnlyCall);
    CPPUNIT_TEST(testRecordCallOnePersonRdv);
    CPPUNIT_TEST(testStopCallWhileRecording);
    CPPUNIT_TEST(testDaemonPreference);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RecorderTest, RecorderTest::name());

void
RecorderTest::setUp()
{
    // Generate a temporary directory with a file inside
    recordDir = "records";
    fileutils::recursive_mkdir(recordDir.c_str());
    CPPUNIT_ASSERT(fileutils::isDirectory(recordDir));

    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    bobCall.reset();

    libjami::setRecordPath(recordDir);
}

void
RecorderTest::tearDown()
{
    libjami::setIsAlwaysRecording(false);
    fileutils::removeAll(recordDir);

    wait_for_removal_of({aliceId, bobId});
}

void
RecorderTest::registerSignalHandlers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId) {
                bobCall.callId = callId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaChangeRequested>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId && bobCall.callId == callId) {
                bobCall.changeRequested = true;
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::CallSignal::StateChange>([=](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            if (accountId == aliceId) {
                auto details = libjami::getCallDetails(aliceId, callId);
                if (details["PEER_NUMBER"].find(bobUri) != std::string::npos)
                    bobCall.hostState = state;
            } else if (bobCall.callId == callId)
                bobCall.state = state;
            cv.notify_one();
        }));

    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>&) {
            if (callId == bobCall.callId)
                bobCall.mediaStatus = event;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::RecordPlaybackStopped>(
        [&](const std::string& path) {
            recordedFile = path;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

void
RecorderTest::testRecordCall()
{
    JAMI_INFO("Start testRecordCall");
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttributeA
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
        {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
        {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
        {libjami::Media::MediaAttributeKey::LABEL, "audio_0"},
        {libjami::Media::MediaAttributeKey::SOURCE, ""}};
    std::map<std::string, std::string> mediaAttributeV
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
        {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
        {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
        {libjami::Media::MediaAttributeKey::LABEL, "video_0"},
        {libjami::Media::MediaAttributeKey::SOURCE, "file://" + videoPath}};
    mediaList.emplace_back(mediaAttributeA);
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    libjami::acceptWithMedia(bobId, bobCall.callId, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    std::this_thread::sleep_for(5s);

    // Start recorder
    recordedFile.clear();
    CPPUNIT_ASSERT(!libjami::getIsRecording(aliceId, callId));
    libjami::toggleRecording(aliceId, callId);
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(libjami::getIsRecording(aliceId, callId));

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return recordedFile.empty(); }));

    // add local video
    {
        auto newMediaList = mediaList;
        newMediaList.emplace_back(mediaAttributeV);

        // Request Media Change
        libjami::requestMediaChange(aliceId, callId, newMediaList);

        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return bobCall.changeRequested; }));

        // Answer the change request
        bobCall.mediaStatus = "";
        libjami::answerMediaChangeRequest(bobId, bobCall.callId, newMediaList);
        bobCall.changeRequested = false;
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
            return bobCall.mediaStatus
                == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
        }));

        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !recordedFile.empty() && recordedFile.find(".ogg") != std::string::npos; }));
        recordedFile = "";
        // give time to start camera
        std::this_thread::sleep_for(10s);

        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return recordedFile.empty(); }));
    }

    // mute local video
    {
        mediaAttributeV[libjami::Media::MediaAttributeKey::MUTED] = TRUE_STR;
        auto newMediaList = mediaList;
        newMediaList.emplace_back(mediaAttributeV);

        // Mute Bob video
        libjami::requestMediaChange(aliceId, callId, newMediaList);
        std::this_thread::sleep_for(5s);
        libjami::requestMediaChange(bobId, bobCall.callId, newMediaList);

        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !recordedFile.empty() && recordedFile.find(".webm") != std::string::npos; }));
        recordedFile = "";
        std::this_thread::sleep_for(10s);
    }

    // Stop recorder after a few seconds
    libjami::toggleRecording(aliceId, callId);
    CPPUNIT_ASSERT(!libjami::getIsRecording(aliceId, callId));

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !recordedFile.empty() && recordedFile.find(".ogg") != std::string::npos; }));

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));
    JAMI_INFO("End testRecordCall");
}

void
RecorderTest::testRecordAudioOnlyCall()
{
    JAMI_INFO("Start testRecordAudioOnlyCall");
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    // Audio only call
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
           {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
           {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
           {libjami::Media::MediaAttributeKey::LABEL, "audio_0"},
           {libjami::Media::MediaAttributeKey::SOURCE, ""}};
    mediaList.emplace_back(mediaAttribute);
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    libjami::acceptWithMedia(bobId, bobCall.callId, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // Start recorder
    recordedFile.clear();
    libjami::toggleRecording(aliceId, callId);
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(libjami::getIsRecording(aliceId, callId));

    // Toggle recording
    libjami::toggleRecording(aliceId, callId);
    CPPUNIT_ASSERT(!libjami::getIsRecording(aliceId, callId));

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return !recordedFile.empty() && recordedFile.find(".ogg") != std::string::npos;
    }));

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));
    JAMI_INFO("End testRecordAudioOnlyCall");
}

void
RecorderTest::testRecordCallOnePersonRdv()
{
    JAMI_INFO("Start testRecordCallOnePersonRdv");
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    try {
        bobAccount->editConfig(
            [&](AccountConfig& config) { config.isRendezVous = true; });
    } catch (...) {}

    CPPUNIT_ASSERT(bobAccount->config().isRendezVous);

    recordedFile.clear();

    JAMI_INFO("Start call between Alice and Bob");
    // Audio only call
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttributeA
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
           {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
           {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
           {libjami::Media::MediaAttributeKey::LABEL, "audio_0"},
           {libjami::Media::MediaAttributeKey::SOURCE, ""}};
    mediaList.emplace_back(mediaAttributeA);
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    CPPUNIT_ASSERT(!libjami::getIsRecording(aliceId, callId));
    libjami::toggleRecording(aliceId, callId);
    std::this_thread::sleep_for(5s);

    CPPUNIT_ASSERT(libjami::getIsRecording(aliceId, callId));

    // Stop recorder
    libjami::toggleRecording(aliceId, callId);
    CPPUNIT_ASSERT(!libjami::getIsRecording(aliceId, callId));

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !recordedFile.empty() && recordedFile.find(".ogg") != std::string::npos; }));

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER"; }));
    JAMI_INFO("End testRecordCallOnePersonRdv");
}

void
RecorderTest::testStopCallWhileRecording()
{
    JAMI_INFO("Start testStopCallWhileRecording");
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttributeA
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
        {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
        {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
        {libjami::Media::MediaAttributeKey::LABEL, "audio_0"},
        {libjami::Media::MediaAttributeKey::SOURCE, ""}};
    std::map<std::string, std::string> mediaAttributeV
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
        {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
        {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
        {libjami::Media::MediaAttributeKey::LABEL, "video_0"},
        {libjami::Media::MediaAttributeKey::SOURCE, "file://" + videoPath}};
    mediaList.emplace_back(mediaAttributeA);
    mediaList.emplace_back(mediaAttributeV);
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    libjami::acceptWithMedia(bobId, bobCall.callId, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // give time to start camera
    std::this_thread::sleep_for(5s);

    // Start recorder
    recordedFile.clear();
    libjami::toggleRecording(aliceId, callId);
    std::this_thread::sleep_for(10s);
    CPPUNIT_ASSERT(libjami::getIsRecording(aliceId, callId));

    // Hangup call
    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER" && !recordedFile.empty() && recordedFile.find(".webm") != std::string::npos; }));
    JAMI_INFO("End testStopCallWhileRecording");
}

void
RecorderTest::testDaemonPreference()
{
    JAMI_INFO("Start testDaemonPreference");
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    libjami::setIsAlwaysRecording(true);
    recordedFile.clear();

    JAMI_INFO("Start call between Alice and Bob");
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttributeA
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
        {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
        {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
        {libjami::Media::MediaAttributeKey::LABEL, "audio_0"},
        {libjami::Media::MediaAttributeKey::SOURCE, ""}};
    std::map<std::string, std::string> mediaAttributeV
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
        {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
        {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
        {libjami::Media::MediaAttributeKey::LABEL, "video_0"},
        {libjami::Media::MediaAttributeKey::SOURCE, "file://" + videoPath}};
    mediaList.emplace_back(mediaAttributeA);
    mediaList.emplace_back(mediaAttributeV);
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    libjami::acceptWithMedia(bobId, bobCall.callId, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return bobCall.mediaStatus
               == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS;
    }));

    // Let record some seconds
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(libjami::getIsRecording(aliceId, callId));
    std::this_thread::sleep_for(10s);

    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER" && !recordedFile.empty() && recordedFile.find(".webm") != std::string::npos; }));
    JAMI_INFO("End testDaemonPreference");
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::RecorderTest::name())
