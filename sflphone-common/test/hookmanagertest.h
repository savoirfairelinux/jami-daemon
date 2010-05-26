/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
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
