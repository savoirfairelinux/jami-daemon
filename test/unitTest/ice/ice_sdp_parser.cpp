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

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "sip/sdp.h"
#include "media/audio/audio_rtp_session.h"
#include "media/audio/audio_receive_thread.h"

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
    std::string userName_ {};
    std::string alias_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
    bool compliancyEnabled_ {false};
};

// Used to register a MediaFrame observer to RTP session to validate
// media flow.
class MediaReceiver : public Observer<std::shared_ptr<MediaFrame>>
{
public:
    MediaReceiver(MediaType type)
        : mediaType_(type)
        , mediaTypeStr_(type == MediaType::MEDIA_AUDIO ? "AUDIO" : "VIDEO") {};

    virtual ~MediaReceiver() {};
    void update(Observable<std::shared_ptr<jami::MediaFrame>>* observer,
                const std::shared_ptr<jami::MediaFrame>& mediaframe) override;

    bool waitForMediaFlow();
    const MediaType mediaType_ {MediaType::MEDIA_NONE};
    const std::string mediaTypeStr_ {};
    const std::chrono::seconds TIME_OUT {10};
    const unsigned REQUIRED_FRAME_COUNT {100};

private:
    unsigned long frameCounter_ {0};
    std::condition_variable cv_ {};
    std::mutex mtx_;
};

void
MediaReceiver::update(Observable<std::shared_ptr<jami::MediaFrame>>*,
                      const std::shared_ptr<jami::MediaFrame>& frame)
{
    std::unique_lock<std::mutex> lock {mtx_};
    if (frame and frame->getFrame())
        frameCounter_++;

    if (frameCounter_ % 10 == 1) {
        JAMI_INFO("[%s] Frame counter %lu", mediaTypeStr_.c_str(), frameCounter_);
    }

    if (frameCounter_ >= REQUIRED_FRAME_COUNT)
        cv_.notify_one();
}

bool
MediaReceiver::waitForMediaFlow()
{
    std::unique_lock<std::mutex> lock {mtx_};

    return cv_.wait_for(lock, TIME_OUT, [this] { return frameCounter_ > 100; });
}

class IceSdpParsingTest : public CppUnit::TestFixture
{
public:
    IceSdpParsingTest()
        : audioReceiver_(std::make_shared<MediaReceiver>(MediaType::MEDIA_AUDIO))
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

        // We must have valid media receiver.
        CPPUNIT_ASSERT(audioReceiver_);
    }
    ~IceSdpParsingTest() { DRing::fini(); }

    static std::string name() { return "IceSdpParsingTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void call_with_rfc5245_compliancy_disabled();
    void call_with_rfc5245_compliancy_enabled();
    void call_with_rfc5245_compliancy_enabled_caller_side();

    CPPUNIT_TEST_SUITE(IceSdpParsingTest);
    CPPUNIT_TEST(call_with_rfc5245_compliancy_disabled);
    CPPUNIT_TEST(call_with_rfc5245_compliancy_enabled);
    // TODO. Do we need it ?
    // CPPUNIT_TEST(call_with_rfc5245_compliancy_enabled_caller_side);
    CPPUNIT_TEST_SUITE_END();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& callId,
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
    void audio_video_call();
    static void configureTest(CallData& bob, CallData& alice);
    static std::string getUserAlias(const std::string& callId);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});
    static bool attachReceiver(const std::string& callId, std::shared_ptr<MediaReceiver> receiver);
    static bool detachReceiver(const std::string& callId, std::shared_ptr<MediaReceiver> receiver);

private:
    CallData aliceData_;
    CallData bobData_;
    std::shared_ptr<MediaReceiver> audioReceiver_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(IceSdpParsingTest, IceSdpParsingTest::name());

void
IceSdpParsingTest::setUp()
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
IceSdpParsingTest::tearDown()
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
IceSdpParsingTest::getUserAlias(const std::string& callId)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    if (not call) {
        JAMI_WARN("Call [%s] does not exist anymore!", callId.c_str());
        return {};
    }

    auto const& account = call->getAccount().lock();
    if (not account) {
        return {};
    }

    return account->getAccountDetails()[ConfProperties::ALIAS];
}

void
IceSdpParsingTest::onIncomingCallWithMedia(const std::string& accountId,
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
        JAMI_WARN("Call [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(CallData::Signal(DRing::CallSignal::IncomingCallWithMedia::name));

    callData.cv_.notify_one();
}

void
IceSdpParsingTest::onCallStateChange(const std::string& callId,
                                     const std::string& state,
                                     CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_WARN("Call [%s] does not exist anymore!", callId.c_str());
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
IceSdpParsingTest::onMediaNegotiationStatus(const std::string& callId,
                                            const std::string& event,
                                            CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_WARN("Call [%s] does not exist!", callId.c_str());
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
IceSdpParsingTest::waitForSignal(CallData& callData,
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

bool
IceSdpParsingTest::attachReceiver(const std::string& callId,
                                  std::shared_ptr<MediaReceiver> mediaReceiver)
{
    CPPUNIT_ASSERT(mediaReceiver);
    auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(callId));
    if (not call) {
        JAMI_ERR("Call [%s] does not exist!", callId.c_str());
    }
    CPPUNIT_ASSERT(call);

    CPPUNIT_ASSERT(mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO
                   or mediaReceiver->mediaType_ == MediaType::MEDIA_VIDEO);

    if (mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO) {
        auto audioRtp = call->getAudioRtp();
        auto receiver = audioRtp->getAudioReceive().get();
        CPPUNIT_ASSERT(receiver != nullptr);
        if (receiver == nullptr)
            return false;
        return receiver->attach(mediaReceiver.get());
    }

    auto videoRtp = call->getVideoRtp();
    auto receiver = videoRtp->getVideoReceive().get();
    CPPUNIT_ASSERT(receiver != nullptr);
    if (receiver == nullptr)
        return false;
    return receiver->attach(mediaReceiver.get());
}

bool
IceSdpParsingTest::detachReceiver(const std::string& callId,
                                  std::shared_ptr<MediaReceiver> mediaReceiver)
{
    CPPUNIT_ASSERT(mediaReceiver);
    auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(callId));
    if (not call) {
        JAMI_ERR("Call [%s] does not exist!", callId.c_str());
    }
    CPPUNIT_ASSERT(call);

    CPPUNIT_ASSERT(mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO
                   or mediaReceiver->mediaType_ == MediaType::MEDIA_VIDEO);

    if (mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO) {
        auto audioRtp = call->getAudioRtp();
        auto receiver = audioRtp->getAudioReceive().get();
        CPPUNIT_ASSERT(receiver != nullptr);
        return receiver->detach(mediaReceiver.get());
    }

    auto videoRtp = call->getVideoRtp();
    auto receiver = videoRtp->getVideoReceive().get();
    CPPUNIT_ASSERT(receiver != nullptr);
    return receiver->detach(mediaReceiver.get());
}

void
IceSdpParsingTest::configureTest(CallData& aliceData, CallData& bobData)
{
    {
        CPPUNIT_ASSERT(not aliceData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<JamiAccount>(aliceData.accountId_);
        aliceData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];

        account->enableIceCompIdRfc5245Compliance(aliceData.compliancyEnabled_);
    }

    {
        CPPUNIT_ASSERT(not bobData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<JamiAccount>(bobData.accountId_);
        bobData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];

        account->enableIceCompIdRfc5245Compliance(bobData.compliancyEnabled_);
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

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onCallStateChange(callId, state, user == aliceData.alias_ ? aliceData : bobData);
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
IceSdpParsingTest::audio_video_call()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    JAMI_INFO("Waiting for accounts setup ...");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(10));

    configureTest(aliceData_, bobData_);

    JAMI_INFO("=== Start a call and validate ===");

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "main audio";
    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "main video";

    std::vector<MediaAttribute> offer;
    offer.emplace_back(audio);
    offer.emplace_back(video);
    std::vector<MediaAttribute> answer;
    answer.emplace_back(audio);
    answer.emplace_back(video);

    auto const& aliceCall = std::dynamic_pointer_cast<SIPCall>(
        (Manager::instance().getAccount<JamiAccount>(aliceData_.accountId_))
            ->newOutgoingCall(bobData_.userName_, offer));
    CPPUNIT_ASSERT(aliceCall);
    aliceData_.callId_ = aliceCall->getCallId();

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData_.accountId_.c_str(),
              bobData_.accountId_.c_str());

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_, DRing::CallSignal::IncomingCallWithMedia::name));
    // Answer the call.
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(answer);
        Manager::instance().answerCallWithMedia(bobData_.callId_, mediaList);
    }

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_,
                                 DRing::CallSignal::MediaNegotiationStatus::name,
                                 MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(
        waitForSignal(bobData_, DRing::CallSignal::StateChange::name, StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData_.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(aliceData_,
                                 DRing::CallSignal::MediaNegotiationStatus::name,
                                 MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Give some time to media to start.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Register the media observer to validate media flow.
    // NOTE: For now, only audio is validated.
    CPPUNIT_ASSERT(attachReceiver(aliceData_.callId_, audioReceiver_));

    JAMI_INFO("Waiting for media fot flow ...");
    CPPUNIT_ASSERT(audioReceiver_->waitForMediaFlow());
    CPPUNIT_ASSERT(detachReceiver(aliceData_.callId_, audioReceiver_));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData_.callId_);

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData_,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
IceSdpParsingTest::call_with_rfc5245_compliancy_disabled()
{
    aliceData_.compliancyEnabled_ = bobData_.compliancyEnabled_ = false;
    audio_video_call();
}

void
IceSdpParsingTest::call_with_rfc5245_compliancy_enabled()
{
    aliceData_.compliancyEnabled_ = bobData_.compliancyEnabled_ = true;
    audio_video_call();
}

void
IceSdpParsingTest::call_with_rfc5245_compliancy_enabled_caller_side()
{
    aliceData_.compliancyEnabled_ = true;
    bobData_.compliancyEnabled_ = false;
    audio_video_call();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::IceSdpParsingTest::name())
