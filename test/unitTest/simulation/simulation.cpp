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
#include "manager.h"
#include "test_runner.h"

namespace jami {
namespace test {

class SimulationTest : public CppUnit::TestFixture
{
public:
    SimulationTest()
        : dst_(true, true)
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~SimulationTest() { libjami::fini(); }
    static std::string name() { return "SimulationTest"; }

    void testSimpleConversation();
    void testMissingMessage();

    // Unit test suite
    CPPUNIT_TEST_SUITE(SimulationTest);
    CPPUNIT_TEST(testSimpleConversation);
    CPPUNIT_TEST(testMissingMessage);
    CPPUNIT_TEST_SUITE_END();

private:
    void runTest(const std::string& unitTestPath);
    ConversationDST dst_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SimulationTest, SimulationTest::name());

void
SimulationTest::runTest(const std::string& unitTestPath)
{
    UnitTest unitTest = dst_.loadUnitTestConfig(unitTestPath);
    CPPUNIT_ASSERT(unitTest.numAccounts > 0);
    CPPUNIT_ASSERT(!unitTest.events.empty());

    CPPUNIT_ASSERT(dst_.setUp(unitTest.numAccounts));

    dst_.runUnitTest(unitTest);
    CPPUNIT_ASSERT(dst_.checkAppearancesForAllAccounts());
    dst_.resetRepositories();
}

void
SimulationTest::testSimpleConversation()
{
    runTest("simulation/data/simpleConversation.json");
}

void
SimulationTest::testMissingMessage()
{
    runTest("simulation/data/missingMessage.json");
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SimulationTest::name())