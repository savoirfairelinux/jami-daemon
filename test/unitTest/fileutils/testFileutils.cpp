/*
 *  Copyright (C) 2004-2017 Savoir-Faire Linux Inc.
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

#include "../../test_runner.h"
#include "../../findPath.h"
#include "fileutils.h"

#include "dring.h"

#include <string>
#include <iostream>

namespace ring { namespace fileutils { namespace test {
    class FileutilsTest : public CppUnit::TestFixture {
    public:
        static std::string name() { return "fileutils"; }

        void setUp();
        void tearDown();

    private:
        void testPath();
        void testState();

        CPPUNIT_TEST_SUITE(FileutilsTest);
        CPPUNIT_TEST(testPath);
        CPPUNIT_TEST(testState);
        CPPUNIT_TEST_SUITE_END();

        const std::string NON_EXISTANT_RELATIVE_PATH = "/test_directory";
        const std::string FILEUTILS_PATH = pathTest()+"/unitTest/fileutils";

    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileutilsTest,
                                                FileutilsTest::name());

    void
    FileutilsTest::setUp()
    {
    }

    void
    FileutilsTest::tearDown()
    {

    }

    void
    FileutilsTest::testPath()
    {
        std::cout<<"home: "<< get_home_dir()<<", config: "<<get_config_dir()<<", data: "<<get_data_dir()<<", cache: "<<get_cache_dir()<<std::endl;

        // check existed directory
        CPPUNIT_ASSERT(check_dir(get_home_dir().c_str()));
        CPPUNIT_ASSERT(check_dir(get_config_dir().c_str()));
        CPPUNIT_ASSERT(check_dir(get_data_dir().c_str()));
        CPPUNIT_ASSERT(check_dir(get_cache_dir().c_str()));
        // check non-existent directory
        CPPUNIT_ASSERT(!check_dir(NON_EXISTANT_RELATIVE_PATH.c_str()));

        CPPUNIT_ASSERT(!isPathRelative(get_home_dir()));
        CPPUNIT_ASSERT(isPathRelative("https://ring.cx/"));

        CPPUNIT_ASSERT(!isFile(get_home_dir()));
        CPPUNIT_ASSERT(isFile(FILEUTILS_PATH+"/testFileutils.cpp"));

        CPPUNIT_ASSERT(isDirectory(get_home_dir()));
        CPPUNIT_ASSERT(!isDirectory(FILEUTILS_PATH+"/testFileutils.cpp"));
        CPPUNIT_ASSERT(isDirectory(FILEUTILS_PATH));

        //recursive_mkdir(NON_EXISTANT_RELATIVE_PATH, mode_t mode=0755);
    }

    void
    FileutilsTest::testState()
    {

    }
}}} // namespace ring::fileutils::test

RING_TEST_RUNNER(ring::fileutils::test::FileutilsTest::name())
