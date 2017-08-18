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

#include "dring.h"
#include <stdio.h>
#include <string.h>

#include "../../test_runner.h"

namespace DRing { namespace test {
    class DringTest : public CppUnit::TestFixture {
        public:
            static std::string name() { return "dring"; }

        private:
            void testDring();

            CPPUNIT_TEST_SUITE(DringTest);
            CPPUNIT_TEST(testDring);
            CPPUNIT_TEST_SUITE_END();

            const std::string ACTUAL_RING_VERSION = "4.0.0";
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(DringTest, DringTest::name());

    void
    DringTest::testDring()
    {
        std::string str(version(),5);
        CPPUNIT_ASSERT(str.compare(ACTUAL_RING_VERSION)==0);

        CPPUNIT_ASSERT(init(InitFlag(DRING_FLAG_DEBUG
            | DRING_FLAG_CONSOLE_LOG
            | DRING_FLAG_AUTOANSWER)));
        CPPUNIT_ASSERT(init(InitFlag()));

        CPPUNIT_ASSERT(start("dring-sample.yml"));
        CPPUNIT_ASSERT(start("dring-sample.yml"));

        fini();
        fini();
    }
}} // namespace DRing::test

RING_TEST_RUNNER(DRing::test::DringTest::name())
