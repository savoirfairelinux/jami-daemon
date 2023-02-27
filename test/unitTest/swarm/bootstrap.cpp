/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
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
#include <msgpack.hpp>
#include <filesystem>

#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "conversation_interface.h"
#include "fileutils.h"
#include "jami.h"
#include "jamidht/conversation.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/swarm/swarm_channel_handler.h"
#include "manager.h"

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {

struct ConvInfoTest
{
    std::string accountId;
    std::string convId;
    bool requestReceived = false;
    bool conversationReady = false;
    std::vector<std::map<std::string, std::string>> messages;
    Conversation::BootstrapStatus bootstrap {Conversation::BootstrapStatus::FAILED};
};

namespace test {

class BootstrapTest : public CppUnit::TestFixture
{
public:
    ~BootstrapTest() { libjami::fini(); }
    static std::string name() { return "Bootstrap"; }
    void setUp();
    void tearDown();

    ConvInfoTest aliceData;
    ConvInfoTest bobData;
    ConvInfoTest bob2Data;
    ConvInfoTest carlaData;

    std::mutex mtx;
    std::condition_variable cv;

private:
    void connectSignals();

    void testBootstrapOk();
    void testBootstrapFailed();
    void testBootstrapNeverNewDevice();
    void testBootstrapCompat();

    CPPUNIT_TEST_SUITE(BootstrapTest);
    CPPUNIT_TEST(testBootstrapOk);
    CPPUNIT_TEST(testBootstrapFailed);
    CPPUNIT_TEST(testBootstrapNeverNewDevice);
    CPPUNIT_TEST(testBootstrapCompat);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(BootstrapTest, BootstrapTest::name());

void
BootstrapTest::setUp()
{
    aliceData = {};
    bobData = {};
    bob2Data = {};
    carlaData = {};

    // Init daemon
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    auto actors = load_actors("actors/alice-bob-carla.yml");
    aliceData.accountId = actors["alice"];
    bobData.accountId = actors["bob"];
    carlaData.accountId = actors["carla"];

    Manager::instance().sendRegister(carlaData.accountId, false);
    wait_for_announcement_of({aliceData.accountId, bobData.accountId});
    connectSignals();
}

void
BootstrapTest::tearDown()
{
    libjami::unregisterSignalHandlers();
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    if (bob2Data.accountId.empty()) {
        wait_for_removal_of({aliceData.accountId, bobData.accountId, carlaData.accountId});
    } else {
        wait_for_removal_of(
            {aliceData.accountId, bobData.accountId, carlaData.accountId, bob2Data.accountId});
    }
}

void
BootstrapTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& convId) {
            if (accountId == aliceData.accountId) {
                aliceData.convId = convId;
                aliceData.conversationReady = true;
            } else if (accountId == bobData.accountId) {
                bobData.convId = convId;
                bobData.conversationReady = true;
            } else if (accountId == bob2Data.accountId) {
                bob2Data.convId = convId;
                bob2Data.conversationReady = true;
            } else if (accountId == carlaData.accountId) {
                carlaData.convId = convId;
                carlaData.conversationReady = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == aliceData.accountId) {
                    aliceData.requestReceived = true;
                } else if (accountId == bobData.accountId) {
                    bobData.requestReceived = true;
                } else if (accountId == bob2Data.accountId) {
                    bob2Data.requestReceived = true;
                } else if (accountId == carlaData.accountId) {
                    carlaData.requestReceived = true;
                }
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /*conversationId*/,
            std::map<std::string, std::string> message) {
            if (accountId == aliceData.accountId) {
                aliceData.messages.emplace_back(message);
            } else if (accountId == bobData.accountId) {
                bobData.messages.emplace_back(message);
            } else if (accountId == bob2Data.accountId) {
                bob2Data.messages.emplace_back(message);
            } else if (accountId == carlaData.accountId) {
                carlaData.messages.emplace_back(message);
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Link callback for convModule()
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData.accountId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData.accountId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaData.accountId);

    aliceAccount->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            aliceData.bootstrap = status;
            cv.notify_one();
        });
    bobAccount->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            bobData.bootstrap = status;
            cv.notify_one();
        });
    carlaAccount->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            carlaData.bootstrap = status;
            cv.notify_one();
        });
}

void
BootstrapTest::testBootstrapOk()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData.accountId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData.accountId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            aliceData.bootstrap = status;
            cv.notify_one();
        });

    std::unique_lock<std::mutex> lk {mtx};
    auto convId = libjami::startConversation(aliceData.accountId);

    libjami::addConversationMember(aliceData.accountId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobData.accountId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.conversationReady && aliceData.messages.size() == aliceMsgSize + 1
               && aliceData.bootstrap == Conversation::BootstrapStatus::SUCCESS
               && bobData.bootstrap == Conversation::BootstrapStatus::SUCCESS;
    }));
}

void
BootstrapTest::testBootstrapFailed()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData.accountId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData.accountId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            aliceData.bootstrap = status;
            cv.notify_one();
        });

    std::unique_lock<std::mutex> lk {mtx};
    auto convId = libjami::startConversation(aliceData.accountId);

    libjami::addConversationMember(aliceData.accountId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobData.accountId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.conversationReady && aliceData.messages.size() == aliceMsgSize + 1
               && aliceData.bootstrap == Conversation::BootstrapStatus::SUCCESS
               && bobData.bootstrap == Conversation::BootstrapStatus::SUCCESS;
    }));

    // Now bob goes offline, it should disconnect alice
    Manager::instance().sendRegister(bobData.accountId, false);
    // Alice will try to maintain before failing (so will take 30secs to fail)
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return aliceData.bootstrap == Conversation::BootstrapStatus::FAILED;
    }));
}

void
BootstrapTest::testBootstrapNeverNewDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData.accountId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData.accountId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            aliceData.bootstrap = status;
            cv.notify_one();
        });

    std::unique_lock<std::mutex> lk {mtx};
    auto convId = libjami::startConversation(aliceData.accountId);

    libjami::addConversationMember(aliceData.accountId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobData.accountId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.conversationReady && aliceData.messages.size() == aliceMsgSize + 1
               && aliceData.bootstrap == Conversation::BootstrapStatus::SUCCESS
               && bobData.bootstrap == Conversation::BootstrapStatus::SUCCESS;
    }));

    // Alice offline
    Manager::instance().sendRegister(aliceData.accountId, false);
    // Bob will try to maintain before failing (so will take 30secs to fail)
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bobData.bootstrap == Conversation::BootstrapStatus::FAILED;
    }));

    // Create bob2
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
    bob2Data.accountId = Manager::instance().addAccount(details);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Data.accountId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool bob2Connected = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = bob2Account->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus != "UNREGISTERED")
                    bob2Connected = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bob2Connected; }));
    bob2Account->convModule()->onBootstrapStatus(
        [&](std::string /*convId*/, Conversation::BootstrapStatus status) {
            bob2Data.bootstrap = status;
            cv.notify_one();
        });

    // Disconnect bob2, to create a valid conv betwen Alice and Bob1
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.bootstrap == Conversation::BootstrapStatus::SUCCESS
               && bob2Data.bootstrap == Conversation::BootstrapStatus::SUCCESS;
    }));

    // Bob offline
    Manager::instance().sendRegister(bobData.accountId, false);
    // Bob2 will try to maintain before failing (so will take 30secs to fail)
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return bob2Data.bootstrap == Conversation::BootstrapStatus::FAILED;
    }));

    // Alice bootstrap should go to fallback (because bob2 never wrote into the conversation) & Connected
    Manager::instance().sendRegister(aliceData.accountId, true);
    // Wait for announcement, ICE fallback + delay
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() {
        return aliceData.bootstrap == Conversation::BootstrapStatus::SUCCESS;
    }));
}

void
BootstrapTest::testBootstrapCompat()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceData.accountId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobData.accountId);
    auto bobUri = bobAccount->getUsername();

    dynamic_cast<SwarmChannelHandler*>(aliceAccount->channelHandlers()[Uri::Scheme::SWARM].get())
        ->disableSwarmManager
        = true;

    std::unique_lock<std::mutex> lk {mtx};
    auto convId = libjami::startConversation(aliceData.accountId);

    libjami::addConversationMember(aliceData.accountId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobData.accountId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return bobData.conversationReady && aliceData.messages.size() == aliceMsgSize + 1;
    }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(aliceData.accountId, convId, "hi"s, "");
    cv.wait_for(lk, 30s, [&]() {
        return bobData.messages.size() == bobMsgSize + 1
               && bobData.bootstrap == Conversation::BootstrapStatus::FAILED;
    });
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::BootstrapTest::name())
