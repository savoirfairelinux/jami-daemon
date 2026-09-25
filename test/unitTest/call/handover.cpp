/*
 *  Copyright (C) 2026 Savoir-faire Linux Inc.
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
#include "media_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace std::literals::chrono_literals;

namespace jami::test {

class HandoverTest : public CppUnit::TestFixture
{
public:
    HandoverTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~HandoverTest() { libjami::fini(); }

    static std::string name() { return "CallHandover"; }

    void setUp();
    void tearDown();

private:
    struct CallState
    {
        std::shared_ptr<SIPCall> alice;
        std::shared_ptr<SIPCall> bob;
        pjsip_transport* aliceTransport {nullptr};
        pjsip_transport* bobTransport {nullptr};
        std::shared_ptr<dhtnet::IceTransport> aliceIce;
        std::shared_ptr<dhtnet::IceTransport> bobIce;
        unsigned aliceNegotiations {0};
        unsigned bobNegotiations {0};
    };

    void testRecoverAfterSipChannelCloses();
    void testCallerNetworkChange();
    void testCalleeNetworkChange();
    void testBothPeersChangeNetwork();
    void testLegacyPeerDisconnectsWithoutRetry();
    void testResetsPublishedAddresses();
    void testRecoveryOpensNewSocket();

    CPPUNIT_TEST_SUITE(HandoverTest);
    CPPUNIT_TEST(testRecoverAfterSipChannelCloses);
    CPPUNIT_TEST(testCallerNetworkChange);
    CPPUNIT_TEST(testCalleeNetworkChange);
    CPPUNIT_TEST(testBothPeersChangeNetwork);
    CPPUNIT_TEST(testLegacyPeerDisconnectsWithoutRetry);
    CPPUNIT_TEST(testResetsPublishedAddresses);
    CPPUNIT_TEST(testRecoveryOpensNewSocket);
    CPPUNIT_TEST_SUITE_END();

    // Alice calls Bob, who answers; returns once media is negotiated on both sides.
    void startCall();
    CallState currentState();
    // Checks that the call survived with new SIP channels and new ICE sessions.
    void checkRecovered(const CallState& before);
    void hangUp();

    std::shared_ptr<JamiAccount> alice_;
    std::shared_ptr<JamiAccount> bob_;
    std::string aliceId_;
    std::string bobId_;
    std::string aliceCallId_;
    std::string bobCallId_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, unsigned> negotiations_;
    bool aliceCurrent_ {false};
    bool bobCurrent_ {false};
    unsigned ended_ {0};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HandoverTest, HandoverTest::name());

void
HandoverTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-public-incoming.yml");
    aliceId_ = actors["alice"];
    bobId_ = actors["bob"];
    alice_ = Manager::instance().getAccount<JamiAccount>(aliceId_);
    bob_ = Manager::instance().getAccount<JamiAccount>(bobId_);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    handlers.insert(
        libjami::exportable_callback<libjami::CallSignal::IncomingCall>([this](const std::string& accountId,
                                                                               const std::string& callId,
                                                                               const std::string&,
                                                                               const std::vector<libjami::MediaMap>&) {
            if (accountId != bobId_)
                return;
            std::lock_guard lock(mutex_);
            bobCallId_ = callId;
            cv_.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [this](const std::string& accountId, const std::string&, const std::string& state, signed) {
            std::lock_guard lock(mutex_);
            if (state == "CURRENT") {
                aliceCurrent_ |= accountId == aliceId_;
                bobCurrent_ |= accountId == bobId_;
            } else if (state == "OVER") {
                ++ended_;
            }
            cv_.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [this](const std::string& callId, const std::string& event, const std::vector<libjami::MediaMap>&) {
            if (event != libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS)
                return;
            std::lock_guard lock(mutex_);
            ++negotiations_[callId];
            cv_.notify_all();
        }));
    libjami::registerSignalHandlers(handlers);
}

void
HandoverTest::tearDown()
{
    alice_.reset();
    bob_.reset();
    wait_for_removal_of({aliceId_, bobId_});
}

void
HandoverTest::startCall()
{
    aliceCallId_ = libjami::placeCallWithMedia(aliceId_, bob_->getUsername(), {});
    std::string bobCallId;
    {
        std::unique_lock lock(mutex_);
        CPPUNIT_ASSERT(cv_.wait_for(lock, 30s, [&] { return not bobCallId_.empty(); }));
        bobCallId = bobCallId_;
    }
    Manager::instance().acceptCall(bobId_, bobCallId);

    std::unique_lock lock(mutex_);
    CPPUNIT_ASSERT(cv_.wait_for(lock, 30s, [&] {
        return aliceCurrent_ and bobCurrent_ and negotiations_[aliceCallId_] > 0 and negotiations_[bobCallId_] > 0;
    }));
}

HandoverTest::CallState
HandoverTest::currentState()
{
    CallState state;
    std::string bobCallId;
    {
        std::lock_guard lock(mutex_);
        bobCallId = bobCallId_;
        state.aliceNegotiations = negotiations_[aliceCallId_];
        state.bobNegotiations = negotiations_[bobCallId_];
    }
    state.alice = std::dynamic_pointer_cast<SIPCall>(alice_->getCall(aliceCallId_));
    state.bob = std::dynamic_pointer_cast<SIPCall>(bob_->getCall(bobCallId));
    if (state.alice) {
        if (auto* transport = state.alice->getTransport())
            state.aliceTransport = transport->get();
        state.aliceIce = state.alice->getIceMedia();
    }
    if (state.bob) {
        if (auto* transport = state.bob->getTransport())
            state.bobTransport = transport->get();
        state.bobIce = state.bob->getIceMedia();
    }
    return state;
}

void
HandoverTest::checkRecovered(const CallState& before)
{
    {
        std::unique_lock lock(mutex_);
        CPPUNIT_ASSERT(cv_.wait_for(lock, 15s, [&] {
            return ended_ > 0
                   or (negotiations_[aliceCallId_] > before.aliceNegotiations
                       and negotiations_[bobCallId_] > before.bobNegotiations);
        }));
        CPPUNIT_ASSERT_EQUAL(0u, ended_);
    }

    // When both peers recover at once, the caller may restart ICE a second time.
    auto deadline = std::chrono::steady_clock::now() + 15s;
    while ((before.alice->isRecovering() or before.bob->isRecovering()) and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(100ms);

    auto after = currentState();
    CPPUNIT_ASSERT(after.alice and after.bob);
    CPPUNIT_ASSERT(not after.alice->isRecovering() and not after.bob->isRecovering());
    CPPUNIT_ASSERT(after.aliceTransport and after.aliceTransport != before.aliceTransport);
    CPPUNIT_ASSERT(after.bobTransport and after.bobTransport != before.bobTransport);
    CPPUNIT_ASSERT(after.aliceIce and after.aliceIce != before.aliceIce and after.aliceIce->isRunning());
    CPPUNIT_ASSERT(after.bobIce and after.bobIce != before.bobIce and after.bobIce->isRunning());

    std::lock_guard lock(mutex_);
    CPPUNIT_ASSERT_EQUAL(0u, ended_);
}

void
HandoverTest::hangUp()
{
    Manager::instance().hangupCall(aliceId_, aliceCallId_);
    std::unique_lock lock(mutex_);
    CPPUNIT_ASSERT(cv_.wait_for(lock, 30s, [&] { return ended_ == 2; }));
}

void
HandoverTest::testRecoverAfterSipChannelCloses()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob and before.aliceTransport and before.bobTransport);
    CPPUNIT_ASSERT(before.alice->peerSupportsHandover() and before.bob->peerSupportsHandover());
    CPPUNIT_ASSERT(before.alice->getTransport()->peerAccountId() == bob_->getUsername());
    CPPUNIT_ASSERT(before.aliceIce and before.aliceIce->isRunning());

    CPPUNIT_ASSERT(pjsip_transport_shutdown(before.aliceTransport) == PJ_SUCCESS);
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testCallerNetworkChange()
{
    startCall();
    auto before = currentState();
    alice_->networkInterfaceChanged();
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testCalleeNetworkChange()
{
    startCall();
    auto before = currentState();
    bob_->networkInterfaceChanged();
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testBothPeersChangeNetwork()
{
    startCall();
    auto before = currentState();
    libjami::networkInterfaceChanged();
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testLegacyPeerDisconnectsWithoutRetry()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob and before.aliceTransport);
    before.alice->setPeerHandoverSupported(false);
    before.bob->setPeerHandoverSupported(false);

    libjami::networkInterfaceChanged();
    std::this_thread::sleep_for(3s);
    auto unchanged = currentState();
    CPPUNIT_ASSERT_EQUAL(before.aliceNegotiations, unchanged.aliceNegotiations);
    CPPUNIT_ASSERT_EQUAL(before.bobNegotiations, unchanged.bobNegotiations);
    CPPUNIT_ASSERT(unchanged.aliceTransport == before.aliceTransport);
    CPPUNIT_ASSERT(unchanged.bobTransport == before.bobTransport);

    CPPUNIT_ASSERT(pjsip_transport_shutdown(before.aliceTransport) == PJ_SUCCESS);
    std::unique_lock lock(mutex_);
    CPPUNIT_ASSERT(cv_.wait_for(lock, 5s, [&] { return ended_ == 2; }));
}

void
HandoverTest::testResetsPublishedAddresses()
{
    auto& manager = alice_->connectionManager();
    manager.setPublishedAddress(dhtnet::IpAddr("192.0.2.1"));
    manager.setPublishedAddress(dhtnet::IpAddr("2001:db8::1"));
    CPPUNIT_ASSERT(manager.getPublishedIpAddress(AF_INET));
    CPPUNIT_ASSERT(manager.getPublishedIpAddress(AF_INET6));

    manager.setPublishedAddress({});
    CPPUNIT_ASSERT(not manager.getPublishedIpAddress(AF_INET));
    CPPUNIT_ASSERT(not manager.getPublishedIpAddress(AF_INET6));
}

void
HandoverTest::testRecoveryOpensNewSocket()
{
    bob_->certStore().pinCertificate(alice_->identity().second);
    alice_->certStore().pinCertificate(bob_->identity().second);
    auto& manager = alice_->connectionManager();
    const auto bobDevice = DeviceId(std::string(bob_->currentDeviceId()));

    auto connect = [&](const dhtnet::ConnectDeviceOptions& options) {
        struct Result
        {
            std::mutex mutex;
            std::condition_variable cv;
            bool done {false};
            bool connected {false};
        };
        auto result = std::make_shared<Result>();
        manager.connectDevice(
            bobDevice,
            "sip",
            [result](std::shared_ptr<dhtnet::ChannelSocket> socket, const DeviceId&) {
                std::lock_guard lock(result->mutex);
                result->done = true;
                result->connected = socket != nullptr;
                result->cv.notify_one();
            },
            options);
        std::unique_lock lock(result->mutex);
        CPPUNIT_ASSERT(result->cv.wait_for(lock, 30s, [&] { return result->done; }));
        CPPUNIT_ASSERT(result->connected);
    };

    connect({});
    const auto sockets = manager.activeSockets();
    // Placing a call forces a new socket only instead of waiting for one
    // being negotiated: it must reuse the connected one.
    connect({.forceNewSocket = true});
    CPPUNIT_ASSERT_EQUAL(sockets, manager.activeSockets());
    // A recovery must not trust it, as it may predate a network change.
    connect({.forceNewSocket = true, .ignoreConnectedSockets = true});
    CPPUNIT_ASSERT_EQUAL(sockets + 1, manager.activeSockets());
}

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::HandoverTest::name())
