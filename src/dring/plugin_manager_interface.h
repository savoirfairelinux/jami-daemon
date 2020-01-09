#pragma once

#include "def.h"

#include <string>
#include <vector>
#include <map>

namespace DRing {
DRING_PUBLIC void loadPlugin(const std::string& path);
DRING_PUBLIC void unloadPlugin(const std::string& path);
DRING_PUBLIC void togglePlugin(const std::string& path, bool toggle);
DRING_PUBLIC std::map<std::string,std::string> getPluginDetails(const std::string& path);
DRING_PUBLIC std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
DRING_PUBLIC bool setPluginPreference(const std::string& path, const std::string& key, const std::string& value);
DRING_PUBLIC std::map<std::string,std::string> getPluginPreferencesValues(const std::string& path);
DRING_PUBLIC bool resetPluginPreferencesValues(const std::string& path);
DRING_PUBLIC std::vector<std::string> listPlugins();
DRING_PUBLIC int installPlugin(const std::string& jplPath, bool force);
DRING_PUBLIC int uninstallPlugin(const std::string& pluginRootPath);
}

