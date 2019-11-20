#include "pluginpreferencesparser.h"
#include "logger.h"

#include <fstream>
#include <set>

namespace jami {
std::vector<MapStrStr> PluginPreferencesParser::parsePreferencesConfigFile(const std::string &preferenceFilePath)
{
    std::ifstream file(preferenceFilePath);
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::set<std::string> keys;
    std::vector<std::map<std::string, std::string>> preferences;
    if(file) {
        bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
        if(ok && root.isArray()) {
            for(int i=0; i< static_cast<int>(root.size()); i++) {
                const Json::Value jsonPreference = root[i];
                std::string category = jsonPreference.get("category", "NoCategory").asString();
                std::string type = jsonPreference.get("type", "None").asString();
                std::string key = jsonPreference.get("key", "None").asString();
                if(type != "None" && key != "None") {
                    if(keys.find(key) == keys.end()) {
                       const auto& preferenceAttributes = parsePreferenceConfig(jsonPreference, type);
                       // If the parsing of the attributes was successful, commit the map and the key
                       if(!preferenceAttributes.empty()) {
                           preferences.push_back(std::move(preferenceAttributes));
                           keys.insert(key);
                       }
                    } 
                }
            }
        } else {
            JAMI_ERR() << "PluginPreferencesParser:: Failed to parse preferences.json for plugin: "
                       << preferenceFilePath;
        }
    }
    
    return preferences;
}

MapStrStr PluginPreferencesParser::parsePreferenceConfig(const Json::Value& jsonPreference,
                                                    const std::string& type)
{
    std::map<std::string, std::string> preferenceMap;
    const auto& members = jsonPreference.getMemberNames();
    // Insert other fields
    for(const auto& member : members) {
        const Json::Value& value = jsonPreference[member];
        if(value.isString()) {
            preferenceMap.insert(std::pair<std::string,std::string>{member, jsonPreference[member].asString()});
        } else if (value.isArray()) {
            preferenceMap.insert(std::pair<std::string,std::string>{member, convertArrayToString(jsonPreference[member])});
        }
    }
    return preferenceMap;
}

std::string PluginPreferencesParser::convertArrayToString(const Json::Value jsonArray)
{   std::string stringArray = "[";
    
    for(int i=0; i< static_cast<int>(jsonArray.size()) - 1; i++) {
        if(jsonArray[i].isString()) {
            stringArray+=jsonArray[i].asString()+",";
        } else if(jsonArray[i].isArray()) {
            stringArray+=convertArrayToString(jsonArray[i])+",";
        }
    }
    
    int lastIndex = static_cast<int>(jsonArray.size()) - 1;
    if(jsonArray[lastIndex].isString()) {
        stringArray+=jsonArray[lastIndex].asString();
    }
    
    stringArray+="]";
    
    return stringArray;
}
}


