#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

/*
 * @file pluginmanager.h
 * @brief   Base class of the plugin manager
 */

#include "plugin.h"
#include "global.h"

#include <map> 
#include <string> 

namespace sflphone {

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
        int loadPlugins( const std::string &path = "" );

        int instanciatePlugin( void *handlePtr, Plugin** plugin );

        /**
         * Check if a plugin has been already loaded
         * @param name  The name of the plugin looked for
         * @return Plugin*  The pointer on the plugin or NULL if not found
         */
        Plugin* isPluginLoaded( const std::string &name );

        int registerPlugin (void *handle, Plugin *plugin);
        
        /**
         * Load a unix dynamic/shared library 
         * @param filename  The path to the dynamic/shared library
         * @return void*    A pointer on it
         */
        void * loadDynamicLibrary( const std::string &filename );
        
        /**
         * Unload a unix dynamic/shared library 
         * @param pluginHandleptr  The pointer on the loaded plugin
         */
        int unloadDynamicLibrary( void * pluginHandlePtr );


        private:
        /**
         * Default constructor
         */
        PluginManager();
        
        /* Map of plugins associated by their string name */
        typedef std::map<std::string, Plugin*> pluginMap;
        pluginMap _loadedPlugins;

        /* The unique static instance */
        static PluginManager* _instance;
    };
}

#endif //PLUGIN_MANAGER_H
