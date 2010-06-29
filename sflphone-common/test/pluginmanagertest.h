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

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include <assert.h>

// Application import
#include "plug-in/pluginmanager.h"
#include "plug-in/librarymanager.h"
#include "plug-in/plugin.h"

/*
 * @file pluginManagerTest.cpp  
 * @brief       Regroups unitary tests related to the plugin manager.
 */

#ifndef _PLUGINMANAGER_TEST_
#define _PLUGINMANAGER_TEST_

class PluginManagerTest : public CppUnit::TestCase {

   /**
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE( PluginManagerTest );
        CPPUNIT_TEST( testLoadDynamicLibrary );
        CPPUNIT_TEST( testUnloadDynamicLibrary );
        CPPUNIT_TEST( testInstanciatePlugin );
        CPPUNIT_TEST( testInitPlugin );
        CPPUNIT_TEST( testRegisterPlugin );
        CPPUNIT_TEST( testLoadPlugins );
        CPPUNIT_TEST( testUnloadPlugins );
    CPPUNIT_TEST_SUITE_END();

    public:
        PluginManagerTest() : CppUnit::TestCase("Plugin Manager Tests") {}
        
        /*
         * Code factoring - Common resources can be initialized here.
         * This method is called by unitcpp before each test
         */
        void setUp();

        /*
         * Code factoring - Common resources can be released here.
         * This method is called by unitcpp after each test
         */
        inline void tearDown ();

        void testLoadDynamicLibrary ();
        
        void testUnloadDynamicLibrary ();

        void testInstanciatePlugin ();

        void testInitPlugin ();

        void testRegisterPlugin ();

        void testLoadPlugins ();

        void testUnloadPlugins ();

    private:
        PluginManager *_pm;
        LibraryManager *library;
        Plugin *plugin;
};

/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PluginManagerTest, "PluginManagerTest");
CPPUNIT_TEST_SUITE_REGISTRATION( PluginManagerTest );

#endif
