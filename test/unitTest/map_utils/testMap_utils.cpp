/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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

#include "map_utils.h"

#include <vector>
#include <map>
#include <string>

#include "../../test_runner.h"

namespace ring { namespace map_utils { namespace test {

class MapUtilsTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "map_utils"; }

    void setUp();

private:
    void test_extractKeys();
    void test_extractValues();

    CPPUNIT_TEST_SUITE(MapUtilsTest);
    CPPUNIT_TEST(test_extractKeys);
    CPPUNIT_TEST(test_extractValues);
    CPPUNIT_TEST_SUITE_END();

    const int NUMBERMAPVALUE = 5;
    std::map<int, std::string> m;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MapUtilsTest, MapUtilsTest::name());

void
MapUtilsTest::setUp()
{
    for (int i = 0; i < NUMBERMAPVALUE; i++) {
        std::string string_i = std::to_string(i);
        m[i] = "value " + string_i;
    }
}

void
MapUtilsTest::test_extractKeys()
{
    auto result = extractKeys(m);

    CPPUNIT_ASSERT(result.size() == m.size());

    int i = 0;
    for (auto& key : result) {
        CPPUNIT_ASSERT(key == i);
        ++i;
    }
}

void
MapUtilsTest::test_extractValues()
{
    auto result = extractValues(m);

    CPPUNIT_ASSERT(result.size() == m.size());

    int i = 0;
    for (auto& value : result) {
        CPPUNIT_ASSERT(value.compare("value " + std::to_string(i)) == 0);
        ++i;
    }
}

}}} // namespace ring::map_utils::test

RING_TEST_RUNNER(ring::map_utils::test::MapUtilsTest::name());
