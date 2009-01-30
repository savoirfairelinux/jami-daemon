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

#include <stdio.h>
#include <sstream>
#include <dlfcn.h>

#include "pluginmanagerTest.h"

using std::cout;
using std::endl;

#define PLUGIN_TEST_DIR  "/usr/lib/sflphone/plugins/libplugintest.so"
#define PLUGIN_TEST_NAME  "mytest"


void PluginManagerTest::setUp(){
    // Instanciate the plugin manager singleton
    _pm = ::sflphone::PluginManager::instance();
    handlePtr = NULL;
    plugin = 0;
}

void PluginManagerTest::testLoadDynamicLibrary(){
    CPPUNIT_ASSERT(_pm->loadDynamicLibrary(PLUGIN_TEST_DIR) != NULL);
}

void PluginManagerTest::testUnloadDynamicLibrary(){

    handlePtr = _pm->loadDynamicLibrary(PLUGIN_TEST_DIR);
    CPPUNIT_ASSERT(handlePtr != 0);
    CPPUNIT_ASSERT(_pm->unloadDynamicLibrary(handlePtr) == 0 );
}

void PluginManagerTest::testInstanciatePlugin(){
    
    handlePtr = _pm->loadDynamicLibrary (PLUGIN_TEST_DIR);
    CPPUNIT_ASSERT (handlePtr != 0);
    CPPUNIT_ASSERT (_pm->instanciatePlugin (handlePtr, &plugin) == 0);
    CPPUNIT_ASSERT (plugin!=NULL);
}

void PluginManagerTest::testInitPlugin(){

    handlePtr = _pm->loadDynamicLibrary (PLUGIN_TEST_DIR);
    CPPUNIT_ASSERT (handlePtr != 0);
    CPPUNIT_ASSERT (_pm->instanciatePlugin (handlePtr, &plugin) == 0);
    CPPUNIT_ASSERT (plugin!=NULL);
    CPPUNIT_ASSERT (plugin->initFunc(0) == 0);
    CPPUNIT_ASSERT (plugin->getPluginName() == PLUGIN_TEST_NAME);
}

void PluginManagerTest::testRegisterPlugin(){

    handlePtr = _pm->loadDynamicLibrary (PLUGIN_TEST_DIR);
    CPPUNIT_ASSERT (handlePtr != 0);
    CPPUNIT_ASSERT (_pm->instanciatePlugin (handlePtr, &plugin) == 0);
    CPPUNIT_ASSERT (_pm->isPluginLoaded (PLUGIN_TEST_NAME) == NULL);
    CPPUNIT_ASSERT (_pm->registerPlugin (handlePtr, plugin) == 0);
    CPPUNIT_ASSERT (_pm->isPluginLoaded (PLUGIN_TEST_NAME) == plugin);
}

void PluginManagerTest::tearDown(){
    // Delete the plugin manager object
    delete _pm; _pm=0;
    handlePtr = NULL;
    if(plugin)
        delete plugin; plugin = 0;
}
