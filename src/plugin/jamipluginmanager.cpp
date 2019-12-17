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

int JamiPluginManager::installPlugin(const std::string &jplPath, bool force)
{
    int r{0};
    if(fileutils::isFile(jplPath)) {
        std::map<std::string, std::string> manifestMap;
        r = static_cast<int>(readPluginManifestFromArchive(jplPath, manifestMap));
        if(r == 0) {
            std::string name = manifestMap["name"];
            std::string version = manifestMap["version"];
            const std::string destinationDir{fileutils::get_data_dir()
                                             + DIR_SEPARATOR_CH + "plugins"
                                             + DIR_SEPARATOR_CH + name};
            // Find if there is an existing version of this plugin
            const auto alreadyInstalledManifestMap = parseManifestFile(manifestPath(destinationDir));

            if(!alreadyInstalledManifestMap.empty()) {
                if(force) {
                    r = uninstallPlugin(destinationDir);
                    if(r == 0) {
                        r = static_cast<int>(archiver::uncompressArchive(jplPath, destinationDir,
                                                                         uncompressor.fileMatchPair));
                    }
                } else {
                    std::string installedVersion = alreadyInstalledManifestMap.at("version");
                    if(version > installedVersion) {
                        r = uninstallPlugin(destinationDir);
                        if(r == 0) {
                            r = static_cast<int>(archiver::uncompressArchive(jplPath, destinationDir,
                                                                             uncompressor.fileMatchPair));
                        }
                    } else if (version == installedVersion){
                        // An error code of 100 to know that this version is the same as the one installed
                        r = 100;
                    } else {
                        // An error code of 100 to know that this version is older than the one installed
                        r = 200;
                    }
                }
            } else {
                r = static_cast<int>(archiver::uncompressArchive(jplPath, destinationDir,
                                                                 uncompressor.fileMatchPair));
            }
        }
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

