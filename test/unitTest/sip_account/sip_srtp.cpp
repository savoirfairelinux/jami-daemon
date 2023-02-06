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
class SipSrtpTest : public CppUnit::TestFixture
{
public:
    SipSrtpTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~SipSrtpTest() { libjami::fini(); }

    static std::string name() { return "SipSrtpTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void audio_video_srtp_enabled_test();

    CPPUNIT_TEST_SUITE(SipSrtpTest);
    CPPUNIT_TEST(audio_video_srtp_enabled_test);

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
    void audio_video_call(std::vector<MediaAttribute> offer,
                          std::vector<MediaAttribute> answer,
                          bool validateMedia = true);
    static void configureTest(CallData& bob, CallData& alice);
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

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SipSrtpTest, SipSrtpTest::name());

void
SipSrtpTest::setUp()
{
    aliceData_.listeningPort_ = 5080;
    std::map<std::string, std::string> details = libjami::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::LOCAL_PORT] = std::to_string(aliceData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::SRTP::KEY_EXCHANGE] = "sdes";
    aliceData_.accountId_ = Manager::instance().addAccount(details);

    bobData_.listeningPort_ = 5082;
    details = libjami::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::LOCAL_PORT] = std::to_string(bobData_.listeningPort_);
    details[ConfProperties::UPNP_ENABLED] = "false";
    details[ConfProperties::SRTP::KEY_EXCHANGE] = "sdes";
    bobData_.accountId_ = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize accounts ...");
    auto aliceAccount = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    auto bobAccount = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);
}

void
SipSrtpTest::tearDown()
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
    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    libjami::unregisterSignalHandlers();
}

std::string
SipSrtpTest::getUserAlias(const std::string& callId)
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
SipSrtpTest::onIncomingCallWithMedia(const std::string& accountId,
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
SipSrtpTest::onCallStateChange(const std::string&,
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
    // NOTE. Only states that we are interested on will notify the CV. If this
    // unit test is modified to process other states, they must be added here.
    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP" or state == "RINGING") {
        callData.cv_.notify_one();
    }
}

void
SipSrtpTest::onMediaNegotiationStatus(const std::string& callId,
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
SipSrtpTest::waitForSignal(CallData& callData,
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
SipSrtpTest::configureTest(CallData& aliceData, CallData& bobData)
{
    {
        CPPUNIT_ASSERT(not aliceData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(aliceData.accountId_);
        aliceData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        aliceData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(aliceData.listeningPort_);
    }

    {
        CPPUNIT_ASSERT(not bobData.accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(bobData.accountId_);
        bobData.userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        bobData.alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        account->setLocalPort(bobData.listeningPort_);
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
SipSrtpTest::audio_video_call(std::vector<MediaAttribute> offer,
                              std::vector<MediaAttribute> answer,
                              bool validateMedia)
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureTest(aliceData_, bobData_);

    JAMI_INFO("=== Start a call and validate ===");

    std::string bobUri = "127.0.0.1:" + std::to_string(bobData_.listeningPort_);

    aliceData_.callId_ = libjami::placeCallWithMedia(aliceData_.accountId_,
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
    libjami::hangUp(bobData_.accountId_, bobData_.callId_);

    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, libjami::CallSignal::StateChange::name, StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
SipSrtpTest::audio_video_srtp_enabled_test()
{
    // Test with video enabled on Alice's side and disabled
    // on Bob's side.

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(aliceData_.accountId_);
    CPPUNIT_ASSERT(aliceAcc->isSrtpEnabled());

    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(bobData_.accountId_);
    CPPUNIT_ASSERT(bobAcc->isSrtpEnabled());

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

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SipSrtpTest::name())
