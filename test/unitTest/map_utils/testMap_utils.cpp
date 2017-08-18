/*
 *  Copyright (C) 2004-2017 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
            void tearDown();

        private:
            void testVectorFromMapKeys();
            void testVectorFromMapValues();
            void testFindByValue();

            CPPUNIT_TEST_SUITE(MapUtilsTest);
            CPPUNIT_TEST(testVectorFromMapKeys);
            CPPUNIT_TEST(testVectorFromMapValues);
            CPPUNIT_TEST(testFindByValue);
            CPPUNIT_TEST_SUITE_END();

            const int NUMBERMAPVALUE = 5;
            std::vector<std::string> v;
            std::map<std::string, std::string> m;
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MapUtilsTest, MapUtilsTest::name());

    void
    MapUtilsTest::setUp()
    {
        for(int i=0; i<NUMBERMAPVALUE; i++){
            std::string string_i = std::to_string(i);
            m[string_i]= "value "+string_i;
        }
    }

    void
    MapUtilsTest::tearDown()
    {
        m.clear();
        v.clear();
    }

    void
    MapUtilsTest::testVectorFromMapKeys()
    {
        vectorFromMapKeys(m,v);
        for(int i=0; i<NUMBERMAPVALUE; i++){
            CPPUNIT_ASSERT(v.at(i).compare(std::to_string(i))==0);
        }
    }

    void
    MapUtilsTest::testVectorFromMapValues()
    {
        vectorFromMapValues(m,v);
        for(int i=0; i<NUMBERMAPVALUE; i++){
            CPPUNIT_ASSERT(v.at(i).compare("value "+std::to_string(i))==0);
        }
    }

    void
    MapUtilsTest::testFindByValue()
    {
        std::vector<int> vi;
        std::map<int, int> mi;

        for(int i=0; i<NUMBERMAPVALUE; i++){

            mi[i]= 10+i;
            vi.push_back(10+i);
        }
        //findByValue(mi, vi); TODO findByValue crash
        //error: no match for ‘operator==’ (operand types are ‘const int’ and ‘std::vector<int>’)

    }

}}} // namespace ring::map_utils::test

RING_TEST_RUNNER(ring::map_utils::test::MapUtilsTest::name())
