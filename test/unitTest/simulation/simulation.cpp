/*
 *  Copyright (C) 2025-2026 Savoir-faire Linux Inc.
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

#include <string>

#include "dst/dst.h"
#include "test_runner.h"

namespace jami {
namespace test {

class SimulationTest : public CppUnit::TestFixture
{
public:
    SimulationTest() {}
    ~SimulationTest() { libjami::fini(); }
    static std::string name() { return "SimulationTest"; }

    void setUp();
    void tearDown();

    void testSimpleConversation();
    void testMissingMessage();

    // Unit test suite
    CPPUNIT_TEST_SUITE(SimulationTest);
    CPPUNIT_TEST(testSimpleConversation);
    CPPUNIT_TEST(testMissingMessage);
    CPPUNIT_TEST_SUITE_END();

private:
    ConversationDST dst_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SimulationTest, SimulationTest::name());

void
SimulationTest::setUp()
{
    dst_.setUp();
    dst_.connectSignals();
    // Need to wait for devices to be registered
    CPPUNIT_ASSERT(dst_.waitForDeviceAnnouncements());
}

void
SimulationTest::tearDown()
{
    dst_.tearDown();
}

void
SimulationTest::testSimpleConversation()
{
    CPPUNIT_ASSERT(dst_.loadUnitTestConfig("simpleConversation"));
    dst_.runUnitTest();
    CPPUNIT_ASSERT(dst_.checkAppearancesForAllAccounts());
}

void
SimulationTest::testMissingMessage()
{
    CPPUNIT_ASSERT(dst_.loadUnitTestConfig("missingMessage"));
    dst_.runUnitTest();
    CPPUNIT_ASSERT(dst_.checkAppearancesForAllAccounts());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SimulationTest::name())