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

// Application import
#include "manager.h"

#include "config/config.h"
#include "user_cfg.h"

#include "audio/audiolayer.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"

/*
 * @file audiorecorderTest.cpp  
 * @brief       Regroups unitary tests related to the plugin manager.
 */

#ifndef _AUDIOLAYER_TEST_
#define _AUDIOLAYER_TEST_



class AudioLayerTest : public CppUnit::TestCase {

    /*
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE( AudioLayerTest );
        CPPUNIT_TEST( testAudioLayerConfig );
        // CPPUNIT_TEST( testAudioLayerSwitch );
        CPPUNIT_TEST( testPulseConnect );
        // CPPUNIT_TEST( testAlsaConnect );
    CPPUNIT_TEST_SUITE_END();

    public:
        AudioLayerTest() : CppUnit::TestCase("Audio Layer Tests") {}
        
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

        void testAudioLayerConfig();
        void testAudioLayerSwitch();
        void testPulseConnect();
        void testAlsaConnect();

    private:

        ManagerImpl* manager;
        
        PulseLayer* _pulselayer;
       
        int layer;
};

/* Register our test module */
CPPUNIT_TEST_SUITE_REGISTRATION( AudioLayerTest );

#endif
