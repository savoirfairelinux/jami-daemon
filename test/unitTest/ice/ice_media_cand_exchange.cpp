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
#include "jamidht/jamiaccount.h"
#include "sip/sipaccount.h"
#include "../../test_runner.h"

#include "jami.h"
#include "media_const.h"
#include "call_const.h"
#include "account_const.h"
#include "sip/sipcall.h"

#include "connectivity/upnp/upnp_control.h"

#include "common.h"

using namespace libjami::Account;
using namespace libjami::Call;

namespace jami {
namespace test {

struct CallData
{
    struct Signal
    {
        Signal(const std::string& callId, const std::string& name, const std::string& event = {})
            : callId_(std::move(callId))
            , name_(std::move(name))
            , event_(std::move(event)) {};

        std::string callId_ {};
        std::string name_ {};
        std::string event_ {};
    };

    std::string accountId_ {};
    std::string displayName_ {};
    std::string userName_ {};
    std::string alias_ {};
    std::string callId_ {};
    std::vector<Signal> signals_;
    std::condition_variable cv_ {};
    std::mutex mtx_;
    bool deviceAnnounced_ {false};
    bool upnpEnabled_ {false};
    bool turnEnabled_ {false};
    IpAddr dest_ {};
    // SIP accounts only
    uint16_t listeningPort_ {0};
};

class IceMediaCandExchangeTest : public CppUnit::TestFixture
{
public:
    IceMediaCandExchangeTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~IceMediaCandExchangeTest() { libjami::fini(); }

    static std::string name() { return "IceMediaCandExchangeTest"; }
    void setUp();
    void tearDown();

private:
    // Test cases.
    // This is not actual test, it only checks if upnp is
    // functional. This information is needed to validate
    // generated ICE srflx candidates.
    void check_upnp();
    void jami_account_no_turn();
    void jami_account_with_turn();
    void sip_account_no_turn();
    void sip_account_with_turn();

    CPPUNIT_TEST_SUITE(IceMediaCandExchangeTest);
    CPPUNIT_TEST(check_upnp);
    CPPUNIT_TEST(jami_account_no_turn);
    CPPUNIT_TEST(jami_account_with_turn);
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
    void test_call(const char* accountType);
    static void validate_ice_candidates(CallData& user,
                                        const char* accountType,
                                        bool hasUpnp,
                                        bool upnpSameAsPublished);
    static void setupJamiAccount(CallData& user);
    static void setupSipAccount(CallData& user);
    static void setupAccounts(CallData& bob, CallData& alice, const char* accountType);
    static void configureAccount(CallData& user, const char* accountType);
    static std::string getUserAlias(const std::string& callId);
    // Wait for a signal from the callbacks. Some signals also report the event that
    // triggered the signal a like the StateChange signal.
    static bool waitForSignal(CallData& callData,
                              const std::string& signal,
                              const std::string& expectedEvent = {});

private:
    CallData aliceData_;
    CallData bobData_;
    const size_t MEDIA_COUNT {2};
    std::condition_variable upnpCv_ {};
    std::mutex upnpMtx_;
    bool hasUpnp_ {false};
    bool upnpAddrSameAsPublished_ {false};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(IceMediaCandExchangeTest, IceMediaCandExchangeTest::name());

void
IceMediaCandExchangeTest::setUp()
{
    // Everything is done in ConfigureTest()
}

void
IceMediaCandExchangeTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");
    wait_for_removal_of({aliceData_.accountId_, bobData_.accountId_});
}

std::string
IceMediaCandExchangeTest::getUserAlias(const std::string& callId)
{
    auto call = Manager::instance().getCallFromCallID(callId);

    if (call) {
        auto const& account = call->getAccount().lock();
        if (not account) {
            return {};
        }

        return account->getAccountDetails()[ConfProperties::ALIAS];
    }

    JAMI_WARN("Call [%s] does not exist!", callId.c_str());
    return {};
}

void
IceMediaCandExchangeTest::onIncomingCallWithMedia(const std::string& accountId,
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
        JAMI_WARN("Call [%s] does not exist!", callId.c_str());
        callData.callId_ = {};
        return;
    }

    std::unique_lock<std::mutex> lock {callData.mtx_};
    callData.callId_ = callId;
    callData.signals_.emplace_back(
        CallData::Signal(callId, libjami::CallSignal::IncomingCallWithMedia::name));

    callData.cv_.notify_one();
}

void
IceMediaCandExchangeTest::onCallStateChange(const std::string&,
                                            const std::string& callId,
                                            const std::string& state,
                                            CallData& callData)
{
    JAMI_INFO("Signal [%s] - user [%s] - call [%s] - state [%s]",
              libjami::CallSignal::StateChange::name,
              callData.alias_.c_str(),
              callId.c_str(),
              state.c_str());
    {
        std::unique_lock<std::mutex> lock {callData.mtx_};
        callData.signals_.emplace_back(
            CallData::Signal(callId, libjami::CallSignal::StateChange::name, state));
    }

    if (state == "RINGING" or state == "CURRENT" or state == "HUNGUP" or state == "OVER") {
        callData.cv_.notify_one();
    }
}

void
IceMediaCandExchangeTest::onMediaNegotiationStatus(const std::string& callId,
                                                   const std::string& event,
                                                   CallData& callData)
{
    auto call = Manager::instance().getCallFromCallID(callId);
    if (not call) {
        JAMI_WARN("Call [%s] does not exist!", callId.c_str());
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
            CallData::Signal(callId, libjami::CallSignal::MediaNegotiationStatus::name, event));
    }

    callData.cv_.notify_one();
}

bool
IceMediaCandExchangeTest::waitForSignal(CallData& callData,
                                        const std::string& expectedSignal,
                                        const std::string& expectedEvent)
{
    const std::chrono::seconds TIME_OUT {45};
    std::unique_lock<std::mutex> lock {callData.mtx_};

    // Combined signal + event (if any).
    std::string sigEvent(expectedSignal);
    if (not expectedEvent.empty())
        sigEvent += "::" + expectedEvent;

    JAMI_INFO("[%s] is waiting for [%s] signal/event", callData.alias_.c_str(), sigEvent.c_str());

    auto res = callData.cv_.wait_for(lock, TIME_OUT, [&] {
        // Search for the expected signal in list of received signals.
        for (auto it = callData.signals_.begin(); it != callData.signals_.end(); it++) {
            // The predicate is true if the signal names match, and if the
            // expectedEvent is not empty, the events must also match.
            if (it->name_ == expectedSignal
                and (expectedEvent.empty() or it->event_ == expectedEvent)) {
                // Done with this signal.
                callData.signals_.erase(it);
                return true;
            }
        }
        // Signal/event not found.
        return false;
    });

    if (not res) {
        JAMI_ERR("[%s] waiting for signal/event [%s] [call:%s] timed-out!",
                 callData.alias_.c_str(),
                 sigEvent.c_str(),
                 callData.callId_.c_str());

        JAMI_INFO("[%s] currently has the following signals:", callData.alias_.c_str());

        for (auto const& sig : callData.signals_) {
            JAMI_INFO() << "Signal [" << sig.name_
                        << (sig.event_.empty() ? "" : ("::" + sig.event_)) << "] "
                        << "call [" << sig.callId_ << "]";
        }
    }

    return res;
}

void
IceMediaCandExchangeTest::setupJamiAccount(CallData& user)
{
    auto account = Manager::instance().getAccount<JamiAccount>(user.accountId_);
    auto details = account->getAccountDetails();

    user.userName_ = details[ConfProperties::USERNAME];
    user.alias_ = details[ConfProperties::ALIAS];

    // Apply the settings according to the test case
    details[ConfProperties::UPNP_ENABLED] = user.upnpEnabled_ ? "true" : "false";
    details[ConfProperties::TURN::ENABLED] = user.turnEnabled_ ? "true" : "false";
    libjami::setAccountDetails(user.accountId_, details);
}

void
IceMediaCandExchangeTest::setupSipAccount(CallData& user)
{
    CPPUNIT_ASSERT_GREATER(0, static_cast<int>(user.listeningPort_));

    auto details = libjami::getAccountTemplate(libjami::Account::ProtocolNames::SIP);
    details[ConfProperties::TYPE] = libjami::Account::ProtocolNames::SIP;
    details[ConfProperties::DISPLAYNAME] = user.displayName_.c_str();
    details[ConfProperties::ALIAS] = user.displayName_.c_str();
    details[ConfProperties::LOCAL_PORT] = std::to_string(user.listeningPort_);
    details[ConfProperties::PUBLISHED_PORT] = std::to_string(user.listeningPort_);
    details[ConfProperties::PUBLISHED_SAMEAS_LOCAL] = "true";
    details[ConfProperties::SRTP::ENABLED] = "false";
    details[ConfProperties::SRTP::KEY_EXCHANGE] = "NONE";

    // Apply the settings according to the test case
    details[ConfProperties::UPNP_ENABLED] = user.upnpEnabled_ ? "true" : "false";
    details[ConfProperties::TURN::ENABLED] = user.turnEnabled_ ? "true" : "false";

    user.accountId_ = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(not user.accountId_.empty());
    auto const& account = Manager::instance().getAccount<Account>(user.accountId_);
    details = account->getAccountDetails();
    user.userName_ = details[ConfProperties::USERNAME];
    user.alias_ = details[ConfProperties::ALIAS];
    user.dest_ = ip_utils::getLocalAddr(AF_INET);
    user.dest_.setPort(user.listeningPort_);
}

void
IceMediaCandExchangeTest::setupAccounts(CallData& aliceData,
                                        CallData& bobData,
                                        const char* accountType)
{
    if (strcmp(accountType, libjami::Account::ProtocolNames::SIP) == 0) {
        JAMI_INFO("Setup SIP accounts and configure test case ...");

        aliceData.displayName_ = "ALICE";
        aliceData.listeningPort_ = 5080;
        setupSipAccount(aliceData);

        bobData.displayName_ = "BOB";
        bobData.listeningPort_ = 5082;
        setupSipAccount(bobData);
    } else {
        JAMI_INFO("Setup JAMI accounts and configure test case ...");
        auto actors = load_actors("actors/alice-bob-no-upnp.yml");

        aliceData.accountId_ = actors["alice"];
        auto const& aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData.accountId_);

        bobData.accountId_ = actors["bob"];
        auto const& bobAccount = Manager::instance().getAccount<JamiAccount>(bobData.accountId_);

        wait_for_announcement_of({aliceAccount->getAccountID(), bobAccount->getAccountID()});

        setupJamiAccount(bobData);
        setupJamiAccount(aliceData);
    }

    // Setup signal handlers.
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> signalHandlers;

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCallWithMedia>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<libjami::MediaMap> mediaList) {
            auto user = getUserAlias(callId);
            if (user.empty()) {
                // The call was probably already removed, in this case, just
                // check callId against current calls.
                if (callId == aliceData.callId_)
                    user = aliceData.alias_;
                if (callId == bobData.callId_)
                    user = bobData.alias_;
            }

            if (not user.empty()) {
                onIncomingCallWithMedia(accountId,
                                        callId,
                                        mediaList,
                                        user == aliceData.alias_ ? aliceData : bobData);
            } else {
                JAMI_WARN("Received [CallSignal::IncomingCallWithMedia] for a removed call [%s]",
                          callId.c_str());
            }
        }));

    signalHandlers.insert(
        libjami::exportable_callback<libjami::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            auto user = getUserAlias(callId);
            if (user.empty()) {
                // The call was probably already removed, in this case, just
                // check callId against current calls.
                if (callId == aliceData.callId_)
                    user = aliceData.alias_;
                if (callId == bobData.callId_)
                    user = bobData.alias_;
            }
            if (not user.empty()) {
                onCallStateChange(accountId,
                                  callId,
                                  state,
                                  user == aliceData.alias_ ? aliceData : bobData);
            } else {
                JAMI_WARN("Received [StateChange::%s] for a removed call [%s]",
                          state.c_str(),
                          callId.c_str());
            }
        }));

    signalHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string& callId,
            const std::string& event,
            const std::vector<std::map<std::string, std::string>>& /* mediaList */) {
            auto user = getUserAlias(callId);
            if (user.empty()) {
                // The call was probably already removed, in this case, just
                // check callId against current calls.
                if (callId == aliceData.callId_)
                    user = aliceData.alias_;
                if (callId == bobData.callId_)
                    user = bobData.alias_;
            }
            if (not user.empty()) {
                onMediaNegotiationStatus(callId,
                                         event,
                                         user == aliceData.alias_ ? aliceData : bobData);
            } else {
                JAMI_WARN("Received [CallSignal::MediaNegotiationStatus] for a removed call [%s]",
                          callId.c_str());
            }
        }));

    signalHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>&) {
                auto account = Manager::instance().getAccount(accountId);
                if (not account) {
                    JAMI_ERR("No account with ID [%s]", accountId.c_str());
                    return;
                }

                auto details = account->getVolatileAccountDetails();
                auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];

                if (accountId == aliceData.accountId_) {
                    aliceData.deviceAnnounced_ = deviceAnnounced == "true";
                    aliceData.cv_.notify_one();
                } else if (accountId == bobData.accountId_) {
                    bobData.deviceAnnounced_ = deviceAnnounced == "true";
                    bobData.cv_.notify_one();
                } else {
                    JAMI_ERR("Account with ID [%s] is unknown", accountId.c_str());
                }
            }));

    libjami::registerSignalHandlers(signalHandlers);
}

void
IceMediaCandExchangeTest::configureAccount(CallData& user, const char* accountType)
{
    auto details
        = strcmp(accountType, libjami::Account::ProtocolNames::RING) == 0
              ? Manager::instance().getAccount<JamiAccount>(user.accountId_)->getAccountDetails()
              : Manager::instance().getAccount<SIPAccount>(user.accountId_)->getAccountDetails();

    // Apply the settings according to the test case
    details[ConfProperties::UPNP_ENABLED] = user.upnpEnabled_ ? "true" : "false";
    details[ConfProperties::TURN::ENABLED] = user.turnEnabled_ ? "true" : "false";

    libjami::setAccountDetails(user.accountId_, details);

    // Note: setAccountDetails will trigger a re-register of the account, so
    // we need to wait for the registration to proceed.
    // Only done for JAMI accounts.

    if (strcmp(accountType, libjami::Account::ProtocolNames::RING) != 0)
        return;

    auto account = Manager::instance().getAccount<JamiAccount>(user.accountId_);
    CPPUNIT_ASSERT(account);

    JAMI_INFO("Waiting for [%s] account [%s] to register ...",
              user.alias_.c_str(),
              user.accountId_.c_str());
    std::unique_lock<std::mutex> lock {user.mtx_};
    CPPUNIT_ASSERT(
        user.cv_.wait_for(lock, std::chrono::seconds(60), [&] { return user.deviceAnnounced_; }));
}

void
IceMediaCandExchangeTest::validate_ice_candidates(CallData& user,
                                                  const char* accountType,
                                                  bool hasUpnp,
                                                  bool upnpSameAsPublished)
{
    auto sipCall = std::dynamic_pointer_cast<SIPCall>(
        Manager::instance().getCallFromCallID(user.callId_));
    CPPUNIT_ASSERT(sipCall);

    // Only check the first component for now.
    auto localCand = sipCall->getLocalIceCandidates(1);
    // Should not be empty.
    CPPUNIT_ASSERT(not localCand.empty());

    int srflxCandCount = 0;
    int relayCandCount = 0;
    for (auto const& cand : localCand) {
        JAMI_INFO("[%s] ICE cand: [%s]", user.alias_.c_str(), cand.c_str());
        if (cand.find("srflx") != std::string::npos) {
            srflxCandCount++;
        } else if (cand.find("relay") != std::string::npos) {
            relayCandCount++;
        }
    }

    // Note:
    // Enabling UPNP does not guarantee that we have valid UPNP srflx
    // candidate. However, for RING accounts, we must have at least one
    // srflx candidate generated using the public address discovered
    // after connecting to the DHT.
    // For SIP accounts, we use un-registered (P2P) mode), so srflx
    // candidates are only added if UPNP is enabled and the mapping is
    // successful.
    int expectedSrflxCandCount = 0;
    if (strcmp(accountType, libjami::Account::ProtocolNames::RING) == 0) {
        if (hasUpnp) {
            if (upnpSameAsPublished) {
                // UPNP and published are the same, published wont be added.
                expectedSrflxCandCount = 1;
            } else {
                // Both UPNP and published will be added.
                expectedSrflxCandCount = 2;
            }
        } else {
            // No UPNP, only published address.
            expectedSrflxCandCount = 1;
        }
    } else {
        // SIP accounts in un-registered mode will have only UPNP srflx
        // candidates if available
        if (hasUpnp) {
            expectedSrflxCandCount = 1;
        }
    }

    // Validate srflx
    CPPUNIT_ASSERT_EQUAL(expectedSrflxCandCount, srflxCandCount);

    // Validate relay
    CPPUNIT_ASSERT_EQUAL(user.turnEnabled_ ? 1 : 0, relayCandCount);
}

void
IceMediaCandExchangeTest::test_call(const char* accountType)
{
    JAMI_INFO("=== Start a call and validate ICE for account type [%s] ===", accountType);

    MediaAttribute media_0(MediaType::MEDIA_AUDIO);
    media_0.label_ = "audio_0";
    media_0.enabled_ = true;
    MediaAttribute media_1(MediaType::MEDIA_VIDEO);
    media_1.label_ = "video_0";
    media_1.enabled_ = true;

    std::vector<MediaAttribute> offer;
    offer.emplace_back(media_0);
    offer.emplace_back(media_1);

    std::vector<MediaAttribute> answer;
    answer.emplace_back(media_0);
    answer.emplace_back(media_1);

    CPPUNIT_ASSERT_EQUAL(MEDIA_COUNT, offer.size());
    CPPUNIT_ASSERT_EQUAL(MEDIA_COUNT, answer.size());

    // Set the destination according to account type.
    auto dest = strcmp(accountType, libjami::Account::ProtocolNames::SIP) == 0
                    ? bobData_.dest_.toString(true)
                    : bobData_.userName_;

    CPPUNIT_ASSERT(not dest.empty());

    aliceData_.callId_ = libjami::placeCallWithMedia(aliceData_.accountId_,
                                                   dest,
                                                   MediaAttribute::mediaAttributesToMediaMaps(
                                                       offer));
    CPPUNIT_ASSERT(not aliceData_.callId_.empty());

    JAMI_INFO("ALICE [%s] started a call with BOB [%s:%s] and wait for answer",
              aliceData_.accountId_.c_str(),
              bobData_.accountId_.c_str(),
              dest.c_str());

    // Give it some time to ring
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Wait for call to be processed.
    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, libjami::CallSignal::StateChange::name, StateEvent::RINGING));

    // Wait for incoming call signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_, libjami::CallSignal::IncomingCallWithMedia::name));

    // Answer the call.
    libjami::acceptWithMedia(bobData_.accountId_,
                           bobData_.callId_,
                           MediaAttribute::mediaAttributesToMediaMaps(answer));

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(bobData_,
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Wait for the StateChange signal.
    CPPUNIT_ASSERT(
        waitForSignal(bobData_, libjami::CallSignal::StateChange::name, StateEvent::CURRENT));

    JAMI_INFO("BOB answered the call [%s]", bobData_.callId_.c_str());

    // Wait for media negotiation complete signal.
    CPPUNIT_ASSERT(waitForSignal(aliceData_,
                                 libjami::CallSignal::MediaNegotiationStatus::name,
                                 libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS));

    // Validate ICE candidates
    validate_ice_candidates(aliceData_, accountType, hasUpnp_, upnpAddrSameAsPublished_);
    validate_ice_candidates(bobData_, accountType, hasUpnp_, upnpAddrSameAsPublished_);

    // Give some time to media to start.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Bob hang-up.
    JAMI_INFO("Hang up BOB's call and wait for ALICE to hang up");
    Manager::instance().hangupCall(bobData_.accountId_, bobData_.callId_);

    CPPUNIT_ASSERT(
        waitForSignal(bobData_, libjami::CallSignal::StateChange::name, StateEvent::HUNGUP));

    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, libjami::CallSignal::StateChange::name, StateEvent::HUNGUP));

    CPPUNIT_ASSERT(waitForSignal(bobData_, libjami::CallSignal::StateChange::name, StateEvent::OVER));

    CPPUNIT_ASSERT(
        waitForSignal(aliceData_, libjami::CallSignal::StateChange::name, StateEvent::OVER));

    JAMI_INFO("Call terminated on both sides");

    // Reset signals
    aliceData_.signals_.clear();
    aliceData_.callId_.clear();
    bobData_.signals_.clear();
    bobData_.callId_.clear();
}

void
IceMediaCandExchangeTest::check_upnp()
{
    auto accountType = libjami::Account::ProtocolNames::RING;
    setupAccounts(aliceData_, bobData_, accountType);
    auto const& account = Manager::instance().getAccount<JamiAccount>(aliceData_.accountId_);
    auto publishedAddr = account->getPublishedIpAddress(AF_INET);

    const std::chrono::seconds TIME_OUT {15};
    auto upnpCtrl = std::make_shared<upnp::Controller>();
    upnp::Mapping map {upnp::PortType::UDP};
    std::string upnpAddr {};

    map.setNotifyCallback([this_ = this, &publishedAddr, &upnpAddr](
                              upnp::Mapping::sharedPtr_t mapRes) {
        if (mapRes->getState() == upnp::MappingState::OPEN) {
            upnpAddr = mapRes->getExternalAddress();
            this_->upnpAddrSameAsPublished_ = publishedAddr.toString(false).compare(upnpAddr) == 0;
            this_->hasUpnp_ = true;
            this_->upnpCv_.notify_one();
        } else {
            this_->upnpAddrSameAsPublished_ = false;
            this_->hasUpnp_ = false;
        }
    });

    upnpCtrl->reserveMapping(map);

    JAMI_INFO("Waiting for upnp request response ...");

    std::unique_lock<std::mutex> lock(upnpMtx_);
    auto res = upnpCv_.wait_for(lock, TIME_OUT, [this_ = this] { return this_->hasUpnp_; });

    if (res) {
        JAMI_INFO("UPNP is available");
        if (upnpAddrSameAsPublished_) {
            JAMI_INFO("UPNP address [%s] same as published address: expect only one srflx cand per "
                      "component",
                      upnpAddr.c_str());
        } else {
            JAMI_INFO("UPNP [%s] and published [%s] addresses differ: expect two srflx cand per "
                      "component",
                      upnpAddr.c_str(),
                      publishedAddr.toString().c_str());
        }
    } else {
        JAMI_WARN("UPNP is not available (timeout!)");
    }
}

void
IceMediaCandExchangeTest::jami_account_no_turn()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto accountType = libjami::Account::ProtocolNames::RING;
    setupAccounts(aliceData_, bobData_, accountType);

    bobData_.turnEnabled_ = false;
    aliceData_.turnEnabled_ = false;

    {
        aliceData_.upnpEnabled_ = false;
        bobData_.upnpEnabled_ = false;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }

    {
        aliceData_.upnpEnabled_ = true;
        bobData_.upnpEnabled_ = false;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }

    {
        aliceData_.upnpEnabled_ = false;
        bobData_.upnpEnabled_ = true;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }

    {
        aliceData_.upnpEnabled_ = true;
        bobData_.upnpEnabled_ = true;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }
}

void
IceMediaCandExchangeTest::jami_account_with_turn()
{
    JAMI_INFO("=== Begin test %s ===", __FUNCTION__);

    auto accountType = libjami::Account::ProtocolNames::RING;

    setupAccounts(aliceData_, bobData_, accountType);
    bobData_.turnEnabled_ = true;
    aliceData_.turnEnabled_ = true;

    {
        aliceData_.upnpEnabled_ = false;
        bobData_.upnpEnabled_ = false;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }

    {
        aliceData_.upnpEnabled_ = true;
        bobData_.upnpEnabled_ = false;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }

    {
        aliceData_.upnpEnabled_ = false;
        bobData_.upnpEnabled_ = true;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }

    {
        aliceData_.upnpEnabled_ = true;
        bobData_.upnpEnabled_ = true;
        configureAccount(aliceData_, accountType);
        configureAccount(bobData_, accountType);
        test_call(accountType);
    }
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::IceMediaCandExchangeTest::name())
