#include "plugin_manager_interface.h"
#include "manager.h"
#include "logger.h"
#include <iostream>

namespace DRing {
//
void
loadPlugin(const std::string& path){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    JAMI_WARN() << " LOADING PLUGIN ...\t" << path;
    psm.loadPlugin(path);
    JAMI_WARN() << "PLUGIN LOADED \t" << path;
}

void
unloadPlugin(const std::string& path){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    JAMI_WARN() << " UNLOADING PLUGIN ...\t" << path;
    psm.unloadPlugin(path);
    JAMI_WARN() << "PLUGIN UNLOADED \t" << path;
}

void
togglePlugin(const std::string& path, bool toggle){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    psm.togglePlugin(path,toggle);
    if(toggle) {
        JAMI_WARN() << " Plugin " << path << " ON";
    } else {
        JAMI_WARN() << " Plugin " << path << " OFF";
    }
}

std::vector<std::map<std::string,std::string>>
getPluginPreferences(const std::string& path){
    return jami::Manager::instance().getPluginPreferencesManager().parse(path);
}


}
