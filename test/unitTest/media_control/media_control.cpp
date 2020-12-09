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
    std::string userName_ {};
    std::string alias_ {};
    std::shared_ptr<JamiAccount> account_;
    std::shared_ptr<SIPCall> sipCall_;
    std::vector<MediaAttribute> mediaAttrList_ {};
    std::string signal_ {};
    std::string event_ {};
    std::condition_variable cv_ {};
};

struct TestScenario
{
    struct MediaState
    {
        MediaState(bool audio, bool video)
            : audioMuted_(audio)
            , videoMuted_(video) {};
        MediaState() = delete;

        // "false" if muted, "true" otherwise
        bool audioMuted_ {false};
        bool videoMuted_ {false};
    };

    TestScenario(const MediaState& offer,
                 const MediaState& answer,
                 const MediaState& offerUpdate,
                 const MediaState& answerUpdate)
        : offer_(std::move(offer))
        , answer_(std::move(answer))
        , offerUpdate_(std::move(offerUpdate))
        , answerUpdate_(std::move(answerUpdate))
    {}

    TestScenario() = delete;

    MediaState offer_;
    MediaState answer_;
    MediaState offerUpdate_;
    MediaState answerUpdate_;
};

using MediaMap = std::map<std::string, std::string>;

/**
 * Basic tests for media negotiation.
 */
class MediaControlTest : public CppUnit::TestFixture
{
public:
    MediaControlTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~MediaControlTest() { DRing::fini(); }

    static std::string name() { return "MediaControlTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    void testCallWithMediaList();

    CPPUNIT_TEST_SUITE(MediaControlTest);
    CPPUNIT_TEST(testCallWithMediaList);
    CPPUNIT_TEST_SUITE_END();

    // Helpers
    MediaMap createMediaMap(const char* type, bool muted, const char* label);
    void testWithScenario(CallData& aliceData, CallData& bobData, const TestScenario& scenario);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    bool waitForSignal(CallData& callData,
                       const std::string& signal,
                       const std::string& expectedEvent = {});

    // Event/Signal handlers
    void onCallStateChange(const std::string& callId, const std::string& state, CallData& callData);
    void onIncomingCall(const std::string& accountId, const std::string& callId, CallData& callData);
    void onVideoMuted(const std::string& callId, bool muted, CallData& callData);

private:
    std::string aliceAccountId_;
    std::string bobAccountId_;
    std::mutex mtx_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaControlTest, MediaControlTest::name());

void
MediaControlTest::setUp()
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
MediaControlTest::tearDown()
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
MediaControlTest::createMediaMap(const char* type, bool muted, const char* label)
{
    std::map<std::string, std::string> mediaMap;
    mediaMap.emplace("MEDIA_TYPE", type);
    mediaMap.emplace("ENABLED", "true");
    muted ? mediaMap.emplace("MUTED", "true") : mediaMap.emplace("MUTED", "false");
    mediaMap.emplace("SECURE", "true");
    mediaMap.emplace("LABEL", label);

    return mediaMap;
}

void
MediaControlTest::onIncomingCall(const std::string& accountId,
                                 const std::string& callId,
                                 CallData& callData)
{
    CPPUNIT_ASSERT_EQUAL(callData.account_->getAccountID(), accountId);

    JAMI_DBG("%s [%s] received an incoming call [%s]",
             callData.alias_.c_str(),
             accountId.c_str(),
             callId.c_str());
    auto call = Manager::instance().getCallFromCallID(callId);

    call->answer();

    callData.sipCall_ = std::dynamic_pointer_cast<SIPCall>(call);
    callData.event_ = StateEvent::INCOMING;
}

void
MediaControlTest::onCallStateChange(const std::string& callId,
                                    const std::string& state,
                                    CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    // Interested only in main (parent) call.
    if (not call or call->isSubcall())
        return;

    auto account = call->getAccount().lock();
    assert(account);

    JAMI_DBG("User [%s] - Call [%s] - State [%s]",
             account->getAccountDetails()[ConfProperties::ALIAS].c_str(),
             callId.c_str(),
             state.c_str());

    if (account->getAccountID() != callData.account_->getAccountID())
        return;

    callData.event_ = state;
    callData.signal_ = DRing::CallSignal::StateChange::name;

    if (state == "CURRENT" or state == "OVER" or state == "HUNGUP") {
        callData.cv_.notify_one();
    }
}

void
MediaControlTest::onVideoMuted(const std::string& callId, bool muted, CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call or call->isSubcall())
        return;
    auto account = call->getAccount().lock();
    assert(account);

    JAMI_INFO("Received video-muted signal on call [%s] for user [%s]",
              call->getCallId().c_str(),
              account->getAccountDetails()[ConfProperties::ALIAS].c_str());

    if (account->getAccountID() != callData.account_->getAccountID())
        return;

    CPPUNIT_ASSERT_EQUAL(true, muted);

    call->getMediaAttributeList(callData.mediaAttrList_);
    CPPUNIT_ASSERT_EQUAL(2ul, callData.mediaAttrList_.size());

    callData.signal_ = DRing::CallSignal::VideoMuted::name;
    callData.cv_.notify_one();
}

bool
MediaControlTest::waitForSignal(CallData& callData,
                                const std::string& expectedSignal,
                                const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {20};
    std::unique_lock<std::mutex> lock {mtx_};

    auto res = callData.cv_.wait_for(lock, TIME_OUT, [&] {
        bool pred = callData.signal_ == expectedSignal;
        if (not expectedEvent.empty()) {
            pred = pred and callData.event_ == expectedEvent;
        }
        return pred;
    });

    if (not res) {
        std::string msg(expectedSignal);
        if (not expectedEvent.empty()) {
            msg.append("::");
            msg.append(expectedEvent);
        }

        JAMI_ERR("Waiting for Signal [%s] timed-out!", expectedSignal.c_str());
    }

    return res;
}
void
MediaControlTest::testWithScenario(CallData& aliceData,
                                   CallData& bobData,
                                   const TestScenario& scenario)
{
    // Create the list of medias
    std::vector<std::map<std::string, std::string>> mediaList;
    // Always add audio even if not enabled or muted
    auto audioMap = createMediaMap(MediaAttributeValue::AUDIO,
                                   scenario.offer_.audioMuted_,
                                   "main audio");
    mediaList.emplace_back(audioMap);

    // Video is added in first offer only if not muted
    if (not scenario.offer_.videoMuted_) {
        auto videoMap = createMediaMap(MediaAttributeValue::VIDEO,
                                       scenario.offer_.videoMuted_,
                                       "main video");
        mediaList.emplace_back(videoMap);
    }

    JAMI_INFO("ALICE [%s] calls Bob [%s] and wait for BOB to answer",
              aliceData.account_->getAccountID().c_str(),
              bobData.account_->getAccountID().c_str());

    aliceData.sipCall_ = std::dynamic_pointer_cast<SIPCall>(
        aliceData.account_->newOutgoingCall(bobData.userName_, mediaList));
    assert(aliceData.sipCall_);

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData.sipCall_->getCallId().c_str());

    auto aliceLocalMedia = aliceData.sipCall_->getSDP().getLocalMediaDescriptions();
    CPPUNIT_ASSERT_EQUAL(mediaList.size(), aliceLocalMedia.size());

    auto bobLocalMedia = bobData.sipCall_->getSDP().getLocalMediaDescriptions();
    CPPUNIT_ASSERT_EQUAL(mediaList.size(), bobLocalMedia.size());

    CPPUNIT_ASSERT(bobLocalMedia[0].enabled);
    CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_AUDIO, bobLocalMedia[0].type);
    CPPUNIT_ASSERT(not bobLocalMedia[0].onHold);
    CPPUNIT_ASSERT(bobLocalMedia[0].addr);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Update the media.
    mediaList[0]["MUTED"] = scenario.offerUpdate_.audioMuted_ ? "true" : "false";

    // Check first that video media exist before updating.
    if (mediaList.size() > 1) {
        mediaList[1]["MUTED"] = scenario.offerUpdate_.videoMuted_ ? "true" : "false";
    }

    Manager::instance().updateMediaStreams(aliceData.sipCall_->getCallId(), mediaList);

    // Wait for the VideoMute signal.
    JAMI_INFO("Waiting for the video muted signal");
#if 1
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Waiting for video-muted timed out!",
                                 true,
                                 waitForSignal(aliceData, DRing::CallSignal::VideoMuted::name));
#else
    CPPUNIT_ASSERT_MESSAGE("Waiting for video-muted timed out!",
                           aliceData.cv_.wait_for(lock, std::chrono::seconds(20), [&] {
                               return not aliceData.mediaAttrList_[1].muted_;
                           }));
#endif
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData.sipCall_->getCallId());

    // Manager::instance().hangupCall(aliceData.sipCall_->getCallId());

    CPPUNIT_ASSERT_EQUAL(true,
                         waitForSignal(aliceData,
                                       DRing::CallSignal::StateChange::name,
                                       StateEvent::HUNGUP));

    JAMI_INFO("Call terminated on both sides");
}

void
MediaControlTest::testCallWithMediaList()
{
#if 1
    JAMI_INFO("Waiting....");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif

    // initTestScenarios();

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
            onIncomingCall(accountId, callId, bobData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            onCallStateChange(callId, state, aliceData);
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) { onVideoMuted(callId, muted, aliceData); }));

    DRing::registerSignalHandlers(signalHandlers);

    TestScenario scenario({false, false}, {false, false}, {false, true}, {false, true});

    testWithScenario(aliceData, bobData, scenario);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MediaControlTest::name())
