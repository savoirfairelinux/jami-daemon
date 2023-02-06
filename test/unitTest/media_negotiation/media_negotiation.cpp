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
#include "sip/sipaccount.h"
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

    CallData() = default;
    CallData(CallData&& other) = delete;
    CallData(const CallData& other)
    {
        accountId_ = std::move(other.accountId_);
        listeningPort_ = other.listeningPort_;
        userName_ = std::move(other.userName_);
        alias_ = std::move(other.alias_);
        callId_ = std::move(other.callId_);
        signals_ = std::move(other.signals_);
    };

    std::string accountId_ {};
    std::string userName_ {};
    std::string alias_ {};
    uint16_t listeningPort_ {0};
    std::string toUri_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
};

/**
 * Basic tests for media negotiation.
 */
class MediaNegotiationTest
{
public:
    MediaNegotiationTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~MediaNegotiationTest() { libjami::fini(); }

    static std::string name() { return "MediaNegotiationTest"; }

protected:
    // Test cases.
    void audio_and_video_then_caller_mute_video();
    void audio_only_then_caller_add_video();
    void audio_and_video_then_caller_mute_audio();
    void audio_and_video_answer_muted_video_then_mute_video();
    void audio_and_video_then_change_video_source();
    void negotiate_2_videos_1_audio();

    // Event/Signal handlers
    static void onCallStateChange(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& state,
                                  CallData& callData);
    static void onIncomingCallWithMedia(const std::string& accountId,
                                        const std::string& callId,
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData);
    // For backward compatibility test cases.
    // TODO. Do we still need this?
    static void onIncomingCall(const std::string& accountId,
                               const std::string& callId,
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
    std::string getAccountId(const std::string& callId);
    std::string getUserAlias(const std::string& callId);
    // Infer media direction of an offer.
    static uint8_t directionToBitset(MediaDirection direction, bool isLocal);
    static MediaDirection bitsetToDirection(uint8_t val);
    static MediaDirection inferInitialDirection(const MediaAttribute& offer);
    // Infer media direction of an answer.
    static MediaDirection inferNegotiatedDirection(MediaDirection local, MediaDirection answer);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool validateMuteState(std::vector<MediaAttribute> expected,
                                  std::vector<MediaAttribute> actual);
    static bool validateMediaDirection(std::vector<MediaDescription> descrList,
                                       std::vector<MediaAttribute> listInOffer,
                                       std::vector<MediaAttribute> listInAnswer);
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

    bool isSipAccount_ {false};
    std::map<std::string, CallData> callDataMap_;
    std::set<std::string> testAccounts_;
};

// Specialized test case for Jami accounts
class MediaNegotiationTestJami : public MediaNegotiationTest, public CppUnit::TestFixture
{
public:
    MediaNegotiationTestJami() { isSipAccount_ = false; }

    static std::string name() { return "MediaNegotiationTestJami"; }

    void setUp() override
    {
        auto actors = load_actors("actors/alice-bob-no-upnp.yml");
        callDataMap_["ALICE"].accountId_ = actors["alice"];
        callDataMap_["BOB"].accountId_ = actors["bob"];

        JAMI_INFO("Initialize account...");
        auto aliceAccount = Manager::instance().getAccount<JamiAccount>(
            callDataMap_["ALICE"].accountId_);
        auto bobAccount = Manager::instance().getAccount<JamiAccount>(
            callDataMap_["BOB"].accountId_);

        wait_for_announcement_of({aliceAccount->getAccountID(), bobAccount->getAccountID()});
    }

    void tearDown() override
    {
        wait_for_removal_of({callDataMap_["ALICE"].accountId_, callDataMap_["BOB"].accountId_});
    }

private:
    CPPUNIT_TEST_SUITE(MediaNegotiationTestJami);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_video);
    CPPUNIT_TEST(audio_only_then_caller_add_video);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_audio);
    CPPUNIT_TEST(audio_and_video_answer_muted_video_then_mute_video);
    CPPUNIT_TEST(audio_and_video_then_change_video_source);
    CPPUNIT_TEST(negotiate_2_videos_1_audio);
    CPPUNIT_TEST_SUITE_END();
};

// Specialized test case for SIP accounts
class MediaNegotiationTestSip : public MediaNegotiationTest, public CppUnit::TestFixture
{
public:
    MediaNegotiationTestSip() { isSipAccount_ = true; }

    static std::string name() { return "MediaNegotiationTestSip"; }

    bool addTestAccount(const std::string& alias, uint16_t port)
    {
        CallData callData;
        callData.alias_ = alias;
        callData.userName_ = alias;
        callData.listeningPort_ = port;
        std::map<std::string, std::string> details = libjami::getAccountTemplate("SIP");
        details[ConfProperties::TYPE] = "SIP";
        details[ConfProperties::USERNAME] = alias;
        details[ConfProperties::DISPLAYNAME] = alias;
        details[ConfProperties::ALIAS] = alias;
        details[ConfProperties::LOCAL_PORT] = std::to_string(port);
        details[ConfProperties::UPNP_ENABLED] = "false";
        callData.accountId_ = Manager::instance().addAccount(details);
        testAccounts_.insert(callData.accountId_);
        callDataMap_.emplace(alias, std::move(callData));
        return (not callDataMap_[alias].accountId_.empty());
    }

    void setUp() override
    {
        CPPUNIT_ASSERT(addTestAccount("ALICE", 5080));
        CPPUNIT_ASSERT(addTestAccount("BOB", 5082));
    }

    void tearDown() override
    {
        std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
        std::mutex mtx;
        std::unique_lock<std::mutex> lk {mtx};
        std::condition_variable cv;
        std::atomic_bool accountsRemoved {false};
        confHandlers.insert(
            libjami::exportable_callback<libjami::ConfigurationSignal::AccountsChanged>([&]() {
                auto currAccounts = Manager::instance().getAccountList();
                for (auto iter = testAccounts_.begin(); iter != testAccounts_.end();) {
                    auto item = std::find(currAccounts.begin(), currAccounts.end(), *iter);
                    if (item == currAccounts.end()) {
                        JAMI_INFO("Removing account %s", (*iter).c_str());
                        iter = testAccounts_.erase(iter);
                    } else {
                        iter++;
                    }
                }

                if (testAccounts_.empty()) {
                    accountsRemoved = true;
                    JAMI_INFO("All accounts removed...");
                    cv.notify_one();
                }
            }));

        libjami::registerSignalHandlers(confHandlers);

        Manager::instance().removeAccount(callDataMap_["ALICE"].accountId_, true);
        Manager::instance().removeAccount(callDataMap_["BOB"].accountId_, true);
        CPPUNIT_ASSERT(
            cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

        libjami::unregisterSignalHandlers();
    }

private:
    CPPUNIT_TEST_SUITE(MediaNegotiationTestSip);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_video);
    CPPUNIT_TEST(audio_only_then_caller_add_video);
    CPPUNIT_TEST(audio_and_video_then_caller_mute_audio);
    CPPUNIT_TEST(audio_and_video_answer_muted_video_then_mute_video);
    CPPUNIT_TEST(audio_and_video_then_change_video_source);
    CPPUNIT_TEST(negotiate_2_videos_1_audio);
    CPPUNIT_TEST_SUITE_END();
};

std::string
MediaNegotiationTest::getAccountId(const std::string& callId)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    if (not call) {
        JAMI_WARN("Call [%s] does not exist anymore!", callId.c_str());
        return {};
    }

    auto const& account = call->getAccount().lock();

    if (account) {
        return account->getAccountID();
    }

    JAMI_WARN("Account owning the call [%s] does not exist anymore!", callId.c_str());
    return {};
}

std::string
MediaNegotiationTest::getUserAlias(const std::string& accountId)
{
    if (accountId.empty()) {
        JAMI_WARN("No account ID is empty");
        return {};
    }

    auto ret = std::find_if(callDataMap_.begin(), callDataMap_.end(), [accountId](auto const& item) {
        return item.second.accountId_ == accountId;
    });

    if (ret != callDataMap_.end())
        return ret->first;

    JAMI_WARN("No matching test account %s", accountId.c_str());
    return {};
}

MediaDirection
MediaNegotiationTest::inferInitialDirection(const MediaAttribute& mediaAttr)
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
MediaNegotiationTest::directionToBitset(MediaDirection direction, bool isLocal)
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
MediaNegotiationTest::bitsetToDirection(uint8_t val)
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
MediaNegotiationTest::inferNegotiatedDirection(MediaDirection local, MediaDirection remote)
{
    uint8_t val = directionToBitset(local, true) & directionToBitset(remote, false);
    return bitsetToDirection(val);
}

bool
MediaNegotiationTest::validateMuteState(std::vector<MediaAttribute> expected,
                                        std::vector<MediaAttribute> actual)
{
    CPPUNIT_ASSERT_EQUAL(expected.size(), actual.size());

    return std::equal(expected.begin(),
                      expected.end(),
                      actual.begin(),
                      actual.end(),
                      [](auto const& expAttr, auto const& actAttr) {
                          return expAttr.muted_ == actAttr.muted_;
                      });
}

bool
MediaNegotiationTest::validateMediaDirection(std::vector<MediaDescription> descrList,
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
MediaNegotiationTest::onIncomingCallWithMedia(const std::string& accountId,
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
MediaNegotiationTest::onIncomingCall(const std::string& accountId,
                                     const std::string& callId,
                                     CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.accountId_, accountId);

    JAMI_INFO("Signal [%s] - user [%s] - call [%s]",
              libjami::CallSignal::IncomingCall::name,
              callData.alias_.c_str(),
              callId.c_str());

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
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::IncomingCall::name));

    callData.cv_.notify_one();
}

void
MediaNegotiationTest::onMediaChangeRequested(const std::string& accountId,
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
    callData.signals_.emplace_back(CallData::Signal(libjami::CallSignal::MediaChangeRequested::name));

    callData.cv_.notify_one();
}

void
MediaNegotiationTest::onCallStateChange(const std::string& accountId,
                                        const std::string& callId,
                                        const std::string& state,
                                        CallData& callData)
{
    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              libjami::CallSignal::StateChange::name,
              callData.alias_.c_str(),
              callId.c_str(),
              state.c_str());

    CPPUNIT_ASSERT(accountId == callData.accountId_);

    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(libjami::CallSignal::StateChange::name, state));
    }
    // NOTE. Only states that we are interested in will notify the CV.
    // If this unit test is modified to process other states, they must
    // be added here.
    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP" or state == "RINGING") {
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
            JAMI_INFO() << "\tSignal [" << sig.name_
                        << (sig.event_.empty() ? "" : ("::" + sig.event_)) << "]";
        }
    }

    return res;
}

void
MediaNegotiationTest::configureScenario()
{
    // Configure Alice
    {
        CPPUNIT_ASSERT(not callDataMap_["ALICE"].accountId_.empty());
        auto const& account = Manager::instance().getAccount<Account>(
            callDataMap_["ALICE"].accountId_);
        callDataMap_["ALICE"].userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        callDataMap_["ALICE"].alias_ = account->getAccountDetails()[ConfProperties::ALIAS];
        if (isSipAccount_) {
            auto sipAccount = std::dynamic_pointer_cast<SIPAccount>(account);
            CPPUNIT_ASSERT(sipAccount);
            sipAccount->setLocalPort(callDataMap_["ALICE"].listeningPort_);
        }
    }

    // Configure Bob
    {
        CPPUNIT_ASSERT(not callDataMap_["BOB"].accountId_.empty());
        auto const& account = Manager::instance().getAccount<Account>(
            callDataMap_["BOB"].accountId_);
        callDataMap_["BOB"].userName_ = account->getAccountDetails()[ConfProperties::USERNAME];
        callDataMap_["BOB"].alias_ = account->getAccountDetails()[ConfProperties::ALIAS];

        if (isSipAccount_) {
            auto sipAccount = std::dynamic_pointer_cast<SIPAccount>(account);
            CPPUNIT_ASSERT(sipAccount);
            sipAccount->setLocalPort(callDataMap_["BOB"].listeningPort_);
            callDataMap_["BOB"].toUri_ = fmt::format("127.0.0.1:{}",
                                                     callDataMap_["BOB"].listeningPort_);
        }
    }

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> signalHandlers;

    // Insert needed signal handlers.
    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<libjami::MediaMap> mediaList) {
            auto user = getUserAlias(accountId);
            if (not user.empty())
                onIncomingCallWithMedia(accountId, callId, mediaList, callDataMap_[user]);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaChangeRequested>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::vector<libjami::MediaMap> mediaList) {
            auto user = getUserAlias(accountId);
            if (not user.empty())
                onMediaChangeRequested(accountId, callId, mediaList, callDataMap_[user]);
        }));

    signalHandlers.insert(
        libjami::exportable_callback<libjami::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            auto user = getUserAlias(accountId);
            if (not user.empty())
                onCallStateChange(accountId, callId, state, callDataMap_[user]);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) {
            auto user = getUserAlias(getAccountId(callId));
            if (not user.empty())
                onVideoMuted(callId, muted, callDataMap_[user]);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>&) {
            auto user = getUserAlias(getAccountId(callId));
            if (not user.empty())
                onMediaNegotiationStatus(callId, event, callDataMap_[user]);
        }));

    libjami::registerSignalHandlers(signalHandlers);
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

    aliceData.callId_ = libjami::placeCallWithMedia(aliceData.accountId_,
                                                  isSipAccount_ ? bobData.toUri_
                                                                : callDataMap_["BOB"].userName_,
                                                  MediaAttribute::mediaAttributesToMediaMaps(
                                                      scenario.offer_));
    CPPUNIT_ASSERT(not aliceData.callId_.empty());

    auto aliceCall = std::static_pointer_cast<SIPCall>(
        Manager::instance().getCallFromCallID(aliceData.callId_));
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
        libjami::acceptWithMedia(bobData.accountId_, bobData.callId_, mediaList);
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

    // Validate Alice's media
    {
        auto mediaList = aliceCall->getMediaAttributeList();
        CPPUNIT_ASSERT_EQUAL(mediaCount, mediaList.size());

        // Validate mute state
        CPPUNIT_ASSERT(validateMuteState(scenario.offer_, mediaList));

        auto& sdp = aliceCall->getSDP();

        // Validate local media direction
        auto descrList = sdp.getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
        // For Alice, local is the offer and remote is the answer.
        CPPUNIT_ASSERT(validateMediaDirection(descrList, scenario.offer_, scenario.answer_));
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
        auto descrList = sdp.getActiveMediaDescription(false);
        CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
        // For Bob, local is the answer and remote is the offer.
        CPPUNIT_ASSERT(validateMediaDirection(descrList, scenario.answer_, scenario.offer_));
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

    // Not all media change requests requires validation from client.
    if (scenario.expectMediaChangeRequest_) {
        // Wait for media change request signal.
        CPPUNIT_ASSERT_EQUAL(true,
                             waitForSignal(bobData, libjami::CallSignal::MediaChangeRequested::name));

        // Answer the change request.
        auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(scenario.answerUpdate_);
        libjami::answerMediaChangeRequest(bobData.accountId_, bobData.callId_, mediaList);
    }

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
            auto descrList = sdp.getActiveMediaDescription(false);
            CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
            CPPUNIT_ASSERT(
                validateMediaDirection(descrList, scenario.offerUpdate_, scenario.answerUpdate_));
            // Validate remote media direction
            descrList = sdp.getActiveMediaDescription(true);
            CPPUNIT_ASSERT_EQUAL(mediaCount, descrList.size());
            CPPUNIT_ASSERT(
                validateMediaDirection(descrList, scenario.answerUpdate_, scenario.offerUpdate_));
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
MediaNegotiationTest::audio_and_video_then_caller_mute_video()
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
    scenario.expectMediaChangeRequest_ = false;

    testWithScenario(callDataMap_["ALICE"], callDataMap_["BOB"], scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::audio_only_then_caller_add_video()
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
    video.muted_ = true;
    scenario.answerUpdate_.emplace_back(video);
    scenario.expectMediaRenegotiation_ = true;
    scenario.expectMediaChangeRequest_ = true;

    testWithScenario(callDataMap_["ALICE"], callDataMap_["BOB"], scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::audio_and_video_then_caller_mute_audio()
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
    scenario.expectMediaChangeRequest_ = false;

    testWithScenario(callDataMap_["ALICE"], callDataMap_["BOB"], scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::audio_and_video_answer_muted_video_then_mute_video()
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
    video.muted_ = true;
    scenario.answer_.emplace_back(audio);
    scenario.answer_.emplace_back(video);

    // Updated offer/answer
    video.muted_ = true;
    scenario.offerUpdate_.emplace_back(audio);
    scenario.offerUpdate_.emplace_back(video);

    video.muted_ = true;
    scenario.answerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(video);

    scenario.expectMediaChangeRequest_ = false;
    scenario.expectMediaRenegotiation_ = true;

    testWithScenario(callDataMap_["ALICE"], callDataMap_["BOB"], scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::audio_and_video_then_change_video_source()
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
    scenario.expectMediaChangeRequest_ = false;

    testWithScenario(callDataMap_["ALICE"], callDataMap_["BOB"], scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

void
MediaNegotiationTest::negotiate_2_videos_1_audio()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    configureScenario();

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "audio_0";
    defaultAudio.enabled_ = true;

    MediaAttribute defaultVideo(MediaType::MEDIA_VIDEO);
    defaultVideo.label_ = "video_0";
    defaultVideo.sourceUri_ = "foo";
    defaultVideo.enabled_ = true;

    MediaAttribute defaultVideo2(MediaType::MEDIA_VIDEO);
    defaultVideo2.label_ = "video_1";
    defaultVideo2.sourceUri_ = "bar";
    defaultVideo2.enabled_ = true;

    MediaAttribute audio(defaultAudio);
    MediaAttribute video(defaultVideo);
    MediaAttribute video2(defaultVideo2);

    TestScenario scenario;
    // First offer/answer
    scenario.offer_.emplace_back(audio);
    scenario.offer_.emplace_back(video);
    scenario.answer_.emplace_back(audio);
    scenario.answer_.emplace_back(video);

    // Update offer/answer with 2 videos
    scenario.offerUpdate_.emplace_back(audio);
    scenario.offerUpdate_.emplace_back(video);
    scenario.offerUpdate_.emplace_back(video2);
    scenario.answerUpdate_.emplace_back(audio);
    scenario.answerUpdate_.emplace_back(video);
    scenario.answerUpdate_.emplace_back(video2);

    scenario.expectMediaRenegotiation_ = true;
    scenario.expectMediaChangeRequest_ = true;

    testWithScenario(callDataMap_["ALICE"], callDataMap_["BOB"], scenario);

    libjami::unregisterSignalHandlers();

    JAMI_INFO("=== End test %s ===", __FUNCTION__);
}

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaNegotiationTestJami, MediaNegotiationTestJami::name());
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaNegotiationTestSip, MediaNegotiationTestSip::name());

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::MediaNegotiationTestJami::name())
