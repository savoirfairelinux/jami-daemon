#ifndef PLUGIN_H
#define PLUGIN_H

#include <string> 
#include "global.h" 

#include "pluginmanager.h" 

/*
 * @file plugin.h
 * @brief Define a plugin object 
 */

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
        virtual int initFunc (PluginInfo **info) = 0;

    private:
        Plugin &operator =(const Plugin &plugin);

        std::string _name;
};

typedef Plugin* createFunc (void);

typedef void destroyFunc (Plugin*);

#endif //PLUGIN_H

