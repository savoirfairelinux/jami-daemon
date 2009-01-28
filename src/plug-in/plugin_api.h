#ifndef PLUGIN_H
#define PLUGIN_H

#include <string> 

#include "global.h" 

/*
 * @file plugin.h
 * @brief Define a plugin object 
 */

#ifdef __cplusplus
extern "C" {
#endif

    namespace sflphone {

        class PluginManager;

        class Plugin {

            public:
                Plugin( const std::string &name ){
                    _name = name;
                }

                virtual ~Plugin()  {}

            public:
                /**
                 * Return the minimal core version required so that the plugin could work
                 * @return int  The version required
                 */
                virtual int getCoreVersion() const = 0;

            private:
                Plugin &operator =(const Plugin &plugin);

                std::string _name;
        };
        
        typedef Plugin* createFunc( void* );
        
        typedef void destroyFunc( Plugin* );

    }

#ifdef  __cplusplus
}
#endif

#endif //PLUGIN_H

