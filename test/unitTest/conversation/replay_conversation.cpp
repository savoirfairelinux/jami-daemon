/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>
#include <msgpack.hpp>

#include "manager.h"
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"

#include <git2.h>
#include <filesystem>

using namespace std::string_literals;
using namespace DRing::Account;

struct ConvInfoTest
{
    std::string id {};
    time_t created {0};
    time_t removed {0};
    time_t erased {0};

    MSGPACK_DEFINE_MAP(id, created, removed, erased)
};

namespace jami {
namespace test {

class ReplayConversationTest : public CppUnit::TestFixture
{
public:
    ~ReplayConversationTest() { DRing::fini(); }
    static std::string name() { return "ReplayConversation"; }
    void setUp();
    void tearDown();

    std::map<std::string, std::string> nameToId {};

private:
    void testReplay();

    CPPUNIT_TEST_SUITE(ReplayConversationTest);
    CPPUNIT_TEST(testReplay);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ReplayConversationTest, ReplayConversationTest::name());

void
ReplayConversationTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    std::set<std::string> names;
    std::ifstream file = fileutils::ifstream("conversation");
    if (!file.is_open()) {
        JAMI_WARN("@@@ Could not load conversation");
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);

        std::string name;
        ss >> name;
        if (names.find(name) == names.end()) {
            names.emplace(name);
            JAMI_WARN("@@@ Detected account %s", name.c_str());
        }
    }

    for (const auto& name : names) {
        std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
        details[ConfProperties::TYPE] = "RING";
        details[ConfProperties::DISPLAYNAME] = name;
        details[ConfProperties::ALIAS] = name;
        details[ConfProperties::UPNP_ENABLED] = "true";
        details[ConfProperties::ARCHIVE_PASSWORD] = "";
        details[ConfProperties::ARCHIVE_PIN] = "";
        details[ConfProperties::ARCHIVE_PATH] = "";
        nameToId[name] = Manager::instance().addAccount(details);
    }

    JAMI_WARN("@@@ Initialize accounts...");
    // TODO only wait registered
    std::this_thread::sleep_for(std::chrono::seconds(30));
}

void
ReplayConversationTest::tearDown()
{
    DRing::unregisterSignalHandlers();
    for (const auto& [name, id] : nameToId)
        Manager::instance().removeAccount(id, true);
    // TODO only wait registered
    std::this_thread::sleep_for(std::chrono::seconds(30));
}

void
ReplayConversationTest::testReplay()
{
    std::map<std::string, std::shared_ptr<JamiAccount>> idToAccount {};
    for (const auto& [name, id] : nameToId)
        idToAccount[id] = Manager::instance().getAccount<JamiAccount>(id);

    auto firstAccount = idToAccount.begin()->second;
    auto convId = firstAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) { cv.notify_one(); }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            conversationReady = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    JAMI_WARN("@@@ Init conv members");
    auto idx = 0;
    for (const auto& [id, account] : idToAccount) {
        idx += 1;
        if (idx == 1)
            continue;
        requestReceived = false;
        CPPUNIT_ASSERT(firstAccount->addConversationMember(convId, account->getUsername()));
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

        conversationReady = false;
        account->acceptConversationRequest(convId);
        CPPUNIT_ASSERT(
            cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationReady; }));
    }

    // Wait that all accounts are ready
    // std::this_thread::sleep_for(std::chrono::seconds(10)); // TODO why not retrieven?&??????
    JAMI_WARN("@@@ Write conv");

    std::ifstream file = fileutils::ifstream("conversation");
    if (!file.is_open()) {
        JAMI_DBG("Could not load conversation");
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);

        std::string name;
        ss >> name;

        std::string body = line.substr(name.size() + 1);

        auto id = nameToId[name];
        auto account = idToAccount[id];
        account->sendMessage(convId, body);
    }

    // Wait for sync // TODO improve
    std::this_thread::sleep_for(std::chrono::seconds(60));
    for (const auto& [name, id] : nameToId) {
        auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + id + DIR_SEPARATOR_STR
                        + "conversations" + DIR_SEPARATOR_STR + convId;
        auto destPath = std::string("/home/amarok/" + convId + "_" + id);
        JAMI_ERR("@@@@ COPY TO %s", destPath.c_str());
        std::rename(repoPath.c_str(), destPath.c_str());
    }

    DRing::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ReplayConversationTest::name())
