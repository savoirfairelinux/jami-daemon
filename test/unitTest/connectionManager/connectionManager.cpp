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

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/multiplexed_socket.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "account_const.h"

using namespace DRing::Account;

namespace jami {
namespace test {

class ConnectionManagerTest : public CppUnit::TestFixture
{
public:
    ConnectionManagerTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~ConnectionManagerTest() { DRing::fini(); }
    static std::string name() { return "ConnectionManager"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testConnectDevice();
    void testAcceptConnection();
    void testMultipleChannels();
    void testMultipleChannelsSameName();
    void testDeclineConnection();
    void testSendReceiveData();
    void testAcceptsICERequest();
    void testDeclineICERequest();
    void testChannelRcvShutdown();
    void testChannelSenderShutdown();
    void testCloseConnectionWithDevice();
    void testShutdownCallbacks();

    CPPUNIT_TEST_SUITE(ConnectionManagerTest);
    CPPUNIT_TEST(testConnectDevice);
    CPPUNIT_TEST(testAcceptConnection);
    CPPUNIT_TEST(testMultipleChannels);
    CPPUNIT_TEST(testMultipleChannelsSameName);
    CPPUNIT_TEST(testDeclineConnection);
    CPPUNIT_TEST(testSendReceiveData);
    CPPUNIT_TEST(testAcceptsICERequest);
    CPPUNIT_TEST(testDeclineICERequest);
    CPPUNIT_TEST(testChannelRcvShutdown);
    CPPUNIT_TEST(testChannelSenderShutdown);
    CPPUNIT_TEST(testCloseConnectionWithDevice);
    CPPUNIT_TEST(testShutdownCallbacks);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConnectionManagerTest, ConnectionManagerTest::name());

void
ConnectionManagerTest::setUp()
{
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
    auto accountsReady = 0;
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
ConnectionManagerTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - 2) {
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);
    // Because cppunit is not linked with dbus, just poll if removed
    cv.wait_for(lk, std::chrono::seconds(30));

    DRing::unregisterSignalHandlers();
}

void
ConnectionManagerTest::testConnectDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv, cvReceive;
    bool successfullyConnected = false;
    bool successfullyReceive = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive, &cvReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "git://*";
            cvReceive.notify_one();
            return true;
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });
    cvReceive.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyReceive);
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyConnected);
}

void
ConnectionManagerTest::testAcceptConnection()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "git://*";
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&receiverConnected](const DeviceId&,
                             const std::string& name,
                             std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testMultipleChannels()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyConnected2 = false;
    int receiverConnected = 0;

    bobAccount->connectionManager().onChannelRequest(
        [](const DeviceId&, const std::string&) { return true; });

    bobAccount->connectionManager().onConnectionReady(
        [&receiverConnected](const DeviceId&,
                             const std::string&,
                             std::shared_ptr<ChannelSocket> socket) {
            if (socket)
                receiverConnected += 1;
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "sip://*",
                       [&cv, &successfullyConnected2](std::shared_ptr<ChannelSocket> socket,
                                                      const DeviceId&) {
                           if (socket) {
                               successfullyConnected2 = true;
                           }
                           cv.notify_one();
                       });

    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(successfullyConnected2);
    CPPUNIT_ASSERT(receiverConnected == 2);
}

void
ConnectionManagerTest::testMultipleChannelsSameName()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyConnected2 = false;
    int receiverConnected = 0;

    bobAccount->connectionManager().onChannelRequest(
        [](const DeviceId&, const std::string&) { return true; });

    bobAccount->connectionManager().onConnectionReady(
        [&receiverConnected](const DeviceId&,
                             const std::string&,
                             std::shared_ptr<ChannelSocket> socket) {
            if (socket)
                receiverConnected += 1;
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });

    // We can open two sockets with the same name, it will be two different channel
    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected2](std::shared_ptr<ChannelSocket> socket,
                                                      const DeviceId&) {
                           if (socket) {
                               successfullyConnected2 = true;
                           }
                           cv.notify_one();
                       });

    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(successfullyConnected2);
    CPPUNIT_ASSERT(receiverConnected == 2);
}

void
ConnectionManagerTest::testSendReceiveData()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::atomic_int events(0);
    bool successfullyConnected = false, successfullyConnected2 = false, successfullyReceive = false,
         receiverConnected = false;
    const uint8_t buf_other[] = {0x64, 0x65, 0x66, 0x67};
    const uint8_t buf_test[] = {0x68, 0x69, 0x70, 0x71};
    bool dataOk = false, dataOk2 = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string&) {
            successfullyReceive = true;
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
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
                    events++;
                    cv.notify_one();
                }
            }
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "test",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            std::error_code ec;
                                                            socket->write(&buf_test[0], 4, ec);
                                                        }
                                                        events++;
                                                        cv.notify_one();
                                                    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "other",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected2 = true;
                                                            std::error_code ec;
                                                            socket->write(&buf_other[0], 4, ec);
                                                        }
                                                        events++;
                                                        cv.notify_one();
                                                    });

    auto expiration = std::chrono::system_clock::now() + std::chrono::seconds(10);
    cv.wait_until(lk, expiration, [&events]() { return events == 4; });
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
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string&) {
            successfullyReceive = true;
            return false;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&receiverConnected](const DeviceId&,
                             const std::string&,
                             std::shared_ptr<ChannelSocket> socket) {
            if (socket)
                receiverConnected = true;
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(!successfullyConnected);
    CPPUNIT_ASSERT(!receiverConnected);
}

void
ConnectionManagerTest::testAcceptsICERequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [](const DeviceId&, const std::string&) { return true; });
    bobAccount->connectionManager().onICERequest([&](const DeviceId&) {
        successfullyReceive = true;
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
        [&receiverConnected](const DeviceId&,
                             const std::string& name,
                             std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testDeclineICERequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [](const DeviceId&, const std::string&) { return true; });
    bobAccount->connectionManager().onICERequest([&](const DeviceId&) {
        successfullyReceive = true;
        return false;
    });

    bobAccount->connectionManager().onConnectionReady(
        [&receiverConnected](const DeviceId&,
                             const std::string& name,
                             std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
        });

    aliceAccount->connectionManager()
        .connectDevice(bobDeviceId,
                       "git://*",
                       [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket,
                                                     const DeviceId&) {
                           if (socket) {
                               successfullyConnected = true;
                           }
                           cv.notify_one();
                       });

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(!successfullyConnected);
    CPPUNIT_ASSERT(!receiverConnected);
}

void
ConnectionManagerTest::testChannelRcvShutdown()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, scv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    bool shutdownReceived = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "git://*";
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "git://*");
            rcv.notify_one();
            socket->shutdown();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            socket->onShutdown([&]() {
                                                                shutdownReceived = true;
                                                                scv.notify_one();
                                                            });
                                                            successfullyConnected = true;
                                                        }
                                                    });

    rcv.wait_for(lk, std::chrono::seconds(30));
    scv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(shutdownReceived);
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testChannelSenderShutdown()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, scv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    bool shutdownReceived = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "git://*";
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            if (socket) {
                socket->onShutdown([&]() {
                    shutdownReceived = true;
                    scv.notify_one();
                });
            }
            receiverConnected = socket && (name == "git://*");
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            rcv.notify_one();
                                                            socket->shutdown();
                                                        }
                                                    });

    rcv.wait_for(lk, std::chrono::seconds(30));
    scv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(shutdownReceived);
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testCloseConnectionWithDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, scv;
    std::atomic_int events(0);
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "git://*";
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            if (socket) {
                socket->onShutdown([&]() {
                    events += 1;
                    scv.notify_one();
                });
            }
            receiverConnected = socket && (name == "git://*");
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            socket->onShutdown([&]() {
                                                                events += 1;
                                                                scv.notify_one();
                                                            });
                                                            successfullyConnected = true;
                                                            rcv.notify_one();
                                                        }
                                                    });

    rcv.wait_for(lk, std::chrono::seconds(30));
    // This should trigger onShutdown
    aliceAccount->connectionManager().closeConnectionsWith(bobDeviceId);
    auto expiration = std::chrono::system_clock::now() + std::chrono::seconds(10);
    scv.wait_until(lk, expiration, [&events]() { return events == 2; });
    CPPUNIT_ASSERT(events == 2);
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testShutdownCallbacks()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);
    auto aliceDeviceId = DeviceId(aliceAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID]);

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, chan2cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive, &chan2cv](const DeviceId&, const std::string& name) {
            if (name == "1") {
                successfullyReceive = true;
            } else {
                chan2cv.notify_one();
                // Do not return directly. Let the connection be closed
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket && (name == "1");
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "1",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                            rcv.notify_one();
                                                        }
                                                    });
    // Connect first channel. This will initiate a mx sock
    rcv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);

    // Connect another channel, but close the connection
    bool channel2NotConnected = false;
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "2",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        channel2NotConnected = !socket;
                                                        rcv.notify_one();
                                                    });
    chan2cv.wait_for(lk, std::chrono::seconds(30));

    // This should trigger onShutdown for second callback
    bobAccount->connectionManager().closeConnectionsWith(aliceDeviceId);
    rcv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(channel2NotConnected);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConnectionManagerTest::name())
