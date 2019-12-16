#pragma once
#include <string>
#include <map>

namespace jami {
struct PluginPreferencesMap {
    std::string path;
    std::map<std::string, std::string> preferenceValuesMap;
};
}

