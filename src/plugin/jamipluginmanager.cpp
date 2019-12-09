#include "jamipluginmanager.h"
#include "logger.h"

#include <sstream>
#include <fstream>
#include <regex>
#include <stdexcept>

extern "C" {
#include <archive.h>
}

#include <json/json.h>

#if defined(__arm__)
#if defined(__ARM_ARCH_7A__)
#if defined(__ARM_NEON__)
#if defined(__ARM_PCS_VFP)
#define ABI "armeabi-v7a/NEON (hard-float)"
#else
#define ABI "armeabi-v7a/NEON"
#endif
#else
#if defined(__ARM_PCS_VFP)
#define ABI "armeabi-v7a (hard-float)"
#else
#define ABI "armeabi-v7a"
#endif
#endif
#else
#define ABI "armeabi"
#endif
#elif defined(__i386__)
#define ABI "x86"
#elif defined(__x86_64__)
#define ABI "x86_64"
#elif defined(__mips64)  /* mips64el-* toolchain defines __mips__ too */
#define ABI "mips64"
#elif defined(__mips__)
#define ABI "mips"
#elif defined(__aarch64__)
#define ABI "arm64-v8a"
#else
#define ABI "unknown"
#endif

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
            throw std::runtime_error("plugin manifest file: bad format");
        }
    } else{
        throw std::runtime_error("failed to parse the plugin manifest file");
    }

    return r;
}

static const std::regex DATA_REGEX("^data" ESC_DIR_SEPARATOR_STR ".+");
static const std::regex SO_REGEX("([a-z0-9]+(?:[_-]?[a-z0-9]+)*)" ESC_DIR_SEPARATOR_STR "([a-z0-9_]+\\.(so|dll))");

const archiver::FileMatchPair  uncompressJplFunction = [](const std::string& relativeFileName) {
    std::smatch match;
    if(relativeFileName == "manifest.json" || std::regex_match(relativeFileName, DATA_REGEX)){
        return std::make_pair(true, relativeFileName);
    } else if(regex_search(relativeFileName, match, SO_REGEX) == true) {
        if(match.str(1)==ABI) {
            return std::make_pair(true, match.str(2));
        }
    }
    return std::make_pair(false, std::string{""});
};

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
                                                                         uncompressJplFunction));
                    }
                } else {
                    std::string installedVersion = alreadyInstalledManifestMap.at("version");
                    if(version > installedVersion) {
                        r = uninstallPlugin(destinationDir);
                        if(r == 0) {
                            r = static_cast<int>(archiver::uncompressArchive(jplPath, destinationDir,
                                                                             uncompressJplFunction));
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
                                                                 uncompressJplFunction));
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
        try {
            checkManifestValidity(ss, details);
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
    }

    return r;
}

std::map<std::string, std::string> JamiPluginManager::parseManifestFile(const std::string &manifestFilePath)
{
    std::map<std::string, std::string> manifestMap;
    std::ifstream file(manifestFilePath);

    if(file) {
        try {
            checkManifestValidity(file, manifestMap);
        } catch (const std::exception& e) {
            manifestMap.clear();
            JAMI_ERR() << e.what();
        }
    }

    return manifestMap;
}

}

