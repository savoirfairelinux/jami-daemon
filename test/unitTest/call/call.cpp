/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "account_schema.h"
#include "media_const.h"
#include "call_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <filesystem>
#include <string>

using namespace libjami::Account;
using namespace libjami::Call::Details;
using namespace std::literals::chrono_literals;
namespace jami {
namespace test {

class CallTest : public CppUnit::TestFixture
{
public:
    CallTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~CallTest() { libjami::fini(); }
    static std::string name() { return "Call"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;

private:
    void testCall();
    void testCachedCall();
    void testStopSearching();
    void testDeclineMultiDevice();
    void testTlsInfosPeerCertificate();
    void testSocketInfos();
    void testInvalidTurn();
    void testTransfer();
    void testDhtPublicInCall();

    CPPUNIT_TEST_SUITE(CallTest);
    CPPUNIT_TEST(testCall);
    CPPUNIT_TEST(testCachedCall);
    CPPUNIT_TEST(testStopSearching);
    CPPUNIT_TEST(testDeclineMultiDevice);
    CPPUNIT_TEST(testTlsInfosPeerCertificate);
    CPPUNIT_TEST(testSocketInfos);
    CPPUNIT_TEST(testInvalidTurn);
    CPPUNIT_TEST(testTransfer);
    CPPUNIT_TEST(testDhtPublicInCall);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CallTest, CallTest::name());

void
CallTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];
}

void
CallTest::tearDown()
{
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    if (bob2Id.empty()) {
        wait_for_removal_of({aliceId, bobId, carlaId});
    } else {
        wait_for_removal_of({aliceId, bobId, carlaId, bob2Id});
    }
}

void
CallTest::testCall()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callReceived.load(); }));

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(aliceId, call);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped == 2; }));
}

void
CallTest::testCachedCall()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    auto aliceUri = aliceAccount->getUsername();

    // Pin certificate from one to another certstore (because we do not perform any DHT operation in this test)
    bobAccount->certStore().pinCertificate(aliceAccount->identity().second);
    aliceAccount->certStore().pinCertificate(bobAccount->identity().second);

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false}, successfullyConnected {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Connect Alice's device and Bob's device");
    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "sip",
                       [&cv, &successfullyConnected](std::shared_ptr<dhtnet::ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket)
                               successfullyConnected = true;
                           cv.notify_one();
                       });
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return successfullyConnected.load(); }));

    JAMI_INFO("Start call between alice and Bob");
    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callReceived.load(); }));

    callStopped = 0;
    JAMI_INFO("Stop call between alice and Bob");
    Manager::instance().hangupCall(aliceId, call);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped == 2; }));
}

void
CallTest::testStopSearching()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    Manager::instance().sendRegister(bobId, false);

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callStopped {false};
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});

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
    std::unique_lock lk {mtx};
    std::condition_variable cv;

    // Add second device for Bob
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;

    bob2Id = Manager::instance().addAccount(details);

    wait_for_announcement_of(bob2Id);

    std::atomic<int> callReceived {0};
    std::atomic<int> callStopped {0};
    std::string callIdBob;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId)
                callIdBob = callId;
            callReceived += 1;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER")
                callStopped++;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto bobAccount2 = Manager::instance().getAccount<JamiAccount>(bob2Id);

    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return callReceived == 2 && !callIdBob.empty(); }));

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().refuseCall(bobId, callIdBob);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
        return callStopped.load() >= 3; /* >= because there is subcalls */
    }));
}

void
CallTest::testTlsInfosPeerCertificate()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic<int> callStopped {0};
    std::string bobCallId;
    std::string aliceCallState;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId)
                bobCallId = callId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId, const std::string&, const std::string& state, signed) {
            if (accountId == aliceId)
                aliceCallState = state;
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !bobCallId.empty(); }));

    Manager::instance().answerCall(bobId, bobCallId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceCallState == "CURRENT"; }));

    auto call = std::dynamic_pointer_cast<SIPCall>(aliceAccount->getCall(callId));
    auto* transport = call->getTransport();
    CPPUNIT_ASSERT(transport);
    auto cert = transport->getTlsInfos().peerCert;
    CPPUNIT_ASSERT(cert && cert->issuer);
    CPPUNIT_ASSERT(cert->issuer->getId().toString() == bobAccount->getUsername());

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped == 2; }));
}

void
CallTest::testSocketInfos()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic<int> callStopped {0};
    std::string bobCallId;
    std::string aliceCallState;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId)
                bobCallId = callId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId, const std::string&, const std::string& state, signed) {
            if (accountId == aliceId)
                aliceCallState = state;
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    auto mediaReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>&) {
            if (event == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS) {
                mediaReady = true;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto callId = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !bobCallId.empty(); }));

    Manager::instance().answerCall(bobId, bobCallId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceCallState == "CURRENT" && mediaReady; }));

    JAMI_INFO("Detail debug");
    auto details = libjami::getCallDetails(aliceId, callId);
    for (auto i = details.begin(); i != details.end(); i++) {
        JAMI_INFO("%s : %s", i->first.c_str(), i->second.c_str());
    }
    auto call = std::dynamic_pointer_cast<SIPCall>(aliceAccount->getCall(callId));
    auto transport = call->getIceMedia();
    CPPUNIT_ASSERT(transport);
    CPPUNIT_ASSERT(transport->isRunning());
    CPPUNIT_ASSERT(transport->link().c_str() == details[libjami::Call::Details::SOCKETS]);

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(aliceId, callId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped == 2; }));
}

void
CallTest::testInvalidTurn()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false};
    std::atomic<int> callStopped {0};
    bool aliceReady = false;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId,
                const std::map<std::string, std::string>& details) {
                if (accountId != aliceId) {
                    return;
                }
                try {
                    aliceReady |= accountId == aliceId
                                && details.at(jami::Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS) == "REGISTERED"
                                && details.at(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED) == "true";
                } catch (const std::out_of_range&) {}
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    std::map<std::string, std::string> details;
    details[ConfProperties::TURN::SERVER] = "1.1.1.1";
    aliceReady = false;
    libjami::setAccountDetails(aliceId, details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceReady; }));

    JAMI_INFO("Start call between alice and Bob");
    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return callReceived.load(); }));

    JAMI_INFO("Stop call between alice and Bob");
    callStopped = 0;
    Manager::instance().hangupCall(aliceId, call);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped == 2; }));
}

void
CallTest::testTransfer()
{
    // Alice call Bob
    // Bob transfer to Carla

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool bobCallReceived {false};
    std::atomic_bool carlaCallReceived {false};
    std::atomic<int> bobCallStopped {0};
    std::atomic<int> aliceCallStopped {0};
    std::string bobCallId;
    std::string carlaCallId;
    std::string carlaCallPeer;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string& peerId,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId) {
                bobCallReceived = true;
                bobCallId = callId;
            } else if (accountId == carlaId) {
                carlaCallReceived = true;
                carlaCallId = callId;
                carlaCallPeer = peerId;
                string_replace(carlaCallPeer, "@ring.dht", "");
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                if (accountId == bobId) {
                    bobCallStopped += 1;
                    cv.notify_one();
                }
                if (accountId == aliceId) {
                    aliceCallStopped += 1;
                    cv.notify_one();
                }
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    JAMI_INFO("Start call between alice and Bob");
    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return bobCallReceived.load(); }));

    JAMI_INFO("Bob transfer to Carla");
    libjami::transfer(bobId, bobCallId, carlaUri);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return bobCallStopped.load(); }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaCallReceived.load(); }));
    CPPUNIT_ASSERT(carlaCallPeer == aliceUri);

    JAMI_INFO("Stop call between alice and carla");
    aliceCallStopped = 0;
    Manager::instance().hangupCall(carlaId, carlaCallId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceCallStopped.load(); }));
}

void
CallTest::testDhtPublicInCall()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_bool callReceived {false};
    std::atomic<int> callStopped {0};
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            callReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string&, const std::string&, const std::string& state, signed) {
            if (state == "OVER") {
                callStopped += 1;
                if (callStopped == 2)
                    cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);

    std::map<std::string, std::string> details;
    details["DHT.PublicInCalls"] = "FALSE";
    libjami::setAccountDetails(bobId, details);

    JAMI_INFO("Start call between alice and Bob");
    auto call = libjami::placeCallWithMedia(aliceId, bobUri, {});

    CPPUNIT_ASSERT(!cv.wait_for(lk, 15s, [&] { return callReceived.load(); }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::CallTest::name())
