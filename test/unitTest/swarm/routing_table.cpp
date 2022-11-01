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

#include "peer_connection.h"
#include "nodes.h"

//#include <opendht/infohash.h>

using namespace std::string_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

int smNumber = 10;
int kNodes = 10;

class RoutingTableTest : public CppUnit::TestFixture
{
public:
    ~RoutingTableTest() { DRing::fini(); }
    static std::string name() { return "RoutingTable"; }

    void setUp();
    void testSuite();

private:
    //################# METHODS AND VARIABLES GENERATING DATA #################//
    std::mutex channelSocketsMtx_;
    std::vector<NodeId> randomNodeIds;
    std::vector<std::shared_ptr<jami::ChannelSocketTest>> randomchannels_;
    std::map<NodeId, std::map<NodeId, std::shared_ptr<jami::ChannelSocketTest>>> channelSockets_;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagers_;
    std::map<NodeId, std::set<NodeId>> nodesToConnect;

    void generaterandomNodeIds();
    void generateSwarmManagers();
    void generateAcceptedConnections(bool all);
    void setKnownNodesToManager(std::shared_ptr<SwarmManager> sm);
    void needSocketCallBack(std::shared_ptr<SwarmManager> sm);
    void needSocketCallBackRefuseAll(std::shared_ptr<SwarmManager> sm);
    void needSocketCallBackAcceptAll(std::shared_ptr<SwarmManager> sm);

    //################# VARIABLES TO TEST DATA #################//

    std::map<std::shared_ptr<jami::SwarmManager>, std::vector<NodeId>> knownNodesSwarmManager;

    //################# UNIT TEST METHODES #################//

    void testBucketKnownNodes();
    void testSwarmManagerConnectingNodes_1b();
    void testSwarmManagerKnowNodes_1b();
    void testSwarmManagerKnowNodes_ConnectingNodes_1b2();
    void testBucketSplit();
    void testClosestNodes_1b();
    void testSendKnownNodes();
    void testBucketSplit_1n();

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    // CPPUNIT_TEST(testBucketKnownNodes);
    // CPPUNIT_TEST(testSwarmManagerConnectingNodes_1b);
    // CPPUNIT_TEST(testSwarmManagerKnowNodes_1b);
    // CPPUNIT_TEST(testBucketSplit);
    CPPUNIT_TEST(testBucketSplit_1n);

    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RoutingTableTest, RoutingTableTest::name());

void
RoutingTableTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized) {
        CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
    }
    generaterandomNodeIds();
    generateSwarmManagers();
}

void
RoutingTableTest::generaterandomNodeIds()
{
    NodeId node;
    for (size_t i = 0; i < kNodes; i++) {
        node = Hash<32>::getRandom();
        randomNodeIds.push_back(node);
        randomchannels_.push_back(std::make_shared<ChannelSocketTest>(node, "test1", 0));
    }
}

void
RoutingTableTest::generateSwarmManagers()
{
    std::mt19937_64 rand_(time(nullptr));
    std::uniform_int_distribution<> distrib(0, kNodes - 1);
    int a;
    int counter = 0;
    std::shared_ptr<SwarmManager> swarmManagerPtr;
    NodeId node;
    std::cout << "              "
              << "SWARM MANAGERS" << std::endl;
    std::cout << "              "
              << "________________________________________________________________" << std::endl;

    while (counter < smNumber) {
        a = distrib(rand_);
        node = randomNodeIds.at(a);
        auto found = swarmManagers_.find(node);

        if (found == swarmManagers_.end()) {
            swarmManagerPtr = std::make_shared<SwarmManager>(node);
            swarmManagers_.insert({node, swarmManagerPtr});
            nodesToConnect.insert({node, {}});
            counter++;
            std::cout << "              " << node << std::endl;
        }
    }

    std::cout << "              "
              << "________________________________________________________________\n"
              << std::endl;
}

void
RoutingTableTest::generateAcceptedConnections(
    bool all) // Argument true = ALL ONLINE, false = RANDOM ONLINE
{
    // il faut faire la meme chose mais pour les known nodes en fait, et si le swarm manager
    // aossicie au node id nexssite pas on la rajoute pas sinon oui. ca peremettra de feaciliter les
    // callback plus tard. La partie random ne sert a rien . je crois.
    std::mt19937_64 rand_(time(nullptr));
    std::uniform_int_distribution<> distrib(0, 1);
    int a;

    for (const auto& swarmMng : swarmManagers_) {
        // nodesToConnect.insert({swarmMng.first, {}});
        std::cout << " \n\n" << std::endl;

        for (const auto& swarmMngBis : swarmManagers_) {
            if (swarmMng.first != swarmMngBis.first) {
                if (all) {
                    nodesToConnect.at(swarmMng.first).insert(swarmMngBis.first);
                    std::cout << "SM ID: " << swarmMng.first
                              << " toConnect  SM ID: " << swarmMngBis.first << std::endl;
                }

                /*
                                std::cout << " SM ID: " << swarmMng.first
                                          << " toConnect  SM ID: " << swarmMngBis.first << std::endl; */
            }
        }
    }

    for (const auto& nodes : nodesToConnect) {
        std::cout << "\nSM ID: " << nodes.first << std::endl;

        for (const auto& nodestoCo : nodes.second) {
            std::cout << " Node to Connect: " << nodestoCo << std::endl;
        }
    }
}

void
RoutingTableTest::setKnownNodesToManager(std::shared_ptr<SwarmManager> sm)
{
    std::mt19937_64 rand_(time(nullptr));
    std::uniform_int_distribution<> distrib(1, kNodes - 1);
    int numberKnownNodesToAdd = distrib(
        rand_); // Corresponds to number of knownNodes we're going to add

    std::uniform_int_distribution<> distribBis(0, kNodes - 1);
    int indexNodeIdToAdd;
    std::vector<NodeId> kNodesToAdd;
    knownNodesSwarmManager.insert({sm, {}});

    int counter = 0;

    while (counter < numberKnownNodesToAdd) {
        indexNodeIdToAdd = distribBis(rand_);
        /*         std::cout << "Index " << indexNodeIdToAdd << std::endl;
         */
        NodeId node = randomNodeIds.at(indexNodeIdToAdd);
        auto it = find(kNodesToAdd.begin(), kNodesToAdd.end(), node);
        if (sm->getMyId() != node && it == kNodesToAdd.end()) {
            /*  std::cout << "MyId " << swarmMng.second->getMyId() << std::endl;
            std::cout << "Node's Id " << node << std::endl;  */

            kNodesToAdd.push_back(node);
            knownNodesSwarmManager.at(sm).push_back(node);
            counter++;
        }
    }

    sm->setKnownNodes(kNodesToAdd);

    // print every node

    /*     for (const auto& swarmMng : knownNodesSwarmManager) {
            std::cout << "SwarmManager " << swarmMng.first->getMyId() << " has: " << std::endl;
            for (const auto& nodes : swarmMng.second) {
                std::cout << "  " << nodes << std::endl;
            }
        } */
}

void
RoutingTableTest::needSocketCallBack(std::shared_ptr<SwarmManager> sm)
{
    sm->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        auto myId = sm->getMyId();
        NodeId node = DeviceId(nodeId);
        bool toConnect = nodeTestisConnected.find(node)->second;

        {
            if (toConnect) {
                // std::cout << "needsocket mon Id : " << myId << " device ID: " << node << std::endl;
                std::lock_guard<std::mutex> lk(channelSocketsMtx_);
                auto& cstRemote = channelSockets_[node][myId];
                auto& cstMe = channelSockets_[myId][node];
                bool addChannel_ = false;
                if (!cstRemote) {
                    cstRemote = std::make_shared<ChannelSocketTest>(myId, "test1", 0);
                    // std::cout << "Created Socket for remote" << std::endl;
                }
                if (!cstMe) {
                    cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
                    // std::cout << "Created Socket for itself " << std::endl;
                }
                ChannelSocketTest::link(cstMe, cstRemote);

                onSocket(cstMe);

                auto smRemote = swarmManagers_[node];
                smRemote->addChannel(cstRemote);
            }
        }
    };
}

void
RoutingTableTest::needSocketCallBackRefuseAll(std::shared_ptr<SwarmManager> sm)
{
    sm->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        auto myId = sm->getMyId();
        NodeId node = DeviceId(nodeId);
        {
            // std::cout << "needsocket mon Id : " << myId << " device ID: " << node << std::endl;
            std::lock_guard<std::mutex> lk(channelSocketsMtx_);
            auto cstRemote = std::make_shared<ChannelSocketTest>(myId, "test1", 0);
            // std::cout << "Created Socket for remote" << std::endl;
            auto cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
        };
    };
};

void
RoutingTableTest::needSocketCallBackAcceptAll(std::shared_ptr<SwarmManager> sm)
{
    sm->needSocketCb_ = [this, myId = sm->getMyId()](const std::string& nodeId, auto&& onSocket) {
        Manager::instance().ioContext()->post(
            [this, myId, node = DeviceId(nodeId), onSocket = std::move(onSocket)]() {
                std::cout << "needsocket mon Id : " << myId << " device ID: " << node << std::endl;
                std::lock_guard<std::mutex> lk(channelSocketsMtx_);
                auto& cstRemote = channelSockets_[node][myId];
                auto& cstMe = channelSockets_[myId][node];
                if (!cstRemote) {
                    cstRemote = std::make_shared<ChannelSocketTest>(myId, "test1", 0);
                    std::cout << "Created Socket for remote" << std::endl;
                }
                if (!cstMe) {
                    cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
                    std::cout << "Created Socket for itself " << std::endl;
                }
                ChannelSocketTest::link(cstMe, cstRemote);

                onSocket(cstMe);

                if (auto smRemote = swarmManagers_[node]) {
                    smRemote->addChannel(cstRemote);
                }
            });
    };
}

void
RoutingTableTest::testBucketKnownNodes()
{
    std::cout << "\ntestBucketKnownNodes" << std::endl;
    Bucket bucket(randomNodeIds.at(0));

    for (int i = 0; i < randomNodeIds.size(); i++) {
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
RoutingTableTest::testSwarmManagerConnectingNodes_1b()
{
    std::cout << "\ntestSwarmManagerKnowNodes_1b" << std::endl;
    for (const auto& smMngr : swarmManagers_) {
        setKnownNodesToManager(smMngr.second);
    }

    for (const auto& sm : swarmManagers_) { //(NodeId, SwarmManager)

        auto& r = sm.second->getRoutingTable();

        for (const auto& testNodes : knownNodesSwarmManager.at(sm.second)) {
            // std::cout << "Node tested " << testNodes << std::endl;
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Not supposed to have known node",
                                         false,
                                         r.hasKnownNode(testNodes));
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be a connecting node",
                                         true,
                                         r.hasConnectingNode(testNodes));
        }
    }
}

void
RoutingTableTest::testSwarmManagerKnowNodes_1b()
{
    std::cout << "\ntestSwarmManagerKnowNodes_ConnectingNodes_1b" << std::endl;

    /*  for (const auto& smMngr : swarmManagers_) {
           needSocketCallBackRefuseAll(smMngr.second);
          // auto& r = smMngr.second->getRoutingTable();
      }
      setKnownNodesToManagers(); */

    // needSocketCallBack();
    // setKnownNodesToManagers();

    /*     sm1->needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
            NodeId node = DeviceId(nodeId);
            bool toConnect = nodeTestisConnected.find(node)->second;

            if (toConnect) {
                auto cstRemote = std::make_shared<ChannelSocketTest>(node, "test1", 0);
                auto cstMe = std::make_shared<ChannelSocketTest>(sm1->getMyId(), "test1", 0);

                test.push_back(cstRemote);

                onSocket(cstRemote);
            }
        }; */

    /*     sm1->setKnownNodes(nodeTestIds1);

        auto& r1 = sm1->getRoutingTable(); */

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be connected to node", true, true);
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
                    auto& cstMe = channelSockets_[sm1.getMyId()][nodeId]; // map<NodeId,
       map<NodeId, shared<ChannelSocketTest>>> if (!cstMe) cstMe =
       std::make_shared<ChannelSocketTest>(); auto& cstRemote =
       channelSockets_[nodeId][sm1.getMyId()]; // map<NodeId, map<NodeId,
       shared<ChannelSocketTest>>> if (!cstRemote) { cstRemote =
       std::make_shared<ChannelSocketTest>();
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

    for (const auto& smMngr : swarmManagers_) {
        needSocketCallBackAcceptAll(smMngr.second);
        setKnownNodesToManager(smMngr.second);
    }

    // setKnownNodesToManagers();

    sleep(1);

    std::cout << "\nKnownNodes List " << std::endl;
    for (const auto& swarmMng : knownNodesSwarmManager) {
        std::cout << "\nSwarmManager " << swarmMng.first->getMyId() << " has: " << std::endl;
        for (const auto& nodes : swarmMng.second) {
            std::cout << " - " << nodes << std::endl;
        }
    }

    std::cout << "\nRoutingTables " << std::endl;

    for (const auto& smMngr : swarmManagers_) {
        smMngr.second->display();
    }
    CPPUNIT_ASSERT(true);

    /*     auto sm1 = std::make_shared<SwarmManager>(nodeTestIds1.at(0));
        auto sm2 = std::make_shared<SwarmManager>(nodeTestIds2.at(0));
        //    auto sm3 = std::make_shared<SwarmManager>(nodeTestIds3.at(0));

        swarmManagers_.insert({sm1->getMyId(), sm1});
        swarmManagers_.insert({sm2->getMyId(), sm2});

        needSocketCallBack(sm1);
        sm1->setKnownNodes(nodeTestIds1);
        sleep(4);
        needSocketCallBack(sm2);
        sm2->setKnownNodes(nodeTestIds2);

        /*     sm3->setKnownNodes(nodeTestIds3);
            auto& r3 = sm3->getRoutingTable();
            std::cout << "\nSWARM MANAGER 3" << std::endl;
            r3.printRoutingTable();
         */

    /*     auto& r1 = sm1->getRoutingTable();
        std::cout << "\nSWARM MANAGER 1" << std::endl;
        r1.printRoutingTable();
        auto& r2 = sm2->getRoutingTable();
        std::cout << "\nSWARM MANAGER 2" << std::endl;
        r2.printRoutingTable();
        /*     sm1->shutdown();
            sm2->shutdown();
         */
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

void
RoutingTableTest::testBucketSplit_1n()
{
    std::cout << "\ntestBucketSplit_1n" << std::endl;

    SwarmManager sm1(randomNodeIds.at(0));
    auto& rt = sm1.getRoutingTable();

    for (size_t i = 1; i < randomNodeIds.size(); i++) {
        auto bucket = rt.findBucket(randomNodeIds.at(i));
        rt.addNode(randomchannels_.at(i), bucket);
        sm1.display();
    }

    // recuperer la routing table, regarder son premier element et voir si c le plus petit de la
    // liste des randomNodes,
    //  faire pareil pour le dernier.

    CPPUNIT_ASSERT(true);
}

void
RoutingTableTest::testSuite()
{
    // testBucketKnownNodes();
    //   CPPUNIT_TEST(testSwarmManagerKnowNodes_ConnectingNodes_1b2);
    //   CPPUNIT_TEST(testBucketSplit);
    //   CPPUNIT_TEST(testClosestNodes_1b);
    //   CPPUNIT_TEST(testSendKnownNodes);
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())
