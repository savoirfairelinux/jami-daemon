/*
 *  Copyright (C)2021 Savoir-faire Linux Inc.
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
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"
#include "media_const.h"
#include "video/sinkclient.h"

using namespace DRing::Account;

namespace jami {
namespace test {

struct CallData
{
    std::string callId {};
    std::string state {};
    std::atomic_bool moderatorMuted {false};

    void reset()
    {
        callId = "";
        state = "";
        moderatorMuted = false;
    }
};

class ConferenceTest : public CppUnit::TestFixture
{
public:
    ConferenceTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
    }
    ~ConferenceTest() { DRing::fini(); }
    static std::string name() { return "Conference"; }
    void setUp();
    void tearDown();

private:
    void testGetConference();
    void testModeratorMuteUpdateParticipantsInfos();
    void testAudioVideoMutedStates();
    void testCreateParticipantsSinks();

    CPPUNIT_TEST_SUITE(ConferenceTest);
    CPPUNIT_TEST(testGetConference);
    CPPUNIT_TEST(testModeratorMuteUpdateParticipantsInfos);
    CPPUNIT_TEST(testAudioVideoMutedStates);
    CPPUNIT_TEST(testCreateParticipantsSinks);
    CPPUNIT_TEST_SUITE_END();

    // Common parts
    std::string aliceId;
    std::string bobId;
    std::string carlaId;
    std::string confId {};
    CallData bobCall {};
    CallData carlaCall {};

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    void registerSignalHandlers();
    void startConference();
    void hangupConference();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConferenceTest, ConferenceTest::name());

void
ConferenceTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];

    bobCall.reset();
    carlaCall.reset();
    confId = {};
}

void
ConferenceTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId, carlaId});
}

void
ConferenceTest::registerSignalHandlers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId) {
                bobCall.callId = callId;
            } else if (accountId == carlaId) {
                carlaCall.callId = callId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [=](const std::string& callId, const std::string& state, signed) {
            if (bobCall.callId == callId)
                bobCall.state = state;
            else if (carlaCall.callId == callId)
                carlaCall.state = state;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::ConferenceCreated>(
        [=](const std::string& conferenceId) {
            confId = conferenceId;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::ConferenceRemoved>(
        [=](const std::string& conferenceId) {
            if (confId == conferenceId)
                confId = "";
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::OnConferenceInfosUpdated>(
        [=](const std::string&,
            const std::vector<std::map<std::string, std::string>> participantsInfos) {
            for (const auto& infos : participantsInfos) {
                if (infos.at("uri").find(bobUri) != std::string::npos) {
                    bobCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                } else if (infos.at("uri").find(carlaUri) != std::string::npos) {
                    carlaCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                }
            }
            cv.notify_one();
        }));

    DRing::registerSignalHandlers(confHandlers);
}

void
ConferenceTest::startConference()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    JAMI_INFO("Start call between Alice and Bob");
    auto call1 = aliceAccount->newOutgoingCall(bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobCall.callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return bobCall.state == "CURRENT"; }));

    JAMI_INFO("Start call between Alice and Carla");
    auto call2 = aliceAccount->newOutgoingCall(carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return !carlaCall.callId.empty(); }));
    Manager::instance().answerCall(carlaCall.callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return carlaCall.state == "CURRENT"; }));

    JAMI_INFO("Start conference");
    Manager::instance().joinParticipant(call1->getCallId(), call2->getCallId());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !confId.empty(); }));
}

void
ConferenceTest::hangupConference()
{
    JAMI_INFO("Stop conference");
    Manager::instance().hangupConference(confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return carlaCall.state == "OVER" && bobCall.state == "OVER" && confId.empty();
    }));
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

void
ConferenceTest::testGetConference()
{
    registerSignalHandlers();

    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 0);

    startConference();

    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 1);
    CPPUNIT_ASSERT(Manager::instance().getConferenceList()[0] == confId);

    hangupConference();

    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 0);

    DRing::unregisterSignalHandlers();
}

void
ConferenceTest::testModeratorMuteUpdateParticipantsInfos()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    JAMI_INFO("Play with mute from the moderator");
    Manager::instance().muteParticipant(confId, bobUri, true);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(5), [&] { return bobCall.moderatorMuted.load(); }));

    Manager::instance().muteParticipant(confId, bobUri, false);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(5), [&] { return !bobCall.moderatorMuted.load(); }));

    hangupConference();

    DRing::unregisterSignalHandlers();
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
    auto call1 = aliceAccount->newOutgoingCall(bobUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return !bobCall.callId.empty(); }));
    Manager::instance().answerCall(bobCall.callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return bobCall.state == "CURRENT"; }));

    call1->muteMedia(DRing::Media::MediaAttributeValue::AUDIO, true);
    call1->muteMedia(DRing::Media::MediaAttributeValue::VIDEO, true);

    JAMI_INFO("Start call between Alice and Carla");
    auto call2 = aliceAccount->newOutgoingCall(carlaUri);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return !carlaCall.callId.empty(); }));
    Manager::instance().answerCall(carlaCall.callId);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(20), [&] { return carlaCall.state == "CURRENT"; }));

    call2->muteMedia(DRing::Media::MediaAttributeValue::AUDIO, true);
    call2->muteMedia(DRing::Media::MediaAttributeValue::VIDEO, true);

    JAMI_INFO("Start conference");
    Manager::instance().joinParticipant(call1->getCallId(), call2->getCallId());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !confId.empty(); }));

    auto conf = Manager::instance().getConferenceFromID(confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&] {
        return conf->isMediaSourceMuted(jami::MediaType::MEDIA_AUDIO);
    }));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&] {
        return conf->isMediaSourceMuted(jami::MediaType::MEDIA_VIDEO);
    }));

    hangupConference();

    DRing::unregisterSignalHandlers();
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

    auto infos = Manager::instance().getConferenceInfos(confId);

    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(5), [&] {
            bool sinksStatus = true;
            for (auto& info : infos) {
                if (info["uri"] == bobUri) {
                    sinksStatus &= (Manager::instance().getSinkClient(info["sinkId"]) != nullptr);
                } else if (info["uri"] == carlaUri) {
                    sinksStatus &= (Manager::instance().getSinkClient(info["sinkId"]) != nullptr);
                }
            }
            return sinksStatus;
        }));

    hangupConference();

    DRing::unregisterSignalHandlers();
}
} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConferenceTest::name())
