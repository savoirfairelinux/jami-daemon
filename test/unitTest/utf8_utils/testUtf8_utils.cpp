/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#include <string>

#include "utf8_utils.h"
#include "../../test_runner.h"


namespace jami { namespace test {

class Utf8UtilsTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "utf8_utils"; }

private:
    void utf8_validate_test();
    void utf8_make_valid_test();

    CPPUNIT_TEST_SUITE(Utf8UtilsTest);
    CPPUNIT_TEST(utf8_validate_test);
    CPPUNIT_TEST(utf8_make_valid_test);
    CPPUNIT_TEST_SUITE_END();

    const std::string VALIDE_UTF8 = "çèềé{}()/\\*";
    const std::string INVALID_UTF8 = "\xc3\x28";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(Utf8UtilsTest, Utf8UtilsTest::name());

void
Utf8UtilsTest::utf8_validate_test()
{
    CPPUNIT_ASSERT(!utf8_validate(INVALID_UTF8));
    CPPUNIT_ASSERT(utf8_validate(VALIDE_UTF8));
}

void
Utf8UtilsTest::utf8_make_valid_test()
{
    // valide utf8 string in input
    CPPUNIT_ASSERT(utf8_make_valid(VALIDE_UTF8).compare(VALIDE_UTF8) == 0);

    // invalid utf8 string in input
    std::string str = utf8_make_valid(INVALID_UTF8);
    CPPUNIT_ASSERT(utf8_validate(str));
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::Utf8UtilsTest::name());
