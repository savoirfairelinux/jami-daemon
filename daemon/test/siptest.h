/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

// Application import
#include "manager.h"

/*
 * @file siptest.h
 * @brief       Regroups unitary tests related to the SIP module
 */

#ifndef _SIP_TEST_
#define _SIP_TEST_

class SIPTest : public CppUnit::TestCase {

        /**
          * Use cppunit library macros to add unit test the factory
          */
        CPPUNIT_TEST_SUITE(SIPTest);
        CPPUNIT_TEST ( testSimpleOutgoingIpCall );
        CPPUNIT_TEST ( testParseDisplayName );
        // CPPUNIT_TEST ( testSimpleIncomingIpCall );
        // CPPUNIT_TEST ( testTwoOutgoingIpCall );
        // CPPUNIT_TEST ( testTwoIncomingIpCall );
        // CPPUNIT_TEST ( testHoldIpCall);
        // CPPUNIT_TEST ( testIncomingIpCallSdp );
        CPPUNIT_TEST_SUITE_END();

    public:
        SIPTest() : CppUnit::TestCase("SIP module Tests") {}

        /*
         * Code factoring - Common resources can be initialized here.
         * This method is called by unitcpp before each test
         */
        void setUp();

        /*
         * Code factoring - Common resources can be released here.
         * This method is called by unitcpp after each test
         */
        void tearDown();


        void testSimpleOutgoingIpCall(void);

        void testSimpleIncomingIpCall(void);

        void testTwoOutgoingIpCall(void);

        void testTwoIncomingIpCall(void);

        void testHoldIpCall(void);

        void testIncomingIpCallSdp(void);

        void testSIPURI();

        void testParseDisplayName();
};

/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SIPTest, "SIPTest");
CPPUNIT_TEST_SUITE_REGISTRATION(SIPTest);

#endif
