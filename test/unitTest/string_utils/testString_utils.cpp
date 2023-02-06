/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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

#include "string_utils.h"
#include "account.h"

#include <string>
#include <string_view>

#include "../../test_runner.h"

using namespace std::literals;

namespace jami {
namespace test {

class StringUtilsTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "string_utils"; }

private:
    void bool_to_str_test();
    void to_string_test();
    void to_number_test();
    void split_string_test();
    void version_test();

    CPPUNIT_TEST_SUITE(StringUtilsTest);
    CPPUNIT_TEST(bool_to_str_test);
    CPPUNIT_TEST(to_string_test);
    CPPUNIT_TEST(to_number_test);
    CPPUNIT_TEST(split_string_test);
    CPPUNIT_TEST(version_test);
    CPPUNIT_TEST_SUITE_END();

    const double DOUBLE = 3.14159265359;
    const int INT = 42;
    const std::string PI_DOUBLE = "3.14159265359";
    const std::string PI_FLOAT = "3.14159265359";
    const std::string PI_42 = "42";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(StringUtilsTest, StringUtilsTest::name());

void
StringUtilsTest::bool_to_str_test()
{
    CPPUNIT_ASSERT(bool_to_str(true) == TRUE_STR);
    CPPUNIT_ASSERT(bool_to_str(false) == FALSE_STR);
}

void
StringUtilsTest::to_string_test()
{
    // test with double
    CPPUNIT_ASSERT(to_string(DOUBLE) == PI_DOUBLE);

    // test with float
    float varFloat = 3.14;
    std::string sVarFloat = to_string(varFloat);
    CPPUNIT_ASSERT(sVarFloat.at(0) == '3' && sVarFloat.at(1) == '.' && sVarFloat.at(2) == '1'
                   && sVarFloat.at(3) == '4');

    // test with int
    CPPUNIT_ASSERT(std::to_string(INT).compare(PI_42) == 0);

    CPPUNIT_ASSERT_EQUAL("0000000000000010"s, to_hex_string(16));
    CPPUNIT_ASSERT_EQUAL((uint64_t)16, from_hex_string("0000000000000010"s));
}

void
StringUtilsTest::to_number_test()
{
    // test with int
    CPPUNIT_ASSERT(jami::stoi(PI_42) == INT);

    // test with double
    CPPUNIT_ASSERT(jami::stod(PI_DOUBLE) == DOUBLE);
}

void
StringUtilsTest::split_string_test()
{
    auto data = "*fdg454()**{&xcx*"sv;
    auto split_string_result = split_string(data, '*');
    CPPUNIT_ASSERT(split_string_result.size() == 2);
    CPPUNIT_ASSERT(split_string_result.at(0) == "fdg454()"sv
                   && split_string_result.at(1) == "{&xcx"sv);

    auto split_string_to_unsigned_result = split_string_to_unsigned("/4545497//45454/", '/');
    CPPUNIT_ASSERT(split_string_to_unsigned_result.size() == 2);
    CPPUNIT_ASSERT(split_string_to_unsigned_result.at(0) == 4545497
                   && split_string_to_unsigned_result.at(1) == 45454);

    std::string_view line;
    split_string_result.clear();
    while (jami::getline(data, line, '*')) {
        split_string_result.emplace_back(line);
    }
    CPPUNIT_ASSERT(split_string_result.size() == 2);
    CPPUNIT_ASSERT(split_string_result.at(0) == "fdg454()"sv
                   && split_string_result.at(1) == "{&xcx"sv);
}

void
StringUtilsTest::version_test()
{
    CPPUNIT_ASSERT(Account::meetMinimumRequiredVersion({}, {}));
    CPPUNIT_ASSERT(Account::meetMinimumRequiredVersion({1, 2, 3}, {}));
    CPPUNIT_ASSERT(Account::meetMinimumRequiredVersion({1, 2, 3}, {1, 2, 3}));
    CPPUNIT_ASSERT(Account::meetMinimumRequiredVersion({1, 2, 3, 4}, {1, 2, 3}));
    CPPUNIT_ASSERT(Account::meetMinimumRequiredVersion({2}, {1, 2, 3}));
    CPPUNIT_ASSERT(Account::meetMinimumRequiredVersion(
        split_string_to_unsigned("1.2.3.5", '.'),
        split_string_to_unsigned("1.2.3.4", '.')));

    CPPUNIT_ASSERT(!Account::meetMinimumRequiredVersion({}, {1, 2, 3}));
    CPPUNIT_ASSERT(!Account::meetMinimumRequiredVersion({1, 2, 2}, {1, 2, 3}));
    CPPUNIT_ASSERT(!Account::meetMinimumRequiredVersion({1, 2, 3}, {1, 2, 3, 4}));
    CPPUNIT_ASSERT(!Account::meetMinimumRequiredVersion({1, 2, 3}, {2}));
    CPPUNIT_ASSERT(!Account::meetMinimumRequiredVersion(
        split_string_to_unsigned("1.2.3.4", '.'),
        split_string_to_unsigned("1.2.3.5", '.')));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::StringUtilsTest::name());
