/*
 *  Copyright (C) 2017-2023 Savoir-faire Linux Inc.
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
#include "connectivity/connectionmanager.h"
#include "connectivity/multiplexed_socket.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"

using namespace libjami::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

class ConnectionManagerTest : public CppUnit::TestFixture
{
public:
    ConnectionManagerTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConnectionManagerTest() { libjami::fini(); }
    static std::string name() { return "ConnectionManager"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testConnectDevice();
    void testAcceptConnection();
    void testMultipleChannels();
    void testMultipleChannelsOneDeclined();
    void testMultipleChannelsSameName();
    void testDeclineConnection();
    void testSendReceiveData();
    void testAcceptsICERequest();
    void testDeclineICERequest();
    void testChannelRcvShutdown();
    void testChannelSenderShutdown();
    void testCloseConnectionWith();
    void testShutdownCallbacks();
    void testFloodSocket();
    void testDestroyWhileSending();
    void testIsConnecting();
    void testCanSendBeacon();
    void testCannotSendBeacon();
    void testConnectivityChangeTriggerBeacon();
    void testOnNoBeaconTriggersShutdown();
    void testShutdownWhileNegotiating();

    CPPUNIT_TEST_SUITE(ConnectionManagerTest);
    CPPUNIT_TEST(testConnectDevice);
    CPPUNIT_TEST(testAcceptConnection);
    CPPUNIT_TEST(testMultipleChannels);
    CPPUNIT_TEST(testMultipleChannelsOneDeclined);
    CPPUNIT_TEST(testMultipleChannelsSameName);
    CPPUNIT_TEST(testDeclineConnection);
    CPPUNIT_TEST(testSendReceiveData);
    CPPUNIT_TEST(testAcceptsICERequest);
    CPPUNIT_TEST(testDeclineICERequest);
    CPPUNIT_TEST(testChannelRcvShutdown);
    CPPUNIT_TEST(testChannelSenderShutdown);
    CPPUNIT_TEST(testCloseConnectionWith);
    CPPUNIT_TEST(testShutdownCallbacks);
    CPPUNIT_TEST(testFloodSocket);
    CPPUNIT_TEST(testDestroyWhileSending);
    CPPUNIT_TEST(testIsConnecting);
    CPPUNIT_TEST(testCanSendBeacon);
    CPPUNIT_TEST(testCannotSendBeacon);
    CPPUNIT_TEST(testConnectivityChangeTriggerBeacon);
    CPPUNIT_TEST(testOnNoBeaconTriggersShutdown);
    CPPUNIT_TEST(testShutdownWhileNegotiating);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConnectionManagerTest, ConnectionManagerTest::name());

void
ConnectionManagerTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
ConnectionManagerTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
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
        [&successfullyReceive, &cvReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                                           const std::string& name) {
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
    CPPUNIT_ASSERT(cvReceive.wait_for(lk, 60s, [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return successfullyConnected; }));
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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string& name) {
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
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
        [](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });

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

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
        return successfullyConnected && successfullyConnected2 && receiverConnected == 2;
    }));
    CPPUNIT_ASSERT(aliceAccount->connectionManager().activeSockets() == 1);
}

void
ConnectionManagerTest::testMultipleChannelsOneDeclined()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyNotConnected = false;
    bool successfullyConnected2 = false;
    int receiverConnected = 0;

    bobAccount->connectionManager().onChannelRequest(
        [](const std::shared_ptr<dht::crypto::Certificate>&, const std::string& name) {
            if (name == "git://*")
                return false;
            return true;
        });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId&, const std::string&, std::shared_ptr<ChannelSocket> socket) {
            if (socket)
                receiverConnected += 1;
            cv.notify_one();
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (!socket)
                                                            successfullyNotConnected = true;
                                                        cv.notify_one();
                                                    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "sip://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket)
                                                            successfullyConnected2 = true;
                                                        cv.notify_one();
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
        return successfullyNotConnected && successfullyConnected2 && receiverConnected == 1;
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
        [](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });

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

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string&) {
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string&) {
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
    cv.wait_for(lk, 30s);
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
        [](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });
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

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
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
        [](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });
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

    cv.wait_for(lk, 30s);
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(!receiverConnected);
    CPPUNIT_ASSERT(!successfullyConnected);
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
    std::condition_variable cv;
    bool successfullyConnected = false;
    bool shutdownReceived = false;

    std::shared_ptr<ChannelSocket> bobSock;

    bobAccount->connectionManager().onChannelRequest(
        [](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });

    bobAccount->connectionManager().onConnectionReady(
        [&](const DeviceId& did, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
            if (socket && name == "git://*" && did != bobDeviceId) {
                bobSock = socket;
                cv.notify_one();
            }
        });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        if (socket) {
                                                            socket->onShutdown([&] {
                                                                shutdownReceived = true;
                                                                cv.notify_one();
                                                            });
                                                            successfullyConnected = true;
                                                            cv.notify_one();
                                                        }
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return bobSock && successfullyConnected; }));
    bobSock->shutdown();
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return shutdownReceived; }));
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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string& name) {
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

    rcv.wait_for(lk, 30s);
    scv.wait_for(lk, 30s);
    CPPUNIT_ASSERT(shutdownReceived);
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);
}

void
ConnectionManagerTest::testCloseConnectionWith()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    auto bobUri = bobAccount->getUsername();

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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string& name) {
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

    rcv.wait_for(lk, 30s);
    // This should trigger onShutdown
    aliceAccount->connectionManager().closeConnectionsWith(bobUri);
    CPPUNIT_ASSERT(scv.wait_for(lk, 60s, [&] {
        return events == 2 && successfullyReceive && successfullyConnected && receiverConnected;
    }));
}

void
ConnectionManagerTest::testShutdownCallbacks()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));
    auto aliceUri = aliceAccount->getUsername();

    bobAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });
    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable rcv, chan2cv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;

    bobAccount->connectionManager().onChannelRequest(
        [&successfullyReceive, &chan2cv](const std::shared_ptr<dht::crypto::Certificate>&,
                                         const std::string& name) {
            if (name == "1") {
                successfullyReceive = true;
            } else {
                chan2cv.notify_one();
                // Do not return directly. Let the connection be closed
                std::this_thread::sleep_for(10s);
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
    CPPUNIT_ASSERT(rcv.wait_for(lk, 30s, [&] {
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
    chan2cv.wait_for(lk, 30s);

    // This should trigger onShutdown for second callback
    bobAccount->connectionManager().closeConnectionsWith(aliceUri);
    CPPUNIT_ASSERT(rcv.wait_for(lk, 30s, [&] { return channel2NotConnected; }));
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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string& name) {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return successfullyConnected && receiverConnected; }));
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return successfullyConnected && receiverConnected; }));
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
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
        [&successfullyReceive](const std::shared_ptr<dht::crypto::Certificate>&,
                               const std::string& name) {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return successfullyConnected && receiverConnected; }));
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return successfullyConnected && receiverConnected; }));
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

    bobAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) {
            successfullyReceive = true;
            cv.notify_one();
            std::this_thread::sleep_for(2s);
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return successfullyReceive; }));
    CPPUNIT_ASSERT(aliceAccount->connectionManager().isConnecting(bobDeviceId, "sip"));
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return successfullyConnected; }));
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
    bool successfullyConnected = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceSocket && bobSocket && successfullyConnected; }));
    CPPUNIT_ASSERT(aliceSocket->canSendBeacon());

    // Because onConnectionReady is true before version is sent, we can wait a bit
    // before canSendBeacon is true.
    auto start = std::chrono::steady_clock::now();
    auto aliceCanSendBeacon = false;
    auto bobCanSendBeacon = false;
    do {
        aliceCanSendBeacon = aliceSocket->canSendBeacon();
        bobCanSendBeacon = bobSocket->canSendBeacon();
        if (!bobCanSendBeacon || !aliceCanSendBeacon)
            std::this_thread::sleep_for(1s);
    } while ((not bobCanSendBeacon or not aliceCanSendBeacon)
             and std::chrono::steady_clock::now() - start < 5s);

    CPPUNIT_ASSERT(bobCanSendBeacon && aliceCanSendBeacon);
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
    bool successfullyConnected = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceSocket && bobSocket; }));

    int version = 1412;
    bobSocket->setOnVersionCb([&](auto v) {
        version = v;
        cv.notify_one();
    });
    aliceSocket->setVersion(0);
    aliceSocket->sendVersion();
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return version == 0; }));
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
    bool successfullyConnected = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceSocket && bobSocket; }));

    bool hasRequest = false;
    bobSocket->setOnBeaconCb([&](auto p) {
        if (p)
            hasRequest = true;
        cv.notify_one();
    });
    aliceAccount->connectionManager().connectivityChanged();
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return hasRequest; }));
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
    bool successfullyConnected = false;

    std::shared_ptr<MultiplexedSocket> aliceSocket, bobSocket;
    bobAccount->connectionManager().onChannelRequest(
        [&](const std::shared_ptr<dht::crypto::Certificate>&, const std::string&) { return true; });
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceSocket && bobSocket; }));

    bool isClosed = false;
    aliceSocket->onShutdown([&] {
        isClosed = true;
        cv.notify_one();
    });
    bobSocket->answerToBeacon(false);
    aliceAccount->connectionManager().connectivityChanged();
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return isClosed; }));
}

void
ConnectionManagerTest::testShutdownWhileNegotiating()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = DeviceId(std::string(bobAccount->currentDeviceId()));

    aliceAccount->connectionManager().onICERequest([](const DeviceId&) { return true; });

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool successfullyReceive = false;
    bool notConnected = false;

    bobAccount->connectionManager().onICERequest([&](const DeviceId&) {
        successfullyReceive = true;
        cv.notify_one();
        return true;
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId,
                                                    "git://*",
                                                    [&](std::shared_ptr<ChannelSocket> socket,
                                                        const DeviceId&) {
                                                        notConnected = !socket;
                                                        cv.notify_one();
                                                    });

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return successfullyReceive; }));
    Manager::instance().setAccountActive(aliceId, false, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return notConnected; }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConnectionManagerTest::name())
