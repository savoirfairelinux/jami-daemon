/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
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

#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "conversation/conversationcommon.h"
#include "manager.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct ConvData
{
    std::string id {};
    bool requestReceived {false};
    bool conferenceChanged {false};
    bool conferenceRemoved {false};
    std::string hostState {};
    std::vector<std::map<std::string, std::string>> messages {};
};

class ConversationCallTest : public CppUnit::TestFixture
{
public:
    ~ConversationCallTest() { DRing::fini(); }
    static std::string name() { return "ConversationCallTest"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;
    std::string carlaId;
    ConvData aliceData_;
    ConvData bobData_;
    ConvData bob2Data_;
    ConvData carlaData_;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

private:
    void connectSignals();
    void enableCarla();

    void testActiveCalls();
    void testActiveCalls3Peers();
    void testRejoinCall();
    void testParticipantHangupConfNotRemoved();
    void testJoinFinishedCall();
    void testJoinFinishedCallForbidden();
    void testUsePreference();

    CPPUNIT_TEST_SUITE(ConversationCallTest);
    CPPUNIT_TEST(testActiveCalls);
    CPPUNIT_TEST(testActiveCalls3Peers);
    CPPUNIT_TEST(testRejoinCall);
    CPPUNIT_TEST(testParticipantHangupConfNotRemoved);
    CPPUNIT_TEST(testJoinFinishedCall);
    CPPUNIT_TEST(testJoinFinishedCallForbidden);
    CPPUNIT_TEST(testUsePreference);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationCallTest, ConversationCallTest::name());

void
ConversationCallTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));

    auto actors = load_actors("actors/alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];
    aliceData_ = {};
    bobData_ = {};
    bob2Data_ = {};
    carlaData_ = {};

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
ConversationCallTest::tearDown()
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
ConversationCallTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId)
                aliceData_.id = conversationId;
            else if (accountId == bobId)
                bobData_.id = conversationId;
            else if (accountId == bob2Id)
                bob2Data_.id = conversationId;
            else if (accountId == carlaId)
                carlaData_.id = conversationId;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            if (accountId == aliceId && aliceData_.id == conversationId)
                aliceData_.messages.emplace_back(message);
            if (accountId == bobId && bobData_.id == conversationId)
                bobData_.messages.emplace_back(message);
            if (accountId == bob2Id && bob2Data_.id == conversationId)
                bob2Data_.messages.emplace_back(message);
            if (accountId == carlaId && carlaData_.id == conversationId)
                carlaData_.messages.emplace_back(message);
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& /*conversationId*/,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == aliceId)
                    aliceData_.requestReceived = true;
                if (accountId == bobId)
                    bobData_.requestReceived = true;
                if (accountId == bob2Id)
                    bob2Data_.requestReceived = true;
                if (accountId == carlaId)
                    carlaData_.requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::ConferenceChanged>(
        [&](const std::string& accountId, const std::string&, const std::string&) {
            if (accountId == aliceId)
                aliceData_.conferenceChanged = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::ConferenceRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId)
                aliceData_.conferenceRemoved = true;
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::CallSignal::StateChange>([&](const std::string& accountId,
                                                                       const std::string& callId,
                                                                       const std::string& state,
                                                                       signed) {
            if (accountId == aliceId) {
                auto details = DRing::getCallDetails(aliceId, callId);
                if (details.find("PEER_NUMBER") != details.end()) {
                    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
                    auto bobUri = bobAccount->getUsername();
                    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
                    auto carlaUri = carlaAccount->getUsername();

                    if (details["PEER_NUMBER"].find(bobUri) != std::string::npos)
                        bobData_.hostState = state;
                    else if (details["PEER_NUMBER"].find(carlaUri) != std::string::npos)
                        carlaData_.hostState = state;
                }
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
}

void
ConversationCallTest::enableCarla()
{
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    // Enable carla
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool carlaConnected = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced = details[DRing::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));
    confHandlers.clear();
    DRing::unregisterSignalHandlers();
}

void
ConversationCallTest::testActiveCalls()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    connectSignals();

    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);

    // start call
    aliceData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.messages.empty(); });
    CPPUNIT_ASSERT(aliceData_.messages[0]["type"] == "application/call-history+json");

    // get active calls = 1
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 1);

    // hangup
    aliceData_.messages.clear();
    Manager::instance().hangupCall(aliceId, callId);

    // should get message
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.messages.empty(); });
    CPPUNIT_ASSERT(aliceData_.messages[0].find("duration") != aliceData_.messages[0].end());

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);
}

void
ConversationCallTest::testActiveCalls3Peers()
{
    enableCarla();
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    DRing::addConversationMember(aliceId, aliceData_.id, bobUri);
    DRing::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));

    aliceData_.messages.clear();
    DRing::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    DRing::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });

    auto destination = fmt::format("swarm:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->at("uri"),
                                   bobData_.messages.rbegin()->at("device"),
                                   bobData_.messages.rbegin()->at("confId"));

    aliceData_.conferenceChanged = false;
    DRing::placeCallWithMedia(bobId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT";
    });
    aliceData_.conferenceChanged = false;
    DRing::placeCallWithMedia(carlaId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && carlaData_.hostState == "CURRENT";
    });

    // get 3 participants
    auto callList = DRing::getParticipantList(aliceId, bobData_.messages.rbegin()->at("confId"));
    CPPUNIT_ASSERT(callList.size() == 3);

    // get active calls = 1
    CPPUNIT_ASSERT(DRing::getActiveCalls(bobId, bobData_.id).size() == 1);

    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupCall(aliceId, callId);

    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });
    CPPUNIT_ASSERT(aliceData_.messages[0].find("duration") != aliceData_.messages[0].end());
    CPPUNIT_ASSERT(bobData_.messages[0].find("duration") != bobData_.messages[0].end());
    CPPUNIT_ASSERT(carlaData_.messages[0].find("duration") != carlaData_.messages[0].end());

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);
}

void
ConversationCallTest::testRejoinCall()
{
    enableCarla();
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    DRing::addConversationMember(aliceId, aliceData_.id, bobUri);
    DRing::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));

    aliceData_.messages.clear();
    DRing::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    DRing::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });

    auto confId = bobData_.messages.rbegin()->at("confId");
    auto destination = fmt::format("swarm:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->at("uri"),
                                   bobData_.messages.rbegin()->at("device"),
                                   bobData_.messages.rbegin()->at("confId"));

    aliceData_.conferenceChanged = false;
    auto bobCall = DRing::placeCallWithMedia(bobId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT";
    });
    aliceData_.conferenceChanged = false;
    DRing::placeCallWithMedia(carlaId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && carlaData_.hostState == "CURRENT";
    });

    CPPUNIT_ASSERT(DRing::getParticipantList(aliceId, confId).size() == 3);

    // hangup 1 participant and rejoin
    aliceData_.messages.clear();
    bobData_.messages.clear();
    aliceData_.conferenceChanged = false;
    Manager::instance().hangupCall(bobId, bobCall);
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "OVER";
    });

    CPPUNIT_ASSERT(DRing::getParticipantList(aliceId, confId).size() == 2);

    aliceData_.conferenceChanged = false;
    DRing::placeCallWithMedia(bobId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT";
    });

    CPPUNIT_ASSERT(DRing::getParticipantList(aliceId, confId).size() == 3);
    CPPUNIT_ASSERT(aliceData_.messages.empty());
    CPPUNIT_ASSERT(bobData_.messages.empty());

    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupCall(aliceId, callId);

    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });
    CPPUNIT_ASSERT(aliceData_.messages[0].find("duration") != aliceData_.messages[0].end());
    CPPUNIT_ASSERT(bobData_.messages[0].find("duration") != bobData_.messages[0].end());
    CPPUNIT_ASSERT(carlaData_.messages[0].find("duration") != carlaData_.messages[0].end());

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);
}

void
ConversationCallTest::testParticipantHangupConfNotRemoved()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    DRing::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return bobData_.requestReceived; }));

    aliceData_.messages.clear();
    DRing::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_);
    });

    auto destination = fmt::format("swarm:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->at("uri"),
                                   bobData_.messages.rbegin()->at("device"),
                                   bobData_.messages.rbegin()->at("confId"));

    aliceData_.conferenceChanged = false;
    auto bobCallId = DRing::placeCallWithMedia(bobId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT";
    });

    // hangup bob MUST NOT stop the conference
    aliceData_.messages.clear();
    bobData_.messages.clear();
    aliceData_.conferenceChanged = false;
    Manager::instance().hangupCall(bobId, bobCallId);

    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return aliceData_.conferenceRemoved; }));
}

void
ConversationCallTest::testJoinFinishedCall()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });
    DRing::addConversationMember(aliceId, aliceData_.id, bobUri);
    DRing::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));
    aliceData_.messages.clear();
    DRing::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    DRing::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));
    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });
    auto confId = bobData_.messages.rbegin()->at("confId");
    auto destination = fmt::format("swarm:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->at("uri"),
                                   bobData_.messages.rbegin()->at("device"),
                                   bobData_.messages.rbegin()->at("confId"));
    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupCall(aliceId, callId);
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    // If bob try to join the call, it will re-host a new conference
    // and commit a new active call.
    auto bobCall = DRing::placeCallWithMedia(bobId, destination, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_) && bobData_.hostState == "CURRENT";
    });
    confId = bobData_.messages.rbegin()->at("confId");
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 1);
    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupConference(aliceId, confId);
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });
    CPPUNIT_ASSERT(aliceData_.messages[0].find("duration") != aliceData_.messages[0].end());
    CPPUNIT_ASSERT(bobData_.messages[0].find("duration") != bobData_.messages[0].end());
    CPPUNIT_ASSERT(carlaData_.messages[0].find("duration") != carlaData_.messages[0].end());
    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);
}

void
ConversationCallTest::testJoinFinishedCallForbidden()
{
    enableCarla();
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    // Do not host conference for others
    DRing::setConversationPreferences(aliceId, aliceData_.id, {{"hostConference", "false"}});

    DRing::addConversationMember(aliceId, aliceData_.id, bobUri);
    DRing::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));

    aliceData_.messages.clear();
    DRing::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    DRing::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });

    auto confId = bobData_.messages.rbegin()->at("confId");
    auto destination = fmt::format("swarm:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->at("uri"),
                                   bobData_.messages.rbegin()->at("device"),
                                   bobData_.messages.rbegin()->at("confId"));

    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupCall(aliceId, callId);

    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });

    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);

    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    // If bob try to join the call, it will re-host a new conference
    // and commit a new active call.
    auto bobCall = DRing::placeCallWithMedia(bobId, destination, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_) && bobData_.hostState == "CURRENT";
    });

    confId = bobData_.messages.rbegin()->at("confId");

    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 1);

    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupConference(aliceId, confId);

    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });
    CPPUNIT_ASSERT(aliceData_.messages[0].find("duration") != aliceData_.messages[0].end());
    CPPUNIT_ASSERT(bobData_.messages[0].find("duration") != bobData_.messages[0].end());
    CPPUNIT_ASSERT(carlaData_.messages[0].find("duration") != carlaData_.messages[0].end());

    // get active calls = 0
    CPPUNIT_ASSERT(DRing::getActiveCalls(aliceId, aliceData_.id).size() == 0);
}

void
ConversationCallTest::testUsePreference()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start conversation
    DRing::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });
    DRing::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData_.requestReceived; }));
    aliceData_.messages.clear();
    DRing::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));

    // Update preferences
    aliceData_.messages.clear();
    bobData_.messages.clear();
    auto lastCommitIsProfile = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/update-profile";
    };
    DRing::updateConversationInfos(aliceId,
                                   aliceData_.id,
                                   std::map<std::string, std::string> {
                                       {"rdvAccount", aliceUri},
                                       {"rdvDevice", aliceDevice},
                                   });
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsProfile(aliceData_) && lastCommitIsProfile(bobData_);
    });

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    auto callId = DRing::placeCallWithMedia(bobId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_);
    });
    auto confId = bobData_.messages.rbegin()->at("confId");

    // Alice should be the host
    CPPUNIT_ASSERT(aliceAccount->getConference(confId));
    Manager::instance().hangupCall(bobId, callId);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationCallTest::name())
