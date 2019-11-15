#include "plugin_manager_interface.h"
#include "manager.h"
#include "videomanager.h"
#include "video/video_input.h"
#include "logger.h"
#include <iostream>

namespace DRing {
//
void
loadPlugin(const std::string& path){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    JAMI_WARN() << " LOADING PLUGIN ...\t" << path;
    if(psm){
        psm->loadPlugin(path);
        JAMI_WARN() << "PLUGIN LOADED \t" << path;
    }
}

void
unloadPlugin(const std::string& path){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    JAMI_WARN() << " UNLOADING PLUGIN ...\t" << path;
    if(psm){
        psm->unloadPlugin(path);
        JAMI_WARN() << "PLUGIN UNLOADED \t" << path;
    }
}

void
togglePlugin(const std::string& path, bool toggle){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    if(psm) {
        psm->togglePlugin(path,toggle);
    }
    if(toggle) {
        JAMI_WARN() << " Plugin " << path << " ON";
    } else {
        JAMI_WARN() << " Plugin " << path << " OFF";
    }
}

}
