#ifndef PLUGIN_H
#define PLUGIN_H

#include <string> 

#include "global.h" 

/*
 * @file plugin.h
 * @brief Define a plugin object 
 */

namespace sflphone {

class PluginManager;

    class Plugin {
    
        public:
            Plugin( const std::string &name );
            //Plugin( const Plugin &plugin );
            virtual ~Plugin()  {}

        public:
            /**
             * Return the minimal core version required so that the plugin could work
             * @return int  The version required
             */
            virtual int getCoreVersion() const = 0;
            
            /**
             * Register the plugin to the plugin manager
             */
            virtual void registerPlugin( PluginManager & ) = 0;

        private:
            Plugin &operator =(const Plugin &plugin);

    };
}

#endif //PLUGIN_H

