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
#include "sip/sipvoiplink.h"
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
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
    void testRecoveryRequiresMatchingNegotiation();
    void testRecoveryRestartWaitsForInvite();
    void testReinviteWorkMatchesPendingTransaction();
    void testCancelledQueuedRecoveryOffer();
    void testRejectsRecoveryOfferWithoutUsableMedia();
    void testQueuedMediaCompletionChecksGeneration();
    void testCurrentIceFailureEndsCall();
    void testBlockedPeerEndsCall();
    void testRemovedPeerEndsCall();
    void testCalleeBlockingCallerEndsCall();
    void testHoldDuringRecovery();
    void testHoldAndMediaChangeDuringRecovery();
    void testResumeAndMediaChangeDuringRecovery();
    void testMediaChangeWaitsForInviteAndIce();
    void testMediaChangeRequestRunsOffMainThread();
    void testDeferredCallbackStartsNewRecovery();
    void testCallerMediaChangeDuringRecovery();
    void testCalleeMediaChangeDuringRecovery();
    void testRecoveryAcrossAddressFamilies();
    void testIgnoresRequestsOnAbandonedChannel();
    void testHandoverRequestClassification();
    void testHandoverHeaderValidation();
    void testRetirementIsPerCall();
    void testPeerMethodsSkipCallLock();

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
    CPPUNIT_TEST(testRecoveryRequiresMatchingNegotiation);
    CPPUNIT_TEST(testRecoveryRestartWaitsForInvite);
    CPPUNIT_TEST(testReinviteWorkMatchesPendingTransaction);
    CPPUNIT_TEST(testCancelledQueuedRecoveryOffer);
    CPPUNIT_TEST(testRejectsRecoveryOfferWithoutUsableMedia);
    CPPUNIT_TEST(testQueuedMediaCompletionChecksGeneration);
    CPPUNIT_TEST(testCurrentIceFailureEndsCall);
    CPPUNIT_TEST(testBlockedPeerEndsCall);
    CPPUNIT_TEST(testRemovedPeerEndsCall);
    CPPUNIT_TEST(testCalleeBlockingCallerEndsCall);
    CPPUNIT_TEST(testHoldDuringRecovery);
    CPPUNIT_TEST(testHoldAndMediaChangeDuringRecovery);
    CPPUNIT_TEST(testResumeAndMediaChangeDuringRecovery);
    CPPUNIT_TEST(testMediaChangeWaitsForInviteAndIce);
    CPPUNIT_TEST(testMediaChangeRequestRunsOffMainThread);
    CPPUNIT_TEST(testDeferredCallbackStartsNewRecovery);
    CPPUNIT_TEST(testCallerMediaChangeDuringRecovery);
    CPPUNIT_TEST(testCalleeMediaChangeDuringRecovery);
    CPPUNIT_TEST(testRecoveryAcrossAddressFamilies);
    CPPUNIT_TEST(testIgnoresRequestsOnAbandonedChannel);
    CPPUNIT_TEST(testHandoverRequestClassification);
    CPPUNIT_TEST(testHandoverHeaderValidation);
    CPPUNIT_TEST(testRetirementIsPerCall);
    CPPUNIT_TEST(testPeerMethodsSkipCallLock);
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
    std::shared_ptr<pjsip_tx_data> sendRecoveryOffer(const std::shared_ptr<SIPCall>& call,
                                                     std::uint64_t generation,
                                                     bool disableMedia,
                                                     std::atomic_int& status);

    std::shared_ptr<JamiAccount> alice_;
    std::shared_ptr<JamiAccount> bob_;
    std::string aliceId_;
    std::string bobId_;
    std::string aliceCallId_;
    std::string bobCallId_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, unsigned> negotiations_;
    std::thread::id mediaRequestThread_;
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
        [this](const std::string& accountId, const std::string& callId, const std::vector<libjami::MediaMap>& mediaList) {
            {
                std::lock_guard lock(mutex_);
                mediaRequestThread_ = std::this_thread::get_id();
                cv_.notify_all();
            }
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

    before.alice->followPeerTransport(transport, contact, 0, 1);
    before.alice->followPeerTransport(transport, contact, 0, 1);
    {
        std::unique_lock lock(mutex_);
        CPPUNIT_ASSERT(cv_.wait_for(lock, 15s, [&] { return negotiations_[aliceCallId_] > before.aliceNegotiations; }));
    }
    std::this_thread::sleep_for(500ms);
    {
        std::lock_guard lock(mutex_);
        CPPUNIT_ASSERT_EQUAL(before.aliceNegotiations + 1, negotiations_[aliceCallId_]);
    }
    auto recovered = currentState();
    before.alice->followPeerTransport(transport, contact, 0, 1);
    std::this_thread::sleep_for(500ms);
    CPPUNIT_ASSERT(before.alice->getIceMedia() == recovered.aliceIce);
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
        before.alice->onIceInitDone(replacement, true, 999);
        before.alice->onIceNegoDone(replacement, false, 999);
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
HandoverTest::testRecoveryRequiresMatchingNegotiation()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.aliceIce and before.aliceIce->isRunning());
    auto transport = Manager::instance().sipVoIPLink().sipTransportBroker->addTransport(before.aliceTransport);
    auto otherTransport = Manager::instance().sipVoIPLink().sipTransportBroker->addTransport(before.bobTransport);
    auto otherIce = Manager::instance().getIceTransportFactory()->createTransport("unrelated-negotiation");
    CPPUNIT_ASSERT(transport and otherTransport and otherIce);
    {
        std::lock_guard lock(before.alice->callMutex_);
        auto& call = *before.alice;
        call.startRecoveryLocked();
        call.recoveryNegotiation_ = SIPCall::RecoveryNegotiation {call.recoveryGeneration_, transport, before.aliceIce};
        call.completeRecoveryIfReadyLocked();
        CPPUNIT_ASSERT(call.isRecovering());
        call.recoveryNegotiation_->phase = SIPCall::RecoveryNegotiation::Phase::MediaReady;
        ++call.recoveryNegotiation_->generation;
        call.completeRecoveryIfReadyLocked();
        CPPUNIT_ASSERT(call.isRecovering());
        call.recoveryNegotiation_->generation = call.recoveryGeneration_;
        call.recoveryNegotiation_->transport = otherTransport;
        call.completeRecoveryIfReadyLocked();
        CPPUNIT_ASSERT(call.isRecovering());
        call.recoveryNegotiation_->transport = transport;
        call.recoveryNegotiation_->ice = otherIce;
        call.completeRecoveryIfReadyLocked();
        CPPUNIT_ASSERT(call.isRecovering());
        call.recoveryNegotiation_->ice = before.aliceIce;
        call.waitingForRecoveryChannel_ = true;
        call.completeRecoveryIfReadyLocked();
        CPPUNIT_ASSERT(call.isRecovering());
        call.waitingForRecoveryChannel_ = false;
        call.completeRecoveryIfReadyLocked();
        CPPUNIT_ASSERT(not call.isRecovering());
        CPPUNIT_ASSERT(not call.recoveryNegotiation_);
    }
    hangUp();
}

void
HandoverTest::testRecoveryRestartWaitsForInvite()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.alice->inviteSession_);
    bool pending;
    {
        std::lock_guard lock(before.alice->callMutex_);
        sip_utils::PJDialogLock dialogLock {before.alice->inviteSession_->dlg};
        before.alice->startRecoveryLocked();
        auto* previousTransaction = before.alice->inviteSession_->invite_tsx;
        pjsip_transaction transaction {};
        before.alice->inviteSession_->invite_tsx = &transaction;
        before.alice->restartIceAfterRecovery(dhtnet::IceTransportOptions {}, before.alice->recoveryGeneration_);
        pending = before.alice->recoveryRestartPending_ and not before.alice->recoveryNegotiation_
                  and before.alice->getIceMedia() == before.aliceIce;
        before.alice->inviteSession_->invite_tsx = previousTransaction;
        before.alice->stopCallRecovery();
        CPPUNIT_ASSERT(not before.alice->recoveryRestartPending_);
    }
    CPPUNIT_ASSERT(pending);
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
        // Audio is always offered sendrecv, so an audio-only hold never shows in the peer's peerHold_.
        return state.alice->getState() == (held ? Call::CallState::HOLD : Call::CallState::ACTIVE)
               and state.alice->getMediaAttributeList().front().hold_ == held
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
HandoverTest::testMediaChangeRequestRunsOffMainThread()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.bob);
    auto mainThread = std::make_shared<std::promise<std::thread::id>>();
    auto mainThreadFuture = mainThread->get_future();
    runOnMainThread([mainThread] { mainThread->set_value(std::this_thread::get_id()); });
    CPPUNIT_ASSERT(mainThreadFuture.wait_for(15s) == std::future_status::ready);
    auto mainThreadId = mainThreadFuture.get();
    auto audioOnly = before.alice->currentMediaList();
    std::erase_if(audioOnly, [](const auto& media) {
        return media.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    {
        std::lock_guard lock(mutex_);
        mediaRequestThread_ = {};
    }
    CPPUNIT_ASSERT(libjami::requestMediaChange(aliceId_, aliceCallId_, audioOnly));
    {
        std::unique_lock lock(mutex_);
        CPPUNIT_ASSERT(cv_.wait_for(lock, 15s, [&] { return mediaRequestThread_ != std::thread::id {}; }));
        CPPUNIT_ASSERT(mediaRequestThread_ != mainThreadId);
    }
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

static void
storeFinalStatus(void* token, pjsip_event* event)
{
    if (event->type == PJSIP_EVENT_TSX_STATE)
        static_cast<std::atomic_int*>(token)->store(event->body.tsx_state.tsx->status_code);
}

std::shared_ptr<pjsip_tx_data>
HandoverTest::sendRecoveryOffer(const std::shared_ptr<SIPCall>& call,
                                std::uint64_t generation,
                                bool disableMedia,
                                std::atomic_int& status)
{
    std::lock_guard lock(call->callMutex_);
    auto* dialog = call->inviteSession_->dlg;
    sip_utils::PJDialogLock dialogLock {dialog};
    pjsip_tx_data* tdata = nullptr;
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS),
                         pjsip_dlg_create_request(dialog, &pjsip_invite_method, dialog->local.cseq++, &tdata));
    auto request = std::shared_ptr<pjsip_tx_data>(tdata, [](pjsip_tx_data* data) { pjsip_tx_data_dec_ref(data); });
    CPPUNIT_ASSERT(call->sdp_ and call->sdp_->getActiveLocalSdpSession());
    auto* offer = pjmedia_sdp_session_clone(tdata->pool, call->sdp_->getActiveLocalSdpSession());
    ++offer->origin.version;
    if (disableMedia) {
        for (unsigned i = 0; i < offer->media_count; ++i)
            offer->media[i]->desc.port = 0;
    }
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS), pjsip_create_sdp_body(tdata->pool, offer, &tdata->msg->body));
    sip_utils::addHandoverGeneration(tdata, generation);
    tdata->mod_data[dialog->ua->id] = nullptr;
    pjsip_tpselector selector {};
    selector.type = PJSIP_TPSELECTOR_TRANSPORT;
    selector.u.transport = call->getTransport()->get();
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS), pjsip_tx_data_set_transport(tdata, &selector));
    pjsip_tx_data_add_ref(tdata);
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS),
                         pjsip_endpt_send_request(Manager::instance().sipVoIPLink().getEndpoint(),
                                                  tdata,
                                                  -1,
                                                  &status,
                                                  &storeFinalStatus));
    return request;
}

void
HandoverTest::testReinviteWorkMatchesPendingTransaction()
{
    startCall();
    auto before = currentState();
    bool pending;
    bool wrongTransactionIgnored;
    bool completedTransactionIgnored;
    bool terminatedTransactionIgnored;
    {
        std::lock_guard lock(before.bob->callMutex_);
        auto& call = *before.bob;
        sip_utils::PJDialogLock dialogLock {call.inviteSession_->dlg};
        auto* previousTransaction = call.inviteSession_->invite_tsx;
        auto generation = call.mediaNegotiationGeneration_.load();
        pjsip_transaction transaction {};
        transaction.role = PJSIP_ROLE_UAS;
        transaction.cseq = 42;
        transaction.state = PJSIP_TSX_STATE_PROCEEDING;
        transaction.status_code = PJSIP_SC_TRYING;
        call.inviteSession_->invite_tsx = &transaction;
        call.mediaNegotiationGeneration_ = generation + 1;
        pending = call.isPendingReinviteLocked(42) and not call.isPendingReinviteLocked(41);
        call.rejectPendingReinviteLocked(41);
        wrongTransactionIgnored = call.mediaNegotiationGeneration_ == generation + 1
                                  and transaction.status_code == PJSIP_SC_TRYING;
        transaction.status_code = PJSIP_SC_REQUEST_TERMINATED;
        transaction.state = PJSIP_TSX_STATE_COMPLETED;
        call.rejectPendingReinviteLocked(42);
        completedTransactionIgnored = not call.isPendingReinviteLocked(42)
                                      and call.mediaNegotiationGeneration_ == generation + 1;
        transaction.state = PJSIP_TSX_STATE_TERMINATED;
        transaction.status_code = 0;
        terminatedTransactionIgnored = not call.isPendingReinviteLocked(42);
        call.inviteSession_->invite_tsx = previousTransaction;
        call.mediaNegotiationGeneration_ = generation;
    }
    CPPUNIT_ASSERT(pending and wrongTransactionIgnored and completedTransactionIgnored and terminatedTransactionIgnored);
    hangUp();
}

void
HandoverTest::testCancelledQueuedRecoveryOffer()
{
    startCall();
    auto before = currentState();
    const auto generation = before.bob->recoveryGeneration();
    auto entered = std::make_shared<std::promise<void>>();
    auto enteredFuture = entered->get_future();
    auto release = std::make_shared<std::promise<void>>();
    auto released = release->get_future().share();
    runOnMainThread([entered, released] {
        entered->set_value();
        released.wait_for(15s);
    });
    CPPUNIT_ASSERT(enteredFuture.wait_for(5s) == std::future_status::ready);

    static std::atomic_int status;
    status = 0;
    auto request = sendRecoveryOffer(before.alice, generation + 1, false, status);
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (before.bob->mediaNegotiationGeneration_ != generation + 1 and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CPPUNIT_ASSERT(before.bob->mediaNegotiationGeneration_ == generation + 1);
    {
        std::lock_guard lock(before.bob->callMutex_);
        sip_utils::PJDialogLock dialogLock {before.bob->inviteSession_->dlg};
        auto* transaction = before.bob->inviteSession_->invite_tsx;
        CPPUNIT_ASSERT(transaction and transaction->status_code == PJSIP_SC_TRYING);
    }
    pjsip_tx_data* cancel = nullptr;
    auto* endpoint = Manager::instance().sipVoIPLink().getEndpoint();
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS), pjsip_endpt_create_cancel(endpoint, request.get(), &cancel));
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS), pjsip_tx_data_set_transport(cancel, &request->tp_sel));
    CPPUNIT_ASSERT_EQUAL(pj_status_t(PJ_SUCCESS), pjsip_endpt_send_request(endpoint, cancel, -1, nullptr, nullptr));
    deadline = std::chrono::steady_clock::now() + 5s;
    while (not status and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CPPUNIT_ASSERT_EQUAL(int(PJSIP_SC_REQUEST_TERMINATED), status.load());
    release->set_value();

    auto drained = std::make_shared<std::promise<void>>();
    auto drainedFuture = drained->get_future();
    runOnMainThread([drained] { drained->set_value(); });
    CPPUNIT_ASSERT(drainedFuture.wait_for(5s) == std::future_status::ready);
    auto ready = [&] {
        std::lock_guard lock(before.bob->callMutex_);
        return not before.bob->isReinviteBusyLocked();
    };
    deadline = std::chrono::steady_clock::now() + 5s;
    while (not ready() and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CPPUNIT_ASSERT(ready());
    CPPUNIT_ASSERT_EQUAL(generation, before.bob->recoveryGeneration());
    CPPUNIT_ASSERT(before.bob->getTransport()->get() == before.bobTransport);
    CPPUNIT_ASSERT(not before.bob->isRetiredSipTransport(before.bobTransport));
    CPPUNIT_ASSERT(before.bob->getIceMedia() == before.bobIce);
    hangUp();
}

void
HandoverTest::testRejectsRecoveryOfferWithoutUsableMedia()
{
    startCall();
    auto before = currentState();
    auto& call = *before.bob;
    const auto generation = call.recoveryGeneration();
    const pjmedia_sdp_session* local;
    const pjmedia_sdp_session* remote;
    {
        std::lock_guard lock(call.callMutex_);
        local = call.sdp_->getActiveLocalSdpSession();
        remote = call.sdp_->getActiveRemoteSdpSession();
    }
    static std::atomic_int status;
    status = 0;
    auto request = sendRecoveryOffer(before.alice, generation + 1, true, status);
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (not status and std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CPPUNIT_ASSERT_EQUAL(int(PJSIP_SC_NOT_ACCEPTABLE_HERE), status.load());
    {
        std::lock_guard lock(call.callMutex_);
        CPPUNIT_ASSERT_EQUAL(generation, call.recoveryGeneration());
        CPPUNIT_ASSERT(call.isCurrentMediaNegotiation());
        CPPUNIT_ASSERT(call.sdp_->getActiveLocalSdpSession() == local);
        CPPUNIT_ASSERT(call.sdp_->getActiveRemoteSdpSession() == remote);
        CPPUNIT_ASSERT(not call.isRecovering());
    }
    auto audioOnly = before.bob->currentMediaList();
    std::erase_if(audioOnly, [](const auto& media) {
        return media.at(libjami::Media::MediaAttributeKey::MEDIA_TYPE) != libjami::Media::MediaAttributeValue::AUDIO;
    });
    CPPUNIT_ASSERT(libjami::requestMediaChange(bobId_, bobCallId_, audioOnly));
    waitForMediaChange(false);
    hangUp();
}

void
HandoverTest::testQueuedMediaCompletionChecksGeneration()
{
    startCall();
    auto before = currentState();
    auto replacement = Manager::instance().getIceTransportFactory()->createTransport("queued-completion");
    CPPUNIT_ASSERT(replacement and not replacement->isInitialized() and not replacement->isFailed());
    CPPUNIT_ASSERT(before.alice->remoteHasValidIceAttributes());
    auto result = std::make_shared<std::promise<bool>>();
    auto future = result->get_future();
    runOnMainThread([call = before.alice, replacement, result] {
        std::lock_guard lock(call->callMutex_);
        auto generation = call->recoveryGeneration();
        call->setIceMedia(replacement, true);
        call->waitForIceInit_ = false;
        call->onMediaNegotiationComplete();
        call->recoveryGeneration_ = generation + 1;
        call->mediaNegotiationGeneration_ = generation + 1;
        runOnMainThread([call, generation, result] {
            std::lock_guard lock(call->callMutex_);
            bool staleDiscarded = not call->waitForIceInit_;
            call->onMediaNegotiationComplete();
            runOnMainThread([call, generation, result, staleDiscarded] {
                std::lock_guard lock(call->callMutex_);
                bool currentHandled = call->waitForIceInit_;
                call->setIceMedia({}, true);
                call->waitForIceInit_ = false;
                call->recoveryGeneration_ = generation;
                call->mediaNegotiationGeneration_ = generation;
                result->set_value(staleDiscarded and currentHandled);
            });
        });
    });
    CPPUNIT_ASSERT(future.wait_for(5s) == std::future_status::ready);
    CPPUNIT_ASSERT(future.get());
    CPPUNIT_ASSERT(before.alice->getIceMedia() == before.aliceIce);
    hangUp();
}

void
HandoverTest::testIgnoresRequestsOnAbandonedChannel()
{
    startCall();
    auto before = currentState();
    auto oldAlice = Manager::instance().sipVoIPLink().sipTransportBroker->addTransport(before.aliceTransport);
    auto oldBob = Manager::instance().sipVoIPLink().sipTransportBroker->addTransport(before.bobTransport);
    alice_->networkInterfaceChanged();
    checkRecovered(before);
    auto recovered = currentState();

    // Bob sends requests on the previous channel, as if queued before he
    // followed Alice: none may bring her back to it.
    auto send = [&](const pjsip_method& method, std::optional<pj_int32_t> cseq,
                    std::optional<std::uint64_t> generation, std::atomic_int* status) {
        std::lock_guard lock(before.bob->callMutex_);
        auto* dialog = before.bob->inviteSession_->dlg;
        sip_utils::PJDialogLock dialogLock {dialog};
        pjsip_tx_data* tdata = nullptr;
        auto used = cseq ? *cseq : dialog->local.cseq++;
        if (pjsip_dlg_create_request(dialog, &method, used, &tdata) != PJ_SUCCESS)
            return -1;
        if (generation)
            sip_utils::addHandoverGeneration(tdata, *generation);
        tdata->mod_data[dialog->ua->id] = nullptr;
        pjsip_tpselector selector {};
        selector.type = PJSIP_TPSELECTOR_TRANSPORT;
        selector.u.transport = before.bobTransport;
        pjsip_tx_data_set_transport(tdata, &selector);
        if (pjsip_endpt_send_request(Manager::instance().sipVoIPLink().getEndpoint(), tdata, -1, status,
                                     status ? &storeFinalStatus : nullptr)
            != PJ_SUCCESS)
            return -1;
        return static_cast<int>(used);
    };
    auto aliceRemoteCseq = [&] {
        auto* dialog = before.alice->inviteSession_->dlg;
        sip_utils::PJDialogLock dialogLock {dialog};
        return dialog->remote.cseq;
    };

    // A new request is accepted on any channel, ordered by its CSeq.
    auto fresh = send(pjsip_options_method, std::nullopt, std::nullopt, nullptr);
    CPPUNIT_ASSERT(fresh >= 0);
    for (int i = 0; i < 50 and aliceRemoteCseq() != fresh; ++i)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT_EQUAL(fresh, static_cast<int>(aliceRemoteCseq()));

    // Stale requests are refused by the dialog, and an offer for a previous
    // handover generation before reaching it.
    // Static as a transaction may report after a failed assertion.
    static std::atomic_int byeStatus;
    static std::atomic_int inviteStatus;
    byeStatus = 0;
    inviteStatus = 0;
    CPPUNIT_ASSERT(send(pjsip_bye_method, fresh, std::nullopt, &byeStatus) >= 0);
    CPPUNIT_ASSERT(send(pjsip_invite_method, std::nullopt, 0, &inviteStatus) >= 0);
    for (int i = 0; i < 100 and (not byeStatus or not inviteStatus); ++i)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT_EQUAL(int(PJSIP_SC_INTERNAL_SERVER_ERROR), byeStatus.load());
    CPPUNIT_ASSERT_EQUAL(int(PJSIP_SC_REQUEST_PENDING), inviteStatus.load());

    std::this_thread::sleep_for(1s);
    auto after = currentState();
    CPPUNIT_ASSERT(after.aliceTransport == recovered.aliceTransport);
    CPPUNIT_ASSERT(after.aliceIce == recovered.aliceIce);
    CPPUNIT_ASSERT_EQUAL(recovered.aliceNegotiations, after.aliceNegotiations);
    CPPUNIT_ASSERT(before.alice->getConnectionState() == Call::ConnectionState::CONNECTED);
    CPPUNIT_ASSERT(before.alice->isRetiredSipTransport(oldAlice->get()));
    {
        std::lock_guard lock(mutex_);
        CPPUNIT_ASSERT_EQUAL(0u, ended_);
    }
    hangUp();
}

void
HandoverTest::testHandoverRequestClassification()
{
    auto* pool = pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                               "handover-classify-test", 4096, 4096, nullptr);
    CPPUNIT_ASSERT(pool);
    pjsip_msg_body body {};
    auto classify = [&](const pjsip_method& method, bool caller, std::uint64_t current,
                        std::optional<std::uint64_t> generation, std::optional<std::uint64_t> request,
                        bool withBody = true) {
        pjsip_tx_data sent {};
        sent.pool = pool;
        sent.msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
        sent.msg->line.req.method = method;
        if (withBody)
            sent.msg->body = &body;
        if (generation)
            sip_utils::addHandoverGeneration(&sent, *generation);
        if (request)
            sip_utils::addHandoverRequest(&sent, *request);
        pjsip_rx_data received {};
        received.msg_info.msg = sent.msg;
        return SIPCall::classifyHandoverRequest(&received, caller, current);
    };
    using Kind = SIPCall::HandoverRequest;
    pjsip_method info {};
    auto infoName = sip_utils::CONST_PJ_STR("INFO");
    pjsip_method_init_np(&info, &infoName);

    std::vector<std::pair<const char*, bool>> checks {
        {"older-generation probe to caller", classify(pjsip_options_method, true, 2, 0, 1) == Kind::Probe},
        {"initial-generation probe", classify(pjsip_options_method, true, 0, 0, 1, false) == Kind::Probe},
        {"current-generation probe", classify(pjsip_options_method, true, 2, 2, 1, false) == Kind::Probe},
        {"probe without generation", classify(pjsip_options_method, true, 2, std::nullopt, 1, false) == Kind::Invalid},
        {"probe to callee", classify(pjsip_options_method, false, 2, 0, 1) == Kind::Pass},
        {"unnumbered probe", classify(pjsip_options_method, true, 2, 0, 0) == Kind::Pass},
        {"plain options", classify(pjsip_options_method, true, 2, std::nullopt, std::nullopt) == Kind::Pass},
        {"options without request number", classify(pjsip_options_method, true, 2, 0, std::nullopt) == Kind::Pass},
        {"legacy invite", classify(pjsip_invite_method, false, 2, std::nullopt, std::nullopt) == Kind::Pass},
        {"stale offer to callee", classify(pjsip_invite_method, false, 2, 1, std::nullopt) == Kind::Stale},
        {"stale offer to caller", classify(pjsip_invite_method, true, 2, 1, std::nullopt) == Kind::Stale},
        {"current offer", classify(pjsip_invite_method, true, 2, 2, std::nullopt) == Kind::Pass},
        {"recovery offer", classify(pjsip_invite_method, false, 2, 3, std::nullopt) == Kind::Pass},
        {"recovery without offer", classify(pjsip_invite_method, false, 2, 3, std::nullopt, false) == Kind::Invalid},
        {"generation from callee", classify(pjsip_invite_method, true, 2, 3, std::nullopt) == Kind::Invalid},
        {"stale info", classify(info, false, 2, 0, std::nullopt) == Kind::Pass},
        {"bye", classify(pjsip_bye_method, true, 2, 0, std::nullopt) == Kind::Pass},
        {"no message", SIPCall::classifyHandoverRequest(nullptr, true, 2) == Kind::Pass},
    };

    pjsip_tx_data sent {};
    sent.pool = pool;
    sent.msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
    sent.msg->line.req.method = pjsip_invite_method;
    sent.msg->body = &body;
    auto name = sip_utils::CONST_PJ_STR("X-Jami-Handover-Generation");
    auto value = sip_utils::CONST_PJ_STR("1x");
    pjsip_msg_add_hdr(sent.msg, reinterpret_cast<pjsip_hdr*>(pjsip_generic_string_hdr_create(pool, &name, &value)));
    pjsip_rx_data received {};
    received.msg_info.msg = sent.msg;
    checks.emplace_back("malformed generation",
                        SIPCall::classifyHandoverRequest(&received, false, 2) == Kind::Invalid);

    for (const auto* text : {"", "-1", "+1", "1x", " 1", "1 ", "18446744073709551616"}) {
        sent.msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
        sent.msg->line.req.method = pjsip_options_method;
        sip_utils::addHandoverRequest(&sent, 1);
        auto malformed = sip_utils::CONST_PJ_STR(std::string_view(text));
        pjsip_msg_add_hdr(sent.msg,
                          reinterpret_cast<pjsip_hdr*>(pjsip_generic_string_hdr_create(pool, &name, &malformed)));
        received.msg_info.msg = sent.msg;
        checks.emplace_back("malformed probe generation",
                            SIPCall::classifyHandoverRequest(&received, true, 2) == Kind::Invalid);
    }
    for (const auto duplicate : {0u, 1u}) {
        sent.msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
        sent.msg->line.req.method = pjsip_options_method;
        sip_utils::addHandoverRequest(&sent, 1);
        sip_utils::addHandoverGeneration(&sent, 0);
        sip_utils::addHandoverGeneration(&sent, duplicate);
        received.msg_info.msg = sent.msg;
        checks.emplace_back("duplicate probe generation",
                            SIPCall::classifyHandoverRequest(&received, true, 2) == Kind::Invalid);
    }
    pj_pool_release(pool);

    for (const auto& [check, passed] : checks)
        CPPUNIT_ASSERT_MESSAGE(check, passed);
}

void
HandoverTest::testHandoverHeaderValidation()
{
    auto* pool = pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                               "handover-header-test", 4096, 4096, nullptr);
    CPPUNIT_ASSERT(pool);
    pjsip_tx_data sent {};
    sent.pool = pool;
    sent.msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
    sip_utils::addHandoverGeneration(&sent, 42);
    sip_utils::addHandoverRequest(&sent, 7);
    sip_utils::addCallHandoverHeader(&sent);
    pjsip_rx_data received {};
    received.msg_info.msg = sent.msg;
    bool roundTrip = sip_utils::getHandoverGeneration(&received) == 42
                     and sip_utils::getHandoverRequest(&received) == 7 and sip_utils::supportsCallHandover(&received);
    sip_utils::addHandoverGeneration(&sent, 42);
    bool duplicateRejected = not sip_utils::getHandoverGeneration(&received);
    bool malformedRejected = true;
    auto name = sip_utils::CONST_PJ_STR("X-Jami-Handover-Generation");
    for (const auto* text : {"", "-1", "+1", "1x", "18446744073709551616"}) {
        sent.msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
        auto value = sip_utils::CONST_PJ_STR(std::string_view(text));
        auto* header = pjsip_generic_string_hdr_create(pool, &name, &value);
        pjsip_msg_add_hdr(sent.msg, reinterpret_cast<pjsip_hdr*>(header));
        received.msg_info.msg = sent.msg;
        malformedRejected &= sip_utils::hasHandoverGeneration(&received)
                             and not sip_utils::getHandoverGeneration(&received);
    }
    pj_pool_release(pool);
    CPPUNIT_ASSERT(roundTrip and duplicateRejected and malformedRejected);
}

void
HandoverTest::testRetirementIsPerCall()
{
    startCall();
    auto before = currentState();
    auto old = Manager::instance().sipVoIPLink().sipTransportBroker->addTransport(before.aliceTransport);
    auto otherCall = std::make_shared<SIPCall>(alice_, "shared-channel-call", Call::CallType::OUTGOING,
                                               before.alice->currentMediaList());
    otherCall->setSipTransport(old, alice_->getContactHeader(old));
    alice_->networkInterfaceChanged();
    checkRecovered(before);
    CPPUNIT_ASSERT(before.alice->isRetiredSipTransport(old->get()));
    CPPUNIT_ASSERT(not otherCall->isRetiredSipTransport(old->get()));
    CPPUNIT_ASSERT(otherCall->getTransport() == old.get());
    CPPUNIT_ASSERT(old->isConnected());
    hangUp();
}

void
HandoverTest::testPeerMethodsSkipCallLock()
{
    startCall();
    auto before = currentState();
    CPPUNIT_ASSERT(before.alice and before.alice->inviteSession_);
    std::promise<void> locked;
    std::promise<void> release;
    std::thread holder([&, released = release.get_future()] {
        std::lock_guard lock(before.alice->callMutex_);
        locked.set_value();
        released.wait_for(5s);
    });
    locked.get_future().wait();
    auto start = std::chrono::steady_clock::now();
    {
        // As invite_session_state_changed_cb does, while a renegotiation holds callMutex_.
        sip_utils::PJDialogLock dialogLock {before.alice->inviteSession_->dlg};
        before.alice->setPeerAllowMethods({"INVITE", "MESSAGE"});
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    release.set_value();
    holder.join();
    CPPUNIT_ASSERT(elapsed < 1s);
    CPPUNIT_ASSERT(before.alice->isSipMethodAllowedByPeer("MESSAGE"));
    hangUp();
}

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::HandoverTest::name())
