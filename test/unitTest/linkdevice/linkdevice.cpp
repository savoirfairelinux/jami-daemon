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

#include <dhtnet/connectionmanager.h>
#include <dhtnet/multiplexed_socket.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <git2.h>
#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>

#include "account_factory.h"
#include "account_schema.h"
#include "common.h"

#include "account_const.h"
#include <condition_variable>
#include <filesystem>
#include <string>

#include <dhtnet/multiplexed_socket.h>

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
    void testExportWithWrongPassword();
    // TODO void testMultipleConnections(); // because should not be trying to send the password
    // data to the wrong connection (test leaking password, archive)

    CPPUNIT_TEST_SUITE(LinkDeviceTest);
    // CPPUNIT_TEST(testCreateOldDevice);
    // CPPUNIT_TEST(testQrConnection); // has issues but can be run to diagnose early connection issues
    CPPUNIT_TEST(testExportNoPassword);          // passes
    CPPUNIT_TEST(testExportWithCorrectPassword); // passes
    // CPPUNIT_TEST(testExportWithWrongPassword); // TODO for security/pentesting
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
    } catch (std::exception e) {
        JAMI_WARNING("[ut_linkdevice] Error during account cleanup... this should be ok.");
    }
}

void
LinkDeviceTest::testCreateOldDevice()
{
    const std::string testTag = "testCreateOldDevice";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testCreateOldDevice\n", testTag);

    std::mutex mtxOld;
    std::unique_lock<std::mutex> lkOld {mtxOld};
    std::condition_variable cvOld;

    auto oldDeviceStarted = false;
    std::string oldDeviceId = "";

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    JAMI_DEBUG("[ut_linkdevice::{}] Registering handlers..", testTag);
    // setup oldDevice
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
                        std::lock_guard<std::mutex> lock(mtxOld);
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
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceStarted; }));
    wait_for_removal_of(oldDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testCreateOldDevice\n", testTag);
    libjami::unregisterSignalHandlers();
}

// tests the qr code generation that relies on Dht to generate an auth uri
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

    std::string oldDeviceId = "";

    // signals
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
                    std::lock_guard<std::mutex> lock(mtxOld);
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
                    std::lock_guard<std::mutex> lock(mtxNew);
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
            std::lock_guard<std::mutex> lock(mtxOld);
            ++authSignalCount;

            // connecting status
            if (status == 2) {
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
    std::map<std::string, std::string> detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);

    // Wait for old device to start
    std::unique_lock<std::mutex> lkOld {mtxOld};
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return oldDeviceStarted; }));

    // Create new device
    JAMI_DEBUG("[ut_linkdevice:{}] [NewDevice] Creating new device account.", testTag);
    std::map<std::string, std::string> detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    std::unique_lock<std::mutex> lkNew {mtxNew};
    CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));

    // Initiate device linking
    JAMI_DEBUG(
        "[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
        testTag,
        oldDeviceId,
        qrInfo);
    libjami::addDevice(oldDeviceId, qrInfo);

    // Wait for connecting status
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 60s, [&] { return linkDeviceConnecting; }));

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

    // signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    // setup oldDevice
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
                    std::lock_guard<std::mutex> lock(mtxOld);
                    oldDeviceStarted = true;
                    JAMI_DEBUG("[ut_linkdevice:{} {}] [OldDevice] Started oldDevice.",
                               testTag,
                               accountId);
                    cvOld.notify_one();
                }
            }));
    // simulate user input on new device
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            [&](const std::string& accountId,
                int status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard<std::mutex> lock(mtxNew);
                // auth status
                if (status == 3) {
                    authStartedOnNewDevice = true;
                }

                // token available status
                if (status == 1) {
                    qrInfo = details["token"];
                    JAMI_DEBUG("[ut_linkdevice:{}] cb -> DeviceAuthStateChanged {}\naccountId: {}",
                               testTag,
                               qrInfo);
                    CPPUNIT_ASSERT(qrInfo.substr(0, 9) == "jami-auth");
                }
                cvNew.notify_one();
            }));
    // simulate add device on oldDevice
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
            [&](const std::string& accountId,
                uint32_t opId,
                int status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard<std::mutex> lock(mtxOld);
                JAMI_DEBUG("[ut_linkdevice:{} {}] cb -> AddDeviceStateChanged", testTag, accountId);
                ++authSignalCount;
                // auth status
                if (status == 3) {
                    authStartedOnOldDevice = true;
                }
                // done status
                if (status == 5) {
                    auto errorIt = details.find("error");
                    if (errorIt != details.end()
                        && (!errorIt->second.empty() || errorIt->second != "none")) {
                        // error when link device
                    } else {
                        linkDeviceSuccessful = true;
                    }
                }
                JAMI_DEBUG("[ut_linkdevice:{}] authSignalCount = {}", testTag, authSignalCount);
                cvOld.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    JAMI_DEBUG("[ut_linkdevice:{}] Registered signal handlers.", testTag);

    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Creating old device account.", testTag);
    std::map<std::string, std::string> detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}",
               testTag,
               oldDeviceId);

    std::unique_lock<std::mutex> lkOld {mtxOld};
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return oldDeviceStarted; }));

    std::map<std::string, std::string> detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    auto newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);
    std::unique_lock<std::mutex> lkNew {mtxNew};
    CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 60s, [&] { return !qrInfo.empty(); }));

    JAMI_DEBUG(
        "[ut_linkdevice:{}] [OldDevice] Simulating QR scan... testing libjami::addDevice({}, {})",
        testTag,
        oldDeviceId,
        qrInfo);

    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return authStartedOnOldDevice; }));
    // confirm linkDevice on old device
    libjami::confirmAddDevice(operationId);
    CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 120s, [&] { return authStartedOnNewDevice; }));
    // provide account authentication on new device
    libjami::provideAccountAuthentication(newDeviceId, "", fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD);
    // wait for linkDevice to complete
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return linkDeviceSuccessful; }));

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

    oldDeviceId = "";
    newDeviceId = "";

    // signals
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
                    std::lock_guard<std::mutex> lock(mtxOld);
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
                std::lock_guard<std::mutex> lock(mtxNew);
                // auth status
                if (status == 3) {
                    authStartedOnNewDevice = true;
                }
                // token available status
                if (status == 1) {
                    qrInfo = details["token"];
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
                int status,
                const std::map<std::string, std::string>& details) {
                std::lock_guard<std::mutex> lock(mtxOld);
                ++authSignalCount;

                // auth status
                if (status == 3) {
                    authStartedOnOldDevice = true;
                }
                // done status
                if (status == 5) {
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
    std::map<std::string, std::string> detailsOldDevice = libjami::getAccountTemplate("RING");
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD] = correctPassword;
    detailsOldDevice[ConfProperties::ARCHIVE_PASSWORD_SCHEME]
        = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;
    detailsOldDevice[ConfProperties::ARCHIVE_HAS_PASSWORD] = "true";
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(oldDeviceId);
    // simulate user entering password credentials
    JAMI_DEBUG("[ut_linkdevice:{}] [OldDevice] Created OldDevice account with accountId = {}",
               testTag,
               oldDeviceId);

    std::unique_lock<std::mutex> lkOld {mtxOld};
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 120s, [&] { return oldDeviceStarted; }));
    CPPUNIT_ASSERT(oldAcc->isPasswordValid(correctPassword));

    JAMI_DEBUG("[ut_linkdevice:{}] [New Device] Creating new device account.", testTag);
    // TODO change this while loop to a wait_for lock in case it gets stuck forever and messes with
    // automated testing
    while (!oldDeviceStarted) {
        JAMI_WARNING("[LinkDevice:{}] Waiting for oldDeviceStarted...", testTag);
    }

    std::map<std::string, std::string> detailsNewDevice = libjami::getAccountTemplate("RING");
    detailsNewDevice[ConfProperties::ARCHIVE_URL] = "jami-auth";
    newDeviceId = jami::Manager::instance().addAccount(detailsNewDevice);

    // Wait for QR code
    std::unique_lock<std::mutex> lkNew {mtxNew};
    CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return !qrInfo.empty(); }));

    // Initiate device linking
    operationId = libjami::addDevice(oldDeviceId, qrInfo);
    CPPUNIT_ASSERT(operationId > 0);

    // Wait for auth to start on old device
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 30s, [&] { return authStartedOnOldDevice; }));

    // Confirm add device on old device
    libjami::confirmAddDevice(operationId);

    // Wait for auth to start on new device
    CPPUNIT_ASSERT(cvNew.wait_for(lkNew, 30s, [&] { return authStartedOnNewDevice; }));

    // Provide password authentication on new device
    libjami::provideAccountAuthentication(newDeviceId,
                                          correctPassword,
                                          fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD);

    // Wait for successful linking
    CPPUNIT_ASSERT(cvOld.wait_for(lkOld, 60s, [&] { return linkDeviceSuccessful; }));

    // Cleanup
    wait_for_removal_of(oldDeviceId);
    wait_for_removal_of(newDeviceId);
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithCorrectPassword\n",
               testTag);
    libjami::unregisterSignalHandlers();
}

// TODO poke the protocol
void
LinkDeviceTest::testExportWithWrongPassword()
{
    std::string testTag = "testExportWithWrongPassword";
    JAMI_DEBUG("\n[ut_linkdevice::{}] Starting ut_linkdevice::testExportWithWrongPassword\n",
               testTag);
    // TODO send an incorrect password and make sure the account transfer does not occur
    JAMI_DEBUG("\n[ut_linkdevice::{}] Finished ut_linkdevice::testExportWithWrongPassword\n",
               testTag);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::LinkDeviceTest::name())
