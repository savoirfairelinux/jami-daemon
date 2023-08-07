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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>

#include "manager.h"
#include "opendht/dhtrunner.h"
#include "opendht/thread_pool.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"

#include <dhtnet/ice_transport.h>
#include <dhtnet/ice_transport_factory.h>

using namespace libjami::Account;

namespace jami {
namespace test {

class IceTest : public CppUnit::TestFixture
{
public:
    IceTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~IceTest() { libjami::fini(); }
    static std::string name() { return "Ice"; }
    void setUp();
    void tearDown();

    // For future tests with publicIp
    std::shared_ptr<dht::DhtRunner> dht_ {};
    std::unique_ptr<dhtnet::IpAddr> turnV4_ {};

private:
    void testRawIceConnection();
    void testTurnMasterIceConnection();
    void testTurnSlaveIceConnection();
    void testReceiveTooManyCandidates();
    void testCompleteOnFailure();

    CPPUNIT_TEST_SUITE(IceTest);
    CPPUNIT_TEST(testRawIceConnection);
    CPPUNIT_TEST(testTurnMasterIceConnection);
    CPPUNIT_TEST(testTurnSlaveIceConnection);
    CPPUNIT_TEST(testReceiveTooManyCandidates);
    CPPUNIT_TEST(testCompleteOnFailure);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(IceTest, IceTest::name());

void
IceTest::setUp()
{
    if (!dht_) {
        dht_ = std::make_shared<dht::DhtRunner>();
        dht::DhtRunner::Config config {};
        dht::DhtRunner::Context context {};
        dht_->run(0, config, std::move(context));
        dht_->bootstrap("bootstrap.jami.net:4222");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    if (!turnV4_) {
        turnV4_ = std::make_unique<dhtnet::IpAddr>("turn.jami.net", AF_INET);
    }
}

void
IceTest::tearDown()
{}

void
IceTest::testRawIceConnection()
{
    dhtnet::IceTransportOptions ice_config;
    ice_config.upnpEnable = true;
    ice_config.tcpEnable = true;
    std::shared_ptr<dhtnet::IceTransport> ice_master, ice_slave;
    std::mutex mtx, mtx_create, mtx_resp, mtx_init;
    std::unique_lock<std::mutex> lk {mtx}, lk_create {mtx_create}, lk_resp {mtx_resp},
        lk_init {mtx_init};
    std::condition_variable cv, cv_create, cv_resp, cv_init;
    std::string init = {};
    std::string response = {};
    bool iceMasterReady = false, iceSlaveReady = false;
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_master != nullptr;
            }));
            auto iceAttributes = ice_master->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_master->getLocalCandidates(1)) {
                icemsg << addr << "\n";
                JAMI_DBG() << "Added local ICE candidate " << addr;
            }
            init = icemsg.str();
            cv_init.notify_one();
            CPPUNIT_ASSERT(cv_resp.wait_for(lk_resp, std::chrono::seconds(10), [&] {
                return !response.empty();
            }));
            auto sdp = ice_master->parseIceCandidates(response);
            CPPUNIT_ASSERT(
                ice_master->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceMasterReady = ok;
        cv.notify_one();
    };
    ice_config.master = true;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();

    ice_master = Manager::instance().getIceTransportFactory().createTransport("master ICE");
    ice_master->initIceInstance(ice_config);
    cv_create.notify_all();
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_slave != nullptr;
            }));
            auto iceAttributes = ice_slave->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_slave->getLocalCandidates(1)) {
                icemsg << addr << "\n";
                JAMI_DBG() << "Added local ICE candidate " << addr;
            }
            response = icemsg.str();
            cv_resp.notify_one();
            CPPUNIT_ASSERT(
                cv_init.wait_for(lk_resp, std::chrono::seconds(10), [&] { return !init.empty(); }));
            auto sdp = ice_slave->parseIceCandidates(init);
            CPPUNIT_ASSERT(
                ice_slave->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceSlaveReady = ok;
        cv.notify_one();
    };
    ice_config.master = false;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();

    ice_slave = Manager::instance().getIceTransportFactory().createTransport("slave ICE");
    ice_slave->initIceInstance(ice_config);

    cv_create.notify_all();
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(10), [&] { return iceMasterReady && iceSlaveReady; }));
}

void
IceTest::testTurnMasterIceConnection()
{
    const auto& addr4 = dht_->getPublicAddress(AF_INET);
    CPPUNIT_ASSERT(addr4.size() != 0);
    CPPUNIT_ASSERT(turnV4_);
    dhtnet::IceTransportOptions ice_config;
    ice_config.upnpEnable = true;
    ice_config.tcpEnable = true;
    std::shared_ptr<dhtnet::IceTransport> ice_master, ice_slave;
    std::mutex mtx, mtx_create, mtx_resp, mtx_init;
    std::unique_lock<std::mutex> lk {mtx}, lk_create {mtx_create}, lk_resp {mtx_resp},
        lk_init {mtx_init};
    std::condition_variable cv, cv_create, cv_resp, cv_init;
    std::string init = {};
    std::string response = {};
    bool iceMasterReady = false, iceSlaveReady = false;
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_master != nullptr;
            }));
            auto iceAttributes = ice_master->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_master->getLocalCandidates(1)) {
                if (addr.find("host") == std::string::npos) {
                    // We only want to add relayed + public ip
                    icemsg << addr << "\n";
                    JAMI_DBG() << "Added local ICE candidate " << addr;
                } else {
                    // Replace host by non existing IP (we still need host to not fail the start)
                    std::regex e("((?:[0-9]{1,3}\\.){3}[0-9]{1,3})");
                    auto newaddr = std::regex_replace(addr, e, "100.100.100.100");
                    if (newaddr != addr)
                        icemsg << newaddr << "\n";
                }
            }
            init = icemsg.str();
            cv_init.notify_one();
            CPPUNIT_ASSERT(cv_resp.wait_for(lk_resp, std::chrono::seconds(10), [&] {
                return !response.empty();
            }));

            auto sdp = ice_master->parseIceCandidates(response);
            CPPUNIT_ASSERT(
                ice_master->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceMasterReady = ok;
        cv.notify_one();
    };
    ice_config.accountPublicAddr = dhtnet::IpAddr(*addr4[0].get());
    ice_config.accountLocalAddr = dhtnet::ip_utils::getLocalAddr(AF_INET);
    ice_config.turnServers.emplace_back(dhtnet::TurnServerInfo()
                                            .setUri(turnV4_->toString(true))
                                            .setUsername("ring")
                                            .setPassword("ring")
                                            .setRealm("ring"));
    ice_config.master = true;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();
    ice_master = Manager::instance().getIceTransportFactory().createTransport("master ICE");
    ice_master->initIceInstance(ice_config);
    cv_create.notify_all();
    ice_config.turnServers = {};
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_slave != nullptr;
            }));
            auto iceAttributes = ice_slave->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_slave->getLocalCandidates(1)) {
                if (addr.find("host") == std::string::npos) {
                    // We only want to add relayed + public ip
                    icemsg << addr << "\n";
                    JAMI_DBG() << "Added local ICE candidate " << addr;
                } else {
                    // Replace host by non existing IP (we still need host to not fail the start)
                    std::regex e("((?:[0-9]{1,3}\\.){3}[0-9]{1,3})");
                    auto newaddr = std::regex_replace(addr, e, "100.100.100.100");
                    if (newaddr != addr)
                        icemsg << newaddr << "\n";
                }
            }
            response = icemsg.str();
            cv_resp.notify_one();
            CPPUNIT_ASSERT(
                cv_init.wait_for(lk_resp, std::chrono::seconds(10), [&] { return !init.empty(); }));
            auto sdp = ice_slave->parseIceCandidates(init);
            CPPUNIT_ASSERT(
                ice_slave->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceSlaveReady = ok;
        cv.notify_one();
    };
    ice_config.master = false;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();
    ice_slave = Manager::instance().getIceTransportFactory().createTransport("slave ICE");
    ice_slave->initIceInstance(ice_config);

    cv_create.notify_all();
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(10), [&] { return iceMasterReady && iceSlaveReady; }));
    CPPUNIT_ASSERT(ice_master->getLocalAddress(1).toString(false) == turnV4_->toString(false));
}

void
IceTest::testTurnSlaveIceConnection()
{
    const auto& addr4 = dht_->getPublicAddress(AF_INET);
    CPPUNIT_ASSERT(addr4.size() != 0);
    CPPUNIT_ASSERT(turnV4_);
    dhtnet::IceTransportOptions ice_config;
    ice_config.upnpEnable = true;
    ice_config.tcpEnable = true;
    std::shared_ptr<dhtnet::IceTransport> ice_master, ice_slave;
    std::mutex mtx, mtx_create, mtx_resp, mtx_init;
    std::unique_lock<std::mutex> lk {mtx}, lk_create {mtx_create}, lk_resp {mtx_resp},
        lk_init {mtx_init};
    std::condition_variable cv, cv_create, cv_resp, cv_init;
    std::string init = {};
    std::string response = {};
    bool iceMasterReady = false, iceSlaveReady = false;
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_master != nullptr;
            }));
            auto iceAttributes = ice_master->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_master->getLocalCandidates(1)) {
                if (addr.find("host") == std::string::npos) {
                    // We only want to add relayed + public ip
                    icemsg << addr << "\n";
                    JAMI_DBG() << "Added local ICE candidate " << addr;
                } else {
                    // Replace host by non existing IP (we still need host to not fail the start)
                    std::regex e("((?:[0-9]{1,3}\\.){3}[0-9]{1,3})");
                    auto newaddr = std::regex_replace(addr, e, "100.100.100.100");
                    if (newaddr != addr)
                        icemsg << newaddr << "\n";
                }
            }
            init = icemsg.str();
            cv_init.notify_one();
            CPPUNIT_ASSERT(cv_resp.wait_for(lk_resp, std::chrono::seconds(10), [&] {
                return !response.empty();
            }));
            auto sdp = ice_master->parseIceCandidates(response);
            CPPUNIT_ASSERT(
                ice_master->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceMasterReady = ok;
        cv.notify_one();
    };
    ice_config.accountPublicAddr = dhtnet::IpAddr(*addr4[0].get());
    ice_config.accountLocalAddr = dhtnet::ip_utils::getLocalAddr(AF_INET);
    ice_config.master = true;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();
    ice_master = Manager::instance().getIceTransportFactory().createTransport("master ICE");
    ice_master->initIceInstance(ice_config);
    cv_create.notify_all();
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_slave != nullptr;
            }));
            auto iceAttributes = ice_slave->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_slave->getLocalCandidates(1)) {
                if (addr.find("host") == std::string::npos) {
                    // We only want to add relayed + public ip
                    icemsg << addr << "\n";
                    JAMI_DBG() << "Added local ICE candidate " << addr;
                } else {
                    // Replace host by non existing IP (we still need host to not fail the start)
                    std::regex e("((?:[0-9]{1,3}\\.){3}[0-9]{1,3})");
                    auto newaddr = std::regex_replace(addr, e, "100.100.100.100");
                    if (newaddr != addr)
                        icemsg << newaddr << "\n";
                }
            }
            response = icemsg.str();
            cv_resp.notify_one();
            CPPUNIT_ASSERT(
                cv_init.wait_for(lk_resp, std::chrono::seconds(10), [&] { return !init.empty(); }));
            auto sdp = ice_slave->parseIceCandidates(init);
            CPPUNIT_ASSERT(
                ice_slave->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceSlaveReady = ok;
        cv.notify_one();
    };
    ice_config.turnServers.emplace_back(dhtnet::TurnServerInfo()
                                            .setUri(turnV4_->toString(true))
                                            .setUsername("ring")
                                            .setPassword("ring")
                                            .setRealm("ring"));
    ice_config.master = false;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();
    ice_slave = Manager::instance().getIceTransportFactory().createTransport("slave ICE");
    ice_slave->initIceInstance(ice_config);
    cv_create.notify_all();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(10), [&] { return iceMasterReady && iceSlaveReady; }));
    CPPUNIT_ASSERT(ice_slave->getLocalAddress(1).toString(false) == turnV4_->toString(false));
}

void
IceTest::testReceiveTooManyCandidates()
{
    const auto& addr4 = dht_->getPublicAddress(AF_INET);
    CPPUNIT_ASSERT(addr4.size() != 0);
    CPPUNIT_ASSERT(turnV4_);
    dhtnet::IceTransportOptions ice_config;
    ice_config.upnpEnable = true;
    ice_config.tcpEnable = true;
    std::shared_ptr<dhtnet::IceTransport> ice_master, ice_slave;
    std::mutex mtx, mtx_create, mtx_resp, mtx_init;
    std::unique_lock<std::mutex> lk {mtx}, lk_create {mtx_create}, lk_resp {mtx_resp},
        lk_init {mtx_init};
    std::condition_variable cv, cv_create, cv_resp, cv_init;
    std::string init = {};
    std::string response = {};
    bool iceMasterReady = false, iceSlaveReady = false;
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_master != nullptr;
            }));
            auto iceAttributes = ice_master->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_master->getLocalCandidates(1)) {
                icemsg << addr << "\n";
                JAMI_DBG() << "Added local ICE candidate " << addr;
            }
            init = icemsg.str();
            cv_init.notify_one();
            CPPUNIT_ASSERT(cv_resp.wait_for(lk_resp, std::chrono::seconds(10), [&] {
                return !response.empty();
            }));
            auto sdp = ice_master->parseIceCandidates(response);
            CPPUNIT_ASSERT(
                ice_master->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceMasterReady = ok;
        cv.notify_one();
    };
    ice_config.accountPublicAddr = dhtnet::IpAddr(*addr4[0].get());
    ice_config.accountLocalAddr = dhtnet::ip_utils::getLocalAddr(AF_INET);
    ice_config.turnServers.emplace_back(dhtnet::TurnServerInfo()
                                            .setUri(turnV4_->toString(true))
                                            .setUsername("ring")
                                            .setPassword("ring")
                                            .setRealm("ring"));
    ice_config.master = true;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();

    ice_master = Manager::instance().getIceTransportFactory().createTransport("master ICE");
    ice_master->initIceInstance(ice_config);
    cv_create.notify_all();
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_slave != nullptr;
            }));
            auto iceAttributes = ice_slave->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_master->getLocalCandidates(1)) {
                icemsg << addr << "\n";
                JAMI_DBG() << "Added local ICE candidate " << addr;
            }
            for (auto i = 0; i < std::min(256, PJ_ICE_ST_MAX_CAND); ++i) {
                icemsg << "Hc0a800a5 1 TCP 2130706431 192.168.0." << i
                       << " 43613 typ host tcptype passive"
                       << "\n";
                icemsg << "Hc0a800a5 1 TCP 2130706431 192.168.0." << i
                       << " 9 typ host tcptype active"
                       << "\n";
            }
            response = icemsg.str();
            cv_resp.notify_one();
            CPPUNIT_ASSERT(
                cv_init.wait_for(lk_resp, std::chrono::seconds(10), [&] { return !init.empty(); }));
            auto sdp = ice_slave->parseIceCandidates(init);
            CPPUNIT_ASSERT(
                ice_slave->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceSlaveReady = ok;
        cv.notify_one();
    };
    ice_config.master = false;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();

    ice_slave = Manager::instance().getIceTransportFactory().createTransport("slave ICE");
    ice_slave->initIceInstance(ice_config);
    cv_create.notify_all();
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(10), [&] { return iceMasterReady && iceSlaveReady; }));
}

void
IceTest::testCompleteOnFailure()
{
    const auto& addr4 = dht_->getPublicAddress(AF_INET);
    CPPUNIT_ASSERT(addr4.size() != 0);
    CPPUNIT_ASSERT(turnV4_);
    dhtnet::IceTransportOptions ice_config;
    ice_config.upnpEnable = true;
    ice_config.tcpEnable = true;
    std::shared_ptr<dhtnet::IceTransport> ice_master, ice_slave;
    std::mutex mtx, mtx_create, mtx_resp, mtx_init;
    std::unique_lock<std::mutex> lk {mtx}, lk_create {mtx_create}, lk_resp {mtx_resp},
        lk_init {mtx_init};
    std::condition_variable cv, cv_create, cv_resp, cv_init;
    std::string init = {};
    std::string response = {};
    bool iceMasterNotReady = false, iceSlaveNotReady = false;
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_master != nullptr;
            }));
            auto iceAttributes = ice_master->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_master->getLocalCandidates(1)) {
                if (addr.find("relay") != std::string::npos) {
                    // We only want to relayed and modify the rest (to have CONNREFUSED)
                    icemsg << addr << "\n";
                    JAMI_DBG() << "Added local ICE candidate " << addr;
                } else {
                    // Replace host by non existing IP (we still need host to not fail the start)
                    std::regex e("((?:[0-9]{1,3}\\.){3}[0-9]{1,3})");
                    auto newaddr = std::regex_replace(addr, e, "100.100.100.100");
                    if (newaddr != addr)
                        icemsg << newaddr << "\n";
                }
            }
            init = icemsg.str();
            cv_init.notify_one();
            CPPUNIT_ASSERT(cv_resp.wait_for(lk_resp, std::chrono::seconds(10), [&] {
                return !response.empty();
            }));
            auto sdp = ice_master->parseIceCandidates(response);
            CPPUNIT_ASSERT(
                ice_master->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceMasterNotReady = !ok;
        cv.notify_one();
    };
    ice_config.accountPublicAddr = dhtnet::IpAddr(*addr4[0].get());
    ice_config.accountLocalAddr = dhtnet::ip_utils::getLocalAddr(AF_INET);
    ice_config.master = true;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();
    ice_master = Manager::instance().getIceTransportFactory().createTransport("master ICE");
    ice_master->initIceInstance(ice_config);
    cv_create.notify_all();
    ice_config.onInitDone = [&](bool ok) {
        CPPUNIT_ASSERT(ok);
        dht::ThreadPool::io().run([&] {
            CPPUNIT_ASSERT(cv_create.wait_for(lk_create, std::chrono::seconds(10), [&] {
                return ice_slave != nullptr;
            }));
            auto iceAttributes = ice_slave->getLocalAttributes();
            std::stringstream icemsg;
            icemsg << iceAttributes.ufrag << "\n";
            icemsg << iceAttributes.pwd << "\n";
            for (const auto& addr : ice_slave->getLocalCandidates(1)) {
                if (addr.find("relay") != std::string::npos) {
                    // We only want to relayed and modify the rest (to have CONNREFUSED)
                    icemsg << addr << "\n";
                    JAMI_DBG() << "Added local ICE candidate " << addr;
                } else {
                    // Replace host by non existing IP (we still need host to not fail the start)
                    std::regex e("((?:[0-9]{1,3}\\.){3}[0-9]{1,3})");
                    auto newaddr = std::regex_replace(addr, e, "100.100.100.100");
                    if (newaddr != addr)
                        icemsg << newaddr << "\n";
                }
            }
            response = icemsg.str();
            cv_resp.notify_one();
            CPPUNIT_ASSERT(
                cv_init.wait_for(lk_resp, std::chrono::seconds(10), [&] { return !init.empty(); }));
            auto sdp = ice_slave->parseIceCandidates(init);
            CPPUNIT_ASSERT(
                ice_slave->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates)));
        });
    };
    ice_config.onNegoDone = [&](bool ok) {
        iceSlaveNotReady = !ok;
        cv.notify_one();
    };
    ice_config.turnServers.emplace_back(dhtnet::TurnServerInfo()
                                            .setUri(turnV4_->toString(true))
                                            .setUsername("ring")
                                            .setPassword("ring")
                                            .setRealm("ring"));
    ice_config.master = false;
    ice_config.streamsCount = 1;
    ice_config.compCountPerStream = 1;
    ice_config.upnpContext = Manager::instance().upnpContext();
    ice_config.factory = &Manager::instance().getIceTransportFactory();
    ice_slave = Manager::instance().getIceTransportFactory().createTransport("slave ICE");
    ice_slave->initIceInstance(ice_config);
    cv_create.notify_all();
    // Check that nego failed and callback called
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(120), [&] {
        return iceMasterNotReady && iceSlaveNotReady;
    }));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::IceTest::name())
