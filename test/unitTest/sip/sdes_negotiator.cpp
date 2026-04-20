#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <vector>

#include "sip/sdes_negotiator.h"
#include "test_runner.h"

namespace jami {
namespace test {

class SdesNegotiatorTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "sdes_negotiator"; }

private:
    void acceptsAes256Suite80();
    void acceptsAes256Suite32();

    CPPUNIT_TEST_SUITE(SdesNegotiatorTest);
    CPPUNIT_TEST(acceptsAes256Suite80);
    CPPUNIT_TEST(acceptsAes256Suite32);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SdesNegotiatorTest, SdesNegotiatorTest::name());

void
SdesNegotiatorTest::acceptsAes256Suite80()
{
    const auto crypto = SdesNegotiator::negotiate(
        std::vector<std::string> {"1 AES_256_CM_HMAC_SHA1_80 inline:dGVzdA=="});

    CPPUNIT_ASSERT(crypto);
    CPPUNIT_ASSERT_EQUAL(std::string("1"), crypto.getTag());
    CPPUNIT_ASSERT_EQUAL(std::string("AES_256_CM_HMAC_SHA1_80"), crypto.getCryptoSuite());
    CPPUNIT_ASSERT_EQUAL(std::string("inline"), crypto.getSrtpKeyMethod());
    CPPUNIT_ASSERT_EQUAL(std::string("dGVzdA=="), crypto.getSrtpKeyInfo());
}

void
SdesNegotiatorTest::acceptsAes256Suite32()
{
    const auto crypto = SdesNegotiator::negotiate(
        std::vector<std::string> {"1 AES_256_CM_HMAC_SHA1_32 inline:QUJDREVGRw=="});

    CPPUNIT_ASSERT(crypto);
    CPPUNIT_ASSERT_EQUAL(std::string("AES_256_CM_HMAC_SHA1_32"), crypto.getCryptoSuite());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SdesNegotiatorTest::name());
