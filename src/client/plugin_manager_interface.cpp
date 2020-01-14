#include "plugin_manager_interface.h"
#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "logger.h"
#include <iostream>

namespace DRing {
//
bool
loadPlugin(const std::string& path){
    auto& jpm = jami::Manager::instance().getJamiPluginManager();
    JAMI_WARN() << " LOADING PLUGIN ...\t" << path;
    bool r = jpm.loadPlugin(path);
    JAMI_WARN() << "PLUGIN LOADED \t" << path;
    return r;
}

bool
unloadPlugin(const std::string& path){
    auto& jpm = jami::Manager::instance().getJamiPluginManager();
    JAMI_WARN() << " UNLOADING PLUGIN ...\t" << path;
    bool r = jpm.unloadPlugin(path);
    JAMI_WARN() << "PLUGIN UNLOADED \t" << path;
    return r;
}

void
togglePlugin(const std::string& path, bool toggle){
    auto& jpm = jami::Manager::instance().getJamiPluginManager();
    jpm.togglePlugin(path,toggle);
    if(toggle) {
        JAMI_WARN() << " Plugin " << path << " ON";
    } else {
        JAMI_WARN() << " Plugin " << path << " OFF";
    }
}

std::map<std::string,std::string>
getPluginDetails(const std::string& path){
    return jami::Manager::instance().getJamiPluginManager().getPluginDetails(path);
}

std::vector<std::map<std::string,std::string>>
getPluginPreferences(const std::string& path){
    return jami::Manager::instance().getJamiPluginManager().getPluginPreferences(path);
}

bool
setPluginPreference(const std::string& path, const std::string& key, const std::string& value) {
    return jami::Manager::instance().getJamiPluginManager().setPluginPreference(path, key, value);
}

std::map<std::string,std::string>
getPluginPreferencesValues(const std::string& path){
    return jami::Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(path);
}
bool
resetPluginPreferencesValues(const std::string& path){
    return jami::Manager::instance().getJamiPluginManager().resetPluginPreferencesValuesMap(path);
}

std::vector<std::string>
listAvailablePlugins() {
    return jami::Manager::instance().getJamiPluginManager().listAvailablePlugins();
}

std::vector<std::string>
listLoadedPlugins() {
    return jami::Manager::instance().getJamiPluginManager().listLoadedPlugins();
}

int installPlugin(const std::string& jplPath, bool force) {
    return jami::Manager::instance().getJamiPluginManager().installPlugin(jplPath, force);
}

int uninstallPlugin(const std::string& pluginRootPath) {
    return jami::Manager::instance().getJamiPluginManager().uninstallPlugin(pluginRootPath);
}

std::vector<std::string> listCallMediaHandlers() {
    return jami::Manager::instance().getJamiPluginManager().getCallServicesManager().listCallMediaHandlers();
}

void toggleCallMediaHandler(const std::string& id) {
    return jami::Manager::instance().getJamiPluginManager().getCallServicesManager().toggleCallMediaHandler(id);
}

std::map<std::string,std::string> getCallMediaHandlerDetails(const std::string& id) {
    return jami::Manager::instance().getJamiPluginManager().getCallServicesManager().getCallMediaHandlerDetails(id);
}
}
