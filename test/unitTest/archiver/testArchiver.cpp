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

#include "archiver.h"
#include "../../test_runner.h"

// Get the current path
#include <stdio.h>
#ifdef WINDOWS
    #include <direct.h>
    #define GetCurrentDir _getcwd
#else
    #include <unistd.h>
    #define GetCurrentDir getcwd
 #endif

namespace ring_test {
    class ArchiverTest : public CppUnit::TestFixture {
    public:
        static std::string name() { return "archiver"; }

    private:
        void initPath();
        void compress_decompress();
        void export_Accounts();
        void import_Accounts();

        CPPUNIT_TEST_SUITE(ArchiverTest);
        CPPUNIT_TEST(compress_decompress);
        CPPUNIT_TEST(export_Accounts);
        CPPUNIT_TEST(import_Accounts);
        CPPUNIT_TEST_SUITE_END();

        const std::string archiverSentenceTest_ = "GHY12#@!()+*_hf&";
        std::string archivePath;
        const std::string badPath = "dfdf/dsfdsf";
        const std::string archivePassword = "P@ssw0rd";
        const std::string badPassword = "BadP@sw00rd";

        std::vector<std::string> accountIDs;
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ArchiverTest,
                                                ArchiverTest::name());


    void
    ArchiverTest::initPath()
    {
        // retrieve the current path
        char cCurrentPath[FILENAME_MAX];
        GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
        cCurrentPath[sizeof(cCurrentPath) - 1] = '\0';

        archivePath = std::string(cCurrentPath)+"/unitTest/archiver/archive";
    }

    void
    ArchiverTest::compress_decompress()
    {
        std::vector<uint8_t> file = ring::archiver::decompress(ring::archiver::compress(archiverSentenceTest_));
        std::string decoded {file.begin(), file.end()};
        CPPUNIT_ASSERT(decoded.compare(archiverSentenceTest_) == 0);
    }

    void
    ArchiverTest::export_Accounts()
    {
        initPath();
        accountIDs = {"SIP","RING"};
        //std::cout << ">>>> path:  " << archivePath << '\n';
        CPPUNIT_ASSERT(ring::archiver::exportAccounts(accountIDs, archivePath,archivePassword) == 0);
        CPPUNIT_ASSERT(ring::archiver::exportAccounts(accountIDs, badPath, archivePassword) != 0);

    }

    void
    ArchiverTest::import_Accounts()
    {
        initPath();
        CPPUNIT_ASSERT(ring::archiver::importAccounts(archivePath,archivePassword) == 0);
        CPPUNIT_ASSERT(ring::archiver::importAccounts(archivePath,badPassword) != 0);
        CPPUNIT_ASSERT(ring::archiver::importAccounts(badPath, archivePassword) != 0);
    }


} // namespace tests

RING_TEST_RUNNER(ring_test::ArchiverTest::name())
