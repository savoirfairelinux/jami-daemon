#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "smartools.h"
#include "../../test_runner.h"

namespace ring_test {
    class SmartoolsTest : public CppUnit::TestFixture {
    public:
        static std::string name() { return "smartools"; }

    private:
        void setFramerateTest();


        CPPUNIT_TEST_SUITE(SmartoolsTest);
        CPPUNIT_TEST(setFramerateTest);

        CPPUNIT_TEST_SUITE_END();

        //const std::vector<uint8_t> test_bytes = { 23, 45, 67, 87, 89, 34, 2, 45, 9, 10 };

    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SmartoolsTest, SmartoolsTest::name());

    void SmartoolsTest::setFramerateTest()
    {
        CPPUNIT_ASSERT(true);
    }

} // namespace tests

RING_TEST_RUNNER(ring_test::SmartoolsTest::name())
