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

#include <dirent.h>
#include <dlfcn.h>

#include "pluginmanager.h"

PluginManager* PluginManager::_instance = 0;

PluginManager*
PluginManager::instance()
{
    if (!_instance) {
        return new PluginManager();
    }

    return _instance;
}

PluginManager::PluginManager()
        :_loadedPlugins()
{
    _instance = this;
}

PluginManager::~PluginManager()
{
    _instance = 0;
}

int
PluginManager::loadPlugins (const std::string &path)
{
    std::string pluginDir, current;
    DIR *dir;
    dirent *dirStruct;
    LibraryManager *library;
    Plugin *plugin;

    const std::string pDir = "..";
    const std::string cDir = ".";

    /* The directory in which plugins are dropped. Default: /usr/lib/sflphone/plugins/ */
    (path == "") ? pluginDir = std::string (PLUGINS_DIR).append ("/") :pluginDir = path;
    _debug ("Loading plugins from %s...", pluginDir.c_str());

    dir = opendir (pluginDir.c_str());
    /* Test if the directory exists or is readable */

    if (dir) {
        /* Read the directory */
        while ( (dirStruct=readdir (dir))) {
            /* Get the name of the current item in the directory */
            current = dirStruct->d_name;
            /* Test if the current item is not the parent or the current directory */

            if (current != pDir && current != cDir) {

                /* Load the dynamic library */
                library = loadDynamicLibrary (pluginDir + current);

                /* Instanciate the plugin object */

                if (instanciatePlugin (library, &plugin) != 0) {
                    _debug ("Error instanciating the plugin ...");
                    return 1;
                }

                /* Regitering the current plugin */
                if (registerPlugin (plugin, library) != 0) {
                    _debug ("Error registering the plugin ...");
                    return 1;
                }
            }
        }
    } else
        return 1;

    /* Close the directory */
    closedir (dir);

    return 0;
}

int
PluginManager::unloadPlugins (void)
{
    PluginInfo *info;

    if (_loadedPlugins.empty())    return 0;

    /* Use an iterator on the loaded plugins map */
    pluginMap::iterator iter;

    iter = _loadedPlugins.begin();

    while (iter != _loadedPlugins.end()) {
        info = iter->second;

        if (deletePlugin (info) != 0) {
            _debug ("Error deleting the plugin ... ");
            return 1;
        }

        unloadDynamicLibrary (info->_libraryPtr);

        if (unregisterPlugin (info) != 0) {
            _debug ("Error unregistering the plugin ... ");
            return 1;
        }

        iter++;
    }

    return 0;
}

bool
PluginManager::isPluginLoaded (const std::string &name)
{
    if (_loadedPlugins.empty())    return false;

    /* Use an iterator on the loaded plugins map */
    pluginMap::iterator iter;

    iter = _loadedPlugins.find (name);

    /* Returns map::end if the specified key has not been found */
    if (iter==_loadedPlugins.end())
        return false;

    /* Returns the plugin pointer */
    return true;
}


LibraryManager*
PluginManager::loadDynamicLibrary (const std::string& filename)
{
    /* Load the library through the library manager */
    return new LibraryManager (filename);
}

int
PluginManager::unloadDynamicLibrary (LibraryManager *libraryPtr)
{
    _debug ("Unloading dynamic library ...");
    /* Close it */
    return libraryPtr->unloadLibrary ();
}

int
PluginManager::instanciatePlugin (LibraryManager *libraryPtr, Plugin **plugin)
{
    createFunc *createPlugin;
    LibraryManager::SymbolHandle symbol;

    if (libraryPtr->resolveSymbol ("createPlugin", &symbol) != 0)
        return 1;

    createPlugin = (createFunc*) symbol;

    *plugin = createPlugin();

    return 0;
}

int
PluginManager::deletePlugin (PluginInfo *plugin)
{
    destroyFunc *destroyPlugin;
    LibraryManager::SymbolHandle symbol;

    if (plugin->_libraryPtr->resolveSymbol ("destroyPlugin", &symbol) != 0)
        return 1;

    destroyPlugin = (destroyFunc*) symbol;

    /* Call it */
    destroyPlugin (plugin->_plugin);

    return 0;
}

int
PluginManager::registerPlugin (Plugin *plugin, LibraryManager *library)
{
    std::string key;
    PluginInfo *p_info;

    if (plugin==0)
        return 1;

    p_info = new PluginInfo();

    /* Retrieve information from the plugin */
    plugin->initFunc (&p_info);

    key = p_info->_name;

    //p_info->_plugin = plugin;
    p_info->_libraryPtr = library;

    /* Add the data in the loaded plugin map */
    _loadedPlugins[ key ] = p_info;

    return 0;
}

int
PluginManager::unregisterPlugin (PluginInfo *plugin)
{
    pluginMap::iterator iter;
    std::string key;

    key = plugin->_name;

    if (!isPluginLoaded (key))
        return 1;

    iter = _loadedPlugins.find (key);

    _loadedPlugins.erase (iter);

    return 0;
}
