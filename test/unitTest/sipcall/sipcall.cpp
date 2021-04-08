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
    // Determine if we should expect a media negotiation after a media change
    // request. The logic to set this flag must reflect the exact behavior of
    // the production code.
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
class SipCallTest : public CppUnit::TestFixture
{
public:
    SipCallTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~SipCallTest() { DRing::fini(); }

    static std::string name() { return "SipCallTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void audio_only();

    CPPUNIT_TEST_SUITE(SipCallTest);
    CPPUNIT_TEST(audio_only);
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
    static void configureScenario(CallData& callData);
    void testWithScenario(CallData& callData,
                          const std::string& peerAccountId,
                          const TestScenario& scenario);
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
    CallData callData_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SipCallTest, SipCallTest::name());

void
SipCallTest::setUp()
{
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    callData_.accountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(callData_.accountId_);
    aliceAccount->enableMultiStream(true);

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
SipCallTest::tearDown()
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

    Manager::instance().removeAccount(callData_.accountId_, true);

    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}
std::string
SipCallTest::getUserAlias(const std::string& callId)
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
SipCallTest::inferDirection(bool localMute, bool remoteMute)
{
#if 1
    return MediaDirection::SENDRECV;
#else
    // NOTE:
    // The media direction inferred here should be the correct one
    // according to the spec. However, the current implementation
    // always set 'sendrecv' regardless of the mute state.
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
SipCallTest::onIncomingCallWithMedia(const std::string& accountId,
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

    // NOTE:
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
SipCallTest::onMediaChangeRequested(const std::string& accountId,
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
SipCallTest::onCallStateChange(const std::string& callId,
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
SipCallTest::onVideoMuted(const std::string& callId, bool muted, CallData& callData)
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
SipCallTest::onMediaNegotiationStatus(const std::string& callId,
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
SipCallTest::waitForSignal(CallData& callData,
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
SipCallTest::configureScenario(CallData& callData)
{
    {
        CPPUNIT_ASSERT(not callData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<JamiAccount>(callData.accountId_);
        callData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        callData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
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
                onIncomingCallWithMedia(accountId, callId, mediaList, callData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaChangeRequested>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::vector<DRing::MediaMap> mediaList) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaChangeRequested(accountId, callId, mediaList, callData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onCallStateChange(callId, state, callData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onVideoMuted(callId, muted, callData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId, const std::string& event) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaNegotiationStatus(callId, event, callData);
        }));

    DRing::registerSignalHandlers(signalHandlers);
}

void
SipCallTest::testWithScenario(CallData& callData,
                              const std::string& peerAccountId,
                              const TestScenario& scenario)
{
    JAMI_INFO("=== Start a call and validate ===");

    // The media count of the offer and answer must match (RFC-3264).
    auto mediaCountOffer = scenario.offer_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCountOffer, scenario.answer_.size());

    JAMI_INFO("ALICE [%s] calls [%s] and wait for answer",
              callData.accountId_.c_str(),
              peerAccountId.c_str());

    auto const& aliceCall = std::dynamic_pointer_cast<SIPCall>(
        (Manager::instance().getAccount<JamiAccount>(callData.accountId_))
            ->newOutgoingCall(peerAccountId, scenario.offer_));
    CPPUNIT_ASSERT(aliceCall);
    callData.callId_ = aliceCall->getCallId();

    JAMI_INFO("ALICE [%s] calls BOB [%s] and wait for BOB to answer",
              callData.accountId_.c_str(),
              peerAccountId.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(callData,
                                       DRing::CallSignal::MediaNegotiationStatus::name,
                                       MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Validate Alice's SDP
    {
        auto aliceLocalMedia = aliceCall->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaCountOffer, aliceLocalMedia.size());
        for (size_t idx = 0; idx < mediaCountOffer; idx++) {
            CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.offer_[idx].muted_,
                                                scenario.answer_[idx].muted_),
                                 aliceLocalMedia[idx].direction_);
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(4));

    JAMI_INFO("Hang up the call");
    Manager::instance().hangupCall(callData.callId_);
}

void
SipCallTest::audio_only()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);
#if 1
    JAMI_INFO("Waiting for accounts setup ...");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif

    configureScenario(callData_);

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

        // TODO_MC. Fix me
        const std::string& peerAccountId("b6fe21a33eb5ac88ea6af17617afc8d73bd3e8b7");
        testWithScenario(callData_, peerAccountId, scenario);
    }

    DRing::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SipCallTest::name())
