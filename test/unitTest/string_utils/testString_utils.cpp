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

#include "string_utils.h"
#include <string>

#include "../../test_runner.h"


namespace ring { namespace test {
    class StringUtilsTest : public CppUnit::TestFixture {
        public:
            static std::string name() { return "string_utils"; }

        private:
            void bool_to_str_Test();
            void to_string_Test();
            void to_number_Test();
            void split_string_test();

            CPPUNIT_TEST_SUITE(StringUtilsTest);
            CPPUNIT_TEST(bool_to_str_Test);
            CPPUNIT_TEST(to_string_Test);
            CPPUNIT_TEST(to_number_Test);
            CPPUNIT_TEST(split_string_test);
            CPPUNIT_TEST_SUITE_END();

            const double DOUBLE = 3.14159265359;
            const int INT = 42 ;
            const std::string PI_DOUBLE = "3.14159265359";
            const std::string PI_FLOAT = "3.14159265359";
            const std::string PI_42 = "42";
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(StringUtilsTest, StringUtilsTest::name());


    void
    StringUtilsTest::bool_to_str_Test()
    {
        CPPUNIT_ASSERT(bool_to_str(true) == "true");
        CPPUNIT_ASSERT(bool_to_str(false) == "false");
    }

    void
    StringUtilsTest::to_string_Test()
    {
        // test with double
        CPPUNIT_ASSERT(to_string(DOUBLE) == PI_DOUBLE);

        // test with float
        float varFloat = 3.14;
        std::string sVarFloat = to_string(varFloat);
        CPPUNIT_ASSERT(sVarFloat.at(0)=='3' && sVarFloat.at(1)=='.' && sVarFloat.at(2)=='1' && sVarFloat.at(3)=='4');

        // test with int
        CPPUNIT_ASSERT(to_string(INT).compare(PI_42)==0);
    }

    void
    StringUtilsTest::to_number_Test()
    {
        //test with int
        CPPUNIT_ASSERT(ring::stoi(PI_42) == INT);

        //test with double
        CPPUNIT_ASSERT(ring::stod(PI_DOUBLE) == DOUBLE);
    }

    void
    StringUtilsTest::split_string_test()
    {
        std::vector<std::string> splitString = split_string("*fdg454()**{&xcx*",'*');

        for(int i=0 ; i< splitString.size(); i++){
            std::cout<< splitString.at(i)<<std::endl;
        }
        CPPUNIT_ASSERT(splitString.at(0)=="fdg454()"&&
                       splitString.at(1)=="{&xcx");

        std::vector<unsigned> splitUnsigned = split_string_to_unsigned("/4545497//45454/",'/');
        CPPUNIT_ASSERT(splitUnsigned.at(0)==4545497&&
                       splitUnsigned.at(1)==45454);



    }

}} // namespace ring_test

RING_TEST_RUNNER(ring::test::StringUtilsTest::name())
