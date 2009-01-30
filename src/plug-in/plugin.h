#ifndef PLUGIN_H
#define PLUGIN_H

#include <string> 
#include "global.h" 

/*
 * @file plugin.h
 * @brief Define a plugin object 
 */

namespace sflphone {

    class Plugin {

        public:
            Plugin( const std::string &name ){
                _name = name;
            }

            virtual ~Plugin()  {}

            inline std::string getPluginName (void) { return _name; }

            /**
             * Return the minimal core version required so that the plugin could work
             * @return int  The version required
             */
            virtual int initFunc (int i)  = 0;
        
        private:
            Plugin &operator =(const Plugin &plugin);

            std::string _name;
    };

}
typedef ::sflphone::Plugin* createFunc (void);

typedef void destroyFunc (::sflphone::Plugin*);

#endif //PLUGIN_H

