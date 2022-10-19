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
//#include "jamidht/swarm/routing_table.h"
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
    static std::string name() { return "RoutingTable"; }
    std::set<NodeId> tabl;

private:
    void testBucketKnownNodes();
    void testRoutingTableKnowNodes();
    void testSendKnownNodes();

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    CPPUNIT_TEST(testBucketKnownNodes);
    CPPUNIT_TEST(testRoutingTableKnowNodes);
    CPPUNIT_TEST(testSendKnownNodes);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RoutingTableTest, RoutingTableTest::name());

RoutingTableTest::RoutingTableTest()
{
    // Init daemon
    /*     DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("jami-sample.yml")); */
    /*     for (int i = 0; i < 50; i++) {
            tabl.insert(Hash<32>::getRandom());
        }

        for (std::set<NodeId>::iterator i = tabl.begin(); i != tabl.end(); i++) {
            std::cout << *i << std::endl;
        } */
}

void
RoutingTableTest::testBucketKnownNodes()
{
    Bucket bucket(nodeTestIds1.at(0));

    for (int i = 0; i < nodeTestIds1.size(); i++) {
        bucket.addKnownNode(nodeTestIds1.at(i));
    }

    NodeId rId = bucket.randomId();
    NodeId notNode("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f42");
    CPPUNIT_ASSERT(bucket.hasKnownNode(rId));
    CPPUNIT_ASSERT(!bucket.hasKnownNode(notNode));
    CPPUNIT_ASSERT(bucket.getKnownNodesSize() == 9);
}

void
RoutingTableTest::testRoutingTableKnowNodes()
{
    SwarmManager sm1(nodeTestIds1.at(0));
    std::cout << " SM1 ID: " << sm1.getMyId() << std::endl;

    sm1.needSocketCb_ = [&](const std::string& nodeId, auto&& onSocket) {
        // Create ChannelSocketTest (socket)
        // onSocket(socket);
        // cv.notify_all();
    };

    sm1.setKnownNodes(nodeTestIds1);

    auto& r1 = sm1.getRoutingTable();

    NodeId node1_1 = nodeTestIds1.at(1);
    std::cout << " node1_1: " << node1_1 << std::endl;

    NodeId node1_2 = nodeTestIds1.at(nodeTestIds1.size() - 1);
    std::cout << " node1_2: " << node1_2 << std::endl;

    // Test if the routing tables contain first and last element knownNodes
    // to corresponding bucket.
    CPPUNIT_ASSERT(!r1.hasKnownNode(node1_1));
    CPPUNIT_ASSERT(!r1.hasKnownNode(node1_2));
    CPPUNIT_ASSERT(r1.hasConnectingNode(node1_1));
    CPPUNIT_ASSERT(r1.hasConnectingNode(node1_2));

    // CPPUNIT_ASSERT(cv.wait_for(lk, 10s, r1.hasNode(socket)));
    // CPPUNIT_ASSERT(r1.hasNode(socket));
}

void
RoutingTableTest::testSendKnownNodes()
{
    SwarmManager sm1(nodeTestIds1.at(0));
    SwarmManager sm2(nodeTestIds2.at(0));

    CPPUNIT_ASSERT(true);
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())
