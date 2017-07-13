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

#include <string>
#include <vector>
#include "dring.h"

#include "preferences.h"
#include "../../test_runner.h"

namespace ring_test {
    class PreferencesTest : public CppUnit::TestFixture {
        public:
            static std::string name() { return "preferences"; }
            void setUp();
            void tearDown();

        private:
            void testSerialize();
            void testManageAccount();
            void init_daemon();

            CPPUNIT_TEST_SUITE(PreferencesTest);
            CPPUNIT_TEST(init_daemon);
            CPPUNIT_TEST(testSerialize);
            CPPUNIT_TEST(testManageAccount);
            CPPUNIT_TEST_SUITE_END();

            ring::Preferences *preferences;
    };

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PreferencesTest, PreferencesTest::name());

    void
    PreferencesTest::setUp()
    {
        preferences = new ring::Preferences();
    }

    void
    PreferencesTest::tearDown()
    {
        delete preferences;
    }

    void
    PreferencesTest::init_daemon()
    {
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }

    void
    PreferencesTest::testManageAccount()
    {
        // Add accounts to accountOrder_
        std::string id("ID1");
        std::string id2("ID2");
        preferences->addAccount(id);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id+"/")==0);
        preferences->addAccount(id2);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id2+"/"+id+"/")==0);

        // Test verifyAccountOrder
        std::vector<std::string> accounts;
        accounts.push_back(id2);
        accounts.push_back("wrongAccountID");
        preferences->verifyAccountOrder(accounts);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id2+"/")==0);

        preferences->addAccount(id);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id+"/"+id2+"/")==0);

        // Remove accounts to accountOrder_
        preferences->removeAccount(id);
        CPPUNIT_ASSERT(preferences->getAccountOrder().compare(id2+"/")==0);
        preferences->removeAccount(id2);
        CPPUNIT_ASSERT(preferences->getAccountOrder().empty());
    }

    void
    PreferencesTest::testSerialize()
    {

    }

} // namespace ring_test

RING_TEST_RUNNER(ring_test::PreferencesTest::name())
