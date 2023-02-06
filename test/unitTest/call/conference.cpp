/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

#include "manager.h"
#include "client/videomanager.h"
#include "connectivity/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"
#include "media_const.h"
#include "video/sinkclient.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"

using namespace libjami::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct CallData
{
    std::string callId {};
    std::string state {};
    std::string device {};
    std::string streamId {};
    std::string hostState {};
    std::atomic_bool moderatorMuted {false};
    std::atomic_bool raisedHand {false};
    std::atomic_bool active {false};
    std::atomic_bool recording {false};

    void reset()
    {
        callId = "";
        state = "";
        device = "";
        streamId = "";
        hostState = "";
        moderatorMuted = false;
        active = false;
        raisedHand = false;
        recording = false;
    }
};

class ConferenceTest : public CppUnit::TestFixture
{
public:
    ConferenceTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConferenceTest() { libjami::fini(); }
    static std::string name() { return "Conference"; }
    void setUp();
    void tearDown();

private:
    void testGetConference();
    void testModeratorMuteUpdateParticipantsInfos();
    void testUnauthorizedMute();
    void testAudioVideoMutedStates();
    void testCreateParticipantsSinks();
    void testMuteStatusAfterRemove();
    void testActiveStatusAfterRemove();
    void testHandsUp();
    void testPeerLeaveConference();
    void testJoinCallFromOtherAccount();
    void testDevices();
    void testUnauthorizedSetActive();
    void testHangup();
    void testIsConferenceParticipant();
    void testAudioConferenceConfInfo();
    void testHostAddRmSecondVideo();
    void testParticipantAddRmSecondVideo();
    void testPropagateRecording();
    void testBrokenParticipantAudioAndVideo();
    void testBrokenParticipantAudioOnly();
    void testRemoveConferenceInOneOne();

    CPPUNIT_TEST_SUITE(ConferenceTest);
    CPPUNIT_TEST(testGetConference);
    CPPUNIT_TEST(testModeratorMuteUpdateParticipantsInfos);
    CPPUNIT_TEST(testUnauthorizedMute);
    CPPUNIT_TEST(testAudioVideoMutedStates);
    CPPUNIT_TEST(testCreateParticipantsSinks);
    CPPUNIT_TEST(testMuteStatusAfterRemove);
    CPPUNIT_TEST(testActiveStatusAfterRemove);
    CPPUNIT_TEST(testHandsUp);
    CPPUNIT_TEST(testPeerLeaveConference);
    CPPUNIT_TEST(testJoinCallFromOtherAccount);
    CPPUNIT_TEST(testDevices);
    CPPUNIT_TEST(testUnauthorizedSetActive);
    CPPUNIT_TEST(testHangup);
    CPPUNIT_TEST(testIsConferenceParticipant);
    CPPUNIT_TEST(testAudioConferenceConfInfo);
    CPPUNIT_TEST(testHostAddRmSecondVideo);
    CPPUNIT_TEST(testParticipantAddRmSecondVideo);
    CPPUNIT_TEST(testPropagateRecording);
    CPPUNIT_TEST(testBrokenParticipantAudioAndVideo);
    CPPUNIT_TEST(testBrokenParticipantAudioOnly);
    CPPUNIT_TEST(testRemoveConferenceInOneOne);
    CPPUNIT_TEST_SUITE_END();

    // Common parts
    std::string aliceId;
    std::atomic_bool hostRecording {false};
    std::string bobId;
    std::string carlaId;
    std::string daviId;
    std::string confId {};
    std::vector<std::map<std::string, std::string>> pInfos_ {};
    bool confChanged {false};

    CallData bobCall {};
    CallData carlaCall {};
    CallData daviCall {};

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    void registerSignalHandlers();
    void startConference(bool audioOnly = false, bool addDavi = false);
    void hangupConference();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConferenceTest, ConferenceTest::name());

void
ConferenceTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla-davi.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];
    daviId = actors["davi"];

    bobCall.reset();
    carlaCall.reset();
    daviCall.reset();
    confId = {};
    confChanged = false;
    hostRecording = false;
}

void
ConferenceTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId, carlaId, daviId});
}

void
ConferenceTest::registerSignalHandlers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto daviUri = daviAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId) {
                bobCall.callId = callId;
            } else if (accountId == carlaId) {
                carlaCall.callId = callId;
            } else if (accountId == daviId) {
                daviCall.callId = callId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string& state,
            signed) {
            if (accountId == aliceId) {
                auto details = libjami::getCallDetails(aliceId, callId);
                if (details["PEER_NUMBER"].find(bobUri) != std::string::npos)
                    bobCall.hostState = state;
                else if (details["PEER_NUMBER"].find(carlaUri) != std::string::npos)
                    carlaCall.hostState = state;
                else if (details["PEER_NUMBER"].find(daviUri) != std::string::npos)
                    daviCall.hostState = state;
            } else if (bobCall.callId == callId)
                bobCall.state = state;
            else if (carlaCall.callId == callId)
                carlaCall.state = state;
            else if (daviCall.callId == callId)
                daviCall.state = state;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceCreated>(
        [=](const std::string&, const std::string& conferenceId) {
            confId = conferenceId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceRemoved>(
        [=](const std::string&, const std::string& conferenceId) {
            if (confId == conferenceId)
                confId = "";
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceChanged>(
        [=](const std::string&, const std::string& conferenceId, const std::string&) {
            if (confId == conferenceId)
                confChanged = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::OnConferenceInfosUpdated>(
        [=](const std::string&,
            const std::vector<std::map<std::string, std::string>> participantsInfos) {
            pInfos_ = participantsInfos;
            for (const auto& infos : participantsInfos) {
                if (infos.at("uri").find(bobUri) != std::string::npos) {
                    bobCall.active = infos.at("active") == "true";
                    bobCall.recording = infos.at("recording") == "true";
                    bobCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                    bobCall.raisedHand = infos.at("handRaised") == "true";
                    bobCall.device = infos.at("device");
                    bobCall.streamId = infos.at("sinkId");
                } else if (infos.at("uri").find(carlaUri) != std::string::npos) {
                    carlaCall.active = infos.at("active") == "true";
                    carlaCall.recording = infos.at("recording") == "true";
                    carlaCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                    carlaCall.raisedHand = infos.at("handRaised") == "true";
                    carlaCall.device = infos.at("device");
                    carlaCall.streamId = infos.at("sinkId");
                } else if (infos.at("uri").find(daviUri) != std::string::npos) {
                    daviCall.active = infos.at("active") == "true";
                    daviCall.recording = infos.at("recording") == "true";
                    daviCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                    daviCall.raisedHand = infos.at("handRaised") == "true";
                    daviCall.device = infos.at("device");
                    daviCall.streamId = infos.at("sinkId");
                } else if (infos.at("uri").find(aliceUri) != std::string::npos) {
                    hostRecording = infos.at("recording") == "true";
                }
            }
            cv.notify_one();
        }));

    libjami::registerSignalHandlers(confHandlers);
}

void
ConferenceTest::startConference(bool audioOnly, bool addDavi)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto daviUri = daviAccount->getUsername();

    std::vector<std::map<std::string, std::string>> mediaList;
    if (audioOnly) {
        std::map<std::string, std::string> mediaAttribute
            = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                libjami::Media::MediaAttributeValue::AUDIO},
               {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
               {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
               {libjami::Media::MediaAttributeKey::SOURCE, ""},
               {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
        mediaList.emplace_back(mediaAttribute);
    }

    JAMI_INFO("Start call between Alice and Bob");
    auto call1 = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.hostState == "CURRENT"; }));

    JAMI_INFO("Start call between Alice and Carla");
    auto call2 = libjami::placeCallWithMedia(aliceId, carlaUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !carlaCall.callId.empty(); }));
    Manager::instance().answerCall(carlaId, carlaCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return carlaCall.hostState == "CURRENT"; }));

    JAMI_INFO("Start conference");
    confChanged = false;
    Manager::instance().joinParticipant(aliceId, call1, aliceId, call2);
    // ConfChanged is the signal emitted when the 2 calls will be added to the conference
    // Also, wait that participants appears in conf info to get all good informations
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
        return !confId.empty() && confChanged && !carlaCall.device.empty()
               && !bobCall.device.empty();
    }));

    if (addDavi) {
        JAMI_INFO("Start call between Alice and Davi");
        auto call1 = libjami::placeCallWithMedia(aliceId, daviUri, mediaList);
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
        Manager::instance().answerCall(daviId, daviCall.callId);
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
        Manager::instance().addParticipant(aliceId, call1, aliceId, confId);
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
    }
}

void
ConferenceTest::hangupConference()
{
    JAMI_INFO("Stop conference");
    Manager::instance().hangupConference(aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
        return bobCall.state == "OVER" && carlaCall.state == "OVER" && confId.empty();
    }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
        return daviCall.callId.empty() ? true : daviCall.state == "OVER";
    }));
}

void
ConferenceTest::testGetConference()
{
    registerSignalHandlers();

    CPPUNIT_ASSERT(libjami::getConferenceList(aliceId).size() == 0);

    startConference();

    CPPUNIT_ASSERT(libjami::getConferenceList(aliceId).size() == 1);
    CPPUNIT_ASSERT(libjami::getConferenceList(aliceId)[0] == confId);

    hangupConference();

    CPPUNIT_ASSERT(libjami::getConferenceList(aliceId).size() == 0);

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testModeratorMuteUpdateParticipantsInfos()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    JAMI_INFO("Play with mute from the moderator");
    libjami::muteStream(aliceId, confId, bobUri, bobCall.device, bobCall.streamId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.moderatorMuted.load(); }));

    libjami::muteStream(aliceId, confId, bobUri, bobCall.device, bobCall.streamId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.moderatorMuted.load(); }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testUnauthorizedMute()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    JAMI_INFO("Play with mute from unauthorized");
    libjami::muteStream(carlaId, confId, bobUri, bobCall.device, bobCall.streamId, true);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 15s, [&] { return bobCall.moderatorMuted.load(); }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testAudioVideoMutedStates()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    auto call1Id = libjami::placeCallWithMedia(aliceId, bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.hostState == "CURRENT"; }));
    auto call1 = aliceAccount->getCall(call1Id);
    call1->muteMedia(libjami::Media::MediaAttributeValue::AUDIO, true);
    call1->muteMedia(libjami::Media::MediaAttributeValue::VIDEO, true);

    JAMI_INFO("Start call between Alice and Carla");
    auto call2Id = libjami::placeCallWithMedia(aliceId, carlaUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !carlaCall.callId.empty(); }));
    Manager::instance().answerCall(carlaId, carlaCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return carlaCall.hostState == "CURRENT"; }));

    auto call2 = aliceAccount->getCall(call2Id);
    call2->muteMedia(libjami::Media::MediaAttributeValue::AUDIO, true);
    call2->muteMedia(libjami::Media::MediaAttributeValue::VIDEO, true);

    JAMI_INFO("Start conference");
    Manager::instance().joinParticipant(aliceId, call1Id, aliceId, call2Id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !confId.empty(); }));

    auto conf = aliceAccount->getConference(confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
        return conf->isMediaSourceMuted(jami::MediaType::MEDIA_AUDIO);
    }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
        return conf->isMediaSourceMuted(jami::MediaType::MEDIA_VIDEO);
    }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testCreateParticipantsSinks()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    startConference();

    auto expectedNumberOfParticipants = 3;

    // Check participants number
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));

    if (not jami::getVideoDeviceMonitor().getDeviceList().empty()) {
        JAMI_INFO() << "Check sinks if video device available.";
        for (auto& info : pInfos_) {
            auto uri = string_remove_suffix(info["uri"], '@');
            if (uri == bobUri) {
                CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
                    return Manager::instance().getSinkClient(info["sinkId"]) != nullptr;
                }));
            } else if (uri == carlaUri) {
                CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
                    return Manager::instance().getSinkClient(info["sinkId"]) != nullptr;
                }));
            }
        }
    } else {
        JAMI_INFO() << "Check sinks if no video device available.";
        for (auto& info : pInfos_) {
            auto uri = string_remove_suffix(info["uri"], '@');
            if (uri == bobUri) {
                CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
                    return Manager::instance().getSinkClient(info["sinkId"]) == nullptr;
                }));
            } else if (uri == carlaUri) {
                CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
                    return Manager::instance().getSinkClient(info["sinkId"]) == nullptr;
                }));
            }
        }
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testMuteStatusAfterRemove()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    libjami::muteStream(aliceId, confId, daviUri, daviCall.device, daviCall.streamId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return daviCall.moderatorMuted.load(); }));

    Manager::instance().hangupCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    daviCall.reset();

    auto call2 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    Manager::instance().answerCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    Manager::instance().addParticipant(aliceId, call2, aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));

    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !daviCall.moderatorMuted.load(); }));

    Manager::instance().hangupCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testActiveStatusAfterRemove()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "audio_0";
    defaultAudio.enabled_ = true;

    libjami::setActiveStream(aliceId, confId, daviUri, daviCall.device, daviCall.streamId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return daviCall.active.load(); }));

    Manager::instance().hangupCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    daviCall.reset();

    auto call2 = libjami::placeCallWithMedia(aliceId,
                                             daviUri,
                                             MediaAttribute::mediaAttributesToMediaMaps(
                                                 {defaultAudio}));
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    Manager::instance().answerCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    Manager::instance().addParticipant(aliceId, call2, aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));

    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !daviCall.active.load(); }));

    Manager::instance().hangupCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testHandsUp()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    JAMI_INFO("Play with raise hand");
    libjami::raiseHand(bobId, bobCall.callId, bobUri, bobCall.device, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.raisedHand.load(); }));

    libjami::raiseHand(bobId, bobCall.callId, bobUri, bobCall.device, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.raisedHand.load(); }));

    // Remove davi from moderators
    libjami::setModerator(aliceId, confId, daviUri, false);

    // Test to raise hand
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
    libjami::raiseHand(daviId, daviCall.callId, daviUri, daviCall.device, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return daviCall.raisedHand.load(); }));

    // Test to raise hand for another one (should fail)
    libjami::raiseHand(bobId, bobCall.callId, carlaUri, carlaCall.device, true);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&] { return carlaCall.raisedHand.load(); }));

    // However, a moderator should be able to lower the hand (but not a non moderator)
    libjami::raiseHand(carlaId, carlaCall.callId, carlaUri, carlaCall.device, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return carlaCall.raisedHand.load(); }));

    libjami::raiseHand(daviId, carlaCall.callId, carlaUri, carlaCall.device, false);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&] { return !carlaCall.raisedHand.load(); }));

    libjami::raiseHand(bobId, bobCall.callId, carlaUri, carlaCall.device, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !carlaCall.raisedHand.load(); }));

    Manager::instance().hangupCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    daviCall.reset();

    auto call2 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    Manager::instance().answerCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    Manager::instance().addParticipant(aliceId, call2, aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));

    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !daviCall.raisedHand.load(); }));

    Manager::instance().hangupCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testPeerLeaveConference()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();
    Manager::instance().hangupCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER" && confId.empty(); }));

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testJoinCallFromOtherAccount()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference();

    JAMI_INFO("Play with raise hand");
    libjami::raiseHand(aliceId, confId, bobUri, bobCall.device, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.raisedHand.load(); }));

    libjami::raiseHand(aliceId, confId, bobUri, bobCall.device, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.raisedHand.load(); }));

    JAMI_INFO("Start call between Alice and Davi");
    auto call1 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    Manager::instance().answerCall(daviId, daviCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    CPPUNIT_ASSERT(Manager::instance().addParticipant(daviId, daviCall.callId, aliceId, confId));
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testDevices()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobDevice = std::string(bobAccount->currentDeviceId());
    auto carlaDevice = std::string(carlaAccount->currentDeviceId());

    startConference();

    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] {
        return bobCall.device == bobDevice && carlaDevice == carlaCall.device;
    }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testUnauthorizedSetActive()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();

    startConference();

    libjami::setActiveStream(carlaId, confId, bobUri, bobCall.device, bobCall.streamId, true);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 15s, [&] { return bobCall.active.load(); }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testHangup()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    libjami::hangupParticipant(carlaId, confId, daviUri, daviCall.device); // Unauthorized
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&] { return daviCall.state == "OVER"; }));
    libjami::hangupParticipant(aliceId, confId, daviUri, daviCall.device);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return daviCall.state == "OVER"; }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testIsConferenceParticipant()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    // is Conference participant should be true for Carla
    auto participants = aliceAccount->getConference(confId)->getParticipantList();
    CPPUNIT_ASSERT(participants.size() == 2);
    auto call1 = *participants.begin();
    auto call2 = *participants.rbegin();
    CPPUNIT_ASSERT(aliceAccount->getCall(call1)->isConferenceParticipant());
    CPPUNIT_ASSERT(aliceAccount->getCall(call2)->isConferenceParticipant());

    // hangup bob will stop the conference
    Manager::instance().hangupCall(aliceId, call1);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return confId.empty(); }));
    CPPUNIT_ASSERT(!aliceAccount->getCall(call2)->isConferenceParticipant());
    Manager::instance().hangupCall(aliceId, call2);

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testHostAddRmSecondVideo()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    registerSignalHandlers();

    startConference();

    // Alice adds new media
    pInfos_.clear();
    std::vector<std::map<std::string, std::string>> mediaList
        = {{{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
             libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
             libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "bar"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
             libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "foo"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_1"}}};
    libjami::requestMediaChange(aliceId, confId, mediaList);

    // Check that alice has two videos attached to the conference
    auto aliceVideos = [&]() {
        int result = 0;
        for (auto i = 0u; i < pInfos_.size(); ++i)
            if (pInfos_[i]["uri"].find(aliceUri) != std::string::npos)
                result += 1;
        return result;
    };
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceVideos() == 2; }));

    // Alice removes her second video
    pInfos_.clear();
    mediaList.pop_back();
    libjami::requestMediaChange(aliceId, confId, mediaList);

    // Check that alice has ont video attached to the conference
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceVideos() == 1; }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testAudioConferenceConfInfo()
{
    registerSignalHandlers();

    startConference(true);

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testParticipantAddRmSecondVideo()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    registerSignalHandlers();

    startConference();

    // Bob adds new media
    pInfos_.clear();
    std::vector<std::map<std::string, std::string>> mediaList
        = {{{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
             libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
             libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "bar"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
             libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "foo"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_1"}}};
    libjami::requestMediaChange(bobId, bobCall.callId, mediaList);

    // Check that bob has two videos attached to the conference
    auto bobVideos = [&]() {
        int result = 0;
        for (auto i = 0u; i < pInfos_.size(); ++i)
            if (pInfos_[i]["uri"].find(bobUri) != std::string::npos)
                result += 1;
        return result;
    };
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return bobVideos() == 2; }));

    // Bob removes his second video
    pInfos_.clear();
    mediaList.pop_back();
    libjami::requestMediaChange(bobId, bobCall.callId, mediaList);

    // Check that bob has ont video attached to the conference
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return bobVideos() == 1; }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testPropagateRecording()
{
    registerSignalHandlers();

    startConference();

    JAMI_INFO("Play with recording state");
    libjami::toggleRecording(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.recording.load(); }));

    libjami::toggleRecording(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.recording.load(); }));

    libjami::toggleRecording(aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return hostRecording.load(); }));

    libjami::toggleRecording(aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !hostRecording.load(); }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testBrokenParticipantAudioAndVideo()
{
    registerSignalHandlers();

    // Start conference with four participants
    startConference(false, true);
    auto expectedNumberOfParticipants = 4u;

    // Check participants number
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));

    // Crash participant
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto call2Crash = std::dynamic_pointer_cast<SIPCall>(daviAccount->getCall(daviCall.callId));
    pjsip_transport_shutdown(call2Crash->getTransport()->get());

    // Check participants number
    // It should have one less participant than in the conference start
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&] { return expectedNumberOfParticipants - 1 == pInfos_.size(); }));

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testBrokenParticipantAudioOnly()
{
    registerSignalHandlers();

    // Start conference with four participants
    startConference(true, true);
    auto expectedNumberOfParticipants = 4u;

    // Check participants number
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));

    // Crash participant
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto call2Crash = std::dynamic_pointer_cast<SIPCall>(daviAccount->getCall(daviCall.callId));
    pjsip_transport_shutdown(call2Crash->getTransport()->get());

    // Check participants number
    // It should have one less participant than in the conference start
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&] { return expectedNumberOfParticipants - 1 == pInfos_.size(); }));

    hangupConference();
    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testRemoveConferenceInOneOne()
{
    registerSignalHandlers();
    startConference();
    // Here it's 1:1 calls we merged, so we can close the conference
    JAMI_INFO("Hangup Bob");
    Manager::instance().hangupCall(bobId, bobCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return confId.empty() && bobCall.state == "OVER"; }));
    Manager::instance().hangupCall(carlaId, carlaCall.callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaCall.state == "OVER"; }));
    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConferenceTest::name())
