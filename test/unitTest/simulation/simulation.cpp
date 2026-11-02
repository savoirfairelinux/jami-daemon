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
    void testAlreadyMemberInClonedRepo();
    void testMessageComparisonIgnoresBodyLinearizedParent();
    void testClientHistorySeed();

    // Unit test suite
    CPPUNIT_TEST_SUITE(SimulationTest);
    CPPUNIT_TEST(testSimpleConversation);
    CPPUNIT_TEST(testMissingMessage);
    CPPUNIT_TEST(testAlreadyMemberInClonedRepo);
    CPPUNIT_TEST(testMessageComparisonIgnoresBodyLinearizedParent);
    CPPUNIT_TEST(testClientHistorySeed);
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
    CPPUNIT_ASSERT(dst_.checkAllAccounts());
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

void
SimulationTest::testAlreadyMemberInClonedRepo()
{
    runTest("simulation/data/alreadyMemberInClonedRepo.json");
}

void
SimulationTest::testMessageComparisonIgnoresBodyLinearizedParent()
{
    RepositoryAccount account(std::make_shared<JamiAccount>("dst-message-comparison"));
    account.account->setConfig(account.account->buildConfig());
    libjami::SwarmMessage expected;
    expected.id = "message";
    expected.type = "text/plain";
    expected.linearizedParent = "parent";
    expected.body = {{"body", "message body"}, {"linearizedParent", "log-parent"}};
    expected.reactions = {{{"id", "reaction"}, {"body", "reaction body"}, {"linearizedParent", "log-parent"}}};
    expected.editions = {{{"id", "edition"}, {"body", "previous body"}, {"linearizedParent", "log-parent"}}};

    auto actual = expected;
    actual.body["linearizedParent"] = "";
    actual.reactions.front()["linearizedParent"] = "arrival-parent";
    actual.editions.front().erase("linearizedParent");
    CPPUNIT_ASSERT(dst_.checkMessagesMatch(account, {expected}, {actual}));

    actual.linearizedParent = "wrong-parent";
    CPPUNIT_ASSERT(!dst_.checkMessagesMatch(account, {expected}, {actual}));
    actual.linearizedParent = expected.linearizedParent;

    actual.body["body"] = "wrong body";
    CPPUNIT_ASSERT(!dst_.checkMessagesMatch(account, {expected}, {actual}));
    actual.body["body"] = expected.body.at("body");

    actual.reactions.front()["body"] = "wrong reaction";
    CPPUNIT_ASSERT(!dst_.checkMessagesMatch(account, {expected}, {actual}));
    actual.reactions.front()["body"] = expected.reactions.front().at("body");

    actual.editions.front()["body"] = "wrong edition";
    CPPUNIT_ASSERT(!dst_.checkMessagesMatch(account, {expected}, {actual}));
}

void
SimulationTest::testClientHistorySeed()
{
    CPPUNIT_ASSERT(dst_.setUp());
    CPPUNIT_ASSERT(dst_.run(13841374182132717297ULL, 500));
    dst_.resetRepositories();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SimulationTest::name())