#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include <string> 
#include "global.h" 

#include "plugin.h" 

/*
 * @file plugininterface.h
 * @brief Define a plugin object 
 */

namespace sflphone {

    class PluginManager;
    class Plugin;

    class PluginInterface {

        public:
            PluginInterface( const std::string &name ){
                _name = name;
            }

            virtual ~PluginInterface()  {}

            inline std::string getInterfaceName (void) { return _name; }

            /**
             * Return the minimal core version required so that the plugin could work
             * @return int  The version required
             */
            virtual int initFunc ()  = 0;

            virtual int registerFunc (Plugin **plugin) = 0;

        private:
            PluginInterface &operator =(const PluginInterface &plugin);

            std::string _name;
    };

    typedef PluginInterface* createFunc (void);

    typedef void destroyFunc( PluginInterface* );

}

#endif //PLUGIN_INTERFACE_H

