/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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


#include "audio/delaydetection.h"

#ifndef _DELAYDETECTION_TEST_
#define _DELAYDETECTION_TEST_

/*
 * @file delaydetection.cpp  
 * @brief       Regroups unitary tests related to the delay detection algorithm.
 */

class DelayDetectionTest : public CppUnit::TestCase {
  
    CPPUNIT_TEST_SUITE( DelayDetectionTest );
      CPPUNIT_TEST( testCrossCorrelation );
      CPPUNIT_TEST( testCrossCorrelationDelay );
      CPPUNIT_TEST( testFirFilter );
      CPPUNIT_TEST( testIntToFloatConversion );
      CPPUNIT_TEST( testDownSamplingData );
      CPPUNIT_TEST( testDelayDetection );
    CPPUNIT_TEST_SUITE_END();

 public: 

    DelayDetectionTest() : CppUnit::TestCase("Delay Detection Tests") {}

    void setUp();

    inline void tearDown();

    void testCrossCorrelation();

    void testCrossCorrelationDelay();

    void testFirFilter();

    void testIntToFloatConversion();

    void testDownSamplingData();

    void testDelayDetection();

 private:

    DelayDetection _delaydetect;
    
};

/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(DelayDetectionTest, "DelayDetectionTest");
CPPUNIT_TEST_SUITE_REGISTRATION( DelayDetectionTest );

#endif
