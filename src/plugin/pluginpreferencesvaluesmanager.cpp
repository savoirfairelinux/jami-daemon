#include "pluginpreferencesvaluesmanager.h"
#include "logger.h"

#include <msgpack.hpp>

#include <fstream>

namespace jami {

bool PluginPreferencesValuesManager::savePreferenceValue(const std::string &preferencesValuesFilePath,
                                                         const std::string &key, const std::string &value)
{
    bool returnValue = true;
    std::map<std::string, std::string> pluginPreferencesMap = getPreferencesValuesMap(preferencesValuesFilePath);
    // Using [] instead of insert to get insert or update effect
    pluginPreferencesMap[key] = value;

    {
        std::ofstream fs(preferencesValuesFilePath, std::ios::binary);
        if(!fs.good()) {
            return false;
        }
        try {
            std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
            msgpack::pack(fs, pluginPreferencesMap);
        } catch (const std::exception& e) {
            returnValue = false;
            JAMI_ERR() << e.what();
        }
    }

    return returnValue; 
}

std::map<std::string, std::string> PluginPreferencesValuesManager::getPreferencesValuesMap(const std::string &preferencesValuesFilePath)
{
    std::ifstream file(preferencesValuesFilePath, std::ios::binary);
    std::map<std::string, std::string>  rmap;
    // If file is accessible
    if(file.good()) {
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
        // Get file size
        std::string str;
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        // If not empty
        if(fileSize > 0) {
            // Read whole file content and put it in the string str
            str.reserve(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            try {
                // Unpack the string
                msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());
                // Deserialized object is valid during the msgpack::object_handle instance is alive.
                msgpack::object deserialized = oh.get();
                deserialized.convert(rmap);
            } catch (const std::exception& e) {
                JAMI_ERR() << e.what();
            }
        }
    }

    return rmap;
}

bool PluginPreferencesValuesManager::resetPluginPreferencesValuesMap(const std::string &preferencesValuesFilePath)
{
    bool returnValue = true;
    std::map<std::string, std::string> pluginPreferencesMap{};

    {
        std::ofstream fs(preferencesValuesFilePath, std::ios::binary);
        if(!fs.good()) {
            return false;
        }
        try {
            std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
            msgpack::pack(fs, pluginPreferencesMap);
        } catch (const std::exception& e) {
            returnValue = false;
            JAMI_ERR() << e.what();
        }
    }

    return returnValue;
}

}
