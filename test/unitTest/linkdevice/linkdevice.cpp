/*
 *  Copyright (C) 2021-2026 Savoir-faire Linux Inc.
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

#include "manager.h"
#include "jamidht/archive_account_manager.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>
#include <dhtnet/multiplexed_socket.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <filesystem>
#include <condition_variable>

#include <git2.h>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class LinkDeviceTest : public CppUnit::TestFixture
{
public:
    LinkDeviceTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not jami::Manager::initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~LinkDeviceTest() { libjami::fini(); }
    static std::string name() { return "LinkDevice"; }
    void setUp();
    void tearDown();

    std::string oldDeviceId;
    std::string newDeviceId;

private:
    const std::string JAMI_ID = "JAMI_ID";
    void testQrConnection();
    void testExportNoPassword();
    void testExportWithCorrectPassword();
    void testExportWithWrongPasswordOnce();
    void testExportWithWrongPasswordMaxAttempts();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtxOld;
    std::condition_variable cvOld;
    std::mutex mtxNew;
    std::condition_variable cvNew;

    std::string qrInfo;
    uint32_t operationId = 0;
    bool oldDeviceUpdated = false;
    bool passwordWasIncorrect = false;

    bool authCompletedOnNewDevice = false;
    bool authStartedOnOldDevice = false;
    bool authStartedOnNewDevice = false;

    bool linkDeviceConnecting = false;
    bool linkDeviceSuccessful = false;
    bool linkDeviceUnsuccessful = false;

    void setupSignals();

    // TODO void testMultipleConnections(); // because should not be trying to send the password
    // data to the wrong connection (test leaking password, archive)

    CPPUNIT_TEST_SUITE(LinkDeviceTest);
    CPPUNIT_TEST(testQrConnection);
    CPPUNIT_TEST(testExportNoPassword);
    CPPUNIT_TEST(testExportWithCorrectPassword);
    CPPUNIT_TEST(testExportWithWrongPasswordOnce);
    CPPUNIT_TEST(testExportWithWrongPasswordMaxAttempts);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(LinkDeviceTest, LinkDeviceTest::name());

void
LinkDeviceTest::setUp()
{
    oldDeviceId = "";
    newDeviceId = "";

    auto detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    wait_for_announcement_of(oldDeviceId);

    qrInfo = "";
    operationId = 0;

    oldDeviceUpdated = false;
    passwordWasIncorrect = false;
    authCompletedOnNewDevice = false;
    authStartedOnOldDevice = false;
    authStartedOnNewDevice = false;
    linkDeviceSuccessful = false;
    linkDeviceUnsuccessful = false;
    linkDeviceConnecting = false;

    setupSignals();
}

void
LinkDeviceTest::tearDown()
{
    if (!newDeviceId.empty()) {
        wait_for_removal_of(newDeviceId);
    }
    wait_for_removal_of(oldDeviceId);

    confHandlers.clear();
}

void
LinkDeviceTest::setupSignals()
{
    libjami::unregisterSignalHandlers();
    confHandlers.clear();

    // Simulate user input on new device
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
        [&](const std::string& accountId, uint8_t status, const std::map<std::string, std::string>& details) {
            std::lock_guard lock(mtxNew);
            // Auth status
            if (accountId != newDeviceId) {
                return;
            }
            // Token available status
            if (status == static_cast<uint8_t>(DeviceAuthState::TOKEN_AVAILABLE)) {
                qrInfo = details.at("token");
                CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
            }

            if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                if (authStartedOnNewDevice) {
                    passwordWasIncorrect = true;
                }
                authStartedOnNewDevice = true;
            }

            // Account Auth provided
            if (status == static_cast<uint8_t>(DeviceAuthState::DONE)) {
                authCompletedOnNewDevice = true;
            }
            cvNew.notify_one();
        }));
    // Simulate add device on oldDevice
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
        [&](const std::string& accountId,
            uint32_t opId,
            uint8_t status,
            const std::map<std::string, std::string>& details) {
            std::lock_guard lock(mtxOld);

            if (accountId != oldDeviceId)
                return;
            if (operationId > 0 && opId != operationId)
                return;

            // Connecting status
            if (status == static_cast<uint8_t>(DeviceAuthState::CONNECTING)) {
                linkDeviceConnecting = true;
            }

            // Auth status
            if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                authStartedOnOldDevice = true;
            }
            // Done status
            if (status == static_cast<uint8_t>(DeviceAuthState::DONE)) {
                auto errorIt = details.find("error");
                if (errorIt != details.end() && (!errorIt->second.empty() && errorIt->second != "none")) {
                    if (errorIt->second == "auth_error") {
                        passwordWasIncorrect = true;
                        linkDeviceUnsuccessful = true;
                    } else if (errorIt->second == "none") {
                        linkDeviceUnsuccessful = false;
                    }
                } else {
                    linkDeviceSuccessful = true;
                }
            }
            cvOld.notify_one();
        }));
    // Confirm details were updated
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::AccountDetailsChanged>(
        [&](const std::string& accountId, const std::map<std::string, std::string>& /*details*/) {
            std::lock_guard lock(mtxOld);

            if (accountId != oldDeviceId)
                return;

            oldDeviceUpdated = true;
            cvOld.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

// Tests the QR code generation that relies on Dht to generate an auth uri
void
LinkDeviceTest::testQrConnection()
{
    const std::string testTag = "testQrConnection";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testQrConnection\n", testTag);

    // Create new device
    JAMI_DEBUG("[ut_linkdevice:{}] [NewDevice] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    // Initiate device linking
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
               testTag,
               oldDeviceId,
               qrInfo);
    libjami::addDevice(oldDeviceId, qrInfo);

    // Wait for connecting status
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 60s, [&] { return linkDeviceConnecting; }));
    } // Unlock mtxOld

    // Cleanup
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testQrConnection\n", testTag);
}

void
LinkDeviceTest::testExportNoPassword()
{
    const std::string testTag = "testExportNoPassword";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportNoPassword\n", testTag);

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    // TODO Sleep to compensate for the absence of a callback in the DHT connectionmanager to indicate that the
    // temporary account is ready to receive connections and a matching signal in the archive_account_manager
    std::this_thread::sleep_for(20s);

    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
               testTag,
               oldDeviceId,
               qrInfo);

    // Initiate device linking
    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 60s, [&] { return authStartedOnOldDevice; }));
    } // Unlock mtxOld

    // Confirm linkDevice on old device
    libjami::confirmAddDevice(oldDeviceId, operationId);

    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return authStartedOnNewDevice; }));
    } // Unlock mtxNew

    // Provide account authentication on new device
    libjami::provideAccountAuthentication(newDeviceId, "", "password");

    // Wait for linkDevice to complete
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 60s, [&] { return linkDeviceSuccessful; }));
    } // Unlock mtxOld

    // Wait for new device successful authentication
    {
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 60s, [&] { return authCompletedOnNewDevice; }));
    }

    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportNoPassword\n", testTag);
}

void
LinkDeviceTest::testExportWithCorrectPassword()
{
    const std::string testTag = "testExportWithCorrectPassword";
    const std::string correctPassword = "c2cvc3jmbafsw";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithCorrectPassword\n", testTag);

    // Create old device with password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account with password.", testTag);
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);
    oldAcc->changeArchivePassword("", correctPassword);

    // Simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}", testTag, oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return oldDeviceUpdated; }));
    } // Unlock mtxOld

    // Validate password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Validating password on old device.", testTag);
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
               testTag,
               oldDeviceId,
               qrInfo);

    // TODO Sleep to compensate for the absence of a callback in the DHT connectionmanager to indicate that the
    // temporary account is ready to receive connections and a matching signal in the archive_account_manager
    std::this_thread::sleep_for(20s);

    // Initiate device linking
    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);

    // Wait for auth to start on old device
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return authStartedOnOldDevice; }));
    } // Unlock mtxOld

    // Confirm add device on old device
    libjami::confirmAddDevice(oldDeviceId, operationId);

    // Wait for auth to start on new device
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return authStartedOnNewDevice; }));
    } // Unlock mtxNew

    // Provide password authentication on new device
    libjami::provideAccountAuthentication(newDeviceId, correctPassword, "password");

    // Wait for successful linking
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 60s, [&] { return linkDeviceSuccessful; }));
    } // Unlock mtxOld

    // Wait for new device successful authentication
    {
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 60s, [&] { return authCompletedOnNewDevice; }));
    }

    // Cleanup
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n", testTag);
}

void
LinkDeviceTest::testExportWithWrongPasswordOnce()
{
    const std::string testTag = "testExportWithWrongPasswordOnce";
    const std::string correctPassword = "c2cvc3jmbafsw";
    const std::string wrongPassword = "bad_password";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithWrongPasswordOnce\n", testTag);

    // Create old device with password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account with password.", testTag);
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);
    oldAcc->changeArchivePassword("", correctPassword);

    // Simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}", testTag, oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceUpdated; }));
    } // Unlock mtxOld

    // Validate password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Validating password on old device.", testTag);
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    // TODO Sleep to compensate for the absence of a callback in the DHT connectionmanager to indicate that the
    // temporary account is ready to receive connections and a matching signal in the archive_account_manager
    std::this_thread::sleep_for(20s);

    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);

    // Wait for auth to start on old device
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return authStartedOnOldDevice; }));
    } // Unlock mtxOld

    // Confirm add device on old device
    libjami::confirmAddDevice(oldDeviceId, operationId);

    // Wait for auth to start on new device
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return authStartedOnNewDevice; }));
    } // Unlock mtxNew

    // Provide incorrect password authentication on new device
    JAMI_DEBUG("[ut_linkdevice:{}] [NewDevice] Providing wrong password authentication on new device.", testTag);

    libjami::provideAccountAuthentication(newDeviceId, wrongPassword, "password");

    // Wait for auth to fail on new device
    { // Lock mtxOld
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvOld.wait_for(lkNew, 30s, [&] { return passwordWasIncorrect; }));
    } // Unlock mtxOld

    // Provide correct password authentication on new device
    JAMI_DEBUG("[ut_linkdevice:{}] [NewDevice] Providing correct password authentication on new device.", testTag);
    libjami::provideAccountAuthentication(newDeviceId, correctPassword, "password");

    // Wait for successful linking
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return linkDeviceSuccessful; }));
    } // Unlock mtxOld

    // Wait for new device successful authentication
    {
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 60s, [&] { return authCompletedOnNewDevice; }));
    }

    // Cleanup
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n", testTag);
}

void
LinkDeviceTest::testExportWithWrongPasswordMaxAttempts()
{
    const std::string testTag = "testExportWithWrongPasswordMaxAttempts";
    const std::string correctPassword = "c2cvc3jmbafsw";
    const std::string wrongPassword = "bad_password";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithWrongPasswordMaxAttempts\n", testTag);

    // Create old device with password
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);
    oldAcc->changeArchivePassword("", correctPassword);

    // Simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}", testTag, oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceUpdated; }));
    } // Unlock mtxOld

    // Validate password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Validating password on old device.", testTag);
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    // TODO Sleep to compensate for the absence of a callback in the DHT connectionmanager to indicate that the
    // temporary account is ready to receive connections and a matching signal in the archive_account_manager
    std::this_thread::sleep_for(20s);

    // Initiate device linking
    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);

    // Wait for auth to start on old device
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return authStartedOnOldDevice; }));
    } // Unlock mtxOld

    // Confirm add device on old device
    libjami::confirmAddDevice(oldDeviceId, operationId);

    // Wait for auth to start on new device
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return authStartedOnNewDevice; }));
    } // Unlock mtxNew

    for (int attempt = 1; attempt <= 3; attempt++) {
        // Provide incorrect password authentication on new device
        JAMI_DEBUG("[ut_linkdevice:{}] [NewDevice] Providing wrong password authentication on new "
                   "device. Attempt {}/3",
                   testTag,
                   attempt);
        passwordWasIncorrect = false;
        libjami::provideAccountAuthentication(newDeviceId, wrongPassword, "password");

        if (attempt < 3) {
            { // Lock mtxNew
                std::unique_lock lkNew {mtxNew};
                CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return passwordWasIncorrect; }));
            } // Unlock mtxNew
        } else {
            { // Lock mtxOld
                std::unique_lock lkOld {mtxOld};
                CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return linkDeviceUnsuccessful; }));
            } // Unlock mtxOld
        }
    }

    // Wait for unsuccessful linking
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return linkDeviceUnsuccessful; }));
    } // Unlock mtxOld

    // Cleanup
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n", testTag);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::LinkDeviceTest::name())
