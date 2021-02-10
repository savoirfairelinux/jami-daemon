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

    std::string userName_ {};
    std::string alias_ {};
    std::shared_ptr<JamiAccount> account_;
    std::shared_ptr<SIPCall> sipCall_;
    std::vector<MediaAttribute> mediaAttrList_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
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
    void testCallWithMediaList();

    CPPUNIT_TEST_SUITE(MediaNegotiationTest);
    CPPUNIT_TEST(testCallWithMediaList);
    CPPUNIT_TEST_SUITE_END();

    // Event/Signal handlers
    void onCallStateChange(const std::string& callId, const std::string& state, CallData& callData);
    void onIncomingCall(const std::string& accountId, const std::string& callId, CallData& callData);
    void onVideoMuted(const std::string& callId, bool muted, CallData& callData);
    void onMediaStateChanged(const std::string& callId,
                             const std::string& event,
                             CallData& callData);

    // Helpers
    void testWithScenario(CallData& aliceData, CallData& bobData, const TestScenario& scenario);
    Call::MediaMap createMediaMap(const MediaAttribute& mediaAttr);
    // Infer media direction from the mute state.
    // Note that when processing caller side, local is the caller and
    // remote is the callee, and when processing the callee, the local is
    // callee and remote is the caller.
    MediaDirection inferDirection(bool localMute, bool remoteMute);
    std::string getUserAlias(const std::string& callId);

    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    bool waitForSignal(CallData& callData,
                       const std::string& signal,
                       const std::string& expectedEvent = {});

private:
    std::string aliceAccountId_;
    std::string bobAccountId_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaNegotiationTest, MediaNegotiationTest::name());

void
MediaNegotiationTest::setUp()
{
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    // details[ConfProperties::TURN::ENABLED] = "false";
    // details[ConfProperties::STUN::ENABLED] = "false";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceAccountId_ = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    // details[ConfProperties::TURN::ENABLED] = "false";
    // details[ConfProperties::STUN::ENABLED] = "false";
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobAccountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceAccountId_);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobAccountId_);
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

    Manager::instance().removeAccount(aliceAccountId_, true);
    Manager::instance().removeAccount(bobAccountId_, true);
    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}

MediaMap
MediaNegotiationTest::createMediaMap(const MediaAttribute& mediaAttr)
{
    std::map<std::string, std::string> mediaMap;

    mediaMap.emplace("MEDIA_TYPE",
                     mediaAttr.type_ == MediaType::MEDIA_AUDIO ? MediaAttributeValue::AUDIO
                                                               : MediaAttributeValue::VIDEO);
    mediaMap.emplace("ENABLED",
                     mediaAttr.enabled_ ? MediaAttributeValue::TRUE_VAL
                                        : MediaAttributeValue::FALSE_VAL);
    mediaMap.emplace("MUTED",
                     mediaAttr.muted_ ? MediaAttributeValue::TRUE_VAL
                                      : MediaAttributeValue::FALSE_VAL);
    mediaMap.emplace("SECURE",
                     mediaAttr.secure_ ? MediaAttributeValue::TRUE_VAL
                                       : MediaAttributeValue::FALSE_VAL);
    mediaMap.emplace("LABEL", mediaAttr.label_);

    return mediaMap;
}

MediaDirection
MediaNegotiationTest::inferDirection(bool localMute, bool remoteMute)
{
    if (not localMute and not remoteMute)
        return MediaDirection::SENDRECV;

    if (localMute and not remoteMute)
        return MediaDirection::RECVONLY;

    if (not localMute and remoteMute)
        return MediaDirection::SENDONLY;

    return MediaDirection::INACTIVE;
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

void
MediaNegotiationTest::onIncomingCall(const std::string& accountId,
                                     const std::string& callId,
                                     CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.account_->getAccountID(), accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s]",
              DRing::CallSignal::IncomingCall::name,
              callData.alias_.c_str(),
              callId.c_str());

    auto call = Manager::instance().getCallFromCallID(callId);

    if (not call) {
        JAMI_ERR("Call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    call->answer();

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.sipCall_ = std::dynamic_pointer_cast<SIPCall>(call);
    callData.signals_.emplace_back(
        CallData::Signal(DRing::CallSignal::IncomingCall::name, StateEvent::INCOMING));
}

void
MediaNegotiationTest::onCallStateChange(const std::string& callId,
                                        const std::string& state,
                                        CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_ERR("Call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    // if (call->isSubcall()) {
    //     return;
    // }

    auto account = call->getAccount().lock();
    if (not account) {
        JAMI_ERR("Account owning the call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              DRing::CallSignal::StateChange::name,
              callData.alias_.c_str(),
              callId.c_str(),
              state.c_str());

    if (account->getAccountID() != callData.account_->getAccountID())
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
        JAMI_ERR("Call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    auto account = call->getAccount().lock();
    if (not account) {
        JAMI_ERR("Account owning the call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              DRing::CallSignal::VideoMuted::name,
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              muted ? "Mute" : "Un-mute");

    if (account->getAccountID() != callData.account_->getAccountID())
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.mediaAttrList_ = call->getMediaAttributeList();
        callData.signals_.emplace_back(
            CallData::Signal(DRing::CallSignal::VideoMuted::name, muted ? "muted" : "un-muted"));
    }

    callData.cv_.notify_one();
}

void
MediaNegotiationTest::onMediaStateChanged(const std::string& callId,
                                          const std::string& event,
                                          CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_ERR("Call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    auto account = call->getAccount().lock();
    if (not account) {
        JAMI_ERR("Call with ID [%s] does not exist!", callId.c_str());
        return;
    }

    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              DRing::CallSignal::MediaStateChanged::name,
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              event.c_str());

    if (account->getAccountID() != callData.account_->getAccountID())
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.mediaAttrList_ = call->getMediaAttributeList();
        callData.signals_.emplace_back(
            CallData::Signal(DRing::CallSignal::MediaStateChanged::name, event));
    }

    callData.cv_.notify_one();
}

bool
MediaNegotiationTest::waitForSignal(CallData& callData,
                                    const std::string& expectedSignal,
                                    const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {20};
    std::unique_lock<std::mutex> lock {callData.mtx_};

    JAMI_INFO("[%s] is waiting for [%s::%s] signal",
              callData.alias_.c_str(),
              expectedSignal.c_str(),
              expectedEvent.c_str());

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
        JAMI_ERR("[%s] waiting for signal [%s::%s] timed-out!",
                 callData.alias_.c_str(),
                 expectedSignal.c_str(),
                 expectedEvent.c_str());

        JAMI_INFO("[%s] currently has the following signals:", callData.alias_.c_str());

        for (auto const& sig : callData.signals_) {
            JAMI_INFO("Signal [%s::%s]", sig.name_.c_str(), sig.event_.c_str());
        }
    }

    return res;
}

void
MediaNegotiationTest::testWithScenario(CallData& aliceData,
                                       CallData& bobData,
                                       const TestScenario& scenario)
{
    JAMI_INFO("=== Start a call and validate ===");

    // Create the list of medias
    std::vector<Call::MediaMap> mediaList;
    for (auto const& mediaAttr : scenario.offer_) {
        auto const& map = createMediaMap(mediaAttr);
        mediaList.emplace_back(map);
    }
    JAMI_INFO("ALICE [%s] calls BOB [%s] and wait for BOB to answer",
              aliceData.account_->getAccountID().c_str(),
              bobData.account_->getAccountID().c_str());

    aliceData.sipCall_ = std::dynamic_pointer_cast<SIPCall>(
        aliceData.account_->newOutgoingCall(bobData.userName_, mediaList));
    assert(aliceData.sipCall_);

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData,
                                       DRing::CallSignal::MediaStateChanged::name,
                                       MediaStateChangedEvent::MEDIA_NEGOTIATED));
    // Wait for the StateChange signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(bobData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData.sipCall_->getCallId().c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::MediaStateChanged::name,
                                       MediaStateChangedEvent::MEDIA_NEGOTIATED));

    // Wait for media streaming signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::MediaStateChanged::name,
                                       MediaStateChangedEvent::MEDIA_STARTED));

    {
        // Validate Alice's SDP
        auto aliceLocalMedia = aliceData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaList.size(), aliceLocalMedia.size());

        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.offer_[0].muted_, scenario.answer_[0].muted_),
                             aliceLocalMedia[0].direction_);
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.offer_[1].muted_, scenario.answer_[1].muted_),
                             aliceLocalMedia[1].direction_);

        // Validate Bob's SDP
        auto bobLocalMedia = bobData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaList.size(), bobLocalMedia.size());
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.answer_[0].muted_, scenario.offer_[0].muted_),
                             bobLocalMedia[0].direction_);
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.answer_[1].muted_, scenario.offer_[1].muted_),
                             bobLocalMedia[1].direction_);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Update the media
    mediaList.clear();
    for (auto const& mediaAttr : scenario.offerUpdate_) {
        auto const& map = createMediaMap(mediaAttr);
        mediaList.emplace_back(map);
    }

    JAMI_INFO("=== Update Media and validate ===");
    Manager::instance().updateMediaStreams(aliceData.sipCall_->getCallId(), mediaList);

    // Wait for the VideoMute signal.
    CPPUNIT_ASSERT_EQUAL(true, waitForSignal(aliceData, DRing::CallSignal::VideoMuted::name));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::MediaStateChanged::name,
                                       MediaStateChangedEvent::MEDIA_NEGOTIATED));

    {
        CPPUNIT_ASSERT_EQUAL(scenario.offerUpdate_.size(), aliceData.mediaAttrList_.size());
        CPPUNIT_ASSERT_EQUAL(true, scenario.offerUpdate_[1].muted_);

        // Validate Alice's SDP
        auto aliceLocalMedia = aliceData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaList.size(), aliceLocalMedia.size());
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.offerUpdate_[0].muted_,
                                            scenario.answerUpdate_[0].muted_),
                             aliceLocalMedia[0].direction_);
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.offerUpdate_[1].muted_,
                                            scenario.answerUpdate_[1].muted_),
                             aliceLocalMedia[1].direction_);

        // Validate Bob's SDP
        auto bobLocalMedia = bobData.sipCall_->getSDP().getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaList.size(), bobLocalMedia.size());
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.answerUpdate_[0].muted_,
                                            scenario.offerUpdate_[0].muted_),
                             bobLocalMedia[0].direction_);
        CPPUNIT_ASSERT_EQUAL(inferDirection(scenario.answerUpdate_[1].muted_,
                                            scenario.offerUpdate_[1].muted_),
                             bobLocalMedia[1].direction_);
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
MediaNegotiationTest::testCallWithMediaList()
{
#if 1
    JAMI_INFO("Waiting for accounts setup ...");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif

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
            auto user = getUserAlias(callId);
            onIncomingCall(accountId, callId, user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            auto user = getUserAlias(callId);
            onCallStateChange(callId, state, user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) {
            auto user = getUserAlias(callId);
            onVideoMuted(callId, muted, user == aliceData.alias_ ? aliceData : bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::MediaStateChanged>(
        [&](const std::string& callId, const std::string& event) {
            auto user = getUserAlias(callId);
            onMediaStateChanged(callId, event, user == aliceData.alias_ ? aliceData : bobData);
        }));

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
        video.muted_ = false;
        scenario.answerUpdate_.emplace_back(audio);
        scenario.answerUpdate_.emplace_back(video);

        testWithScenario(aliceData, bobData, scenario);
    }
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MediaNegotiationTest::name())
