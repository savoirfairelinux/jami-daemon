/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "account_const.h"

using namespace DRing::Account;

namespace jami { namespace test {

class ConnectionManagerTest : public CppUnit::TestFixture {
public:
    ~ConnectionManagerTest() {
        DRing::fini();
    }
    static std::string name() { return "ConnectionManager"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testConnectDevice();
    void testAcceptConnection();
    void testMultipleChannels();
    void testDeclineConnection();
    void testSendReceiveData();

    CPPUNIT_TEST_SUITE(ConnectionManagerTest);
    CPPUNIT_TEST(testConnectDevice);
    CPPUNIT_TEST(testAcceptConnection);
    CPPUNIT_TEST(testMultipleChannels);
    CPPUNIT_TEST(testSendReceiveData);
    //TODO destroy seems to hang CPPUNIT_TEST(testDeclineConnection);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConnectionManagerTest, ConnectionManagerTest::name());

void
ConnectionManagerTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
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

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    bool ready = false;
    bool idx = 0;
    while(!ready && idx < 100) {
        auto details = aliceAccount->getVolatileAccountDetails();
        auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready = (daemonStatus == "REGISTERED");
        details = bobAccount->getVolatileAccountDetails();
        daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready &= (daemonStatus == "REGISTERED");
        if (!ready) {
            idx += 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void
ConnectionManagerTest::tearDown()
{
    Manager::instance().removeAccount(aliceId);
    Manager::instance().removeAccount(bobId);
}

void
ConnectionManagerTest::testConnectDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable cv;
    bool successfullyConnected = false;

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected = true;
        }
        cv.notify_one();
    });
    cv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyConnected);
}

void
ConnectionManagerTest::testAcceptConnection()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        successfullyReceive = name == "git://*";
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&receiverConnected](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        receiverConnected = socket && (name == "git://*");
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected = true;
        }
        cv.notify_one();
    });

    cv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testMultipleChannels()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyConnected2 = false;
    int receiverConnected = 0;

    bobAccount->connectionManager().onChannelRequest(
    [](const std::string&, const std::string& name) {
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&receiverConnected](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        if (socket) receiverConnected += 1;
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected = true;
        }
        cv.notify_one();
    });

    // We can open two sockets with the same name, it will be two different channel
    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&cv, &successfullyConnected2](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected2 = true;
        }
        cv.notify_one();
    });

    cv.wait_for(lk, std::chrono::seconds(10));
    cv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(successfullyConnected2);
    CPPUNIT_ASSERT(receiverConnected == 2);
}


void
ConnectionManagerTest::testSendReceiveData()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable cv;
    bool successfullyConnected = false, successfullyConnected2 = false, successfullyReceive = false, receiverConnected = false;
    const uint8_t buf_other[] = {0x64, 0x65, 0x66, 0x67};
    const uint8_t buf_test[] = {0x68, 0x69, 0x70, 0x71};
    bool dataOk = false, dataOk2 = false;

    bobAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        successfullyReceive = true;
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        if (socket && (name == "test" || name == "other")) {
            receiverConnected = true;
            std::error_code ec;
            auto res = socket->waitForData(std::chrono::milliseconds(5000), ec);
            if (res == 4) {
                uint8_t buf[4];
                socket->read(&buf[0], 4, ec);
                if (name == "test")
                    dataOk = std::equal(std::begin(buf), std::end(buf), std::begin(buf_test));
                else
                    dataOk2 = std::equal(std::begin(buf), std::end(buf), std::begin(buf_other));
                cv.notify_one();
            }
        }
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "test",
        [&](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected = true;
            uint8_t buf[] = {0x64, 0x65, 0x66, 0x67};
            std::error_code ec;
            socket->write(&buf_test[0], 4, ec);
        }
        cv.notify_one();
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "other",
        [&](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected2 = true;
            std::error_code ec;
            socket->write(&buf_other[0], 4, ec);
        }
        cv.notify_one();
    });

    // TODO semaphore 4
    cv.wait_for(lk, std::chrono::seconds(10));
    cv.wait_for(lk, std::chrono::seconds(10));
    cv.wait_for(lk, std::chrono::seconds(10));
    cv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(successfullyConnected2);
    CPPUNIT_ASSERT(receiverConnected);
    CPPUNIT_ASSERT(dataOk);
    CPPUNIT_ASSERT(dataOk2);

}

void
ConnectionManagerTest::testDeclineConnection()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        successfullyReceive = true;
        return false;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&receiverConnected](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        if (receiverConnected)
            receiverConnected = true;
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected = true;
        }
        cv.notify_one();
    });
    cv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(!successfullyConnected);
    CPPUNIT_ASSERT(!receiverConnected);
}

}} // namespace test

RING_TEST_RUNNER(jami::test::ConnectionManagerTest::name())
