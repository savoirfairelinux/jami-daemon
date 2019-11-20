%header %{

#include "dring/dring.h"
#include "dring/plugin_manager_interface.h"
%}

namespace DRing {
void loadPlugin(const std::string& path);
void unloadPlugin(const std::string& path);
void togglePlugin(const std::string& path, bool toggle);
std::map<std::string,std::string> getPluginDetails(const std::string& path);
std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
bool setPluginPreference(const std::string& path, const std::string& key, const std::string& value);
std::map<std::string,std::string> getPluginPreferencesValuesMap(const std::string& path);
std::vector<std::string> listPlugins(std::string arch);
int removePlugin(const std::string& pluginRootPath);
}
