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

#include "pluginmanagerTest.h"

using std::cout;
using std::endl;

void PluginManagerTest::setUp(){
    _pm = ::sflphone::PluginManager::instance();
}

void PluginManagerTest::testLoadPluginDirectory(){
    CPPUNIT_ASSERT(_pm->loadPlugins() == 0);
}

void PluginManagerTest::testLoadPlugin(){
    CPPUNIT_ASSERT(_pm->loadPlugins() == 0);
    //CPPUNIT_ASSERT( _pm->isPluginLoaded("test") == NULL );
}

void PluginManagerTest::testRegisterPlugin(){
    // First load the default directory
    _pm->loadPlugins();
    // Resolve the symbol
}

void PluginManagerTest::tearDown(){
    // Delete the plugin manager object
    delete _pm; _pm=0;
}
