#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

/*
 * @file pluginmanager.h
 * @brief   Base class of the plugin manager
 */

#include "plugin.h"

#include <map> 
#include <string> 

namespace sflphone {

    class PluginManager {

        public:
            /**
             * Load all the plugins found in a specific directory
             * @param path  The absolute path to the directory
             */
            void loadPlugins( const std::string &path );

        private:
            /* Map of plugins associated by their string name */
            typedef std::map<std::string, ::sflphone::Plugin> pluginMap;

            pluginMap _loadedPlugins;
    };
}

#endif //PLUGIN_MANAGER_H
