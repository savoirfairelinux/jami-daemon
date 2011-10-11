/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <stdio.h>
#include <sstream>
#include <dlfcn.h>

#include "pluginmanagertest.h"

using std::cout;
using std::endl;

#define FAKE_PLUGIN_DESC  "mytest"


void PluginManagerTest::setUp()
{
    FAKE_PLUGIN_DIR = std::string(getenv("FAKE_PLUGIN_DIR"));
    FAKE_PLUGIN_NAME =  std::string(getenv("FAKE_PLUGIN_NAME"));
    // Instanciate the plugin manager singleton
    _pm = PluginManager::instance();
    library = 0;
    plugin = 0;
}

void PluginManagerTest::testLoadDynamicLibrary()
{
    _debug("-------------------- PluginManagerTest::testLoadDynamicLibrary --------------------\n");

    CPPUNIT_ASSERT(_pm->loadDynamicLibrary(FAKE_PLUGIN_NAME) != NULL);
}

void PluginManagerTest::testUnloadDynamicLibrary()
{
    _debug("-------------------- PluginManagerTest::testUnloadDynamicLibrary --------------------\n");

    library = _pm->loadDynamicLibrary(FAKE_PLUGIN_NAME);
    CPPUNIT_ASSERT(library != NULL);
    CPPUNIT_ASSERT(_pm->unloadDynamicLibrary(library) == 0);
}

void PluginManagerTest::testInstanciatePlugin()
{
    _debug("-------------------- PluginManagerTest::testInstanciatePlugin --------------------\n");

    library = _pm->loadDynamicLibrary(FAKE_PLUGIN_NAME);
    CPPUNIT_ASSERT(library != NULL);
    CPPUNIT_ASSERT(_pm->instanciatePlugin(library, &plugin) == 0);
    CPPUNIT_ASSERT(plugin!=NULL);
}

void PluginManagerTest::testInitPlugin()
{
    _debug("-------------------- PluginManagerTest::testInitPlugin --------------------\n");

    library = _pm->loadDynamicLibrary(FAKE_PLUGIN_NAME);
    CPPUNIT_ASSERT(library != NULL);
    CPPUNIT_ASSERT(_pm->instanciatePlugin(library, &plugin) == 0);
    CPPUNIT_ASSERT(plugin!=NULL);
    CPPUNIT_ASSERT(plugin->getPluginName() == FAKE_PLUGIN_DESC);
}

void PluginManagerTest::testRegisterPlugin()
{
    _debug("-------------------- PluginManagerTest::testRegisterPlugin --------------------\n");

    library = _pm->loadDynamicLibrary(FAKE_PLUGIN_NAME);
    CPPUNIT_ASSERT(library != NULL);
    CPPUNIT_ASSERT(_pm->instanciatePlugin(library, &plugin) == 0);
    CPPUNIT_ASSERT(_pm->isPluginLoaded(FAKE_PLUGIN_DESC) == false);
    CPPUNIT_ASSERT(_pm->registerPlugin(plugin, library) == 0);
    CPPUNIT_ASSERT(_pm->isPluginLoaded(FAKE_PLUGIN_DESC) == true);
}

void PluginManagerTest::testLoadPlugins()
{
    _debug("-------------------- PluginManagerTest::testLoadPlugins --------------------\n");

    try {
        CPPUNIT_ASSERT(_pm->loadPlugins(FAKE_PLUGIN_DIR) == 0);
        CPPUNIT_ASSERT(_pm->isPluginLoaded(FAKE_PLUGIN_DESC) == true);
    } catch (LibraryManagerException &e) {

    }
}

void PluginManagerTest::testUnloadPlugins()
{
    _debug("-------------------- PluginManagerTest::testUnloadPlugins --------------------\n");

    try {

        CPPUNIT_ASSERT(_pm->loadPlugins(FAKE_PLUGIN_DIR) == 0);
        CPPUNIT_ASSERT(_pm->isPluginLoaded(FAKE_PLUGIN_DESC) == true);
        CPPUNIT_ASSERT(_pm->unloadPlugins() == 0);
        CPPUNIT_ASSERT(_pm->isPluginLoaded(FAKE_PLUGIN_DESC) == false);
    } catch (LibraryManagerException &e) {

    }
}

void PluginManagerTest::tearDown()
{
    // Delete the plugin manager object
    delete _pm;
    _pm=0;

    delete plugin;
    plugin = 0;

    delete library;
    library = 0;
}
