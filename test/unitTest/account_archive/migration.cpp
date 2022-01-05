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
using namespace DRing::Account;

namespace jami {
namespace test {

class MigrationTest : public CppUnit::TestFixture
{
public:
    MigrationTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~MigrationTest() { DRing::fini(); }
    static std::string name() { return "AccountArchive"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;

private:
    void testLoadExpiredAccount();
    void testMigrationAfterRevokation();

    CPPUNIT_TEST_SUITE(MigrationTest);
    CPPUNIT_TEST(testLoadExpiredAccount);
    CPPUNIT_TEST(testMigrationAfterRevokation);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MigrationTest, MigrationTest::name());

void
MigrationTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
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
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto aliceMigrated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::MigrationEnded>(
        [&](const std::string& accountId, const std::string& state) {
            if (accountId == aliceId && state == "SUCCESS") {
                aliceMigrated = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

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
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Generate bob2
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    // Add second device for Bob
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;

    auto deviceRevoked = false;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::DeviceRevocationEnded>(
            [&](const std::string& accountId, const std::string&, int status) {
                if (accountId == bobId && status == 0)
                    deviceRevoked = true;
                cv.notify_one();
            }));
    auto bobMigrated = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::MigrationEnded>(
        [&](const std::string& accountId, const std::string& state) {
            if (accountId == bob2Id && state == "SUCCESS") {
                bobMigrated = true;
            }
            cv.notify_one();
        }));
    auto knownChanged = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::KnownDevicesChanged>(
        [&](const std::string& accountId, auto devices) {
            if (accountId == bobId && devices.size() == 2)
                knownChanged = true;
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    bob2Id = Manager::instance().addAccount(details);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return knownChanged; }));

    // Revoke bob2
    auto bob2Device = std::string(bob2Account->currentDeviceId());
    bobAccount->revokeDevice("", bob2Device);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() { return deviceRevoked; }));

    // Check migration is triggered and expiration updated
    bob2Account->forceReloadAccount();
    CPPUNIT_ASSERT(cv.wait_for(lk, 15s, [&]() { return bobMigrated; }));
    // Because the device was revoked, a new ID must be generated there
    CPPUNIT_ASSERT(bob2Device != bob2Account->currentDeviceId());
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::MigrationTest::name())
