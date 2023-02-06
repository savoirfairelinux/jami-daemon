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
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~SipBasicCallTest() { libjami::fini(); }

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
                                        const std::vector<libjami::MediaMap> mediaList,
                                        CallData& callData);
    static void onMediaNegotiationStatus(const std::string& callId,
                                         const std::string& event,
                                         CallData& callData);

    // Helpers
    bool addTestAccount(const std::string& alias, uint16_t port);
    void audio_video_call(std::vector<MediaAttribute> offer,
                          std::vector<MediaAttribute> answer,
                          bool expectedToSucceed = true,
                          bool validateMedia = true);
    void configureTest();
    std::string getAccountId(const std::string& callId);
    std::string getUserAlias(const std::string& accountId);
    CallData& getCallData(const std::string& account);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

    std::map<std::string, CallData> callDataMap_;
    std::set<std::string> testAccounts_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SipBasicCallTest, SipBasicCallTest::name());

bool
SipBasicCallTest::addTestAccount(const std::string& alias, uint16_t port)
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

void
SipBasicCallTest::setUp()
{
    JAMI_INFO("Initialize accounts ...");

    CPPUNIT_ASSERT(addTestAccount("ALICE", 5080));
    CPPUNIT_ASSERT(addTestAccount("BOB", 5082));
    CPPUNIT_ASSERT(addTestAccount("CARLA", 5084));
}

void
SipBasicCallTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");

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
    Manager::instance().removeAccount(callDataMap_["CARLA"].accountId_, true);

    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    libjami::unregisterSignalHandlers();
}

std::string
SipBasicCallTest::getAccountId(const std::string& callId)
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
SipBasicCallTest::getUserAlias(const std::string& accountId)
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

void
SipBasicCallTest::onIncomingCallWithMedia(const std::string& accountId,
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
SipBasicCallTest::onCallStateChange(const std::string& accountId,
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
            JAMI_INFO() << "\tSignal [" << sig.name_
                        << (sig.event_.empty() ? "" : ("::" + sig.event_)) << "]";
        }
    }

    return res;
}

void
SipBasicCallTest::configureTest()
{
    {
        CPPUNIT_ASSERT(not callDataMap_["ALICE"].accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(
            callDataMap_["ALICE"].accountId_);
        account->setLocalPort(callDataMap_["ALICE"].listeningPort_);
    }

    {
        CPPUNIT_ASSERT(not callDataMap_["BOB"].accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(
            callDataMap_["BOB"].accountId_);
        account->setLocalPort(callDataMap_["BOB"].listeningPort_);
    }

    {
        CPPUNIT_ASSERT(not callDataMap_["CARLA"].accountId_.empty());
        auto const& account = Manager::instance().getAccount<SIPAccount>(
            callDataMap_["CARLA"].accountId_);
        account->setLocalPort(callDataMap_["CARLA"].listeningPort_);
    }

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> signalHandlers;

    // Insert needed signal handlers.
    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<libjami::MediaMap> mediaList) {
            auto alias = getUserAlias(accountId);
            if (not alias.empty()) {
                onIncomingCallWithMedia(accountId, callId, mediaList, callDataMap_[alias]);
            }
        }));

    signalHandlers.insert(
        libjami::exportable_callback<libjami::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            auto alias = getUserAlias(accountId);
            if (not alias.empty())
                onCallStateChange(accountId, callId, state, callDataMap_[alias]);
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>& /* mediaList */) {
            auto alias = getUserAlias(getAccountId(callId));
            if (not alias.empty())
                onMediaNegotiationStatus(callId, event, callDataMap_[alias]);
        }));

    libjami::registerSignalHandlers(signalHandlers);
}

void
SipBasicCallTest::audio_video_call(std::vector<MediaAttribute> offer,
                                   std::vector<MediaAttribute> answer,
                                   bool expectedToSucceed,
                                   bool validateMedia)
{
    configureTest();

    JAMI_INFO("=== Start a call and validate ===");

    std::string bobUri = callDataMap_["BOB"].userName_
                         + "@127.0.0.1:" + std::to_string(callDataMap_["BOB"].listeningPort_);

    callDataMap_["ALICE"].callId_
        = libjami::placeCallWithMedia(callDataMap_["ALICE"].accountId_,
                                    bobUri,
                                    MediaAttribute::mediaAttributesToMediaMaps(offer));

    CPPUNIT_ASSERT(not callDataMap_["ALICE"].callId_.empty());

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              callDataMap_["ALICE"].accountId_.c_str(),
              callDataMap_["BOB"].accountId_.c_str());

    // Give it some time to ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Wait for call to be processed.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(
        waitForSignal(callDataMap_["BOB"], libjami::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    libjami::acceptWithMedia(callDataMap_["BOB"].accountId_,
                           callDataMap_["BOB"].callId_,
                           MediaAttribute::mediaAttributesToMediaMaps(answer));

    if (expectedToSucceed) {
        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["BOB"],
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::CURRENT));

        JAMI_INFO("BOB answered the call [%s]", callDataMap_["BOB"].callId_.c_str());

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["ALICE"],
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        // Validate Alice's media
        if (validateMedia) {
            auto call = Manager::instance().getCallFromCallID(callDataMap_["ALICE"].callId_);
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
            auto call = Manager::instance().getCallFromCallID(callDataMap_["BOB"].callId_);
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
        libjami::hangUp(callDataMap_["BOB"].accountId_, callDataMap_["BOB"].callId_);
    } else {
        // The media negotiation for the call is expected to fail, so we
        // should receive the signal.
        CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::FAILURE));
    }

    // The hang-up signal will be emitted on caller's side (Alice) in both
    // success failure scenarios.

    CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
SipBasicCallTest::audio_only_test()
{
    // Test with video enabled on Alice's side and disabled
    // on Bob's side.

    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(
        callDataMap_["ALICE"].accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(callDataMap_["BOB"].accountId_);

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

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(
        callDataMap_["ALICE"].accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(callDataMap_["BOB"].accountId_);

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

    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(callDataMap_["BOB"].accountId_);

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

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(
        callDataMap_["ALICE"].accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(callDataMap_["BOB"].accountId_);

    std::vector<MediaAttribute> offer;
    std::vector<MediaAttribute> answer;

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    MediaAttribute video(MediaType::MEDIA_VIDEO);

    audio.enabled_ = true;
    audio.label_ = "audio_0";
    video.enabled_ = true;
    video.label_ = "video_0";

    // Alice's media
    offer.emplace_back(audio);
    offer.emplace_back(video);

    // Bob's media
    answer.emplace_back(audio);
    answer.emplace_back(video);

    {
        configureTest();

        JAMI_INFO("=== Start a call and validate ===");

        std::string bobUri = callDataMap_["BOB"].userName_
                             + "@127.0.0.1:" + std::to_string(callDataMap_["BOB"].listeningPort_);

        callDataMap_["ALICE"].callId_
            = libjami::placeCallWithMedia(callDataMap_["ALICE"].accountId_,
                                        bobUri,
                                        MediaAttribute::mediaAttributesToMediaMaps(offer));

        CPPUNIT_ASSERT(not callDataMap_["ALICE"].callId_.empty());

        JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
                  callDataMap_["ALICE"].accountId_.c_str(),
                  callDataMap_["BOB"].accountId_.c_str());

        // Give it some time to ring
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Wait for call to be processed.
        CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::RINGING));

        // Wait for incoming call signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["BOB"], libjami::CallSignal::IncomingCallWithMedia::name));

        // Answer the call.
        libjami::acceptWithMedia(callDataMap_["BOB"].accountId_,
                               callDataMap_["BOB"].callId_,
                               MediaAttribute::mediaAttributesToMediaMaps(answer));

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["BOB"],
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::CURRENT));

        JAMI_INFO("BOB answered the call [%s]", callDataMap_["BOB"].callId_.c_str());

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["ALICE"],
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

        // Validate Alice's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(callDataMap_["ALICE"].callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.first.direction_);
                CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.second.direction_);
            }
        }

        // Validate Bob's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(callDataMap_["BOB"].callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.first.direction_);
                CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.second.direction_);
            }
        }

        // Give some time to media to start and flow
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Hold/Resume the call
        JAMI_INFO("Hold Alice's call");
        libjami::hold(callDataMap_["ALICE"].accountId_, callDataMap_["ALICE"].callId_);
        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::HOLD));

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["ALICE"],
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        {
            // Validate hold state.
            auto call = Manager::instance().getCallFromCallID(callDataMap_["ALICE"].callId_);
            auto activeMediaList = call->getMediaAttributeList();
            for (const auto& mediaAttr : activeMediaList) {
                CPPUNIT_ASSERT(mediaAttr.onHold_);
            }
        }

        // Validate Alice's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(callDataMap_["ALICE"].callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                if (media.first.type == MediaType::MEDIA_AUDIO) {
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.first.direction_);
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.second.direction_);
                } else {
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDONLY, media.first.direction_);
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::RECVONLY, media.second.direction_);
                }
            }
        }

        // Validate Bob's side of media direction
        {
            auto call = std::static_pointer_cast<SIPCall>(
                Manager::instance().getCallFromCallID(callDataMap_["BOB"].callId_));
            auto& sdp = call->getSDP();
            auto mediaStreams = sdp.getMediaSlots();
            for (auto const& media : mediaStreams) {
                if (media.first.type == MediaType::MEDIA_AUDIO) {
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.first.direction_);
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.second.direction_);
                } else {
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::RECVONLY, media.first.direction_);
                    CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDONLY, media.second.direction_);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));

        JAMI_INFO("Resume Alice's call");
        libjami::unhold(callDataMap_["ALICE"].accountId_, callDataMap_["ALICE"].callId_);

        // Wait for the StateChange signal.
        CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::CURRENT));

        // Wait for media negotiation complete signal.
        CPPUNIT_ASSERT(
            waitForSignal(callDataMap_["ALICE"],
                          libjami::CallSignal::MediaNegotiationStatus::name,
                          libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
        std::this_thread::sleep_for(std::chrono::seconds(2));

        {
            // Validate hold state.
            auto call = Manager::instance().getCallFromCallID(callDataMap_["ALICE"].callId_);
            auto activeMediaList = call->getMediaAttributeList();
            for (const auto& mediaAttr : activeMediaList) {
                CPPUNIT_ASSERT(not mediaAttr.onHold_);
            }
        }

        // Bob hang-up.
        JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
        libjami::hangUp(callDataMap_["BOB"].accountId_, callDataMap_["BOB"].callId_);

        // The hang-up signal will be emitted on caller's side (Alice) in both
        // success and failure scenarios.

        CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                     libjami::CallSignal::StateChange::name,
                                     StateEvent::HUNGUP));

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

    auto const aliceAcc = Manager::instance().getAccount<SIPAccount>(
        callDataMap_["ALICE"].accountId_);
    auto const bobAcc = Manager::instance().getAccount<SIPAccount>(callDataMap_["BOB"].accountId_);
    auto const carlaAcc = Manager::instance().getAccount<SIPAccount>(
        callDataMap_["CARLA"].accountId_);

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

    configureTest();

    JAMI_INFO("=== Start a call and validate ===");

    std::string bobUri = callDataMap_["BOB"].userName_
                         + "@127.0.0.1:" + std::to_string(callDataMap_["BOB"].listeningPort_);

    callDataMap_["ALICE"].callId_
        = libjami::placeCallWithMedia(callDataMap_["ALICE"].accountId_,
                                    bobUri,
                                    MediaAttribute::mediaAttributesToMediaMaps(offer));

    CPPUNIT_ASSERT(not callDataMap_["ALICE"].callId_.empty());

    JAMI_INFO("ALICE [%s] started a call with BOB [%s] and wait for answer",
              callDataMap_["ALICE"].accountId_.c_str(),
              callDataMap_["BOB"].accountId_.c_str());

    // Give it some time to ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Wait for call to be processed.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(
        waitForSignal(callDataMap_["BOB"], libjami::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    libjami::acceptWithMedia(callDataMap_["BOB"].accountId_,
                           callDataMap_["BOB"].callId_,
                           MediaAttribute::mediaAttributesToMediaMaps(answer));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", callDataMap_["BOB"].callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Give some time to media to start and flow
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Transfer the call to Carla
    std::string carlaUri = carlaAcc->getUsername()
                           + "@127.0.0.1:" + std::to_string(callDataMap_["CARLA"].listeningPort_);

    libjami::transfer(callDataMap_["ALICE"].accountId_,
                    callDataMap_["ALICE"].callId_,
                    carlaUri); // TODO. Check trim

    // Expect Alice's call to end.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["ALICE"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::HUNGUP));

    // Wait for the new call to be processed.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(
        waitForSignal(callDataMap_["CARLA"], libjami::CallSignal::IncomingCallWithMedia::name));

    // Let it ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Carla answers the call.
    libjami::acceptWithMedia(callDataMap_["CARLA"].accountId_,
                           callDataMap_["CARLA"].callId_,
                           MediaAttribute::mediaAttributesToMediaMaps(answer));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["CARLA"],
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["CARLA"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::CURRENT));

    JAMI_INFO("CARLA answered the call [%s]", callDataMap_["BOB"].callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));
    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["BOB"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::CURRENT));

    // Validate Carla's side of media direction
    {
        auto call = std::static_pointer_cast<SIPCall>(
            Manager::instance().getCallFromCallID(callDataMap_["CARLA"].callId_));
        auto& sdp = call->getSDP();
        auto mediaStreams = sdp.getMediaSlots();
        for (auto const& media : mediaStreams) {
            CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.first.direction_);
            CPPUNIT_ASSERT_EQUAL(MediaDirection::SENDRECV, media.second.direction_);
        }
    }

    // NOTE:
    // For now, we dont validate Bob's media because currently
    // test does not update BOB's call ID (callDataMap_["BOB"].callId_
    // still point to the first call).
    // It seems there is no easy way to get the ID of the new call
    // made by Bob to Carla.

    // Give some time to media to start and flow
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Bob hang-up.
    JAMI_INFO("Hang up CARLA's call and wait for CARLA to hang up");
    libjami::hangUp(callDataMap_["CARLA"].accountId_, callDataMap_["CARLA"].callId_);

    // Expect end call on Carla's side.
    CPPUNIT_ASSERT(waitForSignal(callDataMap_["CARLA"],
                                 libjami::CallSignal::StateChange::name,
                                 StateEvent::HUNGUP));

    JAMI_INFO("Calls normally ended on both sides");
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SipBasicCallTest::name())
