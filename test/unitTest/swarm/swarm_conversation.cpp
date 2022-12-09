/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *  Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>
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

#include "jami.h"
#include "../common.h"
#include "jamidht/swarm/swarm_manager.h"
#include "connectivity/multiplexed_socket.h"

#include "connectivity/peer_connection.h"
#include <opendht/thread_pool.h>

#include "../../test_runner.h"
#include "account_const.h"
#include "conversation/conversationcommon.h"
#include "manager.h"

using namespace dht;
using NodeId = dht::PkId;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct ConvData
{
    std::string id {};
    bool requestReceived {false};
    bool needsHost {false};
    bool conferenceChanged {false};
    bool conferenceRemoved {false};
    std::string hostState {};
    std::vector<std::map<std::string, std::string>> messages {};
};

class SwarmConversationTest : public CppUnit::TestFixture
{
public:
    SwarmConversationTest();
    ~SwarmConversationTest();
    static std::string name() { return "SwarmConversationTest"; }

    std::map<std::string, ConvData> accountMap;
    std::vector<std::string> accountIds;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

private:
    void connectSignals();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    void testSendMessage();

    CPPUNIT_TEST_SUITE(SwarmConversationTest);

    CPPUNIT_TEST(testSendMessage);

    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SwarmConversationTest, SwarmConversationTest::name());

SwarmConversationTest::SwarmConversationTest()
{
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    auto actors = load_actors("actors/account_list.yml");

    for (const auto& act : actors) {
        auto id = act.second;
        accountIds.emplace_back(id);
        std::cout << act.second << std::endl;
        accountMap.insert({id, {}});
    }

    wait_for_announcement_of(accountIds, 60s);
}

SwarmConversationTest::~SwarmConversationTest()
{
    wait_for_removal_of(accountIds);
    libjami::fini();
}

void
SwarmConversationTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            for (const auto& accId : accountIds) {
                if (accountId == accId) {
                    accountMap[accId].id = conversationId;
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            std::map<std::string, std::string> message) {
            for (const auto& accId : accountIds) {
                if (accountId == accId && accountMap[accId].id == conversationId) {
                    accountMap[accId].messages.emplace_back(message);
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string&,
                std::map<std::string, std::string>) {
                for (const auto& accId : accountIds) {
                    if (accountId == accId) {
                        accountMap[accId].requestReceived = true;
                    }
                }
                cv.notify_one();
            }));

    libjami::registerSignalHandlers(confHandlers);
}

void
SwarmConversationTest::testSendMessage()
{
    /*     std::map<std::string, std::shared_ptr<JamiAccount>> jamiAccounts;

        for (const auto& accId : accountIds) {
            jamiAccounts.insert({accId, Manager::instance().getAccount<JamiAccount>(accId)});
            std::cout << "created account for: " << accId << std::endl;
        }

        std::mutex mtx;
        std::unique_lock<std::mutex> lk {mtx};
        std::condition_variable cv;

        connectSignals();

        auto aliceId = jamiAccounts.begin()->first;
        auto convId = libjami::startConversation(aliceId); */

    /*     for (const auto jAcc : jamiAccounts) {
            auto userUri = jAcc.second->getUsername();
            libjami::addConversationMember(aliceId, convId, userUri);
        } */

    /*     CPPUNIT_ASSERT(
            cv.wait_for(lk, 30s, [&]() { return accountMap[accountIds.at(1)].requestReceived; }));

        for (auto it = std::next(accountIds.begin()); it != accountIds.end(); ++it) {
            libjami::acceptConversationRequest(*it, convId);
        } */

    /*     CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
            return accountMap[accountIds.at(1)].id == convId;
        }));

     libjami::sendMessage(aliceId, convId, "hi"s, "");
        CPPUNIT_ASSERT(true); */

    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::SwarmConversationTest::name())
