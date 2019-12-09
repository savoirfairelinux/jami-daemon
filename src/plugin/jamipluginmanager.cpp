#include "jamipluginmanager.h"
#include "logger.h"

#include <sstream>
#include <fstream>

extern "C" {
#include <archive.h>
}

#include <json/json.h>

namespace jami {

long checkManifestValidity(std::istream& stream, std::map<std::string, std::string>& manifestMap) {
    long r{ARCHIVE_OK};
    Json::Value root; 
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;

    bool ok = Json::parseFromStream(rbuilder, stream, &root, &errs);

    if(ok) {
        std::string name = root.get("name", "").asString();
        std::string description = root.get("description", "").asString();
        std::string version = root.get("version", "").asString();
        if(!name.empty() || !version.empty()){
            manifestMap["name"] = name;
            manifestMap["description"] = description;
            manifestMap["version"] = version;
        } else {
            // -50 name and version empty
            r = -50l;
            JAMI_ERR() << "plugin manifest file: bad format";
        }
    } else{
        // -40 failed to parse the stream
        r = -40l;
        JAMI_ERR() << "failed to parse the plugin manifest file";
    }

    return r;
}

long JamiPluginManager::readPluginManifestFromArchive(const std::string &jplPath, std::map<std::string, std::string>& details)
{
    long r = ARCHIVE_OK;
    std::stringstream ss;
    r = archiver::readFileFromArchiveToStream(jplPath,"manifest.json", ss);
    if(r == ARCHIVE_OK) {
        r = checkManifestValidity(ss, details);
    }

    return r;
}

std::map<std::string, std::string> JamiPluginManager::parseManifestFile(const std::string &manifestFilePath)
{
    std::map<std::string, std::string> manifestMap;
    std::ifstream file(manifestFilePath);

    if(file) {
        long r = checkManifestValidity(file, manifestMap);
        if(r != 0) {
            manifestMap.clear();
        }
    }

    return manifestMap;
}

}

