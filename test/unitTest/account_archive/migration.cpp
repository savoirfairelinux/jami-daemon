/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
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
#include <filesystem>
#include <fstream>
#include <streambuf>
#include <fmt/format.h>

#include "manager.h"
#include "jamidht/accountarchive.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "account_const.h"
#include "common.h"
#include "fileutils.h"

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class MigrationTest : public CppUnit::TestFixture
{
public:
    MigrationTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~MigrationTest() { libjami::fini(); }
    static std::string name() { return "AccountArchive"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;

private:
    void testLoadExpiredAccount();
    void testMigrationAfterRevokation();
    void testExpiredDeviceInSwarm();

    CPPUNIT_TEST_SUITE(MigrationTest);
    CPPUNIT_TEST(testLoadExpiredAccount);
    CPPUNIT_TEST(testMigrationAfterRevokation);
    CPPUNIT_TEST(testExpiredDeviceInSwarm);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MigrationTest, MigrationTest::name());

void
MigrationTest::setUp()
{
    auto actors = load_actors("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
MigrationTest::tearDown()
{
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    if (!bob2Id.empty())
        wait_for_removal_of({aliceId, bob2Id, bobId});
    else
        wait_for_removal_of({aliceId, bobId});
}

void
MigrationTest::testLoadExpiredAccount()
{
    wait_for_announcement_of({aliceId, bobId});
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());

    // Get alice's expiration
    auto archivePath = fmt::format("{}/{}/archive.gz",
                                   fileutils::get_data_dir(),
                                   aliceAccount->getAccountID());
    auto devicePath = fmt::format("{}/{}/ring_device.crt",
                                  fileutils::get_data_dir(),
                                  aliceAccount->getAccountID());
    auto archive = AccountArchive(archivePath, "");
    auto deviceCert = dht::crypto::Certificate(fileutils::loadFile(devicePath));
    auto deviceExpiration = deviceCert.getExpiration();
    auto accountExpiration = archive.id.second->getExpiration();

    // Update validity
    CPPUNIT_ASSERT(aliceAccount->setValidity("", {}, 9));
    archive = AccountArchive(archivePath, "");
    deviceCert = dht::crypto::Certificate(fileutils::loadFile(devicePath));
    auto newDeviceExpiration = deviceCert.getExpiration();
    auto newAccountExpiration = archive.id.second->getExpiration();

    // Check expiration is changed
    CPPUNIT_ASSERT(newDeviceExpiration < deviceExpiration
                   && newAccountExpiration < accountExpiration);

    // Sleep and wait that certificate is expired
    std::this_thread::sleep_for(10s);

    // reload account, check migration signals
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto aliceMigrated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::MigrationEnded>(
        [&](const std::string& accountId, const std::string& state) {
            if (accountId == aliceId && state == "SUCCESS") {
                aliceMigrated = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Check migration is triggered and expiration updated
    aliceAccount->forceReloadAccount();
    CPPUNIT_ASSERT(cv.wait_for(lk, 15s, [&]() { return aliceMigrated; }));

    archive = AccountArchive(archivePath, "");
    deviceCert = dht::crypto::Certificate(fileutils::loadFile(devicePath));
    deviceExpiration = deviceCert.getExpiration();
    accountExpiration = archive.id.second->getExpiration();
    CPPUNIT_ASSERT(newDeviceExpiration < deviceExpiration
                   && newAccountExpiration < accountExpiration);
    CPPUNIT_ASSERT(aliceUri == aliceAccount->getUsername());
    CPPUNIT_ASSERT(aliceDevice == aliceAccount->currentDeviceId());
}

void
MigrationTest::testMigrationAfterRevokation()
{
    wait_for_announcement_of({aliceId, bobId});
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Generate bob2
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    // Add second device for Bob
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
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

    auto deviceRevoked = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceRevocationEnded>(
            [&](const std::string& accountId, const std::string&, int status) {
                if (accountId == bobId && status == 0)
                    deviceRevoked = true;
                cv.notify_one();
            }));
    auto bobMigrated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::MigrationEnded>(
        [&](const std::string& accountId, const std::string& state) {
            if (accountId == bob2Id && state == "SUCCESS") {
                bobMigrated = true;
            }
            cv.notify_one();
        }));
    auto knownChanged = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::KnownDevicesChanged>(
        [&](const std::string& accountId, auto devices) {
            if (accountId == bobId && devices.size() == 2)
                knownChanged = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    bob2Id = Manager::instance().addAccount(details);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return knownChanged; }));

    // Revoke bob2
    auto bob2Device = std::string(bob2Account->currentDeviceId());
    bobAccount->revokeDevice("", bob2Device);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return deviceRevoked; }));
    // Note: bob2 will need some seconds to get the revokation list
    std::this_thread::sleep_for(10s);

    // Check migration is triggered and expiration updated
    bob2Account->forceReloadAccount();
    CPPUNIT_ASSERT(cv.wait_for(lk, 15s, [&]() { return bobMigrated; }));
    // Because the device was revoked, a new ID must be generated there
    CPPUNIT_ASSERT(bob2Device != bob2Account->currentDeviceId());
}

void
MigrationTest::testExpiredDeviceInSwarm()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    auto messageBobReceived = 0, messageAliceReceived = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            } else {
                messageAliceReceived += 1;
            }
            cv.notify_one();
        }));
    bool requestReceived = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    bool conversationReady = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    auto aliceMigrated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::MigrationEnded>(
        [&](const std::string& accountId, const std::string& state) {
            if (accountId == aliceId && state == "SUCCESS") {
                aliceMigrated = true;
            }
            cv.notify_one();
        }));
    bool aliceStopped = false, aliceAnnounced = false, aliceRegistered = false, aliceRegistering = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "UNREGISTERED")
                    aliceStopped = true;
                else if (daemonStatus == "REGISTERED")
                    aliceRegistered = true;
                else if (daemonStatus == "TRYING")
                    aliceRegistering = true;
                auto announced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                if (announced == "true")
                    aliceAnnounced = true;
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // NOTE: We must update certificate before announcing, else, there will be several
    // certificates on the DHT

    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return aliceRegistering; }));
    auto aliceDevice = std::string(aliceAccount->currentDeviceId());
    CPPUNIT_ASSERT(aliceAccount->setValidity("", {}, 90));
    auto now = std::chrono::system_clock::now();
    aliceRegistered = false;
    aliceAccount->forceReloadAccount();
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return aliceRegistered; }));
    CPPUNIT_ASSERT(aliceAccount->currentDeviceId() == aliceDevice);

    aliceStopped = false;
    Manager::instance().sendRegister(aliceId, false);
    CPPUNIT_ASSERT(cv.wait_for(lk, 15s, [&]() { return aliceStopped; }));
    aliceAnnounced = false;
    Manager::instance().sendRegister(aliceId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return aliceAnnounced; }));

    CPPUNIT_ASSERT(aliceAccount->currentDeviceId() == aliceDevice);

    // Create conversation
    auto convId = libjami::startConversation(aliceId);

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return requestReceived; }));

    messageAliceReceived = 0;
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return conversationReady; }));

    // Assert that repository exists
    auto repoPath = fileutils::get_data_dir() / bobAccount->getAccountID()
                    / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    // Wait that alice sees Bob
    cv.wait_for(lk, 20s, [&]() { return messageAliceReceived == 1; });

    messageBobReceived = 0;
    libjami::sendMessage(aliceId, convId, "hi"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return messageBobReceived == 1; }));

    // Wait for certificate to expire
    std::this_thread::sleep_until(now + 100s);
    // Check migration is triggered and expiration updated
    aliceAnnounced = false;
    aliceAccount->forceReloadAccount();
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceAnnounced; }));
    CPPUNIT_ASSERT(aliceAccount->currentDeviceId() == aliceDevice);

    // check that certificate in conversation is expired
    auto devicePath = fmt::format("{}/devices/{}.crt", repoPath, aliceAccount->currentDeviceId());
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(devicePath));
    auto cert = dht::crypto::Certificate(fileutils::loadFile(devicePath));
    now = std::chrono::system_clock::now();
    CPPUNIT_ASSERT(cert.getExpiration() < now);

    // Resend a new message
    messageBobReceived = 0;
    libjami::sendMessage(aliceId, convId, "hi again"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return messageBobReceived == 1; }));
    messageAliceReceived = 0;
    libjami::sendMessage(bobId, convId, "hi!"s, "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return messageAliceReceived == 1; }));

    // check that certificate in conversation is updated
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(devicePath));
    cert = dht::crypto::Certificate(fileutils::loadFile(devicePath));
    now = std::chrono::system_clock::now();
    CPPUNIT_ASSERT(cert.getExpiration() > now);

    // Check same device as begining
    CPPUNIT_ASSERT(aliceAccount->currentDeviceId() == aliceDevice);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MigrationTest::name())
