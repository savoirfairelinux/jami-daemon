/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * @file hookmanagerTest.cpp
 * @brief       Regroups unitary tests related to the hook manager.
 */

#ifndef _HOOKMANAGER_TEST_
#define _HOOKMANAGER_TEST_

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include <assert.h>

// Application import
#include "hooks/urlhook.h"

class HookManagerTest : public CppUnit::TestFixture {

   /**
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE (HookManagerTest);
        CPPUNIT_TEST (testAddAction);
        CPPUNIT_TEST (testLargeUrl);
    CPPUNIT_TEST_SUITE_END ();

    public:
        /*
         * Code factoring - Common resources can be initialized here.
         * This method is called by unitcpp before each test
         */
        void setUp();

        void testAddAction ();

        void testLargeUrl ();
        /*
         * Code factoring - Common resources can be released here.
         * This method is called by unitcpp after each test
         */
        void tearDown ();

    private:
        UrlHook *urlhook;
};
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HookManagerTest, "HookManagerTest");
CPPUNIT_TEST_SUITE_REGISTRATION( HookManagerTest );

#endif
