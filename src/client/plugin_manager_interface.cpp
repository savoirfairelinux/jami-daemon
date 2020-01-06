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

std::map<std::string,std::string>
getPluginDetails(const std::string& path){
    return jami::Manager::instance().getJamiPluginManager().getPluginDetails(path);
}

std::vector<std::map<std::string,std::string>>
getPluginPreferences(const std::string& path){
    return jami::Manager::instance().getPluginPreferencesManager().getPluginPreferences(path);
}

bool
setPluginPreference(const std::string& path, const std::string& key, const std::string& value) {
    return jami::Manager::instance().getPluginPreferencesManager().savePluginPreferenceValue(path, key, value);
}

std::map<std::string,std::string>
getPluginPreferencesValuesMap(const std::string& path){
    return jami::Manager::instance().getPluginPreferencesManager().getPluginPreferencesValuesMap(path);
}
bool
resetPluginPreferencesValuesMap(const std::string& path){
    return jami::Manager::instance().getPluginPreferencesManager().resetPluginPreferencesValuesMap(path);
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
