#ifndef PLUGIN_H
#define PLUGIN_H

#include "plugininterface.h"

namespace sflphone {

    class PluginInterface;

    class Plugin {

        public:

            Plugin (void*, PluginInterface *interface);
            Plugin (PluginInterface *interface);

            ~Plugin ();

            void setName (std::string name);
        private:
            std::string _name;
            int _version_major;
            int _version_minor;
            int _required;
            void *_handlePtr;
            PluginInterface *_interface;

            friend class PluginTest;
            friend class PluginManager;
    };
}
#endif //PLUGIN_H
