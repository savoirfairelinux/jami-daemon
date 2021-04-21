/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

namespace jami {
namespace test {

class CompabilityTest : public CppUnit::TestFixture
{
public:
    ~CompabilityTest() { DRing::fini(); }
    static std::string name() { return "Compability"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testIsComposing();
    void testRemoveConvOnOldTrustRequestConfirm();
    void testRemoveConvOnDhtTextMessage();
    void testSendFileCompatibility();

    CPPUNIT_TEST_SUITE(CompabilityTest);
    CPPUNIT_TEST(testIsComposing);
    CPPUNIT_TEST(testRemoveConvOnOldTrustRequestConfirm);
    CPPUNIT_TEST(testRemoveConvOnDhtTextMessage);
    CPPUNIT_TEST(testSendFileCompatibility);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CompabilityTest, CompabilityTest::name());

void
CompabilityTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                bool ready = false;
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready = (daemonStatus == "REGISTERED");
                details = bobAccount->getVolatileAccountDetails();
                daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready &= (daemonStatus == "REGISTERED");
            }));
    DRing::registerSignalHandlers(confHandlers);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
}

void
CompabilityTest::tearDown()
{
    DRing::unregisterSignalHandlers();
    auto currentAccSize = Manager::instance().getAccountList().size();
    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);

    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    // Because cppunit is not linked with dbus, just poll if removed
    for (int i = 0; i < 40; ++i) {
        if (Manager::instance().getAccountList().size() <= currentAccSize - 2)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void
CompabilityTest::testIsComposing()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationRemoved = false, requestReceived = false, aliceComposing = false;
    std::string convId = "";
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& /*payload*/,
            time_t /*received*/) {
            if (account_id == bobId)
                requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId) {
                conversationRemoved = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::ComposingStatusChanged>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& peer,
                bool state) {
                if (accountId == bobId && conversationId == "" && peer == aliceUri) {
                    aliceComposing = state;
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return !convId.empty() && requestReceived;
    }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri, false));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationRemoved; }));

    // Send iscomposing to non swarm compatible contact

    aliceAccount->setIsComposing("jami:" + bobUri, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return aliceComposing; }));

    aliceAccount->setIsComposing("jami:" + bobUri, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !aliceComposing; }));
    DRing::unregisterSignalHandlers();
}

void
CompabilityTest::testRemoveConvOnOldTrustRequestConfirm()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationRemoved = false, requestReceived = false;
    std::string convId = "";
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& /*payload*/,
            time_t /*received*/) {
            if (account_id == bobId)
                requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId) {
                conversationRemoved = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&]() { return !convId.empty(); }));
    ConversationRepository repo(aliceAccount, convId);
    // Mode must be one to one
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::ONE_TO_ONE);
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri, false));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationRemoved; }));
}

void
CompabilityTest::testRemoveConvOnDhtTextMessage()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool conversationRemoved = false, requestReceived = false;
    std::string convId = "";
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& /*payload*/,
            time_t /*received*/) {
            if (account_id == bobId)
                requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId) {
                conversationRemoved = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&]() { return !convId.empty(); }));
    ConversationRepository repo(aliceAccount, convId);
    // Mode must be one to one
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::ONE_TO_ONE);
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));
    bobAccount->sendTextMessage(aliceUri, {{"text/plain", "Hi!"}});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return conversationRemoved; }));
}

void
CompabilityTest::testSendFileCompatibility()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool successfullyReceive = false, requestReceived = false;
    std::string convId = "";
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& /*payload*/,
            time_t /*received*/) {
            if (account_id == bobId)
                requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                convId = conversationId;
            }
            cv.notify_one();
        }));
    bobAccount->connectionManager().onChannelRequest([&](const DeviceId&, const std::string& name) {
        successfullyReceive = name.find("file://") == 0;
        cv.notify_one();
        return true;
    });
    DRing::registerSignalHandlers(confHandlers);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(5), [&]() { return !convId.empty(); }));
    ConversationRepository repo(aliceAccount, convId);
    // Mode must be one to one
    CPPUNIT_ASSERT(repo.mode() == ConversationMode::ONE_TO_ONE);
    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + aliceAccount->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + convId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    // Now, sending a file will trigger file://id instead of data-transfer://conv for compat

    // Send file
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    std::string unused;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;

    CPPUNIT_ASSERT(DRing::sendFile(info, unused) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30), [&]() { return successfullyReceive; });
    std::remove("SEND");
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::CompabilityTest::name())
