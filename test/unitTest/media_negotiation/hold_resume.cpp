/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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
#include "connectivity/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "jami/media_const.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "sip/sdp.h"

#include "common.h"

using namespace libjami::Account;
using namespace libjami::Call;

namespace jami {
namespace test {

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
    // Determine if we should expect the MediaNegotiationStatus signal.
    bool expectMediaRenegotiation_ {false};
};

struct CallData
{
    struct Signal
    {
        Signal(const std::string& name, const std::string& event = {})
            : name_(std::move(name))
            , event_(std::move(event)) {};

        std::string name_ {};
        std::string event_ {};
    };

    std::string accountId_ {};
    std::string userName_ {};
    std::string alias_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
};

/**
 * Basic tests for media negotiation.
 */
class HoldResumeTest : public CppUnit::TestFixture
{
public:
    HoldResumeTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~HoldResumeTest() { libjami::fini(); }

    static std::string name() { return "HoldResumeTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void audio_and_video_then_hold_resume();
    void audio_only_then_hold_resume();

    CPPUNIT_TEST_SUITE(HoldResumeTest);
    CPPUNIT_TEST(audio_and_video_then_hold_resume);
    CPPUNIT_TEST(audio_only_then_hold_resume);
    CPPUNIT_TEST_SUITE_END();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData);
    // For backward compatibility test cases
    static void onIncomingCall(const std::string& accountId,
                               const std::string& callId,
                               CallData& callData);
    static void onMediaChangeRequested(const std::string& accountId,
                                       const std::string& callId,
                                       const std::vector<libjami::MediaMap> mediaList,
                                       CallData& callData);
    static void onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData);

    // Helpers
    static void configureScenario(CallData& bob, CallData& alice);
    void testWithScenario(CallData& aliceData, CallData& bobData, const TestScenario& scenario);
    static std::string getUserAlias(const std::string& callId);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

private:
    CallData aliceData_;
    CallData bobData_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HoldResumeTest, HoldResumeTest::name());

void
HoldResumeTest::setUp()
{
    auto actors = load_actors("actors/alice-bob-no-upnp.yml");

    aliceData_.accountId_ = actors["alice"];
    bobData_.accountId_ = actors["bob"];

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData_.accountId_);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData_.accountId_);

    wait_for_announcement_of({aliceAccount->getAccountID(), bobAccount->getAccountID()});
}

void
HoldResumeTest::tearDown()
{
    wait_for_removal_of({aliceData_.accountId_, bobData_.accountId_});
}

std::string
HoldResumeTest::getUserAlias(const std::string& callId)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    if (not call) {
        JAMI_WARN("Call with ID [%s] does not exist anymore!", callId.c_str());
        return {};
    }

    auto const& account = call->getAccount().lock();
    if (not account) {
        return {};
    }

    return account->getAccountDetails()[ConfProperties::ALIAS];
}

void
HoldResumeTest::onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - media count [%lu]",
              libjami::CallSignal::IncomingCallWithMedia::name,
              callData.alias_.c_str(),
              callId.c_str(),
              mediaList.size());

    if (not Manager::instance().getCallFromCallID(callId)) {
        JAMI_WARN("Call with ID [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::IncomingCallWithMedia::name));

    callData.cv_.notify_one();
}

void
HoldResumeTest::onIncomingCall(const std::string& accountId,
                               const std::string& callId,
                               CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s]",
              libjami::CallSignal::IncomingCall::name,
              callData.alias_.c_str(),
              callId.c_str());

    if (not Manager::instance().getCallFromCallID(callId)) {
        JAMI_WARN("Call with ID [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::IncomingCall::name));

    callData.cv_.notify_one();
}

void
HoldResumeTest::onMediaChangeRequested(const std::string& accountId,
                                       const std::string& callId,
                                       const std::vector<libjami::MediaMap> mediaList,
                                       CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - media count [%lu]",
              libjami::CallSignal::MediaChangeRequested::name,
              callData.alias_.c_str(),
              callId.c_str(),
              mediaList.size());

    if (not Manager::instance().getCallFromCallID(callId)) {
        JAMI_WARN("Call with ID [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::MediaChangeRequested::name));

    callData.cv_.notify_one();
}

void
HoldResumeTest::onCallStateChange(const std::string& callId,
                                  const std::string& state,
                                  CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_WARN("Call with ID [%s] does not exist anymore!", callId.c_str());
        return;
    }

    auto account = call->getAccount().lock();
    if (not account) {
        JAMI_WARN("Account owning the call [%s] does not exist!", callId.c_str());
        return;
    }

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              libjami::CallSignal::StateChange::name,
              callData.alias_.c_str(),
              callId.c_str(),
              state.c_str());

    if (account->getAccountID() != callData.accountId_)
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(libjami::CallSignal::StateChange::name, state));
    }

    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP") {
        callData.cv_.notify_one();
    }
}

void
HoldResumeTest::onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_WARN("Call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    auto account = call->getAccount().lock();
    if (not account) {
        JAMI_WARN("Account owning the call [%s] does not exist!", callId.c_str());
        return;
    }

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              libjami::CallSignal::MediaNegotiationStatus::name,
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              event.c_str());

    if (account->getAccountID() != callData.accountId_)
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(libjami::CallSignal::MediaNegotiationStatus::name, event));
    }

    callData.cv_.notify_one();
}

bool
HoldResumeTest::waitForSignal(CallData& callData,
                              const std::string& expectedSignal,
                              const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {30};
    std::unique_lock<std::mutex> lock {callData.mtx_};

    // Combined signal + event (if any).
    std::string sigEvent(expectedSignal);
    if (not expectedEvent.empty())
        sigEvent += "::" + expectedEvent;

    JAMI_INFO("[%s] is waiting for [%s] signal/event", callData.alias_.c_str(), sigEvent.c_str());

    auto res = callData.cv_.wait_for(lock, TIME_OUT, [&] {
        // Search for the expected signal in list of received signals.
        bool pred = false;
        for (auto it = callData.signals_.begin(); it != callData.signals_.end(); it++) {
            // The predicate is true if the signal names match, and if the
            // expectedEvent is not empty, the events must also match.
            if (it->name_ == expectedSignal
                and (expectedEvent.empty() or it->event_ == expectedEvent)) {
                pred = true;
                // Done with this signal.
                callData.signals_.erase(it);
                break;
            }
        }

        return pred;
    });

    if (not res) {
        JAMI_ERR("[%s] waiting for signal/event [%s] timed-out!",
                 callData.alias_.c_str(),
                 sigEvent.c_str());

        JAMI_INFO("[%s] currently has the following signals:", callData.alias_.c_str());

        for (auto const& sig : callData.signals_) {
            JAMI_INFO() << "Signal [" << sig.name_
                        << (sig.event_.empty() ? "" : ("::" + sig.event_)) << "]";
        }
    }

    return res;
}

void
HoldResumeTest::configureScenario(CallData& aliceData, CallData& bobData)
{
    {
        CPPUNIT_ASSERT(not aliceData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<JamiAccount>(aliceData.accountId_);
        aliceData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
    }

    {
        CPPUNIT_ASSERT(not bobData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<JamiAccount>(bobData.accountId_);
        bobData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
    }

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> signalHandlers;

    // Insert needed signal handlers.
    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<libjami::MediaMap> mediaList) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onIncomingCallWithMedia(accountId,
                                        callId,
                                        mediaList,
                                        user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
        [&](const std::string& accountId, const std::string& callId, const std::string&) {
            auto user = getUserAlias(callId);
            if (not user.empty()) {
                onIncomingCall(accountId, callId, user == aliceData.alias_ ? aliceData : bobData);
            }
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaChangeRequested>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::vector<libjami::MediaMap> mediaList) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaChangeRequested(accountId,
                                       callId,
                                       mediaList,
                                       user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string& callId, const std::string& state, signed) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onCallStateChange(callId, state, user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>&) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaNegotiationStatus(callId,
                                         event,
                                         user == aliceData.alias_ ? aliceData : bobData);
        }));

    libjami::registerSignalHandlers(signalHandlers);
}

void
HoldResumeTest::testWithScenario(CallData& aliceData,
                                 CallData& bobData,
                                 const TestScenario& scenario)
{
    JAMI_INFO("=== Start a call and validate ===");

    // The media count of the offer and answer must match (RFC-3264).
    auto mediaCount = scenario.offer_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCount, scenario.answer_.size());

    auto const& aliceCall = std::dynamic_pointer_cast<SIPCall>(
        (Manager::instance().getAccount<JamiAccount>(aliceData.accountId_))
            ->newOutgoingCall(bobData.userName_,
                              MediaAttribute::mediaAttributesToMediaMaps(scenario.offer_)));
    CPPUNIT_ASSERT(aliceCall);
    aliceData.callId_ = aliceCall->getCallId();

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData.accountId_.c_str(),
              bobData.accountId_.c_str());

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData, libjami::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.answer_);
        Manager::instance().answerCall(bobData.accountId_, bobData.callId_, mediaList);
    }

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(
        true,
        waitForSignal(bobData,
                      libjami::CallSignal::MediaNegotiationStatus::name,
                      libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
    // Wait for the StateChange signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData,
                                       libjami::CallSignal::StateChange::name,
                                       StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(
        true,
        waitForSignal(aliceData,
                      libjami::CallSignal::MediaNegotiationStatus::name,
                      libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Validate Alice's media and SDP
    {
        auto mediaAttr = aliceCall->getMediaAttributeList();
        CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
        for (size_t idx = 0; idx < mediaCount; idx++) {
            CPPUNIT_ASSERT_EQUAL(scenario.offer_[idx].muted_, mediaAttr[idx].muted_);
        }
        // Check media direction
        auto& sdp = aliceCall->getSDP();
        auto mediaStreams = sdp.getMediaSlots();
        for (auto const& media : mediaStreams) {
            CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::SENDRECV);
            CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::SENDRECV);
        }
    }

    // Validate Bob's media
    {
        auto const& bobCall = std::dynamic_pointer_cast<SIPCall>(
            Manager::instance().getCallFromCallID(bobData.callId_));
        auto mediaAttr = bobCall->getMediaAttributeList();
        CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
        for (size_t idx = 0; idx < mediaCount; idx++) {
            CPPUNIT_ASSERT_EQUAL(scenario.answer_[idx].muted_, mediaAttr[idx].muted_);
        }
        // Check media direction
        auto& sdp = bobCall->getSDP();
        auto mediaStreams = sdp.getMediaSlots();
        for (auto const& media : mediaStreams) {
            CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::SENDRECV);
            CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::SENDRECV);
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    JAMI_INFO("=== Hold the call and validate ===");
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.offerUpdate_);
        libjami::hold(aliceData.accountId_, aliceData.callId_);
    }

    // Update and validate media count.
    mediaCount = scenario.offerUpdate_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCount, scenario.answerUpdate_.size());

    if (scenario.expectMediaRenegotiation_) {
        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT_EQUAL(
            true,
            waitForSignal(aliceData,
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Validate Alice's media
        {
            auto mediaAttr = aliceCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
            for (size_t idx = 0; idx < mediaCount; idx++) {
                CPPUNIT_ASSERT(mediaAttr[idx].onHold_);
                CPPUNIT_ASSERT_EQUAL(scenario.offerUpdate_[idx].muted_, mediaAttr[idx].muted_);
                // Check isCaptureDeviceMuted API
                CPPUNIT_ASSERT_EQUAL(mediaAttr[idx].muted_,
                                     aliceCall->isCaptureDeviceMuted(mediaAttr[idx].type_));
            }
        }

        // Validate Bob's media
        {
            auto const& bobCall = std::dynamic_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(bobData.callId_));
            auto mediaAttr = bobCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
            for (size_t idx = 0; idx < mediaCount; idx++) {
                CPPUNIT_ASSERT(not mediaAttr[idx].onHold_);
                CPPUNIT_ASSERT_EQUAL(scenario.answerUpdate_[idx].muted_, mediaAttr[idx].muted_);
                // Check isCaptureDeviceMuted API
                CPPUNIT_ASSERT_EQUAL(mediaAttr[idx].muted_,
                                     bobCall->isCaptureDeviceMuted(mediaAttr[idx].type_));
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    JAMI_INFO("=== Resume the call and validate ===");
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.offerUpdate_);
        libjami::unhold(aliceData.accountId_, aliceData.callId_);
    }

    // Update and validate media count.
    mediaCount = scenario.offerUpdate_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCount, scenario.answerUpdate_.size());

    if (scenario.expectMediaRenegotiation_) {
        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT_EQUAL(
            true,
            waitForSignal(aliceData,
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        // Validate Alice's media
        {
            auto mediaAttr = aliceCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
            for (size_t idx = 0; idx < mediaCount; idx++) {
                CPPUNIT_ASSERT(not mediaAttr[idx].onHold_);
                CPPUNIT_ASSERT_EQUAL(scenario.offerUpdate_[idx].muted_, mediaAttr[idx].muted_);
                // Check isCaptureDeviceMuted API
                CPPUNIT_ASSERT_EQUAL(mediaAttr[idx].muted_,
                                     aliceCall->isCaptureDeviceMuted(mediaAttr[idx].type_));
            }
        }

        // Validate Bob's media
        {
            auto const& bobCall = std::dynamic_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(bobData.callId_));
            auto mediaAttr = bobCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
            for (size_t idx = 0; idx < mediaCount; idx++) {
                CPPUNIT_ASSERT(not mediaAttr[idx].onHold_);
                CPPUNIT_ASSERT_EQUAL(scenario.answerUpdate_[idx].muted_, mediaAttr[idx].muted_);
                // Check isCaptureDeviceMuted API
                CPPUNIT_ASSERT_EQUAL(mediaAttr[idx].muted_,
                                     bobCall->isCaptureDeviceMuted(mediaAttr[idx].type_));
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData.accountId_, bobData.callId_);

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       libjami::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
HoldResumeTest::audio_and_video_then_hold_resume()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario(aliceData_, bobData_);

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "audio_0";
    defaultAudio.enabled_ = true;

    MediaAttribute defaultVideo(MediaType::MEDIA_VIDEO);
    defaultVideo.label_ = "video_0";
    defaultVideo.enabled_ = true;

    MediaAttribute audio(defaultAudio);
    MediaAttribute video(defaultVideo);

    TestScenario scenario;
    // First offer/answer
    scenario.offer_.emplace_back(audio);
    scenario.offer_.emplace_back(video);
    scenario.answer_.emplace_back(audio);
    scenario.answer_.emplace_back(video);

    // Updated offer/answer
    scenario.offerUpdate_.emplace_back(audio);
    scenario.offerUpdate_.emplace_back(video);

    scenario.answerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(video);
    scenario.expectMediaRenegotiation_ = true;

    testWithScenario(aliceData_, bobData_, scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
HoldResumeTest::audio_only_then_hold_resume()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario(aliceData_, bobData_);

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "audio_0";
    defaultAudio.enabled_ = true;

    MediaAttribute audio(defaultAudio);

    TestScenario scenario;
    // First offer/answer
    scenario.offer_.emplace_back(audio);
    scenario.answer_.emplace_back(audio);

    // Updated offer/answer
    scenario.offerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(audio);
    scenario.expectMediaRenegotiation_ = true;

    testWithScenario(aliceData_, bobData_, scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::HoldResumeTest::name())
