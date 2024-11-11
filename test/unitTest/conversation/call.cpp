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
#include "media_const.h"
#include "sip/sipcall.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct ConvData
{
    std::string id {};
    std::string callId {};
    std::string confId {};
    bool requestReceived {false};
    bool needsHost {false};
    bool conferenceChanged {false};
    bool conferenceRemoved {false};
    std::string hostState {};
    std::string state {};
    std::vector<libjami::SwarmMessage> messages {};
};

class ConversationCallTest : public CppUnit::TestFixture
{
public:
    ~ConversationCallTest() { libjami::fini(); }
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
    std::vector<std::map<std::string, std::string>> pInfos_ {};

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
    void testJoinWhileActiveCall();
    void testCallSelfIfDefaultHost();
    void testNeedsHost();
    void testAudioOnly();
    void testJoinAfterMuteHost();
    void testBusy();
    void testDecline();
    void testNoDevice();

    CPPUNIT_TEST_SUITE(ConversationCallTest);
    CPPUNIT_TEST(testActiveCalls);
    CPPUNIT_TEST(testActiveCalls3Peers);
    CPPUNIT_TEST(testRejoinCall);
    CPPUNIT_TEST(testParticipantHangupConfNotRemoved);
    CPPUNIT_TEST(testJoinFinishedCall);
    CPPUNIT_TEST(testJoinFinishedCallForbidden);
    CPPUNIT_TEST(testUsePreference);
    CPPUNIT_TEST(testJoinWhileActiveCall);
    CPPUNIT_TEST(testCallSelfIfDefaultHost);
    CPPUNIT_TEST(testNeedsHost);
    CPPUNIT_TEST(testAudioOnly);
    CPPUNIT_TEST(testJoinAfterMuteHost);
    CPPUNIT_TEST(testBusy);
    CPPUNIT_TEST(testDecline);
    CPPUNIT_TEST(testNoDevice);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationCallTest, ConversationCallTest::name());

void
ConversationCallTest::setUp()
{
    // Init daemon
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

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
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const libjami::SwarmMessage& message) {
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
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::NeedsHost>(
        [&](const std::string& accountId, const std::string& /*conversationId*/) {
            if (accountId == aliceId)
                aliceData_.needsHost = true;
            if (accountId == bobId)
                bobData_.needsHost = true;
            if (accountId == bob2Id)
                bob2Data_.needsHost = true;
            if (accountId == carlaId)
                carlaData_.needsHost = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceChanged>(
        [&](const std::string& accountId, const std::string&, const std::string&) {
            if (accountId == aliceId)
                aliceData_.conferenceChanged = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceCreated>(
        [&](const std::string& accountId, const std::string&, const std::string& confId) {
            if (accountId == aliceId)
                aliceData_.confId = confId;
            else if (accountId == bobId)
                bobData_.confId = confId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId)
                aliceData_.conferenceRemoved = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string& state,
            signed) {
            if (accountId == aliceId) {
                auto details = libjami::getCallDetails(aliceId, callId);
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
            } else if (accountId == bobId) {
                bobData_.callId = callId;
                bobData_.state = state;
            } else if (accountId == carlaId) {
                carlaData_.state = state;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::OnConferenceInfosUpdated>(
        [=](const std::string&,
            const std::vector<std::map<std::string, std::string>> participantsInfos) {
            pInfos_ = participantsInfos;
            cv.notify_one();
        }));

    libjami::registerSignalHandlers(confHandlers);
}

void
ConversationCallTest::enableCarla()
{
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    // Enable carla
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool carlaConnected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto deviceAnnounced
                    = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (deviceAnnounced == "true") {
                    carlaConnected = true;
                    cv.notify_one();
                }
            }));
    libjami::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaConnected; }));
    confHandlers.clear();
    libjami::unregisterSignalHandlers();
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);

    // start call
    aliceData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.conferenceChanged && !aliceData_.messages.empty(); });
    CPPUNIT_ASSERT(aliceData_.messages.rbegin()->type == "application/call-history+json");

    // get active calls = 1
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 1);

    // hangup
    aliceData_.messages.clear();
    Manager::instance().hangupConference(aliceId, aliceData_.confId);

    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData_.messages.empty(); }));
    CPPUNIT_ASSERT(aliceData_.messages.rbegin()->body.find("duration") != aliceData_.messages.rbegin()->body.end());

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));

    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });
    auto confId = bobData_.messages.rbegin()->body.at("confId");
    auto destination = fmt::format("rdv:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->body.at("uri"),
                                   bobData_.messages.rbegin()->body.at("device"),
                                   confId);

    aliceData_.conferenceChanged = false;
    libjami::placeCallWithMedia(bobId, destination, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT";
    }));
    aliceData_.conferenceChanged = false;
    // get 2 other participants
    libjami::placeCallWithMedia(carlaId, destination, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && carlaData_.hostState == "CURRENT" && libjami::getParticipantList(aliceId, confId).size() == 2;
    }));

    // get active calls = 1
    CPPUNIT_ASSERT(libjami::getActiveCalls(bobId, bobData_.id).size() == 1);

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
    CPPUNIT_ASSERT(aliceData_.messages[0].body.find("duration") != aliceData_.messages[0].body.end());
    CPPUNIT_ASSERT(bobData_.messages[0].body.find("duration") != bobData_.messages[0].body.end());
    CPPUNIT_ASSERT(carlaData_.messages[0].body.find("duration") != carlaData_.messages[0].body.end());

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));

    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });

    auto confId = bobData_.messages.rbegin()->body.at("confId");
    auto destination = fmt::format("rdv:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->body.at("uri"),
                                   bobData_.messages.rbegin()->body.at("device"),
                                   confId);

    aliceData_.conferenceChanged = false;
    libjami::placeCallWithMedia(bobId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT"  && bobData_.state == "CURRENT";
    });
    aliceData_.conferenceChanged = false;
    libjami::placeCallWithMedia(carlaId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && carlaData_.hostState == "CURRENT" && carlaData_.state == "CURRENT";
    });

    CPPUNIT_ASSERT(libjami::getParticipantList(aliceId, confId).size() == 2);

    // hangup 1 participant and rejoin
    aliceData_.messages.clear();
    bobData_.messages.clear();
    aliceData_.conferenceChanged = false;
    Manager::instance().hangupCall(bobId, bobData_.callId);
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "OVER";
    });

    CPPUNIT_ASSERT(libjami::getParticipantList(aliceId, confId).size() == 1);

    aliceData_.conferenceChanged = false;
    libjami::placeCallWithMedia(bobId, destination, {});
    cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT";
    });

    CPPUNIT_ASSERT(libjami::getParticipantList(aliceId, confId).size() == 2);
    CPPUNIT_ASSERT(aliceData_.messages.empty());
    CPPUNIT_ASSERT(bobData_.messages.empty());

    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupConference(aliceId, confId);

    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    }));
    CPPUNIT_ASSERT(aliceData_.messages[0].body.find("duration") != aliceData_.messages[0].body.end());
    CPPUNIT_ASSERT(bobData_.messages[0].body.find("duration") != bobData_.messages[0].body.end());
    CPPUNIT_ASSERT(carlaData_.messages[0].body.find("duration") != carlaData_.messages[0].body.end());

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return bobData_.requestReceived; }));

    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_);
    });

    auto destination = fmt::format("rdv:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->body.at("uri"),
                                   bobData_.messages.rbegin()->body.at("device"),
                                   bobData_.messages.rbegin()->body.at("confId"));

    aliceData_.conferenceChanged = false;
    auto bobCallId = libjami::placeCallWithMedia(bobId, destination, {});
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });
    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));
    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });
    auto confId = bobData_.messages.rbegin()->body.at("confId");
    auto destination = fmt::format("rdv:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->body.at("uri"),
                                   bobData_.messages.rbegin()->body.at("device"),
                                   bobData_.messages.rbegin()->body.at("confId"));
    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupConference(aliceId, aliceData_.confId);
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    // If bob try to join the call, it will re-host a new conference
    // and commit a new active call.
    auto bobCall = libjami::placeCallWithMedia(bobId, destination, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_) && bobData_.hostState == "CURRENT";
    });
    confId = bobData_.messages.rbegin()->body.at("confId");
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 1);
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
    CPPUNIT_ASSERT(aliceData_.messages[0].body.find("duration") != aliceData_.messages[0].body.end());
    CPPUNIT_ASSERT(bobData_.messages[0].body.find("duration") != bobData_.messages[0].body.end());
    CPPUNIT_ASSERT(carlaData_.messages[0].body.find("duration") != carlaData_.messages[0].body.end());
    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    // Do not host conference for others
    libjami::setConversationPreferences(aliceId, aliceData_.id, {{"hostConference", "false"}});

    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    libjami::addConversationMember(aliceId, aliceData_.id, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived && carlaData_.requestReceived;
    }));

    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(carlaId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !carlaData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_);
    });

    auto confId = bobData_.messages.rbegin()->body.at("confId");
    auto destination = fmt::format("rdv:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->body.at("uri"),
                                   bobData_.messages.rbegin()->body.at("device"),
                                   bobData_.messages.rbegin()->body.at("confId"));

    // hangup
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    Manager::instance().hangupConference(aliceId, aliceData_.confId);

    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return !aliceData_.messages.empty() && !bobData_.messages.empty()
               && !carlaData_.messages.empty();
    });

    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);

    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    // If bob try to join the call, it will re-host a new conference
    // and commit a new active call.
    auto bobCall = libjami::placeCallWithMedia(bobId, destination, {});
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_)
               && lastCommitIsCall(carlaData_) && bobData_.hostState == "CURRENT";
    });

    confId = bobData_.messages.rbegin()->body.at("confId");

    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 1);

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
    CPPUNIT_ASSERT(aliceData_.messages[0].body.find("duration") != aliceData_.messages[0].body.end());
    CPPUNIT_ASSERT(bobData_.messages[0].body.find("duration") != bobData_.messages[0].body.end());
    CPPUNIT_ASSERT(carlaData_.messages[0].body.find("duration") != carlaData_.messages[0].body.end());

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
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
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });
    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData_.requestReceived; }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));

    // Update preferences
    aliceData_.messages.clear();
    bobData_.messages.clear();
    auto lastCommitIsProfile = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/update-profile";
    };
    libjami::updateConversationInfos(aliceId,
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
    libjami::placeCallWithMedia(bobId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_);
    });
    auto confId = bobData_.messages.rbegin()->body.at("confId");

    // Alice should be the host
    CPPUNIT_ASSERT(aliceAccount->getConference(confId));
    // Bob should be the only participant
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&]() { return pInfos_.size() == 1; }));
    auto uri = string_remove_suffix(pInfos_[0]["uri"], '@');
    CPPUNIT_ASSERT(uri == bobUri);
    Manager::instance().hangupConference(bobId, bobData_.confId);
}

void
ConversationCallTest::testJoinWhileActiveCall()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    // start call
    aliceData_.messages.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return lastCommitIsCall(aliceData_); }));

    auto confId = aliceData_.messages.rbegin()->body.at("confId");
    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return bobData_.requestReceived; }));

    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));

    CPPUNIT_ASSERT(libjami::getActiveCalls(bobId, bobData_.id).size() == 1);

    aliceData_.messages.clear();
    bobData_.messages.clear();
    aliceData_.conferenceChanged = false;
    Manager::instance().hangupConference(aliceId, confId);

    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return aliceData_.conferenceRemoved; }));
}

void
ConversationCallTest::testCallSelfIfDefaultHost()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });
    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData_.requestReceived; }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    // Update preferences
    aliceData_.messages.clear();
    bobData_.messages.clear();
    auto lastCommitIsProfile = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/update-profile";
    };
    libjami::updateConversationInfos(aliceId,
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
    pInfos_.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, {});
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/call-history+json";
    };
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_);
    }));
    auto confId = aliceData_.messages.rbegin()->body.at("confId");
    // Alice should be the host
    CPPUNIT_ASSERT(aliceAccount->getConference(confId));
    Manager::instance().hangupConference(aliceId, confId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return aliceData_.conferenceRemoved; }));

}

void
ConversationCallTest::testNeedsHost()
{
    enableCarla();
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });
    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData_.requestReceived; }));
    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));
    // Update preferences
    aliceData_.messages.clear();
    bobData_.messages.clear();
    auto lastCommitIsProfile = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->body.at("type") == "application/update-profile";
    };
    libjami::updateConversationInfos(aliceId,
                                     aliceData_.id,
                                     std::map<std::string, std::string> {
                                         {"rdvAccount", aliceUri},
                                         {"rdvDevice", aliceDevice},
                                     });
    // should get message
    cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsProfile(aliceData_) && lastCommitIsProfile(bobData_);
    });
    // Disable Host
    Manager::instance().sendRegister(aliceId, false);
    // start call
    libjami::placeCallWithMedia(bobId, "swarm:" + aliceData_.id, {});
    // Can fail after 30 seconds if it triggers a new ICE request (before No response from DHT)
    CPPUNIT_ASSERT(cv.wait_for(lk, 40s, [&]() { return bobData_.needsHost; }));
}

void
ConversationCallTest::testAudioOnly()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    connectSignals();

    // Start conversation
    libjami::startConversation(aliceId);
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); });

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);

    // start call
    aliceData_.messages.clear();
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
            libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
            {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);
    pInfos_.clear();
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, mediaList);
    // should get message
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.messages.empty() && !pInfos_.empty(); });
    CPPUNIT_ASSERT(aliceData_.messages[0].type == "application/call-history+json");
    CPPUNIT_ASSERT(pInfos_.size() == 1);
    CPPUNIT_ASSERT(pInfos_[0]["videoMuted"] == "true");

    // hangup
    aliceData_.messages.clear();
    Manager::instance().hangupConference(aliceId, aliceData_.confId);

    // should get message
    cv.wait_for(lk, 30s, [&]() { return !aliceData_.messages.empty(); });
    CPPUNIT_ASSERT(aliceData_.messages[0].body.find("duration") != aliceData_.messages[0].body.end());

    // get active calls = 0
    CPPUNIT_ASSERT(libjami::getActiveCalls(aliceId, aliceData_.id).size() == 0);
}

void
ConversationCallTest::testJoinAfterMuteHost()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    // Start conversation
    libjami::startConversation(aliceId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData_.id.empty(); }));

    libjami::addConversationMember(aliceId, aliceData_.id, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData_.requestReceived;
    }));

    aliceData_.messages.clear();
    libjami::acceptConversationRequest(bobId, aliceData_.id);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData_.id.empty() && !aliceData_.messages.empty();
    }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
            libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
            {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);
    libjami::placeCallWithMedia(aliceId, "swarm:" + aliceData_.id, mediaList);
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->type == "application/call-history+json";
    };
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return lastCommitIsCall(aliceData_) && lastCommitIsCall(bobData_) && !pInfos_.empty();
    }));
    auto confId = bobData_.messages.rbegin()->body.at("confId");

    // Mute host
    auto conference = aliceAccount->getConference(aliceData_.confId);
    auto proposedList = conference->currentMediaList();
    for (auto& media : proposedList)
        if (media["LABEL"] == "audio_0")
            media["MUTED"] = "true";
    libjami::requestMediaChange(aliceId, confId, proposedList);

    // Bob join, alice must stay muted
    auto destination = fmt::format("rdv:{}/{}/{}/{}",
                                   bobData_.id,
                                   bobData_.messages.rbegin()->body.at("uri"),
                                   bobData_.messages.rbegin()->body.at("device"),
                                   confId);

    aliceData_.conferenceChanged = false;
    pInfos_.clear();
    auto bobCall = libjami::placeCallWithMedia(bobId, destination, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceData_.conferenceChanged && bobData_.hostState == "CURRENT"  && bobData_.state == "CURRENT" && pInfos_.size() == 2;
    }));
    // Audio of the host is still muted
    CPPUNIT_ASSERT(pInfos_[0]["audioLocalMuted"] == "true");

}

void
ConversationCallTest::testBusy()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData_.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData_.id.empty(); }));


    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
            libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
            {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);
    libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->type == "application/call-history+json";
    };
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return lastCommitIsCall(aliceData_);
    }));
    auto reason = aliceData_.messages.rbegin()->body.at("reason");
    CPPUNIT_ASSERT(reason == "busy");
}
void
ConversationCallTest::testDecline()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData_.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData_.id.empty(); }));

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
            libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
            {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);
    libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return !bobData_.callId.empty() && bobData_.state == "INCOMING"; }));

    libjami::refuse(bobId, bobData_.callId);

    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->type == "application/call-history+json";
    };
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return lastCommitIsCall(aliceData_);
    }));
    auto reason = aliceData_.messages.rbegin()->body.at("reason");
    CPPUNIT_ASSERT(reason == "declined");
}

void
ConversationCallTest::testNoDevice()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();

    aliceAccount->addContact("e2eb225c76be68713d4874d290200849436c6355");
    aliceAccount->sendTrustRequest("e2eb225c76be68713d4874d290200849436c6355", {});

    // start call
    aliceData_.messages.clear();
    bobData_.messages.clear();
    carlaData_.messages.clear();
    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute
        = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
            libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
            {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);
    libjami::placeCallWithMedia(aliceId, "e2eb225c76be68713d4874d290200849436c6355", mediaList);

    auto lastCommitIsCall = [&](const auto& data) {
        return !data.messages.empty()
               && data.messages.rbegin()->type == "application/call-history+json";
    };
    // should get message
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return lastCommitIsCall(aliceData_);
    }));
    auto reason = aliceData_.messages.rbegin()->body.at("reason");
    CPPUNIT_ASSERT(reason == "no_device");
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationCallTest::name())
