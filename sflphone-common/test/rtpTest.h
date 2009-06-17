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
#include "audio/audiortp.h"
#include "../src/call.h"
#include "../src/sipcall.h"
#include "../src/sipvoiplink.h"

#include "config/config.h"
#include "user_cfg.h"



/*
 * @file audiorecorderTest.cpp  
 * @brief       Regroups unitary tests related to the plugin manager.
 */

#ifndef _RTP_TEST_
#define _RTP_TEST_

class AudioRtp;
class AudioRtpRTX;
class SIPVoIPLink;

class RtpTest : public CppUnit::TestCase {

    /*
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE( RtpTest );
        CPPUNIT_TEST( testRtpInitClose );
	CPPUNIT_TEST( testRtpThread );
	CPPUNIT_TEST( testRtpResampling );
    CPPUNIT_TEST_SUITE_END();

    public:

        RtpTest() : CppUnit::TestCase("Audio Layer Tests") {}
        
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

	bool pjsipInit();

        void testRtpInitClose();

	void testRtpThread();

	void testRtpResampling();


    private:

	enum CallType {Incoming, Outgoing};

        ManagerImpl* manager;

        AudioRtp *audiortp;

	AudioRtpRTX *rtpthread;

	SIPCall *sipcall;

	pj_caching_pool _cp;

	pj_pool_t *_pool;

};

/* Register our test module */
CPPUNIT_TEST_SUITE_REGISTRATION( RtpTest );

#endif
