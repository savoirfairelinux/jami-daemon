#ifndef PLUGIN_H
#define PLUGIN_H

#include <string> 

/*
 * @file plugin.h
 * @brief Define a plugin object 
 */

namespace sflphone {

class PluginManager;

    class Plugin {
    
        public:
            Plugin( const std::string &filename );
            Plugin( const Plugin &plugin );
            ~Plugin();

        public:
            /**
             * Return the minimal core version required so that the plugin could work
             * @return int  The version required
             */
            int getCoreVersion() const;
            
            /**
             * Register the plugin to the plugin manager
             */
            void registerPlugin( PluginManager & );

        private:
            Plugin &operator =(const Plugin &plugin);

    };
}

#endif //PLUGIN_H

