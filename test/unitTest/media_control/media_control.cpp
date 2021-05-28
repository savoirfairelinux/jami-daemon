/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "sip/sdp.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>

#include "common.h"

using namespace DRing::Account;
using namespace DRing::Call;

namespace jami {
namespace test {

struct CallData
{
    std::string userName_ {};
    std::string alias_ {};
    std::shared_ptr<JamiAccount> account_;
    std::shared_ptr<SIPCall> sipCall_;
    std::vector<MediaAttribute> mediaAttrList_ {};
    std::string signal_ {};
    std::string event_ {};
    std::condition_variable cv_ {};
};

struct TestScenario
{
    TestScenario(const std::vector<MediaAttribute>& offer,
                 const std::vector<MediaAttribute>& answer,
                 const std::vector<MediaAttribute>& offerUpdate,
                 const std::vector<MediaAttribute>& answerUpdate)
        : offer_(std::move(offer))
        , answer_(std::move(answer))
        , offerUpdate_(std::move(offerUpdate))
        , answerUpdate_(std::move(answerUpdate))
    {}

    TestScenario() {};

    std::vector<MediaAttribute> offer_;
    std::vector<MediaAttribute> answer_;
    std::vector<MediaAttribute> offerUpdate_;
    std::vector<MediaAttribute> answerUpdate_;
};

/**
 * Basic tests for media negotiation.
 */
class MediaControlTest : public CppUnit::TestFixture
{
public:
    MediaControlTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~MediaControlTest() { DRing::fini(); }

    static std::string name() { return "MediaControlTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void testCallWithMediaList();

    CPPUNIT_TEST_SUITE(MediaControlTest);
    CPPUNIT_TEST(testCallWithMediaList);
    CPPUNIT_TEST_SUITE_END();

    // Helpers
    void testWithScenario(CallData& aliceData, CallData& bobData, const TestScenario& scenario);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    bool waitForSignal(CallData& callData,
                       const std::string& signal,
                       const std::string& expectedEvent = {});

    // Event/Signal handlers
    void onCallStateChange(const std::string& callId, const std::string& state, CallData& callData);
    void onIncomingCall(const std::string& accountId, const std::string& callId, CallData& callData);
    void onVideoMuted(const std::string& callId, bool muted, CallData& callData);

private:
    std::string aliceAccountId_;
    std::string bobAccountId_;
    std::mutex mtx_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaControlTest, MediaControlTest::name());

void
MediaControlTest::setUp()
{
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceAccountId_ = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobAccountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");

    WAIT_FOR_ANNOUNCEMENT_OF(aliceAccountId_, bobAccountId_);
}

void
MediaControlTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    std::atomic_bool accountsRemoved {false};
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - 2) {
                accountsRemoved = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceAccountId_, true);
    Manager::instance().removeAccount(bobAccountId_, true);
    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}

void
MediaControlTest::onIncomingCall(const std::string& accountId,
                                 const std::string& callId,
                                 CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.account_->getAccountID(), accountId);

    JAMI_DBG("%s [%s] received an incoming call [%s]",
             callData.alias_.c_str(),
             accountId.c_str(),
             callId.c_str());
    auto call = Manager::instance().getCallFromCallID(callId);

    Manager::instance().answerCall(callId);

    callData.sipCall_ = std::dynamic_pointer_cast<SIPCall>(call);
    callData.event_ = StateEvent::INCOMING;
}

void
MediaControlTest::onCallStateChange(const std::string& callId,
                                    const std::string& state,
                                    CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    // Interested only in main (parent) call.
    if (not call or call->isSubcall())
        return;

    auto account = call->getAccount().lock();
    assert(account);

    JAMI_DBG("Signal [Call State Change] - user [%s] - call [%s] - state [%s]",
             account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
             callId.c_str(),
             state.c_str());

    if (account->getAccountID() != callData.account_->getAccountID())
        return;

    callData.event_ = state;
    callData.signal_ = DRing::CallSignal::StateChange::name;

    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP") {
        callData.cv_.notify_one();
    }
}

void
MediaControlTest::onVideoMuted(const std::string& callId, bool muted, CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call or call->isSubcall())
        return;
    auto account = call->getAccount().lock();
    assert(account);

    JAMI_INFO("Signal [Video Muted] - user [%s] - call [%s] - state [%s]",
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              muted ? "Mute" : "Un-mute");

    if (account->getAccountID() != callData.account_->getAccountID())
        return;

    callData.mediaAttrList_ = call->getMediaAttributeList();

    callData.signal_ = DRing::CallSignal::VideoMuted::name;
    callData.cv_.notify_one();
}

bool
MediaControlTest::waitForSignal(CallData& callData,
                                const std::string& expectedSignal,
                                const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {20};
    std::unique_lock<std::mutex> lock {mtx_};

    auto res = callData.cv_.wait_for(lock, TIME_OUT, [&] {
        bool pred = callData.signal_ == expectedSignal;
        if (not expectedEvent.empty()) {
            pred = pred and callData.event_ == expectedEvent;
        }
        return pred;
    });

    if (not res) {
        std::string msg(expectedSignal);
        if (not expectedEvent.empty()) {
            msg.append("::");
            msg.append(expectedEvent);
        }

        JAMI_ERR("Waiting for Signal [%s] timed-out!", expectedSignal.c_str());
    }

    return res;
}
void
MediaControlTest::testWithScenario(CallData& aliceData,
                                   CallData& bobData,
                                   const TestScenario& scenario)
{
    JAMI_INFO("ALICE [%s] calls Bob [%s] and wait for BOB to answer",
              aliceData.account_->getAccountID().c_str(),
              bobData.account_->getAccountID().c_str());

    aliceData.sipCall_ = std::dynamic_pointer_cast<SIPCall>(
        aliceData.account_->newOutgoingCall(bobData.userName_, scenario.offer_));
    assert(aliceData.sipCall_);

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData.sipCall_->getCallId().c_str());

    {
        // TODO. Must check against scenario data.

        // Validate Alice's SDP
        auto aliceLocalMedia = aliceData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(scenario.offer_.size(), aliceLocalMedia.size());
        CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, aliceLocalMedia[1].direction_);

        // Validate Bob's SDP
        auto bobLocalMedia = bobData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(scenario.answer_.size(), bobLocalMedia.size());

        CPPUNIT_ASSERT(bobLocalMedia[0].enabled);
        CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_AUDIO, bobLocalMedia[0].type);
        CPPUNIT_ASSERT(not bobLocalMedia[0].onHold);
        CPPUNIT_ASSERT(bobLocalMedia[0].addr);
        CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, bobLocalMedia[1].direction_);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Update the media
    auto mediaListUpdate = MediaAttribute::mediaAttributesToMediaMaps(scenario.offerUpdate_);
    Manager::instance().requestMediaChange(aliceData.sipCall_->getCallId(), mediaListUpdate);

    // Wait for the VideoMute signal.
    JAMI_INFO("Waiting for the video muted signal");

    CPPUNIT_ASSERT_EQUAL(true, waitForSignal(aliceData, DRing::CallSignal::VideoMuted::name));

    {
        // Validate Alice's SDP
        auto aliceLocalMedia = aliceData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(scenario.offerUpdate_.size(), aliceLocalMedia.size());
        CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, aliceLocalMedia[0].direction_);

        // Validate Bob's SDP
        auto bobLocalMedia = bobData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(scenario.answerUpdate_.size(), bobLocalMedia.size());
        CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, bobLocalMedia[0].direction_);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData.sipCall_->getCallId());

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
MediaControlTest::testCallWithMediaList()
{
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> signalHandlers;

    CallData aliceData;
    aliceData.account_ = Manager::instance().getAccount<JamiAccount>(aliceAccountId_);
    aliceData.userName_ = aliceData.account_->getAccountDetails()[ConfProperties::USERNAME];
    aliceData.alias_ = aliceData.account_->getAccountDetails()[ConfProperties::ALIAS];

    CallData bobData;
    bobData.account_ = Manager::instance().getAccount<JamiAccount>(bobAccountId_);
    bobData.userName_ = bobData.account_->getAccountDetails()[ConfProperties::USERNAME];
    bobData.alias_ = bobData.account_->getAccountDetails()[ConfProperties::ALIAS];

    // Insert needed signal handlers.
    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string& accountId, const std::string& callId, const std::string&) {
            onIncomingCall(accountId, callId, bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            onCallStateChange(callId, state, aliceData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) { onVideoMuted(callId, muted, aliceData); }));

    DRing::registerSignalHandlers(signalHandlers);

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "main audio";

    MediaAttribute defaultVideo(MediaType::MEDIA_VIDEO);
    defaultVideo.label_ = "main video";

    {
        MediaAttribute audio(defaultAudio);
        MediaAttribute video(defaultVideo);

        TestScenario scenario;
        // First offer/answer
        scenario.offer_.emplace_back(audio);
        scenario.offer_.emplace_back(video);
        scenario.answer_.emplace_back(audio);
        scenario.answer_.emplace_back(video);

        // Updated offer/answer
        video.muted_ = true;
        scenario.offerUpdate_.emplace_back(audio);
        scenario.offerUpdate_.emplace_back(video);
        scenario.answerUpdate_.emplace_back(audio);
        scenario.answerUpdate_.emplace_back(video);

        testWithScenario(aliceData, bobData, scenario);
    }
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MediaControlTest::name())
