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
#include "account_const.h"
#include "sip/sipcall.h"
#include "sip/sdp.h"

using namespace DRing::Account;

namespace jami {
namespace test {

enum class CallEvents {
    CALL_NOP,
    CALL_RECEIVED,
    CALL_ANSWERED,
    CALL_INITIATED,
    CALL_UPDATED,
    CALL_TERMINATED
};

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
    static std::string name() { return "Call"; }
    void setUp();
    void tearDown();

    std::string aliceAccountId_;
    std::string bobAccountId_;

    struct CallData
    {
        std::shared_ptr<SIPCall> sipCall_;
        CallEvents event_ {CallEvents::CALL_NOP};
    };

private:
    void testCallWithMediaList();

    CPPUNIT_TEST_SUITE(MediaControlTest);
    CPPUNIT_TEST(testCallWithMediaList);
    CPPUNIT_TEST_SUITE_END();
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

void
MediaControlTest::testCallWithMediaList()
{
#if 1
    JAMI_INFO("Waiting....");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> signalHandlers;

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceAccountId_);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobAccountId_);
    auto bobUri = bobAccount->getAccountDetails()[ConfProperties::USERNAME];
    auto aliceUri = aliceAccount->getAccountDetails()[ConfProperties::USERNAME];

    CallData aliceCall;
    CallData bobCall;

    // Insert needed signal handlers.

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string& accountId, const std::string& callId, const std::string&) {
            CPPUNIT_ASSERT_EQUAL(bobAccountId_, accountId);
            JAMI_DBG("Bob [%s] received an incoming call [%s]", accountId.c_str(), callId.c_str());
            auto call = Manager::instance().getCallFromCallID(callId);
            CPPUNIT_ASSERT_EQUAL(accountId, bobAccountId_);
            call->answer();
            bobCall.sipCall_ = std::dynamic_pointer_cast<SIPCall>(call);
            bobCall.event_ = CallEvents::CALL_RECEIVED;
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string& callId, const std::string& state, signed) {
            auto call = Manager::instance().getCallFromCallID(callId);
            if (not call or call->isSubcall())
                return;
            auto account = call->getAccount().lock();

            if (account->getAccountID() != aliceAccount->getAccountID())
                return;

            if (state == "CURRENT") {
                aliceCall.event_ = CallEvents::CALL_ANSWERED;
                cv.notify_one();
            }

            if (state == "OVER" or state == "HUNGUP") {
                aliceCall.event_ = CallEvents::CALL_TERMINATED;
                cv.notify_one();
            }

            JAMI_DBG("State of Alice call [%s] changed to [%s]", callId.c_str(), state.c_str());
        }));

    signalHandlers.insert(DRing::exportable_callback<DRing::CallSignal::VideoMuted>(
        [&](const std::string& callId, bool muted) {
            auto call = Manager::instance().getCallFromCallID(callId);
            if (not call or call->isSubcall())
                return;
            auto account = call->getAccount().lock();

            if (account->getAccountID() != aliceAccount->getAccountID())
                return;

            CPPUNIT_ASSERT(muted);

            JAMI_INFO("Alice muted the video on call [%s]", bobCall.sipCall_->getCallId().c_str());

            cv.notify_one();
        }));

    DRing::registerSignalHandlers(signalHandlers);

    // Create the list of medias
    std::vector<std::map<std::string, std::string>> mediaList;

    std::map<std::string, std::string> audioMap;
    audioMap.emplace("MEDIA_TYPE", MediaAttributeValue::AUDIO);
    audioMap.emplace("ENABLED", "true");
    audioMap.emplace("MUTED", "false");
    audioMap.emplace("SECURE", "true");
    audioMap.emplace("LABEL", "main audio");

    std::map<std::string, std::string> videoMap;
    videoMap.emplace("MEDIA_TYPE", MediaAttributeValue::VIDEO);
    videoMap.emplace("ENABLED", "true");
    videoMap.emplace("MUTED", "false");
    videoMap.emplace("SECURE", "true");
    videoMap.emplace("LABEL", "main video");

    mediaList.emplace_back(audioMap);
    mediaList.emplace_back(videoMap);

    JAMI_INFO("Alice [%s] calls Bob [%s]", aliceAccountId_.c_str(), bobAccountId_.c_str());

    aliceCall.sipCall_ = std::dynamic_pointer_cast<SIPCall>(
        aliceAccount->newOutgoingCall(bobUri, mediaList));
    assert(aliceCall.sipCall_);

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] {
        return aliceCall.event_ == CallEvents::CALL_ANSWERED;
    }));

    JAMI_INFO("Bob answered the call [%s]", bobCall.sipCall_->getCallId().c_str());

    auto aliceLocalMedia = aliceCall.sipCall_->getSDP().getLocalMediaDescriptions();
    CPPUNIT_ASSERT_EQUAL(mediaList.size(), aliceLocalMedia.size());

    auto bobLocalMedia = bobCall.sipCall_->getSDP().getLocalMediaDescriptions();
    CPPUNIT_ASSERT_EQUAL(mediaList.size(), bobLocalMedia.size());

    CPPUNIT_ASSERT(bobLocalMedia[0].enabled);
    CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_AUDIO, bobLocalMedia[0].type);
    CPPUNIT_ASSERT(not bobLocalMedia[0].onHold);
    CPPUNIT_ASSERT(bobLocalMedia[0].addr);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Mute the video.
    auto const& it = mediaList[1].find("MUTED");
    it->second = "true";
    Manager::instance().updateMediaStreams(aliceCall.sipCall_->getCallId(), mediaList);

    // Wait for the VideoMute signal.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] {
        return not aliceCall.sipCall_->getVideoRtp()->isSending();
    }));

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Bob hang-up.
    Manager::instance().hangupCall(bobCall.sipCall_->getCallId());

    JAMI_INFO("Bob hang-up. Waiting for Alice to hang up");

    Manager::instance().hangupCall(aliceCall.sipCall_->getCallId());

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] {
        return aliceCall.event_ == CallEvents::CALL_TERMINATED;
    }));

    JAMI_INFO("Call terminated on both sides");
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MediaControlTest::name())
