/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"
#include "fileutils.h"

#include "jami.h"

#include <string>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <fstream>

#include <unistd.h>
#include <stdlib.h>

namespace jami {
namespace fileutils {
namespace test {

class FileutilsTest : public CppUnit::TestFixture
{
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
    void testSha3sumEmpty();
    void testSha3sumKnownValue();
    void testSha3FileMatchesSha3sum();
    void testSha3FileEmpty();
    void testSha3FileNonExistent();
    void testSha3FileDirectory();
    void testSha3FileLargeExactMultiple();
    void testSha3FileLargeNonMultiple();

    CPPUNIT_TEST_SUITE(FileutilsTest);
    CPPUNIT_TEST(testPath);
    CPPUNIT_TEST(testLoadFile);
    CPPUNIT_TEST(testIsDirectoryWritable);
    CPPUNIT_TEST(testGetCleanPath);
    CPPUNIT_TEST(testFullPath);
    CPPUNIT_TEST(testSha3sumEmpty);
    CPPUNIT_TEST(testSha3sumKnownValue);
    CPPUNIT_TEST(testSha3FileMatchesSha3sum);
    CPPUNIT_TEST(testSha3FileEmpty);
    CPPUNIT_TEST(testSha3FileNonExistent);
    CPPUNIT_TEST(testSha3FileDirectory);
    CPPUNIT_TEST(testSha3FileLargeExactMultiple);
    CPPUNIT_TEST(testSha3FileLargeNonMultiple);
    CPPUNIT_TEST_SUITE_END();

    static constexpr auto tmpFileName = "temp_file";

    std::filesystem::path TEST_PATH;
    std::filesystem::path NON_EXISTANT_PATH_BASE;
    std::filesystem::path NON_EXISTANT_PATH;
    std::filesystem::path EXISTANT_FILE;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileutilsTest, FileutilsTest::name());

void
FileutilsTest::setUp()
{
    char template_name[] = {"jami_unit_tests_XXXXXX"};

    // Generate a temporary directory with a file inside
    auto* directory = mkdtemp(template_name);
    CPPUNIT_ASSERT(directory);

    TEST_PATH = directory;
    EXISTANT_FILE = TEST_PATH / tmpFileName;
    NON_EXISTANT_PATH_BASE = TEST_PATH / "not_existing_path";
    NON_EXISTANT_PATH = NON_EXISTANT_PATH_BASE / "test";

    std::ofstream ofs(EXISTANT_FILE, std::ios::binary);
    ofs.write("Jami", 4);
    ofs.close();
}

void
FileutilsTest::tearDown()
{
    std::filesystem::remove(EXISTANT_FILE);
    std::filesystem::remove(TEST_PATH);
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
    CPPUNIT_ASSERT(file.at(0) == 'J');
    CPPUNIT_ASSERT(file.at(1) == 'a');
    CPPUNIT_ASSERT(file.at(2) == 'm');
    CPPUNIT_ASSERT(file.at(3) == 'i');
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
    // empty base
    CPPUNIT_ASSERT(getCleanPath("", NON_EXISTANT_PATH).compare(NON_EXISTANT_PATH) == 0);
    // the base is not contain in the path
    CPPUNIT_ASSERT(getCleanPath(NON_EXISTANT_PATH, NON_EXISTANT_PATH_BASE).compare(NON_EXISTANT_PATH_BASE) == 0);
    // the method is use correctly
    CPPUNIT_ASSERT(getCleanPath(NON_EXISTANT_PATH_BASE, NON_EXISTANT_PATH).compare("test") == 0);
}

void
FileutilsTest::testFullPath()
{
    // empty base
    CPPUNIT_ASSERT(getFullPath("", "relativePath").compare("relativePath") == 0);
    // the path is not relative
    CPPUNIT_ASSERT(getFullPath(NON_EXISTANT_PATH_BASE, "/tmp").compare("/tmp") == 0);
    // the method is use correctly
    CPPUNIT_ASSERT(getFullPath(NON_EXISTANT_PATH_BASE, "test").compare(NON_EXISTANT_PATH) == 0);
}

void
FileutilsTest::testSha3sumEmpty()
{
    std::vector<uint8_t> empty;
    auto hash = sha3sum(empty);
    // SHA3-512 of empty input
    CPPUNIT_ASSERT_EQUAL(std::string("a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
                                     "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26"),
                         hash);
}

void
FileutilsTest::testSha3sumKnownValue()
{
    // SHA3-512 of "Jami"
    std::vector<uint8_t> data {'J', 'a', 'm', 'i'};
    auto hash = sha3sum(data);
    CPPUNIT_ASSERT_EQUAL(std::string("464b0b5ec9966df566f56abc66b265d9b75484d0a707377a61ca2002fb25a16e"
                                     "bac7bd6beb3d4e9975f2598e0617fca3d44a2dca227335d26418baec83ec9455"),
                         hash);
}

void
FileutilsTest::testSha3FileMatchesSha3sum()
{
    // The existing temp file contains "Jami"
    auto fileHash = sha3File(EXISTANT_FILE);
    std::vector<uint8_t> data {'J', 'a', 'm', 'i'};
    auto memHash = sha3sum(data);
    CPPUNIT_ASSERT_EQUAL(memHash, fileHash);
}

void
FileutilsTest::testSha3FileEmpty()
{
    // Create an empty file
    auto emptyFile = TEST_PATH / "empty_file";
    {
        std::ofstream ofs(emptyFile, std::ios::binary);
    }
    auto fileHash = sha3File(emptyFile);
    std::vector<uint8_t> empty;
    auto memHash = sha3sum(empty);
    CPPUNIT_ASSERT_EQUAL(memHash, fileHash);
    std::filesystem::remove(emptyFile);
}

void
FileutilsTest::testSha3FileNonExistent()
{
    auto hash = sha3File("/nonexistent/path/to/file");
    CPPUNIT_ASSERT(hash.empty());
}

void
FileutilsTest::testSha3FileDirectory()
{
    // sha3File on a directory should return empty
    auto hash = sha3File(TEST_PATH);
    CPPUNIT_ASSERT(hash.empty());
}

void
FileutilsTest::testSha3FileLargeExactMultiple()
{
    // 128 KB = 2 * 64 KB, exactly on the read buffer boundary
    constexpr size_t fileSize = 128 * 1024ul;
    std::vector<uint8_t> data(fileSize);
    for (size_t i = 0; i < fileSize; ++i)
        data[i] = static_cast<uint8_t>(i & 0xFF);

    auto largePath = TEST_PATH / "large_exact";
    {
        std::ofstream ofs(largePath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    auto fileHash = sha3File(largePath);
    auto memHash = sha3sum(data);
    CPPUNIT_ASSERT_EQUAL(memHash, fileHash);
    std::filesystem::remove(largePath);
}

void
FileutilsTest::testSha3FileLargeNonMultiple()
{
    // 100000 bytes, not a multiple of 64 KB (65536)
    constexpr size_t fileSize = 100000ul;
    std::vector<uint8_t> data(fileSize);
    for (size_t i = 0; i < fileSize; ++i)
        data[i] = static_cast<uint8_t>(i & 0xFF);

    auto largePath = TEST_PATH / "large_nonmultiple";
    {
        std::ofstream ofs(largePath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    auto fileHash = sha3File(largePath);
    auto memHash = sha3sum(data);
    CPPUNIT_ASSERT_EQUAL(memHash, fileHash);
    std::filesystem::remove(largePath);
}

} // namespace test
} // namespace fileutils
} // namespace jami

CORE_TEST_RUNNER(jami::fileutils::test::FileutilsTest::name());
