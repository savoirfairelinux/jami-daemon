/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *  Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>
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

#include "../../test_runner.h"
#include "jami.h"
#include "../common.h"
#include "jamidht/swarm/swarm_manager.h"
#include "jamidht/multiplexed_socket.h"

#include "opendht/infohash.h"
#include "peer_connection.h"

#include "nodes.h"

using namespace std::string_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

class RoutingTableTest : public CppUnit::TestFixture
{
public:
    RoutingTableTest();
    ~RoutingTableTest() { DRing::fini(); }
    static std::string name() { return "RoutingTable"; }

private:
    /*     std::shared_ptr<SwarmManager> sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(0));
        std::shared_ptr<SwarmManager> sm2 = std::make_shared<SwarmManager>(nodeTestIds2.at(0)); */

    void testBucketKnownNodes();
    void testSwarmManagerKnowNodes_1b();
    void testSwarmManagerKnowNodes_ConnectingNodes_1b();
    void testSwarmManagerKnowNodes_ConnectingNodes_1b2();
    void testBucketSplit();
    void testClosestNodes_1b();
    void testSendKnownNodes();

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    // CPPUNIT_TEST(testBucketKnownNodes);
    // CPPUNIT_TEST(testSwarmManagerKnowNodes_1b);
    // CPPUNIT_TEST(testSwarmManagerKnowNodes_ConnectingNodes_1b);
    // CPPUNIT_TEST(testSwarmManagerKnowNodes_ConnectingNodes_1b2);
    CPPUNIT_TEST(testBucketSplit);
    // CPPUNIT_TEST(testClosestNodes_1b);
    // CPPUNIT_TEST(testSendKnownNodes);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RoutingTableTest, RoutingTableTest::name());

RoutingTableTest::RoutingTableTest()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
}

void
RoutingTableTest::testBucketKnownNodes()
{
    std::cout << "\ntestBucketKnownNodes" << std::endl;
    Bucket bucket(nodeTestIds1.at(0));

    for (int i = 0; i < nodeTestIds1.size(); i++) {
        bucket.addKnownNode(nodeTestIds1.at(i));
    }

    NodeId rId = bucket.randomId();
    NodeId notNode("053927d831827a9f7e606d4c9aaae833922c0d35b3960dd2250085f46c0e4f42");

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have the known node", true, bucket.hasKnownNode(rId));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be in known nodes",
                                 false,
                                 bucket.hasConnectingNode(rId));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Node not supposed to exist in bucket",
                                 false,
                                 bucket.hasKnownNode(notNode));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Error with bucket size", true, bucket.getKnownNodesSize() == 9);
}

void
RoutingTableTest::testSwarmManagerKnowNodes_1b()
{
    std::cout << "\ntestSwarmManagerKnowNodes_1b" << std::endl;

    SwarmManager sm1(nodeTestIds1.at(0));
    sm1.setKnownNodes(nodeTestIds1);

    auto& r1 = sm1.getRoutingTable();
    NodeId node1 = nodeTestIds1.at(2);
    NodeId node2 = nodeTestIds1.at(6);
    NodeId notNode("053927d831827a9f7e606d4c9aaae833922c0d35b3960dd2250085f46c0e4f42");

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node1", false, r1.hasKnownNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node2", false, r1.hasKnownNode(node2));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be a connecting node1",
                                 true,
                                 r1.hasConnectingNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be a connecting node2",
                                 true,
                                 r1.hasConnectingNode(node2));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Node not supposed to exist in table",
                                 false,
                                 r1.hasKnownNode(notNode));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Node not supposed to exist in table",
                                 false,
                                 r1.hasConnectingNode(notNode));
}

void
RoutingTableTest::testSwarmManagerKnowNodes_ConnectingNodes_1b()
{
    std::cout << "\ntestSwarmManagerKnowNodes_ConnectingNodes_1b" << std::endl;
    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(0));

    std::vector<std::shared_ptr<ChannelSocketTest>> test;

    sm1->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        NodeId node = DeviceId(nodeId);
        bool toConnect = nodeTestisConnected.find(node)->second;

        if (toConnect) {
            auto cstRemote = std::make_shared<ChannelSocketTest>(node, "test1", 0);
            auto cstMe = std::make_shared<ChannelSocketTest>(sm1->getMyId(), "test1", 0);

            test.push_back(cstRemote);

            onSocket(cstRemote);
        }
    };

    sm1->setKnownNodes(nodeTestIds1);

    auto& r1 = sm1->getRoutingTable();

    NodeId node1_1 = nodeTestIds1.at(1);

    NodeId node1_2 = nodeTestIds1.at(7);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be connected to node",
                                 true,
                                 r1.hasNode(test.at(0))); // corresponds to node1_1
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be connected to node and not known node",
                                 false,
                                 r1.hasKnownNode(node1_1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be connected to node and not connecting node",
                                 false,
                                 r1.hasConnectingNode(node1_1));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be connecting node", false, r1.hasKnownNode(node1_2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be connecting node",
                                 true,
                                 r1.hasConnectingNode(node1_2));
}

void
RoutingTableTest::testSwarmManagerKnowNodes_ConnectingNodes_1b2()
{
    std::cout << "\ntestSwarmManagerKnowNodes_ConnectingNodes_1b2" << std::endl;

    /*     SwarmManager sm1(nodeTestIds1.at(0));

        sm1.needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
            auto cstRemote = std::make_shared<ChannelSocketTest>(DeviceId(nodeId), "test1", 0);
            // onSocket(cstRemote);
            /*         // Create ChannelSocketTest (socket)
                    auto& cstMe = channelSockets_[sm1.getMyId()][nodeId]; // map<NodeId, map<NodeId,
               shared<ChannelSocketTest>>> if (!cstMe) cstMe =
       std::make_shared<ChannelSocketTest>(); auto& cstRemote =
       channelSockets_[nodeId][sm1.getMyId()]; // map<NodeId, map<NodeId, shared<ChannelSocketTest>>>
       if (!cstRemote) { cstRemote = std::make_shared<ChannelSocketTest>();
                        //swarmManagerRemote->addSwarmChannel(cstRemote);
                    }
                    cstRemote->setPeer(cstMe);
                    cstMe->setPeer(cstRemote);

                    onSocket(cstRemote);
                    // cv.notify_all();
        };

        sm1.setKnownNodes(nodeTestIds1);

        auto& r1 = sm1.getRoutingTable(); */

    // Test if the routing tables contain first and last element knownNodes
    // to corresponding bucket.
    /*     CPPUNIT_ASSERT(!r1.hasKnownNode(node1_1));
        CPPUNIT_ASSERT(!r1.hasKnownNode(node1_2));
        CPPUNIT_ASSERT(r1.hasConnectingNode(node1_1));
        CPPUNIT_ASSERT(r1.hasConnectingNode(node1_2)); */

    // CPPUNIT_ASSERT(cv.wait_for(lk, 10s, r1.hasNode(socket)));
    // CPPUNIT_ASSERT(r1.hasNode(socket));

    CPPUNIT_ASSERT(true);
}

void
RoutingTableTest::testBucketSplit()
{
    std::cout << "\ntestBucketSplit" << std::endl;

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(0));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds2.at(0));
    /*     auto sm3 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));
     */

    sm1->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        NodeId node = DeviceId(nodeId);
        {
            std::lock_guard<std::mutex> lk(channelSocketsMtx_);
            auto& cstRemote = channelSockets_[node][sm1->getMyId()];
            auto& cstMe = channelSockets_[sm1->getMyId()][node];
            if (!cstRemote)
                cstRemote = std::make_shared<ChannelSocketTest>(sm1->getMyId(), "test1", 0);
            if (!cstMe)
                cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
            ChannelSocketTest::link(cstMe, cstRemote);
        }
        onSocket(channelSockets_[sm1->getMyId()][node]);
    };

    sm2->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        NodeId node = DeviceId(nodeId);
        {
            std::lock_guard<std::mutex> lk(channelSocketsMtx_);
            auto& cstRemote = channelSockets_[node][sm2->getMyId()];
            auto& cstMe = channelSockets_[sm2->getMyId()][node];
            if (!cstRemote)
                cstRemote = std::make_shared<ChannelSocketTest>(sm1->getMyId(), "test1", 0);
            if (!cstMe)
                cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
            ChannelSocketTest::link(cstMe, cstRemote);
        }
        onSocket(channelSockets_[sm2->getMyId()][node]);
    };

    sm1->setKnownNodes(nodeTestIds1);
    auto& r1 = sm1->getRoutingTable();
    std::cout << "\nSWARM MANAGER 1" << std::endl;
    r1.printRoutingTable();
    sm2->setKnownNodes(nodeTestIds2);
    auto& r2 = sm2->getRoutingTable();
    std::cout << "\nSWARM MANAGER 2" << std::endl;
    r2.printRoutingTable();

    /*     sm3->setKnownNodes(nodeTestIds3);
        auto& r3 = sm3->getRoutingTable();
        std::cout << "\nSWARM MANAGER 3" << std::endl;
        r3.printRoutingTable();
     */
    sm1->shutdown();
    sm2->shutdown();

    CPPUNIT_ASSERT(true);
}

void
RoutingTableTest::testClosestNodes_1b()
{
    std::cout << "\ntestClosestNodes_1b" << std::endl;

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(0));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds2.at(0));

    std::vector<std::shared_ptr<ChannelSocketTest>> test;

    sm1->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        NodeId node = DeviceId(nodeId);
        auto cstRemote = std::make_shared<ChannelSocketTest>(node, "test1", 0);
        auto cstMe = std::make_shared<ChannelSocketTest>(sm1->getMyId(), "test1", 0);
        test.push_back(cstRemote);
        ChannelSocketTest::link(cstMe, cstRemote);

        onSocket(cstRemote);
    };

    sm2->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        NodeId node = DeviceId(nodeId);
        auto cstRemote = std::make_shared<ChannelSocketTest>(node, "test1", 0);
        auto cstMe = std::make_shared<ChannelSocketTest>(sm2->getMyId(), "test1", 0);
        test.push_back(cstRemote);
        ChannelSocketTest::link(cstMe, cstRemote);

        onSocket(cstRemote);
    };

    sm1->setKnownNodes(nodeTestIds1);
    auto& r1 = sm1->getRoutingTable();

    sm2->setKnownNodes(nodeTestIds2);
    auto& r2 = sm2->getRoutingTable();

    std::vector<NodeId> closestr1 = r1.closestNodes(nodeTestIds2.at(2), 2);
    std::vector<NodeId> closestr2 = r2.closestNodes(nodeTestIds1.at(2), 2);
    /*
        std::cout << "\nTesting closest nodes" << std::endl;

        std::cout << " Closest two nodes in r1 to: " << nodeTestIds2.at(2) << std::endl;
        std::cout << closestr1[0] << std::endl;
        std::cout << closestr1[1] << std::endl;

        std::cout << " Closest two nodes in r2 to: " << nodeTestIds1.at(2) << std::endl;
        std::cout << closestr2[0] << std::endl;
        std::cout << closestr2[1] << std::endl; */

    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ERR1",
        NodeId("28f4c7e34eb4310b2e1ea3b139ee6993e6b021770ee98895a54cdd1e372bd78e"),
        closestr1[0]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ERR2",
        NodeId("2dd1dd976c7dc234ca737c85e4ea48ad09423067a77405254424c4cdd845720d"),
        closestr1[1]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ERR3",
        NodeId("4f76e769061f343b2caf9eea35632d28cde8d7a67e5e0f59857733cabc538997"),
        closestr2[0]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ERR4",
        NodeId("77a9fba2c5a65812d9290c567897131b20a723e0ca2f65ef5c6b421585e4da2b"),
        closestr2[1]);
}

void
RoutingTableTest::testSendKnownNodes()
{
    std::cout << "\ntestSendKnownNodes" << std::endl;

    /*     SwarmManager sm1(nodeTestIds1.at(0));
        SwarmManager sm2(nodeTestIds2.at(0)); */

    CPPUNIT_ASSERT(true);
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())
