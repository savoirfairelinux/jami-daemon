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
#include <algorithm>

#include "connectivity/peer_connection.h"
#include "nodes.h"

#include <opendht/thread_pool.h>

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

constexpr size_t nNodes = 10;
constexpr size_t mNodes = 5;
constexpr size_t kNodes = 10;

constexpr size_t BOOTSTRAP_SIZE = 2;
constexpr int time = 2;

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
    // ################# METHODS AND VARIABLES GENERATING DATA #################//

    std::mt19937_64 rd {dht::crypto::getSeededRandomEngine<std::mt19937_64>()};
    std::mutex channelSocketsMtx_;
    std::vector<NodeId> randomNodeIds;
    std::map<NodeId, std::map<NodeId, std::shared_ptr<jami::ChannelSocketTest>>> channelSockets_;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagers;
    std::map<NodeId, std::set<NodeId>> nodesToConnect;
    std::set<NodeId> messageNode;

    void generaterandomNodeIds();
    void generateSwarmManagers();
    std::shared_ptr<jami::SwarmManager> getManager(const NodeId& id)
    {
        auto it = swarmManagers.find(id);
        return it == swarmManagers.end() ? nullptr : it->second;
    }
    void setKnownNodesToManager(const std::shared_ptr<SwarmManager>& sm);
    void needSocketCallBack(const std::shared_ptr<SwarmManager>& sm);

    // ################# METHODS AND VARIABLES TO TEST DATA #################//

    std::map<std::shared_ptr<jami::SwarmManager>, std::vector<NodeId>> knownNodesSwarmManager;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagersTest_;
    std::vector<NodeId> discoveredNodes;

    void crossNodes(NodeId nodeId);
    void distribution();

    // ################# UNIT TEST METHODES #################//

    void testBucketMainFunctions();
    void testRoutingTableMainFunctions();
    void testBucketKnownNodes();
    void testSwarmManagerConnectingNodes_1b();
    void testClosestNodes_1b();
    void testClosestNodes_multipleb();
    void testSendKnownNodes_1b();
    void testSendKnownNodes_multipleb();
    void testMobileNodeFunctions();
    void testMobileNodeAnnouncement();
    void testMobileNodeSplit();
    void testSendMobileNodes();
    void testBucketSplit_1n();
    void testSwarmManagersSmallBootstrapList();
    void testRoutingTableForConnectingNode();
    void testRoutingTableForShuttingNode();
    void testRoutingTableForMassShuttingsNodes();
    void testSwarmManagersWMobileModes();

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    CPPUNIT_TEST(testBucketMainFunctions);
    CPPUNIT_TEST(testRoutingTableMainFunctions);
    CPPUNIT_TEST(testClosestNodes_multipleb);
    CPPUNIT_TEST(testBucketSplit_1n);
    CPPUNIT_TEST(testBucketKnownNodes);
    CPPUNIT_TEST(testSendKnownNodes_1b);
    CPPUNIT_TEST(testSendKnownNodes_multipleb);
    CPPUNIT_TEST(testClosestNodes_1b);
    CPPUNIT_TEST(testSwarmManagersSmallBootstrapList);
    CPPUNIT_TEST(testSwarmManagerConnectingNodes_1b);
    CPPUNIT_TEST(testRoutingTableForConnectingNode);
    CPPUNIT_TEST(testMobileNodeFunctions);
    CPPUNIT_TEST(testMobileNodeAnnouncement);
    CPPUNIT_TEST(testMobileNodeSplit);
    CPPUNIT_TEST(testSendMobileNodes);
    CPPUNIT_TEST(testSwarmManagersWMobileModes);
    CPPUNIT_TEST(testRoutingTableForMassShuttingsNodes);
    CPPUNIT_TEST(testRoutingTableForShuttingNode);
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
    auto total = nNodes + mNodes;
    randomNodeIds.reserve(total);
    for (size_t i = 0; i < total; i++) {
        NodeId node = Hash<32>::getRandom();
        randomNodeIds.emplace_back(node);
    }
}

void
RoutingTableTest::generateSwarmManagers()
{
    auto total = nNodes + mNodes;
    for (size_t i = 0; i < total; i++) {
        const NodeId& node = randomNodeIds.at(i);
        auto sm = std::make_shared<SwarmManager>(node);
        i >= nNodes ? sm->setMobility(false) : sm->setMobility(true);
        swarmManagers[node] = sm;
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
RoutingTableTest::needSocketCallBack(const std::shared_ptr<SwarmManager>& sm)
{
    sm->needSocketCb_ = [this, wsm = std::weak_ptr<SwarmManager>(sm)](const std::string& nodeId,
                                                                      auto&& onSocket) {
        Manager::instance().ioContext()->post([this, wsm, nodeId, onSocket = std::move(onSocket)] {
            auto sm = wsm.lock();
            if (!sm)
                return;

            NodeId node = DeviceId(nodeId);
            std::lock_guard<std::mutex> lk(channelSocketsMtx_);
            if (auto smRemote = getManager(node)) {
                auto myId = sm->getId();
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
            }
        });
    };
}

void
RoutingTableTest::distribution()
{
    std::vector<unsigned> dist(8);
    for (const auto& sm : swarmManagers) {
        auto val = sm.second->getRoutingTable().getRoutingTableNodeCount();
        if (dist.size() <= val)
            dist.resize(val + 1);
        dist[val]++;
    }
    for (size_t i = 0; i < dist.size(); i++) {
        std::cout << "Swarm Managers with " << i << " nodes: " << dist[i] << std::endl;
    }
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

    NodeInfo InfoNode1(true, sNode2);

    std::set<std::shared_ptr<ChannelSocketInterface>> socketsCheck {sNode1, sNode2};
    std::set<NodeId> nodesCheck {node1, node2};

    Bucket bucket(node0);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Lower limit error", node0, bucket.getLowerLimit());

    bucket.addNode(sNode1);
    bucket.addNode(std::move(InfoNode1));

    bucket.printBucket(0);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have node", true, bucket.hasNode(sNode1->deviceId()));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have node", true, bucket.hasNode(sNode2->deviceId()));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes",
                                 true,
                                 socketsCheck == bucket.getNodeSockets());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes", true, nodesCheck == bucket.getNodeIds());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have nodes", true, bucket.isFull());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node",
                                 false,
                                 bucket.hasKnownNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node",
                                 false,
                                 bucket.hasKnownNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have connecting node",
                                 false,
                                 bucket.hasConnectingNode(node2));

    CPPUNIT_ASSERT_THROW_MESSAGE("Supposed to be out of range",
                                 bucket.getKnownNode(5),
                                 std::out_of_range);

    bucket.removeNode(sNode1->deviceId()); // ICI
    bucket.shutdownNode(sNode2->deviceId());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", true, bucket.hasKnownNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", false, bucket.hasKnownNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", false, bucket.hasMobileNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have known node", true, bucket.hasMobileNode(node2));
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

    bucket.removeNode(node1);
    bucket.removeNode(node2);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node", false, bucket.hasNode(node2));

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
    std::cout << "\ntestRoutingTableMainFunctions" << std::endl;

    RoutingTable rt;
    NodeId node1 = nodeTestIds1.at(0);
    NodeId node2 = nodeTestIds1.at(1);
    NodeId node3 = nodeTestIds1.at(2);

    rt.setId(node1);

    rt.addKnownNode(node1);
    rt.addKnownNode(node2);
    rt.addKnownNode(node3);

    CPPUNIT_ASSERT(!rt.hasKnownNode(node1));
    CPPUNIT_ASSERT(rt.hasKnownNode(node2));

    auto knownNodes = rt.getKnownNodes();

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have 2 nodes", true, knownNodes.size() == 2);

    auto bucket1 = rt.findBucket(node1);
    auto bucket2 = rt.findBucket(node2);
    auto bucket3 = rt.findBucket(node3);

    rt.addNode(nodeTestChannels1.at(0), bucket1);
    rt.addNode(nodeTestChannels1.at(1), bucket2);
    rt.addNode(nodeTestChannels1.at(2), bucket3);

    CPPUNIT_ASSERT(!rt.hasNode(node1));
    CPPUNIT_ASSERT(rt.hasNode(node2));
    CPPUNIT_ASSERT(rt.hasNode(node3));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist 0", false, rt.removeNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist 1", true, rt.removeNode(node2));

    rt.removeNode(node1);
    rt.removeNode(node2);
    rt.removeNode(node3);

    rt.addConnectingNode(node1);
    rt.addConnectingNode(node2);
    rt.addConnectingNode(node3);

    std::vector<NodeId> nodesCheck({node2, node3});
    const auto& nodes = rt.getConnectingNodes();

    std::vector<NodeId> connectingNode;
    connectingNode.insert(connectingNode.end(), nodes.begin(), nodes.end());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist 3", false, rt.hasNode(node3));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist 1", false, rt.hasConnectingNode(node1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist 3", true, rt.hasConnectingNode(node3));

    std::vector<NodeId> diff;
    std::set_difference(connectingNode.begin(),
                        connectingNode.end(),
                        nodes.begin(),
                        nodes.end(),
                        std::inserter(diff, diff.begin()));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be equal", true, diff.size() == 0);

    rt.shutdownNode(node2);
    rt.shutdownNode(node3);
    rt.printRoutingTable();
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist", true, rt.hasConnectingNode(node2));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to exist", true, rt.hasConnectingNode(node3));
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

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(2));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds1.at(6));

    for (size_t i = 0; i < nodeTestChannels1.size(); i++) {
        sm1->addChannel(nodeTestChannels1.at(i));
        sm2->addChannel(nodeTestChannels1.at(i));
    }

    std::vector<NodeId>
        closestNodes1 {NodeId("2dd1dd976c7dc234ca737c85e4ea48ad09423067a77405254424c4cdd845720d"),
                       NodeId("30e177a56bd1a7969e1973ad8b210a556f6a2b15debc972661a8f555d52edbe2"),
                       NodeId("312226d8fa653704758a681c8c21ec81cec914d0b8aa19e1142d3cf900e3f3b4")};

    std::vector<NodeId>
        closestNodes2 {NodeId("30e177a56bd1a7969e1973ad8b210a556f6a2b15debc972661a8f555d52edbe2"),
                       NodeId("312226d8fa653704758a681c8c21ec81cec914d0b8aa19e1142d3cf900e3f3b4"),
                       NodeId("33f280d8208f42ac34321e6e6871aecd100c2bfd4f1848482e7a7ed8ae895414")};

    auto closestNodes1_ = sm1->getRoutingTable().closestNodes(nodeTestIds1.at(5), 3);
    auto closestNodes2_ = sm2->getRoutingTable().closestNodes(nodeTestIds1.at(5), 3);

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
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node ntc2 0",
                                 false,
                                 rt1.hasNode(nodeTestChannels2.at(0)->deviceId()));

    int sm1BucketCounter = 1;
    for (const auto& buckIt : b1) {
        switch (sm1BucketCounter) {
        case 1:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Size error", 0u, buckIt.getNodesSize());
            break;

        case 2: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(1),
                                         nodeTestIds2.at(2),
                                         nodeTestIds2.at(3),
                                         nodeTestIds2.at(4),
                                         nodeTestIds2.at(8)};

            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getNodeIds());
        }

        break;

        case 3: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(5),
                                         nodeTestIds2.at(6),
                                         nodeTestIds2.at(7),
                                         nodeTestIds2.at(9)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getNodeIds());
        }

        break;
        }

        sm1BucketCounter++;
    }

    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", 3, sm1BucketCounter - 1);

    // SM2
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node ntc2 9",
                                 false,
                                 rt2.hasNode(nodeTestChannels2.at(9)->deviceId()));

    int sm2BucketCounter = 1;
    for (const auto& buckIt : b2) {
        switch (sm2BucketCounter) {
        case 1: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(0),
                                         nodeTestIds2.at(1),
                                         nodeTestIds2.at(2),
                                         nodeTestIds2.at(3),
                                         nodeTestIds2.at(4),
                                         nodeTestIds2.at(8)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getNodeIds());
        }

        break;

        case 2: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(6), nodeTestIds2.at(7)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getNodeIds());
        }

        break;

        case 3:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have node ntc2 5",
                                         true,
                                         buckIt.hasNode(nodeTestChannels2.at(5)->deviceId()));
            break;
        }

        sm2BucketCounter++;
    }

    CPPUNIT_ASSERT_EQUAL_MESSAGE("ERROR", 3, sm2BucketCounter - 1);

    // SM3
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have node ntc2 5",
                                 false,
                                 rt3.hasNode(nodeTestChannels2.at(5)->deviceId()));

    int sm3BucketCounter = 1;
    for (const auto& buckIt : b3) {
        switch (sm3BucketCounter) {
        case 1: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(0),
                                         nodeTestIds2.at(1),
                                         nodeTestIds2.at(2),
                                         nodeTestIds2.at(3),
                                         nodeTestIds2.at(4),
                                         nodeTestIds2.at(8)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getNodeIds());
        }

        break;

        case 2: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(6), nodeTestIds2.at(7)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getNodeIds());
        } break;

        case 3:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have node ntc2 9",
                                         true,
                                         buckIt.hasNode(nodeTestChannels2.at(9)->deviceId()));
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
    needSocketCallBack(sm1);

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
    needSocketCallBack(sm1);

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
RoutingTableTest::testMobileNodeFunctions()
{
    std::cout << "\ntestMobileNodeFunctions" << std::endl;

    RoutingTable rt;
    NodeId node1 = nodeTestIds1.at(0);
    NodeId node2 = nodeTestIds1.at(1);
    NodeId node3 = nodeTestIds1.at(2);

    rt.setId(node1);
    rt.addMobileNode(node1);
    rt.addMobileNode(node2);
    rt.addMobileNode(node3);

    CPPUNIT_ASSERT(!rt.hasMobileNode(node1));
    CPPUNIT_ASSERT(rt.hasMobileNode(node2));
    CPPUNIT_ASSERT(rt.hasMobileNode(node3));

    auto mobileNodes = rt.getMobileNodes();
    CPPUNIT_ASSERT(mobileNodes.size() == 2);

    rt.removeMobileNode(node2);
    rt.removeMobileNode(node3);

    CPPUNIT_ASSERT(!rt.hasMobileNode(node2));
    CPPUNIT_ASSERT(!rt.hasMobileNode(node3));
}

void
RoutingTableTest::testMobileNodeAnnouncement()
{
    std::cout << "\ntestMobileNodeAnnouncement" << std::endl;

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(0));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds2.at(1));

    swarmManagers.insert({sm1->getId(), sm1});
    swarmManagers.insert({sm2->getId(), sm2});
    sm2->setMobility(true);

    std::vector<NodeId> node2Co = {
        NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")};

    needSocketCallBack(sm1);

    sm1->setKnownNodes(node2Co);
    sleep(1);
    auto& rt1 = sm1->getRoutingTable();

    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "Supposed to have",
        true,
        rt1.hasNode(NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")));

    sm2->shutdown();

    auto mb1 = rt1.getMobileNodes();

    std::vector<NodeId> node2Test = {
        NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")};

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be identical", true, node2Test == mb1);
}

void
RoutingTableTest::testMobileNodeSplit()
{
    std::cout << "\ntestMobileNodeSplit" << std::endl;

    SwarmManager sm1(nodeTestIds1.at(0));

    auto& rt1 = sm1.getRoutingTable();

    for (size_t i = 0; i < nodeTestIds1.size(); i++) {
        rt1.addNode(nodeTestChannels1.at(i));
    }

    sm1.setMobileNodes(nodeTestIds2);

    auto& buckets = rt1.getBuckets();

    rt1.printRoutingTable();

    unsigned counter = 1;

    for (auto& buckIt : buckets) {
        switch (counter) {
        case 1:
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have",
                                         false,
                                         buckIt.hasMobileNode(nodeTestIds2.at(0)));
            break;

        case 4: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(2),
                                         nodeTestIds2.at(3),
                                         nodeTestIds2.at(4),
                                         nodeTestIds2.at(8)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getMobileNodes());
        }

        break;

        case 5: {
            std::set<NodeId> nodesCheck {nodeTestIds2.at(5),
                                         nodeTestIds2.at(6),
                                         nodeTestIds2.at(7),
                                         nodeTestIds2.at(9)};
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known nodes",
                                         true,
                                         nodesCheck == buckIt.getMobileNodes());
        }

        break;
        }

        counter++;
    }
}

void
RoutingTableTest::testSendMobileNodes()
{
    std::cout << "\ntestSendMobileNodes" << std::endl;

    auto sm1 = std::make_shared<SwarmManager>(nodeTestIds2.at(8));
    auto sm2 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));

    std::cout << sm1->getId() << std::endl;

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

    std::vector<NodeId> mobileNodes
        = {NodeId("4000000000000000000000000000000000000000000000000000000000000000"),
           NodeId("8000000000000000000000000000000000000000000000000000000000000000")};
    sm2->setMobileNodes(mobileNodes);

    std::vector<NodeId> node2Co = {
        NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8")};
    needSocketCallBack(sm1);

    sm1->setKnownNodes(node2Co);

    sleep(4);

    auto bucket1 = rt1.findBucket(sm1->getId());
    auto bucket2 = rt2.findBucket(sm2->getId());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have",
                                 true,
                                 bucket1->hasMobileNode(mobileNodes.at(0)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have",
                                 false,
                                 bucket1->hasMobileNode(mobileNodes.at(1)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have", false, rt1.hasMobileNode(mobileNodes.at(1)));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have",
                                 true,
                                 bucket2->hasMobileNode(mobileNodes.at(0)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to have", true, rt2.hasMobileNode(mobileNodes.at(1)));
}

void
RoutingTableTest::crossNodes(NodeId nodeId)
{
    std::list<NodeId> pendingNodes {nodeId};
    discoveredNodes.clear();

    for (const auto& curNode : pendingNodes) {
        if (std::find(discoveredNodes.begin(), discoveredNodes.end(), curNode)
            == discoveredNodes.end()) {
            if (discoveredNodes.emplace_back(curNode)) {
                if (auto sm = getManager(curNode))
                    for (auto const& node : sm->getRoutingTable().getNodes()) {
                        pendingNodes.emplace_back(node);
                    }
            }
        }
    }
}

void
RoutingTableTest::testSwarmManagersSmallBootstrapList()
{
    std::cout << "\ntestSwarmManagersSmallBootstrapList" << std::endl;

    for (const auto& sm : swarmManagers) {
        needSocketCallBack(sm.second);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
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

    std::cout << "Waiting " << time * 2 << "s..." << std::endl;
    sleep(time * 2);

    crossNodes(swarmManagers.begin()->first);
    distribution();

    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());
}

void
RoutingTableTest::testRoutingTableForConnectingNode()
{
    std::cout << "\ntestRoutingTableForConnectingNode" << std::endl;

    for (const auto& sm : swarmManagers) {
        needSocketCallBack(sm.second);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
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

    swarmManagers.insert({sm1->getId(), sm1});
    swarmManagers.insert({sm2->getId(), sm2});

    needSocketCallBack(sm1);
    needSocketCallBack(sm2);

    std::vector<NodeId> knownNodesSm1({randomNodeIds.at(2), randomNodeIds.at(3)});
    std::vector<NodeId> knownNodesSm2({randomNodeIds.at(4), randomNodeIds.at(5)});

    sm1->setKnownNodes(knownNodesSm1);
    sm2->setKnownNodes(knownNodesSm2);

    sleep(10);

    crossNodes(swarmManagers.begin()->first);
    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());
}

void
RoutingTableTest::testRoutingTableForShuttingNode()
{
    std::cout << "\ntestRoutingTableForShuttingNode" << std::endl;

    for (const auto& sm : swarmManagers) {
        needSocketCallBack(sm.second);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
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

    swarmManagers.emplace(sm1->getId(), sm1);
    needSocketCallBack(sm1);

    std::vector<NodeId> knownNodesSm1({randomNodeIds.at(2), randomNodeIds.at(3)});
    sm1->setKnownNodes(knownNodesSm1);

    sleep(10);

    crossNodes(swarmManagers.begin()->first);
    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());

    for (const auto& sm : swarmManagers) {
        if (sm.first != nodeTestIds3.at(0)) {
            swarmManagersTest_.emplace(sm);
        }
    }

    auto it1 = swarmManagers.find(sm1Id);
    swarmManagers.erase(it1);

    auto it2 = channelSockets_.find(sm1Id);
    channelSockets_.erase(it2);

    sm1 = {};
    sleep(5);
    for (const auto& sm : swarmManagersTest_) {
        auto& a = sm.second->getRoutingTable();
        CPPUNIT_ASSERT(!a.hasNode(sm1Id));
    }
}

void
RoutingTableTest::testRoutingTableForMassShuttingsNodes()
{
    std::cout << "\ntestRoutingTableForMassShuttingsNodes" << std::endl;
    std::vector<NodeId> swarmToCompare;

    for (const auto& sm : swarmManagers) {
        needSocketCallBack(sm.second);
        swarmManagersTest_.emplace(sm);
        swarmToCompare.emplace_back(sm.first);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
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

    std::cout << "Waiting " << time * 2 << "s... " << std::endl;
    sleep(time * 2);

    crossNodes(swarmManagers.begin()->first);

    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());

    // ADDING NEW NODES TO NETWORK
    for (size_t i = 0; i < nodeTestIds1.size(); i++) {
        auto sm = std::make_shared<SwarmManager>(nodeTestIds1.at(i));
        auto smId = sm->getId();
        swarmManagers.emplace(smId, sm);
        needSocketCallBack(sm);
        std::vector<NodeId> knownNodesSm({randomNodeIds.at(2), randomNodeIds.at(3)});
        sm->setKnownNodes(knownNodesSm);
    }

    sleep(time * 3);
    crossNodes(swarmManagers.begin()->first);

    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());

    // SHUTTING DOWN ADDED NODES
    std::lock_guard<std::mutex> lk(channelSocketsMtx_);
    for (auto& nodes : nodeTestIds1) {
        auto it = swarmManagers.find(nodes);
        if (it != swarmManagers.end()) {
            it->second->shutdown();
            channelSockets_.erase(it->second->getId());
            swarmManagers.erase(it);
        }
    }

    sleep(time * 2);

    crossNodes(swarmManagers.begin()->first);

    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());

    for (const auto& sm : swarmManagersTest_) {
        for (size_t i = 0; i < nodeTestIds1.size(); i++) {
            auto& a = sm.second->getRoutingTable();
            if (!a.hasNode(nodeTestIds1.at(i))) {
                CPPUNIT_ASSERT(true);
            } else {
                CPPUNIT_ASSERT(false);
            }
        }
    }
}

void
RoutingTableTest::testSwarmManagersWMobileModes()
{
    std::cout << "\testSwarmManagersWMobileModes" << std::endl;

    for (const auto& sm : swarmManagers) {
        needSocketCallBack(sm.second);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
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

    std::cout << "Waiting " << time << "s..." << std::endl;
    sleep(time);

    distribution();

    crossNodes(swarmManagers.begin()->first);
    sleep(2);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be equal",
                                 swarmManagers.size(),
                                 discoveredNodes.size());

    // Shutting down Mobile Nodes
    {
        std::lock_guard<std::mutex> lk(channelSocketsMtx_);
        for (auto it = swarmManagers.begin(); it != swarmManagers.end();) {
            if (it->second->isMobile()) {
                it->second->shutdown();
                it = swarmManagers.erase(it);
                channelSockets_.erase(it->second->getId());
            } else {
                ++it;
            }
        }
    }

    sleep(4);

    {
        if (!swarmManagers.empty()) {
            crossNodes(swarmManagers.begin()->first);
            distribution();
        }
    }

    sleep(4);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be equal",
                                 swarmManagers.size(),
                                 discoveredNodes.size());
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())
