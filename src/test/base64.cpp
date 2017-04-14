#define BOOST_TEST_MODULE Base64
#include <boost/test/included/unit_test.hpp>
#include "base64.h"

const std::vector<uint8_t> test_bytes = { 23, 45, 67, 87, 89, 34, 2, 45, 9, 10 };
const std::string test_base64 = "Fy1DV1kiAi0JCg==";
const std::string test_invalid_base64 = "ERSAÄÖöädt4-++asd==";

BOOST_AUTO_TEST_CASE(encoding_test)
{
    const std::string output = ring::base64::encode(test_bytes);
    BOOST_TEST(test_base64.compare(output) == 0);
}


BOOST_AUTO_TEST_CASE(decoding_test_success)
{
    const std::vector<uint8_t> output = ring::base64::decode(test_base64);
    BOOST_TEST(std::equal(test_bytes.begin(), test_bytes.end(), output.begin()));
}

BOOST_AUTO_TEST_CASE(decoding_test_fail)
{
    // Currently, the input is not validated, i.e. the function most not throw an
    // exception if decoding fails to make sure calling code not expecting any
    // is no broken. (Some validation should be implemented sometimes later, though.
    BOOST_WARN_NO_THROW(ring::base64::decode(test_invalid_base64));
}
