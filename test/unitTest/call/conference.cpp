/*
 *  Copyright (C)2021 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

using namespace DRing::Account;

namespace jami {
namespace test {

class ConferenceTest : public CppUnit::TestFixture
{
public:
    ConferenceTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~ConferenceTest() { DRing::fini(); }
    static std::string name() { return "Call"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string carlaId;

private:
    void testGetConference();
    void testModeratorMuteUpdateParticipantsInfos();

    CPPUNIT_TEST_SUITE(ConferenceTest);
    CPPUNIT_TEST(testGetConference);
    CPPUNIT_TEST(testModeratorMuteUpdateParticipantsInfos);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConferenceTest, ConferenceTest::name());

void
ConferenceTest::setUp()
{
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "CARLA";
    details[ConfProperties::ALIAS] = "CARLA";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    carlaId = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
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
                details = carlaAccount->getVolatileAccountDetails();
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
ConferenceTest::tearDown()
{
    DRing::unregisterSignalHandlers();
    JAMI_INFO("Remove created accounts...");

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    std::atomic_bool accountsRemoved {false};
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - 3) {
                accountsRemoved = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);
    Manager::instance().removeAccount(carlaId, true);
    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}

void
ConferenceTest::testGetConference()
{
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::string bobCall, carlaCall, confId;
    std::atomic_bool callOngoing {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string& accountId, const std::string& callId, const std::string&) {
            if (accountId == bobId)
                bobCall = callId;
            else if (accountId == carlaId)
                carlaCall = callId;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
            } else if (state == "CURRENT") {
                callOngoing = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::ConferenceCreated>(
        [&](const std::string& conferenceId) {
            confId = conferenceId;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 0);

    JAMI_INFO("Start call between Alice and Bob");
    auto call1 = aliceAccount->newOutgoingCall(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !bobCall.empty(); }));
    Manager::instance().answerCall(bobCall);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return callOngoing.load(); }));

    JAMI_INFO("Start call between Alice and Carla");
    auto call2 = aliceAccount->newOutgoingCall(carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !carlaCall.empty(); }));
    callOngoing = false;
    Manager::instance().answerCall(carlaCall);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return callOngoing.load(); }));

    JAMI_INFO("Start conference");
    Manager::instance().joinParticipant(call1->getCallId(), call2->getCallId());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !confId.empty(); }));

    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 1);
    CPPUNIT_ASSERT(Manager::instance().getConferenceList()[0] == confId);

    JAMI_INFO("Stop conference");
    callStopped = 0;
    Manager::instance().hangupConference(confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return callStopped >= 3; /* Note: can hang subcall Subcalls */
    }));
    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 0);
}

void
ConferenceTest::testModeratorMuteUpdateParticipantsInfos()
{
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::string bobCall, carlaCall, confId;
    std::atomic_bool callOngoing {false}, bobMuted {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string& accountId, const std::string& callId, const std::string&) {
            if (accountId == bobId)
                bobCall = callId;
            else if (accountId == carlaId)
                carlaCall = callId;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
            } else if (state == "CURRENT") {
                callOngoing = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::ConferenceCreated>(
        [&](const std::string& conferenceId) {
            confId = conferenceId;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::OnConferenceInfosUpdated>(
        [&](const std::string& conferenceId,
            const std::vector<std::map<std::string, std::string>> participantsInfos) {
            for (const auto& infos : participantsInfos) {
                if (infos.at("uri").find(bobUri) != std::string::npos) {
                    bobMuted = infos.at("audioModeratorMuted") == "true";
                }
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between Alice and Bob");
    auto call1 = aliceAccount->newOutgoingCall(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !bobCall.empty(); }));
    Manager::instance().answerCall(bobCall);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return callOngoing.load(); }));

    JAMI_INFO("Start call between Alice and Carla");
    auto call2 = aliceAccount->newOutgoingCall(carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !carlaCall.empty(); }));
    callOngoing = false;
    Manager::instance().answerCall(carlaCall);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return callOngoing.load(); }));

    JAMI_INFO("Start conference");
    Manager::instance().joinParticipant(call1->getCallId(), call2->getCallId());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return !confId.empty(); }));

    JAMI_INFO("Play with mute from the moderator");
    Manager::instance().muteParticipant(confId, bobUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&] { return bobMuted.load(); }));

    Manager::instance().muteParticipant(confId, bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&] { return !bobMuted.load(); }));

    JAMI_INFO("Stop conference");
    callStopped = 0;
    Manager::instance().hangupConference(confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return callStopped >= 3; /* Note: can hang subcall Subcalls */
    }));
    CPPUNIT_ASSERT(Manager::instance().getConferenceList().size() == 0);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConferenceTest::name())
