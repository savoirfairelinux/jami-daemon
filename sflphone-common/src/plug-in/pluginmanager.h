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

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

/*
 * @file pluginmanager.h
 * @brief   Base class of the plugin manager
 */

#include "librarymanager.h"
#include "global.h"

#include <map>
#include <string>
#include <vector>

class Plugin;

typedef struct PluginInfo {
    std::string _name;
    LibraryManager *_libraryPtr;
    Plugin *_plugin;
    int _major_version;
    int _minor_version;
} PluginInfo;

#include "plugin.h"

class PluginManager
{
    public:
        /**
         * Destructor
         */
        ~PluginManager();

        /**
         * Returns the unique instance of the plugin manager
         */
        static PluginManager* instance();

        /**
         * Load all the plugins found in a specific directory
         * @param path  The absolute path to the directory
         * @return int  The number of items loaded
         */
        int loadPlugins (const std::string &path = "");

        int unloadPlugins (void);

        int instanciatePlugin (LibraryManager* libraryPtr, Plugin** plugin);

        /**
         * Check if a plugin has been already loaded
         * @param name  The name of the plugin looked for
         * @return bool  The pointer on the plugin or NULL if not found
         */
        bool isPluginLoaded (const std::string &name);

        int registerPlugin (Plugin *plugin, LibraryManager *library);

        int unregisterPlugin (PluginInfo *plugin);

        int deletePlugin (PluginInfo *plugin);

        /**
         * Load a unix dynamic/shared library
         * @param filename  The path to the dynamic/shared library
         * @return LibraryManager*    A pointer on the library
         */
        LibraryManager* loadDynamicLibrary (const std::string &filename);

        /**
         * Unload a unix dynamic/shared library
         * @param LibraryManager*  The pointer on the loaded library
         */
        int unloadDynamicLibrary (LibraryManager* libraryPtr);

    private:
        /**
         * Default constructor
         */
        PluginManager();

        /* Map of plugins associated by their string name */
        typedef std::map<std::string, PluginInfo*> pluginMap;
        pluginMap _loadedPlugins;

        /* The unique static instance */
        static PluginManager* _instance;

};

#endif //PLUGIN_MANAGER_H
