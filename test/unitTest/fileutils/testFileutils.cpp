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

#include "../../test_runner.h"
#include "fileutils.h"

#include "jami.h"

#include <string>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

namespace jami { namespace fileutils { namespace test {

class FileutilsTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "fileutils"; }

    void setUp();
    void tearDown();

private:
    void testPath();
    void testLoadFile();
    void testIsDirectoryWritable();
    void testGetCleanPath();
    void testFullPath();
    void testCopy();

    CPPUNIT_TEST_SUITE(FileutilsTest);
    CPPUNIT_TEST(testPath);
    CPPUNIT_TEST(testLoadFile);
    CPPUNIT_TEST(testIsDirectoryWritable);
    CPPUNIT_TEST(testGetCleanPath);
    CPPUNIT_TEST(testFullPath);
    CPPUNIT_TEST(testCopy);
    CPPUNIT_TEST_SUITE_END();

    static constexpr auto tmpFileName = "temp_file";

    std::string TEST_PATH;
    std::string NON_EXISTANT_PATH_BASE;
    std::string NON_EXISTANT_PATH;
    std::string EXISTANT_FILE;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileutilsTest, FileutilsTest::name());

void
FileutilsTest::setUp()
{
    char template_name[] = {"ring_unit_tests_XXXXXX"};

    // Generate a temporary directory with a file inside
    auto directory = mkdtemp(template_name);
    CPPUNIT_ASSERT(directory);

    TEST_PATH = directory;
    EXISTANT_FILE = TEST_PATH + DIR_SEPARATOR_STR + tmpFileName;
    NON_EXISTANT_PATH_BASE = TEST_PATH + DIR_SEPARATOR_STR + "not_existing_path";
    NON_EXISTANT_PATH = NON_EXISTANT_PATH_BASE + DIR_SEPARATOR_STR + "test";

    auto* fd = fopen(EXISTANT_FILE.c_str(), "w");
    fwrite("RING", 1, 4, fd);
    fclose(fd);
}

void
FileutilsTest::tearDown()
{
    unlink(EXISTANT_FILE.c_str());
    rmdir(TEST_PATH.c_str());
}

void
FileutilsTest::testPath()
{
    CPPUNIT_ASSERT(isPathRelative("relativePath"));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(EXISTANT_FILE));
    CPPUNIT_ASSERT(!std::filesystem::is_directory(EXISTANT_FILE));
    CPPUNIT_ASSERT(std::filesystem::is_directory(TEST_PATH));
}

void
FileutilsTest::testLoadFile()
{
    auto file = loadFile(EXISTANT_FILE);
    CPPUNIT_ASSERT(file.size() == 4);
    CPPUNIT_ASSERT(file.at(0) == 'R');
    CPPUNIT_ASSERT(file.at(1) == 'I');
    CPPUNIT_ASSERT(file.at(2) == 'N');
    CPPUNIT_ASSERT(file.at(3) == 'G');
}

void
FileutilsTest::testIsDirectoryWritable()
{
    CPPUNIT_ASSERT(dhtnet::fileutils::recursive_mkdir(NON_EXISTANT_PATH_BASE));
    CPPUNIT_ASSERT(isDirectoryWritable(NON_EXISTANT_PATH_BASE));
    CPPUNIT_ASSERT(dhtnet::fileutils::removeAll(NON_EXISTANT_PATH_BASE) == 0);
    // Create directory with permission: read by owner
    CPPUNIT_ASSERT(dhtnet::fileutils::recursive_mkdir(NON_EXISTANT_PATH_BASE, 0400));
    CPPUNIT_ASSERT(!isDirectoryWritable(NON_EXISTANT_PATH_BASE));
    CPPUNIT_ASSERT(dhtnet::fileutils::removeAll(NON_EXISTANT_PATH_BASE) == 0);
}

void
FileutilsTest::testGetCleanPath()
{
    //empty base
    CPPUNIT_ASSERT(getCleanPath("", NON_EXISTANT_PATH).compare(NON_EXISTANT_PATH) == 0);
    //the base is not contain in the path
    CPPUNIT_ASSERT(getCleanPath(NON_EXISTANT_PATH, NON_EXISTANT_PATH_BASE).compare(NON_EXISTANT_PATH_BASE) == 0);
    //the method is use correctly
    CPPUNIT_ASSERT(getCleanPath(NON_EXISTANT_PATH_BASE, NON_EXISTANT_PATH).compare("test") == 0);
}

void
FileutilsTest::testFullPath()
{
    //empty base
    CPPUNIT_ASSERT(getFullPath("", "relativePath").compare("relativePath") == 0);
    //the path is not relative
    CPPUNIT_ASSERT(getFullPath(NON_EXISTANT_PATH_BASE, "/tmp").compare("/tmp") == 0);
    //the method is use correctly
    CPPUNIT_ASSERT(getFullPath(NON_EXISTANT_PATH_BASE, "test").compare(NON_EXISTANT_PATH) == 0);
}

void
FileutilsTest::testCopy()
{
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(EXISTANT_FILE));
    CPPUNIT_ASSERT(!std::filesystem::is_regular_file(NON_EXISTANT_PATH_BASE));
    CPPUNIT_ASSERT(copy(EXISTANT_FILE, NON_EXISTANT_PATH_BASE));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(NON_EXISTANT_PATH_BASE));
    CPPUNIT_ASSERT(dhtnet::fileutils::removeAll(NON_EXISTANT_PATH_BASE) == 0);
}

}}} // namespace jami::test::fileutils

RING_TEST_RUNNER(jami::fileutils::test::FileutilsTest::name());
