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
    if(psm){
        psm->loadPlugin(path);
    }
    JAMI_WARN() << " LOADING PLUGIN " << path;
}

void
unloadPlugin(const std::string& path){
    auto& psm = jami::Manager::instance().getPluginServicesManager();
    if(psm){
        psm->unloadPlugin(path);
    }
    JAMI_WARN() << " UNLOADING PLUGIN " << path;
}

}
