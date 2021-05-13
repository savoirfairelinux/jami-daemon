/*
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
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
#include <filesystem>
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

class CallTest : public CppUnit::TestFixture
{
public:
    CallTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~CallTest() { DRing::fini(); }
    static std::string name() { return "Call"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;

private:
    void testCall();
    void testCachedCall();
    void testStopSearching();
    void testDeclineMultiDevice();

    CPPUNIT_TEST_SUITE(CallTest);
    CPPUNIT_TEST(testCall);
    CPPUNIT_TEST(testCachedCall);
    CPPUNIT_TEST(testStopSearching);
    CPPUNIT_TEST(testDeclineMultiDevice);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CallTest, CallTest::name());

void
CallTest::setUp()
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

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
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
CallTest::tearDown()
{
    DRing::unregisterSignalHandlers();
    JAMI_INFO("Remove created accounts...");

    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    std::atomic_bool accountsRemoved {false};
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - bob2Id.empty()
                    ? 2
                    : 3) {
                accountsRemoved = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);
    if (!bob2Id.empty())
        Manager::instance().removeAccount(bob2Id, true);
    // Because cppunit is not linked with dbus, just poll if removed
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return accountsRemoved.load(); }));

    DRing::unregisterSignalHandlers();
}

void
CallTest::testCall()
{
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string&, const std::string&, const std::string&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto call = aliceAccount->newOutgoingCall(bobUri);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callReceived.load(); }));

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(call->getCallId());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callStopped == 2; }));
}

void
CallTest::testCachedCall()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false}, successfullyConnected {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string&, const std::string&, const std::string&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    JAMI_INFO("Connect Alice's device and Bob's device");
    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "sip",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket)
                               successfullyConnected = true;
                           cv.notify_one();
                       });
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&] { return successfullyConnected.load(); }));

    JAMI_INFO("Start call between alice and Bob");
    auto call = aliceAccount->newOutgoingCall(bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callReceived.load(); }));

    callStopped = 0;
    JAMI_INFO("Stop call between alice and Bob");
    Manager::instance().hangupCall(call->getCallId());
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callStopped == 2; }));
}

void
CallTest::testStopSearching()
{
    JAMI_INFO("Waiting....");
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    Manager::instance().sendRegister(bobId, false);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callStopped {false};
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped = true;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto call = aliceAccount->newOutgoingCall(bobUri);

    // Bob not there, so we should get a SEARCHING STATUS
    JAMI_INFO("Wait OVER state");
    // Then wait for the DHT no answer. this can take some times
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return callStopped.load(); }));
}

void
CallTest::testDeclineMultiDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    // Add second device for Bob
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    bob2Id = Manager::instance().addAccount(details);

    bool ready = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);
                if (!bob2Account)
                    return;
                auto details = bob2Account->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    ready = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(20), [&] { return ready; }));
    DRing::unregisterSignalHandlers();

    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::atomic<int> callReceived {0};
    std::atomic<int> callStopped {0};
    std::string callIdBob;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
        [&](const std::string& accountId, const std::string& callId, const std::string&) {
            if (accountId == bobId)
                callIdBob = callId;
            callReceived += 1;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [&](const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto call = aliceAccount->newOutgoingCall(bobUri);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return callReceived == 2 && !callIdBob.empty();
    }));

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(callIdBob);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callStopped == 3; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::CallTest::name())
