#pragma once

#include "def.h"

#include <string>
#include <vector>
#include <map>

namespace DRing {
DRING_PUBLIC void loadPlugin(const std::string& path);
DRING_PUBLIC void unloadPlugin(const std::string& path);
DRING_PUBLIC void togglePlugin(const std::string& path, bool toggle);
DRING_PUBLIC std::string getPluginIconPath(const std::string& path);
DRING_PUBLIC std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
DRING_PUBLIC bool setPluginPreference(const std::string& path, const std::string& key, const std::string& value);
DRING_PUBLIC std::map<std::string,std::string> getPluginPreferencesValuesMap(const std::string& path);
}

