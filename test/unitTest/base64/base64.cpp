/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: florian Wiesweg <florian.wiesweg@campus.tu-berlin.de>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "test_runner.h"

#include "base64.h"

namespace jami { namespace test {

class Base64Test : public CppUnit::TestFixture {
public:
    static std::string name() { return "base64"; }

private:
    void encodingTest();
    void decodingTestSuccess();
    void decodingTestFail();

    CPPUNIT_TEST_SUITE(Base64Test);
    CPPUNIT_TEST(encodingTest);
    CPPUNIT_TEST(decodingTestSuccess);
    CPPUNIT_TEST(decodingTestFail);
    CPPUNIT_TEST_SUITE_END();

    const std::vector<uint8_t> test_bytes = { 23, 45, 67, 87, 89, 34, 2, 45, 9, 10 };
    const std::string test_base64 = "Fy1DV1kiAi0JCg==";
    const std::string test_invalid_base64 = "ERSAÄÖöädt4-++asd==";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(Base64Test, Base64Test::name());

void
Base64Test::encodingTest()
{
    const std::string output = jami::base64::encode(test_bytes);
    CPPUNIT_ASSERT(test_base64.compare(output) == 0);
}

void
Base64Test::decodingTestSuccess()
{
    const std::vector<uint8_t> output = jami::base64::decode(test_base64);
    CPPUNIT_ASSERT(std::equal(test_bytes.begin(), test_bytes.end(), output.begin()));
}

void
Base64Test::decodingTestFail()
{
    // Currently, the input is not validated, i.e. the function most not throw an
    // exception if decoding fails to make sure calling code not expecting any
    // is no broken. (Some validation should be implemented sometimes later, though.
    jami::base64::decode(test_invalid_base64);
    CPPUNIT_ASSERT(true);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::Base64Test::name());
