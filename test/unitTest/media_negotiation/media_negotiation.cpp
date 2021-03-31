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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "sip/sdp.h"

using namespace DRing::Account;
using namespace DRing::Call;

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
    // Determine if we should expect the MediaChangeRequested signal.
    bool expectMediaChangeRequest_ {false};
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
class MediaNegotiationTest : public CppUnit::TestFixture
{
public:
    MediaNegotiationTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~MediaNegotiationTest() { DRing::fini(); }

    static std::string name() { return "MediaNegotiationTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void audio_and_video_then_mute_video();
    void audio_only_then_add_video();
    void audio_and_video_then_mute_audio();

    CPPUNIT_TEST_SUITE(MediaNegotiationTest);
    CPPUNIT_TEST(audio_and_video_then_mute_video);
    CPPUNIT_TEST(audio_only_then_add_video);
    CPPUNIT_TEST(audio_and_video_then_mute_audio);
    CPPUNIT_TEST_SUITE_END();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<DRing::MediaMap> mediaList,
                                        CallData& callData);
    static void onMediaChangeRequested(const std::string& accountId,
                                       const std::string& callId,
                                       const std::vector<DRing::MediaMap> mediaList,
                                       CallData& callData);
    static void onVideoMuted(const std::string& callId, bool muted, CallData& callData);
    static void onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData);

    // Helpers
    static void configureScenario(CallData& bob, CallData& alice);
    void testWithScenario(CallData& aliceData, CallData& bobData, const TestScenario& scenario);
    static std::string getUserAlias(const std::string& callId);
    // Infer media direction from the mute state.
    // Note that when processing caller side, local is the caller and
    // remote is the callee, and when processing the callee, the local is
    // callee and remote is the caller.
    static MediaDirection inferDirection(bool localMute, bool remoteMute);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

private:
    CallData aliceData_;
    CallData bobData_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaNegotiationTest, MediaNegotiationTest::name());

void
MediaNegotiationTest::setUp()
{
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceData_.accountId_ = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobData_.accountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData_.accountId_);
    aliceAccount->enableMultiStream(true);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData_.accountId_);
    bobAccount->enableMultiStream(true);

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::atomic_bool accountsReady {false};
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                bool ready = false;
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready = (daemonStatus == "REGISTERED");
                details = bobAccount->getVolatileAccountDetails();
                daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready &= (daemonStatus == "REGISTERED");
                if (ready) {
                    accountsReady = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsReady.load(); }));
    DRing::unregisterSignalHandlers();
}

void
MediaNegotiationTest::tearDown()
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
    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}

std::string
MediaNegotiationTest::getUserAlias(const std::string& callId)
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

MediaDirection
MediaNegotiationTest::inferDirection([[maybe_unused]] bool localMute,
                                     [[maybe_unused]] bool remoteMute)
{
#if 1
    return MediaDirection::SENDRECV;
#else
    // NOTE. The media direction inferred here should be the correct one
    // according to the spec (RFC-3264 and RFC-6337). However, the current
    // implementation always set 'sendrecv' regardless of the mute state.
    if (not localMute and not remoteMute)
        return MediaDirection::SENDRECV;

    if (localMute and not remoteMute)
        return MediaDirection::RECVONLY;

    if (not localMute and remoteMute)
        return MediaDirection::SENDONLY;

    return MediaDirection::INACTIVE;
#endif
}

void
MediaNegotiationTest::onIncomingCallWithMedia(const std::string& accountId,
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

    // NOTE.
    // We shouldn't access shared_ptr<Call> as this event is supposed to mimic
    // the client, and the client have no access to this type. But here, we only
    // needed to check if the call exists. This is the most straightforward and
    // reliable way to do it until we add a new API (like hasCall(id)).
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
MediaNegotiationTest::onMediaChangeRequested(const std::string& accountId,
                                             const std::string& callId,
                                             const std::vector<DRing::MediaMap> mediaList,
                                             CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - media count [%lu]",
              DRing::CallSignal::MediaChangeRequested::name,
              callData.alias_.c_str(),
              callId.c_str(),
              mediaList.size());

    // TODO
    // We shouldn't access shared_ptr<Call> as this event is supposed to mimic
    // the client, and the client have no access to this type. But here, we only
    // needed to check if the call exists. This is the most straightforward and
    // reliable way to do it until we add a new API (like hasCall(id)).
    if (not Manager::instance().getCallFromCallID(callId)) {
        JAMI_WARN("Call with ID [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(CallData::Signal(DRing::CallSignal::MediaChangeRequested::name));

    callData.cv_.notify_one();
}

void
MediaNegotiationTest::onCallStateChange(const std::string& callId,
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

    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP") {
        callData.cv_.notify_one();
    }
}

void
MediaNegotiationTest::onVideoMuted(const std::string& callId, bool muted, CallData& callData)
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
              DRing::CallSignal::VideoMuted::name,
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              muted ? "Mute" : "Un-mute");

    if (account->getAccountID() != callData.accountId_)
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(DRing::CallSignal::VideoMuted::name, muted ? "muted" : "un-muted"));
    }

    callData.cv_.notify_one();
}

void
MediaNegotiationTest::onMediaNegotiationStatus(const std::string& callId,
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
MediaNegotiationTest::waitForSignal(CallData& callData,
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
MediaNegotiationTest::configureScenario(CallData& aliceData, CallData& bobData)
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
                                        user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaChangeRequested>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::vector<DRing::MediaMap> mediaList) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaChangeRequested(accountId,
                                       callId,
                                       mediaList,
                                       user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onCallStateChange(callId, state, user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onVideoMuted(callId, muted, user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId, const std::string& event) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaNegotiationStatus(callId,
                                         event,
                                         user == aliceData.alias_ ? aliceData : bobData);
        }));

    DRing::registerSignalHandlers(signalHandlers);
}

void
MediaNegotiationTest::testWithScenario(CallData& aliceData,
                                       CallData& bobData,
                                       const TestScenario& scenario)
{
    JAMI_INFO("=== Start a call and validate ===");

    // The media count of the offer and answer must match (RFC-3264).
    auto mediaCount = scenario.offer_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCount, scenario.answer_.size());

    auto const& aliceCall = std::dynamic_pointer_cast<SIPCall>(
        (Manager::instance().getAccount<JamiAccount>(aliceData.accountId_))
            ->newOutgoingCall(bobData.userName_, scenario.offer_));
    CPPUNIT_ASSERT(aliceCall);
    aliceData.callId_ = aliceCall->getCallId();

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData.accountId_.c_str(),
              bobData.accountId_.c_str());

    // Wait for incoming call signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData, DRing::CallSignal::IncomingCallWithMedia::name));
    // Answer the call.
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.answer_);
        Manager::instance().answerCallWithMedia(bobData.callId_, mediaList);
    }

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData,
                                       DRing::CallSignal::MediaNegotiationStatus::name,
                                       MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
    // Wait for the StateChange signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::MediaNegotiationStatus::name,
                                       MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Validate Alice's media
    {
        auto mediaAttr = aliceCall->getMediaAttributeList();
        CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
        for (size_t idx = 0; idx < mediaCount; idx++) {
            CPPUNIT_ASSERT_EQUAL(scenario.offer_[idx].muted_, mediaAttr[idx].muted_);
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
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    JAMI_INFO("=== Request Media Change and validate ===");
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.offerUpdate_);
        Manager::instance().requestMediaChange(aliceData.callId_, mediaList);
    }

    // Update and validate media count.
    mediaCount = scenario.offerUpdate_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCount, scenario.answerUpdate_.size());

    // Not all media change requests requires validation from client.
    if (scenario.expectMediaChangeRequest_) {
        // Wait for media change request signal.
        CPPUNIT_ASSERT_EQUAL(true,
                             waitForSignal(bobData, DRing::CallSignal::MediaChangeRequested::name));

        // Answer the change request.
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.answerUpdate_);
        Manager::instance().answerMediaChangeRequest(bobData.callId_, mediaList);
    }

    if (scenario.expectMediaRenegotiation_) {
        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT_EQUAL(true,
                             waitForSignal(aliceData,
                                           DRing::CallSignal::MediaNegotiationStatus::name,
                                           MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Validate Alice's media
        {
            auto mediaAttr = aliceCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
            for (size_t idx = 0; idx < mediaCount; idx++) {
                CPPUNIT_ASSERT_EQUAL(scenario.offerUpdate_[idx].muted_, mediaAttr[idx].muted_);
            }
        }

        // Validate Bob's media
        {
            auto const& bobCall = std::dynamic_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(bobData.callId_));
            auto mediaAttr = bobCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaAttr.size());
            for (size_t idx = 0; idx < mediaCount; idx++) {
                CPPUNIT_ASSERT_EQUAL(scenario.answerUpdate_[idx].muted_, mediaAttr[idx].muted_);
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData.callId_);

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
MediaNegotiationTest::audio_and_video_then_mute_video()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    JAMI_INFO("Waiting for accounts setup ...");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(10));

    configureScenario(aliceData_, bobData_);

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
        scenario.offerUpdate_.emplace_back(audio);
        video.muted_ = true;
        scenario.offerUpdate_.emplace_back(video);

        scenario.answerUpdate_.emplace_back(audio);
        video.muted_ = false;
        scenario.answerUpdate_.emplace_back(video);
        scenario.expectMediaRenegotiation_ = true;
        scenario.expectMediaChangeRequest_ = false;

        testWithScenario(aliceData_, bobData_, scenario);
    }

    DRing::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::audio_only_then_add_video()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    JAMI_INFO("Waiting for accounts setup ...");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(10));

    configureScenario(aliceData_, bobData_);

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
        scenario.answer_.emplace_back(audio);

        // Updated offer/answer
        scenario.offerUpdate_.emplace_back(audio);
        scenario.offerUpdate_.emplace_back(video);
        scenario.answerUpdate_.emplace_back(audio);
        scenario.answerUpdate_.emplace_back(video);
        scenario.expectMediaRenegotiation_ = true;
        scenario.expectMediaChangeRequest_ = true;

        testWithScenario(aliceData_, bobData_, scenario);
    }

    DRing::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::audio_and_video_then_mute_audio()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    JAMI_INFO("Waiting for accounts setup ...");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(10));

    configureScenario(aliceData_, bobData_);

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
        audio.muted_ = true;
        scenario.offerUpdate_.emplace_back(audio);
        scenario.offerUpdate_.emplace_back(video);

        audio.muted_ = false;
        scenario.answerUpdate_.emplace_back(audio);
        scenario.answerUpdate_.emplace_back(video);

        scenario.expectMediaRenegotiation_ = false;
        scenario.expectMediaChangeRequest_ = false;

        testWithScenario(aliceData_, bobData_, scenario);
    }

    DRing::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MediaNegotiationTest::name())
