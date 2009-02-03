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

class PluginManager {
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
