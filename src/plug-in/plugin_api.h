#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <string> 

#include "global.h" 

/*
 * @file plugin_api.h
 * @brief Define a plugin object 
 */

#ifdef __cplusplus
extern "C" {
#endif

    namespace sflphone {

        class PluginManager;

        typedef struct PluginApi_Version{
            int version;
            int revision;
        }

        typedef struct Register_Params{
            PluginApi_Version plugin_version;
            create_t create_func;
            destroy_t destroy_func;
        }Register_Params;

        class PluginApi {

            public:
                PluginApi( const std::string &name );
                //Plugin( const Plugin &plugin );
                virtual ~PluginApi()  {}

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
                PluginApi &operator =(const PluginApi &plugin);

        };

        typedef Plugin* create_t( void* );
        typedef int destroy_t( Plugin* );
    }

#ifdef  __cplusplus
}
#endif

#endif //PLUGIN_API_H

