#include "plugin_manager_interface.h"
#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "logger.h"
#include <iostream>

namespace DRing {
//
void
loadPlugin(const std::string& path){
    auto& jpm = jami::Manager::instance().getJamiPluginManager();
    JAMI_WARN() << " LOADING PLUGIN ...\t" << path;
    jpm.loadPlugin(path);
    JAMI_WARN() << "PLUGIN LOADED \t" << path;
}

void
unloadPlugin(const std::string& path){
    auto& jpm = jami::Manager::instance().getJamiPluginManager();
    JAMI_WARN() << " UNLOADING PLUGIN ...\t" << path;
    jpm.unloadPlugin(path);
    JAMI_WARN() << "PLUGIN UNLOADED \t" << path;
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
    listPlugins() {
    return jami::Manager::instance().getJamiPluginManager().listPlugins();
}
int installPlugin(const std::string& jplPath, bool force) {
    return jami::Manager::instance().getJamiPluginManager().installPlugin(jplPath, force);
}

int uninstallPlugin(const std::string& pluginRootPath) {
    return jami::Manager::instance().getJamiPluginManager().uninstallPlugin(pluginRootPath);
}
}
