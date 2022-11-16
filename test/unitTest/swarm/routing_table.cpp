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
#include "connectivity/multiplexed_socket.h"

#include "connectivity/peer_connection.h"
#include "nodes.h"

#include <opendht/thread_pool.h>

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

constexpr size_t smNumber = 100000;
constexpr size_t kNodes = 100000;
constexpr size_t BOOTSTRAP_SIZE = 3;
constexpr int time = 200;

struct Counter
{
    Counter(unsigned t)
        : target(t)
    {}
    const unsigned target;
    unsigned added {0};
    std::mutex mutex;
    std::condition_variable cv;

    void count()
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++added;
        if (added == target)
            cv.notify_one();
    }
    bool wait(std::chrono::steady_clock::duration timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [&] { return added == target; });
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait(lock, [&] { return added == target; });
    }
};

class RoutingTableTest : public CppUnit::TestFixture
{
public:
    ~RoutingTableTest() { libjami::fini(); }
    static std::string name() { return "RoutingTable"; }

    void setUp();
    void tearDown();

private:
    //################# METHODS AND VARIABLES GENERATING DATA #################//

    std::mt19937_64 rd {dht::crypto::getSeededRandomEngine<std::mt19937_64>()};
    std::mutex channelSocketsMtx_;
    std::vector<NodeId> randomNodeIds;
    std::map<NodeId, std::map<NodeId, std::shared_ptr<jami::ChannelSocketTest>>> channelSockets_;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagersRandom;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagers;
    std::map<NodeId, std::set<NodeId>> nodesToConnect;
    std::set<NodeId> messageNode;

    void generaterandomNodeIds();
    void generateSwarmManagers();
    void setKnownNodesToManager(const std::shared_ptr<SwarmManager>& sm);
    void needSocketCallBackAcceptAllRandom(const std::shared_ptr<SwarmManager>& sm);
    void needSocketCallBackAcceptAll(const std::shared_ptr<SwarmManager>& sm);

    //################# METHODS AND VARIABLES TO TEST DATA #################//

    std::map<std::shared_ptr<jami::SwarmManager>, std::vector<NodeId>> knownNodesSwarmManager;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagersTest_;
    std::set<NodeId> discoveredNodes;

    void crossNodes(NodeId nodeId);

    //################# UNIT TEST METHODES #################//

    void testBucketMainFunctions();
    void testRoutingTableMainFunctions();
    void testBucketKnownNodes();
    void testSwarmManagerConnectingNodes_1b();
    void testSwarmManagerKnowNodes_1b();
    void testClosestNodes_1b();
    void testClosestNodes_multipleb();
    void testSendKnownNodes_1b();
    void testSendKnownNodes_multipleb();
    void testBucketSplit_1n();
    void testSwarmManagers();
    void testSwarmManagersSmallBootstrapList();
    void testRoutingTableForConnectingNode();
    void testRoutingTableForShuttingNode();
    void testRoutingTableForMassShuttingsNodes();

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    // CPPUNIT_TEST(testSwarmManagerKnowNodes_1b);
    // CPPUNIT_TEST(testSwarmManagers);

    // GOOD TESTS
    /*     CPPUNIT_TEST(testBucketMainFunctions);
        CPPUNIT_TEST(testBucketSplit_1n);
        CPPUNIT_TEST(testBucketKnownNodes);
        CPPUNIT_TEST(testClosestNodes_1b);
        CPPUNIT_TEST(testClosestNodes_multipleb);
        CPPUNIT_TEST(testRoutingTableMainFunctions);
        CPPUNIT_TEST(testSwarmManagerConnectingNodes_1b);
        CPPUNIT_TEST(testSendKnownNodes_1b);
        CPPUNIT_TEST(testSendKnownNodes_multipleb);
        CPPUNIT_TEST(testSwarmManagersSmallBootstrapList);
        CPPUNIT_TEST(testRoutingTableForConnectingNode);
        CPPUNIT_TEST(testRoutingTableForShuttingNode);
        CPPUNIT_TEST(testRoutingTableForMassShuttingsNodes); */
    CPPUNIT_TEST(testSwarmManagersSmallBootstrapList);

    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RoutingTableTest, RoutingTableTest::name());

void
RoutingTableTest::setUp()
{
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized) {
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    generaterandomNodeIds();
    generateSwarmManagers();
}

void
RoutingTableTest::tearDown()
{
    discoveredNodes.clear();
    swarmManagersTest_.clear();
}

void
RoutingTableTest::generaterandomNodeIds()
{
    randomNodeIds.reserve(kNodes);
    for (size_t i = 0; i < kNodes; i++) {
        NodeId node = Hash<32>::getRandom();
        randomNodeIds.emplace_back(node);
    }
}

void
RoutingTableTest::generateSwarmManagers()
{
    for (size_t i = 0; i < kNodes; i++) {
        const NodeId& node = randomNodeIds.at(i);
        swarmManagersRandom[node] = std::make_shared<SwarmManager>(node);
    }
}

void
RoutingTableTest::setKnownNodesToManager(const std::shared_ptr<SwarmManager>& sm)
{
    std::uniform_int_distribution<> distrib(1, kNodes - 1);

    int numberKnownNodesToAdd = distrib(rd);

    std::uniform_int_distribution<> distribBis(0, kNodes - 1);
    int indexNodeIdToAdd;
    std::vector<NodeId> kNodesToAdd;
    knownNodesSwarmManager.insert({sm, {}});

    int counter = 0;

    while (counter < numberKnownNodesToAdd) {
        indexNodeIdToAdd = distribBis(rd);

        NodeId node = randomNodeIds.at(indexNodeIdToAdd);
        auto it = find(kNodesToAdd.begin(), kNodesToAdd.end(), node);
        if (sm->getId() != node && it == kNodesToAdd.end()) {
            kNodesToAdd.push_back(node);
            knownNodesSwarmManager.at(sm).push_back(node);
            counter++;
        }
    }

    sm->setKnownNodes(kNodesToAdd);
}

void
RoutingTableTest::needSocketCallBackAcceptAllRandom(const std::shared_ptr<SwarmManager>& sm)
{
    sm->needSocketCb_ = [this, wsm = std::weak_ptr<SwarmManager>(sm)](const std::string& nodeId,
                                                                      auto&& onSocket) {
        dht::ThreadPool::computation().run([this, wsm, nodeId, onSocket = std::move(onSocket)] {
            auto sm = wsm.lock();
            if (!sm)
                return;
            NodeId node = DeviceId(nodeId);
            if (auto smRemote = swarmManagersRandom[node]) {
                auto myId = sm->getId();
                std::unique_lock<std::mutex> lk(channelSocketsMtx_);
                auto& cstRemote = channelSockets_[node][myId];
                auto& cstMe = channelSockets_[myId][node];
                if (!cstRemote)
                    cstRemote = std::make_shared<ChannelSocketTest>(myId, "test1", 0);
                if (!cstMe)
                    cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
                lk.unlock();
                ChannelSocketTest::link(cstMe, cstRemote);
                onSocket(cstMe);
                smRemote->addChannel(cstRemote);
            };
        });
    };
};

void
RoutingTableTest::needSocketCallBackAcceptAll(const std::shared_ptr<SwarmManager>& sm)
{
    sm->needSocketCb_ = [this, wsm = std::weak_ptr<SwarmManager>(sm)](const std::string& nodeId,
                                                                      auto&& onSocket) {
        Manager::instance().ioContext()->post([this, wsm, nodeId, onSocket = std::move(onSocket)] {
            auto sm = wsm.lock();
            if (!sm)
                return;

            NodeId node = DeviceId(nodeId);
            if (auto smRemote = swarmManagers[node]) {
                auto myId = sm->getId();

                std::lock_guard<std::mutex> lk(channelSocketsMtx_);
                auto& cstRemote = channelSockets_[node][myId];
                auto& cstMe = channelSockets_[myId][node];
                if (!cstRemote) {
                    cstRemote = std::make_shared<ChannelSocketTest>(myId, "test1", 0);
                }
                if (!cstMe) {
                    cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
                }
                ChannelSocketTest::link(cstMe, cstRemote);

                onSocket(cstMe);
                smRemote->addChannel(cstRemote);
            };
        });
    };
}

void
RoutingTableTest::testBucketMainFunctions()
{
    NodeId node0 = nodeTestIds1.at(0);
    NodeId node1 = nodeTestIds1.at(1);
    NodeId node2 = nodeTestIds1.at(2);
    NodeId node3 = nodeTestIds1.at(3);

    auto sNode1 = nodeTestChannels1.at(1);
    auto sNode2 = nodeTestChannels1.at(2);
    auto sNode3 = nodeTestChannels1.at(3);

    std::set<std::shared_ptr<ChannelSocketInterface>> socketsCheck {sNode1, sNode2};
    std::set<NodeId> nodesCheck {node1, node2};

    Bucket bucket(node0);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Lower limit error", node0, bucket.getLowerLimit());

    bucket.addNode(sNode1);
    bucket.addNode(sNode2);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have node", true, bucket.hasNode(sNode1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have node",
                                 true,
                                 bucket.hasNodeId(sNode2->deviceId()));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes", true, socketsCheck == bucket.getNodes());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes", true, nodesCheck == bucket.getNodeIds());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes", true, bucket.isFull());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node",
                                 false,
                                 bucket.hasKnownNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node",
                                 false,
                                 bucket.hasKnownNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                 false,
                                 nodesCheck == bucket.getKnownNodes());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node2));

    bucket.deleteNode(sNode1);
    bucket.deleteNodeId(sNode2->deviceId());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(sNode1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(sNode2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", true, bucket.hasKnownNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", true, bucket.hasKnownNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", node2, bucket.getKnownNodeId(0));
    CPPUNIT_ASSERT_THROW(bucket.getKnownNodeId(10), std::out_of_range);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known nodes",
                                 true,
                                 nodesCheck == bucket.getKnownNodes());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node2));

    auto nodeTest = bucket.randomId(rd);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("One of the two nodes",
                                 true,
                                 nodeTest == node1 || nodeTest == node2);

    bucket.addNode(sNode1);
    bucket.addNode(sNode2);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be 2", 2u, bucket.getNodesSize());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to return zero, node already added",
                                 false,
                                 bucket.addNode(sNode2));

    bucket.deleteNode(sNode1);
    bucket.deleteNode(sNode2);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(sNode1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(sNode2));

    bucket.addKnownNode(node3);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", true, bucket.hasKnownNode(node3));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node3));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be 3", 3u, bucket.getKnownNodesSize());
    bucket.removeKnownNode(node3);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node",
                                 false,
                                 bucket.hasKnownNode(node3));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node3));

    bucket.addConnectingNode(node1);
    bucket.addConnectingNode(node2);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have connecting node",
                                 true,
                                 bucket.hasConnectingNode(node1));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes",
                                 true,
                                 nodesCheck == bucket.getConnectingNodes());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be 2", 2u, bucket.getConnectingNodesSize());

    bucket.removeConnectingNode(node2);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not upposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be 1", 1u, bucket.getConnectingNodesSize());
}

void
RoutingTableTest::testBucketKnownNodes()
{
    std::cout << "\ntestBucketKnownNodes" << std::endl;
    Bucket bucket(randomNodeIds.at(0));

    for (size_t i = 0; i < randomNodeIds.size(); i++) {
        bucket.addKnownNode(randomNodeIds.at(i));
    }

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have the known node",
                                 true,
                                 bucket.hasKnownNode(randomNodeIds.at(randomNodeIds.size() - 1)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Error with bucket size",
                                 true,
                                 bucket.getKnownNodesSize() == randomNodeIds.size());
}

void
RoutingTableTest::testRoutingTableMainFunctions()
{
    RoutingTable rt;
    NodeId node1 = nodeTestIds1.at(0);
    NodeId node2 = nodeTestIds1.at(1);
    NodeId node3 = nodeTestIds1.at(3);

    rt.setId(node1);

    rt.addKnownNode(node1);
    rt.addKnownNode(node2);

    CPPUNIT_ASSERT(!rt.hasKnownNode(node1));
    CPPUNIT_ASSERT(rt.hasKnownNode(node2));

    auto bucket1 = rt.findBucket(node1);
    auto bucket2 = rt.findBucket(node2);
    auto bucket3 = rt.findBucket(node3);

    rt.addNode(nodeTestChannels1.at(0), bucket1);
    rt.addNode(nodeTestChannels1.at(1), bucket2);
    rt.addNode(nodeTestChannels1.at(2), bucket3);

    CPPUNIT_ASSERT(!rt.hasNode(nodeTestChannels1.at(0)));
    CPPUNIT_ASSERT(rt.hasNode(nodeTestChannels1.at(1)));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist",
                                 false,
                                 rt.deleteNode(nodeTestChannels1.at(0)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist",
                                 true,
                                 rt.deleteNode(nodeTestChannels1.at(1)));

    rt.deleteNode(nodeTestChannels1.at(2));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist",
                                 false,
                                 rt.hasNode(nodeTestChannels1.at(2)));
}

void
RoutingTableTest::testSwarmManagerConnectingNodes_1b()
{
    std::cout << "\ntestSwarmManagerConnectingNodes_1b" << std::endl;

    SwarmManager sm1(nodeTestIds1.at(0));
    auto& rt1 = sm1.getRoutingTable();

    std::vector<NodeId> toTest(
        {NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41"),
         NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")});

    sm1.setKnownNodes(toTest);

    CPPUNIT_ASSERT(!rt1.hasConnectingNode(nodeTestIds1.at(0)));
    CPPUNIT_ASSERT(rt1.hasConnectingNode(nodeTestIds1.at(1)));
    CPPUNIT_ASSERT(!rt1.hasKnownNode(nodeTestIds1.at(0)));
    CPPUNIT_ASSERT(!rt1.hasKnownNode(nodeTestIds1.at(1)));
}

void
RoutingTableTest::testClosestNodes_1b()
{
    std::cout << "\ntestClosestNodes_1b" << std::endl;

    SwarmManager sm1(nodeTestIds1.at(0));
    SwarmManager sm2(nodeTestIds2.at(0));

    auto& rt1 = sm1.getRoutingTable();
    auto& rt2 = sm2.getRoutingTable();

    auto bucket1 = rt1.findBucket(nodeTestIds1.at(0));
    auto bucket2 = rt2.findBucket(nodeTestIds2.at(0));

    for (size_t i = 0; i < nodeTestIds2.size(); i++) {
        bucket1->addNode(nodeTestChannels1.at(i));
        bucket2->addNode(nodeTestChannels2.at(i));
    }

    std::vector<NodeId>
        closestNodes1 {NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8"),
                       NodeId("28f4c7e34eb4310b2e1ea3b139ee6993e6b021770ee98895a54cdd1e372bd78e"),
                       NodeId("2dd1dd976c7dc234ca737c85e4ea48ad09423067a77405254424c4cdd845720d"),
                       NodeId("33f280d8208f42ac34321e6e6871aecd100c2bfd4f1848482e7a7ed8ae895414")

        };

    std::vector<NodeId>
        closestNodes2 {NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41"),
                       NodeId("4f76e769061f343b2caf9eea35632d28cde8d7a67e5e0f59857733cabc538997"),
                       NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8"),
                       NodeId("77a9fba2c5a65812d9290c567897131b20a723e0ca2f65ef5c6b421585e4da2b")

        };

    auto closestNodes1_ = rt1.closestNodes(nodeTestIds2.at(4), 4);
    auto closestNodes2_ = rt2.closestNodes(nodeTestIds1.at(4), 4);
    auto sameIdTest = rt2.closestNodes(nodeTestIds2.at(0), 1);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, closestNodes1 == closestNodes1_);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, closestNodes2 == closestNodes2_);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, nodeTestIds1.at(0) == sameIdTest.at(0));
}

void
RoutingTableTest::testClosestNodes_multipleb()
{
    std::cout << "\ntestClosestNodes_multipleb" << std::endl;

    SwarmManager sm1(nodeTestIds1.at(2));
    SwarmManager sm2(nodeTestIds1.at(6));

    auto& rt1 = sm1.getRoutingTable();
    auto& rt2 = sm2.getRoutingTable();

    for (size_t i = 0; i < nodeTestIds1.size(); i++) {
        auto bucket1 = rt1.findBucket(nodeTestIds1.at(i));
        auto bucket2 = rt2.findBucket(nodeTestIds1.at(i));

        rt1.addNode(nodeTestChannels1.at(i), bucket1);
        rt2.addNode(nodeTestChannels1.at(i), bucket2);
    }

    std::vector<NodeId>
        closestNodes1 {NodeId("2dd1dd976c7dc234ca737c85e4ea48ad09423067a77405254424c4cdd845720d"),
                       NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41"),
                       NodeId("1bd92a8aab91e63267fd91c6ff4d88896bca4b69e422b11894881cd849fa1467")

        };

    std::vector<NodeId>
        closestNodes2 {NodeId("30e177a56bd1a7969e1973ad8b210a556f6a2b15debc972661a8f555d52edbe2"),
                       NodeId("312226d8fa653704758a681c8c21ec81cec914d0b8aa19e1142d3cf900e3f3b4"),
                       NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41")

        };
    auto closestNodes1_ = rt1.closestNodes(nodeTestIds1.at(5), 3);
    auto closestNodes2_ = rt2.closestNodes(nodeTestIds1.at(5), 3);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, closestNodes1 == closestNodes1_);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, closestNodes2 == closestNodes2_);
}

void
RoutingTableTest::testBucketSplit_1n()
{
    std::cout << "\ntestBucketSplit_1n" << std::endl;

    SwarmManager sm1(nodeTestIds2.at(0));
    SwarmManager sm2(nodeTestIds2.at(nodeTestIds2.size() - 1));
    SwarmManager sm3(nodeTestIds2.at(nodeTestIds2.size() / 2));

    auto& rt1 = sm1.getRoutingTable();
    auto& rt2 = sm2.getRoutingTable();
    auto& rt3 = sm3.getRoutingTable();

    auto& b1 = rt1.getBuckets();
    auto& b2 = rt2.getBuckets();
    auto& b3 = rt3.getBuckets();

    for (size_t i = 0; i < nodeTestIds2.size(); i++) {
        auto bucket1 = rt1.findBucket(nodeTestIds2.at(i));
        auto bucket2 = rt2.findBucket(nodeTestIds2.at(i));
        auto bucket3 = rt3.findBucket(nodeTestIds2.at(i));

        rt1.addNode(nodeTestChannels2.at(i), bucket1);
        rt2.addNode(nodeTestChannels2.at(i), bucket2);
        rt3.addNode(nodeTestChannels2.at(i), bucket3);
    }

    // SM1
    // Check if nodes don't exist
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt1.hasNode(nodeTestChannels2.at(0)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt1.hasNode(nodeTestChannels2.at(3)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt1.hasNode(nodeTestChannels2.at(4)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt1.hasNode(nodeTestChannels2.at(7)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt1.hasNode(nodeTestChannels2.at(8)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt1.hasNode(nodeTestChannels2.at(9)));

    // for sm1, supposed to have 3 buckets
    int sm1BucketCounter = 1;
    for (const auto& buckIt : b1) {
        switch (sm1BucketCounter) {
        case 1:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", 0u, buckIt.getNodesSize());
            break;

        case 2:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(1)));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(2)));
            break;

        case 3:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(5)));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(6)));
            break;
        }

        sm1BucketCounter++;
    }

    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", 3, sm1BucketCounter - 1);

    // SM2
    // Check if nodes don't exist
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt2.hasNode(nodeTestChannels2.at(2)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt2.hasNode(nodeTestChannels2.at(3)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt2.hasNode(nodeTestChannels2.at(4)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt2.hasNode(nodeTestChannels2.at(8)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt2.hasNode(nodeTestChannels2.at(9)));

    // for sm2, supposed to have 3 buckets
    int sm2BucketCounter = 1;
    for (const auto& buckIt : b2) {
        switch (sm2BucketCounter) {
        case 1:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(0)));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(1)));
            break;

        case 2:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(6)));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(7)));
            break;

        case 3:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(5)));
            break;
        }

        sm2BucketCounter++;
    }

    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", 3, sm2BucketCounter - 1);

    // SM3
    // Check if nodes don't exist
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt3.hasNode(nodeTestChannels2.at(2)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt3.hasNode(nodeTestChannels2.at(3)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt3.hasNode(nodeTestChannels2.at(4)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", false, rt3.hasNode(nodeTestChannels2.at(8)));

    // for sm3, supposed to have 3 buckets
    int sm3BucketCounter = 1;
    for (const auto& buckIt : b3) {
        switch (sm3BucketCounter) {
        case 1:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(0)));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(1)));
            break;

        case 2:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(6)));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(7)));
            break;

        case 3:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", true, buckIt.hasNode(nodeTestChannels2.at(9)));
            break;
        }

        sm3BucketCounter++;
    }
    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", 3, sm3BucketCounter - 1);
}

void
RoutingTableTest::testSendKnownNodes_1b()
{
    std::cout << "\ntestSendKnownNodes" << std::endl;

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds2.at(0));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));

    swarmManagers.insert({sm1->getId(), sm1});
    swarmManagers.insert({sm2->getId(), sm2});

    auto& rt1 = sm1->getRoutingTable();
    auto& rt2 = sm2->getRoutingTable();

    auto bucket1 = rt1.findBucket(nodeTestIds2.at(0));
    auto bucket2 = rt2.findBucket(nodeTestIds3.at(0));

    for (size_t i = 0; i < nodeTestChannels3.size(); i++) {
        auto node = nodeTestChannels3.at(i)->deviceId();
        if (node != sm1->getId() && node != sm2->getId()) {
            bucket2->addNode(nodeTestChannels3.at(i));
        }
    }

    std::vector<NodeId> node2Co = {
        NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")};
    needSocketCallBackAcceptAll(sm1);

    sm1->setKnownNodes(node2Co);

    auto start = std::chrono::steady_clock::now();
    bool cn1 {false}, cn2 {false};
    auto isGood = [&] {
        return (cn1 and cn2);
    };
    do {
        std::this_thread::sleep_for(1s);
        cn1 = bucket1->hasConnectingNode(nodeTestIds3.at(2));
        cn2 = bucket1->hasConnectingNode(nodeTestIds3.at(3));

        if (isGood())
            break;
    } while (std::chrono::steady_clock::now() - start < 10s);

    CPPUNIT_ASSERT(isGood());
}

void
RoutingTableTest::testSendKnownNodes_multipleb()
{
    std::cout << "\ntestSendKnownNodes_multipleb" << std::endl;

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds2.at(8));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));

    swarmManagers.insert({sm1->getId(), sm1});
    swarmManagers.insert({sm2->getId(), sm2});

    auto& rt1 = sm1->getRoutingTable();
    auto& rt2 = sm2->getRoutingTable();

    for (size_t i = 0; i < nodeTestIds2.size(); i++) {
        if (i != 1 && i != 0) {
            auto bucket1 = rt1.findBucket(nodeTestIds2.at(i));
            rt1.addNode(nodeTestChannels2.at(i), bucket1);
        }

        auto bucket2 = rt2.findBucket(nodeTestIds3.at(i));
        rt2.addNode(nodeTestChannels3.at(i), bucket2);
    }

    std::vector<NodeId> node2Co = {
        NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")};
    needSocketCallBackAcceptAll(sm1);

    sm1->setKnownNodes(node2Co);

    auto bucket1 = rt1.findBucket(nodeTestIds3.at(1));
    auto bucket2 = rt1.findBucket(nodeTestIds3.at(3));

    auto start = std::chrono::steady_clock::now();
    bool cn1 {false}, cn2 {false};
    auto isGood = [&] {
        return (cn1 or cn2);
    };
    do {
        std::this_thread::sleep_for(1s);
        cn1 = bucket1->hasConnectingNode(nodeTestIds3.at(1));
        cn2 = bucket2->hasConnectingNode(nodeTestIds3.at(3));

    } while (not isGood() and std::chrono::steady_clock::now() - start < 10s);

    CPPUNIT_ASSERT(isGood());
}

void
RoutingTableTest::crossNodes(NodeId nodeId)
{
    std::list<NodeId> pendingNodes {nodeId};
    for (const auto& curNode : pendingNodes) {
        if (discoveredNodes.emplace(curNode).second) {
            for (auto const& node : swarmManagersRandom[curNode]->getRoutingTable().getNodes()) {
                pendingNodes.emplace_back(node);
            }
        }
    }
}

void
RoutingTableTest::testSwarmManagers()
{
    std::cout << "testSwarmManagers" << std::endl;

    for (const auto& sm : swarmManagersRandom) {
        needSocketCallBackAcceptAllRandom(sm.second);
    }

    Counter counter(swarmManagersRandom.size());
    for (const auto& sm : swarmManagersRandom) {
        dht::ThreadPool::computation().run([&] {
            sm.second->setKnownNodes(randomNodeIds);
            counter.count();
        });
    }

    counter.wait();

    std::cout << "Waiting 10s..." << std::endl;
    sleep(10);

    crossNodes(swarmManagersRandom.begin()->first);
    CPPUNIT_ASSERT_EQUAL(swarmManagersRandom.size(), discoveredNodes.size());
}

void
RoutingTableTest::testSwarmManagersSmallBootstrapList()
{
    std::cout << "\ntestSwarmManagersSmallBootstrapList" << std::endl;

    for (const auto& sm : swarmManagersRandom) {
        needSocketCallBackAcceptAllRandom(sm.second);
    }

    Counter counter(swarmManagersRandom.size());
    for (const auto& sm : swarmManagersRandom) {
        dht::ThreadPool::computation().run([&] {
            std::vector<NodeId> randIds(BOOTSTRAP_SIZE);
            std::uniform_int_distribution<size_t> distribution(0, randomNodeIds.size() - 1);
            std::generate(randIds.begin(), randIds.end(), [&] {
                return randomNodeIds[distribution(rd)];
            });
            sm.second->setKnownNodes(randIds);
            counter.count();
        });
    }

    counter.wait();
    // ctx.reset();

    std::cout << "Waiting " << time << "s..." << std::endl;
    sleep(time);

    std::vector<unsigned> dist(8);
    for (const auto& sm : swarmManagersRandom) {
        auto val = sm.second->getRoutingTable().getRoutingTableNodeCount();
        if (dist.size() <= val)
            dist.resize(val + 1);
        dist[val]++;
    }
    for (size_t i = 0; i < dist.size(); i++) {
        std::cout << "Swarm Managers with " << i << " nodes: " << dist[i] << std::endl;
    }

    crossNodes(swarmManagersRandom.begin()->first);
    sleep(10);
    CPPUNIT_ASSERT_EQUAL(swarmManagersRandom.size(), discoveredNodes.size());
}

void
RoutingTableTest::testRoutingTableForConnectingNode()
{
    std::cout << "\ntestRoutingTableForConnectingNode" << std::endl;

    for (const auto& sm : swarmManagersRandom) {
        needSocketCallBackAcceptAllRandom(sm.second);
    }

    Counter counter(swarmManagersRandom.size());
    for (const auto& sm : swarmManagersRandom) {
        dht::ThreadPool::computation().run([&] {
            std::vector<NodeId> randIds(BOOTSTRAP_SIZE);
            std::uniform_int_distribution<size_t> distribution(0, randomNodeIds.size() - 1);
            std::generate(randIds.begin(), randIds.end(), [&] {
                return randomNodeIds[distribution(rd)];
            });
            sm.second->setKnownNodes(randIds);
            counter.count();
        });
    }
    counter.wait();

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds3.at(1));

    swarmManagersRandom.insert({sm1->getId(), sm1});
    swarmManagersRandom.insert({sm2->getId(), sm2});

    needSocketCallBackAcceptAllRandom(sm1);
    needSocketCallBackAcceptAllRandom(sm2);

    std::vector<NodeId> knownNodesSm1({randomNodeIds.at(2), randomNodeIds.at(3)});
    std::vector<NodeId> knownNodesSm2({randomNodeIds.at(4), randomNodeIds.at(5)});

    sm1->setKnownNodes(knownNodesSm1);
    sm2->setKnownNodes(knownNodesSm2);

    sleep(5);

    crossNodes(swarmManagersRandom.begin()->first);
    CPPUNIT_ASSERT_EQUAL(swarmManagersRandom.size(), discoveredNodes.size());
}

void
RoutingTableTest::testRoutingTableForShuttingNode()
{
    std::cout << "\ntestRoutingTableForShuttingNode" << std::endl;

    for (const auto& sm : swarmManagersRandom) {
        needSocketCallBackAcceptAllRandom(sm.second);
    }

    Counter counter(swarmManagersRandom.size());
    for (const auto& sm : swarmManagersRandom) {
        dht::ThreadPool::computation().run([&] {
            std::vector<NodeId> randIds(BOOTSTRAP_SIZE);
            std::uniform_int_distribution<size_t> distribution(0, randomNodeIds.size() - 1);
            std::generate(randIds.begin(), randIds.end(), [&] {
                return randomNodeIds[distribution(rd)];
            });
            sm.second->setKnownNodes(randIds);
            counter.count();
        });
    }

    counter.wait();

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));
    auto sm1Id = sm1->getId();

    swarmManagersRandom.emplace(sm1->getId(), sm1);
    needSocketCallBackAcceptAllRandom(sm1);

    std::vector<NodeId> knownNodesSm1({randomNodeIds.at(2), randomNodeIds.at(3)});
    sm1->setKnownNodes(knownNodesSm1);

    sleep(5);

    crossNodes(swarmManagersRandom.begin()->first);
    CPPUNIT_ASSERT_EQUAL(swarmManagersRandom.size(), discoveredNodes.size());

    for (const auto& sm : swarmManagersRandom) {
        if (sm.first != nodeTestIds3.at(0)) {
            swarmManagersTest_.emplace(sm);
        }
    }

    auto it1 = swarmManagersRandom.find(sm1Id);
    swarmManagersRandom.erase(it1);

    auto it2 = channelSockets_.find(sm1Id);
    channelSockets_.erase(it2);

    sm1 = {};
    sleep(5);
    for (const auto& sm : swarmManagersTest_) {
        auto& a = sm.second->getRoutingTable();
        CPPUNIT_ASSERT(!a.hasNodeId(sm1Id));
    }
}

void
RoutingTableTest::testRoutingTableForMassShuttingsNodes()
{
    std::cout << "\ntestRoutingTableForMassShuttingsNodes" << std::endl;

    for (const auto& sm : swarmManagersRandom) {
        needSocketCallBackAcceptAllRandom(sm.second);
        swarmManagersTest_.emplace(sm);
    }

    Counter counter(swarmManagersRandom.size());
    for (const auto& sm : swarmManagersRandom) {
        dht::ThreadPool::computation().run([&] {
            std::vector<NodeId> randIds(BOOTSTRAP_SIZE);
            std::uniform_int_distribution<size_t> distribution(0, randomNodeIds.size() - 1);
            std::generate(randIds.begin(), randIds.end(), [&] {
                return randomNodeIds[distribution(rd)];
            });
            sm.second->setKnownNodes(randIds);

            counter.count();
        });
    }
    counter.wait();
    sleep(5);
    for (const auto& sm : swarmManagersRandom) {
        sm.second->display();
    }

    crossNodes(swarmManagersRandom.begin()->first);

    CPPUNIT_ASSERT_EQUAL(swarmManagersRandom.size(), discoveredNodes.size());

    discoveredNodes.clear();

    // ADDING NEW NODES TO NETWORK
    for (size_t i = 0; i < nodeTestIds1.size(); i++) {
        auto sm = std::make_shared<SwarmManager>(nodeTestIds1.at(i));
        auto smId = sm->getId();
        swarmManagersRandom.emplace(smId, sm);
        needSocketCallBackAcceptAllRandom(sm);
        std::vector<NodeId> knownNodesSm({randomNodeIds.at(2), randomNodeIds.at(3)});
        sm->setKnownNodes(knownNodesSm);
    }

    sleep(5);
    for (const auto& sm : swarmManagersRandom) {
        sm.second->display();
    }

    crossNodes(swarmManagersRandom.begin()->first);

    CPPUNIT_ASSERT_EQUAL(swarmManagersRandom.size(), discoveredNodes.size());

    discoveredNodes.clear();
    sleep(5);

    // SHUTTING DOWN ADDED NODES

    for (size_t i = 0; i < nodeTestIds1.size(); i++) {
        auto it1 = swarmManagersRandom.find(nodeTestIds1.at(i));
        swarmManagersRandom.erase(it1);

        auto it2 = channelSockets_.find(nodeTestIds1.at(i));
        channelSockets_.erase(it2);
    }
    sleep(5);
    for (const auto& sm : swarmManagersTest_) {
        sm.second->display();

        for (size_t i = 0; i < nodeTestIds1.size(); i++) {
            auto& a = sm.second->getRoutingTable();
            CPPUNIT_ASSERT(!a.hasNodeId(nodeTestIds1.at(i)));
        }
    }
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())
