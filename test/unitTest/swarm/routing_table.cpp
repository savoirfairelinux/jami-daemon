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
#include "jamidht/swarm/routing_table.h"

using namespace std::string_literals;
using NodeId = dht::PkId;

namespace jami {
namespace test {

class RoutingTableTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "RoutingTable"; }

    NodeId nodeId;

private:
    void testCreateRoutingTable();

    /*void testContains();
    void testSplitRoutingTable(); */

    CPPUNIT_TEST_SUITE(RoutingTableTest);
    CPPUNIT_TEST(testCreateRoutingTable);
    /*CPPUNIT_TEST(testContains);
    CPPUNIT_TEST(testSplitRoutingTable); */
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RoutingTableTest, RoutingTableTest::name());

/*
 */
void
RoutingTableTest::testCreateRoutingTable()
{
    RoutingTable routingtable_;

    CPPUNIT_ASSERT(true);
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::RoutingTableTest::name())
