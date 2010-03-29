/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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
 * @file configurationTest.cpp
 * @brief       Regroups unitary tests related to the user configuration.
 *              Check if the default configuration has been successfully loaded
 */

#ifndef _CONFIGURATION_TEST_
#define _CONFIGURATION_TEST_

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include <assert.h>

// Application import
#include "manager.h"
#include "audio/audiolayer.h"
#include "global.h"
#include "user_cfg.h"

class ConfigurationTest: public CppUnit::TestFixture {

	/*
	 * Use cppunit library macros to add unit test the factory
	 */
CPPUNIT_TEST_SUITE( ConfigurationTest );
		CPPUNIT_TEST( testInitVolume );
		CPPUNIT_TEST( testDefaultValueAudio );
		CPPUNIT_TEST( testDefaultValuePreferences );
		CPPUNIT_TEST( testDefaultValueSignalisation );
		CPPUNIT_TEST( testInitAudioDriver );
	CPPUNIT_TEST_SUITE_END();

public:
	/*
	 * Unit tests related to the audio preferences
	 */
	void testDefaultValueAudio();

	/*
	 * Unit tests related to the global settings
	 */
	void testDefaultValuePreferences();

	/*
	 * Unit tests related to the global settings
	 */
	void testDefaultValueSignalisation();

	/*
	 * Try to load one SIP account.
	 * So be sure to have only one SIP account so that the test could succeed
	 */
	void testLoadSIPAccount();

	void testUnloadSIPAccount();

	void testInitVolume();

	void testInitAudioDriver();
};
/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConfigurationTest, "ConfigurationTest");
CPPUNIT_TEST_SUITE_REGISTRATION( ConfigurationTest );

#endif
