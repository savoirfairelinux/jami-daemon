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

#include "../../test_runner.h"

#include "manager.h"
#include "connectivity/connectionmanager.h"
#include "account.h"
#include "sip/sipaccount.h"
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
    uint16_t listeningPort_ {0};
    std::string toUri_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
};

class AutoAnswerMediaNegoTest
{
public:
    AutoAnswerMediaNegoTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~AutoAnswerMediaNegoTest() { libjami::fini(); }

protected:
    // Test cases.
    void audio_and_video_then_caller_mute_video();
    void audio_only_then_caller_add_video();
    void audio_and_video_then_caller_mute_audio();
    void audio_and_video_then_change_video_source();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData);
    static void onMediaChangeRequested(const std::string& accountId,
                                       const std::string& callId,
                                       const std::vector<libjami::MediaMap> mediaList,
                                       CallData& callData);
    static void onVideoMuted(const std::string& callId, bool muted, CallData& callData);
    static void onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData);

    // Helpers
    void configureScenario();
    void testWithScenario(CallData& aliceData, CallData& bobData, const TestScenario& scenario);
    static std::string getUserAlias(const std::string& callId);
    // Infer media direction of an offer.
    static uint8_t directionToBitset(MediaDirection direction, bool isLocal);
    static MediaDirection bitsetToDirection(uint8_t val);
    static MediaDirection inferInitialDirection(const MediaAttribute& offer);
    // Infer media direction of an answer.
    static MediaDirection inferNegotiatedDirection(MediaDirection local, MediaDirection answer);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal like the StateChange signal.
    static bool validateMuteState(std::vector<MediaAttribute> expected,
                                  std::vector<MediaAttribute> actual);
    static bool validateMediaDirection(std::vector<MediaDescription> descrList,
                                       std::vector<MediaAttribute> listInOffer,
                                       std::vector<MediaAttribute> listInAnswer);
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

    bool isSipAccount_ {false};
    CallData aliceData_;
    CallData bobData_;
};

class AutoAnswerMediaNegoTestSip : public AutoAnswerMediaNegoTest, public CppUnit::TestFixture
{
public:
    AutoAnswerMediaNegoTestSip() { isSipAccount_ = true; };

    ~AutoAnswerMediaNegoTestSip() {};

    static std::string name() { return "AutoAnswerMediaNegoTestSip"; }
    void setUp() override;
    void tearDown() override;

private:
    CPPUNIT_TEST_SUITE(AutoAnswerMediaNegoTestSip);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_video);
    CPPUNIT_TEST(audio_only_then_caller_add_video);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_audio);
    CPPUNIT_TEST(audio_and_video_then_change_video_source);
    CPPUNIT_TEST_SUITE_END();
};

void
AutoAnswerMediaNegoTestSip::setUp()
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
    details[ConfProperties::AUTOANSWER] = "true";
    details[ConfProperties::LOCAL_PORT] = std::to_string(bobData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    bobData_.accountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize accounts ...");
    auto aliceAccount = Manager::instance().getAccount<Account>(aliceData_.accountId_);
    auto bobAccount = Manager::instance().getAccount<Account>(bobData_.accountId_);
}

void
AutoAnswerMediaNegoTestSip::tearDown()
{
    JAMI_INFO("Remove created accounts...");

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    std::atomic_bool accountsRemoved {false};
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - 2) {
                accountsRemoved = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceData_.accountId_, true);
    Manager::instance().removeAccount(bobData_.accountId_, true);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    libjami::unregisterSignalHandlers();
}

class AutoAnswerMediaNegoTestJami : public AutoAnswerMediaNegoTest, public CppUnit::TestFixture
{
public:
    AutoAnswerMediaNegoTestJami() { isSipAccount_ = false; };

    ~AutoAnswerMediaNegoTestJami() {};

    static std::string name() { return "AutoAnswerMediaNegoTestJami"; }
    void setUp() override;
    void tearDown() override;

private:
    CPPUNIT_TEST_SUITE(AutoAnswerMediaNegoTestJami);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_video);
    CPPUNIT_TEST(audio_only_then_caller_add_video);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_audio);
    CPPUNIT_TEST(audio_and_video_then_change_video_source);
    CPPUNIT_TEST_SUITE_END();
};

void
AutoAnswerMediaNegoTestJami::setUp()
{
    auto actors = load_actors("actors/alice-bob-no-upnp.yml");

    aliceData_.accountId_ = actors["alice"];
    bobData_.accountId_ = actors["bob"];

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<Account>(aliceData_.accountId_);
    auto bobAccount = Manager::instance().getAccount<Account>(bobData_.accountId_);
    auto details = bobAccount->getAccountDetails();
    details[ConfProperties::AUTOANSWER] = "true";
    bobAccount->setAccountDetails(details);
    wait_for_announcement_of({aliceAccount->getAccountID(), bobAccount->getAccountID()});
}

void
AutoAnswerMediaNegoTestJami::tearDown()
{
    wait_for_removal_of({aliceData_.accountId_, bobData_.accountId_});
}

std::string
AutoAnswerMediaNegoTest::getUserAlias(const std::string& callId)
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
AutoAnswerMediaNegoTest::inferInitialDirection(const MediaAttribute& mediaAttr)
{
    if (not mediaAttr.enabled_)
        return MediaDirection::INACTIVE;

    if (mediaAttr.muted_) {
        if (mediaAttr.onHold_)
            return MediaDirection::INACTIVE;
        return MediaDirection::RECVONLY;
    }

    if (mediaAttr.onHold_)
        return MediaDirection::SENDONLY;

    return MediaDirection::SENDRECV;
}

uint8_t
AutoAnswerMediaNegoTest::directionToBitset(MediaDirection direction, bool isLocal)
{
    if (direction == MediaDirection::SENDRECV)
        return 3;
    if (direction == MediaDirection::RECVONLY)
        return isLocal ? 2 : 1;
    if (direction == MediaDirection::SENDONLY)
        return isLocal ? 1 : 2;
    return 0;
}

MediaDirection
AutoAnswerMediaNegoTest::bitsetToDirection(uint8_t val)
{
    if (val == 3)
        return MediaDirection::SENDRECV;
    if (val == 2)
        return MediaDirection::RECVONLY;
    if (val == 1)
        return MediaDirection::SENDONLY;
    return MediaDirection::INACTIVE;
}

MediaDirection
AutoAnswerMediaNegoTest::inferNegotiatedDirection(MediaDirection local, MediaDirection remote)
{
    uint8_t val = directionToBitset(local, true) & directionToBitset(remote, false);
    auto dir = bitsetToDirection(val);
    return dir;
}

bool
AutoAnswerMediaNegoTest::validateMuteState(std::vector<MediaAttribute> expected,
                                           std::vector<MediaAttribute> actual)
{
    CPPUNIT_ASSERT_EQUAL(expected.size(), actual.size());

    for (size_t idx = 0; idx < expected.size(); idx++) {
        if (expected[idx].muted_ != actual[idx].muted_)
            return false;
    }

    return true;
}

bool
AutoAnswerMediaNegoTest::validateMediaDirection(std::vector<MediaDescription> descrList,
                                                std::vector<MediaAttribute> localMediaList,
                                                std::vector<MediaAttribute> remoteMediaList)
{
    CPPUNIT_ASSERT_EQUAL(descrList.size(), localMediaList.size());
    CPPUNIT_ASSERT_EQUAL(descrList.size(), remoteMediaList.size());

    for (size_t idx = 0; idx < descrList.size(); idx++) {
        auto local = inferInitialDirection(localMediaList[idx]);
        auto remote = inferInitialDirection(remoteMediaList[idx]);
        auto negotiated = inferNegotiatedDirection(local, remote);

        if (descrList[idx].direction_ != negotiated) {
            JAMI_WARN("Media [%lu] direction mismatch: expected %i - found %i",
                      idx,
                      static_cast<int>(negotiated),
                      static_cast<int>(descrList[idx].direction_));
            return false;
        }
    }

    return true;
}

void
AutoAnswerMediaNegoTest::onIncomingCallWithMedia(const std::string& accountId,
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
AutoAnswerMediaNegoTest::onMediaChangeRequested(const std::string& accountId,
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
AutoAnswerMediaNegoTest::onCallStateChange(const std::string& accountId UNUSED,
                                           const std::string& callId,
                                           const std::string& state,
                                           CallData& callData)
{
    // TODO. rewrite me using accountId.

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
AutoAnswerMediaNegoTest::onVideoMuted(const std::string& callId, bool muted, CallData& callData)
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
              libjami::CallSignal::VideoMuted::name,
              account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
              call->getCallId().c_str(),
              muted ? "Mute" : "Un-mute");

    if (account->getAccountID() != callData.accountId_)
        return;

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(libjami::CallSignal::VideoMuted::name, muted ? "muted" : "un-muted"));
    }

    callData.cv_.notify_one();
}

void
AutoAnswerMediaNegoTest::onMediaNegotiationStatus(const std::string& callId,
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
AutoAnswerMediaNegoTest::waitForSignal(CallData& callData,
                                       const std::string& expectedSignal,
                                       const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {15};
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
AutoAnswerMediaNegoTest::configureScenario()
{
    // Configure Alice
    {
        CPPUNIT_ASSERT(not aliceData_.accountId_.empty());
        auto const& account = Manager::instance().getAccount<Account>(aliceData_.accountId_);
        aliceData_.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData_.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        if (isSipAccount_) {
            auto sipAccount = std::dynamic_pointer_cast<SIPAccount>(account);
            CPPUNIT_ASSERT(sipAccount);
            sipAccount->setLocalPort(aliceData_.listeningPort_);
        }
    }

    // Configure Bob
    {
        CPPUNIT_ASSERT(not bobData_.accountId_.empty());
        auto const& account = Manager::instance().getAccount<Account>(bobData_.accountId_);
        bobData_.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData_.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        CPPUNIT_ASSERT(account->isAutoAnswerEnabled());

        if (isSipAccount_) {
            auto sipAccount = std::dynamic_pointer_cast<SIPAccount>(account);
            CPPUNIT_ASSERT(sipAccount);
            sipAccount->setLocalPort(bobData_.listeningPort_);
            bobData_.toUri_ = "127.0.0.1:" + std::to_string(bobData_.listeningPort_);
        }
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
                                        user == aliceData_.alias_ ? aliceData_ : bobData_);
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
                                       user == aliceData_.alias_ ? aliceData_ : bobData_);
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
                                  user == aliceData_.alias_ ? aliceData_ : bobData_);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onVideoMuted(callId, muted, user == aliceData_.alias_ ? aliceData_ : bobData_);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>&) {
            auto user = getUserAlias(callId);
            if (not user.empty())
                onMediaNegotiationStatus(callId,
                                         event,
                                         user == aliceData_.alias_ ? aliceData_ : bobData_);
        }));

    libjami::registerSignalHandlers(signalHandlers);
}

void
AutoAnswerMediaNegoTest::testWithScenario(CallData& aliceData,
                                          CallData& bobData,
                                          const TestScenario& scenario)
{
    JAMI_INFO("=== Start a call and validate ===");

    // The media count of the offer and answer must match (RFC-3264).
    auto mediaCount = scenario.offer_.size();
    CPPUNIT_ASSERT_EQUAL(mediaCount, scenario.answer_.size());

    aliceData.callId_ = libjami::placeCallWithMedia(aliceData.accountId_,
                                                  isSipAccount_ ? bobData.toUri_
                                                                : bobData_.userName_,
                                                  MediaAttribute::mediaAttributesToMediaMaps(
                                                      scenario.offer_));
    CPPUNIT_ASSERT(not aliceData.callId_.empty());
    auto aliceCall = std::static_pointer_cast<SIPCall>(
        Manager::instance().getCallFromCallID(aliceData.callId_));

    CPPUNIT_ASSERT(aliceCall);

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              aliceData.accountId_.c_str(),
              bobData.accountId_.c_str());

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData, libjami::CallSignal::IncomingCallWithMedia::name));

    // Bob automatically answers the call.

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

    // Validate Alice's media
    {
        auto mediaList = aliceCall->getMediaAttributeList();
        CPPUNIT_ASSERT_EQUAL(mediaCount, mediaList.size());

        // Validate mute state
        CPPUNIT_ASSERT(validateMuteState(scenario.offer_, mediaList));

        auto& sdp = aliceCall->getSDP();

        // Validate local media direction
        {
            auto descrList = sdp.getActiveMediaDescription(false);
            CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
            // For Alice, local is the offer and remote is the answer.
            CPPUNIT_ASSERT(validateMediaDirection(descrList, scenario.offer_, scenario.answer_));
        }
    }

    // Validate Bob's media
    {
        auto const& bobCall = std::dynamic_pointer_cast<SIPCall>(
            Manager::instance().getCallFromCallID(bobData.callId_));
        auto mediaList = bobCall->getMediaAttributeList();
        CPPUNIT_ASSERT_EQUAL(mediaCount, mediaList.size());

        // Validate mute state
        CPPUNIT_ASSERT(validateMuteState(scenario.answer_, mediaList));

        auto& sdp = bobCall->getSDP();

        // Validate local media direction
        {
            auto descrList = sdp.getActiveMediaDescription(false);
            CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
            // For Bob, local is the answer and remote is the offer.
            CPPUNIT_ASSERT(validateMediaDirection(descrList, scenario.answer_, scenario.offer_));
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    JAMI_INFO("=== Request Media Change and validate ===");
    {
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.offerUpdate_);
        libjami::requestMediaChange(aliceData.accountId_, aliceData.callId_, mediaList);
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
            auto mediaList = aliceCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaList.size());

            // Validate mute state
            CPPUNIT_ASSERT(validateMuteState(scenario.offerUpdate_, mediaList));

            auto& sdp = aliceCall->getSDP();

            // Validate local media direction
            {
                auto descrList = sdp.getActiveMediaDescription(false);
                CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
                CPPUNIT_ASSERT(validateMediaDirection(descrList,
                                                      scenario.offerUpdate_,
                                                      scenario.answerUpdate_));
            }
            // Validate remote media direction
            {
                auto descrList = sdp.getActiveMediaDescription(true);
                CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
                CPPUNIT_ASSERT(validateMediaDirection(descrList,
                                                      scenario.answerUpdate_,
                                                      scenario.offerUpdate_));
            }
        }

        // Validate Bob's media
        {
            auto const& bobCall = std::dynamic_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(bobData.callId_));
            auto mediaList = bobCall->getMediaAttributeList();
            CPPUNIT_ASSERT_EQUAL(mediaCount, mediaList.size());

            // Validate mute state
            CPPUNIT_ASSERT(validateMuteState(scenario.answerUpdate_, mediaList));

            // NOTE:
            // It should be enough to validate media direction on Alice's side
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    libjami::hangUp(bobData.accountId_, bobData.callId_);

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       libjami::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
AutoAnswerMediaNegoTest::audio_and_video_then_caller_mute_video()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario();

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
    video.muted_ = true;
    scenario.offerUpdate_.emplace_back(video);

    scenario.answerUpdate_.emplace_back(audio);
    video.muted_ = false;
    scenario.answerUpdate_.emplace_back(video);
    scenario.expectMediaRenegotiation_ = true;

    testWithScenario(aliceData_, bobData_, scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
AutoAnswerMediaNegoTest::audio_only_then_caller_add_video()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario();

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
    scenario.answer_.emplace_back(audio);

    // Updated offer/answer
    scenario.offerUpdate_.emplace_back(audio);
    scenario.offerUpdate_.emplace_back(video);
    scenario.answerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(video);

    testWithScenario(aliceData_, bobData_, scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
AutoAnswerMediaNegoTest::audio_and_video_then_caller_mute_audio()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario();

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
    audio.muted_ = true;
    scenario.offerUpdate_.emplace_back(audio);
    scenario.offerUpdate_.emplace_back(video);

    audio.muted_ = false;
    scenario.answerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(video);

    scenario.expectMediaRenegotiation_ = false;

    testWithScenario(aliceData_, bobData_, scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
AutoAnswerMediaNegoTest::audio_and_video_then_change_video_source()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario();

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
    // Just change the media source to validate that a new
    // media negotiation (re-invite) will be triggered.
    video.sourceUri_ = "Fake source";
    scenario.offerUpdate_.emplace_back(video);

    scenario.answerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(video);

    scenario.expectMediaRenegotiation_ = true;

    testWithScenario(aliceData_, bobData_, scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AutoAnswerMediaNegoTestSip,
                                      AutoAnswerMediaNegoTestSip::name());
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AutoAnswerMediaNegoTestJami,
                                      AutoAnswerMediaNegoTestJami::name());

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::AutoAnswerMediaNegoTestJami::name())
