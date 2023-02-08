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
#include "sip/sipaccount.h"
#include "../../test_runner.h"

#include "jami.h"
#include "media_const.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"
#include "media/audio/audio_rtp_session.h"
#include "media/audio/audio_receive_thread.h"
#include "media/video/video_rtp_session.h"
#include "media/video/video_receive_thread.h"

#include "common.h"

using namespace libjami::Account;
using namespace libjami::Call;

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
    uint16_t listeningPort_ {0};
    std::string alias_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
    bool compliancyEnabled_ {false};
};

// Used to register a MediaFrame observer to RTP session in order
// to validate the media stream.
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
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

        for (size_t idx = 0; idx < MEDIA_COUNT; idx++) {
            mediaReceivers_.emplace_back(std::make_shared<MediaReceiver>(MediaType::MEDIA_AUDIO));
        }
    }
    ~IceSdpParsingTest() { libjami::fini(); }

    static std::string name() { return "IceSdpParsingTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void call_with_rfc5245_compliancy_disabled();
    void call_with_rfc5245_compliancy_enabled();

    CPPUNIT_TEST_SUITE(IceSdpParsingTest);
    CPPUNIT_TEST(call_with_rfc5245_compliancy_disabled);
    CPPUNIT_TEST(call_with_rfc5245_compliancy_enabled);
    CPPUNIT_TEST_SUITE_END();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData);
    static void onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData);

    // Helpers
    void test_call();
    static void configureTest(CallData& bob, CallData& alice);
    static std::string getUserAlias(const std::string& callId);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});
    static bool attachReceiver(std::shared_ptr<MediaReceiver> receiver,
                               std::shared_ptr<RtpSession> rtpStream);
    static bool detachReceiver(std::shared_ptr<MediaReceiver> receiver,
                               std::shared_ptr<RtpSession> rtpStream);

private:
    CallData aliceData_;
    CallData bobData_;
    const size_t MEDIA_COUNT {2};
    std::vector<std::shared_ptr<MediaReceiver>> mediaReceivers_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(IceSdpParsingTest, IceSdpParsingTest::name());

void
IceSdpParsingTest::setUp()
{
    aliceData_.listeningPort_ = 5080;
    std::map<std::string, std::string> details = libjami::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::LOCAL_PORT] = std::to_string(aliceData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    aliceData_.accountId_ = Manager::instance().addAccount(details);

    bobData_.listeningPort_ = 5082;
    details = libjami::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::LOCAL_PORT] = std::to_string(bobData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    bobData_.accountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize accounts ...");
    auto aliceAccount = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto bobAccount = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);
}

void
IceSdpParsingTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");
    wait_for_removal_of({aliceData_.accountId_, bobData_.accountId_});
}

std::string
IceSdpParsingTest::getUserAlias(const std::string& callId)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    if (not call) {
        JAMI_WARN("Call [%s] does not exist!", callId.c_str());
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
                                           const std::vector<libjami::MediaMap> mediaList,
                                           CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - media count [%lu]",
              libjami::CallSignal::IncomingCallWithMedia::name,
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
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::IncomingCallWithMedia::name));

    callData.cv_.notify_one();
}

void
IceSdpParsingTest::onCallStateChange(const std::string&,
                                     const std::string& callId,
                                     const std::string& state,
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
        for (auto it = callData.signals_.begin(); it != callData.signals_.end(); it++) {
            // The predicate is true if the signal names match, and if the
            // expectedEvent is not empty, the events must also match.
            if (it->name_ == expectedSignal
                and (expectedEvent.empty() or it->event_ == expectedEvent)) {
                // Done with this signal.
                callData.signals_.erase(it);
                return true;
            }
        }
        // Signal/event not found.
        return false;
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
IceSdpParsingTest::attachReceiver(std::shared_ptr<MediaReceiver> mediaReceiver,
                                  std::shared_ptr<RtpSession> rtpSession)
{
    CPPUNIT_ASSERT(mediaReceiver);
    CPPUNIT_ASSERT(mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO
                   or mediaReceiver->mediaType_ == MediaType::MEDIA_VIDEO);

    if (mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO) {
        auto audioRtp = std::dynamic_pointer_cast<AudioRtpSession>(rtpSession);
        auto receiver = audioRtp->getAudioReceive().get();
        CPPUNIT_ASSERT(receiver != nullptr);
        if (receiver == nullptr)
            return false;
        return receiver->attach(mediaReceiver.get());
    }

    auto videoRtp = std::dynamic_pointer_cast<video::VideoRtpSession>(rtpSession);
    auto receiver = videoRtp->getVideoReceive().get();
    CPPUNIT_ASSERT(receiver != nullptr);
    return receiver->attach(mediaReceiver.get());
}

bool
IceSdpParsingTest::detachReceiver(std::shared_ptr<MediaReceiver> mediaReceiver,
                                  std::shared_ptr<RtpSession> rtpSession)
{
    CPPUNIT_ASSERT(mediaReceiver);
    CPPUNIT_ASSERT(mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO
                   or mediaReceiver->mediaType_ == MediaType::MEDIA_VIDEO);

    if (mediaReceiver->mediaType_ == MediaType::MEDIA_AUDIO) {
        auto audioRtp = std::dynamic_pointer_cast<AudioRtpSession>(rtpSession);
        auto receiver = audioRtp->getAudioReceive().get();
        CPPUNIT_ASSERT(receiver != nullptr);
        return receiver->detach(mediaReceiver.get());
    }

    auto videoRtp = std::dynamic_pointer_cast<video::VideoRtpSession>(rtpSession);
    auto receiver = videoRtp->getVideoReceive().get();
    CPPUNIT_ASSERT(receiver != nullptr);
    return receiver->detach(mediaReceiver.get());
}

void
IceSdpParsingTest::configureTest(CallData& aliceData, CallData& bobData)
{
    {
        CPPUNIT_ASSERT(not aliceData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(aliceData.accountId_);
        aliceData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(aliceData.listeningPort_);
        account->enableIceCompIdRfc5245Compliance(aliceData.compliancyEnabled_);
    }

    {
        CPPUNIT_ASSERT(not bobData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(bobData.accountId_);
        bobData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(bobData.listeningPort_);
        account->enableIceCompIdRfc5245Compliance(bobData.compliancyEnabled_);
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

    signalHandlers.insert(
        libjami::exportable_callback<libjami::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onCallStateChange(accountId,
                                  callId,
                                  state,
                                  user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>& /* mediaList */) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaNegotiationStatus(callId,
                                         event,
                                         user == aliceData.alias_ ? aliceData : bobData);
        }));

    libjami::registerSignalHandlers(signalHandlers);
}

void
IceSdpParsingTest::test_call()
{
    configureTest(aliceData_, bobData_);

    JAMI_INFO("=== Start a call and validate ===");

    // NOTE:
    // We use two audio media instead of one audio and one video media
    // to be able to run the test on machines that do not have access to
    // camera.
    // For this specific UT, testing with two audio media is valid, because
    // the main goal is to validate that the media sockets negotiated
    // through ICE can correctly exchange media (RTP packets).

    MediaAttribute media_0(MediaType::MEDIA_AUDIO);
    media_0.label_ = "audio_0";
    media_0.enabled_ = true;
    MediaAttribute media_1(MediaType::MEDIA_AUDIO);
    media_1.label_ = "audio_1";
    media_1.enabled_ = true;

    std::vector<MediaAttribute> offer;
    offer.emplace_back(media_0);
    offer.emplace_back(media_1);

    std::vector<MediaAttribute> answer;
    answer.emplace_back(media_0);
    answer.emplace_back(media_1);

    CPPUNIT_ASSERT_EQUAL(MEDIA_COUNT, offer.size());
    CPPUNIT_ASSERT_EQUAL(MEDIA_COUNT, answer.size());
    auto bobAddr = ip_utils::getLocalAddr(AF_INET);
    bobAddr.setPort(bobData_.listeningPort_);

    aliceData_.callId_ = libjami::placeCallWithMedia(aliceData_.accountId_,
                                                   bobAddr.toString(true),
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
        waitForSignal(aliceData_, libjami::CallSignal::StateChange::name, StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_, libjami::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    libjami::acceptWithMedia(bobData_.accountId_,
                           bobData_.callId_,
                           MediaAttribute::mediaAttributesToMediaMaps(answer));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_,
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(
        waitForSignal(bobData_, libjami::CallSignal::StateChange::name, StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData_.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(aliceData_,
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Give some time to media to start.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Register the media observer to validate media flow.
    CPPUNIT_ASSERT_EQUAL(MEDIA_COUNT, mediaReceivers_.size());
    auto call = std::dynamic_pointer_cast<SIPCall>(
        Manager::instance().getCallFromCallID(aliceData_.callId_));
    CPPUNIT_ASSERT(call);

    auto rtpList = call->getRtpSessionList();
    CPPUNIT_ASSERT(rtpList.size() == offer.size());

    for (size_t i = 0; i < MEDIA_COUNT; i++) {
        CPPUNIT_ASSERT(rtpList[i]);
        CPPUNIT_ASSERT(rtpList[i]->getMediaType() == offer[i].type_);
        CPPUNIT_ASSERT(attachReceiver(mediaReceivers_[i], rtpList[i]));
    }

    // NOTE:
    // This validation step works on hosts/containers that have correctly
    // configured sound system.
    // Currenty hosts/containers used for testing are not setup to capture
    // and playback audio, so this validation will be disabled for now.
#if 0
    JAMI_INFO("Waiting for media to flow ...");
    for (size_t i = 0; i < MEDIA_COUNT; i++) {
        CPPUNIT_ASSERT(mediaReceivers_[i]->waitForMediaFlow());
    }
#endif
    // Detach the observers.
    for (size_t i = 0; i < MEDIA_COUNT; i++) {
        CPPUNIT_ASSERT(detachReceiver(mediaReceivers_[i], rtpList[i]));
    }

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData_.accountId_, bobData_.callId_);

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData_,
                                       libjami::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData_,
                                       libjami::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
IceSdpParsingTest::call_with_rfc5245_compliancy_disabled()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    aliceData_.compliancyEnabled_ = bobData_.compliancyEnabled_ = false;
    test_call();
}

void
IceSdpParsingTest::call_with_rfc5245_compliancy_enabled()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    aliceData_.compliancyEnabled_ = bobData_.compliancyEnabled_ = true;
    test_call();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::IceSdpParsingTest::name())
