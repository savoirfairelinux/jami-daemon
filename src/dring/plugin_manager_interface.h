#pragma once

#include "def.h"

#include <string>
#include <vector>
#include <map>

namespace DRing {
DRING_PUBLIC void loadPlugin(const std::string& path);
DRING_PUBLIC void unloadPlugin(const std::string& path);
DRING_PUBLIC void togglePlugin(const std::string& path, bool toggle);
DRING_PUBLIC std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
}

