/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../test_runner.h"
#include "../common.h"
#include "account_const.h"
#include "jami.h"
#include "jami/account_const.h"
#include "jami/configurationmanager_interface.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/service_manager.h"
#include "manager.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

/**
 * Minimal blocking TCP "echo" server bound to a free local port. Used to
 * stand in for the user-provided service that JamiAccount exposes through
 * the SVC_TUNNEL channel handler.
 */
class TinyEchoServer
{
public:
    TinyEchoServer()
    {
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        CPPUNIT_ASSERT(listenFd_ >= 0);
        int one = 1;
        ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        CPPUNIT_ASSERT(::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        socklen_t alen = sizeof(addr);
        CPPUNIT_ASSERT(::getsockname(listenFd_, reinterpret_cast<sockaddr*>(&addr), &alen) == 0);
        port_ = ntohs(addr.sin_port);
        CPPUNIT_ASSERT(::listen(listenFd_, 4) == 0);
        thread_ = std::thread([this] { run(); });
    }

    ~TinyEchoServer()
    {
        stop();
    }

    void stop()
    {
        if (running_.exchange(false) && listenFd_ >= 0) {
            ::shutdown(listenFd_, SHUT_RDWR);
            ::close(listenFd_);
            listenFd_ = -1;
        }
        if (thread_.joinable())
            thread_.join();
    }

    uint16_t port() const { return port_; }

private:
    void run()
    {
        running_ = true;
        while (running_) {
            int c = ::accept(listenFd_, nullptr, nullptr);
            if (c < 0)
                break;
            std::thread([c] {
                char buf[4096];
                while (true) {
                    ssize_t n = ::recv(c, buf, sizeof(buf), 0);
                    if (n <= 0)
                        break;
                    ssize_t off = 0;
                    while (off < n) {
                        ssize_t w = ::send(c, buf + off, n - off, 0);
                        if (w <= 0) {
                            off = n;
                            break;
                        }
                        off += w;
                    }
                }
                ::close(c);
            }).detach();
        }
    }

    int listenFd_ {-1};
    uint16_t port_ {0};
    std::thread thread_;
    std::atomic_bool running_ {false};
};

/**
 * End-to-end test for the network-services feature:
 *  - Alice and Bob become mutual contacts.
 *  - Alice exposes a TCP echo service (PUBLIC policy).
 *  - Bob queries Alice's services through queryPeerServices(), waiting for
 *    `PeerServicesReceived` to fire with the matching requestId.
 *  - Bob calls openServiceTunnel() and connects a local TCP socket to the
 *    bound loopback port; bytes are round-tripped through Alice's daemon
 *    and the echo server.
 */
class ServiceIntegrationTest : public CppUnit::TestFixture
{
public:
    ServiceIntegrationTest()
    {
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ServiceIntegrationTest() { libjami::fini(); }

    static std::string name() { return "ServiceIntegration"; }
    void setUp() override;
    void tearDown() override;

private:
    void testQueryAndTunnelEcho();

    CPPUNIT_TEST_SUITE(ServiceIntegrationTest);
    CPPUNIT_TEST(testQueryAndTunnelEcho);
    CPPUNIT_TEST_SUITE_END();

    std::string aliceId;
    std::string bobId;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ServiceIntegrationTest, ServiceIntegrationTest::name());

void
ServiceIntegrationTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
ServiceIntegrationTest::tearDown()
{
    libjami::unregisterSignalHandlers();
    handlers_.clear();
    wait_for_removal_of({aliceId, bobId});
}

void
ServiceIntegrationTest::testQueryAndTunnelEcho()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bob = Manager::instance().getAccount<JamiAccount>(bobId);
    CPPUNIT_ASSERT(alice && bob);
    auto aliceUri = alice->getUsername();
    auto bobUri = bob->getUsername();

    // --- Establish a confirmed contact relationship in both directions ---
    bool bobReceivedRequest = false;
    bool aliceContactAdded = false;
    bool bobContactAdded = false;
    handlers_.insert(libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& accountId,
            const std::string& /*from*/,
            const std::string& /*conversationId*/,
            const std::vector<uint8_t>& /*payload*/,
            time_t /*received*/) {
            std::lock_guard lk(mtx_);
            if (accountId == bobId)
                bobReceivedRequest = true;
            cv_.notify_all();
        }));
    handlers_.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& accountId, const std::string& /*uri*/, bool confirmed) {
            std::lock_guard lk(mtx_);
            if (confirmed) {
                if (accountId == aliceId)
                    aliceContactAdded = true;
                if (accountId == bobId)
                    bobContactAdded = true;
            }
            cv_.notify_all();
        }));

    // --- Wire up the service-discovery + tunnel signals we care about ---
    std::atomic<uint32_t> receivedReqId {0};
    std::string receivedJson;
    std::string receivedPeer;
    handlers_.insert(libjami::exportable_callback<libjami::ServiceSignal::PeerServicesReceived>(
        [&](uint32_t requestId,
            const std::string& accountId,
            const std::string& peerId,
            const std::string& servicesJson) {
            if (accountId != bobId)
                return;
            std::lock_guard lk(mtx_);
            receivedReqId = requestId;
            receivedJson = servicesJson;
            receivedPeer = peerId;
            cv_.notify_all();
        }));

    std::string openedTunnelId;
    uint16_t openedLocalPort = 0;
    handlers_.insert(libjami::exportable_callback<libjami::ServiceSignal::TunnelOpened>(
        [&](const std::string& accountId, const std::string& tunnelId, uint16_t localPort) {
            if (accountId != bobId)
                return;
            std::lock_guard lk(mtx_);
            openedTunnelId = tunnelId;
            openedLocalPort = localPort;
            cv_.notify_all();
        }));

    libjami::registerSignalHandlers(handlers_);

    alice->addContact(bobUri);
    alice->sendTrustRequest(bobUri, {});
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 60s, [&] { return bobReceivedRequest; }));
    }
    CPPUNIT_ASSERT(bob->acceptTrustRequest(aliceUri));
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 60s, [&] { return aliceContactAdded && bobContactAdded; }));
    }

    // --- Alice exposes a local echo service (PUBLIC so policy is moot) ---
    TinyEchoServer echo;
    ServiceRecord rec;
    rec.name = "echo";
    rec.description = "test echo service";
    rec.localHost = "127.0.0.1";
    rec.localPort = echo.port();
    rec.policy = AccessPolicy::PUBLIC;
    auto serviceId = alice->serviceManager().addService(rec);
    CPPUNIT_ASSERT(!serviceId.empty());

    // --- Bob discovers Alice's services ---
    auto reqId = libjami::queryPeerServices(bobId, aliceUri);
    CPPUNIT_ASSERT(reqId != 0);
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 60s, [&] { return receivedReqId == reqId; }));
    }
    CPPUNIT_ASSERT_EQUAL(aliceUri, receivedPeer);
    CPPUNIT_ASSERT(receivedJson.find(serviceId) != std::string::npos);
    CPPUNIT_ASSERT(receivedJson.find("\"name\":\"echo\"") != std::string::npos);

    // --- Bob opens a tunnel to that service on a free local port ---
    auto deviceId = std::string(alice->currentDeviceId());
    auto tunnelId = libjami::openServiceTunnel(bobId, aliceUri, deviceId, serviceId, "echo", 0);
    CPPUNIT_ASSERT(!tunnelId.empty());
    {
        std::unique_lock lk(mtx_);
        CPPUNIT_ASSERT(cv_.wait_for(lk, 5s, [&] { return openedTunnelId == tunnelId; }));
    }
    CPPUNIT_ASSERT(openedLocalPort != 0);

    // --- Connect a TCP client to the local tunnel and round-trip data ---
    int client = ::socket(AF_INET, SOCK_STREAM, 0);
    CPPUNIT_ASSERT(client >= 0);
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(openedLocalPort);
    CPPUNIT_ASSERT(::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    const char* hello = "ping";
    CPPUNIT_ASSERT_EQUAL(ssize_t {4}, ::send(client, hello, 4, 0));

    char buf[16] {};
    // The first read may need a few retries while the dhtnet channel finishes
    // its handshake on the alice side.
    ssize_t n = 0;
    auto until = std::chrono::steady_clock::now() + 30s;
    while (n < 4 && std::chrono::steady_clock::now() < until) {
        ssize_t r = ::recv(client, buf + n, 4 - n, 0);
        if (r > 0)
            n += r;
        else if (r == 0)
            break;
        else
            std::this_thread::sleep_for(50ms);
    }
    ::close(client);
    CPPUNIT_ASSERT_EQUAL(ssize_t {4}, n);
    CPPUNIT_ASSERT_EQUAL(std::string("ping"), std::string(buf, 4));

    CPPUNIT_ASSERT(libjami::closeServiceTunnel(bobId, tunnelId));
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::ServiceIntegrationTest::name())
