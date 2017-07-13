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
        void testCheckDir();
        void testPath();
        void testReadDirectory();
        void testLoadFile();
        void testIsDirectoryWritable();;

        CPPUNIT_TEST_SUITE(FileutilsTest);
        CPPUNIT_TEST(testCheckDir);
        CPPUNIT_TEST(testPath);
        CPPUNIT_TEST(testReadDirectory);
        CPPUNIT_TEST(testLoadFile);
        CPPUNIT_TEST(testIsDirectoryWritable);
        CPPUNIT_TEST_SUITE_END();

        const std::string FILEUTILS_PATH = pathTest()+ DIR_SEPARATOR_STR
            + "unitTest" + DIR_SEPARATOR_STR + "fileutils";
        const std::string NON_EXISTANT_PATH = FILEUTILS_PATH + DIR_SEPARATOR_STR
            + "test_mkdir_dir"+ DIR_SEPARATOR_STR + "test";
        const std::string NON_EXISTANT_PATH2 = FILEUTILS_PATH + DIR_SEPARATOR_STR
            + "test_mkdir_dir";
        const std::string EXISTANT_FILE = FILEUTILS_PATH+ DIR_SEPARATOR_STR
            + "testFileutils.cpp";
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
    FileutilsTest::testCheckDir()
    {
        // check existed directory
        CPPUNIT_ASSERT(check_dir(get_home_dir().c_str()));
        CPPUNIT_ASSERT(check_dir(get_config_dir().c_str()));
        CPPUNIT_ASSERT(check_dir(get_data_dir().c_str()));
        CPPUNIT_ASSERT(check_dir(get_cache_dir().c_str()));

        // check non-existent directory
        CPPUNIT_ASSERT(!isDirectory(NON_EXISTANT_PATH));
        CPPUNIT_ASSERT(check_dir(NON_EXISTANT_PATH.c_str()));
        CPPUNIT_ASSERT(isDirectory(NON_EXISTANT_PATH));
        CPPUNIT_ASSERT(removeAll(NON_EXISTANT_PATH2)==0);
        CPPUNIT_ASSERT(!isDirectory(NON_EXISTANT_PATH2));
    }

    void
    FileutilsTest::testPath()
    {
        CPPUNIT_ASSERT(!isPathRelative(get_home_dir()));
        CPPUNIT_ASSERT(isPathRelative("https://ring.cx/"));

        CPPUNIT_ASSERT(!isFile(get_home_dir()));
        CPPUNIT_ASSERT(isFile(EXISTANT_FILE));

        CPPUNIT_ASSERT(!isDirectory(EXISTANT_FILE));
        CPPUNIT_ASSERT(isDirectory(FILEUTILS_PATH));
    }

    void
    FileutilsTest::testReadDirectory()
    {
        CPPUNIT_ASSERT(recursive_mkdir(FILEUTILS_PATH + DIR_SEPARATOR_STR + "readDirectory"+ DIR_SEPARATOR_STR + "test1"));
        CPPUNIT_ASSERT(recursive_mkdir(FILEUTILS_PATH + DIR_SEPARATOR_STR + "readDirectory"+ DIR_SEPARATOR_STR + "test2"));
        std::vector<std::string> dirs = readDirectory(FILEUTILS_PATH + DIR_SEPARATOR_STR + "readDirectory");
        CPPUNIT_ASSERT(dirs.size()==2);
        CPPUNIT_ASSERT(dirs.at(0).compare("test1")==0);
        CPPUNIT_ASSERT(dirs.at(1).compare("test2")==0);
        CPPUNIT_ASSERT(removeAll(FILEUTILS_PATH + DIR_SEPARATOR_STR + "readDirectory")==0);
    }

    void
    FileutilsTest::testLoadFile()
    {
        std::vector<uint8_t> file = loadFile(EXISTANT_FILE);
        CPPUNIT_ASSERT(file.at(7)=='C');
        CPPUNIT_ASSERT(file.at(8)=='o');
        CPPUNIT_ASSERT(file.at(9)=='p');
    }

    void
    FileutilsTest::testIsDirectoryWritable()
    {
        CPPUNIT_ASSERT(recursive_mkdir(NON_EXISTANT_PATH2));
        CPPUNIT_ASSERT(isDirectoryWritable(NON_EXISTANT_PATH2));
        CPPUNIT_ASSERT(removeAll(NON_EXISTANT_PATH2)==0);

        // Create directory with permission: read by owner
        CPPUNIT_ASSERT(recursive_mkdir(NON_EXISTANT_PATH2,0400));
        CPPUNIT_ASSERT(!isDirectoryWritable(NON_EXISTANT_PATH2));
        CPPUNIT_ASSERT(removeAll(NON_EXISTANT_PATH2)==0);
    }
}}} // namespace ring::fileutils::test

RING_TEST_RUNNER(ring::fileutils::test::FileutilsTest::name())
