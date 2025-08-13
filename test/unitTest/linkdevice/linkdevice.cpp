/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "archiver.h"
#include "base64.h"
#include "jami.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"
#include "account_factory.h"
#include "account_schema.h"

#include <dhtnet/connectionmanager.h>
#include <dhtnet/multiplexed_socket.h>

#include <fmt/std.h>
#include <fmt/ranges.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>
#include <condition_variable>
#include <filesystem>
#include <string>

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
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
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
    void testCreateOldDevice();
    void testQrConnection();
    void testExportToPeer();
    void testExportNoPassword();
    void testExportWithCorrectPassword();
    void testExportWithWrongPasswordOnce();
    void testExportWithWrongPasswordMaxAttempts();
    // TODO void testMultipleConnections(); // because should not be trying to send the password
    // data to the wrong connection (test leaking password, archive)

    CPPUNIT_TEST_SUITE(LinkDeviceTest);
    CPPUNIT_TEST(testCreateOldDevice);                    // passes
    CPPUNIT_TEST(testQrConnection);                       // passes
    CPPUNIT_TEST(testExportNoPassword);                   // passes
    CPPUNIT_TEST(testExportWithCorrectPassword);          // passes
    CPPUNIT_TEST(testExportWithWrongPasswordOnce);        // passes
    CPPUNIT_TEST(testExportWithWrongPasswordMaxAttempts); // passes
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(LinkDeviceTest, LinkDeviceTest::name());

void
LinkDeviceTest::setUp()
{}

void
LinkDeviceTest::tearDown()
{
    try {
        // try and cleanup the devices if the test fails or succeeds... this should usually fail due
        // to newDeviceId already being destroyed and same with oldDeviceId
        wait_for_removal_of({oldDeviceId, newDeviceId});
    } catch (const std::exception& e) {
        JAMI_WARNING("[ut_linkdevice] Error during account cleanup... this should be ok.");
    }
}

void
LinkDeviceTest::testCreateOldDevice()
{
    const std::string testTag = "testCreateOldDevice";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testCreateOldDevice\n", testTag);

    std::mutex mtxOld;
    std::condition_variable cvOld;

    auto oldDeviceStarted = false;

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    JAMI_DEBUG("[ut_linkdevice::{}] Registering handlers..", testTag);
    // Setup oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                JAMI_DEBUG("[ut_linkdevice::{}] cb -> VolatileDetailsChanged {}",
                           testTag,
                           accountId);
                if (accountId == oldDeviceId) {
                    auto daemonStatus = details.at(
                        libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    JAMI_DEBUG("[ut_linkdevice::{} {}] oldDevice: daemonStatus = {}",
                               testTag,
                               oldDeviceId,
                               daemonStatus);
                    if (daemonStatus == "true") {
                        std::lock_guard lock(mtxOld);
                        oldDeviceStarted = true;
                        JAMI_DEBUG("[ut_linkdevice::{} {}] [OldDevice] Started old device.",
                                   testTag,
                                   accountId);
                        cvOld.notify_one();
                    }
                }
            }));

    libjami::registerSignalHandlers(confHandlers);
    JAMI_DEBUG("[ut_linkdevice::{}] Registered handlers.", testTag);

    JAMI_DEBUG("[ut_linkdevice::{}] [OldDevice] Creating old device account.", testTag);
    std::map<std::string, std::string> detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    { // Lock mtxOld
        JAMI_DEBUG("LOCKING ====");
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceStarted; }));
    } // Unlock mtxOld

    wait_for_removal_of(oldDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testCreateOldDevice\n", testTag);
    libjami::unregisterSignalHandlers();
}

// Tests the QR code generation that relies on Dht to generate an auth uri
void
LinkDeviceTest::testQrConnection()
{
    const std::string testTag = "testQrConnection";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testQrConnection\n", testTag);
    std::mutex mtxOld;
    std::condition_variable cvOld;
    std::mutex mtxNew;
    std::condition_variable cvNew;

    std::string qrInfo;
    bool linkDeviceConnecting = false;
    bool oldDeviceStarted = false;
    int authSignalCount = 0;

    // Signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Setup oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId != oldDeviceId) {
                    return;
                }
                auto daemonStatus = details.at(
                    libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                if (daemonStatus == "true") {
                    std::lock_guard lock(mtxOld);
                    oldDeviceStarted = true;
                    JAMI_DEBUG("[ut_linkdevice:{} {}] [OldDevice] Started oldDevice.",
                               testTag,
                               accountId);
                    cvOld.notify_one();
                }
            }));

    // Monitor QR code generation
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            [&](const std::string& accountId,
                int status,
                const std::map<std::string, std::string>& details) {
                if (details.find("token") != details.end()) {
                    std::lock_guard lock(mtxNew);
                    qrInfo = details.at("token");
                    JAMI_DEBUG("[ut_linkdevice:{}] cb -> DeviceAuthStateChanged {}",
                               testTag,
                               qrInfo);
                    CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
                    cvNew.notify_one();
                }
            }));

    // Monitor device linking progress
    confHandlers.insert(libjami::exportable_callback<
                        libjami::ConfigurationSignal::AddDeviceStateChanged>(
        [&](const std::string& accountId,
            uint32_t opId,
            int status,
            const std::map<std::string, std::string>& details) {
            std::lock_guard lock(mtxOld);
            ++authSignalCount;

            // Connecting status
            if (status == static_cast<uint8_t>(DeviceAuthState::CONNECTING)) {
                linkDeviceConnecting = true;
            }

            JAMI_DEBUG("[ut_linkdevice:{}] AddDeviceStateChanged: status={}, authSignalCount={}",
                       testTag,
                       status,
                       authSignalCount);
            cvOld.notify_one();
        }));

    libjami::registerSignalHandlers(confHandlers);

    // Create old device
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account.", testTag);
    auto detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);

    // Wait for old device to start
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceStarted; }));
    } // Unlock mtxOld

    // Create new device
    JAMI_DEBUG("[ut_linkdevice:{}] [NewDevice] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    // Initiate device linking
    JAMI_DEBUG(
        "[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
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
    wait_for_removal_of(oldDeviceId);
    wait_for_removal_of(newDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testQrConnection\n", testTag);
    libjami::unregisterSignalHandlers();
}

void
LinkDeviceTest::testExportNoPassword()
{
    const std::string testTag = "testExportNoPassword";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportNoPassword\n", testTag);
    std::mutex mtxOld;
    std::condition_variable cvOld;
    std::mutex mtxNew;
    std::condition_variable cvNew;

    std::string qrInfo;

    bool authStartedOnOldDevice = false;
    bool authStartedOnNewDevice = false;
    bool linkDeviceSuccessful = false;
    uint32_t operationId = 0;
    int authSignalCount = 0;

    std::string oldDeviceId = "";
    bool oldDeviceStarted = false;

    // Signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    // Setup oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId != oldDeviceId) {
                    return;
                }
                JAMI_DEBUG("[ut_linkdevice:{}] cb -> VolatileDetailsChanged == MATCH ==", testTag);
                std::string daemonStatus = details.at(
                    libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                if (daemonStatus == "true") {
                    std::lock_guard lock(mtxOld);
                    oldDeviceStarted = true;
                    JAMI_DEBUG("[ut_linkdevice:{} {}] [OldDevice] Started oldDevice.",
                               testTag,
                               accountId);
                    cvOld.notify_one();
                }
            }));
    // Simulate user input on new device
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            [&](const std::string& accountId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxNew);
                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    authStartedOnNewDevice = true;
                }

                // Token available status
                if (status == static_cast<uint8_t>(DeviceAuthState::TOKEN_AVAILABLE)) {
                    qrInfo = details.at("token");
                    JAMI_DEBUG("[ut_linkdevice:{}] cb -> DeviceAuthStateChanged {}",
                               testTag,
                               qrInfo);
                    CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
                }
                cvNew.notify_one();
            }));
    // Simulate add device on oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
            [&](const std::string& accountId,
                uint32_t opId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxOld);
                JAMI_DEBUG("[ut_linkdevice:{} {}] cb -> AddDeviceStateChanged", testTag, accountId);
                ++authSignalCount;
                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    authStartedOnOldDevice = true;
                }
                // Done status
                if (status == static_cast<uint8_t>(DeviceAuthState::DONE)) {
                    auto errorIt = details.find("error");
                    if (errorIt != details.end()
                        && (!errorIt->second.empty() && errorIt->second != "none")) {
                        // Error when link device
                    } else {
                        linkDeviceSuccessful = true;
                    }
                }
                JAMI_DEBUG("[ut_linkdevice:{}] authSignalCount = {}", testTag, authSignalCount);
                cvOld.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    JAMI_DEBUG("[ut_linkdevice:{}] Registered signal handlers.", testTag);

    JAMI_DEBUG("[ut_linkdevice:{}] Waiting 20s for some reason (FIXME).", testTag);
    std::this_thread::sleep_for(
        std::chrono::seconds(20)); // wait for the old device to be fully initialized

    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account.", testTag);
    std::map<std::string, std::string> detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}",
               testTag,
               oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return oldDeviceStarted; }));
    } // Unlock mtxOld

    JAMI_DEBUG("[ut_linkdevice:{}] Waiting 20s for some reason (FIXME).", testTag);
    std::this_thread::sleep_for(
        std::chrono::seconds(20)); // wait for the old device to be fully initialized

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    JAMI_DEBUG(
        "[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
        testTag,
        oldDeviceId,
        qrInfo);

    // Initiate device linking
    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return authStartedOnOldDevice; }));
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

    wait_for_removal_of(oldDeviceId);
    wait_for_removal_of(newDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportNoPassword\n", testTag);
    libjami::unregisterSignalHandlers();
}

void
LinkDeviceTest::testExportWithCorrectPassword()
{
    const std::string testTag = "testExportWithCorrectPassword";
    const std::string correctPassword = "c2cvc3jmbafsw";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithCorrectPassword\n",
               testTag);

    std::mutex mtxOld;
    std::condition_variable cvOld;
    std::mutex mtxNew;
    std::condition_variable cvNew;

    std::string qrInfo;
    bool authStartedOnOldDevice = false;
    bool authStartedOnNewDevice = false;
    bool linkDeviceSuccessful = false;
    uint32_t operationId = 0;
    int authSignalCount = 0;
    bool oldDeviceStarted = false;

    // Signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Setup oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId != oldDeviceId) {
                    return;
                }
                JAMI_DEBUG("[ut_linkdevice:{}] cb -> VolatileDetailsChanged == MATCH ==", testTag);
                auto daemonStatus = details.at(
                    libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                if (daemonStatus == "true") {
                    std::lock_guard lock(mtxOld);
                    oldDeviceStarted = true;
                    JAMI_DEBUG("[ut_linkdevice:{} {}] [OldDevice] Started oldDevice.",
                               testTag,
                               accountId);
                    cvOld.notify_one();
                }
            }));

    // Monitor QR code generation
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            [&](const std::string& accountId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxNew);

                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    authStartedOnNewDevice = true;
                }
                // Token available status
                if (status == static_cast<uint8_t>(DeviceAuthState::TOKEN_AVAILABLE)) {
                    qrInfo = details.at("token");
                    JAMI_DEBUG("[ut_linkdevice:{}] cb -> DeviceAuthStateChanged {}",
                               testTag,
                               qrInfo);
                    CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
                }
                cvNew.notify_one();
            }));

    // Monitor device linking progress
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
            [&](const std::string& accountId,
                uint32_t opId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxOld);
                ++authSignalCount;

                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    JAMI_DEBUG("[ut_linkdevice:{}] auth started on old device", testTag);
                    authStartedOnOldDevice = true;
                }
                // Done status
                if (status == static_cast<uint8_t>(DeviceAuthState::DONE)) {
                    auto errorIt = details.find("error");
                    if (errorIt != details.end()
                        && (!errorIt->second.empty() && errorIt->second != "none")) {
                        // error when link device
                    } else {
                        linkDeviceSuccessful = true;
                    }
                }

                JAMI_DEBUG("[ut_linkdevice:{}] authSignalCount = {}", testTag, authSignalCount);
                cvOld.notify_one();
            }));

    libjami::registerSignalHandlers(confHandlers);

    // Create old device with password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account with password.", testTag);
    auto detailsOldDevice = libjami::getAccountTemplate("RING");
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD] = correctPassword;
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD_SCHEME]
        = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;
    detailsOldDevice[ConfProperties::ARCHIVE_HAS_PASSWORD] = "true";
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);

    // Simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}",
               testTag,
               oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return oldDeviceStarted; }));
    } // Unlock mtxOld

    // Validate password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Validating password on old device.", testTag);
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] Waiting 20s for some reason (FIXME).", testTag);
    std::this_thread::sleep_for(
        std::chrono::seconds(20)); // wait for the old device to be fully initialized

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

    JAMI_DEBUG(
        "[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
        testTag,
        oldDeviceId,
        qrInfo);

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

    // Cleanup
    wait_for_removal_of(oldDeviceId);
    wait_for_removal_of(newDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n",
               testTag);
    libjami::unregisterSignalHandlers();
}

void
LinkDeviceTest::testExportWithWrongPasswordOnce()
{
    const std::string testTag = "testExportWithWrongPasswordOnce";
    const std::string correctPassword = "c2cvc3jmbafsw";
    const std::string wrongPassword = "bad_password";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithWrongPasswordOnce\n",
               testTag);

    std::mutex mtxOld;
    std::condition_variable cvOld;
    std::mutex mtxNew;
    std::condition_variable cvNew;

    std::string qrInfo;
    bool authStartedOnOldDevice = false;
    bool authStartedOnNewDevice = false;
    bool linkDeviceSuccessful = false;
    bool passwordWasIncorrect = false;
    uint32_t operationId = 0;
    int authSignalCount = 0;
    bool oldDeviceStarted = false;

    // Signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Setup oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId != oldDeviceId) {
                    return;
                }
                JAMI_DEBUG("[ut_linkdevice:{}] cb -> VolatileDetailsChanged == MATCH ==", testTag);
                auto daemonStatus = details.at(
                    libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                if (daemonStatus == "true") {
                    std::lock_guard lock(mtxOld);
                    oldDeviceStarted = true;
                    JAMI_DEBUG("[ut_linkdevice:{} {}] [OldDevice] Started oldDevice.",
                               testTag,
                               accountId);
                    cvOld.notify_one();
                }
            }));

    // Monitor QR code generation
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            [&](const std::string& accountId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxNew);

                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    if (authStartedOnNewDevice) {
                        passwordWasIncorrect = true;
                    }
                    authStartedOnNewDevice = true;
                }
                // Token available status
                if (status == static_cast<uint8_t>(DeviceAuthState::TOKEN_AVAILABLE)) {
                    qrInfo = details.at("token");
                    JAMI_DEBUG("[ut_linkdevice:{}] cb -> DeviceAuthStateChanged {}",
                               testTag,
                               qrInfo);
                    CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
                }
                cvNew.notify_one();
            }));

    // Monitor device linking progress
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
            [&](const std::string& accountId,
                uint32_t opId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxOld);
                ++authSignalCount;

                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    JAMI_DEBUG("[ut_linkdevice:{}] auth started on old device", testTag);
                    authStartedOnOldDevice = true;
                }
                // Done status
                if (status == static_cast<uint8_t>(DeviceAuthState::DONE)) {
                    auto errorIt = details.find("error");
                    if (errorIt != details.end()
                        && (!errorIt->second.empty() && errorIt->second != "none")) {
                        // error when link device
                    } else {
                        linkDeviceSuccessful = true;
                    }
                }

                JAMI_DEBUG("[ut_linkdevice:{}] authSignalCount = {}", testTag, authSignalCount);
                cvOld.notify_one();
            }));

    libjami::registerSignalHandlers(confHandlers);

    // Create old device with password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account with password.", testTag);
    auto detailsOldDevice = libjami::getAccountTemplate("RING");
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD] = correctPassword;
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD_SCHEME]
        = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;
    detailsOldDevice[ConfProperties::ARCHIVE_HAS_PASSWORD] = "true";
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);

    // Simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}",
               testTag,
               oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceStarted; }));
    } // Unlock mtxOld

    // Validate password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Validating password on old device.", testTag);
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] Waiting 20s for some reason (FIXME).", testTag);
    std::this_thread::sleep_for(
        std::chrono::seconds(20)); // wait for the old device to be fully initialized

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

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

    // Provide incorrect password authentication on new device
    JAMI_DEBUG(
        "[ut_linkdevice:{}] [NewDevice] Providing wrong password authentication on new device.",
        testTag);

    libjami::provideAccountAuthentication(newDeviceId, wrongPassword, "password");

    // Wait for auth to fail on new device
    { // Lock mtxOld
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvOld.wait_for(lkNew, 30s, [&] { return passwordWasIncorrect; }));
    } // Unlock mtxOld

    // Provide correct password authentication on new device
    JAMI_DEBUG(
        "[ut_linkdevice:{}] [NewDevice] Providing correct password authentication on new device.",
        testTag);
    libjami::provideAccountAuthentication(newDeviceId, correctPassword, "password");

    // Wait for successful linking
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return linkDeviceSuccessful; }));
    } // Unlock mtxOld

    // Cleanup
    wait_for_removal_of(oldDeviceId);
    wait_for_removal_of(newDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n",
               testTag);
    libjami::unregisterSignalHandlers();
}

void
LinkDeviceTest::testExportWithWrongPasswordMaxAttempts()
{
    const std::string testTag = "testExportWithWrongPasswordMaxAttempts";
    const std::string correctPassword = "c2cvc3jmbafsw";
    const std::string wrongPassword = "bad_password";
    JAMI_DEBUG(
        "\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithWrongPasswordMaxAttempts\n",
        testTag);

    std::mutex mtxOld;
    std::condition_variable cvOld;
    std::mutex mtxNew;
    std::condition_variable cvNew;

    std::string qrInfo;
    bool authStartedOnOldDevice = false;
    bool authStartedOnNewDevice = false;
    bool linkDeviceUnsuccessful = false;
    bool passwordWasIncorrect = false;
    uint32_t operationId = 0;
    int authSignalCount = 0;
    bool oldDeviceStarted = false;

    // Signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Setup oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                if (accountId != oldDeviceId) {
                    return;
                }
                JAMI_DEBUG("[ut_linkdevice:{}] cb -> VolatileDetailsChanged == MATCH ==", testTag);
                auto daemonStatus = details.at(
                    libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                if (daemonStatus == "true") {
                    std::lock_guard lock(mtxOld);
                    oldDeviceStarted = true;
                    JAMI_DEBUG("[ut_linkdevice:{} {}] [OldDevice] Started oldDevice.",
                               testTag,
                               accountId);
                    cvOld.notify_one();
                }
            }));

    // Monitor QR code generation
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            [&](const std::string& accountId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxNew);

                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    if (authStartedOnNewDevice) {
                        passwordWasIncorrect = true;
                    }
                    authStartedOnNewDevice = true;
                }

                // Token available status
                if (status == static_cast<uint8_t>(DeviceAuthState::TOKEN_AVAILABLE)) {
                    qrInfo = details.at("token");
                    JAMI_DEBUG("[ut_linkdevice:{}] cb -> DeviceAuthStateChanged {}",
                               testTag,
                               qrInfo);
                    CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
                }
                cvNew.notify_one();
            }));

    // Monitor device linking progress
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
            [&](const std::string& accountId,
                uint32_t opId,
                uint8_t status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard lock(mtxOld);
                ++authSignalCount;

                // Auth status
                if (status == static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING)) {
                    JAMI_DEBUG("[ut_linkdevice:{}] auth started on old device", testTag);
                    authStartedOnOldDevice = true;
                }
                // Done status
                if (status == static_cast<uint8_t>(DeviceAuthState::DONE)) {
                    auto errorIt = details.find("error");
                    if (errorIt != details.end() && !errorIt->second.empty()) {
                        if (errorIt->second == "auth_error") {
                            passwordWasIncorrect = true;
                            linkDeviceUnsuccessful = true;
                        } else if (errorIt->second == "none") {
                            linkDeviceUnsuccessful = false;
                        }
                    }
                }

                JAMI_DEBUG("[ut_linkdevice:{}] authSignalCount = {}", testTag, authSignalCount);
                cvOld.notify_one();
            }));

    libjami::registerSignalHandlers(confHandlers);

    // Create old device with password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account with password.", testTag);
    auto detailsOldDevice = libjami::getAccountTemplate("RING");
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD] = correctPassword;
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD_SCHEME]
        = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;
    detailsOldDevice[ConfProperties::ARCHIVE_HAS_PASSWORD] = "true";
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);

    // Simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}",
               testTag,
               oldDeviceId);

    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceStarted; }));
    } // Unlock mtxOld

    // Validate password
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Validating password on old device.", testTag);
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] Waiting 20s for some reason (FIXME).", testTag);
    std::this_thread::sleep_for(
        std::chrono::seconds(20)); // wait for the old device to be fully initialized

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    auto detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    { // Lock mtxNew
        std::unique_lock lkNew {mtxNew};
        CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));
    } // Unlock mtxNew

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

        // Wait for auth to fail on new device
        { // Lock mtxOld
            std::unique_lock lkNew {mtxNew};
            CPPUNIT_ASSERT(cvOld.wait_for(lkNew, 30s, [&] { return passwordWasIncorrect; }));
        } // Unlock mtxOld
    }

    // Wait for unsuccessful linking
    { // Lock mtxOld
        std::unique_lock lkOld {mtxOld};
        CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return linkDeviceUnsuccessful; }));
    } // Unlock mtxOld

    // Cleanup
    wait_for_removal_of(oldDeviceId);
    wait_for_removal_of(newDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n",
               testTag);
    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::LinkDeviceTest::name())
