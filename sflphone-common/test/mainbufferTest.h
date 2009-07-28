/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include <assert.h>

#include <stdio.h>
#include <sstream>
#include <ccrtp/rtp.h>


// pjsip import
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath/stun_config.h>

// Application import
#include "manager.h"
#include "audio/mainbuffer.h"
#include "audio/ringbuffer.h"
#include "call.h"
// #include "config/config.h"
// #include "user_cfg.h"



/*
 * @file audiorecorderTest.cpp  
 * @brief       Regroups unitary tests related to the plugin manager.
 */

#ifndef _MAINBUFFER_TEST_
#define _MAINBUFFER_TEST_



class MainBufferTest : public CppUnit::TestCase {

    /*
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE( MainBufferTest );
       CPPUNIT_TEST( testRingBufferCreation );
    // CPPUNIT_TEST( testRtpThread );
    // CPPUNIT_TEST( testRtpResampling );
    CPPUNIT_TEST_SUITE_END();

    public:

        MainBufferTest() : CppUnit::TestCase("Audio Layer Tests") {}
        
        /*
         * Code factoring - Common resources can be initialized here.
         * This method is called by unitcpp before each test
         */
        void setUp();

        /*
         * Code factoring - Common resources can be released here.
         * This method is called by unitcpp after each test
         */
        inline void tearDown();

        void testRingBufferCreation();

	// void testRtpThread();

	// void testRtpResampling();


    private:

	MainBuffer _mainbuffer;
};

/* Register our test module */
CPPUNIT_TEST_SUITE_REGISTRATION( MainBufferTest );

#endif
