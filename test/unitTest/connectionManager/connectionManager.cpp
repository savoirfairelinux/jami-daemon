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
    void testFloodSocket();
    void testDestroyWhileSending();
    void testIsConnecting();
    void testCanSendBeacon();
    void testCannotSendBeacon();
    void testConnectivityChangeTriggerBeacon();
    void testOnNoBeaconTriggersShutdown();

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
    CPPUNIT_TEST(testFloodSocket);
    CPPUNIT_TEST(testDestroyWhileSending);
    CPPUNIT_TEST(testIsConnecting);
    CPPUNIT_TEST(testCanSendBeacon);
    CPPUNIT_TEST(testCannotSendBeacon);
    CPPUNIT_TEST(testConnectivityChangeTriggerBeacon);
    CPPUNIT_TEST(testOnNoBeaconTriggersShutdown);
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
    confHandlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&] {
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
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(
        cvReceive.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyConnected; }));
}

void
ConnectionManagerTest::testAcceptConnection()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return successfullyReceive && successfullyConnected && receiverConnected;
    }));
}

void
ConnectionManagerTest::testMultipleChannels()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected2 = true;
                                                        }
                                                        cv.notify_one();
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return successfullyConnected && successfullyConnected2 && receiverConnected == 2;
    }));
    CPPUNIT_ASSERT(aliceAccount->connectionManager().activeSockets() == 1);
}

void
ConnectionManagerTest::testMultipleChannelsSameName()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });

    // We can open two sockets with the same name, it will be two different channel
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected2 = true;
                                                        }
                                                        cv.notify_one();
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return successfullyConnected && successfullyConnected2 && receiverConnected == 2;
    }));
}

void
ConnectionManagerTest::testSendReceiveData()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return events == 4 && successfullyReceive && successfullyConnected && successfullyConnected2
               && dataOk && dataOk2;
    }));
}

void
ConnectionManagerTest::testDeclineConnection()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
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
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyReceive && successfullyConnected && receiverConnected;
    }));
}

void
ConnectionManagerTest::testDeclineICERequest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
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
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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
                                                            socket->onShutdown([&] {
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
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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
                socket->onShutdown([&] {
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
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

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
                socket->onShutdown([&] {
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
                                                            socket->onShutdown([&] {
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
    CPPUNIT_ASSERT(scv.wait_for(lk, std::chrono::seconds(60), [&] {
        return events == 2 && successfullyReceive && successfullyConnected && receiverConnected;
    }));
}

void
ConnectionManagerTest::testShutdownCallbacks()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));

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
    CPPUNIT_ASSERT(rcv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyReceive && successfullyConnected && receiverConnected;
    }));

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
    CPPUNIT_ASSERT(rcv.wait_for(lk, std::chrono::seconds(30), [&] { return channel2NotConnected; }));
}

void
ConnectionManagerTest::testFloodSocket()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> rcvSock1, rcvSock2, rcvSock3, sendSock, sendSock2, sendSock3;
    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "1";
            return true;
        });
    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket != nullptr;
            if (name == "1")
                rcvSock1 = socket;
            else if (name == "2")
                rcvSock2 = socket;
            else if (name == "3")
                rcvSock3 = socket;
        });
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "1",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            sendSock = socket;
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyReceive && successfullyConnected && receiverConnected;
    }));
    CPPUNIT_ASSERT(receiverConnected);
    successfullyConnected = false;
    receiverConnected = false;
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "2",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            sendSock2 = socket;
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyConnected && receiverConnected;
    }));
    successfullyConnected = false;
    receiverConnected = false;
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "3",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            sendSock3 = socket;
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyConnected && receiverConnected;
    }));
    std::mutex mtxRcv {};
    std::string alphabet, shouldRcv, rcv1, rcv2, rcv3;
    for (int i = 0; i < 100; ++i)
        alphabet += "QWERTYUIOPASDFGHJKLZXCVBNM";
    rcvSock1->setOnRecv([&](const uint8_t* buf, size_t len) {
        rcv1 += std::string(buf, buf + len);
        return len;
    });
    rcvSock2->setOnRecv([&](const uint8_t* buf, size_t len) {
        rcv2 += std::string(buf, buf + len);
        return len;
    });
    rcvSock3->setOnRecv([&](const uint8_t* buf, size_t len) {
        rcv3 += std::string(buf, buf + len);
        return len;
    });
    for (uint64_t i = 0; i < alphabet.size(); ++i) {
        auto send = std::string(8000, alphabet[i]);
        shouldRcv += send;
        std::error_code ec;
        sendSock->write(reinterpret_cast<unsigned char*>(send.data()), send.size(), ec);
        sendSock2->write(reinterpret_cast<unsigned char*>(send.data()), send.size(), ec);
        sendSock3->write(reinterpret_cast<unsigned char*>(send.data()), send.size(), ec);
        CPPUNIT_ASSERT(!ec);
    }
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return shouldRcv == rcv1 && shouldRcv == rcv2 && shouldRcv == rcv3;
    }));
}

void
ConnectionManagerTest::testDestroyWhileSending()
{
    // Same as test before, but destroy the accounts while sending.
    // This test if a segfault occurs
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> rcvSock1, rcvSock2, rcvSock3, sendSock, sendSock2, sendSock3;
    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive](const DeviceId&, const std::string& name) {
            successfullyReceive = name == "1";
            return true;
        });
    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            receiverConnected = socket != nullptr;
            if (name == "1")
                rcvSock1 = socket;
            else if (name == "2")
                rcvSock2 = socket;
            else if (name == "3")
                rcvSock3 = socket;
        });
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "1",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            sendSock = socket;
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyReceive && successfullyConnected && receiverConnected;
    }));
    successfullyConnected = false;
    receiverConnected = false;
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "2",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            sendSock2 = socket;
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyConnected && receiverConnected;
    }));
    successfullyConnected = false;
    receiverConnected = false;
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "3",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            sendSock3 = socket;
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&] {
        return successfullyConnected && receiverConnected;
    }));
    std::mutex mtxRcv {};
    std::string alphabet;
    for (int i = 0; i < 100; ++i)
        alphabet += "QWERTYUIOPASDFGHJKLZXCVBNM";
    rcvSock1->setOnRecv([&](const uint8_t*, size_t len) { return len; });
    rcvSock2->setOnRecv([&](const uint8_t*, size_t len) { return len; });
    rcvSock3->setOnRecv([&](const uint8_t*, size_t len) { return len; });
    for (uint64_t i = 0; i < alphabet.size(); ++i) {
        auto send = std::string(8000, alphabet[i]);
        std::error_code ec;
        sendSock->write(reinterpret_cast<unsigned char*>(send.data()), send.size(), ec);
        sendSock2->write(reinterpret_cast<unsigned char*>(send.data()), send.size(), ec);
        sendSock3->write(reinterpret_cast<unsigned char*>(send.data()), send.size(), ec);
        CPPUNIT_ASSERT(!ec);
    }

    // No need to wait, immediately destroy, no segfault must occurs
}

void
ConnectionManagerTest::testIsConnecting()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false, successfullyReceive = false;

    bobAccount->connectionManager().onChannelRequest([&](const DeviceId&, const std::string&) {
        successfullyReceive = true;
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    });

    CPPUNIT_ASSERT(!aliceAccount->connectionManager().isConnecting(bobDeviceId, "sip"));
    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    // connectDevice is full async, so isConnecting will be true after a few ms.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(aliceAccount->connectionManager().isConnecting(bobDeviceId, "sip"));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyConnected; }));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)); // Just to wait for the callback to finish
    CPPUNIT_ASSERT(!aliceAccount->connectionManager().isConnecting(bobDeviceId, "sip"));
}

void
ConnectionManagerTest::testCanSendBeacon()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false, successfullyReceive = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest([&](const DeviceId&, const std::string&) {
        successfullyReceive = true;
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    });
    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string&, std::shared_ptr<ChannelSocket> socket) {
            if (socket && socket->name() == "sip")
                bobSocket = socket->underlyingSocket();
            cv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            aliceSocket = socket->underlyingSocket();
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    // connectDevice is full async, so isConnecting will be true after a few ms.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&] { return aliceSocket && bobSocket; }));
    CPPUNIT_ASSERT(aliceSocket->canSendBeacon());
    CPPUNIT_ASSERT(bobSocket->canSendBeacon());
}

void
ConnectionManagerTest::testCannotSendBeacon()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false, successfullyReceive = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest([&](const DeviceId&, const std::string&) {
        successfullyReceive = true;
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    });
    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string&, std::shared_ptr<ChannelSocket> socket) {
            if (socket && socket->name() == "sip")
                bobSocket = socket->underlyingSocket();
            cv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            aliceSocket = socket->underlyingSocket();
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    // connectDevice is full async, so isConnecting will be true after a few ms.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&] { return aliceSocket && bobSocket; }));

    int major, minor, patch;
    bobSocket->onVersion([&](auto msg) {
        major = msg.major, minor = msg.minor, patch = msg.patch;
        cv.notify_one();
    });
    aliceSocket->setFakeVersion("9.1.2");
    aliceSocket->sendVersion();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] {
        return major == 9 && minor == 1 && patch == 2;
    }));
    CPPUNIT_ASSERT(!bobSocket->canSendBeacon());
}

void
ConnectionManagerTest::testConnectivityChangeTriggerBeacon()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false, successfullyReceive = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest([&](const DeviceId&, const std::string&) {
        successfullyReceive = true;
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    });
    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string&, std::shared_ptr<ChannelSocket> socket) {
            if (socket && socket->name() == "sip")
                bobSocket = socket->underlyingSocket();
            cv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            aliceSocket = socket->underlyingSocket();
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    // connectDevice is full async, so isConnecting will be true after a few ms.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&] { return aliceSocket && bobSocket; }));

    bool hasRequest = false;
    bobSocket->onBeacon([&](auto msg) {
        if (msg.isRequest)
            hasRequest = true;
        cv.notify_one();
    });
    aliceAccount->connectionManager().connectivityChanged();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return hasRequest; }));
}

void
ConnectionManagerTest::testOnNoBeaconTriggersShutdown()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyConnected = false, successfullyReceive = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest([&](const DeviceId&, const std::string&) {
        successfullyReceive = true;
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    });
    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string&, std::shared_ptr<ChannelSocket> socket) {
            if (socket && socket->name() == "sip")
                bobSocket = socket->underlyingSocket();
            cv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            aliceSocket = socket->underlyingSocket();
                                                            successfullyConnected = true;
                                                        }
                                                        cv.notify_one();
                                                    });
    // connectDevice is full async, so isConnecting will be true after a few ms.
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(60), [&] { return aliceSocket && bobSocket; }));

    bool isClosed = false;
    aliceSocket->onShutdown([&] {
        isClosed = true;
        cv.notify_one();
    });
    bobSocket->answerToBeacon(false);
    aliceAccount->connectionManager().connectivityChanged();
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(10), [&] { return isClosed; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConnectionManagerTest::name())
