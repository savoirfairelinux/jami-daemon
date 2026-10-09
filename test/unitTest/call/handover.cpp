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
#include "connectivity/sip_utils.h"
#include "../../test_runner.h"
#include "jami.h"
#include "media_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <chrono>
#include <atomic>
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
    void testRepeatedNetworkChange();
    void testNetworkChangeDuringMediaReinvite();
    void testProbeAfterPeerRequest();
    void testLegacyPeerDisconnectsWithoutRetry();
    void testResetsPublishedAddresses();
    void testRecoveryOpensNewSocket();
    void testStaleRecoveryAttemptDoesNotTakeOver();
    void testStaleIceCallbacks();
    void testCurrentIceFailureEndsCall();
    void testBlockedPeerEndsCall();
    void testRemovedPeerEndsCall();
    void testCalleeBlockingCallerEndsCall();
    void testHoldDuringRecovery();
    void testHoldAndMediaChangeDuringRecovery();
    void testResumeAndMediaChangeDuringRecovery();
    void testMediaChangeWaitsForInviteAndIce();
    void testDeferredCallbackStartsNewRecovery();
    void testCallerMediaChangeDuringRecovery();
    void testCalleeMediaChangeDuringRecovery();
    void testRecoveryAcrossAddressFamilies();
    void testIgnoresRequestsOnAbandonedChannel();

    CPPUNIT_TEST_SUITE(HandoverTest);
    CPPUNIT_TEST(testRecoverAfterSipChannelCloses);
    CPPUNIT_TEST(testCallerNetworkChange);
    CPPUNIT_TEST(testCalleeNetworkChange);
    CPPUNIT_TEST(testBothPeersChangeNetwork);
    CPPUNIT_TEST(testRepeatedNetworkChange);
    CPPUNIT_TEST(testNetworkChangeDuringMediaReinvite);
    CPPUNIT_TEST(testProbeAfterPeerRequest);
    CPPUNIT_TEST(testLegacyPeerDisconnectsWithoutRetry);
    CPPUNIT_TEST(testResetsPublishedAddresses);
    CPPUNIT_TEST(testRecoveryOpensNewSocket);
    CPPUNIT_TEST(testStaleRecoveryAttemptDoesNotTakeOver);
    CPPUNIT_TEST(testStaleIceCallbacks);
    CPPUNIT_TEST(testCurrentIceFailureEndsCall);
    CPPUNIT_TEST(testBlockedPeerEndsCall);
    CPPUNIT_TEST(testRemovedPeerEndsCall);
    CPPUNIT_TEST(testCalleeBlockingCallerEndsCall);
    CPPUNIT_TEST(testHoldDuringRecovery);
    CPPUNIT_TEST(testHoldAndMediaChangeDuringRecovery);
    CPPUNIT_TEST(testResumeAndMediaChangeDuringRecovery);
    CPPUNIT_TEST(testMediaChangeWaitsForInviteAndIce);
    CPPUNIT_TEST(testDeferredCallbackStartsNewRecovery);
    CPPUNIT_TEST(testCallerMediaChangeDuringRecovery);
    CPPUNIT_TEST(testCalleeMediaChangeDuringRecovery);
    CPPUNIT_TEST(testRecoveryAcrossAddressFamilies);
    CPPUNIT_TEST(testIgnoresRequestsOnAbandonedChannel);
    CPPUNIT_TEST_SUITE_END();

    // Alice calls Bob, who answers; returns once media is negotiated on both sides.
    void startCall();
    CallState currentState();
    // Checks that the call survived with new SIP channels and new ICE sessions.
    void checkRecovered(const CallState& before);
    void hangUp();
    void removePeerDuringCall(bool ban, bool byCallee = false);
    void changeMediaDuringRecovery(bool caller);
    void changeMediaWithControlDuringRecovery(bool resume);
    void waitForMediaChange(bool held);

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
    // Accept the media changes offered by the peer, as clients do.
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaChangeRequested>(
        [](const std::string& accountId, const std::string& callId, const std::vector<libjami::MediaMap>& mediaList) {
            runOnMainThread(
                [accountId, callId, mediaList] { libjami::answerMediaChangeRequest(accountId, callId, mediaList); });
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

    {
        std::unique_lock lock(mutex_);
        CPPUNIT_ASSERT(cv_.wait_for(lock, 30s, [&] {
            return aliceCurrent_ and bobCurrent_ and negotiations_[aliceCallId_] > 0 and negotiations_[bobCallId_] > 0;
        }));
    }
    auto idle = [&] {
        auto state = currentState();
        if (not state.alice or not state.bob)
            return false;
        std::scoped_lock lock(state.alice->callMutex_, state.bob->callMutex_);
        return state.alice->inviteSession_ and state.bob->inviteSession_
               and state.alice->inviteSession_->state == PJSIP_INV_STATE_CONFIRMED
               and state.bob->inviteSession_->state == PJSIP_INV_STATE_CONFIRMED
               and not state.alice->isReinviteBusyLocked() and not state.bob->isReinviteBusyLocked();
    };
    auto deadline = std::chrono::steady_clock::now() + 15s;
    while (not idle() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(50ms);
    CPPUNIT_ASSERT(idle());
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
HandoverTest::testRepeatedNetworkChange()
{
    startCall();
    auto before = currentState();
    alice_->networkInterfaceChanged();
    alice_->networkInterfaceChanged();
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testNetworkChangeDuringMediaReinvite()
{
    startCall();
    auto before = currentState();
    auto media = before.alice->currentMediaList();
    CPPUNIT_ASSERT_EQUAL(size_t(2), media.size());
    std::erase_if(media, [](const auto& stream) {
        return stream.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    CPPUNIT_ASSERT_EQUAL(size_t(1), media.size());
    CPPUNIT_ASSERT(libjami::requestMediaChange(aliceId_, aliceCallId_, media));
    CPPUNIT_ASSERT(before.alice->inviteSession_ and before.alice->inviteSession_->invite_tsx);
    alice_->networkInterfaceChanged();
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testProbeAfterPeerRequest()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.alice->getTransport());
    // Wrap the live channel separately to exercise the dialog-follow path
    // without racing an automatic probe from a second network connection.
    auto transport = std::make_shared<SipTransport>(before.aliceTransport,
                                                    before.alice->getTransport()->getTlsInfos().peerCert);
    transport->setDeviceId(std::string(before.alice->getTransport()->deviceId()));
    transport->setPeerAccountId(bob_->getUsername());
    auto contact = alice_->getContactHeader(transport);

    // An in-dialog request can be queued before the peer's OPTIONS probe.
    before.alice->followPeerTransport(transport, contact, false);
    CPPUNIT_ASSERT(before.alice->getTransport() == transport.get());
    before.alice->followPeerTransport(transport, contact, true);
    before.alice->followPeerTransport(transport, contact, true);
    {
        std::unique_lock lock(mutex_);
        CPPUNIT_ASSERT(cv_.wait_for(lock, 15s, [&] { return negotiations_[aliceCallId_] > before.aliceNegotiations; }));
    }
    std::this_thread::sleep_for(500ms);
    {
        std::lock_guard lock(mutex_);
        CPPUNIT_ASSERT_EQUAL(before.aliceNegotiations + 1, negotiations_[aliceCallId_]);
    }
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

void
HandoverTest::testStaleRecoveryAttemptDoesNotTakeOver()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.alice->getTransport());
    CPPUNIT_ASSERT(before.alice->beginCallRecovery());

    const auto stale = before.alice->startRecoveryAttempt(true);
    const auto latest = before.alice->startRecoveryAttempt(true);
    CPPUNIT_ASSERT(stale and latest and *stale != *latest);

    before.alice->finishRecoveryAttempt(*stale);
    auto transport = std::shared_ptr<SipTransport>(before.alice, before.alice->getTransport());
    CPPUNIT_ASSERT(not before.alice->useRecoveredTransport(transport, "Contact: <sip:stale@localhost>", *stale));
    CPPUNIT_ASSERT(before.alice->needsNewSipChannel());
    before.alice->finishRecoveryAttempt(*latest);
    hangUp();
}

void
HandoverTest::testStaleIceCallbacks()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.aliceIce);
    auto replacement = Manager::instance().getIceTransportFactory()->createTransport("replacement");
    CPPUNIT_ASSERT(replacement);
    auto subcall = std::make_shared<SIPCall>(alice_, "retired-subcall", Call::CallType::OUTGOING,
                                             before.alice->currentMediaList());
    subcall->setIceMedia(before.aliceIce);
    subcall->parent_ = before.alice;
    {
        std::lock_guard lock(before.alice->callMutex_);
        before.alice->setIceMedia(replacement, true);
        before.alice->waitForIceInit_ = true;
        before.alice->onIceInitDone(before.aliceIce, true);
        before.alice->onIceInitDone({}, true);
        CPPUNIT_ASSERT(before.alice->waitForIceInit_);
        before.alice->onIceNegoDone(before.aliceIce, false);
        before.alice->onIceNegoDone(before.aliceIce, true);
        before.alice->onIceNegoDone({}, false);
        subcall->onIceNegoDone(before.aliceIce, false);
        subcall->onIceNegoDone(before.aliceIce, true);
        CPPUNIT_ASSERT(before.alice->getConnectionState() == Call::ConnectionState::CONNECTED);
        CPPUNIT_ASSERT(before.alice->getIceMedia() == replacement);
        before.alice->waitForIceInit_ = false;
        before.alice->setIceMedia({}, true);
    }
    CPPUNIT_ASSERT_EQUAL(before.aliceNegotiations, currentState().aliceNegotiations);
    hangUp();
}

void
HandoverTest::testCurrentIceFailureEndsCall()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.aliceIce);
    before.alice->onIceNegoDone(before.aliceIce, false);
    CPPUNIT_ASSERT(before.alice->getConnectionState() == Call::ConnectionState::DISCONNECTED);
    Manager::instance().hangupCall(bobId_, bobCallId_);
    std::unique_lock lock(mutex_);
    CPPUNIT_ASSERT(cv_.wait_for(lock, 5s, [&] { return ended_ == 2; }));
}

void
HandoverTest::removePeerDuringCall(bool ban, bool byCallee)
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob);
    CPPUNIT_ASSERT(before.alice->peerSupportsHandover() and before.bob->peerSupportsHandover());

    if (byCallee)
        bob_->removeContact(alice_->getUsername(), ban);
    else
        alice_->removeContact(bob_->getUsername(), ban);

    // Closing the connections with a removed peer must end the call on both
    // sides right away, not start a network recovery.
    std::unique_lock lock(mutex_);
    CPPUNIT_ASSERT(cv_.wait_for(lock, 5s, [&] { return ended_ == 2; }));
}

void
HandoverTest::testBlockedPeerEndsCall()
{
    removePeerDuringCall(true);
}

void
HandoverTest::testRemovedPeerEndsCall()
{
    removePeerDuringCall(false);
}

void
HandoverTest::testCalleeBlockingCallerEndsCall()
{
    removePeerDuringCall(true, true);
}

void
HandoverTest::testHoldDuringRecovery()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob and before.aliceTransport);

    CPPUNIT_ASSERT(pjsip_transport_shutdown(before.aliceTransport) == PJ_SUCCESS);
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (not before.alice->isRecovering() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CPPUNIT_ASSERT(before.alice->isRecovering());

    // The hold waits for the end of the recovery, then completes.
    libjami::hold(aliceId_, aliceCallId_);
    auto held = [&] {
        return not before.alice->isRecovering() and before.alice->getState() == Call::CallState::HOLD;
    };
    deadline = std::chrono::steady_clock::now() + 30s;
    while (not held() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT(held());
    {
        std::lock_guard lock(mutex_);
        CPPUNIT_ASSERT_EQUAL(0u, ended_);
    }
    hangUp();
}

void
HandoverTest::waitForMediaChange(bool held)
{
    auto applied = [&] {
        auto state = currentState();
        if (not state.alice or not state.bob)
            return false;
        std::scoped_lock lock(state.alice->callMutex_, state.bob->callMutex_);
        if (state.alice->isReinviteBusyLocked() or state.bob->isReinviteBusyLocked()
            or state.alice->getMediaAttributeList().size() != 1 or state.bob->getMediaAttributeList().size() != 1)
            return false;
        return state.alice->getState() == (held ? Call::CallState::HOLD : Call::CallState::ACTIVE)
               and state.alice->getMediaAttributeList().front().hold_ == held and state.bob->peerHold_ == held
               and state.aliceIce and state.aliceIce->isRunning() and state.bobIce and state.bobIce->isRunning();
    };
    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (not applied() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(50ms);
    CPPUNIT_ASSERT(applied());
    std::lock_guard lock(mutex_);
    CPPUNIT_ASSERT_EQUAL(0u, ended_);
}

void
HandoverTest::changeMediaWithControlDuringRecovery(bool resume)
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob);
    if (resume) {
        libjami::hold(aliceId_, aliceCallId_);
        auto held = [&] {
            std::lock_guard lock(before.alice->callMutex_);
            return before.alice->getState() == Call::CallState::HOLD and not before.alice->isReinviteBusyLocked();
        };
        auto deadline = std::chrono::steady_clock::now() + 15s;
        while (not held() and std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(50ms);
        CPPUNIT_ASSERT(held());
    }
    auto audioOnly = before.alice->currentMediaList();
    std::erase_if(audioOnly, [](const auto& media) {
        return media.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    auto controlSucceeded = std::make_shared<std::atomic_bool>(false);
    {
        std::lock_guard lock(before.alice->callMutex_);
        before.alice->startRecoveryLocked();
        auto reply = [controlSucceeded](bool ok) { controlSucceeded->store(ok); };
        if (resume)
            CPPUNIT_ASSERT(not before.alice->resume(std::move(reply)));
        else
            CPPUNIT_ASSERT(not before.alice->hold(std::move(reply)));
        CPPUNIT_ASSERT(before.alice->requestMediaChange(audioOnly));
        CPPUNIT_ASSERT_EQUAL(size_t(2), before.alice->getMediaAttributeList().size());
        before.alice->stopCallRecovery();
    }
    before.alice->drainPendingRequests();
    waitForMediaChange(not resume);
    CPPUNIT_ASSERT(controlSucceeded->load());
    hangUp();
}

void
HandoverTest::testHoldAndMediaChangeDuringRecovery()
{
    changeMediaWithControlDuringRecovery(false);
}

void
HandoverTest::testResumeAndMediaChangeDuringRecovery()
{
    changeMediaWithControlDuringRecovery(true);
}

void
HandoverTest::testMediaChangeWaitsForInviteAndIce()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.alice->inviteSession_);
    auto audioOnly = before.alice->currentMediaList();
    std::erase_if(audioOnly, [](const auto& media) {
        return media.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    bool sipBlocked;
    bool iceBlocked;
    int busyResult;
    {
        std::lock_guard lock(before.alice->callMutex_);
        auto* dialog = before.alice->inviteSession_->dlg;
        sip_utils::PJDialogLock dialogLock {dialog};
        auto* previousTransaction = before.alice->inviteSession_->invite_tsx;
        pjsip_transaction transaction {};
        before.alice->inviteSession_->invite_tsx = &transaction;
        before.alice->requestMediaChange(audioOnly);
        before.alice->drainPendingRequests();
        busyResult = before.alice->SIPSessionReinvite(before.alice->getMediaAttributeList(), true);
        sipBlocked = before.alice->pendingMediaChange_.has_value()
                     and before.alice->getMediaAttributeList().size() == 2;
        before.alice->inviteSession_->invite_tsx = previousTransaction;
        before.alice->isWaitingForIceAndMedia_ = true;
        before.alice->onInviteTransactionEnded();
        iceBlocked = before.alice->pendingMediaChange_.has_value()
                     and before.alice->getMediaAttributeList().size() == 2;
        before.alice->isWaitingForIceAndMedia_ = false;
    }
    CPPUNIT_ASSERT_EQUAL(PJ_EBUSY, busyResult);
    CPPUNIT_ASSERT(sipBlocked and iceBlocked);
    before.alice->onInviteTransactionEnded();
    waitForMediaChange(false);
    hangUp();
}

void
HandoverTest::testDeferredCallbackStartsNewRecovery()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.alice->inviteSession_);
    auto audioOnly = before.alice->currentMediaList();
    std::erase_if(audioOnly, [](const auto& media) {
        return media.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    auto replied = std::make_shared<std::atomic_bool>(false);
    {
        std::lock_guard lock(before.alice->callMutex_);
        before.alice->startRecoveryLocked();
        before.alice->hold([w = before.alice->weak(), replied](bool ok) {
            if (auto call = w.lock()) {
                std::lock_guard lock(call->callMutex_);
                call->startRecoveryLocked();
                replied->store(ok);
            }
        });
        before.alice->requestMediaChange(audioOnly);
        before.alice->stopCallRecovery();
    }
    before.alice->drainPendingRequests();
    {
        std::lock_guard lock(before.alice->callMutex_);
        CPPUNIT_ASSERT(replied->load());
        CPPUNIT_ASSERT(before.alice->isRecovering());
        CPPUNIT_ASSERT(before.alice->pendingMediaChange_);
        CPPUNIT_ASSERT_EQUAL(size_t(2), before.alice->getMediaAttributeList().size());
        before.alice->stopCallRecovery();
    }
    before.alice->drainPendingRequests();
    waitForMediaChange(true);
    hangUp();
}

void
HandoverTest::changeMediaDuringRecovery(bool caller)
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob and before.aliceTransport and before.bobTransport);
    auto call = caller ? before.alice : before.bob;
    auto audioOnly = call->currentMediaList();
    CPPUNIT_ASSERT_EQUAL(size_t(2), audioOnly.size());
    std::erase_if(audioOnly, [](const auto& media) {
        return media.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    CPPUNIT_ASSERT_EQUAL(size_t(1), audioOnly.size());

    CPPUNIT_ASSERT(pjsip_transport_shutdown(caller ? before.aliceTransport : before.bobTransport) == PJ_SUCCESS);
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (not call->isRecovering() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CPPUNIT_ASSERT(call->isRecovering());

    // The video is dropped while the call is recovering.
    CPPUNIT_ASSERT(libjami::requestMediaChange(caller ? aliceId_ : bobId_, call->getCallId(), audioOnly));

    // The call recovers, then applies the change on both sides.
    auto applied = [&] {
        auto state = currentState();
        return state.alice and state.bob and not state.alice->isRecovering() and not state.bob->isRecovering()
               and state.alice->getMediaAttributeList().size() == 1 and state.bob->getMediaAttributeList().size() == 1
               and state.aliceIce and state.aliceIce->isRunning() and state.bobIce and state.bobIce->isRunning();
    };
    deadline = std::chrono::steady_clock::now() + 30s;
    while (not applied() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT(applied());
    auto after = currentState();
    CPPUNIT_ASSERT(after.aliceTransport != before.aliceTransport and after.bobTransport != before.bobTransport);
    {
        std::lock_guard lock(mutex_);
        CPPUNIT_ASSERT_EQUAL(0u, ended_);
    }
    hangUp();
}

void
HandoverTest::testCallerMediaChangeDuringRecovery()
{
    changeMediaDuringRecovery(true);
}

void
HandoverTest::testCalleeMediaChangeDuringRecovery()
{
    changeMediaDuringRecovery(false);
}

void
HandoverTest::testRecoveryAcrossAddressFamilies()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob and before.alice->inviteSession_ and before.bob->inviteSession_);
    // Give the peers previous Contacts of another address family than the
    // channels, as after moving between IPv4 and IPv6 networks.
    for (const auto& call : {before.alice, before.bob}) {
        auto* dialog = call->inviteSession_->dlg;
        pjsip_dlg_inc_lock(dialog);
        auto* uri = static_cast<pjsip_sip_uri*>(pjsip_uri_clone(dialog->pool, pjsip_uri_get_uri(dialog->target)));
        auto ipv6 = std::string_view(uri->host.ptr, uri->host.slen).find(':') != std::string_view::npos;
        pj_strdup2(dialog->pool, &uri->host, ipv6 ? "192.0.2.1" : "2001:db8::1");
        dialog->target = reinterpret_cast<pjsip_uri*>(uri);
        pjsip_dlg_dec_lock(dialog);
    }

    libjami::networkInterfaceChanged();
    checkRecovered(before);
    hangUp();
}

void
HandoverTest::testIgnoresRequestsOnAbandonedChannel()
{
    startCall();
    auto before = currentState();
    alice_->networkInterfaceChanged();
    checkRecovered(before);
    auto recovered = currentState();

    // A request Bob sent on the previous channel before following Alice must
    // not bring her back to it.
    pjsip_tpselector selector {};
    selector.type = PJSIP_TPSELECTOR_TRANSPORT;
    selector.u.transport = before.bobTransport;
    CPPUNIT_ASSERT(pjsip_dlg_set_transport(before.bob->inviteSession_->dlg, &selector) == PJ_SUCCESS);
    before.bob->requestKeyframe();
    std::this_thread::sleep_for(2s);
    CPPUNIT_ASSERT(currentState().aliceTransport == recovered.aliceTransport);
    hangUp();
}

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::HandoverTest::name())
