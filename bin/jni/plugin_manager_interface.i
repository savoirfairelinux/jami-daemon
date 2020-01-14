%header %{

#include "dring/dring.h"
#include "dring/plugin_manager_interface.h"
%}

namespace DRing {
bool loadPlugin(const std::string& path);
bool unloadPlugin(const std::string& path);
void togglePlugin(const std::string& path, bool toggle);
std::map<std::string,std::string> getPluginDetails(const std::string& path);
std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
bool setPluginPreference(const std::string& path, const std::string& key, const std::string& value);
std::map<std::string,std::string> getPluginPreferencesValues(const std::string& path);
bool resetPluginPreferencesValues(const std::string& path);
std::vector<std::string> listAvailablePlugins();
std::vector<std::string> listLoadedPlugins();
int installPlugin(const std::string& jplPath, bool force);
int uninstallPlugin(const std::string& pluginRootPath);
}
