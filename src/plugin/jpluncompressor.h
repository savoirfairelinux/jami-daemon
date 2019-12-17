#pragma once
#include "fileutils.h"
#include "archiver.h"
#include "logger.h"
#include <regex>

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
class JplUncompressor {
public:
    JplUncompressor() {
        std::string dataRegexSting{"^data"};
        dataRegexSting.append(ESCAPED_DIR_STR).append(".+");
        dataRegex = dataRegexSting;

        std::string soRegexString =  "([a-z0-9]+(?:[_-]?[a-z0-9]+)*)";
        soRegexString.append(ESCAPED_DIR_STR).append("([a-z0-9_]+\\.(so|dll))");
        soRegex = soRegexString;

        fileMatchPair = [this](const std::string& relativeFileName) {
            std::smatch match;
        if(relativeFileName == "manifest.json" || std::regex_match(relativeFileName, dataRegex)){
                return std::make_pair(true, relativeFileName);
            } else if(regex_search(relativeFileName, match, soRegex) == true) {
                if(match.str(1)==ABI) {
                    return std::make_pair(true, match.str(2));
                }
            }
            return std::make_pair(false, std::string{""});
        };
    }

public:
    archiver::FileMatchPair fileMatchPair;

private:
    // Data directory Regex
    std::regex dataRegex;
    // So Regex
    std::regex soRegex;
};

}
