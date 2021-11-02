/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#include "callmanager_interface.h"
#include "manager.h"
#include "sip/sipaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "jami/media_const.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "sip/sdp.h"

using namespace DRing::Account;
using namespace DRing::Call;

namespace jami {
namespace test {

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
    uint16_t listeningPort_ {0};
    std::string userName_ {};
    std::string alias_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
};

/**
 * Call tests for SIP accounts.
 */
class SipBasicCallTest : public CppUnit::TestFixture
{
public:
    SipBasicCallTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~SipBasicCallTest() { DRing::fini(); }

    static std::string name() { return "SipBasicCallTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void audio_only_test();
    void audio_video_test();
    void peer_answer_with_all_media_disabled();
    void hold_resume_test();
    void blind_transfer_test();

    CPPUNIT_TEST_SUITE(SipBasicCallTest);
    CPPUNIT_TEST(audio_only_test);
    CPPUNIT_TEST(audio_video_test);
    // Test when the peer answers with all the media disabled (RTP port = 0)
    CPPUNIT_TEST(peer_answer_with_all_media_disabled);
    CPPUNIT_TEST(hold_resume_test);
    CPPUNIT_TEST(blind_transfer_test);
    CPPUNIT_TEST_SUITE_END();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<DRing::MediaMap> mediaList,
                                        CallData& callData);
    static void onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData);

    // Helpers
    void audio_video_call(std::vector<MediaAttribute> offer,
                          std::vector<MediaAttribute> answer,
                          bool expectedToSucceed = true,
                          bool validateMedia = true);
    static void configureTest(CallData& bob, CallData& alice, CallData& carla);
    static std::string getUserAlias(const std::string& callId);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

private:
    CallData aliceData_;
    CallData bobData_;
    CallData carlaData_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SipBasicCallTest, SipBasicCallTest::name());

void
SipBasicCallTest::setUp()
{
    aliceData_.listeningPort_ = 5080;
    std::map<std::string, std::string> details = DRing::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::USERNAME] = "ALICE";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::LOCAL_PORT] = std::to_string(aliceData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    aliceData_.accountId_ = Manager::instance().addAccount(details);

    bobData_.listeningPort_ = 5082;
    details = DRing::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::USERNAME] = "BOB";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::LOCAL_PORT] = std::to_string(bobData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    bobData_.accountId_ = Manager::instance().addAccount(details);

    carlaData_.listeningPort_ = 5084;
    details = DRing::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::USERNAME] = "CARLA";
    details[ConfProperties::DISPLAYNAME] = "CARLA";
    details[ConfProperties::ALIAS] = "CARLA";
    details[ConfProperties::LOCAL_PORT] = std::to_string(carlaData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    carlaData_.accountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize accounts ...");
    auto aliceAccount = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto bobAccount = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);
    auto carlaAccount = Manager::instance().getAccount<SIPAccount>(carlaData_.accountId_);
}

void
SipBasicCallTest::tearDown()
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

    Manager::instance().removeAccount(aliceData_.accountId_, true);
    Manager::instance().removeAccount(bobData_.accountId_, true);
    Manager::instance().removeAccount(carlaData_.accountId_, true);

    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}

std::string
SipBasicCallTest::getUserAlias(const std::string& callId)
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
SipBasicCallTest::onIncomingCallWithMedia(const std::string& accountId,
                                          const std::string& callId,
                                          const std::vector<DRing::MediaMap> mediaList,
                                          CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - media count [%lu]",
              DRing::CallSignal::IncomingCallWithMedia::name,
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
    callData.signals_.emplace_back(CallData::Signal(DRing::CallSignal::IncomingCallWithMedia::name));

    callData.cv_.notify_one();
}

void
SipBasicCallTest::onCallStateChange(const std::string&,
                                    const std::string& callId,
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
              DRing::CallSignal::StateChange::name,
              callData.alias_.c_str(),
              callId.c_str(),
              state.c_str());

    if (account->getAccountID() != callData.accountId_)
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(DRing::CallSignal::StateChange::name, state));
    }
    // NOTE. Only states that we are interested on will notify the CV. If this
    // unit test is modified to process other states, they must be added here.
    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP" or state == "RINGING") {
        callData.cv_.notify_one();
    }
}

void
SipBasicCallTest::onMediaNegotiationStatus(const std::string& callId,
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
              DRing::CallSignal::MediaNegotiationStatus::name,
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              event.c_str());

    if (account->getAccountID() != callData.accountId_)
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(DRing::CallSignal::MediaNegotiationStatus::name, event));
    }

    callData.cv_.notify_one();
}

bool
SipBasicCallTest::waitForSignal(CallData& callData,
                                const std::string& expectedSignal,
                                const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {10};
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
SipBasicCallTest::configureTest(CallData& aliceData, CallData& bobData, CallData& carlaData)
{
    {
        CPPUNIT_ASSERT(not aliceData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(aliceData.accountId_);
        aliceData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(aliceData.listeningPort_);
        account->enableIceForMedia(true);
    }

    {
        CPPUNIT_ASSERT(not bobData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(bobData.accountId_);
        bobData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(bobData.listeningPort_);
    }
#if 1
    {
        CPPUNIT_ASSERT(not carlaData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(carlaData.accountId_);
        carlaData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        carlaData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(carlaData.listeningPort_);
    }
#endif

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> signalHandlers;

    // Insert needed signal handlers.
    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<DRing::MediaMap> mediaList) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onIncomingCallWithMedia(accountId,
                                        callId,
                                        mediaList,
                                        user == aliceData.alias_
                                            ? aliceData
                                            : (user == bobData.alias_ ? bobData : carlaData));
        }));

    signalHandlers.insert(
        DRing::exportable_callback<DRing::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onCallStateChange(accountId,
                                  callId,
                                  state,
                                  user == aliceData.alias_
                                      ? aliceData
                                      : (user == bobData.alias_ ? bobData : carlaData));
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>& /* mediaList */) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaNegotiationStatus(callId,
                                         event,
                                         user == aliceData.alias_
                                             ? aliceData
                                             : (user == bobData.alias_ ? bobData : carlaData));
        }));

    DRing::registerSignalHandlers(signalHandlers);
}

void
SipBasicCallTest::audio_video_call(std::vector<MediaAttribute> offer,
                                   std::vector<MediaAttribute> answer,
                                   bool expectedToSucceed,
                                   bool validateMedia)
{
    configureTest(aliceData_, bobData_, carlaData_);

    JAMI_INFO("=== Start a call and validate ===");

    std::string bobUri = bobData_.userName_
                         + "@127.0.0.1:" + std::to_string(bobData_.listeningPort_);

    aliceData_.callId_ = DRing::placeCallWithMedia(aliceData_.accountId_,
                                                   bobUri,
                                                   MediaAttribute::mediaAttributesToMediaMaps(
                                                       offer));

    CPPUNIT_ASSERT(not aliceData_.callId_.empty());

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData_.accountId_.c_str(),
              bobData_.accountId_.c_str());

    // Give it some time to ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Wait for call to be processed.
    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_, DRing::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    DRing::acceptWithMedia(bobData_.accountId_,
                           bobData_.callId_,
                           MediaAttribute::mediaAttributesToMediaMaps(answer));

    if (expectedToSucceed) {
        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(bobData_,
                          DRing::CallSignal::MediaNegotiationStatus::name,
                          DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(
            waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

        JAMI_INFO("BOB answered the call [%s]", bobData_.callId_.c_str());

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_,
                          DRing::CallSignal::MediaNegotiationStatus::name,
                          DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        // Validate Alice's media
        if (validateMedia) {
            auto call = Manager::instance().getCallFromCallID(aliceData_.callId_);
            auto activeMediaList = call->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(offer.size(), activeMediaList.size());
            // Audio
            CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_AUDIO, activeMediaList[0].type_);
            CPPUNIT_ASSERT_EQUAL(offer[0].enabled_, activeMediaList[0].enabled_);

            // Video
            if (offer.size() > 1) {
                CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_VIDEO, activeMediaList[1].type_);
                CPPUNIT_ASSERT_EQUAL(offer[1].enabled_, activeMediaList[1].enabled_);
            }
        }

        // Validate Bob's media
        if (validateMedia) {
            auto call = Manager::instance().getCallFromCallID(bobData_.callId_);
            auto activeMediaList = call->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(answer.size(), activeMediaList.size());
            // Audio
            CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_AUDIO, activeMediaList[0].type_);
            CPPUNIT_ASSERT_EQUAL(answer[0].enabled_, activeMediaList[0].enabled_);

            // Video
            if (offer.size() > 1) {
                CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_VIDEO, activeMediaList[1].type_);
                CPPUNIT_ASSERT_EQUAL(answer[1].enabled_, activeMediaList[1].enabled_);
            }
        }

        // Give some time to media to start and flow
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Bob hang-up.
        JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
        DRing::hangUp(bobData_.accountId_, bobData_.callId_);
    } else {
        // The media negotiation for the call is expected to fail, so we
        // should receive the signal.
        CPPUNIT_ASSERT(
            waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::FAILURE));
    }

    // The hang-up signal will be emitted on caller's side (Alice) in both
    // success failure scenarios.

    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
SipBasicCallTest::audio_only_test()
{
    // Test with video enabled on Alice's side and disabled
    // on Bob's side.

    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);

    std::vector<MediaAttribute> offer;
    std::vector<MediaAttribute> answer;

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    MediaAttribute video(MediaType::MEDIA_VIDEO);

    // Configure Alice
    audio.enabled_ = true;
    audio.label_ = "audio_0";
    offer.emplace_back(audio);

    video.enabled_ = true;
    video.label_ = "video_0";
    aliceAcc->enableVideo(true);
    offer.emplace_back(video);

    // Configure Bob
    audio.enabled_ = true;
    audio.label_ = "audio_0";
    answer.emplace_back(audio);

    video.enabled_ = false;
    video.label_ = "video_0";
    bobAcc->enableVideo(false);
    answer.emplace_back(video);

    // Run the scenario
    audio_video_call(offer, answer);
}

void
SipBasicCallTest::audio_video_test()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);

    std::vector<MediaAttribute> offer;
    std::vector<MediaAttribute> answer;

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    MediaAttribute video(MediaType::MEDIA_VIDEO);

    // Configure Alice
    audio.enabled_ = true;
    audio.label_ = "audio_0";
    offer.emplace_back(audio);

    video.enabled_ = true;
    video.label_ = "video_0";
    aliceAcc->enableVideo(true);
    offer.emplace_back(video);

    // Configure Bob
    audio.enabled_ = true;
    audio.label_ = "audio_0";
    answer.emplace_back(audio);

    video.enabled_ = true;
    video.label_ = "video_0";
    bobAcc->enableVideo(true);
    answer.emplace_back(video);

    // Run the scenario
    audio_video_call(offer, answer);
}

void
SipBasicCallTest::peer_answer_with_all_media_disabled()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);

    std::vector<MediaAttribute> offer;
    std::vector<MediaAttribute> answer;

    MediaAttribute video(MediaType::MEDIA_VIDEO);

    // Configure Alice
    video.enabled_ = true;
    video.label_ = "video_0";
    video.secure_ = false;
    offer.emplace_back(video);

    // Configure Bob
    video.enabled_ = false;
    video.label_ = "video_0";
    video.secure_ = false;
    bobAcc->enableVideo(false);
    answer.emplace_back(video);

    // Run the scenario
    audio_video_call(offer, answer, false, false);
}

void
SipBasicCallTest::hold_resume_test()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);

    std::vector<MediaAttribute> offer;
    std::vector<MediaAttribute> answer;

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    MediaAttribute video(MediaType::MEDIA_VIDEO);

    audio.enabled_ = true;
    audio.label_ = "audio_0";

    // Alice's media
    offer.emplace_back(audio);

    // Bob's media
    answer.emplace_back(audio);

    {
        configureTest(aliceData_, bobData_, carlaData_);

        JAMI_INFO("=== Start a call and validate ===");

        std::string bobUri = bobData_.userName_
                             + "@127.0.0.1:" + std::to_string(bobData_.listeningPort_);

        aliceData_.callId_ = DRing::placeCallWithMedia(aliceData_.accountId_,
                                                       bobUri,
                                                       MediaAttribute::mediaAttributesToMediaMaps(
                                                           offer));

        CPPUNIT_ASSERT(not aliceData_.callId_.empty());

        JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
                  aliceData_.accountId_.c_str(),
                  bobData_.accountId_.c_str());

        // Give it some time to ring
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Wait for call to be processed.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::RINGING));

        // Wait for incoming call signal.
        CPPUNIT_ASSERT(waitForSignal(bobData_, DRing::CallSignal::IncomingCallWithMedia::name));

        // Answer the call.
        DRing::acceptWithMedia(bobData_.accountId_,
                               bobData_.callId_,
                               MediaAttribute::mediaAttributesToMediaMaps(answer));

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(bobData_,
                          DRing::CallSignal::MediaNegotiationStatus::name,
                          DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(
            waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

        JAMI_INFO("BOB answered the call [%s]", bobData_.callId_.c_str());

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_,
                          DRing::CallSignal::MediaNegotiationStatus::name,
                          DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Validate Alice's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(aliceData_.callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::SENDRECV);
                CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::SENDRECV);
            }
        }

        // Validate Bob's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(bobData_.callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::SENDRECV);
                CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::SENDRECV);
            }
        }

        // Give some time to media to start and flow
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Hold/Resume the call
        JAMI_INFO("Hold Alice's call");
        DRing::hold(aliceData_.accountId_, aliceData_.callId_);
        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::HOLD));

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_,
                          DRing::CallSignal::MediaNegotiationStatus::name,
                          DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        {
            // Validate hold state.
            auto call = Manager::instance().getCallFromCallID(aliceData_.callId_);
            auto activeMediaList = call->getMediaAttributeList();
            for (const auto& mediaAttr : activeMediaList) {
                CPPUNIT_ASSERT(mediaAttr.onHold_);
            }
        }

        // Validate Alice's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(aliceData_.callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::SENDONLY);
                CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::RECVONLY);
            }
        }

        // Validate Bob's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(bobData_.callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::RECVONLY);
                CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::SENDONLY);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));

        JAMI_INFO("Resume Alice's call");
        DRing::unhold(aliceData_.accountId_, aliceData_.callId_);

        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(aliceData_,
                          DRing::CallSignal::MediaNegotiationStatus::name,
                          DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        std::this_thread::sleep_for(std::chrono::seconds(2));

        {
            // Validate hold state.
            auto call = Manager::instance().getCallFromCallID(aliceData_.callId_);
            auto activeMediaList = call->getMediaAttributeList();
            for (const auto& mediaAttr : activeMediaList) {
                CPPUNIT_ASSERT(not mediaAttr.onHold_);
            }
        }

        // Bob hang-up.
        JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
        DRing::hangUp(bobData_.accountId_, bobData_.callId_);

        // The hang-up signal will be emitted on caller's side (Alice) in both
        // success and failure scenarios.

        CPPUNIT_ASSERT(
            waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::HUNGUP));

        JAMI_INFO("Call terminated on both sides");
    }
}

void
SipBasicCallTest::blind_transfer_test()
{
    // Test a "blind" (a.k.a. unattended) transfer as described in
    // https://datatracker.ietf.org/doc/html/rfc5589

    /** Call transfer scenario:
     *
     * Alice and Bob are in an active call
     * Alice performs a call transfer (SIP REFER method) to Carla
     * Bob automatically accepts the transfer request
     * Alice ends the call with Bob
     * Bob send a new call invite to Carla
     * Carla accepts the call
     * Carl ends the call
     *
     * Here is a simplified version of a call flow from
     * rfc5589
     *
     *
       Alice                     Bob                      Carla
         |      REFER             |                        |
         |----------------------->|                        |
         |      202 Accepted      |                        |
         |<-----------------------|                        |
         |      BYE               |                        |
         |<-----------------------|                        |
         |      200 OK            |                        |
         |----------------------->|                        |
         |                        |     INVITE             |
         |                        |----------------------->|
         |                        |     200 OK             |
         |                        |<-----------------------|
     */

    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);
    auto const carlaAcc = Manager::instance().getAccount<SIPAccount>(carlaData_.accountId_);

    aliceAcc->enableIceForMedia(false);
    bobAcc->enableIceForMedia(false);
    carlaAcc->enableIceForMedia(false);

    std::vector<MediaAttribute> offer;
    std::vector<MediaAttribute> answer;

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    MediaAttribute video(MediaType::MEDIA_VIDEO);

    audio.enabled_ = true;
    audio.label_ = "audio_0";

    // Alice's media
    offer.emplace_back(audio);

    // Bob's media
    answer.emplace_back(audio);

    configureTest(aliceData_, bobData_, carlaData_);

    JAMI_INFO("=== Start a call and validate ===");

    std::string bobUri = bobData_.userName_
                         + "@127.0.0.1:" + std::to_string(bobData_.listeningPort_);

    aliceData_.callId_ = DRing::placeCallWithMedia(aliceData_.accountId_,
                                                   bobUri,
                                                   MediaAttribute::mediaAttributesToMediaMaps(
                                                       offer));

    CPPUNIT_ASSERT(not aliceData_.callId_.empty());

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData_.accountId_.c_str(),
              bobData_.accountId_.c_str());

    // Give it some time to ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Wait for call to be processed.
    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_, DRing::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    DRing::acceptWithMedia(bobData_.callId_, MediaAttribute::mediaAttributesToMediaMaps(answer));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_,
                                 DRing::CallSignal::MediaNegotiationStatus::name,
                                 DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(
        waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData_.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(aliceData_,
                                 DRing::CallSignal::MediaNegotiationStatus::name,
                                 DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Give some time to media to start and flow
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Transfer the call to Carla
    std::string carlaUri = carlaAcc->getUsername()
                           + "@127.0.0.1:" + std::to_string(carlaData_.listeningPort_);

    DRing::transfer(aliceData_.callId_, carlaUri); // TODO. Check trim

    // Expect Alice's call to end.
    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, DRing::CallSignal::StateChange::name, StateEvent::HUNGUP));

    // Wait for the new call to be processed.
    CPPUNIT_ASSERT(
        waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(carlaData_, DRing::CallSignal::IncomingCallWithMedia::name));

    // Let it ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Carla answers the call.
    DRing::acceptWithMedia(carlaData_.callId_, MediaAttribute::mediaAttributesToMediaMaps(answer));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(carlaData_,
                                 DRing::CallSignal::MediaNegotiationStatus::name,
                                 DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(
        waitForSignal(carlaData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

    JAMI_INFO("CARLA answered the call [%s]", bobData_.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_,
                                 DRing::CallSignal::MediaNegotiationStatus::name,
                                 DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(
        waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

    // Validate Carla's side of media direction
    {
        auto call = std::static_pointer_cast<SIPCall>(
            Manager::instance().getCallFromCallID(carlaData_.callId_));
        auto& sdp = call->getSDP();
        auto mediaStreams = sdp.getMediaSlots();
        for (auto const& media : mediaStreams) {
            CPPUNIT_ASSERT_EQUAL(media.first.direction_, MediaDirection::SENDRECV);
            CPPUNIT_ASSERT_EQUAL(media.second.direction_, MediaDirection::SENDRECV);
        }
    }

    // NOTE:
    // For now, we dont validate Bob's media because currently
    // test does not update BOB's call ID (bobData_.callId_
    // still point to the first call).
    // It seems there is no easy way to get the ID of the new call
    // made by Bob to Carla.

    // Give some time to media to start and flow
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Bob hang-up.
    JAMI_INFO("Hang up CARLA's call and wait for CARLA to hang up");
    DRing::hangUp(carlaData_.callId_);

    // Expect end call on Carla's side.
    CPPUNIT_ASSERT(
        waitForSignal(carlaData_, DRing::CallSignal::StateChange::name, StateEvent::HUNGUP));

    JAMI_INFO("Calls normally ended on both sides");
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SipBasicCallTest::name())
